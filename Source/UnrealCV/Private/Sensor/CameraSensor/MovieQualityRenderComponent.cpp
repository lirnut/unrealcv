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
#include "Scalability.h"
#include "HAL/IConsoleManager.h"
#include "LegacyScreenPercentageDriver.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "MoviePipelineSurfaceReader.h"
#include "UnrealcvServer.h"

UMovieQualityRenderComponent::UMovieQualityRenderComponent()
	: Resolution(1920, 1080)
	, bApplyMovieQualitySettings(true)
	, ParentSensor(nullptr)
	, bIsInitialized(false)
	, PixelFormat(PF_FloatRGBA)
	, bForceLinearGamma(true)
	, ImageWriteQueue(nullptr)
{
	PrimaryComponentTick.bCanEverTick = false;
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
		ParentSensor = Cast<UFusionCamSensor>(Owner->GetComponentByClass(UFusionCamSensor::StaticClass()));
		if (ParentSensor)
		{
			UE_LOG(LogTemp, Log, TEXT("MovieQualityRenderComponent: Found parent FusionCamSensor"));
		}
	}

	if (bApplyMovieQualitySettings)
	{
		Initialize();
	}
}

void UMovieQualityRenderComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Shutdown();
	Super::EndPlay(EndPlayReason);
}

void UMovieQualityRenderComponent::Initialize()
{
	if (bIsInitialized)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("MovieQualityRenderComponent: No valid world"));
		return;
	}

	FServerConfig& Config = FUnrealcvServer::Get().Config;
	bool bUseBGRA8 = Config.bLitUseBGRA8;

	if (bUseBGRA8)
	{
		PixelFormat = PF_B8G8R8A8;
		bForceLinearGamma = false;
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

	if (bApplyMovieQualitySettings)
	{
		ApplyMovieQualitySettings();
	}

	bIsInitialized = true;

	UE_LOG(LogTemp, Log, TEXT("MovieQualityRenderComponent initialized at %dx%d, PixelFormat=%s, LinearGamma=%d"),
		Resolution.X, Resolution.Y,
		PixelFormat == PF_FloatRGBA ? TEXT("FloatRGBA") : TEXT("BGRA8"),
		bForceLinearGamma);
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

	if (bApplyMovieQualitySettings)
	{
		RestoreQualitySettings();
	}

	bIsInitialized = false;
}

void UMovieQualityRenderComponent::ApplyMovieQualitySettings()
{
	IConsoleManager& ConsoleMgr = IConsoleManager::Get();
	PreviousQualitySettings.Empty();

	auto SetCVar = [&](const TCHAR* Name, int32 Value)
	{
		if (IConsoleVariable* CVar = ConsoleMgr.FindConsoleVariable(Name))
		{
			PreviousQualitySettings.Add(Name, CVar->GetInt());
			CVar->Set(Value);
		}
	};

	auto SetCVarFloat = [&](const TCHAR* Name, float Value)
	{
		if (IConsoleVariable* CVar = ConsoleMgr.FindConsoleVariable(Name))
		{
			PreviousQualitySettings.Add(Name, CVar->GetFloat());
			CVar->Set(Value);
		}
	};

	Scalability::FQualityLevels QualityLevels;
	QualityLevels.SetFromSingleQualityLevelRelativeToMax(0);
	Scalability::SetQualityLevels(QualityLevels);

	SetCVar(TEXT("r.TextureStreaming"), 0);
	SetCVar(TEXT("r.ForceLOD"), 0);
	SetCVar(TEXT("r.SkeletalMeshLODBias"), -10);
	SetCVar(TEXT("r.ParticleLODBias"), -10);
	SetCVar(TEXT("foliage.DitheredLOD"), 0);
	SetCVar(TEXT("foliage.ForceLOD"), 0);
	SetCVar(TEXT("r.ShadowQuality"), 5);
	SetCVarFloat(TEXT("r.Shadow.DistanceScale"), 10.0f);
	SetCVarFloat(TEXT("r.Shadow.RadiusThreshold"), 0.001f);
	SetCVarFloat(TEXT("r.ViewDistanceScale"), 50.0f);
	SetCVar(TEXT("r.VolumetricRenderTarget"), 1);
	SetCVar(TEXT("r.VolumetricRenderTarget.Mode"), 3);
	SetCVar(TEXT("r.SkyLight.RealTimeReflectionCapture.TimeSlice"), 0);
	SetCVar(TEXT("r.PostProcessing.PropagateAlpha"), 1);

	UE_LOG(LogTemp, Log, TEXT("MovieQualityRenderComponent: Applied Movie Quality Settings"));
}

void UMovieQualityRenderComponent::RestoreQualitySettings()
{
	IConsoleManager& ConsoleMgr = IConsoleManager::Get();

	for (const auto& Pair : PreviousQualitySettings)
	{
		if (IConsoleVariable* CVar = ConsoleMgr.FindConsoleVariable(*Pair.Key))
		{
			CVar->Set(Pair.Value);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("MovieQualityRenderComponent: Restored Previous Quality Settings"));
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

	if (!ParentSensor)
	{
		UE_LOG(LogTemp, Error, TEXT("MovieQualityRenderComponent: No parent sensor"));
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
	if (!World || !ParentSensor)
	{
		return nullptr;
	}

	FRenderTarget* RenderTargetResource = RenderTarget->GameThread_GetRenderTargetResource();

	UBaseCameraSensor* LitSensor = ParentSensor->GetLitCamSensor();
	if (!LitSensor)
	{
		return nullptr;
	}

	FEngineShowFlags ShowFlags = LitSensor->ShowFlags;
	ShowFlags.SetScreenPercentage(true);
	ShowFlags.SetMotionBlur(true);

	TSharedPtr<FSceneViewFamilyContext> ViewFamily = MakeShared<FSceneViewFamilyContext>(
		FSceneViewFamily::ConstructionValues(
			RenderTargetResource,
			World->Scene,
			ShowFlags
		)
		.SetTime(FGameTime::CreateUndilated(World->GetTimeSeconds(), World->GetDeltaSeconds()))
		.SetRealtimeUpdate(true)
	);

	ViewFamily->SceneCaptureSource = LitSensor->CaptureSource;
	ViewFamily->bWorldIsPaused = false;
	ViewFamily->ViewMode = VMI_Lit;
	ViewFamily->bOverrideVirtualTextureThrottle = true;
	ViewFamily->SetScreenPercentageInterface(new FLegacyScreenPercentageDriver(*ViewFamily, 1.0f));

	return ViewFamily;
}

FSceneView* UMovieQualityRenderComponent::CreateSceneView(FSceneViewFamily* ViewFamily)
{
	if (!ParentSensor)
	{
		return nullptr;
	}

	FVector Location = ParentSensor->GetSensorLocation();
	FRotator Rotation = ParentSensor->GetSensorRotation();
	float FOV = ParentSensor->GetSensorFOV();

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
