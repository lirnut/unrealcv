#include "MovieQualityRenderSubsystem.h"
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

UMovieQualityRenderSubsystem::UMovieQualityRenderSubsystem()
	: World(nullptr)
	, Resolution(1920, 1080)
	, bIsInitialized(false)
	, ImageWriteQueue(nullptr)
{
}

UMovieQualityRenderSubsystem::~UMovieQualityRenderSubsystem()
{
	Shutdown();
}

void UMovieQualityRenderSubsystem::Initialize(UWorld* InWorld, FIntPoint InResolution)
{
	if (bIsInitialized)
	{
		return;
	}

	World = InWorld;
	Resolution = InResolution;

	ViewState.Allocate(World->GetFeatureLevel());

	SurfaceQueue = MakeShared<FMoviePipelineSurfaceQueue, ESPMode::ThreadSafe>(
		Resolution,
		EPixelFormat::PF_FloatRGBA,
		3,
		true
	);

	ImageWriteQueue = &FModuleManager::Get().LoadModuleChecked<IImageWriteQueueModule>("ImageWriteQueue").GetWriteQueue();

	bIsInitialized = true;

	UE_LOG(LogTemp, Log, TEXT("MovieQualityRenderSubsystem initialized at %dx%d"), Resolution.X, Resolution.Y);
}

void UMovieQualityRenderSubsystem::Shutdown()
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

void UMovieQualityRenderSubsystem::ApplyMovieQualitySettings()
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

	UE_LOG(LogTemp, Log, TEXT("Applied Movie Quality Settings"));
}

void UMovieQualityRenderSubsystem::RestoreQualitySettings()
{
	IConsoleManager& ConsoleMgr = IConsoleManager::Get();

	for (const auto& Pair : PreviousQualitySettings)
	{
		if (IConsoleVariable* CVar = ConsoleMgr.FindConsoleVariable(*Pair.Key))
		{
			CVar->Set(Pair.Value);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Restored Previous Quality Settings"));
}

void UMovieQualityRenderSubsystem::CaptureFrame(
	UFusionCamSensor* Sensor,
	const FString& OutputPath,
	const FString& PassName,
	int32 FrameNumber,
	TFunction<void(bool)> OnComplete)
{
	if (!bIsInitialized || !Sensor)
	{
		if (OnComplete)
		{
			OnComplete(false);
		}
		return;
	}

	FString PoolKey = FString::Printf(TEXT("%s_%dx%d"), *PassName, Resolution.X, Resolution.Y);
	UTextureRenderTarget2D* RenderTarget = nullptr;

	if (RenderTargetPool.Contains(PoolKey))
	{
		RenderTarget = RenderTargetPool[PoolKey];
	}
	else
	{
		RenderTarget = NewObject<UTextureRenderTarget2D>(this);
		RenderTarget->ClearColor = FLinearColor::Black;
		RenderTarget->TargetGamma = UTextureRenderTarget::GetDefaultDisplayGamma();
		RenderTarget->InitCustomFormat(Resolution.X, Resolution.Y, EPixelFormat::PF_FloatRGBA, true);
		RenderTarget->AddToRoot();
		RenderTargetPool.Add(PoolKey, RenderTarget);
	}

	TSharedPtr<FSceneViewFamilyContext> ViewFamily = CreateViewFamily(Sensor, RenderTarget);
	if (!ViewFamily.IsValid())
	{
		if (OnComplete)
		{
			OnComplete(false);
		}
		return;
	}

	FSceneView* View = CreateSceneView(ViewFamily.Get(), Sensor);
	if (!View)
	{
		if (OnComplete)
		{
			OnComplete(false);
		}
		return;
	}

	SubmitToRenderer(ViewFamily.Get(), RenderTarget, OutputPath, PassName, FrameNumber, OnComplete);
}

TSharedPtr<FSceneViewFamilyContext> UMovieQualityRenderSubsystem::CreateViewFamily(
	UFusionCamSensor* Sensor,
	UTextureRenderTarget2D* RenderTarget)
{
	FRenderTarget* RenderTargetResource = RenderTarget->GameThread_GetRenderTargetResource();

	UBaseCameraSensor* LitSensor = Sensor->GetLitCamSensor();
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

FSceneView* UMovieQualityRenderSubsystem::CreateSceneView(
	FSceneViewFamily* ViewFamily,
	UFusionCamSensor* Sensor)
{
	FVector Location = Sensor->GetSensorLocation();
	FRotator Rotation = Sensor->GetSensorRotation();
	float FOV = Sensor->GetSensorFOV();

	FSceneViewInitOptions ViewInitOptions;
	ViewInitOptions.ViewFamily = ViewFamily;
	ViewInitOptions.ViewOrigin = Location;
	ViewInitOptions.SetViewRectangle(FIntRect(0, 0, Resolution.X, Resolution.Y));
	ViewInitOptions.ViewRotationMatrix = FInverseRotationMatrix(Rotation);
	ViewInitOptions.ViewActor = Sensor->GetOwner();

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

void UMovieQualityRenderSubsystem::SubmitToRenderer(
	FSceneViewFamily* ViewFamily,
	UTextureRenderTarget2D* RenderTarget,
	const FString& OutputPath,
	const FString& PassName,
	int32 FrameNumber,
	TFunction<void(bool)> OnComplete)
{
	FRenderTarget* RenderTargetResource = RenderTarget->GameThread_GetRenderTargetResource();

	FCanvas Canvas(RenderTargetResource, nullptr, World, ViewFamily->GetFeatureLevel(), FCanvas::CDM_DeferDrawing, 1.0f);
	GetRendererModule().BeginRenderingViewFamily(&Canvas, ViewFamily);

	TSharedRef<FImagePixelDataPayload, ESPMode::ThreadSafe> FramePayload = MakeShared<FImagePixelDataPayload, ESPMode::ThreadSafe>();

	auto Callback = [this, OutputPath, PassName, FrameNumber, OnComplete](TUniquePtr<FImagePixelData>&& InPixelData)
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