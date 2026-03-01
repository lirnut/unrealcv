#include "Sensor/CameraSensor/MainViewportRenderComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "UnrealClient.h"
#include "UnrealcvLog.h"
#include "Sensor/ImageWriteQueue.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

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

	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
	{
		FViewport* Viewport = GEngine->GameViewport->Viewport;
		CurrentSize = Viewport->GetSizeXY();
		UE_LOG(LogUnrealCV, Log, TEXT("MainViewportRenderComponent: Current viewport size: %d x %d"), CurrentSize.X, CurrentSize.Y);

		if (CurrentSize.X == ResolutionX && CurrentSize.Y == ResolutionY)
		{
			UE_LOG(LogUnrealCV, Log, TEXT("MainViewportRenderComponent: Resolution already correct, skipping resize"));
			bIsInitialized = true;
			return;
		}

		FViewportFrame* ViewportFrame = Viewport->GetViewportFrame();
		if (ViewportFrame)
		{
			UE_LOG(LogUnrealCV, Log, TEXT("MainViewportRenderComponent: Method 1 - ResizeFrame"));
			ViewportFrame->ResizeFrame(ResolutionX, ResolutionY, EWindowMode::Windowed);
		}

		// CurrentSize = Viewport->GetSizeXY();
		// if (CurrentSize.X != ResolutionX || CurrentSize.Y != ResolutionY)
		// {
		// 	UE_LOG(LogUnrealCV, Log, TEXT("MainViewportRenderComponent: Method 2 - UpdateViewportRHI"));
		// 	Viewport->UpdateViewportRHI(false, ResolutionX, ResolutionY, EWindowMode::Windowed, PF_A2B10G10R10);
		// }

		CurrentSize = Viewport->GetSizeXY();
		if (CurrentSize.X != ResolutionX || CurrentSize.Y != ResolutionY)
		{
			UWorld* World = GetWorld();
			if (World && World->GetFirstPlayerController())
			{
				UE_LOG(LogUnrealCV, Log, TEXT("MainViewportRenderComponent: Method 3 - ConsoleCommand"));
				FString ResCommand = FString::Printf(TEXT("r.setres %dx%d"), ResolutionX, ResolutionY);
				FString Result = World->GetFirstPlayerController()->ConsoleCommand(ResCommand, true);
				UE_LOG(LogUnrealCV, Log, TEXT("MainViewportRenderComponent: ConsoleCommand result: %s"), *Result);
			}
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

	bIsInitialized = true;
	UE_LOG(LogUnrealCV, Log, TEXT("MainViewportRenderComponent initialized: %d x %d"), ResolutionX, ResolutionY);
}

void UMainViewportRenderComponent::Shutdown()
{
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

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return;
	}

	FVector ComponentLocation = GetComponentLocation();
	FRotator ComponentRotation = GetComponentRotation();

	PC->ClientSetRotation(ComponentRotation);

	APawn* Pawn = PC->GetPawn();
	if (Pawn)
	{
		Pawn->SetActorLocation(ComponentLocation, false, nullptr, ETeleportType::TeleportPhysics);
	}
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
	UE_LOG(LogUnrealCV, Log, TEXT("MainViewportRenderComponent::CaptureFrame called"));

	if (!IsInitialized())
	{
		UE_LOG(LogUnrealCV, Error, TEXT("MainViewportRenderComponent not initialized"));
		if (OnPixelDataReady)
		{
			OnPixelDataReady(nullptr);
		}
		return;
	}

	if (!ViewportClient || !ViewportClient->Viewport)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("Viewport not available. ViewportClient=%p, Viewport=%p"),
			ViewportClient, ViewportClient ? ViewportClient->Viewport : nullptr);
		if (OnPixelDataReady)
		{
			OnPixelDataReady(nullptr);
		}
		return;
	}

	FViewport* Viewport = ViewportClient->Viewport;
	FIntPoint ViewportSize = Viewport->GetSizeXY();
	UE_LOG(LogUnrealCV, Log, TEXT("MainViewportRenderComponent: Viewport size = %d x %d"), ViewportSize.X, ViewportSize.Y);

	TArray<FColor> Bitmap;
	UE_LOG(LogUnrealCV, Log, TEXT("MainViewportRenderComponent: Calling GetViewportScreenShot..."));
	bool bSuccess = GetViewportScreenShot(Viewport, Bitmap);
	UE_LOG(LogUnrealCV, Log, TEXT("MainViewportRenderComponent: GetViewportScreenShot returned %d, Bitmap.Num()=%d"), bSuccess, Bitmap.Num());

	if (!bSuccess || Bitmap.Num() == 0)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("Failed to capture viewport screenshot. bSuccess=%d, Bitmap.Num()=%d"), bSuccess, Bitmap.Num());
		if (OnPixelDataReady)
		{
			OnPixelDataReady(nullptr);
		}
		return;
	}

	for (auto& Color : Bitmap)
	{
		Color.A = 255;
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
	if (World && World->GetFirstPlayerController())
	{
		FString FOVCommand = FString::Printf(TEXT("FOV %.1f"), FOV);
		UE_LOG(LogUnrealCV, Log, TEXT("MainViewportRenderComponent: Executing '%s'"), *FOVCommand);
		FString Result = World->GetFirstPlayerController()->ConsoleCommand(FOVCommand, true);
		if (!Result.IsEmpty())
		{
			UE_LOG(LogUnrealCV, Log, TEXT("MainViewportRenderComponent: Command result: %s"), *Result);
		}
		else
		{
			UE_LOG(LogUnrealCV, Log, TEXT("MainViewportRenderComponent: FOV command executed"));
		}
	}
	else
	{
		UE_LOG(LogUnrealCV, Error, TEXT("MainViewportRenderComponent: Failed to get World or PlayerController"));
	}
}
