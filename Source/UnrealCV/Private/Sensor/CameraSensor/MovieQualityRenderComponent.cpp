#include "MovieQualityRenderComponent.h"
#include "LitCamSensor.h"
#include "ImageWriteQueue.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Engine/Canvas.h"
#include "SceneView.h"
#include "EngineModule.h"
#include "RenderingThread.h"
#include "RHICommandList.h"
#include "Modules/ModuleManager.h"
#include "LegacyScreenPercentageDriver.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Sensor/CameraSensor/UnrealCVSurfaceReader.h"
#include "MovieRenderPipelineDataTypes.h"
#include "UnrealcvServer.h"
#include "UnrealcvLog.h"

FMQRCSettings UMovieQualityRenderComponent::GlobalSettings;


void FMovieQualityViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	if (!Component.IsValid()) { return; }
	if (InViewFamily.bIsMainViewFamily)
	{
		if (InViewFamily.Views.Num() > 0 && InViewFamily.Views[0] != nullptr)
		{
			Component->CachedMainViewPostProcessSettings = InViewFamily.Views[0]->FinalPostProcessSettings;
			Component->bHasCachedMainViewPostProcessSettings = true;
			Component->LastMainViewportFrameNumber = InViewFamily.FrameNumber;
		}

		// Process deferred captures AFTER main viewport renders
		Component->ProcessDeferredCaptures();
	}
}

UMovieQualityRenderComponent::UMovieQualityRenderComponent()
  : ShowFlags(EShowFlagInitMode::ESFIM_Game)
{
	bIsInitialized = false;
	FrameCounter = 0;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.0f;


	ShowFlags.SetPostProcessing(true);
	ShowFlags.SetAntiAliasing(true);
	ShowFlags.SetTemporalAA(true);
	ShowFlags.SetScreenPercentage(true);
	ShowFlags.SetMotionBlur(true);

	ShowFlags.SetHair(true);  
	ShowFlags.SetDynamicShadows(true);
	ShowFlags.SetContactShadows(true); 
  	ShowFlags.SetCapsuleShadows(true); 

	ShowFlags.SetPreviewShadowsIndicator(false);

	FServerConfig& Config = FUnrealcvServer::Get().Config;
	CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	FOV = Config.FOV == 0 ? 90 : Config.FOV;

	// other properties need to be initialized when BeginPlay
}

UMovieQualityRenderComponent::~UMovieQualityRenderComponent()
{
	Shutdown();
}

void UMovieQualityRenderComponent::BeginPlay()
{
	Super::BeginPlay();

	// AActor* Owner = GetOwner();
	// if (Owner)
	// {
	// 	auto* ParentSensor = Cast<UFusionCamSensor>(Owner->GetComponentByClass(UFusionCamSensor::StaticClass()));
	// 	if (ParentSensor)
	// 	{
	// 		UE_LOG(LogTemp, Log, TEXT("MovieQualityRenderComponent: Found parent FusionCamSensor"));
	// 	}
	// 	else
	// 	{
	// 		UE_LOG(LogTemp, Warning, TEXT("MovieQualityRenderComponent: can not Found parent FusionCamSensor!"));
	// 	}
	// }

	FServerConfig& Config = FUnrealcvServer::Get().Config;
	int32 ResWidth = Config.Width == 0 ? 640 : Config.Width;
	int32 ResHeight = Config.Height == 0 ? 480 : Config.Height;

	Initialize(ResWidth, ResHeight);

	// Register view extension to capture main viewport's PostProcessSettings
	// NewExtension automatically handles registration
	ViewExtension = FSceneViewExtensions::NewExtension<FMovieQualityViewExtension>(this);
}

void UMovieQualityRenderComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Shutdown();
	Super::EndPlay(EndPlayReason);
}

void UMovieQualityRenderComponent::Initialize(int32 ResolutionX, int32 ResolutionY)
{
	UE_LOG(LogTemp, Warning, TEXT("5"));
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("MovieQualityRenderComponent: No valid world"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("5"));
	Resolution.X = ResolutionX;
	Resolution.Y = ResolutionY;

	UE_LOG(LogTemp, Warning, TEXT("5"));
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

	UE_LOG(LogTemp, Warning, TEXT("5"));
	if (ViewState.GetReference())
	{
		ViewState.Destroy();
	}
	UE_LOG(LogTemp, Warning, TEXT("5"));
	ViewState.Allocate(World->GetFeatureLevel());

	UE_LOG(LogTemp, Warning, TEXT("5"));
	if (SurfaceQueue.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("8"));
		SurfaceQueue->Shutdown();
		UE_LOG(LogTemp, Warning, TEXT("8"));
		SurfaceQueue.Reset();
		UE_LOG(LogTemp, Warning, TEXT("8"));
	}
	UE_LOG(LogTemp, Warning, TEXT("5"));
	SurfaceQueue = MakeShared<FUnrealCVSurfaceQueue, ESPMode::ThreadSafe>(
		Resolution,
		PixelFormat,
		10,
		true
	);

	UE_LOG(LogTemp, Warning, TEXT("5"));
	if (!bIsInitialized)
	{
		ImageWriteQueue = MakeShared<FUnrealCVImageWriteQueue>();
	}

	UE_LOG(LogTemp, Warning, TEXT("5"));

	// Initialize deferred capture state
	bFirstDeferredCapture = true;
	LastMainViewportFrameNumber = 0;
	bHasCachedMainViewPostProcessSettings = false;

	bIsInitialized = true;

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

	if (ImageWriteQueue.IsValid())
	{
		ImageWriteQueue->Shutdown();
		ImageWriteQueue.Reset();
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

	ViewExtension.Reset();

	bIsInitialized = false;
	UE_LOG(LogTemp, Warning, TEXT("[CHECKPOINT] MovieQualityRenderComponent::Shutdown() END"));
}

void UMovieQualityRenderComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bRenderEveryFrame && DeferredCaptureQueue.IsEmpty())
	{
		EnqueueDeferredCapture([](TUniquePtr<FImagePixelData>&& Input) {}, false);
	}
}


void UMovieQualityRenderComponent::CaptureDiscardFrame()
{
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
		RenderTarget->InitCustomFormat(Resolution.X, Resolution.Y, PixelFormat, true);
		RenderTargetPool.Add(PoolKey, RenderTarget);
	}

	TSharedPtr<FSceneViewFamilyContext> ViewFamily = CreateViewFamily(RenderTarget);
	if (!ViewFamily.IsValid())
	{
		return;
	}

	FSceneView* View = CreateSceneView(ViewFamily.Get());
	if (!View)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->SendAllEndOfFrameUpdates();

	FRenderTarget* RenderTargetResource = RenderTarget->GameThread_GetRenderTargetResource();
	FCanvas Canvas(RenderTargetResource, nullptr, World, ViewFamily->GetFeatureLevel(), FCanvas::CDM_DeferDrawing, 1.0f);
	GetRendererModule().BeginRenderingViewFamily(&Canvas, ViewFamily.Get());

	// Render but discard result - no callback, no SurfaceQueue processing
	ENQUEUE_RENDER_COMMAND(DiscardFrameCommand)(
		[RenderTargetResource](FRHICommandListImmediate& RHICmdList) mutable
		{
			// Frame is rendered but nothing is done with it
			// Lumen temporal state is updated, TAA history is preserved
		}
	);
}

void UMovieQualityRenderComponent::FlushPendingFrames()
{
	if (!bIsInitialized || !SurfaceQueue)
	{
		UE_LOG(LogTemp, Warning, TEXT("FlushPendingFrames - Not initialized or no SurfaceQueue"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("FlushPendingFrames - Starting flush of pending GPU readback frames"));
	ProcessDeferredCaptures();
	SurfaceQueue->Shutdown();

	UE_LOG(LogTemp, Log, TEXT("FlushPendingFrames - Flush completed"));
}

void UMovieQualityRenderComponent::CaptureFrame(TFunction<void(TUniquePtr<FImagePixelData>&&)> OnPixelDataReady)
{
	if (!bIsInitialized)
	{
		UE_LOG(LogTemp, Error, TEXT("CaptureFrame - Not initialized"));
		return;
	}

	if (NumWarmup > 0)
	{
		CaptureDiscardFrame();
		UE_LOG(LogTemp, Warning, TEXT("CaptureFrame - Warmup"));
	}

	if (bRenderImmediately)
	{
		ExecuteCaptureFrame(OnPixelDataReady);
	}
	else
	{
		// Deferred path - execute in ViewExtension after main viewport renders
		EnqueueDeferredCapture(MoveTemp(OnPixelDataReady), false);
	}
}

void UMovieQualityRenderComponent::ExecuteCaptureFrame(TFunction<void(TUniquePtr<FImagePixelData>&&)> OnPixelDataReady)
{

	// ResetAllTemporalState();
	if (NumWarmup - 1 > 0)
	{
		for (int32 N = 0; N < NumWarmup - 1; N += 1)
		{
			CaptureDiscardFrame();
			UE_LOG(LogTemp, Warning, TEXT("CaptureFrame - Warmup"));
		}
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
		RenderTarget->InitAutoFormat(Resolution.X, Resolution.Y);
		RenderTarget->AddToRoot();
		RenderTargetPool.Add(PoolKey, RenderTarget);
	}

	TSharedPtr<FSceneViewFamilyContext> ViewFamily = CreateViewFamily(RenderTarget);
	if (!ViewFamily.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("ExecuteCaptureFrame - ViewFamily creation failed"));
		return;
	}

	FSceneView* View = CreateSceneView(ViewFamily.Get());
	if (!View)
	{
		UE_LOG(LogTemp, Error, TEXT("ExecuteCaptureFrame - SceneView creation failed"));
		return;
	}

	SubmitToRendererWithCallback(ViewFamily.Get(), RenderTarget, MoveTemp(OnPixelDataReady));
}

void UMovieQualityRenderComponent::CaptureFrameToFile(const FString& OutputPath, TFunction<void(bool)> OnComplete)
{
	if (!IsInitialized())
	{
		UE_LOG(LogUnrealCV, Error, TEXT("MQRC not initialized"));
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("[CHECKPOINT] CaptureFrameToFile START - Path: %s"), *OutputPath);

	CaptureFrame([this, OutputPath, OnComplete](TUniquePtr<FImagePixelData>&& InPixelData)
	{
		if (!InPixelData.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("[CHECKPOINT] CaptureFrameToFile - Invalid pixel data"));
			if (OnComplete)
			{
				OnComplete(false);
			}
			return;
		}

		TUniquePtr<FUnrealCVImageWriteTask> ImageTask = MakeUnique<FUnrealCVImageWriteTask>();
		ImageTask->PixelData = MoveTemp(InPixelData);
		ImageTask->Filename = OutputPath;
		ImageTask->Format = EImageFormat::PNG;
		ImageTask->CompressionQuality = 100;

		ImageTask->OnCompleted = [OnComplete](bool bSuccess)
		{
			if (OnComplete)
			{
				OnComplete(bSuccess);
			}
		};

		ImageWriteQueue->Enqueue(MoveTemp(ImageTask));
	});

	UE_LOG(LogTemp, Log, TEXT("[CHECKPOINT] CaptureFrameToFile END"));
}

void UMovieQualityRenderComponent::SetShowOnlyComponents(const TArray<TWeakObjectPtr<UPrimitiveComponent>>& InComponents)
{
	ShowOnlyComponents = InComponents;
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
	ViewFamily->SetScreenPercentageInterface(new FLegacyScreenPercentageDriver(*ViewFamily, GlobalSettings.ScreenPercentage));

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
	View->AntiAliasingMethod = GlobalSettings.AntiAliasingMethod;
	auto SPM = GlobalSettings.PrimaryScreenPercentageMethod;
	if (GlobalSettings.AntiAliasingMethod == EAntiAliasingMethod::AAM_FXAA)
	{
		SPM = EPrimaryScreenPercentageMethod::SpatialUpscale;
		UE_LOG(LogTemp, Warning, TEXT("FXAA force use SpatialUpscale"));
	}
	View->PrimaryScreenPercentageMethod = SPM;
	// View->bSceneCaptureUsesRayTracing = true;
	// View->bIsReflectionCapture = true;
	// View->bIsSceneCapture = false;
	// View->bIsSceneCaptureCube = false;
	// View->bIsGameView = true;

	View->OverrideFrameIndexValue = FrameCounter++;
	// View->bAllowTemporalJitter = false;

	SetPostProcessSettings(PostProcessSettings);

	View->StartFinalPostprocessSettings(ViewInitOptions.ViewOrigin);

	if (!bHasCachedMainViewPostProcessSettings)
	{
		UE_LOG(LogTemp, Error, TEXT("bHasCachedMainViewPostProcessSettings = false"));
	}

	if (bHasCachedMainViewPostProcessSettings)
	{
		// Use the cached main viewport's PostProcessSettings
		View->FinalPostProcessSettings = CachedMainViewPostProcessSettings;
		UE_LOG(LogTemp, Warning, TEXT("Use the cached main viewport's PostProcessSettings"));
	}
	else
	{
		// Initialize with default settings (similar to SceneCaptureComponent2D)
		View->FinalPostProcessSettings.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::None;
		View->FinalPostProcessSettings.ReflectionMethod = EReflectionMethod::None;
		View->FinalPostProcessSettings.LumenSurfaceCacheResolution = 0.5f;
		UE_LOG(LogTemp, Warning, TEXT("Initialize with default settings PostProcessSettings"));
	}

	View->OverridePostProcessSettings(PostProcessSettings, PostProcessBlendWeight);

	View->EndFinalPostprocessSettings(ViewInitOptions);

	if (ShowOnlyComponents.Num() > 0)
	{
		TSet<FPrimitiveComponentId> VisiblePrimitives;
		for (const TWeakObjectPtr<UPrimitiveComponent>& CompPtr : ShowOnlyComponents)
		{
			if (UPrimitiveComponent* Comp = CompPtr.Get())
			{
				FPrimitiveComponentId Id = Comp->GetPrimitiveSceneId();
				if (Id.IsValid())
				{
					VisiblePrimitives.Add(Id);
				}
			}
		}
		View->ShowOnlyPrimitives = TOptional<TSet<FPrimitiveComponentId>>(VisiblePrimitives);
	}

	ViewFamily->Views.Add(View);

	return View;
}

void UMovieQualityRenderComponent::SubmitToRendererWithCallback(
	FSceneViewFamily* ViewFamily,
	UTextureRenderTarget2D* RenderTarget,
	TFunction<void(TUniquePtr<FImagePixelData>&&)> OnPixelDataReady)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// We must push any deferred render state recreations before causing any rendering to happen, to make sure that deleted resource references are updated
	World->SendAllEndOfFrameUpdates();

	// // Force wait for all pending rendering commands to complete (Groom/Hair, shadows, etc.)
	// FlushRenderingCommands();

	FRenderTarget* RenderTargetResource = RenderTarget->GameThread_GetRenderTargetResource();

	FCanvas Canvas(RenderTargetResource, nullptr, World, ViewFamily->GetFeatureLevel(), FCanvas::CDM_DeferDrawing, 1.0f);
	GetRendererModule().BeginRenderingViewFamily(&Canvas, ViewFamily);

	TSharedRef<FImagePixelDataPayload, ESPMode::ThreadSafe> FramePayload = MakeShared<FImagePixelDataPayload, ESPMode::ThreadSafe>();

	ENQUEUE_RENDER_COMMAND(CaptureFrameCommand)(
		[SurfaceQueue = this->SurfaceQueue, FramePayload, OnPixelDataReady, RenderTargetResource](FRHICommandListImmediate& RHICmdList) mutable
		{
			SurfaceQueue->OnRenderTargetReady_RenderThread(
				RenderTargetResource->GetRenderTargetTexture(),
				FramePayload,
				MoveTemp(OnPixelDataReady)
			);
		}
	);

	// FlushRenderingCommands();
	// TArray<FColor> Image;
	// FReadSurfaceDataFlags ReadSurfaceDataFlags(RCM_MinMax);
	// ReadSurfaceDataFlags.SetLinearToGamma(false);
	// RenderTargetResource->ReadPixels(Image, ReadSurfaceDataFlags);

	// int32 Width = RenderTarget->SizeX;
	// int32 Height = RenderTarget->SizeY;
	// int32 ExpectedSize = Width * Height;

	// if (Image.Num() != ExpectedSize)
	// {
	// 	UE_LOG(LogUnrealCV, Error, TEXT("ReadPixels failed: expected %d pixels (%dx%d), got %d"),
	// 		ExpectedSize, Width, Height, Image.Num());
	// 	return;
	// }

	// TUniquePtr<FImagePixelData> ImageData = MakeUnique<TImagePixelData<FColor>>(
	// 	FIntPoint(Width, Height),
	// 	TArray64<FColor>(MoveTemp(Image))
	// );

	// OnPixelDataReady(MoveTemp(ImageData));
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


void UMovieQualityRenderComponent::SetPostProcessSettings(FPostProcessSettings& PPSettings)
{
	SetDefaultPostProcessSettings(PPSettings);
}

void UMovieQualityRenderComponent::SetDefaultPostProcessSettings(FPostProcessSettings& PPSettings)
{
    PPSettings.bOverride_ReflectionMethod = 1;
    PPSettings.ReflectionMethod = EReflectionMethod::Type::Lumen;
    // PPSettings.ReflectionMethod = EReflectionMethod::Type::ScreenSpace;
    PPSettings.bOverride_DynamicGlobalIlluminationMethod = 1;
    PPSettings.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::Type::Lumen;
    // PPSettings.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::Type::ScreenSpace;
	PPSettings.bOverride_LumenRayLightingMode = 1;
	PPSettings.LumenRayLightingMode = ELumenRayLightingModeOverride::HitLightingForReflections;
	PPSettings.bOverride_LumenSceneLightingQuality = 1;
	PPSettings.LumenSceneLightingQuality = GlobalSettings.LumenSceneLightingQuality;
	PPSettings.bOverride_LumenSceneDetail = 1;
	PPSettings.LumenSceneDetail = 4.0f;
	// PPSettings.bOverride_LumenSceneViewDistance = 1;
	// PPSettings.LumenSceneViewDistance = 2097152.0f;
	PPSettings.bOverride_LumenFinalGatherQuality = 1;
	PPSettings.LumenFinalGatherQuality = GlobalSettings.LumenFinalGatherQuality;
	PPSettings.bOverride_LumenFinalGatherScreenTraces = 1;
	PPSettings.LumenFinalGatherScreenTraces = 1;
	// PPSettings.bOverride_LumenMaxTraceDistance = 1;
	// PPSettings.LumenMaxTraceDistance = 2097152.0f;
	PPSettings.bOverride_LumenReflectionQuality = 1;
	PPSettings.LumenReflectionQuality = 2.0f;
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
	PPSettings.bOverride_LumenSurfaceCacheResolution = 1;
	PPSettings.LumenSurfaceCacheResolution = 1.0f;

	PPSettings.bOverride_AmbientOcclusionQuality = true;
	PPSettings.AmbientOcclusionQuality = 80.0f;
	PPSettings.bOverride_RayTracingAO = true;
	PPSettings.RayTracingAO = true;
	PPSettings.bOverride_RayTracingAOSamplesPerPixel = true;
	PPSettings.RayTracingAOSamplesPerPixel = 4;

	/////////////////////////////////////////////////////////
	// reduce ghosting phenomenon
	// https://www.reddit.com/r/UnrealEngine5/comments/182y8br/lumen_ghosting_on_moving_objects_please_help/
	// https://forums.unrealengine.com/t/desperate-for-a-definitve-answer-on-lumen-ghosting-issue/661853
	// https://dev.epicgames.com/community/learning/tutorials/mjo7/unreal-engine-temporal-quality-guide
	// PPSettings.bOverride_LumenSceneLightingUpdateSpeed = 1;
	// PPSettings.LumenSceneLightingUpdateSpeed = 2.0f;
	PPSettings.bOverride_LumenFinalGatherLightingUpdateSpeed = 1;
	PPSettings.LumenFinalGatherLightingUpdateSpeed = 8.0f;
	// PPSettings.bOverride_LumenFinalGatherScreenTraces = 1;
	// PPSettings.LumenFinalGatherScreenTraces = 0;
	// PPSettings.bOverride_AmbientOcclusionTemporalBlendWeight = 1;
	// PPSettings.AmbientOcclusionTemporalBlendWeight = 0.0f;
	/////////////////////////////////////////////////////////

	PPSettings.bOverride_AutoExposureMethod = 1;
	PPSettings.AutoExposureMethod = GlobalSettings.ExposureMethod;
	// PPSettings.bOverride_AutoExposureBias = 1;
	// PPSettings.AutoExposureBias = GlobalSettings.ExposureBias;
	// PPSettings.bOverride_AutoExposureMinBrightness = 1;
	// PPSettings.AutoExposureMinBrightness = GlobalSettings.AutoExposureMinBrightness;
	// PPSettings.bOverride_AutoExposureMaxBrightness = 1;
	// PPSettings.AutoExposureMaxBrightness = GlobalSettings.AutoExposureMaxBrightness;
    PPSettings.bOverride_AutoExposureSpeedDown = 1;
    PPSettings.AutoExposureSpeedDown = 20.0f;
    PPSettings.bOverride_AutoExposureSpeedUp = 1;
    PPSettings.AutoExposureSpeedUp = 20.0f;

  	// DOF
  	PPSettings.bOverride_DepthOfFieldScale = true;
  	PPSettings.DepthOfFieldScale = GlobalSettings.DepthOfFieldScale;
  	// PPSettings.bOverride_DepthOfFieldFstop = true;
  	// PPSettings.DepthOfFieldFstop = 8.0f;
	// PPSettings.bOverride_DepthOfFieldSensorWidth = true;
	// PPSettings.DepthOfFieldSensorWidth = 35.0f;
    // PPSettings.bOverride_DepthOfFieldFocalDistance = true;
    // PPSettings.DepthOfFieldFocalDistance = 100.0f;
    // PPSettings.bOverride_DepthOfFieldFocalRegion = true;
    // PPSettings.DepthOfFieldFocalRegion = 2000.0f;

	PPSettings.bOverride_MotionBlurAmount = 1;
	PPSettings.bOverride_MotionBlurMax = 1;
	PPSettings.bOverride_MotionBlurTargetFPS = 1;
	PPSettings.bOverride_MotionBlurPerObjectSize = 1;
	PPSettings.MotionBlurAmount = GlobalSettings.MotionBlurAmount;
	PPSettings.MotionBlurMax = 2.0f;  // default 5.0
	PPSettings.MotionBlurTargetFPS = 30;  // default 30
	PPSettings.MotionBlurPerObjectSize = 0.f;
	// PPSettings.MotionBlurAmount = 0.0f;  // default 0.5
	// PPSettings.MotionBlurMax = 0.f;  // default 5.0
	// PPSettings.MotionBlurTargetFPS = 24;  // default 30
	// PPSettings.MotionBlurPerObjectSize = 0.f;

	FVector4 Saturation = FVector4(GlobalSettings.Saturation, GlobalSettings.Saturation, GlobalSettings.Saturation, 1.0f);
	FVector4 Contrast = FVector4(GlobalSettings.Contrast, GlobalSettings.Contrast, GlobalSettings.Contrast, 1.0f);
	FVector4 Gamma = FVector4(GlobalSettings.Gamma, GlobalSettings.Gamma, GlobalSettings.Gamma, 1.0f);
	FVector4 Gain = FVector4(GlobalSettings.Gain, GlobalSettings.Gain, GlobalSettings.Gain, 1.0f);
	FVector4 Offset = FVector4(0.0f, 0.0f, 0.0f, 0.0f);

	PPSettings.bOverride_ColorSaturation = 1;
	PPSettings.ColorSaturation = Saturation;
	PPSettings.bOverride_ColorContrast = 1;
	PPSettings.ColorContrast = Contrast;
	PPSettings.bOverride_ColorGamma = 1;
	PPSettings.ColorGamma = Gamma;
	PPSettings.bOverride_ColorGain = 1;
	PPSettings.ColorGain = Gain;
	PPSettings.bOverride_ColorOffset = 1;
	PPSettings.ColorOffset = Offset;

	PPSettings.bOverride_ColorSaturationShadows = 1;
	PPSettings.ColorSaturationShadows = Saturation;
	PPSettings.bOverride_ColorContrastShadows = 1;
	PPSettings.ColorContrastShadows = Contrast;
	PPSettings.bOverride_ColorGammaShadows = 1;
	PPSettings.ColorGammaShadows = Gamma;
	PPSettings.bOverride_ColorGainShadows = 1;
	PPSettings.ColorGainShadows = Gain;
	PPSettings.bOverride_ColorOffsetShadows = 1;
	PPSettings.ColorOffsetShadows = Offset;

	PPSettings.bOverride_ColorSaturationMidtones = 1;
	PPSettings.ColorSaturationMidtones = Saturation;
	PPSettings.bOverride_ColorContrastMidtones = 1;
	PPSettings.ColorContrastMidtones = Contrast;
	PPSettings.bOverride_ColorGammaMidtones = 1;
	PPSettings.ColorGammaMidtones = Gamma;
	PPSettings.bOverride_ColorGainMidtones = 1;
	PPSettings.ColorGainMidtones = Gain;
	PPSettings.bOverride_ColorOffsetMidtones = 1;
	PPSettings.ColorOffsetMidtones = Offset;

	PPSettings.bOverride_ColorSaturationHighlights = 1;
	PPSettings.ColorSaturationHighlights = Saturation;
	PPSettings.bOverride_ColorContrastHighlights = 1;
	PPSettings.ColorContrastHighlights = Contrast;
	PPSettings.bOverride_ColorGammaHighlights = 1;
	PPSettings.ColorGammaHighlights = Gamma;
	PPSettings.bOverride_ColorGainHighlights = 1;
	PPSettings.ColorGainHighlights = Gain;
	PPSettings.bOverride_ColorOffsetHighlights = 1;
	PPSettings.ColorOffsetHighlights = Offset;

	PPSettings.bOverride_ToneCurveAmount = 1;
	PPSettings.ToneCurveAmount = 1.0f;
	PPSettings.bOverride_ExpandGamut = 1;
	PPSettings.ExpandGamut = 1.0f;

	// PPSettings.bOverride_Sharpen = 1;
	// PPSettings.Sharpen = 0.0f;
	// PPSettings.bOverride_FilmGrainIntensity = 1;
	// PPSettings.FilmGrainIntensity = 0.0f;
}

void UMovieQualityRenderComponent::EnqueueDeferredCapture(TFunction<void(TUniquePtr<FImagePixelData>&&)> OnPixelDataReady, bool bIsDiscardFrame)
{
	FScopeLock Lock(&QueueLock);

	FDeferredCaptureRequest Request;
	Request.OnPixelDataReady = MoveTemp(OnPixelDataReady);
	Request.EnqueueTime = FPlatformTime::Seconds();
	Request.FrameNumber = GFrameCounter;
	Request.bIsDiscardFrame = bIsDiscardFrame;

	DeferredCaptureQueue.Enqueue(Request);

	UE_LOG(LogTemp, Verbose, TEXT("MQRC: Enqueued deferred capture (Frame %u)"), Request.FrameNumber);
}

void UMovieQualityRenderComponent::ProcessDeferredCaptures()
{
	if (!bIsInitialized)
	{
		return;
	}

	FScopeLock Lock(&QueueLock);

	// Empty queue check
	if (DeferredCaptureQueue.IsEmpty())
	{
		return;
	}

	// Lumen cache management - clear on first deferred capture
	if (bFirstDeferredCapture)
	{
		ResetAllTemporalState();
		bFirstDeferredCapture = false;
	}

	// Frame counter sync
	uint32 SavedFrameCounter = FrameCounter;
	FrameCounter = LastMainViewportFrameNumber;

	// Process all queued captures
	FDeferredCaptureRequest Request;
	while (DeferredCaptureQueue.Dequeue(Request))
	{
		double Latency = FPlatformTime::Seconds() - Request.EnqueueTime;
		UE_LOG(LogTemp, Verbose,
			TEXT("MQRC: Processing deferred capture (Enqueued Frame %u, Latency %.2fms)"),
			Request.FrameNumber, Latency * 1000.0);

		if (Request.bIsDiscardFrame)
		{
			CaptureDiscardFrame();
		}
		else
		{
			ExecuteCaptureFrame(MoveTemp(Request.OnPixelDataReady));
		}
	}

	// Restore frame counter (optional, depends on temporal requirements)
	// FrameCounter = SavedFrameCounter;
}

void UMovieQualityRenderComponent::ResetAllTemporalState()
{
	FSceneViewStateInterface * ViewStateRef = static_cast<FSceneViewStateInterface *>(ViewState.GetReference());
	if (ViewStateRef)
	{
		// ViewStateRef->Lumen.SafeRelease();
		ViewStateRef->ResetViewState();  // 重置所有temporal state
		UE_LOG(LogTemp, Warning, TEXT("MQRC: FULL ViewState reset (including TAA/FrameIndex)"));
	}
}

/*

● Based on my search through Scene.h, here is a comprehensive list of all shadow, global illumination, and Lumen-related settings in FPostProcessSettings:

  Global Illumination

  Method & Basic GI
  ┌─────────────────────────────────┬────────────────────────────────────────┬─────────────────────┐
  │             Setting             │                  Type                  │      Category       │
  ├─────────────────────────────────┼────────────────────────────────────────┼─────────────────────┤
  │ DynamicGlobalIlluminationMethod │ EDynamicGlobalIlluminationMethod::Type │ Global Illumination │
  ├─────────────────────────────────┼────────────────────────────────────────┼─────────────────────┤
  │ IndirectLightingColor           │ FLinearColor                           │ Global Illumination │
  ├─────────────────────────────────┼────────────────────────────────────────┼─────────────────────┤
  │ IndirectLightingIntensity       │ float                                  │ Global Illumination │
  └─────────────────────────────────┴────────────────────────────────────────┴─────────────────────┘
  Lumen Global Illumination
  ┌─────────────────────────────────────┬───────────────────────────────┬───────────────────────────────────────────────┐
  │               Setting               │             Type              │                   Category                    │
  ├─────────────────────────────────────┼───────────────────────────────┼───────────────────────────────────────────────┤
  │ LumenRayLightingMode                │ ELumenRayLightingModeOverride │ Global Illumination|Lumen Global Illumination │
  ├─────────────────────────────────────┼───────────────────────────────┼───────────────────────────────────────────────┤
  │ LumenSceneLightingQuality           │ float (0.25-2)                │ Global Illumination|Lumen Global Illumination │
  ├─────────────────────────────────────┼───────────────────────────────┼───────────────────────────────────────────────┤
  │ LumenSceneDetail                    │ float (0.25-4)                │ Global Illumination|Lumen Global Illumination │
  ├─────────────────────────────────────┼───────────────────────────────┼───────────────────────────────────────────────┤
  │ LumenSceneViewDistance              │ float (1-2097152)             │ Global Illumination|Lumen Global Illumination │
  ├─────────────────────────────────────┼───────────────────────────────┼───────────────────────────────────────────────┤
  │ LumenSceneLightingUpdateSpeed       │ float (0.5-4)                 │ Global Illumination|Lumen Global Illumination │
  ├─────────────────────────────────────┼───────────────────────────────┼───────────────────────────────────────────────┤
  │ LumenFinalGatherQuality             │ float (0.25-2)                │ Global Illumination|Lumen Global Illumination │
  ├─────────────────────────────────────┼───────────────────────────────┼───────────────────────────────────────────────┤
  │ LumenFinalGatherLightingUpdateSpeed │ float (0.5-4)                 │ Global Illumination|Lumen Global Illumination │
  ├─────────────────────────────────────┼───────────────────────────────┼───────────────────────────────────────────────┤
  │ LumenFinalGatherScreenTraces        │ uint8 (bool)                  │ Global Illumination|Lumen Global Illumination │
  ├─────────────────────────────────────┼───────────────────────────────┼───────────────────────────────────────────────┤
  │ LumenMaxTraceDistance               │ float (1-2097152)             │ Global Illumination|Lumen Global Illumination │
  ├─────────────────────────────────────┼───────────────────────────────┼───────────────────────────────────────────────┤
  │ LumenDiffuseColorBoost              │ float (0.01-4)                │ Global Illumination|Lumen Global Illumination │
  ├─────────────────────────────────────┼───────────────────────────────┼───────────────────────────────────────────────┤
  │ LumenSkylightLeaking                │ float (0-0.02)                │ Global Illumination|Lumen Global Illumination │
  ├─────────────────────────────────────┼───────────────────────────────┼───────────────────────────────────────────────┤
  │ LumenSkylightLeakingTint            │ FLinearColor                  │ Global Illumination|Lumen Global Illumination │
  ├─────────────────────────────────────┼───────────────────────────────┼───────────────────────────────────────────────┤
  │ LumenFullSkylightLeakingDistance    │ float (0.1-2000)              │ Global Illumination|Lumen Global Illumination │
  ├─────────────────────────────────────┼───────────────────────────────┼───────────────────────────────────────────────┤
  │ LumenSurfaceCacheResolution         │ float (0.5-1)                 │ Global Illumination|Lumen Global Illumination │
  └─────────────────────────────────────┴───────────────────────────────┴───────────────────────────────────────────────┘
  Ray Tracing GI (Overrides exist but properties may be deprecated in UE 5.6)

  Reflections

  Method
  ┌──────────────────┬─────────────────────────┬─────────────┐
  │     Setting      │          Type           │  Category   │
  ├──────────────────┼─────────────────────────┼─────────────┤
  │ ReflectionMethod │ EReflectionMethod::Type │ Reflections │
  └──────────────────┴─────────────────────────┴─────────────┘
  Lumen Reflections
  ┌────────────────────────────────────────┬────────────────┬───────────────────────────────┐
  │                Setting                 │      Type      │           Category            │
  ├────────────────────────────────────────┼────────────────┼───────────────────────────────┤
  │ LumenReflectionQuality                 │ float (0.25-2) │ Reflections|Lumen Reflections │
  ├────────────────────────────────────────┼────────────────┼───────────────────────────────┤
  │ LumenReflectionsScreenTraces           │ uint8 (bool)   │ Reflections|Lumen Reflections │
  ├────────────────────────────────────────┼────────────────┼───────────────────────────────┤
  │ LumenFrontLayerTranslucencyReflections │ uint8 (bool)   │ Reflections|Lumen Reflections │
  ├────────────────────────────────────────┼────────────────┼───────────────────────────────┤
  │ LumenMaxRoughnessToTraceReflections    │ float (0-1)    │ Reflections|Lumen Reflections │
  ├────────────────────────────────────────┼────────────────┼───────────────────────────────┤
  │ LumenMaxReflectionBounces              │ int32 (1-8)    │ Reflections|Lumen Reflections │
  ├────────────────────────────────────────┼────────────────┼───────────────────────────────┤
  │ LumenMaxRefractionBounces              │ int32 (0-64)   │ Reflections|Lumen Reflections │
  └────────────────────────────────────────┴────────────────┴───────────────────────────────┘
  Screen Space Reflections
  ┌───────────────────────────────────┬────────────────┬──────────────────────────────────────┐
  │              Setting              │      Type      │               Category               │
  ├───────────────────────────────────┼────────────────┼──────────────────────────────────────┤
  │ ScreenSpaceReflectionIntensity    │ float (0-100)  │ Reflections|Screen Space Reflections │
  ├───────────────────────────────────┼────────────────┼──────────────────────────────────────┤
  │ ScreenSpaceReflectionQuality      │ float (0-100)  │ Reflections|Screen Space Reflections │
  ├───────────────────────────────────┼────────────────┼──────────────────────────────────────┤
  │ ScreenSpaceReflectionMaxRoughness │ float (0.01-1) │ Reflections|Screen Space Reflections │
  └───────────────────────────────────┴────────────────┴──────────────────────────────────────┘
  Ambient Occlusion
  ┌─────────────────────────────────────┬─────────────────┬──────────────────────────────────────┐
  │               Setting               │      Type       │               Category               │
  ├─────────────────────────────────────┼─────────────────┼──────────────────────────────────────┤
  │ AmbientOcclusionIntensity           │ float (0-1)     │ Rendering Features|Ambient Occlusion │
  ├─────────────────────────────────────┼─────────────────┼──────────────────────────────────────┤
  │ AmbientOcclusionStaticFraction      │ float (0-1)     │ Rendering Features|Ambient Occlusion │
  ├─────────────────────────────────────┼─────────────────┼──────────────────────────────────────┤
  │ AmbientOcclusionRadius              │ float (0.1-500) │ Rendering Features|Ambient Occlusion │
  ├─────────────────────────────────────┼─────────────────┼──────────────────────────────────────┤
  │ AmbientOcclusionRadiusInWS          │ uint32 (bool)   │ Rendering Features|Ambient Occlusion │
  ├─────────────────────────────────────┼─────────────────┼──────────────────────────────────────┤
  │ AmbientOcclusionFadeDistance        │ float (0-20000) │ Rendering Features|Ambient Occlusion │
  ├─────────────────────────────────────┼─────────────────┼──────────────────────────────────────┤
  │ AmbientOcclusionFadeRadius          │ float (0-20000) │ Rendering Features|Ambient Occlusion │
  ├─────────────────────────────────────┼─────────────────┼──────────────────────────────────────┤
  │ AmbientOcclusionPower               │ float (0.1-8)   │ Rendering Features|Ambient Occlusion │
  ├─────────────────────────────────────┼─────────────────┼──────────────────────────────────────┤
  │ AmbientOcclusionBias                │ float (0-10)    │ Rendering Features|Ambient Occlusion │
  ├─────────────────────────────────────┼─────────────────┼──────────────────────────────────────┤
  │ AmbientOcclusionQuality             │ float (0-100)   │ Rendering Features|Ambient Occlusion │
  ├─────────────────────────────────────┼─────────────────┼──────────────────────────────────────┤
  │ AmbientOcclusionMipBlend            │ float (0.1-1)   │ Rendering Features|Ambient Occlusion │
  ├─────────────────────────────────────┼─────────────────┼──────────────────────────────────────┤
  │ AmbientOcclusionMipScale            │ float (0.5-4)   │ Rendering Features|Ambient Occlusion │
  ├─────────────────────────────────────┼─────────────────┼──────────────────────────────────────┤
  │ AmbientOcclusionMipThreshold        │ float (0-0.1)   │ Rendering Features|Ambient Occlusion │
  ├─────────────────────────────────────┼─────────────────┼──────────────────────────────────────┤
  │ AmbientOcclusionTemporalBlendWeight │ float (0-0.5)   │ Rendering Features|Ambient Occlusion │
  └─────────────────────────────────────┴─────────────────┴──────────────────────────────────────┘
  Ray Tracing Ambient Occlusion
  ┌─────────────────────────────┬─────────────────┬──────────────────────────────────────────────────┐
  │           Setting           │      Type       │                     Category                     │
  ├─────────────────────────────┼─────────────────┼──────────────────────────────────────────────────┤
  │ RayTracingAO                │ uint32 (bool)   │ Rendering Features|Ray Tracing Ambient Occlusion │
  ├─────────────────────────────┼─────────────────┼──────────────────────────────────────────────────┤
  │ RayTracingAOSamplesPerPixel │ int32 (1-65536) │ Rendering Features|Ray Tracing Ambient Occlusion │
  ├─────────────────────────────┼─────────────────┼──────────────────────────────────────────────────┤
  │ RayTracingAOIntensity       │ float (0-1)     │ Rendering Features|Ray Tracing Ambient Occlusion │
  ├─────────────────────────────┼─────────────────┼──────────────────────────────────────────────────┤
  │ RayTracingAORadius          │ float (0-10000) │ Rendering Features|Ray Tracing Ambient Occlusion │
  └─────────────────────────────┴─────────────────┴──────────────────────────────────────────────────┘
  Ray Tracing Translucency (Shadows)
  ┌───────────────────────────────┬────────────────────────────────────────┬─────────────────────────────────┐
  │            Setting            │                  Type                  │            Category             │
  ├───────────────────────────────┼────────────────────────────────────────┼─────────────────────────────────┤
  │ RayTracingTranslucencyShadows │ EReflectedAndRefractedRayTracedShadows │ Rendering Features|Translucency │
  └───────────────────────────────┴────────────────────────────────────────┴─────────────────────────────────┘
  MegaLights
  ┌─────────────┬──────────────┬────────────────────┐
  │   Setting   │     Type     │      Category      │
  ├─────────────┼──────────────┼────────────────────┤
  │ bMegaLights │ uint8 (bool) │ Rendering Features │
  └─────────────┴──────────────┴────────────────────┘
  ---
  Note: Direct shadow settings (Shadow Maps, Distance Field Shadows) are typically controlled via Light actors, not PostProcessSettings. The settings above are what you can control through Post
   Process Volumes for shadows/GI/Lumen effects.


*/