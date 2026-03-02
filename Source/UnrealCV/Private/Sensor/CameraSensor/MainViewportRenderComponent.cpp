#include "Sensor/CameraSensor/MainViewportRenderComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "UnrealClient.h"
#include "UnrealcvLog.h"
#include "Sensor/ImageWriteQueue.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "CoreGlobals.h"
#include "HighResScreenshot.h"
#include "Runtime\Engine\Public\Slate\SceneViewport.h"
#include "RHI.h"
#include "Sensor/CameraSensor/UnrealCVSurfaceReader.h"
#include "MovieRenderPipelineDataTypes.h"

FMVRCSettings UMainViewportRenderComponent::GlobalSettings;

UMainViewportRenderComponent::UMainViewportRenderComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.0f;
}

UMainViewportRenderComponent::~UMainViewportRenderComponent()
{
	Shutdown();
}

void UMainViewportRenderComponent::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	if (World)
	{
		ViewportClient = World->GetGameViewport();
	}

	ImageWriteQueue = MakeShared<FUnrealCVImageWriteQueue>();
}

void UMainViewportRenderComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Shutdown();
	Super::EndPlay(EndPlayReason);
}

void UMainViewportRenderComponent::Initialize(int32 ResolutionX, int32 ResolutionY)
{
	if (ResolutionX <= 0 || ResolutionY <= 0)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("Invalid resolution: %d x %d"), ResolutionX, ResolutionY);
		return;
	}

	FIntPoint CurrentSize;

	if (GEngine && GEngine->GameViewport)
	{
		SceneViewport = GEngine->GameViewport->GetGameViewport();
		FViewport* Viewport = SceneViewport;
		CurrentSize = Viewport->GetSizeXY();
		UE_LOG(LogUnrealCV, Log, TEXT("MainViewportRenderComponent: Current viewport size: %d x %d"), CurrentSize.X, CurrentSize.Y);

		if (CurrentSize.X == ResolutionX && CurrentSize.Y == ResolutionY)
		{
			UE_LOG(LogUnrealCV, Log, TEXT("MainViewportRenderComponent: Resolution already correct, skipping resize"));
			bIsInitialized = true;
			return;
		}

		// FViewportFrame* ViewportFrame = Viewport->GetViewportFrame();
		// if (ViewportFrame)
		// {
		// 	UE_LOG(LogUnrealCV, Log, TEXT("MainViewportRenderComponent: Method 1 - ResizeFrame"));
		// 	ViewportFrame->ResizeFrame(ResolutionX, ResolutionY, EWindowMode::Windowed);
		// }

		// CurrentSize = Viewport->GetSizeXY();
		// if (CurrentSize.X != ResolutionX || CurrentSize.Y != ResolutionY)
		// {
		// 	UE_LOG(LogUnrealCV, Log, TEXT("MainViewportRenderComponent: Method 2 - UpdateViewportRHI"));
		// 	Viewport->UpdateViewportRHI(false, ResolutionX, ResolutionY, EWindowMode::Windowed, PF_A2B10G10R10);
		// }

		// CurrentSize = Viewport->GetSizeXY();
		// if (CurrentSize.X != ResolutionX || CurrentSize.Y != ResolutionY)
		// {
		// 	UWorld* World = GetWorld();
		// 	if (World && World->GetFirstPlayerController())
		// 	{
		// 		UE_LOG(LogUnrealCV, Log, TEXT("MainViewportRenderComponent: Method 3 - ConsoleCommand"));
		// 		FString ResCommand = FString::Printf(TEXT("r.setres %dx%d"), ResolutionX, ResolutionY);
		// 		FString Result = World->GetFirstPlayerController()->ConsoleCommand(ResCommand, true);
		// 		UE_LOG(LogUnrealCV, Log, TEXT("MainViewportRenderComponent: ConsoleCommand result: %s"), *Result);
		// 	}
		// }


		if (SceneViewport)
		{
			SceneViewport->SetViewportSize(ResolutionX, ResolutionY);

			FIntPoint CSize = Viewport->GetSizeXY();
			UE_LOG(LogUnrealCV, Log, TEXT("MainViewportRenderComponent: Viewport size set to: %d x %d"), CSize.X, CSize.Y);
		}

		CurrentSize = Viewport->GetSizeXY();
		UE_LOG(LogUnrealCV, Log, TEXT("MainViewportRenderComponent: Final viewport size: %d x %d"), CurrentSize.X, CurrentSize.Y);

		if (CurrentSize.X != ResolutionX || CurrentSize.Y != ResolutionY)
		{
			UE_LOG(LogUnrealCV, Error, TEXT("MainViewportRenderComponent: Resize FAILED! Expected %dx%d, got %dx%d"),
				ResolutionX, ResolutionY, CurrentSize.X, CurrentSize.Y);
		}
		else
		{
			UE_LOG(LogUnrealCV, Log, TEXT("MainViewportRenderComponent: Resize SUCCESS"));
		}
	}
	else
	{
		UE_LOG(LogUnrealCV, Error, TEXT("MainViewportRenderComponent: GEngine or GameViewport is null!"));
		return;
	}

	if (SurfaceQueue.IsValid())
	{
		UE_LOG(LogUnrealCV, Log, TEXT("MainViewportRenderComponent: Shutting down existing SurfaceQueue"));
		SurfaceQueue->Shutdown();
		SurfaceQueue.Reset();
	}

	if (!SceneViewport)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("MainViewportRenderComponent: SceneViewport is null, cannot initialize SurfaceQueue"));
		bIsInitialized = false;
		return;
	}

	FViewportRHIRef TestViewportRHI = SceneViewport->GetViewportRHI();
	if (!IsValidRef(TestViewportRHI))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("MainViewportRenderComponent: ViewportRHI is invalid at initialization"));
		bIsInitialized = false;
		return;
	}

	FIntPoint ViewportSize(ResolutionX, ResolutionY);
	SurfaceQueue = MakeShared<FUnrealCVSurfaceQueue, ESPMode::ThreadSafe>(
		ViewportSize,
		PF_B8G8R8A8,
		20,
		false
	);

	SurfaceQueue->SetFrameResolveLatency(2);

	UE_LOG(LogUnrealCV, Log, TEXT("MainViewportRenderComponent: SurfaceQueue ZERO latency - synchronous readback for frame sync with BaseCameraSensor"));

	bIsInitialized = true;

	UWorld* World = GetWorld();
	if (World && World->GetFirstPlayerController() && World->GetFirstPlayerController()->PlayerCameraManager)
	{
		APlayerCameraManager* CamMgr = World->GetFirstPlayerController()->PlayerCameraManager;
		float CurrentAspectRatio = (float)ResolutionX / (float)ResolutionY;

		UE_LOG(LogUnrealCV, Log, TEXT("MainViewportRenderComponent: Setting initial AspectRatio %.4f for viewport %dx%d"),
			CurrentAspectRatio, ResolutionX, ResolutionY);

		CamMgr->DefaultAspectRatio = CurrentAspectRatio;
		CamMgr->bDefaultConstrainAspectRatio = false;
	}

	UE_LOG(LogUnrealCV, Log, TEXT("MainViewportRenderComponent initialized: %d x %d"), ResolutionX, ResolutionY);

	FlushRenderingCommands();
}

void UMainViewportRenderComponent::Shutdown()
{
	if (SurfaceQueue.IsValid())
	{
		UE_LOG(LogUnrealCV, Log, TEXT("MainViewportRenderComponent: Shutting down SurfaceQueue"));
		SurfaceQueue->Shutdown();
		SurfaceQueue.Reset();
	}

	if (ImageWriteQueue.IsValid())
	{
		ImageWriteQueue->Shutdown();
		ImageWriteQueue.Reset();
	}
	bIsInitialized = false;
}

void UMainViewportRenderComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// UWorld* World = GetWorld();
	// if (!World)
	// {
	// 	return;
	// }

	// APlayerController* PC = World->GetFirstPlayerController();
	// if (!PC)
	// {
	// 	return;
	// }

	// FVector ComponentLocation = GetComponentLocation();
	// FRotator ComponentRotation = GetComponentRotation();

	// PC->ClientSetRotation(ComponentRotation);

	// APawn* Pawn = PC->GetPawn();
	// if (Pawn)
	// {
	// 	// Pawn->SetActorLocation(ComponentLocation, false, nullptr, ETeleportType::TeleportPhysics);
    //   	FVector DeltaLocation = ComponentLocation - Pawn->GetActorLocation();
    //   	Pawn->AddActorWorldOffset(DeltaLocation, false, nullptr, ETeleportType::TeleportPhysics);
	// }
}

FIntPoint UMainViewportRenderComponent::GetViewportSize() const
{
	if (ViewportClient && ViewportClient->Viewport)
	{
		return ViewportClient->Viewport->GetSizeXY();
	}
	return FIntPoint::ZeroValue;
}

void UMainViewportRenderComponent::CaptureFrame(TFunction<void(TUniquePtr<FImagePixelData>&&)> OnPixelDataReady)
{
	if (GlobalSettings.bUseSyncCapture)
	{
		UE_LOG(LogUnrealCV, Log, TEXT("MainViewportRenderComponent::CaptureFrame - Redirecting to CaptureFrameSync (bUseSyncCapture=true)"));
		CaptureFrameSync(MoveTemp(OnPixelDataReady));
		return;
	}

	UE_LOG(LogUnrealCV, Log, TEXT("MainViewportRenderComponent::CaptureFrame (async mode)"));

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("World is null"));
		if (OnPixelDataReady) OnPixelDataReady(nullptr);
		return;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("PlayerController is null"));
		if (OnPixelDataReady) OnPixelDataReady(nullptr);
		return;
	}

	FVector ComponentLocation = GetComponentLocation();
	FRotator ComponentRotation = GetComponentRotation();

	AActor* Owner = GetOwner();
	FVector OwnerLocation = Owner ? Owner->GetActorLocation() : FVector::ZeroVector;

	UE_LOG(LogUnrealCV, Warning, TEXT("MainViewportRC::CaptureFrame - ComponentLocation: X=%.6f, Y=%.6f, Z=%.6f"), ComponentLocation.X, ComponentLocation.Y, ComponentLocation.Z);
	UE_LOG(LogUnrealCV, Warning, TEXT("MainViewportRC::CaptureFrame - Owner( FusionCamSensor ) Location: X=%.6f, Y=%.6f, Z=%.6f"), OwnerLocation.X, OwnerLocation.Y, OwnerLocation.Z);

	PC->ClientSetRotation(ComponentRotation);

	APawn* Pawn = PC->GetPawn();
	if (Pawn)
	{
		Pawn->SetActorLocation(ComponentLocation);
		UE_LOG(LogUnrealCV, Warning, TEXT("MainViewportRC::CaptureFrame - Pawn moved to: X=%.6f, Y=%.6f, Z=%.6f"), ComponentLocation.X, ComponentLocation.Y, ComponentLocation.Z);
	}

	APlayerCameraManager* CamMgr = PC->PlayerCameraManager;
	if (CamMgr)
	{
		FVector CurrentCamLoc = CamMgr->GetCameraLocation();
		FVector Offset = ComponentLocation - CurrentCamLoc;
		if (!Offset.IsNearlyZero())
		{
			CamMgr->ApplyWorldOffset(Offset, false);
		}

		FMinimalViewInfo POVInfo;
		POVInfo.Location = ComponentLocation;
		POVInfo.Rotation = ComponentRotation;
		POVInfo.FOV = FOV;
		POVInfo.AspectRatio = CamMgr->DefaultAspectRatio;
		POVInfo.bConstrainAspectRatio = false;
		CamMgr->FillCameraCache(POVInfo);
	}

	if (CamMgr && ViewportClient && ViewportClient->Viewport)
	{
		FIntPoint ViewportSize = ViewportClient->Viewport->GetSizeXY();
		if (ViewportSize.X > 0 && ViewportSize.Y > 0)
		{
			float CurrentAspectRatio = (float)ViewportSize.X / (float)ViewportSize.Y;
			CamMgr->DefaultAspectRatio = CurrentAspectRatio;
			CamMgr->bDefaultConstrainAspectRatio = false;

			if (FOV > 0.0f)
			{
				CamMgr->SetFOV(FOV);
			}

			FMinimalViewInfo POVInfo;
			CamMgr->GetCameraViewPoint(POVInfo.Location, POVInfo.Rotation);
			POVInfo.FOV = CamMgr->GetFOVAngle();
			POVInfo.AspectRatio = CamMgr->DefaultAspectRatio;
			POVInfo.bConstrainAspectRatio = CamMgr->bDefaultConstrainAspectRatio;
			FMatrix ProjectionMatrix = POVInfo.CalculateProjectionMatrix();

			UE_LOG(LogUnrealCV, Warning, TEXT("MainViewportRC::CaptureFrame CAMERA DEBUG:"));
			UE_LOG(LogUnrealCV, Warning, TEXT("  - Location: X=%.6f, Y=%.6f, Z=%.6f"), POVInfo.Location.X, POVInfo.Location.Y, POVInfo.Location.Z);
			UE_LOG(LogUnrealCV, Warning, TEXT("  - Rotation: P=%.6f, Y=%.6f, R=%.6f"), POVInfo.Rotation.Pitch, POVInfo.Rotation.Yaw, POVInfo.Rotation.Roll);
			UE_LOG(LogUnrealCV, Warning, TEXT("  - ViewportSize: %dx%d"), ViewportSize.X, ViewportSize.Y);
			UE_LOG(LogUnrealCV, Warning, TEXT("  - FOV: %.6f"), POVInfo.FOV);
			UE_LOG(LogUnrealCV, Warning, TEXT("  - AspectRatio: %.6f"), POVInfo.AspectRatio);
			UE_LOG(LogUnrealCV, Warning, TEXT("  - bConstrainAspectRatio: %d"), POVInfo.bConstrainAspectRatio);
			UE_LOG(LogUnrealCV, Warning, TEXT("  - ProjectionMatrix M[0][0]: %.6f, M[0][1]: %.6f"), ProjectionMatrix.M[0][0], ProjectionMatrix.M[0][1]);
			UE_LOG(LogUnrealCV, Warning, TEXT("  - ProjectionMatrix M[1][0]: %.6f, M[1][1]: %.6f"), ProjectionMatrix.M[1][0], ProjectionMatrix.M[1][1]);
		}
	}

	if (!IsInitialized() || !ViewportClient || !ViewportClient->Viewport || !SurfaceQueue.IsValid())
	{
		UE_LOG(LogUnrealCV, Error, TEXT("Component not properly initialized"));
		if (OnPixelDataReady) OnPixelDataReady(nullptr);
		return;
	}

	FViewport* Viewport = ViewportClient->Viewport;
	FIntPoint ViewportSize = Viewport->GetSizeXY();

	if (!SceneViewport)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("SceneViewport is null"));
		if (OnPixelDataReady) OnPixelDataReady(nullptr);
		return;
	}

	Viewport->Draw(false);
	// FlushRenderingCommands();

	FViewportRHIRef ViewportRHI = SceneViewport->GetViewportRHI();
	if (!IsValidRef(ViewportRHI))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("ViewportRHI is invalid after Draw"));
		if (OnPixelDataReady) OnPixelDataReady(nullptr);
		return;
	}

	UE_LOG(LogUnrealCV, Log, TEXT("Enqueueing async GPU readback for %dx%d"), ViewportSize.X, ViewportSize.Y);

	TSharedRef<FImagePixelDataPayload, ESPMode::ThreadSafe> FramePayload =
		MakeShared<FImagePixelDataPayload, ESPMode::ThreadSafe>();

	ENQUEUE_RENDER_COMMAND(MainViewportAsyncReadback)(
		[ViewportRHI_RT = ViewportRHI,
		 SurfaceQueue_RT = this->SurfaceQueue,
		 FramePayload_RT = FramePayload,
		 OnPixelDataReady_RT = MoveTemp(OnPixelDataReady)]
		(FRHICommandListImmediate& RHICmdList) mutable
		{
			FTextureRHIRef BackBuffer = RHIGetViewportBackBuffer(ViewportRHI_RT);
			if (!BackBuffer.IsValid())
			{
				UE_LOG(LogUnrealCV, Error, TEXT("BackBuffer is invalid"));
				if (OnPixelDataReady_RT) OnPixelDataReady_RT(nullptr);
				return;
			}

			UE_LOG(LogUnrealCV, Log, TEXT("BackBuffer: %dx%d"),
				BackBuffer->GetSizeX(), BackBuffer->GetSizeY());

			RHICmdList.Transition(FRHITransitionInfo(
				BackBuffer,
				ERHIAccess::Unknown,
				ERHIAccess::SRVGraphics
			));

			SurfaceQueue_RT->OnRenderTargetReady_RenderThread(
				BackBuffer,
				FramePayload_RT,
				MoveTemp(OnPixelDataReady_RT)
			);

			UE_LOG(LogUnrealCV, Log, TEXT("Enqueued to SurfaceQueue"));
		}
	);

	UE_LOG(LogUnrealCV, Log, TEXT("Async capture enqueued, callback will fire next frame"));
}

void UMainViewportRenderComponent::CaptureFrameSync(TFunction<void(TUniquePtr<FImagePixelData>&&)> OnPixelDataReady)
{
	UE_LOG(LogUnrealCV, Log, TEXT("MainViewportRenderComponent::CaptureFrameSync (synchronous mode)"));

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("World is null"));
		if (OnPixelDataReady) OnPixelDataReady(nullptr);
		return;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("PlayerController is null"));
		if (OnPixelDataReady) OnPixelDataReady(nullptr);
		return;
	}

	FVector ComponentLocation = GetComponentLocation();
	FRotator ComponentRotation = GetComponentRotation();

	AActor* Owner = GetOwner();
	FVector OwnerLocation = Owner ? Owner->GetActorLocation() : FVector::ZeroVector;

	UE_LOG(LogUnrealCV, Warning, TEXT("MainViewportRC: ComponentLocation: X=%.6f, Y=%.6f, Z=%.6f"), ComponentLocation.X, ComponentLocation.Y, ComponentLocation.Z);
	UE_LOG(LogUnrealCV, Warning, TEXT("MainViewportRC: Owner( FusionCamSensor ) Location: X=%.6f, Y=%.6f, Z=%.6f"), OwnerLocation.X, OwnerLocation.Y, OwnerLocation.Z);

	PC->ClientSetRotation(ComponentRotation);

	APawn* Pawn = PC->GetPawn();
	if (Pawn)
	{
		Pawn->SetActorLocation(ComponentLocation);
		UE_LOG(LogUnrealCV, Warning, TEXT("MainViewportRC: Pawn moved to: X=%.6f, Y=%.6f, Z=%.6f"), ComponentLocation.X, ComponentLocation.Y, ComponentLocation.Z);
	}

	APlayerCameraManager* CamMgr = PC->PlayerCameraManager;
	if (CamMgr)
	{
		FVector CurrentCamLoc = CamMgr->GetCameraLocation();
		FVector Offset = ComponentLocation - CurrentCamLoc;
		if (!Offset.IsNearlyZero())
		{
			CamMgr->ApplyWorldOffset(Offset, false);
		}

		FMinimalViewInfo POVInfo;
		POVInfo.Location = ComponentLocation;
		POVInfo.Rotation = ComponentRotation;
		POVInfo.FOV = FOV;
		POVInfo.AspectRatio = CamMgr->DefaultAspectRatio;
		POVInfo.bConstrainAspectRatio = false;
		CamMgr->FillCameraCache(POVInfo);
	}

	if (CamMgr && ViewportClient && ViewportClient->Viewport)
	{
		FIntPoint ViewportSize = ViewportClient->Viewport->GetSizeXY();
		if (ViewportSize.X > 0 && ViewportSize.Y > 0)
		{
			float CurrentAspectRatio = (float)ViewportSize.X / (float)ViewportSize.Y;
			CamMgr->DefaultAspectRatio = CurrentAspectRatio;
			CamMgr->bDefaultConstrainAspectRatio = false;

			if (FOV > 0.0f)
			{
				CamMgr->SetFOV(FOV);
			}

			FMinimalViewInfo POVInfo;
			CamMgr->GetCameraViewPoint(POVInfo.Location, POVInfo.Rotation);
			POVInfo.FOV = CamMgr->GetFOVAngle();
			POVInfo.AspectRatio = CamMgr->DefaultAspectRatio;
			POVInfo.bConstrainAspectRatio = CamMgr->bDefaultConstrainAspectRatio;
			FMatrix ProjectionMatrix = POVInfo.CalculateProjectionMatrix();

			UE_LOG(LogUnrealCV, Warning, TEXT("MainViewportRC::CaptureFrameSync CAMERA DEBUG:"));
			UE_LOG(LogUnrealCV, Warning, TEXT("  - Location: X=%.6f, Y=%.6f, Z=%.6f"), POVInfo.Location.X, POVInfo.Location.Y, POVInfo.Location.Z);
			UE_LOG(LogUnrealCV, Warning, TEXT("  - Rotation: P=%.6f, Y=%.6f, R=%.6f"), POVInfo.Rotation.Pitch, POVInfo.Rotation.Yaw, POVInfo.Rotation.Roll);
			UE_LOG(LogUnrealCV, Warning, TEXT("  - ViewportSize: %dx%d"), ViewportSize.X, ViewportSize.Y);
			UE_LOG(LogUnrealCV, Warning, TEXT("  - FOV: %.6f"), POVInfo.FOV);
			UE_LOG(LogUnrealCV, Warning, TEXT("  - AspectRatio: %.6f"), POVInfo.AspectRatio);
			UE_LOG(LogUnrealCV, Warning, TEXT("  - bConstrainAspectRatio: %d"), POVInfo.bConstrainAspectRatio);
			UE_LOG(LogUnrealCV, Warning, TEXT("  - ProjectionMatrix M[0][0]: %.6f, M[0][1]: %.6f"), ProjectionMatrix.M[0][0], ProjectionMatrix.M[0][1]);
			UE_LOG(LogUnrealCV, Warning, TEXT("  - ProjectionMatrix M[1][0]: %.6f, M[1][1]: %.6f"), ProjectionMatrix.M[1][0], ProjectionMatrix.M[1][1]);

			float FovRad = FMath::DegreesToRadians(POVInfo.FOV);
			float HalfHeight = ViewportSize.Y * 0.5f;
			float HalfWidth = ViewportSize.X * 0.5f;
			float Fy = HalfHeight / FMath::Tan(FovRad * 0.5f);
			float Fx = Fy / POVInfo.AspectRatio;
			float Cx = HalfWidth;
			float Cy = HalfHeight;
			UE_LOG(LogUnrealCV, Warning, TEXT("  - Intrinsics K: fx=%.2f, fy=%.2f, cx=%.2f, cy=%.2f"), Fx, Fy, Cx, Cy);
		}
	}

	if (!IsInitialized() || !ViewportClient || !ViewportClient->Viewport)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("Component not properly initialized"));
		if (OnPixelDataReady) OnPixelDataReady(nullptr);
		return;
	}

	FViewport* Viewport = ViewportClient->Viewport;
	FIntPoint ViewportSize = Viewport->GetSizeXY();

	if (!SceneViewport)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("SceneViewport is null"));
		if (OnPixelDataReady) OnPixelDataReady(nullptr);
		return;
	}

	bool bSuccess = false;
	TArray<FColor> Bitmap;

	Viewport->Draw(false);
	FlushRenderingCommands();

	FViewportRHIRef ViewportRHI = SceneViewport->GetViewportRHI();
	if (IsValidRef(ViewportRHI))
	{
		UE_LOG(LogUnrealCV, Log, TEXT("MainViewportRC: ViewportRHI is valid, reading backbuffer synchronously"));

		ENQUEUE_RENDER_COMMAND(ReadBackBufferAndPixelsSync)(
			[ViewportRHI_RT = ViewportRHI, ViewportSize_RT = ViewportSize, OutData_RT = &Bitmap](FRHICommandListImmediate& RHICmdList)
			{
				FTextureRHIRef BackBuffer = RHIGetViewportBackBuffer(ViewportRHI_RT);
				if (BackBuffer.IsValid())
				{
					UE_LOG(LogUnrealCV, Log, TEXT("ReadBackBuffer: BackBuffer is valid: %d x %d"),
						BackBuffer->GetSizeX(), BackBuffer->GetSizeY());

					FIntRect ReadRect(0, 0, BackBuffer->GetSizeX(), BackBuffer->GetSizeY());
					FReadSurfaceDataFlags Flags;
					Flags.SetLinearToGamma(false);

					RHICmdList.ReadSurfaceData(BackBuffer, ReadRect, *OutData_RT, Flags);

					UE_LOG(LogUnrealCV, Log, TEXT("ReadBackBuffer: ReadSurfaceData done, Bitmap.Num()=%d"), OutData_RT->Num());
				}
				else
				{
					UE_LOG(LogUnrealCV, Error, TEXT("ReadBackBuffer: BackBuffer is invalid"));
				}
			});
		FlushRenderingCommands();

		bSuccess = Bitmap.Num() > 0;
		UE_LOG(LogUnrealCV, Log, TEXT("MainViewportRC: Sync read backbuffer returned %d, Bitmap.Num()=%d"), bSuccess, Bitmap.Num());
	}
	else
	{
		UE_LOG(LogUnrealCV, Error, TEXT("MainViewportRC: ViewportRHI is invalid"));
	}

	if (!bSuccess || Bitmap.Num() == 0)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("Failed to capture viewport screenshot. bSuccess=%d, Bitmap.Num()=%d"), bSuccess, Bitmap.Num());
		if (OnPixelDataReady)
		{
			OnPixelDataReady(nullptr);
		}
		return;
	}

	TUniquePtr<FImagePixelData> ImageData = MakeUnique<TImagePixelData<FColor>>(
		FIntPoint(ViewportSize.X, ViewportSize.Y),
		TArray64<FColor>(MoveTemp(Bitmap))
	);

	if (OnPixelDataReady)
	{
		OnPixelDataReady(MoveTemp(ImageData));
	}
}

void UMainViewportRenderComponent::CaptureFrameToFile(const FString& OutputPath, TFunction<void(bool)> OnComplete)
{
	if (!IsInitialized())
	{
		UE_LOG(LogUnrealCV, Error, TEXT("MainViewportRenderComponent not initialized"));
		if (OnComplete)
		{
			OnComplete(false);
		}
		return;
	}

	CaptureFrame([this, OutputPath, OnComplete](TUniquePtr<FImagePixelData>&& InPixelData)
	{
		if (!InPixelData.IsValid())
		{
			UE_LOG(LogUnrealCV, Error, TEXT("Invalid pixel data"));
			if (OnComplete)
			{
				OnComplete(false);
			}
			return;
		}

		TUniquePtr<FUnrealCVImageWriteTask> WriteTask = MakeUnique<FUnrealCVImageWriteTask>();
		WriteTask->PixelData = MoveTemp(InPixelData);
		WriteTask->Filename = OutputPath;
		WriteTask->Format = EImageFormat::PNG;
		WriteTask->CompressionQuality = 100;
		WriteTask->OnCompleted = OnComplete;

		if (ImageWriteQueue.IsValid())
		{
			ImageWriteQueue->Enqueue(MoveTemp(WriteTask));
		}
	});
}

void UMainViewportRenderComponent::CaptureScreen()
{
	if (ViewportClient && ViewportClient->Viewport)
	{
		ViewportClient->Viewport->Draw();
	}
}

void UMainViewportRenderComponent::SetFOV(float InFOV)
{
	FOV = InFOV;
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("MainViewportRenderComponent: World is null"));
		return;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("MainViewportRenderComponent: PlayerController is null"));
		return;
	}

	APlayerCameraManager* CamMgr = PC->PlayerCameraManager;
	if (!CamMgr)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("MainViewportRenderComponent: PlayerCameraManager is null"));
		return;
	}

	FIntPoint ViewportSize = GetViewportSize();
	if (ViewportSize.X > 0 && ViewportSize.Y > 0)
	{
		float CurrentAspectRatio = (float)ViewportSize.X / (float)ViewportSize.Y;

		UE_LOG(LogUnrealCV, Log, TEXT("MainViewportRenderComponent: Before - AspectRatio: %.4f, bConstrainAspectRatio: %d, DefaultFOV: %.2f"),
			CamMgr->DefaultAspectRatio, CamMgr->bDefaultConstrainAspectRatio, CamMgr->DefaultFOV);

		CamMgr->DefaultAspectRatio = CurrentAspectRatio;
		CamMgr->bDefaultConstrainAspectRatio = false;

		UE_LOG(LogUnrealCV, Log, TEXT("MainViewportRenderComponent: Setting FOV %.2f with AspectRatio %.4f (viewport %dx%d)"),
			InFOV, CurrentAspectRatio, ViewportSize.X, ViewportSize.Y);
	}
	else
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("MainViewportRenderComponent: Invalid viewport size %dx%d, using default AspectRatio"),
			ViewportSize.X, ViewportSize.Y);
	}

	CamMgr->SetFOV(InFOV);

	UE_LOG(LogUnrealCV, Log, TEXT("MainViewportRenderComponent: After - LockedFOV: %.2f, AspectRatio: %.4f"),
		CamMgr->GetFOVAngle(), CamMgr->DefaultAspectRatio);
}

float UMainViewportRenderComponent::GetActualFOV() const
{
	UWorld* World = GetWorld();
	if (World && World->GetFirstPlayerController() && World->GetFirstPlayerController()->PlayerCameraManager)
	{
		return World->GetFirstPlayerController()->PlayerCameraManager->GetFOVAngle();
	}
	return FOV;
}
