#include "MovieQualityRenderComponent.h"
#include "FusionCamSensor.h"
#include "BaseCameraSensor.h"
#include "LitCamSensor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Engine/Canvas.h"
#include "SceneView.h"
#include "EngineModule.h"
#include "RenderingThread.h"
#include "RHICommandList.h"
#include "ImageWriteQueue.h"
#include "ImageWriteTask.h"
#include "Modules/ModuleManager.h"
#include "LegacyScreenPercentageDriver.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "MoviePipelineSurfaceReader.h"
#include "UnrealcvServer.h"

UMovieQualityRenderComponent::UMovieQualityRenderComponent()
{
	bIsInitialized = false;
	PrimaryComponentTick.bCanEverTick = false;

	ShowFlags.SetScreenPercentage(true);
	ShowFlags.SetMotionBlur(true);

	// other properties need to be initialized when BeginPlay
}

UMovieQualityRenderComponent::~UMovieQualityRenderComponent()
{
	Shutdown();
}

void UMovieQualityRenderComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (Owner)
	{
		auto* ParentSensor = Cast<UFusionCamSensor>(Owner->GetComponentByClass(UFusionCamSensor::StaticClass()));
		if (ParentSensor)
		{
			UE_LOG(LogTemp, Log, TEXT("MovieQualityRenderComponent: Found parent FusionCamSensor"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("MovieQualityRenderComponent: can not Found parent FusionCamSensor!"))
		}
	}

	FServerConfig& Config = FUnrealcvServer::Get().Config;
	int32 ResWidth = Config.Width == 0 ? 640 : Config.Width;
	int32 ResHeight = Config.Height == 0 ? 480 : Config.Height;
	FOV = Config.FOV == 0 ? 90 : Config.FOV;

	Initialize(ESceneCaptureSource::SCS_FinalColorHDR, ResWidth, ResHeight);
}

void UMovieQualityRenderComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Shutdown();
	Super::EndPlay(EndPlayReason);
}

void UMovieQualityRenderComponent::Initialize(ESceneCaptureSource InCaptureSource, int32 ResolutionX, int32 ResolutionY)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("MovieQualityRenderComponent: No valid world"));
		return;
	}

	CaptureSource = InCaptureSource;
	Resolution.X = ResolutionX;
	Resolution.Y = ResolutionY;

	FServerConfig& Config = FUnrealcvServer::Get().Config;
	bool bUseBGRA8 = Config.bLitUseBGRA8;

	if (bUseBGRA8)
	{
		PixelFormat = PF_B8G8R8A8;
		bForceLinearGamma = (CaptureSource == ESceneCaptureSource::SCS_FinalColorLDR);
	}
	else
	{
		PixelFormat = PF_FloatRGBA;
		bForceLinearGamma = true;
	}

	ViewState.Allocate(World->GetFeatureLevel());

	SurfaceQueue = MakeShared<FMoviePipelineSurfaceQueue, ESPMode::ThreadSafe>(
		Resolution,
		PixelFormat,
		3,
		bForceLinearGamma
	);

	ImageWriteQueue = &FModuleManager::Get().LoadModuleChecked<IImageWriteQueueModule>("ImageWriteQueue").GetWriteQueue();

	bIsInitialized = true;

	UE_LOG(LogTemp, Log, TEXT("MovieQualityRenderComponent initialized at %dx%d, PixelFormat=%s, LinearGamma=%d, CaptureSource=%d"),
		Resolution.X, Resolution.Y,
		PixelFormat == PF_FloatRGBA ? TEXT("FloatRGBA") : TEXT("BGRA8"),
		bForceLinearGamma,
		(int32)CaptureSource);
}

void UMovieQualityRenderComponent::Shutdown()
{
	if (!bIsInitialized)
	{
		return;
	}

	if (SurfaceQueue.IsValid())
	{
		SurfaceQueue->Shutdown();
		SurfaceQueue.Reset();
	}

	FSceneViewStateInterface* Ref = ViewState.GetReference();
	if (Ref)
	{
		Ref->ClearMIDPool();
	}
	ViewState.Destroy();

	for (auto& Pair : RenderTargetPool)
	{
		if (Pair.Value)
		{
			Pair.Value->RemoveFromRoot();
		}
	}
	RenderTargetPool.Empty();

	bIsInitialized = false;
}

void UMovieQualityRenderComponent::SaveLitToFile(const FString& OutputPath, TFunction<void(bool)> OnComplete)
{
	if (!bIsInitialized)
	{
		UE_LOG(LogTemp, Error, TEXT("MovieQualityRenderComponent: Not initialized"));
		if (OnComplete)
		{
			OnComplete(false);
		}
		return;
	}

	FString PoolKey = FString::Printf(TEXT("RGB_%dx%d_%s"),
		Resolution.X, Resolution.Y,
		PixelFormat == PF_FloatRGBA ? TEXT("Float") : TEXT("BGRA8"));

	UTextureRenderTarget2D* RenderTarget = nullptr;

	if (RenderTargetPool.Contains(PoolKey))
	{
		RenderTarget = RenderTargetPool[PoolKey];
	}
	else
	{
		RenderTarget = NewObject<UTextureRenderTarget2D>(this);
		RenderTarget->ClearColor = FLinearColor::Black;
		RenderTarget->TargetGamma = GetTargetGamma();
		RenderTarget->InitCustomFormat(Resolution.X, Resolution.Y, PixelFormat, bForceLinearGamma);
		RenderTarget->AddToRoot();
		RenderTargetPool.Add(PoolKey, RenderTarget);
	}

	TSharedPtr<FSceneViewFamilyContext> ViewFamily = CreateViewFamily(RenderTarget);
	if (!ViewFamily.IsValid())
	{
		if (OnComplete)
		{
			OnComplete(false);
		}
		return;
	}

	FSceneView* View = CreateSceneView(ViewFamily.Get());
	if (!View)
	{
		if (OnComplete)
		{
			OnComplete(false);
		}
		return;
	}

	SubmitToRenderer(ViewFamily.Get(), RenderTarget, OutputPath, OnComplete);
}

TSharedPtr<FSceneViewFamilyContext> UMovieQualityRenderComponent::CreateViewFamily(UTextureRenderTarget2D* RenderTarget)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	FRenderTarget* RenderTargetResource = RenderTarget->GameThread_GetRenderTargetResource();

	TSharedPtr<FSceneViewFamilyContext> ViewFamily = MakeShared<FSceneViewFamilyContext>(
		FSceneViewFamily::ConstructionValues(
			RenderTargetResource,
			World->Scene,
			ShowFlags
		)
		.SetTime(FGameTime::CreateUndilated(World->GetTimeSeconds(), World->GetDeltaSeconds()))
		.SetRealtimeUpdate(true)
	);

	ViewFamily->SceneCaptureSource = CaptureSource;
	ViewFamily->bWorldIsPaused = false;
	ViewFamily->ViewMode = VMI_Lit;
	ViewFamily->bOverrideVirtualTextureThrottle = true;
	ViewFamily->SetScreenPercentageInterface(new FLegacyScreenPercentageDriver(*ViewFamily, 1.0f));

	return ViewFamily;
}

FSceneView* UMovieQualityRenderComponent::CreateSceneView(FSceneViewFamily* ViewFamily)
{
	FVector Location = GetComponentLocation();
	FRotator Rotation = GetComponentRotation();

	FSceneViewInitOptions ViewInitOptions;
	ViewInitOptions.ViewFamily = ViewFamily;
	ViewInitOptions.ViewOrigin = Location;
	ViewInitOptions.SetViewRectangle(FIntRect(0, 0, Resolution.X, Resolution.Y));
	ViewInitOptions.ViewRotationMatrix = FInverseRotationMatrix(Rotation);
	ViewInitOptions.ViewActor = GetOwner();

	ViewInitOptions.ViewRotationMatrix = ViewInitOptions.ViewRotationMatrix * FMatrix(
		FPlane(0, 0, 1, 0),
		FPlane(1, 0, 0, 0),
		FPlane(0, 1, 0, 0),
		FPlane(0, 0, 0, 1)
	);

	const float AspectRatio = (float)Resolution.X / (float)Resolution.Y;

	FMinimalViewInfo ViewInfo;
	ViewInfo.Location = Location;
	ViewInfo.Rotation = Rotation;
	ViewInfo.FOV = FOV;
	ViewInfo.AspectRatio = AspectRatio;
	ViewInfo.bConstrainAspectRatio = false;
	ViewInfo.ProjectionMode = ECameraProjectionMode::Perspective;

	ViewInitOptions.ProjectionMatrix = ViewInfo.CalculateProjectionMatrix();

	FSceneView* View = new FSceneView(ViewInitOptions);

	View->State = ViewState.GetReference();
	View->bIsOfflineRender = true;
	View->AntiAliasingMethod = AAM_TemporalAA;

	ViewFamily->Views.Add(View);

	return View;
}

void UMovieQualityRenderComponent::SubmitToRenderer(
	FSceneViewFamily* ViewFamily,
	UTextureRenderTarget2D* RenderTarget,
	const FString& OutputPath,
	TFunction<void(bool)> OnComplete)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		if (OnComplete)
		{
			OnComplete(false);
		}
		return;
	}

	FRenderTarget* RenderTargetResource = RenderTarget->GameThread_GetRenderTargetResource();

	FCanvas Canvas(RenderTargetResource, nullptr, World, ViewFamily->GetFeatureLevel(), FCanvas::CDM_DeferDrawing, 1.0f);
	GetRendererModule().BeginRenderingViewFamily(&Canvas, ViewFamily);

	TSharedRef<FImagePixelDataPayload, ESPMode::ThreadSafe> FramePayload = MakeShared<FImagePixelDataPayload, ESPMode::ThreadSafe>();

	auto Callback = [this, OutputPath, OnComplete](TUniquePtr<FImagePixelData>&& InPixelData)
	{
		TUniquePtr<FImageWriteTask> ImageTask = MakeUnique<FImageWriteTask>();
		ImageTask->PixelData = MoveTemp(InPixelData);
		ImageTask->Filename = OutputPath;
		ImageTask->Format = EImageFormat::PNG;
		ImageTask->CompressionQuality = 100;
		ImageTask->bOverwriteFile = true;

		ImageTask->OnCompleted = [OnComplete](bool bSuccess)
		{
			if (OnComplete)
			{
				OnComplete(bSuccess);
			}
		};

		ImageWriteQueue->Enqueue(MoveTemp(ImageTask));
	};

	ENQUEUE_RENDER_COMMAND(CaptureFrameCommand)(
		[SurfaceQueue = this->SurfaceQueue, FramePayload, Callback, RenderTargetResource](FRHICommandListImmediate& RHICmdList) mutable
		{
			SurfaceQueue->OnRenderTargetReady_RenderThread(
				RenderTargetResource->GetRenderTargetTexture(),
				FramePayload,
				MoveTemp(Callback)
			);
		}
	);
}

float UMovieQualityRenderComponent::GetTargetGamma() const
{
	if (bForceLinearGamma)
	{
		return 1.0f;
	}
	else
	{
		return UTextureRenderTarget::GetDefaultDisplayGamma();
	}
}
