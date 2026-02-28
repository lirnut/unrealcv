#include "MovieQualityRenderComponent.h"
#include "LitCamSensor.h"
#include "ImageWriteQueue.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/PostProcessVolume.h"
#include "EngineUtils.h"
#include "Engine/Canvas.h"
#include "SceneView.h"
#include "SceneViewExtension.h"
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
#include "LandscapeRender.h"
#include "Materials/MaterialRenderProxy.h"

FMQRCSettings UMovieQualityRenderComponent::GlobalSettings;


void FMovieQualityViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	if (!Component.IsValid()) { 
		UE_LOG(LogTemp, Error, TEXT("FMovieQualityViewExtension::BeginRenderViewFamily: !Component.IsValid()"));
		return; 
	}
	if (InViewFamily.bIsMainViewFamily)
	{
		if (InViewFamily.Views.Num() > 0 && InViewFamily.Views[0] != nullptr)
		{
			Component->CachedMainViewPostProcessSettings = InViewFamily.Views[0]->FinalPostProcessSettings;
			Component->CachedMainViewPostProcessSettings.BlendableManager = FBlendableManager();

			Component->bHasCachedMainViewPostProcessSettings = true;
			Component->LastMainViewportFrameNumber = InViewFamily.FrameNumber;
			UE_LOG(LogTemp, Log, TEXT("FMovieQualityViewExtension::BeginRenderViewFamily: sucessfully set CachedMainViewPostProcessSettings"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("FMovieQualityViewExtension::BeginRenderViewFamily: %d"), InViewFamily.Views.Num());
			UE_LOG(LogTemp, Warning, TEXT("FMovieQualityViewExtension::BeginRenderViewFamily: %p"), InViewFamily.Views[0]);
		}
		Component->ProcessDeferredCaptures();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("FMovieQualityViewExtension::BeginRenderViewFamily: !InViewFamily.bIsMainViewFamily"));
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
	ShowFlags.SetContactShadows(false); 
  	ShowFlags.SetCapsuleShadows(true); 

	ShowFlags.SetPreviewShadowsIndicator(false);

	ShowFlags.SetTonemapper(true);
    ShowFlags.SetEyeAdaptation(true);
    ShowFlags.SetPostProcessing(true);  // Also ensure PostProcessing is on

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
	ShowFlags = GetWorld()->GetGameViewport()->EngineShowFlags;


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
		// MQRC Fix: Wait for rendering commands before destroying ViewState
		// This prevents "Material about to be deleted" crashes during re-initialization
		FlushRenderingCommands();
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

	// MQRC Fix: Wait for all pending rendering commands to complete before destroying resources
	// This prevents "Material about to be deleted" crashes when switching maps or destroying component
	UE_LOG(LogTemp, Warning, TEXT("[CHECKPOINT] MovieQualityRenderComponent::Shutdown() - Flushing rendering commands"));
	FlushRenderingCommands();
	UE_LOG(LogTemp, Warning, TEXT("[CHECKPOINT] MovieQualityRenderComponent::Shutdown() - Rendering commands flushed"));

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

	// MQRC Fix: Let ViewState.Destroy() handle MID pool cleanup automatically
	// Explicit ClearMIDPool() causes race condition with async material caching
	// ViewState.Destroy() enqueues proper cleanup on render thread via ReleaseRHI()
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
	// ProcessDeferredCaptures();
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

	// MQRC Fix: Gather ViewExtensions for Landscape LOD system
	// This mimics SceneCaptureRendering.cpp line 885-886
	FSceneViewExtensionContext ViewExtensionContext(World->Scene);
	ViewFamily->ViewExtensions = GEngine->ViewExtensions->GatherActiveExtensions(ViewExtensionContext);

	return ViewFamily;
}

FSceneView* UMovieQualityRenderComponent::CreateSceneView(FSceneViewFamily* VF)
{
	FVector Location = GetComponentLocation();
	FRotator Rotation = GetComponentRotation();

	FSceneViewInitOptions ViewInitOptions;
	ViewInitOptions.ViewFamily = VF;
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
	View->bSceneCaptureUsesRayTracing = true; 
	// View->bIsReflectionCapture = true;
	View->bIsSceneCapture = true;
	// View->bIsSceneCaptureCube = false;
	View->bIsGameView = true;

	View->OverrideFrameIndexValue = FrameCounter++;


	if (bHasCachedMainViewPostProcessSettings)
	{
		UE_LOG(LogTemp, Log, TEXT("MQRC: bHasCachedMainViewPostProcessSettings = true"));
		DebugPrintPostProcessSettingsAll(CachedMainViewPostProcessSettings, TEXT("MQRC: CachedMainViewPostProcessSettings"));
		View->FinalPostProcessSettings = CachedMainViewPostProcessSettings;
		// SetAllOverridePostProcessSettings(View->FinalPostProcessSettings);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("MQRC: bHasCachedMainViewPostProcessSettings = false,  use SetBaseValues() instead"));
		View->FinalPostProcessSettings.SetBaseValues();
	}
	
	DebugPrintPostProcessSettings(View->FinalPostProcessSettings, TEXT("MQRC: Init"));

	UWorld* World = GetWorld();
	if (World)
	{
		UE_LOG(LogTemp, Log, TEXT("MQRC: World found, gathering PostProcessVolumes"));

		TArray<APostProcessVolume*> Volumes;
		for (TActorIterator<APostProcessVolume> It(World); It; ++It)
		{
			if (It->bEnabled && It->bUnbound)
			{
				Volumes.Add(*It);
				UE_LOG(LogTemp, Log, TEXT("MQRC: Found PPV: %s, Priority=%.2f, BlendWeight=%.2f, Blendables=%d"),
					*It->GetName(), It->Priority, It->BlendWeight, It->Settings.WeightedBlendables.Array.Num());
			}
		}

		UE_LOG(LogTemp, Log, TEXT("MQRC: Total enabled unbound PPVs: %d"), Volumes.Num());

		Volumes.Sort([](const APostProcessVolume& A, const APostProcessVolume& B) {
			return A.Priority > B.Priority;
		});


		for (APostProcessVolume* Vol : Volumes)
		{
			DebugPrintPostProcessSettings(Vol->Settings, FString::Printf(TEXT("MQRC: PPV[%s]"), *Vol->GetName()));
			FPostProcessSettings TempSettings = Vol->Settings;

			int32 BlendableCount = TempSettings.WeightedBlendables.Array.Num();
			TempSettings.WeightedBlendables.Array.Empty();
			UE_LOG(LogTemp, Log, TEXT("MQRC: Applied PPV: %s (cleared %d blendables)"), *Vol->GetName(), BlendableCount);

			View->OverridePostProcessSettings(TempSettings, Vol->BlendWeight);
			CopyOverrideFlags(TempSettings, View->FinalPostProcessSettings);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("MQRC: World not found"));
	}

	SetPostProcessSettings(PostProcessSettings);
	DebugPrintPostProcessSettings(PostProcessSettings, TEXT("MQRC: PostProcessSettings Overrides"));
	View->OverridePostProcessSettings(PostProcessSettings, PostProcessBlendWeight);
	// CopyOverrideFlags(PostProcessSettings, View->FinalPostProcessSettings);
	UE_LOG(LogTemp, Log, TEXT("MQRC: Applied PPS overrides"));

	DebugPrintPostProcessSettings(View->FinalPostProcessSettings, TEXT("MQRC: After PPS overrides"));

	// const float MaxLuminance = 1.2f; // Should we use const LuminanceMaxFromLensAttenuation() instead?
	// View->FinalPostProcessSettings.AutoExposureMinBrightness = LuminanceToEV100(MaxLuminance, View->FinalPostProcessSettings.AutoExposureMinBrightness);
	// View->FinalPostProcessSettings.AutoExposureMaxBrightness = LuminanceToEV100(MaxLuminance, View->FinalPostProcessSettings.AutoExposureMaxBrightness);

	// SetAllOverridePostProcessSettings(View->FinalPostProcessSettings);
  	View->EndFinalPostprocessSettings(ViewInitOptions);

	DebugPrintPostProcessSettings(View->FinalPostProcessSettings, TEXT("MQRC: Final"));

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
	VF->Views.Add(View);
	// VF->AllViews.Add(View);

	return View;
}

void UMovieQualityRenderComponent::SubmitToRendererWithCallback(
	FSceneViewFamily* VF,
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

	// MQRC Fix: Setup ViewExtensions for scene capture (required for Landscape LOD system)
	// This mimics SceneCaptureRendering.cpp's SetupSceneViewExtensionsForSceneCapture (lines 806-822)
	for (const FSceneViewExtensionRef& Extension : VF->ViewExtensions)
	{
		Extension->SetupViewFamily(*VF);
	}
	for (const FSceneView* View : VF->Views)
	{
		for (const FSceneViewExtensionRef& Extension : VF->ViewExtensions)
		{
			Extension->SetupView(*VF, *const_cast<FSceneView*>(View));
		}
	}

	FRenderTarget* RenderTargetResource = RenderTarget->GameThread_GetRenderTargetResource();

	FCanvas Canvas(RenderTargetResource, nullptr, World, VF->GetFeatureLevel(), FCanvas::CDM_DeferDrawing, 1.0f);
	GetRendererModule().BeginRenderingViewFamily(&Canvas, VF);

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

void UMovieQualityRenderComponent::CopyOverrideFlags(const FPostProcessSettings& Src, FPostProcessSettings& Dest)
{
#define COPY_OVERRIDE(NAME) if(Src.bOverride_##NAME) Dest.bOverride_##NAME = true;

	COPY_OVERRIDE(TemperatureType);
	COPY_OVERRIDE(WhiteTemp);
	COPY_OVERRIDE(WhiteTint);
	COPY_OVERRIDE(ColorSaturation);
	COPY_OVERRIDE(ColorContrast);
	COPY_OVERRIDE(ColorGamma);
	COPY_OVERRIDE(ColorGain);
	COPY_OVERRIDE(ColorOffset);
	COPY_OVERRIDE(ColorSaturationShadows);
	COPY_OVERRIDE(ColorContrastShadows);
	COPY_OVERRIDE(ColorGammaShadows);
	COPY_OVERRIDE(ColorGainShadows);
	COPY_OVERRIDE(ColorOffsetShadows);
	COPY_OVERRIDE(ColorSaturationMidtones);
	COPY_OVERRIDE(ColorContrastMidtones);
	COPY_OVERRIDE(ColorGammaMidtones);
	COPY_OVERRIDE(ColorGainMidtones);
	COPY_OVERRIDE(ColorOffsetMidtones);
	COPY_OVERRIDE(ColorSaturationHighlights);
	COPY_OVERRIDE(ColorContrastHighlights);
	COPY_OVERRIDE(ColorGammaHighlights);
	COPY_OVERRIDE(ColorGainHighlights);
	COPY_OVERRIDE(ColorOffsetHighlights);
	COPY_OVERRIDE(FilmSlope);
	COPY_OVERRIDE(FilmToe);
	COPY_OVERRIDE(FilmShoulder);
	COPY_OVERRIDE(FilmBlackClip);
	COPY_OVERRIDE(FilmWhiteClip);
	COPY_OVERRIDE(SceneColorTint);
	COPY_OVERRIDE(BloomIntensity);
	COPY_OVERRIDE(BloomThreshold);
	COPY_OVERRIDE(AutoExposureMethod);
	COPY_OVERRIDE(AutoExposureSpeedUp);
	COPY_OVERRIDE(AutoExposureSpeedDown);
	COPY_OVERRIDE(AutoExposureBias);
	COPY_OVERRIDE(AutoExposureMinBrightness);
	COPY_OVERRIDE(AutoExposureMaxBrightness);
	COPY_OVERRIDE(DepthOfFieldScale);
	COPY_OVERRIDE(DepthOfFieldFocalDistance);
	COPY_OVERRIDE(DepthOfFieldFstop);
	COPY_OVERRIDE(MotionBlurAmount);
	COPY_OVERRIDE(MotionBlurMax);
	COPY_OVERRIDE(MotionBlurTargetFPS);
	COPY_OVERRIDE(MotionBlurPerObjectSize);
	COPY_OVERRIDE(ReflectionMethod);
	COPY_OVERRIDE(LumenReflectionQuality);
	COPY_OVERRIDE(DynamicGlobalIlluminationMethod);
	COPY_OVERRIDE(LumenSceneLightingQuality);
	COPY_OVERRIDE(LumenSceneDetail);
	COPY_OVERRIDE(LumenSceneViewDistance);
	COPY_OVERRIDE(LumenSceneLightingUpdateSpeed);
	COPY_OVERRIDE(LumenFinalGatherQuality);
	COPY_OVERRIDE(LumenFinalGatherLightingUpdateSpeed);
	COPY_OVERRIDE(LumenFinalGatherScreenTraces);
	COPY_OVERRIDE(LumenMaxTraceDistance);
	COPY_OVERRIDE(LumenDiffuseColorBoost);
	COPY_OVERRIDE(LumenSkylightLeaking);
	COPY_OVERRIDE(LumenFullSkylightLeakingDistance);
	COPY_OVERRIDE(LumenRayLightingMode);
	COPY_OVERRIDE(LumenReflectionsScreenTraces);
	COPY_OVERRIDE(LumenFrontLayerTranslucencyReflections);
	COPY_OVERRIDE(LumenMaxRoughnessToTraceReflections);
	COPY_OVERRIDE(LumenMaxReflectionBounces);
	COPY_OVERRIDE(LumenMaxRefractionBounces);
	COPY_OVERRIDE(LumenSurfaceCacheResolution);

#undef COPY_OVERRIDE
}

void UMovieQualityRenderComponent::DebugPrintPostProcessSettings(const FPostProcessSettings& Settings, const FString& Prefix)
{
	UE_LOG(LogTemp, Log, TEXT("%s === PostProcessSettings Debug ==="), *Prefix);

#define CHECK_OVERRIDE_FLOAT(Name) \
	if (Settings.bOverride_##Name) { \
		UE_LOG(LogTemp, Log, TEXT("%s   %s = %.4f"), *Prefix, TEXT(#Name), Settings.Name); \
	}

#define CHECK_OVERRIDE_INT(Name) \
	if (Settings.bOverride_##Name) { \
		UE_LOG(LogTemp, Log, TEXT("%s   %s = %d"), *Prefix, TEXT(#Name), (int32)Settings.Name); \
	}

#define CHECK_OVERRIDE_BOOL(Name) \
	if (Settings.bOverride_##Name) { \
		UE_LOG(LogTemp, Log, TEXT("%s   %s = %s"), *Prefix, TEXT(#Name), Settings.Name ? TEXT("true") : TEXT("false")); \
	}

#define CHECK_OVERRIDE_VEC4(Name) \
	if (Settings.bOverride_##Name) { \
		UE_LOG(LogTemp, Log, TEXT("%s   %s = (%.3f, %.3f, %.3f, %.3f)"), *Prefix, TEXT(#Name), \
			Settings.Name.X, Settings.Name.Y, Settings.Name.Z, Settings.Name.W); \
	}

#define CHECK_OVERRIDE_COLOR(Name) \
	if (Settings.bOverride_##Name) { \
		UE_LOG(LogTemp, Log, TEXT("%s   %s = (R:%.3f, G:%.3f, B:%.3f)"), *Prefix, TEXT(#Name), \
			Settings.Name.R, Settings.Name.G, Settings.Name.B); \
	}

#define CHECK_OVERRIDE_VEC2(Name) \
	if (Settings.bOverride_##Name) { \
		UE_LOG(LogTemp, Log, TEXT("%s   %s = (%.3f, %.3f)"), *Prefix, TEXT(#Name), \
			Settings.Name.X, Settings.Name.Y); \
	}

#define CHECK_OVERRIDE_TEXTURE(Name) \
	if (Settings.bOverride_##Name) { \
		UE_LOG(LogTemp, Log, TEXT("%s   %s = %s"), *Prefix, TEXT(#Name), \
			Settings.Name ? *Settings.Name->GetName() : TEXT("nullptr")); \
	}

#define CHECK_OVERRIDE_CURVE(Name) \
	if (Settings.bOverride_##Name) { \
		UE_LOG(LogTemp, Log, TEXT("%s   %s = %s"), *Prefix, TEXT(#Name), \
			Settings.Name ? *Settings.Name->GetName() : TEXT("nullptr")); \
	}

	CHECK_OVERRIDE_INT(TemperatureType);
	CHECK_OVERRIDE_FLOAT(WhiteTemp);
	CHECK_OVERRIDE_FLOAT(WhiteTint);

	CHECK_OVERRIDE_VEC4(ColorSaturation);
	CHECK_OVERRIDE_VEC4(ColorContrast);
	CHECK_OVERRIDE_VEC4(ColorGamma);
	CHECK_OVERRIDE_VEC4(ColorGain);
	CHECK_OVERRIDE_VEC4(ColorOffset);

	CHECK_OVERRIDE_VEC4(ColorSaturationShadows);
	CHECK_OVERRIDE_VEC4(ColorContrastShadows);
	CHECK_OVERRIDE_VEC4(ColorGammaShadows);
	CHECK_OVERRIDE_VEC4(ColorGainShadows);
	CHECK_OVERRIDE_VEC4(ColorOffsetShadows);

	CHECK_OVERRIDE_VEC4(ColorSaturationMidtones);
	CHECK_OVERRIDE_VEC4(ColorContrastMidtones);
	CHECK_OVERRIDE_VEC4(ColorGammaMidtones);
	CHECK_OVERRIDE_VEC4(ColorGainMidtones);
	CHECK_OVERRIDE_VEC4(ColorOffsetMidtones);

	CHECK_OVERRIDE_VEC4(ColorSaturationHighlights);
	CHECK_OVERRIDE_VEC4(ColorContrastHighlights);
	CHECK_OVERRIDE_VEC4(ColorGammaHighlights);
	CHECK_OVERRIDE_VEC4(ColorGainHighlights);
	CHECK_OVERRIDE_VEC4(ColorOffsetHighlights);

	CHECK_OVERRIDE_FLOAT(ColorCorrectionShadowsMax);
	CHECK_OVERRIDE_FLOAT(ColorCorrectionHighlightsMin);
	CHECK_OVERRIDE_FLOAT(ColorCorrectionHighlightsMax);

	CHECK_OVERRIDE_FLOAT(BlueCorrection);
	CHECK_OVERRIDE_FLOAT(ExpandGamut);
	CHECK_OVERRIDE_FLOAT(ToneCurveAmount);

	CHECK_OVERRIDE_FLOAT(FilmSlope);
	CHECK_OVERRIDE_FLOAT(FilmToe);
	CHECK_OVERRIDE_FLOAT(FilmShoulder);
	CHECK_OVERRIDE_FLOAT(FilmBlackClip);
	CHECK_OVERRIDE_FLOAT(FilmWhiteClip);

	CHECK_OVERRIDE_COLOR(SceneColorTint);
	CHECK_OVERRIDE_FLOAT(SceneFringeIntensity);
	CHECK_OVERRIDE_FLOAT(ChromaticAberrationStartOffset);

	CHECK_OVERRIDE_INT(BloomMethod);
	CHECK_OVERRIDE_FLOAT(BloomIntensity);
	CHECK_OVERRIDE_FLOAT(BloomThreshold);
	CHECK_OVERRIDE_FLOAT(BloomSizeScale);
	CHECK_OVERRIDE_FLOAT(Bloom1Size);
	CHECK_OVERRIDE_FLOAT(Bloom2Size);
	CHECK_OVERRIDE_FLOAT(Bloom3Size);
	CHECK_OVERRIDE_FLOAT(Bloom4Size);
	CHECK_OVERRIDE_FLOAT(Bloom5Size);
	CHECK_OVERRIDE_FLOAT(Bloom6Size);
	CHECK_OVERRIDE_COLOR(Bloom1Tint);
	CHECK_OVERRIDE_COLOR(Bloom2Tint);
	CHECK_OVERRIDE_COLOR(Bloom3Tint);
	CHECK_OVERRIDE_COLOR(Bloom4Tint);
	CHECK_OVERRIDE_COLOR(Bloom5Tint);
	CHECK_OVERRIDE_COLOR(Bloom6Tint);

	CHECK_OVERRIDE_FLOAT(BloomDirtMaskIntensity);
	CHECK_OVERRIDE_COLOR(BloomDirtMaskTint);

	CHECK_OVERRIDE_FLOAT(CameraShutterSpeed);
	CHECK_OVERRIDE_FLOAT(CameraISO);
	CHECK_OVERRIDE_INT(AutoExposureMethod);
	CHECK_OVERRIDE_FLOAT(AutoExposureLowPercent);
	CHECK_OVERRIDE_FLOAT(AutoExposureHighPercent);
	CHECK_OVERRIDE_FLOAT(AutoExposureMinBrightness);
	CHECK_OVERRIDE_FLOAT(AutoExposureMaxBrightness);
	CHECK_OVERRIDE_FLOAT(AutoExposureSpeedUp);
	CHECK_OVERRIDE_FLOAT(AutoExposureSpeedDown);
	CHECK_OVERRIDE_FLOAT(AutoExposureBias);
	CHECK_OVERRIDE_FLOAT(AutoExposureBiasBackup);
	CHECK_OVERRIDE_CURVE(AutoExposureBiasCurve);
	CHECK_OVERRIDE_TEXTURE(AutoExposureMeterMask);
	CHECK_OVERRIDE_BOOL(AutoExposureApplyPhysicalCameraExposure);
	CHECK_OVERRIDE_FLOAT(HistogramLogMin);
	CHECK_OVERRIDE_FLOAT(HistogramLogMax);

	CHECK_OVERRIDE_INT(LocalExposureMethod);
	CHECK_OVERRIDE_FLOAT(LocalExposureHighlightContrastScale);
	CHECK_OVERRIDE_FLOAT(LocalExposureShadowContrastScale);
	CHECK_OVERRIDE_FLOAT(LocalExposureDetailStrength);
	CHECK_OVERRIDE_FLOAT(LocalExposureBlurredLuminanceBlend);
	CHECK_OVERRIDE_FLOAT(LocalExposureBlurredLuminanceKernelSizePercent);
	CHECK_OVERRIDE_FLOAT(LocalExposureMiddleGreyBias);

	CHECK_OVERRIDE_FLOAT(LensFlareIntensity);
	CHECK_OVERRIDE_COLOR(LensFlareTint);
	CHECK_OVERRIDE_FLOAT(LensFlareBokehSize);
	CHECK_OVERRIDE_FLOAT(LensFlareThreshold);
	CHECK_OVERRIDE_FLOAT(VignetteIntensity);
	CHECK_OVERRIDE_FLOAT(Sharpen);

	CHECK_OVERRIDE_FLOAT(FilmGrainIntensity);
	CHECK_OVERRIDE_FLOAT(FilmGrainIntensityShadows);
	CHECK_OVERRIDE_FLOAT(FilmGrainIntensityMidtones);
	CHECK_OVERRIDE_FLOAT(FilmGrainIntensityHighlights);

	CHECK_OVERRIDE_FLOAT(AmbientOcclusionIntensity);
	CHECK_OVERRIDE_FLOAT(AmbientOcclusionStaticFraction);
	CHECK_OVERRIDE_FLOAT(AmbientOcclusionRadius);
	CHECK_OVERRIDE_FLOAT(AmbientOcclusionFadeDistance);
	CHECK_OVERRIDE_FLOAT(AmbientOcclusionFadeRadius);
	CHECK_OVERRIDE_BOOL(AmbientOcclusionRadiusInWS);
	CHECK_OVERRIDE_FLOAT(AmbientOcclusionPower);
	CHECK_OVERRIDE_FLOAT(AmbientOcclusionBias);
	CHECK_OVERRIDE_FLOAT(AmbientOcclusionQuality);

	CHECK_OVERRIDE_FLOAT(IndirectLightingIntensity);
	CHECK_OVERRIDE_COLOR(IndirectLightingColor);

	CHECK_OVERRIDE_FLOAT(DepthOfFieldFocalDistance);
	CHECK_OVERRIDE_FLOAT(DepthOfFieldFstop);
	CHECK_OVERRIDE_FLOAT(DepthOfFieldMinFstop);
	CHECK_OVERRIDE_INT(DepthOfFieldBladeCount);
	CHECK_OVERRIDE_FLOAT(DepthOfFieldSensorWidth);
	CHECK_OVERRIDE_FLOAT(DepthOfFieldSqueezeFactor);
	CHECK_OVERRIDE_FLOAT(DepthOfFieldDepthBlurRadius);
	CHECK_OVERRIDE_FLOAT(DepthOfFieldDepthBlurAmount);
	CHECK_OVERRIDE_FLOAT(DepthOfFieldFocalRegion);
	CHECK_OVERRIDE_FLOAT(DepthOfFieldNearTransitionRegion);
	CHECK_OVERRIDE_FLOAT(DepthOfFieldFarTransitionRegion);
	CHECK_OVERRIDE_FLOAT(DepthOfFieldScale);
	CHECK_OVERRIDE_FLOAT(DepthOfFieldNearBlurSize);
	CHECK_OVERRIDE_FLOAT(DepthOfFieldFarBlurSize);
	CHECK_OVERRIDE_FLOAT(DepthOfFieldOcclusion);
	CHECK_OVERRIDE_FLOAT(DepthOfFieldSkyFocusDistance);
	CHECK_OVERRIDE_FLOAT(DepthOfFieldVignetteSize);

	CHECK_OVERRIDE_FLOAT(MotionBlurAmount);
	CHECK_OVERRIDE_FLOAT(MotionBlurMax);
	CHECK_OVERRIDE_INT(MotionBlurTargetFPS);
	CHECK_OVERRIDE_FLOAT(MotionBlurPerObjectSize);

	CHECK_OVERRIDE_INT(ReflectionMethod);
	CHECK_OVERRIDE_FLOAT(LumenReflectionQuality);
	CHECK_OVERRIDE_FLOAT(ScreenSpaceReflectionIntensity);
	CHECK_OVERRIDE_FLOAT(ScreenSpaceReflectionQuality);
	CHECK_OVERRIDE_FLOAT(ScreenSpaceReflectionMaxRoughness);

	CHECK_OVERRIDE_INT(DynamicGlobalIlluminationMethod);
	CHECK_OVERRIDE_FLOAT(LumenSceneLightingQuality);
	CHECK_OVERRIDE_FLOAT(LumenSceneDetail);
	CHECK_OVERRIDE_FLOAT(LumenSceneViewDistance);
	CHECK_OVERRIDE_FLOAT(LumenSceneLightingUpdateSpeed);
	CHECK_OVERRIDE_FLOAT(LumenFinalGatherQuality);
	CHECK_OVERRIDE_FLOAT(LumenFinalGatherLightingUpdateSpeed);
	CHECK_OVERRIDE_BOOL(LumenFinalGatherScreenTraces);
	CHECK_OVERRIDE_FLOAT(LumenMaxTraceDistance);
	CHECK_OVERRIDE_FLOAT(LumenDiffuseColorBoost);
	CHECK_OVERRIDE_FLOAT(LumenSkylightLeaking);
	CHECK_OVERRIDE_COLOR(LumenSkylightLeakingTint);
	CHECK_OVERRIDE_FLOAT(LumenFullSkylightLeakingDistance);
	CHECK_OVERRIDE_INT(LumenRayLightingMode);
	CHECK_OVERRIDE_BOOL(LumenReflectionsScreenTraces);
	CHECK_OVERRIDE_BOOL(LumenFrontLayerTranslucencyReflections);
	CHECK_OVERRIDE_FLOAT(LumenMaxRoughnessToTraceReflections);
	CHECK_OVERRIDE_INT(LumenMaxReflectionBounces);
	CHECK_OVERRIDE_INT(LumenMaxRefractionBounces);

	CHECK_OVERRIDE_BOOL(RayTracingAO);
	CHECK_OVERRIDE_INT(RayTracingAOSamplesPerPixel);
	CHECK_OVERRIDE_FLOAT(RayTracingAOIntensity);
	CHECK_OVERRIDE_FLOAT(RayTracingAORadius);

	// CHECK_OVERRIDE_FLOAT(RayTracingReflectionsMaxRoughness);
	// CHECK_OVERRIDE_INT(RayTracingReflectionsMaxBounces);
	// CHECK_OVERRIDE_INT(RayTracingReflectionsSamplesPerPixel);
	// CHECK_OVERRIDE_INT(RayTracingReflectionsShadows);
	// CHECK_OVERRIDE_INT(RayTracingReflectionsTranslucency);

	CHECK_OVERRIDE_INT(TranslucencyType);
	CHECK_OVERRIDE_FLOAT(RayTracingTranslucencyMaxRoughness);
	CHECK_OVERRIDE_INT(RayTracingTranslucencyRefractionRays);
	CHECK_OVERRIDE_INT(RayTracingTranslucencySamplesPerPixel);
	CHECK_OVERRIDE_INT(RayTracingTranslucencyShadows);
	CHECK_OVERRIDE_INT(RayTracingTranslucencyRefraction);

	CHECK_OVERRIDE_INT(RayTracingTranslucencyMaxPrimaryHitEvents);
	CHECK_OVERRIDE_INT(RayTracingTranslucencyMaxSecondaryHitEvents);
	CHECK_OVERRIDE_BOOL(RayTracingTranslucencyUseRayTracedRefraction);

	// CHECK_OVERRIDE_BOOL(RayTracingGI);
	// CHECK_OVERRIDE_INT(RayTracingGIMaxBounces);
	// CHECK_OVERRIDE_INT(RayTracingGISamplesPerPixel);

	CHECK_OVERRIDE_INT(PathTracingMaxBounces);
	CHECK_OVERRIDE_INT(PathTracingSamplesPerPixel);
	CHECK_OVERRIDE_FLOAT(PathTracingMaxPathIntensity);
	CHECK_OVERRIDE_BOOL(PathTracingEnableEmissiveMaterials);
	CHECK_OVERRIDE_BOOL(PathTracingEnableReferenceDOF);
	CHECK_OVERRIDE_BOOL(PathTracingEnableReferenceAtmosphere);
	CHECK_OVERRIDE_BOOL(PathTracingEnableDenoiser);
	CHECK_OVERRIDE_BOOL(PathTracingIncludeEmissive);
	CHECK_OVERRIDE_BOOL(PathTracingIncludeDiffuse);
	CHECK_OVERRIDE_BOOL(PathTracingIncludeIndirectDiffuse);
	CHECK_OVERRIDE_BOOL(PathTracingIncludeSpecular);
	CHECK_OVERRIDE_BOOL(PathTracingIncludeIndirectSpecular);
	CHECK_OVERRIDE_BOOL(PathTracingIncludeVolume);
	CHECK_OVERRIDE_BOOL(PathTracingIncludeIndirectVolume);

	CHECK_OVERRIDE_COLOR(AmbientCubemapTint);
	CHECK_OVERRIDE_FLOAT(AmbientCubemapIntensity);

	CHECK_OVERRIDE_FLOAT(ColorGradingIntensity);
	CHECK_OVERRIDE_TEXTURE(ColorGradingLUT);

	CHECK_OVERRIDE_FLOAT(BloomConvolutionScatterDispersion);
	CHECK_OVERRIDE_FLOAT(BloomConvolutionSize);
	CHECK_OVERRIDE_FLOAT(BloomConvolutionPreFilterMin);
	CHECK_OVERRIDE_FLOAT(BloomConvolutionPreFilterMax);
	CHECK_OVERRIDE_FLOAT(BloomConvolutionPreFilterMult);
	CHECK_OVERRIDE_FLOAT(BloomConvolutionBufferScale);
	CHECK_OVERRIDE_VEC2(BloomConvolutionCenterUV);
	CHECK_OVERRIDE_TEXTURE(BloomConvolutionTexture);
	CHECK_OVERRIDE_TEXTURE(BloomDirtMask);

	CHECK_OVERRIDE_BOOL(DepthOfFieldUseHairDepth);
	CHECK_OVERRIDE_BOOL(DepthOfFieldPetzvalBokeh);
	CHECK_OVERRIDE_FLOAT(DepthOfFieldPetzvalBokehFalloff);
	CHECK_OVERRIDE_FLOAT(DepthOfFieldPetzvalExclusionBoxRadius);
	CHECK_OVERRIDE_FLOAT(DepthOfFieldAspectRatioScalar);
	// CHECK_OVERRIDE_INT(DepthOfFieldMatteBoxFlags);
	CHECK_OVERRIDE_FLOAT(DepthOfFieldBarrelRadius);
	CHECK_OVERRIDE_FLOAT(DepthOfFieldBarrelLength);

	CHECK_OVERRIDE_FLOAT(AmbientOcclusionMipBlend);
	CHECK_OVERRIDE_FLOAT(AmbientOcclusionMipScale);
	CHECK_OVERRIDE_FLOAT(AmbientOcclusionMipThreshold);
	CHECK_OVERRIDE_FLOAT(AmbientOcclusionTemporalBlendWeight);

	CHECK_OVERRIDE_FLOAT(FilmGrainShadowsMax);
	CHECK_OVERRIDE_FLOAT(FilmGrainHighlightsMin);
	CHECK_OVERRIDE_FLOAT(FilmGrainHighlightsMax);
	CHECK_OVERRIDE_FLOAT(FilmGrainTexelSize);
	CHECK_OVERRIDE_TEXTURE(FilmGrainTexture);

	// CHECK_OVERRIDE_FLOAT(LensFlareBokehShape);
	// CHECK_OVERRIDE_COLOR(LensFlareTints);

	CHECK_OVERRIDE_FLOAT(LocalExposureHighlightThreshold);
	CHECK_OVERRIDE_FLOAT(LocalExposureShadowThreshold);
	CHECK_OVERRIDE_FLOAT(LocalExposureHighlightThresholdStrength);
	CHECK_OVERRIDE_FLOAT(LocalExposureShadowThresholdStrength);
	CHECK_OVERRIDE_CURVE(LocalExposureHighlightContrastCurve);
	CHECK_OVERRIDE_CURVE(LocalExposureShadowContrastCurve);

	// CHECK_OVERRIDE_BOOL(MobileHQGaussian);
	// CHECK_OVERRIDE_FLOAT(ScreenSpaceReflectionRoughnessScale);
	CHECK_OVERRIDE_FLOAT(LumenSurfaceCacheResolution);
	CHECK_OVERRIDE_BOOL(bMegaLights);
	CHECK_OVERRIDE_INT(UserFlags);

	if (Settings.WeightedBlendables.Array.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("%s   WeightedBlendables.Num = %d"), *Prefix, Settings.WeightedBlendables.Array.Num());
	}

#undef CHECK_OVERRIDE_FLOAT
#undef CHECK_OVERRIDE_INT
#undef CHECK_OVERRIDE_BOOL
#undef CHECK_OVERRIDE_VEC4
#undef CHECK_OVERRIDE_COLOR
#undef CHECK_OVERRIDE_VEC2
#undef CHECK_OVERRIDE_TEXTURE
#undef CHECK_OVERRIDE_CURVE

	UE_LOG(LogTemp, Log, TEXT("%s ================================"), *Prefix);
}

void UMovieQualityRenderComponent::DebugPrintPostProcessSettingsAll(const FPostProcessSettings& Settings, const FString& Prefix)
{
	UE_LOG(LogTemp, Log, TEXT("%s === PostProcessSettings Debug ==="), *Prefix);

#define CHECK_OVERRIDE_FLOAT(Name) \
		UE_LOG(LogTemp, Log, TEXT("%s   %s = %.4f"), *Prefix, TEXT(#Name), Settings.Name); 

#define CHECK_OVERRIDE_INT(Name) \
		UE_LOG(LogTemp, Log, TEXT("%s   %s = %d"), *Prefix, TEXT(#Name), (int32)Settings.Name); 

#define CHECK_OVERRIDE_BOOL(Name) \
	UE_LOG(LogTemp, Log, TEXT("%s   %s = %s"), *Prefix, TEXT(#Name), Settings.Name ? TEXT("true") : TEXT("false"));

#define CHECK_OVERRIDE_VEC4(Name) \
	UE_LOG(LogTemp, Log, TEXT("%s   %s = (%.3f, %.3f, %.3f, %.3f)"), *Prefix, TEXT(#Name), \
		Settings.Name.X, Settings.Name.Y, Settings.Name.Z, Settings.Name.W);

#define CHECK_OVERRIDE_COLOR(Name) \
	UE_LOG(LogTemp, Log, TEXT("%s   %s = (R:%.3f, G:%.3f, B:%.3f)"), *Prefix, TEXT(#Name), \
		Settings.Name.R, Settings.Name.G, Settings.Name.B);

#define CHECK_OVERRIDE_VEC2(Name) \
	UE_LOG(LogTemp, Log, TEXT("%s   %s = (%.3f, %.3f)"), *Prefix, TEXT(#Name), \
		Settings.Name.X, Settings.Name.Y);

#define CHECK_OVERRIDE_TEXTURE(Name) \
	UE_LOG(LogTemp, Log, TEXT("%s   %s = %s"), *Prefix, TEXT(#Name), \
		Settings.Name ? *Settings.Name->GetName() : TEXT("nullptr"));

#define CHECK_OVERRIDE_CURVE(Name) \
	UE_LOG(LogTemp, Log, TEXT("%s   %s = %s"), *Prefix, TEXT(#Name), \
		Settings.Name ? *Settings.Name->GetName() : TEXT("nullptr"));

	CHECK_OVERRIDE_INT(TemperatureType);
	CHECK_OVERRIDE_FLOAT(WhiteTemp);
	CHECK_OVERRIDE_FLOAT(WhiteTint);

	CHECK_OVERRIDE_VEC4(ColorSaturation);
	CHECK_OVERRIDE_VEC4(ColorContrast);
	CHECK_OVERRIDE_VEC4(ColorGamma);
	CHECK_OVERRIDE_VEC4(ColorGain);
	CHECK_OVERRIDE_VEC4(ColorOffset);

	CHECK_OVERRIDE_VEC4(ColorSaturationShadows);
	CHECK_OVERRIDE_VEC4(ColorContrastShadows);
	CHECK_OVERRIDE_VEC4(ColorGammaShadows);
	CHECK_OVERRIDE_VEC4(ColorGainShadows);
	CHECK_OVERRIDE_VEC4(ColorOffsetShadows);

	CHECK_OVERRIDE_VEC4(ColorSaturationMidtones);
	CHECK_OVERRIDE_VEC4(ColorContrastMidtones);
	CHECK_OVERRIDE_VEC4(ColorGammaMidtones);
	CHECK_OVERRIDE_VEC4(ColorGainMidtones);
	CHECK_OVERRIDE_VEC4(ColorOffsetMidtones);

	CHECK_OVERRIDE_VEC4(ColorSaturationHighlights);
	CHECK_OVERRIDE_VEC4(ColorContrastHighlights);
	CHECK_OVERRIDE_VEC4(ColorGammaHighlights);
	CHECK_OVERRIDE_VEC4(ColorGainHighlights);
	CHECK_OVERRIDE_VEC4(ColorOffsetHighlights);

	CHECK_OVERRIDE_FLOAT(ColorCorrectionShadowsMax);
	CHECK_OVERRIDE_FLOAT(ColorCorrectionHighlightsMin);
	CHECK_OVERRIDE_FLOAT(ColorCorrectionHighlightsMax);

	CHECK_OVERRIDE_FLOAT(BlueCorrection);
	CHECK_OVERRIDE_FLOAT(ExpandGamut);
	CHECK_OVERRIDE_FLOAT(ToneCurveAmount);

	CHECK_OVERRIDE_FLOAT(FilmSlope);
	CHECK_OVERRIDE_FLOAT(FilmToe);
	CHECK_OVERRIDE_FLOAT(FilmShoulder);
	CHECK_OVERRIDE_FLOAT(FilmBlackClip);
	CHECK_OVERRIDE_FLOAT(FilmWhiteClip);

	CHECK_OVERRIDE_COLOR(SceneColorTint);
	CHECK_OVERRIDE_FLOAT(SceneFringeIntensity);
	CHECK_OVERRIDE_FLOAT(ChromaticAberrationStartOffset);

	CHECK_OVERRIDE_INT(BloomMethod);
	CHECK_OVERRIDE_FLOAT(BloomIntensity);
	CHECK_OVERRIDE_FLOAT(BloomThreshold);
	CHECK_OVERRIDE_FLOAT(BloomSizeScale);
	CHECK_OVERRIDE_FLOAT(Bloom1Size);
	CHECK_OVERRIDE_FLOAT(Bloom2Size);
	CHECK_OVERRIDE_FLOAT(Bloom3Size);
	CHECK_OVERRIDE_FLOAT(Bloom4Size);
	CHECK_OVERRIDE_FLOAT(Bloom5Size);
	CHECK_OVERRIDE_FLOAT(Bloom6Size);
	CHECK_OVERRIDE_COLOR(Bloom1Tint);
	CHECK_OVERRIDE_COLOR(Bloom2Tint);
	CHECK_OVERRIDE_COLOR(Bloom3Tint);
	CHECK_OVERRIDE_COLOR(Bloom4Tint);
	CHECK_OVERRIDE_COLOR(Bloom5Tint);
	CHECK_OVERRIDE_COLOR(Bloom6Tint);

	CHECK_OVERRIDE_FLOAT(BloomDirtMaskIntensity);
	CHECK_OVERRIDE_COLOR(BloomDirtMaskTint);

	CHECK_OVERRIDE_FLOAT(CameraShutterSpeed);
	CHECK_OVERRIDE_FLOAT(CameraISO);
	CHECK_OVERRIDE_INT(AutoExposureMethod);
	CHECK_OVERRIDE_FLOAT(AutoExposureLowPercent);
	CHECK_OVERRIDE_FLOAT(AutoExposureHighPercent);
	CHECK_OVERRIDE_FLOAT(AutoExposureMinBrightness);
	CHECK_OVERRIDE_FLOAT(AutoExposureMaxBrightness);
	CHECK_OVERRIDE_FLOAT(AutoExposureSpeedUp);
	CHECK_OVERRIDE_FLOAT(AutoExposureSpeedDown);
	CHECK_OVERRIDE_FLOAT(AutoExposureBias);
	CHECK_OVERRIDE_FLOAT(AutoExposureBiasBackup);
	CHECK_OVERRIDE_CURVE(AutoExposureBiasCurve);
	CHECK_OVERRIDE_TEXTURE(AutoExposureMeterMask);
	CHECK_OVERRIDE_BOOL(AutoExposureApplyPhysicalCameraExposure);
	CHECK_OVERRIDE_FLOAT(HistogramLogMin);
	CHECK_OVERRIDE_FLOAT(HistogramLogMax);

	CHECK_OVERRIDE_INT(LocalExposureMethod);
	CHECK_OVERRIDE_FLOAT(LocalExposureHighlightContrastScale);
	CHECK_OVERRIDE_FLOAT(LocalExposureShadowContrastScale);
	CHECK_OVERRIDE_FLOAT(LocalExposureDetailStrength);
	CHECK_OVERRIDE_FLOAT(LocalExposureBlurredLuminanceBlend);
	CHECK_OVERRIDE_FLOAT(LocalExposureBlurredLuminanceKernelSizePercent);
	CHECK_OVERRIDE_FLOAT(LocalExposureMiddleGreyBias);

	CHECK_OVERRIDE_FLOAT(LensFlareIntensity);
	CHECK_OVERRIDE_COLOR(LensFlareTint);
	CHECK_OVERRIDE_FLOAT(LensFlareBokehSize);
	CHECK_OVERRIDE_FLOAT(LensFlareThreshold);
	CHECK_OVERRIDE_FLOAT(VignetteIntensity);
	CHECK_OVERRIDE_FLOAT(Sharpen);

	CHECK_OVERRIDE_FLOAT(FilmGrainIntensity);
	CHECK_OVERRIDE_FLOAT(FilmGrainIntensityShadows);
	CHECK_OVERRIDE_FLOAT(FilmGrainIntensityMidtones);
	CHECK_OVERRIDE_FLOAT(FilmGrainIntensityHighlights);

	CHECK_OVERRIDE_FLOAT(AmbientOcclusionIntensity);
	CHECK_OVERRIDE_FLOAT(AmbientOcclusionStaticFraction);
	CHECK_OVERRIDE_FLOAT(AmbientOcclusionRadius);
	CHECK_OVERRIDE_FLOAT(AmbientOcclusionFadeDistance);
	CHECK_OVERRIDE_FLOAT(AmbientOcclusionFadeRadius);
	CHECK_OVERRIDE_BOOL(AmbientOcclusionRadiusInWS);
	CHECK_OVERRIDE_FLOAT(AmbientOcclusionPower);
	CHECK_OVERRIDE_FLOAT(AmbientOcclusionBias);
	CHECK_OVERRIDE_FLOAT(AmbientOcclusionQuality);

	CHECK_OVERRIDE_FLOAT(IndirectLightingIntensity);
	CHECK_OVERRIDE_COLOR(IndirectLightingColor);

	CHECK_OVERRIDE_FLOAT(DepthOfFieldFocalDistance);
	CHECK_OVERRIDE_FLOAT(DepthOfFieldFstop);
	CHECK_OVERRIDE_FLOAT(DepthOfFieldMinFstop);
	CHECK_OVERRIDE_INT(DepthOfFieldBladeCount);
	CHECK_OVERRIDE_FLOAT(DepthOfFieldSensorWidth);
	CHECK_OVERRIDE_FLOAT(DepthOfFieldSqueezeFactor);
	CHECK_OVERRIDE_FLOAT(DepthOfFieldDepthBlurRadius);
	CHECK_OVERRIDE_FLOAT(DepthOfFieldDepthBlurAmount);
	CHECK_OVERRIDE_FLOAT(DepthOfFieldFocalRegion);
	CHECK_OVERRIDE_FLOAT(DepthOfFieldNearTransitionRegion);
	CHECK_OVERRIDE_FLOAT(DepthOfFieldFarTransitionRegion);
	CHECK_OVERRIDE_FLOAT(DepthOfFieldScale);
	CHECK_OVERRIDE_FLOAT(DepthOfFieldNearBlurSize);
	CHECK_OVERRIDE_FLOAT(DepthOfFieldFarBlurSize);
	CHECK_OVERRIDE_FLOAT(DepthOfFieldOcclusion);
	CHECK_OVERRIDE_FLOAT(DepthOfFieldSkyFocusDistance);
	CHECK_OVERRIDE_FLOAT(DepthOfFieldVignetteSize);

	CHECK_OVERRIDE_FLOAT(MotionBlurAmount);
	CHECK_OVERRIDE_FLOAT(MotionBlurMax);
	CHECK_OVERRIDE_INT(MotionBlurTargetFPS);
	CHECK_OVERRIDE_FLOAT(MotionBlurPerObjectSize);

	CHECK_OVERRIDE_INT(ReflectionMethod);
	CHECK_OVERRIDE_FLOAT(LumenReflectionQuality);
	CHECK_OVERRIDE_FLOAT(ScreenSpaceReflectionIntensity);
	CHECK_OVERRIDE_FLOAT(ScreenSpaceReflectionQuality);
	CHECK_OVERRIDE_FLOAT(ScreenSpaceReflectionMaxRoughness);

	CHECK_OVERRIDE_INT(DynamicGlobalIlluminationMethod);
	CHECK_OVERRIDE_FLOAT(LumenSceneLightingQuality);
	CHECK_OVERRIDE_FLOAT(LumenSceneDetail);
	CHECK_OVERRIDE_FLOAT(LumenSceneViewDistance);
	CHECK_OVERRIDE_FLOAT(LumenSceneLightingUpdateSpeed);
	CHECK_OVERRIDE_FLOAT(LumenFinalGatherQuality);
	CHECK_OVERRIDE_FLOAT(LumenFinalGatherLightingUpdateSpeed);
	CHECK_OVERRIDE_BOOL(LumenFinalGatherScreenTraces);
	CHECK_OVERRIDE_FLOAT(LumenMaxTraceDistance);
	CHECK_OVERRIDE_FLOAT(LumenDiffuseColorBoost);
	CHECK_OVERRIDE_FLOAT(LumenSkylightLeaking);
	CHECK_OVERRIDE_COLOR(LumenSkylightLeakingTint);
	CHECK_OVERRIDE_FLOAT(LumenFullSkylightLeakingDistance);
	CHECK_OVERRIDE_INT(LumenRayLightingMode);
	CHECK_OVERRIDE_BOOL(LumenReflectionsScreenTraces);
	CHECK_OVERRIDE_BOOL(LumenFrontLayerTranslucencyReflections);
	CHECK_OVERRIDE_FLOAT(LumenMaxRoughnessToTraceReflections);
	CHECK_OVERRIDE_INT(LumenMaxReflectionBounces);
	CHECK_OVERRIDE_INT(LumenMaxRefractionBounces);

	CHECK_OVERRIDE_BOOL(RayTracingAO);
	CHECK_OVERRIDE_INT(RayTracingAOSamplesPerPixel);
	CHECK_OVERRIDE_FLOAT(RayTracingAOIntensity);
	CHECK_OVERRIDE_FLOAT(RayTracingAORadius);

	// CHECK_OVERRIDE_FLOAT(RayTracingReflectionsMaxRoughness);
	// CHECK_OVERRIDE_INT(RayTracingReflectionsMaxBounces);
	// CHECK_OVERRIDE_INT(RayTracingReflectionsSamplesPerPixel);
	// CHECK_OVERRIDE_INT(RayTracingReflectionsShadows);
	// CHECK_OVERRIDE_INT(RayTracingReflectionsTranslucency);

	CHECK_OVERRIDE_INT(TranslucencyType);
	CHECK_OVERRIDE_FLOAT(RayTracingTranslucencyMaxRoughness);
	CHECK_OVERRIDE_INT(RayTracingTranslucencyRefractionRays);
	CHECK_OVERRIDE_INT(RayTracingTranslucencySamplesPerPixel);
	CHECK_OVERRIDE_INT(RayTracingTranslucencyShadows);
	CHECK_OVERRIDE_INT(RayTracingTranslucencyRefraction);

	CHECK_OVERRIDE_INT(RayTracingTranslucencyMaxPrimaryHitEvents);
	CHECK_OVERRIDE_INT(RayTracingTranslucencyMaxSecondaryHitEvents);
	CHECK_OVERRIDE_BOOL(RayTracingTranslucencyUseRayTracedRefraction);

	// CHECK_OVERRIDE_BOOL(RayTracingGI);
	// CHECK_OVERRIDE_INT(RayTracingGIMaxBounces);
	// CHECK_OVERRIDE_INT(RayTracingGISamplesPerPixel);

	CHECK_OVERRIDE_INT(PathTracingMaxBounces);
	CHECK_OVERRIDE_INT(PathTracingSamplesPerPixel);
	CHECK_OVERRIDE_FLOAT(PathTracingMaxPathIntensity);
	CHECK_OVERRIDE_BOOL(PathTracingEnableEmissiveMaterials);
	CHECK_OVERRIDE_BOOL(PathTracingEnableReferenceDOF);
	CHECK_OVERRIDE_BOOL(PathTracingEnableReferenceAtmosphere);
	CHECK_OVERRIDE_BOOL(PathTracingEnableDenoiser);
	CHECK_OVERRIDE_BOOL(PathTracingIncludeEmissive);
	CHECK_OVERRIDE_BOOL(PathTracingIncludeDiffuse);
	CHECK_OVERRIDE_BOOL(PathTracingIncludeIndirectDiffuse);
	CHECK_OVERRIDE_BOOL(PathTracingIncludeSpecular);
	CHECK_OVERRIDE_BOOL(PathTracingIncludeIndirectSpecular);
	CHECK_OVERRIDE_BOOL(PathTracingIncludeVolume);
	CHECK_OVERRIDE_BOOL(PathTracingIncludeIndirectVolume);

	CHECK_OVERRIDE_COLOR(AmbientCubemapTint);
	CHECK_OVERRIDE_FLOAT(AmbientCubemapIntensity);

	CHECK_OVERRIDE_FLOAT(ColorGradingIntensity);
	CHECK_OVERRIDE_TEXTURE(ColorGradingLUT);

	CHECK_OVERRIDE_FLOAT(BloomConvolutionScatterDispersion);
	CHECK_OVERRIDE_FLOAT(BloomConvolutionSize);
	CHECK_OVERRIDE_FLOAT(BloomConvolutionPreFilterMin);
	CHECK_OVERRIDE_FLOAT(BloomConvolutionPreFilterMax);
	CHECK_OVERRIDE_FLOAT(BloomConvolutionPreFilterMult);
	CHECK_OVERRIDE_FLOAT(BloomConvolutionBufferScale);
	CHECK_OVERRIDE_VEC2(BloomConvolutionCenterUV);
	CHECK_OVERRIDE_TEXTURE(BloomConvolutionTexture);
	CHECK_OVERRIDE_TEXTURE(BloomDirtMask);

	CHECK_OVERRIDE_BOOL(DepthOfFieldUseHairDepth);
	CHECK_OVERRIDE_BOOL(DepthOfFieldPetzvalBokeh);
	CHECK_OVERRIDE_FLOAT(DepthOfFieldPetzvalBokehFalloff);
	CHECK_OVERRIDE_FLOAT(DepthOfFieldPetzvalExclusionBoxRadius);
	CHECK_OVERRIDE_FLOAT(DepthOfFieldAspectRatioScalar);
	// CHECK_OVERRIDE_INT(DepthOfFieldMatteBoxFlags);
	CHECK_OVERRIDE_FLOAT(DepthOfFieldBarrelRadius);
	CHECK_OVERRIDE_FLOAT(DepthOfFieldBarrelLength);

	CHECK_OVERRIDE_FLOAT(AmbientOcclusionMipBlend);
	CHECK_OVERRIDE_FLOAT(AmbientOcclusionMipScale);
	CHECK_OVERRIDE_FLOAT(AmbientOcclusionMipThreshold);
	CHECK_OVERRIDE_FLOAT(AmbientOcclusionTemporalBlendWeight);

	CHECK_OVERRIDE_FLOAT(FilmGrainShadowsMax);
	CHECK_OVERRIDE_FLOAT(FilmGrainHighlightsMin);
	CHECK_OVERRIDE_FLOAT(FilmGrainHighlightsMax);
	CHECK_OVERRIDE_FLOAT(FilmGrainTexelSize);
	CHECK_OVERRIDE_TEXTURE(FilmGrainTexture);

	// CHECK_OVERRIDE_FLOAT(LensFlareBokehShape);
	// CHECK_OVERRIDE_COLOR(LensFlareTints);

	CHECK_OVERRIDE_FLOAT(LocalExposureHighlightThreshold);
	CHECK_OVERRIDE_FLOAT(LocalExposureShadowThreshold);
	CHECK_OVERRIDE_FLOAT(LocalExposureHighlightThresholdStrength);
	CHECK_OVERRIDE_FLOAT(LocalExposureShadowThresholdStrength);
	CHECK_OVERRIDE_CURVE(LocalExposureHighlightContrastCurve);
	CHECK_OVERRIDE_CURVE(LocalExposureShadowContrastCurve);

	// CHECK_OVERRIDE_BOOL(MobileHQGaussian);
	// CHECK_OVERRIDE_FLOAT(ScreenSpaceReflectionRoughnessScale);
	CHECK_OVERRIDE_FLOAT(LumenSurfaceCacheResolution);
	CHECK_OVERRIDE_BOOL(bMegaLights);
	CHECK_OVERRIDE_INT(UserFlags);

	if (Settings.WeightedBlendables.Array.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("%s   WeightedBlendables.Num = %d"), *Prefix, Settings.WeightedBlendables.Array.Num());
	}

#undef CHECK_OVERRIDE_FLOAT
#undef CHECK_OVERRIDE_INT
#undef CHECK_OVERRIDE_BOOL
#undef CHECK_OVERRIDE_VEC4
#undef CHECK_OVERRIDE_COLOR
#undef CHECK_OVERRIDE_VEC2
#undef CHECK_OVERRIDE_TEXTURE
#undef CHECK_OVERRIDE_CURVE

	UE_LOG(LogTemp, Log, TEXT("%s ================================"), *Prefix);
}

void UMovieQualityRenderComponent::SetAllOverridePostProcessSettings(FPostProcessSettings& Settings)
{
#define SET_OVERRIDE_TRUE(Name) Settings.bOverride_##Name = true;

	SET_OVERRIDE_TRUE(TemperatureType);
	SET_OVERRIDE_TRUE(WhiteTemp);
	SET_OVERRIDE_TRUE(WhiteTint);

	SET_OVERRIDE_TRUE(ColorSaturation);
	SET_OVERRIDE_TRUE(ColorContrast);
	SET_OVERRIDE_TRUE(ColorGamma);
	SET_OVERRIDE_TRUE(ColorGain);
	SET_OVERRIDE_TRUE(ColorOffset);

	SET_OVERRIDE_TRUE(ColorSaturationShadows);
	SET_OVERRIDE_TRUE(ColorContrastShadows);
	SET_OVERRIDE_TRUE(ColorGammaShadows);
	SET_OVERRIDE_TRUE(ColorGainShadows);
	SET_OVERRIDE_TRUE(ColorOffsetShadows);

	SET_OVERRIDE_TRUE(ColorSaturationMidtones);
	SET_OVERRIDE_TRUE(ColorContrastMidtones);
	SET_OVERRIDE_TRUE(ColorGammaMidtones);
	SET_OVERRIDE_TRUE(ColorGainMidtones);
	SET_OVERRIDE_TRUE(ColorOffsetMidtones);

	SET_OVERRIDE_TRUE(ColorSaturationHighlights);
	SET_OVERRIDE_TRUE(ColorContrastHighlights);
	SET_OVERRIDE_TRUE(ColorGammaHighlights);
	SET_OVERRIDE_TRUE(ColorGainHighlights);
	SET_OVERRIDE_TRUE(ColorOffsetHighlights);

	SET_OVERRIDE_TRUE(ColorCorrectionShadowsMax);
	SET_OVERRIDE_TRUE(ColorCorrectionHighlightsMin);
	SET_OVERRIDE_TRUE(ColorCorrectionHighlightsMax);
	SET_OVERRIDE_TRUE(BlueCorrection);
	SET_OVERRIDE_TRUE(ExpandGamut);
	SET_OVERRIDE_TRUE(ToneCurveAmount);

	SET_OVERRIDE_TRUE(FilmSlope);
	SET_OVERRIDE_TRUE(FilmToe);
	SET_OVERRIDE_TRUE(FilmShoulder);
	SET_OVERRIDE_TRUE(FilmBlackClip);
	SET_OVERRIDE_TRUE(FilmWhiteClip);

	SET_OVERRIDE_TRUE(SceneColorTint);
	SET_OVERRIDE_TRUE(SceneFringeIntensity);
	SET_OVERRIDE_TRUE(ChromaticAberrationStartOffset);

	SET_OVERRIDE_TRUE(BloomMethod);
	SET_OVERRIDE_TRUE(BloomIntensity);
	SET_OVERRIDE_TRUE(BloomThreshold);
	SET_OVERRIDE_TRUE(BloomSizeScale);
	SET_OVERRIDE_TRUE(Bloom1Size);
	SET_OVERRIDE_TRUE(Bloom2Size);
	SET_OVERRIDE_TRUE(Bloom3Size);
	SET_OVERRIDE_TRUE(Bloom4Size);
	SET_OVERRIDE_TRUE(Bloom5Size);
	SET_OVERRIDE_TRUE(Bloom6Size);
	SET_OVERRIDE_TRUE(Bloom1Tint);
	SET_OVERRIDE_TRUE(Bloom2Tint);
	SET_OVERRIDE_TRUE(Bloom3Tint);
	SET_OVERRIDE_TRUE(Bloom4Tint);
	SET_OVERRIDE_TRUE(Bloom5Tint);
	SET_OVERRIDE_TRUE(Bloom6Tint);

	SET_OVERRIDE_TRUE(BloomDirtMaskIntensity);
	SET_OVERRIDE_TRUE(BloomDirtMaskTint);

	SET_OVERRIDE_TRUE(CameraShutterSpeed);
	SET_OVERRIDE_TRUE(CameraISO);
	SET_OVERRIDE_TRUE(AutoExposureMethod);
	SET_OVERRIDE_TRUE(AutoExposureLowPercent);
	SET_OVERRIDE_TRUE(AutoExposureHighPercent);
	SET_OVERRIDE_TRUE(AutoExposureMinBrightness);
	SET_OVERRIDE_TRUE(AutoExposureMaxBrightness);
	SET_OVERRIDE_TRUE(AutoExposureSpeedUp);
	SET_OVERRIDE_TRUE(AutoExposureSpeedDown);
	SET_OVERRIDE_TRUE(AutoExposureBias);
	SET_OVERRIDE_TRUE(AutoExposureBiasBackup);
	SET_OVERRIDE_TRUE(AutoExposureBiasCurve);
	SET_OVERRIDE_TRUE(AutoExposureMeterMask);
	SET_OVERRIDE_TRUE(AutoExposureApplyPhysicalCameraExposure);
	SET_OVERRIDE_TRUE(HistogramLogMin);
	SET_OVERRIDE_TRUE(HistogramLogMax);

	SET_OVERRIDE_TRUE(LocalExposureMethod);
	SET_OVERRIDE_TRUE(LocalExposureHighlightContrastScale);
	SET_OVERRIDE_TRUE(LocalExposureShadowContrastScale);
	SET_OVERRIDE_TRUE(LocalExposureDetailStrength);
	SET_OVERRIDE_TRUE(LocalExposureBlurredLuminanceBlend);
	SET_OVERRIDE_TRUE(LocalExposureBlurredLuminanceKernelSizePercent);
	SET_OVERRIDE_TRUE(LocalExposureMiddleGreyBias);

	SET_OVERRIDE_TRUE(LensFlareIntensity);
	SET_OVERRIDE_TRUE(LensFlareTint);
	SET_OVERRIDE_TRUE(LensFlareBokehSize);
	SET_OVERRIDE_TRUE(LensFlareThreshold);
	SET_OVERRIDE_TRUE(VignetteIntensity);
	SET_OVERRIDE_TRUE(Sharpen);

	SET_OVERRIDE_TRUE(FilmGrainIntensity);
	SET_OVERRIDE_TRUE(FilmGrainIntensityShadows);
	SET_OVERRIDE_TRUE(FilmGrainIntensityMidtones);
	SET_OVERRIDE_TRUE(FilmGrainIntensityHighlights);

	SET_OVERRIDE_TRUE(AmbientOcclusionIntensity);
	SET_OVERRIDE_TRUE(AmbientOcclusionStaticFraction);
	SET_OVERRIDE_TRUE(AmbientOcclusionRadius);
	SET_OVERRIDE_TRUE(AmbientOcclusionFadeDistance);
	SET_OVERRIDE_TRUE(AmbientOcclusionFadeRadius);
	SET_OVERRIDE_TRUE(AmbientOcclusionRadiusInWS);
	SET_OVERRIDE_TRUE(AmbientOcclusionPower);
	SET_OVERRIDE_TRUE(AmbientOcclusionBias);
	SET_OVERRIDE_TRUE(AmbientOcclusionQuality);

	SET_OVERRIDE_TRUE(IndirectLightingIntensity);
	SET_OVERRIDE_TRUE(IndirectLightingColor);

	SET_OVERRIDE_TRUE(DepthOfFieldFocalDistance);
	SET_OVERRIDE_TRUE(DepthOfFieldFstop);
	SET_OVERRIDE_TRUE(DepthOfFieldMinFstop);
	SET_OVERRIDE_TRUE(DepthOfFieldBladeCount);
	SET_OVERRIDE_TRUE(DepthOfFieldSensorWidth);
	SET_OVERRIDE_TRUE(DepthOfFieldSqueezeFactor);
	SET_OVERRIDE_TRUE(DepthOfFieldDepthBlurRadius);
	SET_OVERRIDE_TRUE(DepthOfFieldDepthBlurAmount);
	SET_OVERRIDE_TRUE(DepthOfFieldFocalRegion);
	SET_OVERRIDE_TRUE(DepthOfFieldNearTransitionRegion);
	SET_OVERRIDE_TRUE(DepthOfFieldFarTransitionRegion);
	SET_OVERRIDE_TRUE(DepthOfFieldScale);
	SET_OVERRIDE_TRUE(DepthOfFieldNearBlurSize);
	SET_OVERRIDE_TRUE(DepthOfFieldFarBlurSize);
	SET_OVERRIDE_TRUE(DepthOfFieldOcclusion);
	SET_OVERRIDE_TRUE(DepthOfFieldSkyFocusDistance);
	SET_OVERRIDE_TRUE(DepthOfFieldVignetteSize);

	SET_OVERRIDE_TRUE(MotionBlurAmount);
	SET_OVERRIDE_TRUE(MotionBlurMax);
	SET_OVERRIDE_TRUE(MotionBlurTargetFPS);
	SET_OVERRIDE_TRUE(MotionBlurPerObjectSize);

	SET_OVERRIDE_TRUE(ReflectionMethod);
	SET_OVERRIDE_TRUE(LumenReflectionQuality);
	SET_OVERRIDE_TRUE(ScreenSpaceReflectionIntensity);
	SET_OVERRIDE_TRUE(ScreenSpaceReflectionQuality);
	SET_OVERRIDE_TRUE(ScreenSpaceReflectionMaxRoughness);

	SET_OVERRIDE_TRUE(DynamicGlobalIlluminationMethod);
	SET_OVERRIDE_TRUE(LumenSceneLightingQuality);
	SET_OVERRIDE_TRUE(LumenSceneDetail);
	SET_OVERRIDE_TRUE(LumenSceneViewDistance);
	SET_OVERRIDE_TRUE(LumenSceneLightingUpdateSpeed);
	SET_OVERRIDE_TRUE(LumenFinalGatherQuality);
	SET_OVERRIDE_TRUE(LumenFinalGatherLightingUpdateSpeed);
	SET_OVERRIDE_TRUE(LumenFinalGatherScreenTraces);
	SET_OVERRIDE_TRUE(LumenMaxTraceDistance);
	SET_OVERRIDE_TRUE(LumenDiffuseColorBoost);
	SET_OVERRIDE_TRUE(LumenSkylightLeaking);
	SET_OVERRIDE_TRUE(LumenSkylightLeakingTint);
	SET_OVERRIDE_TRUE(LumenFullSkylightLeakingDistance);
	SET_OVERRIDE_TRUE(LumenRayLightingMode);
	SET_OVERRIDE_TRUE(LumenReflectionsScreenTraces);
	SET_OVERRIDE_TRUE(LumenFrontLayerTranslucencyReflections);
	SET_OVERRIDE_TRUE(LumenMaxRoughnessToTraceReflections);
	SET_OVERRIDE_TRUE(LumenMaxReflectionBounces);
	SET_OVERRIDE_TRUE(LumenMaxRefractionBounces);

	SET_OVERRIDE_TRUE(RayTracingAO);
	SET_OVERRIDE_TRUE(RayTracingAOSamplesPerPixel);
	SET_OVERRIDE_TRUE(RayTracingAOIntensity);
	SET_OVERRIDE_TRUE(RayTracingAORadius);

	// SET_OVERRIDE_TRUE(RayTracingReflectionsMaxRoughness);
	// SET_OVERRIDE_TRUE(RayTracingReflectionsMaxBounces);
	// SET_OVERRIDE_TRUE(RayTracingReflectionsSamplesPerPixel);
	// SET_OVERRIDE_TRUE(RayTracingReflectionsShadows);
	// SET_OVERRIDE_TRUE(RayTracingReflectionsTranslucency);

	SET_OVERRIDE_TRUE(TranslucencyType);
	SET_OVERRIDE_TRUE(RayTracingTranslucencyMaxRoughness);
	SET_OVERRIDE_TRUE(RayTracingTranslucencyRefractionRays);
	SET_OVERRIDE_TRUE(RayTracingTranslucencySamplesPerPixel);
	SET_OVERRIDE_TRUE(RayTracingTranslucencyShadows);
	SET_OVERRIDE_TRUE(RayTracingTranslucencyRefraction);

	SET_OVERRIDE_TRUE(RayTracingTranslucencyMaxPrimaryHitEvents);
	SET_OVERRIDE_TRUE(RayTracingTranslucencyMaxSecondaryHitEvents);
	SET_OVERRIDE_TRUE(RayTracingTranslucencyUseRayTracedRefraction);

	// SET_OVERRIDE_TRUE(RayTracingGI);
	// SET_OVERRIDE_TRUE(RayTracingGIMaxBounces);
	// SET_OVERRIDE_TRUE(RayTracingGISamplesPerPixel);

	SET_OVERRIDE_TRUE(PathTracingMaxBounces);
	SET_OVERRIDE_TRUE(PathTracingSamplesPerPixel);
	SET_OVERRIDE_TRUE(PathTracingMaxPathIntensity);
	SET_OVERRIDE_TRUE(PathTracingEnableEmissiveMaterials);
	SET_OVERRIDE_TRUE(PathTracingEnableReferenceDOF);
	SET_OVERRIDE_TRUE(PathTracingEnableReferenceAtmosphere);
	SET_OVERRIDE_TRUE(PathTracingEnableDenoiser);
	SET_OVERRIDE_TRUE(PathTracingIncludeEmissive);
	SET_OVERRIDE_TRUE(PathTracingIncludeDiffuse);
	SET_OVERRIDE_TRUE(PathTracingIncludeIndirectDiffuse);
	SET_OVERRIDE_TRUE(PathTracingIncludeSpecular);
	SET_OVERRIDE_TRUE(PathTracingIncludeIndirectSpecular);
	SET_OVERRIDE_TRUE(PathTracingIncludeVolume);
	SET_OVERRIDE_TRUE(PathTracingIncludeIndirectVolume);

	SET_OVERRIDE_TRUE(AmbientCubemapTint);
	SET_OVERRIDE_TRUE(AmbientCubemapIntensity);

	SET_OVERRIDE_TRUE(ColorGradingIntensity);
	SET_OVERRIDE_TRUE(ColorGradingLUT);

	SET_OVERRIDE_TRUE(BloomConvolutionScatterDispersion);
	SET_OVERRIDE_TRUE(BloomConvolutionSize);
	SET_OVERRIDE_TRUE(BloomConvolutionPreFilterMin);
	SET_OVERRIDE_TRUE(BloomConvolutionPreFilterMax);
	SET_OVERRIDE_TRUE(BloomConvolutionPreFilterMult);
	SET_OVERRIDE_TRUE(BloomConvolutionBufferScale);
	SET_OVERRIDE_TRUE(BloomConvolutionCenterUV);
	SET_OVERRIDE_TRUE(BloomConvolutionTexture);
	SET_OVERRIDE_TRUE(BloomDirtMask);

	SET_OVERRIDE_TRUE(DepthOfFieldUseHairDepth);
	SET_OVERRIDE_TRUE(DepthOfFieldPetzvalBokeh);
	SET_OVERRIDE_TRUE(DepthOfFieldPetzvalBokehFalloff);
	SET_OVERRIDE_TRUE(DepthOfFieldPetzvalExclusionBoxRadius);
	SET_OVERRIDE_TRUE(DepthOfFieldAspectRatioScalar);
	// SET_OVERRIDE_TRUE(DepthOfFieldMatteBoxFlags); 
	SET_OVERRIDE_TRUE(DepthOfFieldBarrelRadius);
	SET_OVERRIDE_TRUE(DepthOfFieldBarrelLength);

	SET_OVERRIDE_TRUE(AmbientOcclusionMipBlend);
	SET_OVERRIDE_TRUE(AmbientOcclusionMipScale);
	SET_OVERRIDE_TRUE(AmbientOcclusionMipThreshold);
	SET_OVERRIDE_TRUE(AmbientOcclusionTemporalBlendWeight);

	SET_OVERRIDE_TRUE(FilmGrainShadowsMax);
	SET_OVERRIDE_TRUE(FilmGrainHighlightsMin);
	SET_OVERRIDE_TRUE(FilmGrainHighlightsMax);
	SET_OVERRIDE_TRUE(FilmGrainTexelSize);
	SET_OVERRIDE_TRUE(FilmGrainTexture);

	
	// SET_OVERRIDE_TRUE(LensFlareBokehShape);
	// SET_OVERRIDE_TRUE(LensFlareTints);

	SET_OVERRIDE_TRUE(LocalExposureHighlightThreshold);
	SET_OVERRIDE_TRUE(LocalExposureShadowThreshold);
	SET_OVERRIDE_TRUE(LocalExposureHighlightThresholdStrength);
	SET_OVERRIDE_TRUE(LocalExposureShadowThresholdStrength);
	SET_OVERRIDE_TRUE(LocalExposureHighlightContrastCurve);
	SET_OVERRIDE_TRUE(LocalExposureShadowContrastCurve);

	// SET_OVERRIDE_TRUE(MobileHQGaussian);
	// SET_OVERRIDE_TRUE(ScreenSpaceReflectionRoughnessScale);
	
	SET_OVERRIDE_TRUE(LumenSurfaceCacheResolution);
	SET_OVERRIDE_TRUE(bMegaLights);
	SET_OVERRIDE_TRUE(UserFlags);

#undef SET_OVERRIDE_TRUE
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

	// PPSettings.bOverride_AmbientOcclusionQuality = true;
	// PPSettings.AmbientOcclusionQuality = 80.0f;
	// PPSettings.bOverride_RayTracingAO = true;
	// PPSettings.RayTracingAO = true;
	// PPSettings.bOverride_RayTracingAOSamplesPerPixel = true;
	// PPSettings.RayTracingAOSamplesPerPixel = 4;

	/////////////////////////////////////////////////////////
	// reduce ghosting phenomenon
	// https://www.reddit.com/r/UnrealEngine5/comments/182y8br/lumen_ghosting_on_moving_objects_please_help/
	// https://forums.unrealengine.com/t/desperate-for-a-definitve-answer-on-lumen-ghosting-issue/661853
	// https://dev.epicgames.com/community/learning/tutorials/mjo7/unreal-engine-temporal-quality-guide
	// PPSettings.bOverride_LumenSceneLightingUpdateSpeed = 1;
	// PPSettings.LumenSceneLightingUpdateSpeed = 2.0f;
	if (GlobalSettings.Override_LumenFinalGatherLightingUpdateSpeed)
	{
		PPSettings.bOverride_LumenFinalGatherLightingUpdateSpeed = 1;
		PPSettings.LumenFinalGatherLightingUpdateSpeed = GlobalSettings.LumenFinalGatherLightingUpdateSpeed;
	}
	// PPSettings.bOverride_LumenFinalGatherScreenTraces = 1;
	// PPSettings.LumenFinalGatherScreenTraces = 0;
	// PPSettings.bOverride_AmbientOcclusionTemporalBlendWeight = 1;
	// PPSettings.AmbientOcclusionTemporalBlendWeight = 0.0f;
	/////////////////////////////////////////////////////////

	PPSettings.bOverride_AutoExposureMethod = 1;
	PPSettings.AutoExposureMethod = GlobalSettings.ExposureMethod;
	// PPSettings.AutoExposureMethod = AEM_Manual;
	PPSettings.bOverride_AutoExposureBias = 1;
	// PPSettings.AutoExposureBias = GlobalSettings.ExposureBias;
	PPSettings.AutoExposureBias = 15.0f;
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

	// PPSettings.bOverride_ToneCurveAmount = 1;
	// PPSettings.ToneCurveAmount = 1.0f;
	// PPSettings.bOverride_ExpandGamut = 1;
	// PPSettings.ExpandGamut = 1.0f;

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