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
  : ShowFlags(EShowFlagInitMode::ESFIM_Game)
{
	bIsInitialized = false;
	PrimaryComponentTick.bCanEverTick = false;


	ShowFlags.SetPostProcessing(true);
	ShowFlags.SetAntiAliasing(true);
	ShowFlags.SetTemporalAA(true);
	ShowFlags.SetScreenPercentage(true);
	ShowFlags.SetMotionBlur(true);

	ShowFlags.SetHair(true);  
	ShowFlags.SetDynamicShadows(true);   // 启用动态阴影
	ShowFlags.SetContactShadows(true);   // 启用contact shadows
  	ShowFlags.SetCapsuleShadows(true);   // 启用capsule shadows

	ShowFlags.SetPreviewShadowsIndicator(false);

	FServerConfig& Config = FUnrealcvServer::Get().Config;
	CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	FOV = Config.FOV == 0 ? 90 : Config.FOV;

	AntiAliasingMethod = EAntiAliasingMethod::AAM_FXAA;
	// AntiAliasingMethod = EAntiAliasingMethod::AAM_TemporalAA;
	// AntiAliasingMethod = EAntiAliasingMethod::AAM_TSR;

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
			UE_LOG(LogTemp, Warning, TEXT("MovieQualityRenderComponent: can not Found parent FusionCamSensor!"));
		}
	}

	FServerConfig& Config = FUnrealcvServer::Get().Config;
	int32 ResWidth = Config.Width == 0 ? 640 : Config.Width;
	int32 ResHeight = Config.Height == 0 ? 480 : Config.Height;

	Initialize(ResWidth, ResHeight);
}

void UMovieQualityRenderComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Shutdown();
	Super::EndPlay(EndPlayReason);
}

void UMovieQualityRenderComponent::Initialize(int32 ResolutionX, int32 ResolutionY)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("MovieQualityRenderComponent: No valid world"));
		return;
	}

	Resolution.X = ResolutionX;
	Resolution.Y = ResolutionY;

	FServerConfig& Config = FUnrealcvServer::Get().Config;
	// bool bUseBGRA8 = Config.bLitUseBGRA8;
	bool bUseBGRA8 = false;

	if (bUseBGRA8)
	{
		PixelFormat = PF_B8G8R8A8;
		if (CaptureSource == ESceneCaptureSource::SCS_FinalColorLDR)
		{
			bForceLinearGamma = true;
			ForceTargetGamma = 2.2f;
		}
		else
		{
			bForceLinearGamma = true;
			ForceTargetGamma = 2.2f;
		}
	}
	else
	{
		PixelFormat = PF_FloatRGBA;
		if (CaptureSource == ESceneCaptureSource::SCS_FinalColorLDR)
		{
			bForceLinearGamma = true;
			ForceTargetGamma = 2.2f;
		}
		else
		{
			bForceLinearGamma = false;
			ForceTargetGamma = 2.2f;
		}
	}

	if (!ViewState.GetReference())
	{
		// ViewState.Destroy();
		ViewState.Allocate(World->GetFeatureLevel());
	}

	if (!bIsInitialized)
	{
		SurfaceQueue = MakeShared<FMoviePipelineSurfaceQueue, ESPMode::ThreadSafe>(
			Resolution,
			PixelFormat,
			10,
			true
		);
		ImageWriteQueue = &FModuleManager::Get().LoadModuleChecked<IImageWriteQueueModule>("ImageWriteQueue").GetWriteQueue();
		bIsInitialized = true;
	}

	UE_LOG(LogTemp, Log, TEXT("MovieQualityRenderComponent initialized at %dx%d, PixelFormat=%s, LinearGamma=%d, CaptureSource=%d"),
		Resolution.X, Resolution.Y,
		PixelFormat == PF_FloatRGBA ? TEXT("FloatRGBA") : TEXT("BGRA8"),
		bForceLinearGamma,
		(int32)CaptureSource);
}

void UMovieQualityRenderComponent::Shutdown()
{
	UE_LOG(LogTemp, Warning, TEXT("[CHECKPOINT] MovieQualityRenderComponent::Shutdown() START"));

	if (!bIsInitialized)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CHECKPOINT] MovieQualityRenderComponent::Shutdown() - Not initialized, returning"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[CHECKPOINT] MovieQualityRenderComponent::Shutdown() - Before SurfaceQueue shutdown"));
	if (SurfaceQueue.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CHECKPOINT] MovieQualityRenderComponent::Shutdown() - Calling SurfaceQueue->Shutdown()"));
		SurfaceQueue->Shutdown();
		UE_LOG(LogTemp, Warning, TEXT("[CHECKPOINT] MovieQualityRenderComponent::Shutdown() - SurfaceQueue->Shutdown() completed"));
		SurfaceQueue.Reset();
		UE_LOG(LogTemp, Warning, TEXT("[CHECKPOINT] MovieQualityRenderComponent::Shutdown() - SurfaceQueue.Reset() completed"));
	}

	UE_LOG(LogTemp, Warning, TEXT("[CHECKPOINT] MovieQualityRenderComponent::Shutdown() - Before ViewState cleanup"));
	FSceneViewStateInterface* Ref = ViewState.GetReference();
	if (Ref)
	{
		Ref->ClearMIDPool();
	}
	ViewState.Destroy();
	UE_LOG(LogTemp, Warning, TEXT("[CHECKPOINT] MovieQualityRenderComponent::Shutdown() - ViewState destroyed"));

	for (auto& Pair : RenderTargetPool)
	{
		if (Pair.Value)
		{
			Pair.Value->RemoveFromRoot();
		}
	}
	RenderTargetPool.Empty();

	bIsInitialized = false;
	UE_LOG(LogTemp, Warning, TEXT("[CHECKPOINT] MovieQualityRenderComponent::Shutdown() END"));
}

void UMovieQualityRenderComponent::SaveLitToFile(const FString& OutputPath, TFunction<void(bool)> OnComplete)
{
	UE_LOG(LogTemp, Log, TEXT("[CHECKPOINT] SaveLitToFile START - Path: %s"), *OutputPath);

	if (!bIsInitialized)
	{
		UE_LOG(LogTemp, Error, TEXT("[CHECKPOINT] SaveLitToFile - Not initialized, returning"));
		if (OnComplete)
		{
			OnComplete(false);
		}
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[CHECKPOINT] SaveLitToFile - Creating RenderTarget"));
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
		// RenderTarget->TargetGamma = GEngine->GetDisplayGamma();
		RenderTarget->TargetGamma = ForceTargetGamma;
		RenderTarget->InitCustomFormat(Resolution.X, Resolution.Y, PixelFormat, bForceLinearGamma);
		RenderTarget->AddToRoot();
		RenderTargetPool.Add(PoolKey, RenderTarget);
	}

	UE_LOG(LogTemp, Log, TEXT("[CHECKPOINT] SaveLitToFile - Creating ViewFamily"));
	TSharedPtr<FSceneViewFamilyContext> ViewFamily = CreateViewFamily(RenderTarget);
	if (!ViewFamily.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[CHECKPOINT] SaveLitToFile - ViewFamily creation failed"));
		if (OnComplete)
		{
			OnComplete(false);
		}
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[CHECKPOINT] SaveLitToFile - Creating SceneView"));
	FSceneView* View = CreateSceneView(ViewFamily.Get());
	if (!View)
	{
		UE_LOG(LogTemp, Error, TEXT("[CHECKPOINT] SaveLitToFile - SceneView creation failed"));
		if (OnComplete)
		{
			OnComplete(false);
		}
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[CHECKPOINT] SaveLitToFile - Submitting to renderer"));
	SubmitToRenderer(ViewFamily.Get(), RenderTarget, OutputPath, OnComplete);
	UE_LOG(LogTemp, Log, TEXT("[CHECKPOINT] SaveLitToFile END"));
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
	View->AntiAliasingMethod = AntiAliasingMethod;
	// View->bSceneCaptureUsesRayTracing = true;
	// View->bIsReflectionCapture = true;

	// View->FinalPostProcessSettings.SetBaseValues();
	// View->StartFinalPostprocessSettings(ViewInitOptions.ViewOrigin);

	FPostProcessSettings& PPSettings = View->FinalPostProcessSettings;
    PPSettings.bOverride_ReflectionMethod = 1;
    PPSettings.ReflectionMethod = EReflectionMethod::Type::Lumen;
    // PPSettings.ReflectionMethod = EReflectionMethod::Type::ScreenSpace;
    PPSettings.bOverride_DynamicGlobalIlluminationMethod = 1;
    PPSettings.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::Type::Lumen;
    // PPSettings.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::Type::ScreenSpace;
	PPSettings.bOverride_LumenRayLightingMode = 1;
	PPSettings.LumenRayLightingMode = ELumenRayLightingModeOverride::HitLighting;
	PPSettings.bOverride_LumenSceneLightingQuality = 1;
	PPSettings.LumenSceneLightingQuality = 2.0f;
	PPSettings.bOverride_LumenSceneDetail = 1;
	PPSettings.LumenSceneDetail = 4.0f;
	// PPSettings.bOverride_LumenSceneViewDistance = 1;
	// PPSettings.LumenSceneViewDistance = 2097152.0f;
	PPSettings.bOverride_LumenFinalGatherQuality = 1;
	PPSettings.LumenFinalGatherQuality = 2.0f;
	PPSettings.bOverride_LumenFinalGatherScreenTraces = 1;
	PPSettings.LumenFinalGatherScreenTraces = 1;
	// PPSettings.bOverride_LumenMaxTraceDistance = 1;
	// PPSettings.LumenMaxTraceDistance = 2097152.0f;
	// PPSettings.bOverride_LumenReflectionQuality = 1;
	// PPSettings.LumenReflectionQuality = 2.0f;
	// PPSettings.bOverride_LumenReflectionsScreenTraces = 1;
	// PPSettings.LumenReflectionsScreenTraces = 1;
	// PPSettings.bOverride_LumenFrontLayerTranslucencyReflections = 1;
	// PPSettings.LumenFrontLayerTranslucencyReflections = 1;
	// PPSettings.bOverride_LumenMaxRoughnessToTraceReflections = 1;
	// PPSettings.LumenMaxRoughnessToTraceReflections = 1.0f;
	// PPSettings.bOverride_LumenMaxReflectionBounces = 1;
	// PPSettings.LumenMaxReflectionBounces = 8;
	// PPSettings.bOverride_LumenMaxRefractionBounces = 1;
	// PPSettings.LumenMaxRefractionBounces = 64;
	// PPSettings.bOverride_LumenSurfaceCacheResolution = 1;
	// PPSettings.LumenSurfaceCacheResolution = 1.0f;
	// PPSettings.bOverride_LumenSceneLightingUpdateSpeed = 1;
	// PPSettings.LumenSceneLightingUpdateSpeed = 4.0f;
	// PPSettings.bOverride_LumenFinalGatherLightingUpdateSpeed = 1;
	// PPSettings.LumenFinalGatherLightingUpdateSpeed = 4.0f;

	PPSettings.bOverride_AutoExposureMethod = 1;
	PPSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Histogram;
	PPSettings.bOverride_AutoExposureBias = 1;
	PPSettings.AutoExposureBias = -1.0f;
    PPSettings.bOverride_AutoExposureSpeedDown = 1;
    PPSettings.AutoExposureSpeedDown = 20.0f;
    PPSettings.bOverride_AutoExposureSpeedUp = 1;
    PPSettings.AutoExposureSpeedUp = 20.0f;

	PPSettings.bOverride_MotionBlurAmount = 1;
	PPSettings.bOverride_MotionBlurMax = 1;
	PPSettings.bOverride_MotionBlurTargetFPS = 1;
	PPSettings.bOverride_MotionBlurPerObjectSize = 1;
	PPSettings.MotionBlurAmount = 0.05f;  // default 0.5
	PPSettings.MotionBlurMax = 2.0f;  // default 5.0
	PPSettings.MotionBlurTargetFPS = 24;  // default 30
	PPSettings.MotionBlurPerObjectSize = 0.f;
	// PPSettings.MotionBlurAmount = 0.0f;  // default 0.5
	// PPSettings.MotionBlurMax = 0.f;  // default 5.0
	// PPSettings.MotionBlurTargetFPS = 24;  // default 30
	// PPSettings.MotionBlurPerObjectSize = 0.f;

	// View->EndFinalPostprocessSettings(ViewInitOptions);

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
		// ImageTask->Format = EImageFormat::EXR;
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

// float UMovieQualityRenderComponent::GetTargetGamma() const
// {
// 	// if (bForceLinearGamma)
// 	// {
// 	// 	return 1.0f;
// 	// }
// 	// else
// 	// {
// 		return UTextureRenderTarget::GetDefaultDisplayGamma();
// 	// }
// }
