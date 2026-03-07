// shc @ 2025
// Modified for FusionCamSensor-specific recording
#include "FusionCamCaptureActor.h"
#include "Runtime/Core/Public/Misc/Paths.h"
#include "Runtime/Engine/Classes/Engine/World.h"
#include "Runtime/CoreUObject/Public/UObject/ConstructorHelpers.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"
#include "Runtime/Engine/Classes/Components/MaterialBillboardComponent.h"
#include "Runtime/Engine/Public/TimerManager.h"
#include "Runtime/Engine/Classes/Engine/Engine.h"
#include "Materials/Material.h"
#include "HAL/PlatformProcess.h"

#include "FusionCamSensor.h"
#include "LitCamSensor.h"
#include "DepthCamSensor.h"
#include "AnnotationCamSensor.h"
#include "NormalCamSensor.h"
#include "FlowCamSensor.h"
#include "ShadowCatcherCamSensor.h"
#include "StencilMaskCamSensor.h"
#include "MainViewportRenderComponent.h"
#include "BPFunctionLib/VisionBPLib.h"
#include "BPFunctionLib/SerializeBPLib.h"
#include "BPFunctionLib/RecordingBPLib.h"
#include "Controller/ActorController.h"
#include "UnrealcvLog.h"
#include "UnrealcvServer.h"
#include "AudioMixerDevice.h"
#include "Utils/Serialization.h"

FRecordingSettings AFusionCamCaptureActor::RecordingSettings;
#include "Utils/ImageUtil.h"
#include "Utils/PythonExecutor.h"
#include "Utils/GenericTickableObject.h"
#include "Utils/DeferredTaskScheduler.h"
#include "Misc/FileHelper.h"
#include "Serialization/BufferArchive.h"
#include "BPFunctionLib/LineTraceBPLib.h"
#include "BPFunctionLib/MetaDataBPLib.h"
#include "MovieQualityRenderComponent.h"
#include "MovieQualityRenderSubsystem.h"
#if PLATFORM_WINDOWS
#include "Encoder/UnrealCVMP4Encoder.h"
#include "Encoder/UnrealCVMP4EncoderCommon.h"
#endif

// static const float ROTATE_BUFFER_DURATION_SECONDS = 2.0f;
static const float ROTATE_BUFFER_DURATION_SECONDS = 0.0f;
// static const int32 ROTATE_NUM_FRAMES_OVERRIDE = 121;
// static const int32 WARM_UP_FRAMES = 45;

AFusionCamCaptureActor::AFusionCamCaptureActor()
{
	PrimaryActorTick.bCanEverTick = true;

	bIsRecording = false;
	bAddTimestamp = true;
	bTrackForegroundMovement = true;
	ForegroundMoveSpeed = 0.0f;
	ElapsedSteps = 0;
	TargetForeground = nullptr;
	BackupSensor = nullptr;
	BackupCameraID = -1;
	NumFrames = 0;
	RecordFPS = 0;

	TimeDilation = 0.25f;
	TimeDilationBackUp = 1.0f;

	CurrentTrajectoryIndex = 0;
	bPauseWorldDuringRecord = false;
	WarmUpElapsedFrames = 0;

	MovieQualityRenderer = nullptr;

	MP4EncodedFrameCount = 0;

	InterpolationAlpha = 0.0f;
	LastTrajectoryCaptureTime = 0.0f;
	bHasValidPrevPose = false;

	Billboard = CreateDefaultSubobject<UMaterialBillboardComponent>(TEXT("BillboardComponent"));
	if (!IsRunningCommandlet() && (Billboard != nullptr))
	{
		static ConstructorHelpers::FObjectFinder<UMaterial> MaterialAsset(TEXT("/Engine/EditorMaterials/HelpActorMaterial"));
		Billboard->AddElement(MaterialAsset.Object, nullptr, false, 32.0f, 32.0f, nullptr);
		Billboard->bIsEditorOnly = true;
		Billboard->bHiddenInGame = true;
	}
	RootComponent = Billboard;
}

void AFusionCamCaptureActor::BeginPlay()
{
	Super::BeginPlay();
}

void AFusionCamCaptureActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bIsRecording)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("AFusionCamCaptureActor::Tick - Not Recording, DeltaTime: %f"), DeltaTime);
		return;
	}

	if (!IsValid(TargetSensor))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("TargetSensor is null ..."));
		return;
	}

	if (bPaused)	
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("AFusionCamCaptureActor::Tick - Paused, DeltaTime: %f"), DeltaTime);
		return;
	}


	if (!IsValid(TargetForeground))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("TargetForeground is null ..."));
		return;
	}

	// the second if here particular for matting task will be deprecated, only with the first universal one
	if (CurrentTrajectoryIndex < CurrentTrajectory.Num())
	{

		FRotator OffsetRotator(0, ForegroundMoveAngleOffset, 0);
		FVector LocalDir = OffsetRotator.RotateVector(FVector::ForwardVector);
		FVector Delta = LocalDir * ForegroundMoveSpeed * DeltaTime;

		if (bTrackForegroundMovement && ForegroundMoveSpeed > 0)
		{
			TargetForeground->AddActorLocalOffset(Delta);
		}

		if (CurrentTrajectory[CurrentTrajectoryIndex].bManageTransform)
		{
			FVector ForegroundDisplacement = TargetForeground->GetActorLocation() - ForegroundStartPos;
			if (!bTrackForegroundMovement || FMath::Abs(ForegroundMoveSpeed) < 0.01f)
			{
				ForegroundDisplacement = FVector::ZeroVector;
			}
			UE_LOG(LogUnrealCV, Warning, TEXT("ForegroundDisplacement: %s"), *ForegroundDisplacement.ToString());
			FVector FinalLocation = CurrentTrajectory[CurrentTrajectoryIndex].Location + ForegroundDisplacement;
			UE_LOG(LogUnrealCV, Warning, TEXT("FinalLocation: %s"), *FinalLocation.ToString());

			UWorld* World = GetWorld();
			if (!World)
			{
				check(false);
			}
			APlayerController* PC = World->GetFirstPlayerController();
			if (!PC)
			{
				UE_LOG(LogUnrealCV, Error, TEXT("PC is null ..."));
				return;
			}
			PC->ClientSetRotation(CurrentTrajectory[CurrentTrajectoryIndex].Rotation * 0.9f + PC->GetPawn()->GetActorRotation() * 0.1f);

			APawn* Pawn = PC->GetPawn();
			if (Pawn)
			{
				FVector PawnDelta = FinalLocation - Pawn->GetActorLocation();
				if (!PawnDelta.IsNearlyZero(0.1f))
				{
					Pawn->AddActorWorldOffset(PawnDelta * 0.9f);
				}
			}
		}
		else
		{
			if (bTrackForegroundMovement && ForegroundMoveSpeed > 0)
			{
				FVector WorldDelta = TargetForeground->GetActorRotation().RotateVector(Delta);
				UWorld* World = GetWorld();
				if (!World)
				{
					check(false);
				}
				APlayerController* PC = World->GetFirstPlayerController();
				if (!PC)
				{
					UE_LOG(LogUnrealCV, Error, TEXT("PC is null ..."));
					return;
				}
				FRotator ComponentRotation = TargetSensor->GetSensorRotation();
				PC->ClientSetRotation(ComponentRotation);

				APawn* Pawn = PC->GetPawn();
				if (Pawn)
				{
					Pawn->AddActorWorldOffset(WorldDelta);
				}
			}
		}


	}



}

void AFusionCamCaptureActor::SetSceneHandle(const FSceneHandle& InSceneHandle)
{
	SceneHandle = InSceneHandle;
}

void AFusionCamCaptureActor::StopRecord()
{
	if (bIsRecording)
	{

		if (RecordingSettings.bUseMovieQualityRendering && IsValid(TargetSensor))
		{
			auto* Renderer = TargetSensor->GetMovieQualityRenderer();
			if (Renderer && Renderer->IsInitialized())
			{
				UE_LOG(LogUnrealCV, Log, TEXT("Flushing pending GPU readback frames..."));
				Renderer->FlushPendingFrames();
				UE_LOG(LogUnrealCV, Log, TEXT("GPU readback flush completed"));
			}
		}

		if (RecordingSettings.bRecordViaViewport && IsValid(TargetSensor))
		{
			auto* ViewportRenderer = TargetSensor->GetMainViewportRenderComponent();
			if (ViewportRenderer && ViewportRenderer->IsInitialized())
			{
				UE_LOG(LogUnrealCV, Log, TEXT("Flushing pending GPU readback frames (MainViewportRenderer)..."));
				ViewportRenderer->FlushPendingFrames();
				UE_LOG(LogUnrealCV, Log, TEXT("MainViewportRenderer flush completed"));
			}
		}

		if (IsValid(TargetSensor))
		{
			bool LocationManaged = false;
			for (int i = 0; i <= FMath::Min(CurrentTrajectoryIndex - 1, CurrentTrajectory.Num() - 1); i++)
			{
				if (CurrentTrajectory[i].bManageTransform)
				{
					LocationManaged = true;
					break;
				}
			}
			if (LocationManaged)
			{
				UE_LOG(LogUnrealCV, Log, TEXT("StopRecord: Restoring camera position to OriginalCameraLocation=(%.2f,%.2f,%.2f)"),
					OriginalCameraLocation.X, OriginalCameraLocation.Y, OriginalCameraLocation.Z);
				TargetSensor->SetSensorLocation(OriginalCameraLocation);
				TargetSensor->SetSensorRotation(OriginalCameraRotation);
			}
			else
			{
				UE_LOG(LogUnrealCV, Log, TEXT("StopRecord: No position restoration needed (LocationManaged=false)"));
			}

		}

		UE_LOG(LogUnrealCV, Log, TEXT("FusionCamCaptureActor: Stop recording. %d frames recorded. Real Duration: %.2fs, Real FPS: %.2f"),
			ElapsedSteps, RealWorldTimeDurationSeconds, RealWorldTimeFPS);


		if (RecordingDataTypes.bRecordAudio)
		{
			StopAudioRecord();
		}

		ForegroundMoveSpeed = 0.0f;

		GetWorld()->GetTimerManager().ClearTimer(TimerHandle_Record);
		bIsRecording = false;
		TargetForeground = nullptr;
		CurrentTrajectory.Empty();
		CurrentTrajectoryIndex = 0;

		bHasValidPrevPose = false;
		InterpolationAlpha = 0.0f;
		PrevTrajectoryLocation = FVector::ZeroVector;
		PrevTrajectoryRotation = FRotator::ZeroRotator;
		TargetTrajectoryLocation = FVector::ZeroVector;
		TargetTrajectoryRotation = FRotator::ZeroRotator;

		if (IsValid(BackupSensor))
		{
			BackupSensor = nullptr;
			BackupCameraID = -1;
		}

		if (bPauseWorldDuringRecord)
		{
			GetWorld()->GetFirstPlayerController()->SetPause(false);
		}

		if (TimeDilationBackUp > 0.01f && TimeDilationBackUp > GetWorld()->GetWorldSettings()->TimeDilation)
		{
			SetTimeDilationSlomo(TimeDilationBackUp);
		}
		TriggerVideoGeneration();
	}
	else
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("FusionCamCaptureActor: Stop recording but bIsRecording is false. CurrentTrajectoryIndex = %d, NumFrames = %d"), CurrentTrajectoryIndex, NumFrames);
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle_Record);
	}
}

void AFusionCamCaptureActor::ApplyRecordingConfig(const FRecordingDataTypesConfig& InConfig)
{
	RecordingDataTypes = InConfig;
}

void AFusionCamCaptureActor::OnTimerRecord()
{
	UE_LOG(LogUnrealCV, Warning, TEXT("[CHECKPOINT] OnTimerRecord START - CurrentTrajectoryIndex: %d, ElapsedSteps: %d"), CurrentTrajectoryIndex, ElapsedSteps);

	if (bPaused)
	{
		return;
	}

	if (!IsValid(TargetSensor))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("FusionCamCaptureActor: TargetSensor became invalid during recording!"));
		StopRecord();
		return;
	}

	if (CurrentTrajectoryIndex >= CurrentTrajectory.Num())
	{
		UE_LOG(LogUnrealCV, Log, TEXT("FusionCamCaptureActor: Stop recording normally. CurrentTrajectoryIndex = %d, NumFrames = %d"), CurrentTrajectoryIndex, NumFrames);
		StopRecord();
		return;
	}

	UE_LOG(LogUnrealCV, Warning, TEXT("[CHECKPOINT] OnTimerRecord - Before MoveTo lambda"));

	FVector ForegroundDisplacement = FVector::ZeroVector;
	if (bTrackForegroundMovement && IsValid(TargetForeground))
	{
		ForegroundDisplacement = TargetForeground->GetActorLocation() - ForegroundStartPos;
	}

	auto MoveTo = [this, ForegroundDisplacement] (
		FVector CurrentLocation,
		FVector TrajectoryLocation,
		FRotator Rotation
	)
	{
		FVector FinalLocation = TrajectoryLocation + ForegroundDisplacement;
		TargetSensor->SetSensorLocation(FinalLocation);
		TargetSensor->SetSensorRotation(Rotation);
	};

	float EffectiveTimeDilation = TimeDilation * CurrentTrajectory[CurrentTrajectoryIndex].DesiredEstTimeDilation;

	AWorldSettings* WorldSettings = GetWorld()->GetWorldSettings();

	if (FMath::IsNearlyZero(EffectiveTimeDilation))
	{
		WorldSettings->SetTimeDilation(0.0f);

		{
			if (CurrentTrajectory[CurrentTrajectoryIndex].bManageTransform)
			{
				MoveTo(
					TargetSensor->GetSensorLocation(),
					CurrentTrajectory[CurrentTrajectoryIndex].Location,
					CurrentTrajectory[CurrentTrajectoryIndex].Rotation
				);
			}

			UpdateFocalDistance();

			bool bWarmUp = ( WarmUpElapsedFrames < RecordingSettings.WarmUpFrames );
			RecordFrame(bWarmUp);
			FlushRenderingCommands();
			if (bWarmUp)
			{
				WarmUpElapsedFrames++;
			}
			else
			{
				ElapsedSteps++;
				CurrentTrajectoryIndex++;
			}
		}

		if (CurrentTrajectoryIndex < CurrentTrajectory.Num() &&
		 	 FMath::IsNearlyZero(TimeDilation * CurrentTrajectory[CurrentTrajectoryIndex].DesiredEstTimeDilation))
		{
			FDeferredTaskScheduler::Get().ScheduleTask([this]() {
				OnTimerRecord();
			},RecordingSettings.PausedTickInterval * CurrentTrajectory[CurrentTrajectoryIndex].PausedTickIntervalCoef);
		}
	}
	else
	{
		if (CurrentTrajectory[CurrentTrajectoryIndex].bManageTransform)
		{
			MoveTo(
				TargetSensor->GetSensorLocation(),
				CurrentTrajectory[CurrentTrajectoryIndex].Location,
				CurrentTrajectory[CurrentTrajectoryIndex].Rotation
			);
		}

		UpdateFocalDistance();

		bool bWarmUp = ( WarmUpElapsedFrames < RecordingSettings.WarmUpFrames );
		RecordFrame(bWarmUp);
		if (bWarmUp)
		{
			WarmUpElapsedFrames++;
		}
		else
		{
			ElapsedSteps++;
			CurrentTrajectoryIndex++;
		}
	}

	if (CurrentTrajectoryIndex < CurrentTrajectory.Num() && !FMath::IsNearlyZero(EffectiveTimeDilation))
	{
		EffectiveTimeDilation = TimeDilation * CurrentTrajectory[CurrentTrajectoryIndex].DesiredEstTimeDilation;
		SetTimeDilationSlomo(EffectiveTimeDilation);
	}


	if (CurrentTrajectoryIndex >= CurrentTrajectory.Num())
	{
		UE_LOG(LogUnrealCV, Log, TEXT("FusionCamCaptureActor: Stop recording normally. CurrentTrajectoryIndex = %d, NumFrames = %d"), CurrentTrajectoryIndex, NumFrames);
		StopRecord();
		return;
	}
}

void AFusionCamCaptureActor::SetTimeDilationSlomo(float TD)
{
	auto * World = GetWorld();
	AWorldSettings* WorldSettings = GetWorld()->GetWorldSettings();
	WorldSettings->SetTimeDilation(TD);
	FString Command = FString::Printf(TEXT("Slomo %f"), TD);
	FString Result = World->GetFirstPlayerController()->ConsoleCommand(Command, true);
	UE_LOG(LogUnrealCV, Log, TEXT("FusionCamCaptureActor: ConsoleCommand %s result: %s"), *Command, *Result);
}

void AFusionCamCaptureActor::UpdateFocalDistance()
{
	if (!IsValid(TargetSensor))
	{
		return;
	}

	FVector ForegroundPos = UnifiedTargetLocation;
	if (bTrackForegroundMovement && IsValid(TargetForeground))
	{
		FVector ForegroundDisplacement = TargetForeground->GetActorLocation() - ForegroundStartPos;
		ForegroundPos = UnifiedTargetLocation + ForegroundDisplacement;
	}

	float Distance = (ForegroundPos - TargetSensor->GetSensorLocation()).Size();
	const float FocalRegion = 1 * 100;
	float FocalDistance = FMath::Max(Distance - FocalRegion/2, 10.0f);

	TargetSensor->SetFocalParams(FocalDistance, FocalRegion);

	if (IsValid(BackupSensor))
	{
		BackupSensor->SetFocalParams(FocalDistance, FocalRegion);
	}
}

void AFusionCamCaptureActor::RecordFrame(bool bWarmUp)
{
	UE_LOG(LogUnrealCV, Warning, TEXT("[CHECKPOINT] RecordFrame START - ElapsedSteps: %d"), ElapsedSteps);
	UE_LOG(LogUnrealCV, Warning, TEXT("[CHECKPOINT] RecordFrame - Before acquiring RecordCriticalSection lock"));
	FScopeLock Lock(&RecordCriticalSection);
	UE_LOG(LogUnrealCV, Warning, TEXT("[CHECKPOINT] RecordFrame - Lock acquired"));

	auto SaveRGBToFile = [this](UFusionCamSensor *Sensor, const FString& FileName)
	{
		TArray<FColor> ImageData;
		int Width, Height;
		Sensor->GetLit(ImageData, Width, Height);
		AsyncTask(ENamedThreads::AnyThread, [this, ImageData = MoveTemp(ImageData), Width, Height, FileName]()
		{
			SerializeData(ImageData, Width, Height, FileName);
		});
	};

	auto SaveSegToFile = [this](UFusionCamSensor *Sensor, const FString& FileName)
	{
		TArray<FColor> ImageData;
		int Width, Height;
		Sensor->GetSeg(ImageData, Width, Height);
		AsyncTask(ENamedThreads::AnyThread, [this, ImageData = MoveTemp(ImageData), Width, Height, FileName]()
		{
			SerializeData(ImageData, Width, Height, FileName);
		});
	};


	if (RecordingSettings.bUseMovieQualityRendering || RecordingSettings.bRecordViaViewport)
	{
		TargetSensor->SetAsyncCaptureNextFrame(false);
	}

#if PLATFORM_WINDOWS
	if (bWarmUp)
	{
		if (RecordingSettings.bEnableH264Encoding && !MP4Encoder.IsValid())
		{
			InitializeH264Encoder(RecordFPS);
		}
	}
#endif

	bool bLastFrame = (CurrentTrajectoryIndex == CurrentTrajectory.Num() - 1);

	if (RecordingDataTypes.bRecordRGB)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("[CHECKPOINT] RecordFrame - Recording RGB"));

		if (RecordingSettings.bRecordViaViewport)
		{
			UE_LOG(LogUnrealCV, Log, TEXT("RecordingSettings.bRecordViaViewport: MP4Encoder.IsValid=%d"), MP4Encoder.IsValid());

			auto* ViewportCapture = TargetSensor->GetMainViewportRenderComponent();
			UE_LOG(LogUnrealCV, Log, TEXT("RecordingSettings.bRecordViaViewport: ViewportCapture=%p, IsInitialized=%d"),
				ViewportCapture, ViewportCapture ? ViewportCapture->IsInitialized() : false);
			if (ViewportCapture && ViewportCapture->IsInitialized())
			{
				UE_LOG(LogUnrealCV, Log, TEXT("RecordingSettings.bRecordViaViewport: ViewportCapture IsInitialized=true"));
#if PLATFORM_WINDOWS
				if (MP4Encoder && MP4Encoder->IsInitialized())
				{
					UE_LOG(LogUnrealCV, Log, TEXT("RecordingSettings.bRecordViaViewport: MP4Encoder IS initialized, using MP4 encoding"));
					FIntPoint ViewportSize = ViewportCapture->GetViewportSize();
					int32 EncoderWidth = MP4Encoder->GetWidth();
					int32 EncoderHeight = MP4Encoder->GetHeight();

					if (ViewportSize.X != EncoderWidth || ViewportSize.Y != EncoderHeight)
					{
						UE_LOG(LogUnrealCV, Warning, TEXT("Viewport size mismatch: Viewport=%dx%d, Encoder=%dx%d. Skipping frame."),
							ViewportSize.X, ViewportSize.Y, EncoderWidth, EncoderHeight);
					}
					else
					{
						UE_LOG(LogUnrealCV, Log, TEXT("Viewport: Calling CaptureFrame. ViewportSize=%dx%d, Encoder=%dx%d, bWarmUp=%d, bLastFrame=%d"),
							ViewportSize.X, ViewportSize.Y, EncoderWidth, EncoderHeight, bWarmUp, bLastFrame);
						ViewportCapture->CaptureFrame([this, bWarmUp, bLastFrame, ViewportSize](TUniquePtr<FImagePixelData>&& InPixelData)
						{
							UE_LOG(LogUnrealCV, Log, TEXT("Viewport CaptureFrame callback: InPixelData.IsValid()=%d"), InPixelData.IsValid());

							if (!InPixelData.IsValid())
							{
								UE_LOG(LogUnrealCV, Error, TEXT("Viewport: Invalid pixel data"));
								return;
							}

						AsyncTask(ENamedThreads::AnyThread, [this, bWarmUp, bLastFrame, ViewportSize, InPixelData = MoveTemp(InPixelData)]() mutable
						{
							FIntPoint PixelDataSize = InPixelData->GetSize();
							UE_LOG(LogUnrealCV, Log, TEXT("Viewport: PixelData size=%dx%d, Format=%d"), PixelDataSize.X, PixelDataSize.Y, (int32)InPixelData->GetType());

							const void* RawData = nullptr;
							int64 DataSize;
							InPixelData->GetRawData(RawData, DataSize);
							UE_LOG(LogUnrealCV, Log, TEXT("Viewport: RawData=%p, DataSize=%lld"), RawData, DataSize);

							if (!bWarmUp && RawData && DataSize > 0)
							{
								UE_LOG(LogUnrealCV, Log, TEXT("Viewport: Writing frame to MP4. MP4Encoder.IsValid=%d, IsInitialized=%d"),
									MP4Encoder.IsValid(), MP4Encoder->IsInitialized());

								bool bSuccess = MP4Encoder->WriteFrame((const uint8*)RawData, InPixelData->GetType());
								if (bSuccess)
								{
									MP4EncodedFrameCount++;
									UE_LOG(LogUnrealCV, Log, TEXT("Viewport: WriteFrame SUCCESS. FrameCount=%d"), MP4EncodedFrameCount);
								}
								else
								{
									UE_LOG(LogUnrealCV, Error, TEXT("Viewport H.264: WriteFrame FAILED! FrameCount=%d"), MP4EncodedFrameCount);
								}
							}

							if (bLastFrame)
							{
								MP4Encoder->TryFinalize();
								UE_LOG(LogUnrealCV, Log, TEXT("Viewport H.264 recording finished: %d frames -> %s"),
									   MP4EncodedFrameCount, *MP4OutputPath);
							}
						});
						});
					}
				}
				else
				{
					UE_LOG(LogUnrealCV, Log, TEXT("RecordingSettings.bRecordViaViewport: MP4Encoder NOT initialized or invalid, using PNG fallback"));
					FString FileNameRGB = MakeFilenameNewWithFolder("rgb", ".png", bWarmUp);
					ViewportCapture->CaptureFrameToFile(FileNameRGB);
				}
#endif
			}
		}
#if PLATFORM_WINDOWS
		else if (RecordingSettings.bUseMovieQualityRendering)
		{
			UE_LOG(LogUnrealCV, Warning, TEXT("[CHECKPOINT] RecordFrame - Before GetMovieQualityRenderer()"));
			auto* Renderer = TargetSensor->GetMovieQualityRenderer();
			UE_LOG(LogUnrealCV, Warning, TEXT("[CHECKPOINT] RecordFrame - After GetMovieQualityRenderer(), Renderer=%p"), Renderer);
			if (MP4Encoder && MP4Encoder->IsInitialized())
			{
				UE_LOG(LogUnrealCV, Warning, TEXT("[CHECKPOINT] RecordFrame - Using H.264 encoder"));
				Renderer->CaptureFrame([this, bWarmUp, bLastFrame](TUniquePtr<FImagePixelData>&& InPixelData)
				{
					if (!InPixelData.IsValid())
					{
						UE_LOG(LogUnrealCV, Warning, TEXT("H.264: Invalid pixel data"));
						return;
					}

					const void* RawData = nullptr;
					int64 DataSize;
					InPixelData->GetRawData(RawData, DataSize);

					if (!bWarmUp && RawData && DataSize > 0)
					{
						bool bSuccess = MP4Encoder->WriteFrame((const uint8*)RawData, InPixelData->GetType());
						if (bSuccess)
						{
							MP4EncodedFrameCount++;
						}
						else
						{
							UE_LOG(LogUnrealCV, Warning, TEXT("H.264: Failed to encode frame %d"), MP4EncodedFrameCount);
						}
					}

					if (bLastFrame)
					{
						MP4Encoder->TryFinalize();
						UE_LOG(LogUnrealCV, Log, TEXT("H.264 recording finished: %d frames -> %s"),
							   MP4EncodedFrameCount, *MP4OutputPath);
					}
				});
			}
			else
			{
				FString FileNameRGB = MakeFilenameNewWithFolder("rgb", ".png", bWarmUp);
				UE_LOG(LogUnrealCV, Warning, TEXT("[CHECKPOINT] RecordFrame - Before SaveLitToFile call"));
				Renderer->CaptureFrameToFile(
					FileNameRGB,
					[](bool bSuccess)
					{
						if (!bSuccess)
						{
							UE_LOG(LogUnrealCV, Warning, TEXT("MovieQualityRenderer: RGB capture failed"));
						}
					}
				);
				UE_LOG(LogUnrealCV, Warning, TEXT("[CHECKPOINT] RecordFrame - After SaveLitToFile call"));
			}
		}
#endif
		else
		{
			if (MP4Encoder && MP4Encoder->IsInitialized())
			{
				TArray<FColor> ImageData;
				int Width, Height;
				TargetSensor->GetLit(ImageData, Width, Height);
				if (!bWarmUp && Width > 0 && Height > 0)
				{
					bool bSuccess = MP4Encoder->WriteFrame((const uint8*)ImageData.GetData(), EImagePixelType::Color);
					if (bSuccess)
					{
						MP4EncodedFrameCount++;
					}
					else
					{
						UE_LOG(LogUnrealCV, Warning, TEXT("H.264: Failed to encode frame %d"), MP4EncodedFrameCount);
					}
					if (bLastFrame)
					{
						MP4Encoder->TryFinalize();
						UE_LOG(LogUnrealCV, Log, TEXT("H.264 recording finished: %d frames -> %s"),
							   MP4EncodedFrameCount, *MP4OutputPath);
					}
				}
			}
			else
			{
				FString FileNameRGB = MakeFilenameNewWithFolder("rgb", ".png", bWarmUp);
				TargetSensor->SaveLitToFile(FileNameRGB);
			}


		}
	}



	


	if (RecordingDataTypes.bRecordMask)
	{
		FString FileNameMask = MakeFilenameNewWithFolder("mask", ".png", bWarmUp);
		TargetSensor->SaveSegToFile(FileNameMask);
	}

	if (RecordingDataTypes.bRecordDepth)
	{
		FString DepthFilename = MakeFilenameNewWithFolder("depth", ".npy", bWarmUp);
		TargetSensor->SaveDepthToFile(DepthFilename);
	}

	if (RecordingDataTypes.bRecordNormal)
	{
		FString NormalFilename = MakeFilenameNewWithFolder("normal", ".png", bWarmUp);
		TargetSensor->SaveNormalToFile(NormalFilename);
	}

	if (RecordingDataTypes.bRecordFlow)
	{
		FString FlowFilename = MakeFilenameNewWithFolder("flow", ".png", bWarmUp);
		TargetSensor->SaveFlowToFile(FlowFilename);
	}

	if (RecordingDataTypes.bRecordOneObjectMask && IsValid(TargetForeground))
	{
		FString OneObjFilename = MakeFilenameNewWithFolder("oneobjmask", ".png", bWarmUp);
		TargetSensor->SaveOneObjMaskToFile(TargetForeground, OneObjFilename);
	}

	if (RecordingDataTypes.bRecordOneObjectLit && IsValid(TargetForeground))
	{
		FString OneObjLitFilename = MakeFilenameNewWithFolder("oneobjlit", ".png", bWarmUp);
		TargetSensor->SaveOneObjLitToFile(TargetForeground, OneObjLitFilename);
	}

	if (RecordingDataTypes.bRecordOneObjectGroomLit && IsValid(TargetForeground))
	{
		FString OneObjGroomLitFilename = MakeFilenameNewWithFolder("oneobjgroomlit", ".png", bWarmUp);
		TargetSensor->SaveOneObjGroomLitToFile(TargetForeground, OneObjGroomLitFilename);
	}

	if (RecordingDataTypes.bRecordShadowCatcher && IsValid(TargetForeground))
	{
		FString ShadowCatcherFilename = MakeFilenameNewWithFolder("shadowcatcher", ".png", bWarmUp);
		TargetSensor->SaveShadowCatcherToFile(TargetForeground, ShadowCatcherFilename);
		TargetSensor->GetShadowCatcherCamSensor()->Cleanup(TargetForeground);
	}

	if (RecordingDataTypes.bRecordStencilMask && IsValid(TargetForeground))
	{
		FString StencilMaskFilename = MakeFilenameNewWithFolder("stencilmask", ".png", bWarmUp);
		TargetSensor->SaveStencilMaskToFile(TargetForeground, StencilMaskFilename);
		TargetSensor->GetStencilMaskCamSensor()->Cleanup(TargetForeground);
	}







	if (RecordingDataTypes.bRecordWithoutTarget && IsValid(TargetForeground))
	{
		if (IsValid(BackupSensor))
		{
			BackupSensor->SetSensorLocation(TargetSensor->GetSensorLocation());
			BackupSensor->SetSensorRotation(TargetSensor->GetSensorRotation());

			if (RecordingDataTypes.bRecordRGB) BackupSensor->GetLitCamSensor()->HideActor(TargetForeground);
			if (RecordingDataTypes.bRecordMask) BackupSensor->GetAnnotationCamSensor()->HideActor(TargetForeground);
			if (RecordingDataTypes.bRecordDepth) BackupSensor->GetDepthCamSensor()->HideActor(TargetForeground);
			if (RecordingDataTypes.bRecordNormal) BackupSensor->GetNormalCamSensor()->HideActor(TargetForeground);
			if (RecordingDataTypes.bRecordFlow) BackupSensor->GetFlowCamSensor()->HideActor(TargetForeground);

			if (RecordingDataTypes.bRecordRGB)
			{
				FString FileNameRGB = MakeFilenameNewWithFolder("rgb_woTarget", ".png", bWarmUp);
				BackupSensor->SaveLitToFile(FileNameRGB);
				// SaveRGBToFile(BackupSensor, FileNameRGB);
			}

			if (RecordingDataTypes.bRecordMask)
			{
				FString FileNameMask = MakeFilenameNewWithFolder("mask_woTarget", ".png", bWarmUp);
				BackupSensor->SaveSegToFile(FileNameMask);
				// SaveSegToFile(BackupSensor, FileNameMask);
			}

			if (RecordingDataTypes.bRecordDepth)
			{
				FString DepthFilename = MakeFilenameNewWithFolder("depth_woTarget", ".npy", bWarmUp);
				BackupSensor->SaveDepthToFile(DepthFilename);
			}

			if (RecordingDataTypes.bRecordNormal)
			{
				FString NormalFilename = MakeFilenameNewWithFolder("normal_woTarget", ".png", bWarmUp);
				BackupSensor->SaveNormalToFile(NormalFilename);
			}

			if (RecordingDataTypes.bRecordFlow)
			{
				FString FlowFilename = MakeFilenameNewWithFolder("flow_woTarget", ".png", bWarmUp);
				BackupSensor->SaveFlowToFile(FlowFilename);
			}
		}
		else{
			UE_LOG(LogUnrealCV, Warning, TEXT("FusionCamCaptureActor: BackupSensor is invalid!"));
		}
	}

	if (RecordingDataTypes.bRecordMetadata)
	{
		SaveCameraMetadata(bWarmUp);
	}
}

void AFusionCamCaptureActor::StartAudioRecord()
{
	Audio::FMixerDevice* MixerDevice = GetAudioMixer();
	if (MixerDevice == nullptr)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("FusionCamCaptureActor: Failed to start audio recording - no audio mixer"));
		return;
	}

	MixerDevice->StartRecording(nullptr, 100.0f);
	UE_LOG(LogUnrealCV, Display, TEXT("FusionCamCaptureActor: Audio recording started"));
}

void AFusionCamCaptureActor::StopAudioRecord()
{
	Audio::FMixerDevice* MixerDevice = GetAudioMixer();
	if (MixerDevice == nullptr)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("FusionCamCaptureActor: Failed to stop audio recording - no audio mixer"));
		return;
	}

	float NumChannels = 1.0f;
	float SampleRate = 44100.0f;
	Audio::FAlignedFloatBuffer& RecordedBuffer = MixerDevice->StopRecording(nullptr, NumChannels, SampleRate);

	// Convert float audio to PCM16
	TArray<int16> PCM16Data;
	PCM16Data.Reserve(RecordedBuffer.Num());

	for (float Sample : RecordedBuffer)
	{
		float Clamped = FMath::Clamp(Sample, -1.0f, 1.0f);
		PCM16Data.Add(static_cast<int16>(Clamped * 32767.0f));
	}

	// Save as WAV file
	FString WavFileName = MakeFilenameNew("audio", ".wav");
	FBufferArchive WaveData;

	int32 NumSamples = PCM16Data.Num();
	int32 NumBytes = NumSamples * sizeof(int16);

	// Write WAV header
	WaveData.Serialize(const_cast<char*>("RIFF"), 4);
	int32 ChunkSize = 36 + NumBytes;
	WaveData << ChunkSize;
	WaveData.Serialize(const_cast<char*>("WAVE"), 4);

	// fmt chunk
	WaveData.Serialize(const_cast<char*>("fmt "), 4);
	int32 SubChunk1Size = 16;
	WaveData << SubChunk1Size;
	int16 AudioFormat = 1; // PCM
	WaveData << AudioFormat;
	int16 Channels = static_cast<int16>(NumChannels);
	WaveData << Channels;
	int32 SR = static_cast<int32>(SampleRate);
	WaveData << SR;
	int32 ByteRate = SR * Channels * sizeof(int16);
	WaveData << ByteRate;
	int16 BlockAlign = Channels * sizeof(int16);
	WaveData << BlockAlign;
	int16 BitsPerSample = 16;
	WaveData << BitsPerSample;

	// data chunk
	WaveData.Serialize(const_cast<char*>("data"), 4);
	WaveData << NumBytes;
	WaveData.Serialize(PCM16Data.GetData(), NumBytes);

	// Save to file
	FFileHelper::SaveArrayToFile(WaveData, *WavFileName);
	WaveData.FlushCache();
	WaveData.Empty();

	UE_LOG(LogUnrealCV, Display, TEXT("FusionCamCaptureActor: Audio saved to %s"), *WavFileName);
}

Audio::FMixerDevice* AFusionCamCaptureActor::GetAudioMixer()
{
	if (!IsValid(TargetSensor))
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	FVector CamLocation = TargetSensor->GetSensorLocation();
	FRotator CamRotation = TargetSensor->GetSensorRotation();

	FAudioDevice* AudioDevice = World->GetAudioDeviceRaw();
	if (!AudioDevice)
	{
		return nullptr;
	}

	FTransform ListenerTransform(CamRotation, CamLocation);
	AudioDevice->SetListener(World, 0, ListenerTransform, 0.0f);

	return static_cast<Audio::FMixerDevice*>(AudioDevice);
}

FString AFusionCamCaptureActor::MakeFilenameNew(FString DataType, FString FileExtension)
{
	// Find the position to insert frame number
	

	if (FileExtension.StartsWith(".")) {
		FileExtension.RemoveAt(0);
	}

	// Create filename with frame number and data type
	FString FileName = RecordFileName;
	FString FileBaseName = FString::Printf(TEXT("%d_%s.%s"), ElapsedSteps, *DataType, *FileExtension);
	// FString FileFolder = FString::Printf(TEXT("%d_%s.%s"), ElapsedSteps, *DataType, *FileExtension);
	FileName = FPaths::Combine(FileName, FileBaseName);
	// Combine with output folder
	FileName = FPaths::ConvertRelativePathToFull(FileName);

	return FileName;
}

FString AFusionCamCaptureActor::MakeFilenameNewWithFolder(FString DataType, FString FileExtension, bool bWarmUp)
{
	// Find the position to insert frame number


	if (FileExtension.StartsWith(".")) {
		FileExtension.RemoveAt(0);
	}

	// Create filename with frame number and data type
	FString FileName = RecordFileName;
	FString FileBaseName = FString::Printf(TEXT("%d_%s.%s"), ElapsedSteps, *DataType, *FileExtension);
	FString FileFolder;
	if (bWarmUp)
	{
		FileFolder = FString::Printf(TEXT("warmup/%s"), *DataType);
	}
	else
	{
		FileFolder = FString::Printf(TEXT("%s"), *DataType);
	}
	FileName = FPaths::Combine(FileName, FileFolder);
	FileName = FPaths::Combine(FileName, FileBaseName);
	// Combine with output folder
	FileName = FPaths::ConvertRelativePathToFull(FileName);

	return FileName;
}

void AFusionCamCaptureActor::SaveOverviewMetadata()
{
	if (!IsValid(TargetSensor))
	{
		return;
	}

	float FOV = TargetSensor->GetSensorFOV();
	int32 Width = TargetSensor->GetFilmWidth();
	int32 Height = TargetSensor->GetFilmHeight();
	FString ResolutionStr = FString::Printf(TEXT("%dx%d"), Width, Height);

	float ExposureSpeedDown = 0.0f, ExposureSpeedUp = 0.0f;
	TargetSensor->GetAutoExposureSpeed(ExposureSpeedDown, ExposureSpeedUp);

	float MotionBlurAmount = 0.0f, MotionBlurMax = 0.0f, MotionBlurPerObjectSize = 0.0f;
	int MotionBlurTargetFPS = 0;
	TargetSensor->GetMotionBlurParams(MotionBlurAmount, MotionBlurMax, MotionBlurPerObjectSize, MotionBlurTargetFPS);

	float FocalDistance = 0.0f, FocalRegion = 0.0f;
	TargetSensor->GetFocalParams(FocalDistance, FocalRegion);

	float ChromaticAberration = TargetSensor->GetChromaticAberration();
	float Vignette = TargetSensor->GetVignetteIntensity();

	EBloomMethod BloomMethod = EBloomMethod::BM_FFT;
	float BloomIntensity = 0.0f;
	TargetSensor->GetBloomParams(BloomMethod, BloomIntensity);

	auto ReflectionMethodToString = [](EReflectionMethod::Type Method) -> FString {
		switch (Method) {
			case EReflectionMethod::None: return TEXT("None");
			case EReflectionMethod::Lumen: return TEXT("Lumen");
			case EReflectionMethod::ScreenSpace: return TEXT("ScreenSpace");
			default: return TEXT("Unknown");
		}
	};

	auto GIMethodToString = [](EDynamicGlobalIlluminationMethod::Type Method) -> FString {
		switch (Method) {
			case EDynamicGlobalIlluminationMethod::None: return TEXT("None");
			case EDynamicGlobalIlluminationMethod::Lumen: return TEXT("Lumen");
			case EDynamicGlobalIlluminationMethod::ScreenSpace: return TEXT("ScreenSpace");
			default: return TEXT("Unknown");
		}
	};

	auto ExposureMethodToString = [](EAutoExposureMethod Method) -> FString {
		switch (Method) {
			case EAutoExposureMethod::AEM_Manual: return TEXT("Manual");
			case EAutoExposureMethod::AEM_Histogram: return TEXT("Histogram");
			default: return TEXT("Unknown");
		}
	};

	auto BloomMethodToString = [](EBloomMethod Method) -> FString {
		switch (Method) {
			case EBloomMethod::BM_FFT: return TEXT("FFT");
			case EBloomMethod::BM_SOG: return TEXT("SOG");
			default: return TEXT("Unknown");
		}
	};

	TMap<FString, FString> CameraSettingsStringMap;
	CameraSettingsStringMap.Add("ReflectionMethod", ReflectionMethodToString(TargetSensor->GetReflectionMethod()));
	CameraSettingsStringMap.Add("GlobalIlluminationMethod", GIMethodToString(TargetSensor->GetGlobalIlluminationMethod()));
	CameraSettingsStringMap.Add("ExposureMethod", ExposureMethodToString(TargetSensor->GetExposureMethod()));
	CameraSettingsStringMap.Add("BloomMethod", BloomMethodToString(BloomMethod));

	TMap<FString, float> CameraSettingsMap;
	CameraSettingsMap.Add("FieldOfView", FOV);
	CameraSettingsMap.Add("ImageWidth", static_cast<float>(Width));
	CameraSettingsMap.Add("ImageHeight", static_cast<float>(Height));
	CameraSettingsMap.Add("AutoExposureSpeedDown", ExposureSpeedDown);
	CameraSettingsMap.Add("AutoExposureSpeedUp", ExposureSpeedUp);
	CameraSettingsMap.Add("MotionBlurAmount", MotionBlurAmount);
	CameraSettingsMap.Add("MotionBlurMax", MotionBlurMax);
	CameraSettingsMap.Add("MotionBlurPerObjectSize", MotionBlurPerObjectSize);
	CameraSettingsMap.Add("MotionBlurTargetFPS", static_cast<float>(MotionBlurTargetFPS));
	CameraSettingsMap.Add("DepthOfFieldFocalDistance", FocalDistance);
	CameraSettingsMap.Add("DepthOfFieldFocalRegion", FocalRegion);
	CameraSettingsMap.Add("ChromaticAberrationIntensity", ChromaticAberration);
	CameraSettingsMap.Add("VignetteIntensity", Vignette);
	CameraSettingsMap.Add("BloomIntensity", BloomIntensity);

	TArray<FJsonObjectBP> OccluderArray;
	for (const FOccluderMetadata& Occluder : SceneHandle.OccluderMetadataList)
	{
		TArray<FString> Keys;
		TArray<FJsonObjectBP> Values;
		for (auto& Pair : Occluder.Metadata)
		{
			Keys.Add(Pair.Key);
			Values.Add(FJsonObjectBP(Pair.Value));
		}
		FJsonObjectBP OccluderJson = USerializeBPLib::TMapToJson(Keys, Values);
		OccluderArray.Add(OccluderJson);
	}

	TMap<FString, FColor> AllAnnotationColors;
	
	FString ForegroundColor = TEXT("NULL");
	// check(IsValid(SceneHandle.ForegroundActor))
	if (IsValid(SceneHandle.ForegroundActor))
	{
		FColor AnnotationColor;
		FObjectAnnotator::GetAnnotationColor(SceneHandle.ForegroundActor, AnnotationColor);
		ForegroundColor = FString::Printf(TEXT("%d,%d,%d"), AnnotationColor.R, AnnotationColor.G, AnnotationColor.B);
	}
	AllAnnotationColors = FObjectAnnotator::GetAnnotationColors();


	TMap<FString, FString> ColorMap;
	for (const TPair<FString, FColor>& KV : AllAnnotationColors)
	{
		FString ColorJson = FString::Printf(TEXT("%d,%d,%d"), KV.Value.R, KV.Value.G, KV.Value.B);
		ColorMap.Add(KV.Key, ColorJson);
	}

	FString RealWorldTimeStartStr = RealWorldTimeRecordingStart.ToString(TEXT("%Y-%m-%d %H:%M:%S"));

	TArray<FString> Keys = {
		"VideoName",
		"Resolution",
		"Width",
		"Height",
		"RecordedFPS",
		"FOV",
		"SceneCategory",
		"ForegroundCategory",
		"ForegroundSubcategory",
		"ForegroundObjectMetadata",
		"OccluderMetaDataList",
		"CameraSettings",
		"CameraSettingsString",
		"ForegroundColor",
		"AnnotationColors",
		"RealWorldTimeRecordingStart",
		"SemanticAnnotations"
	};

	FJsonObjectBP SemanticAnnotationsJson = UMetaDataBPLib::GetSemanticAnnotationsJson(GetWorld());

	TArray<FJsonObjectBP> Values = {
		FJsonObjectBP(RecordFileName),
		FJsonObjectBP(ResolutionStr),
		FJsonObjectBP(Width),
		FJsonObjectBP(Height),
		FJsonObjectBP(RecordFPS),
		FJsonObjectBP(FOV),
		FJsonObjectBP(SceneHandle.SceneCategory),
		FJsonObjectBP(SceneHandle.ForegroundCategory),
		FJsonObjectBP(SceneHandle.ForegroundSubcategory),
		FJsonObjectBP(SceneHandle.ForegroundObjectMetadata),
		FJsonObjectBP(OccluderArray),
		FJsonObjectBP(CameraSettingsMap),
		FJsonObjectBP(CameraSettingsStringMap),
		FJsonObjectBP(ForegroundColor),
		FJsonObjectBP(ColorMap),
		FJsonObjectBP(RealWorldTimeStartStr),
		SemanticAnnotationsJson
	};

	FJsonObjectBP JsonObject = USerializeBPLib::TMapToJson(Keys, Values);
	FString JsonStr = USerializeBPLib::JsonToStr(JsonObject);
	FString JsonFilename = FPaths::Combine(FPaths::ConvertRelativePathToFull(FinalDataFolder, RecordFileName), TEXT("overview.json"));

	AsyncTask(ENamedThreads::AnyThread, [JsonStr, JsonFilename]()
	{
		UVisionBPLib::SaveData(JsonStr, JsonFilename);
	});
}

void AFusionCamCaptureActor::SaveCameraMetadata(bool bWarmUp)
{
	if (!IsValid(TargetSensor))
	{
		return;
	}

	FVector Location = TargetSensor->GetSensorLocation();
	FRotator Rotation = TargetSensor->GetSensorRotation();
	float FOV = TargetSensor->GetSensorFOV();
	int32 Width = TargetSensor->GetFilmWidth();
	int32 Height = TargetSensor->GetFilmHeight();

	float FOVRadians = FMath::DegreesToRadians(FOV);
	FMatrix RotationMatrix = FRotationMatrix::Make(Rotation);

	float AspectRatio = (float)Width / (float)Height;
	float HalfFOV = FOVRadians / 2.0f;
	float fx = Width / (2.0f * FMath::Tan(HalfFOV));
	float fy = Height / (2.0f * FMath::Tan(HalfFOV) * AspectRatio);
	float cx = Width / 2.0f;
	float cy = Height / 2.0f;
	TArray K{
		USerializeBPLib::ArrayToJson({FJsonObjectBP(fx)   , FJsonObjectBP(0.0f) , FJsonObjectBP(cx)}),
		USerializeBPLib::ArrayToJson({FJsonObjectBP(0.0f) , FJsonObjectBP(fy)   , FJsonObjectBP(cy)}),
		USerializeBPLib::ArrayToJson({FJsonObjectBP(0.0f) , FJsonObjectBP(0.0f) , FJsonObjectBP(1.0f)})
	};

	TArray RotationArray = {
		USerializeBPLib::ArrayToJson({static_cast<float>(RotationMatrix.M[0][0]), static_cast<float>(RotationMatrix.M[0][1]), static_cast<float>(RotationMatrix.M[0][2])}),
		USerializeBPLib::ArrayToJson({static_cast<float>(RotationMatrix.M[1][0]), static_cast<float>(RotationMatrix.M[1][1]), static_cast<float>(RotationMatrix.M[1][2])}),
		USerializeBPLib::ArrayToJson({static_cast<float>(RotationMatrix.M[2][0]), static_cast<float>(RotationMatrix.M[2][1]), static_cast<float>(RotationMatrix.M[2][2])})
	};

	auto TranslationArray = USerializeBPLib::VectorToJson({Location.X, Location.Y, Location.Z});

	FMatrix c2w_unreal = FMatrix::Identity;
	c2w_unreal.M[0][0] = RotationMatrix.M[0][0]; c2w_unreal.M[0][1] = RotationMatrix.M[0][1]; c2w_unreal.M[0][2] = RotationMatrix.M[0][2]; c2w_unreal.M[0][3] = Location.X;
	c2w_unreal.M[1][0] = RotationMatrix.M[1][0]; c2w_unreal.M[1][1] = RotationMatrix.M[1][1]; c2w_unreal.M[1][2] = RotationMatrix.M[1][2]; c2w_unreal.M[1][3] = Location.Y;
	c2w_unreal.M[2][0] = RotationMatrix.M[2][0]; c2w_unreal.M[2][1] = RotationMatrix.M[2][1]; c2w_unreal.M[2][2] = RotationMatrix.M[2][2]; c2w_unreal.M[2][3] = Location.Z;

	FMatrix w2c_unreal = c2w_unreal.Inverse();

	FMatrix T_cam_unreal_to_colmap = FMatrix::Identity;
	T_cam_unreal_to_colmap.M[0][0] = 0; T_cam_unreal_to_colmap.M[0][1] = 1;
	T_cam_unreal_to_colmap.M[1][0] = 1; T_cam_unreal_to_colmap.M[1][1] = 0;
	T_cam_unreal_to_colmap.M[2][2] = -1;

	FMatrix T_world_colmap_to_unreal = FMatrix::Identity;
	T_world_colmap_to_unreal.M[1][1] = -1;

	FMatrix w2c_colmap = T_cam_unreal_to_colmap * w2c_unreal * T_world_colmap_to_unreal;

	TArray<FJsonObjectBP> W2CColmapArray;
	for (int32 i = 0; i < 4; ++i) {
		FJsonObjectBP RowJson = USerializeBPLib::ArrayToJson({
			FJsonObjectBP(static_cast<float>(w2c_colmap.M[i][0])),
			FJsonObjectBP(static_cast<float>(w2c_colmap.M[i][1])),
			FJsonObjectBP(static_cast<float>(w2c_colmap.M[i][2]))
		});
		W2CColmapArray.Add(RowJson);
	}

	TArray<FString> ExtrinsicsKeys = {"RotationMatrix", "Translation", "w2c_colmap"};
	TArray<FJsonObjectBP> ExtrinsicsValues;
	ExtrinsicsValues.Add(FJsonObjectBP(RotationArray));
	ExtrinsicsValues.Add(TranslationArray);
	ExtrinsicsValues.Add(USerializeBPLib::ArrayToJson(W2CColmapArray));

	FVector ForegroundLocation = FVector::Zero();
	FRotator ForegroundRotation = FRotator::ZeroRotator;
	if (IsValid(SceneHandle.ForegroundActor))
	{
		ForegroundLocation = SceneHandle.ForegroundActor->GetActorLocation();
		ForegroundRotation = SceneHandle.ForegroundActor->GetActorRotation();
	}

	RealWorldTimeRecordingEnd = FDateTime::Now();
	RealWorldTimeDurationSeconds = (RealWorldTimeRecordingEnd - RealWorldTimeRecordingStart).GetTotalSeconds();
	if (RealWorldTimeDurationSeconds > 0.0)
	{
		RealWorldTimeFPS = ElapsedSteps / RealWorldTimeDurationSeconds;
	}
	else
	{
		RealWorldTimeFPS = 0.0;
	}
	FString ResolutionStr = FString::Printf(TEXT("%dx%d"), Width, Height);
	FString RealWorldTimeStartStr = RealWorldTimeRecordingStart.ToString(TEXT("%Y-%m-%d %H:%M:%S"));
	FString RealWorldTimeEndStr = RealWorldTimeRecordingEnd.ToString(TEXT("%Y-%m-%d %H:%M:%S"));

	TArray<FString> Keys = {
		"FrameNumber",
		"CameraLocation",
		"CameraRotation",
		"ForegroundLocation",
		"ForegroundRotation",
		"IntrinsicsMatrix",
		"Extrinsics",

		"RealWorldTimeRecordingEnd",
		"RealWorldTimeDurationSeconds",
		"RealWorldTimeFPS"
	};

	TArray<FJsonObjectBP> Values = {
		FJsonObjectBP(ElapsedSteps),
		FJsonObjectBP(Location),
		FJsonObjectBP(Rotation),
		FJsonObjectBP(ForegroundLocation),
		FJsonObjectBP(ForegroundRotation),
		K,
		FJsonObjectBP(ExtrinsicsKeys, ExtrinsicsValues),
		
		FJsonObjectBP(RealWorldTimeEndStr),
		FJsonObjectBP(static_cast<float>(RealWorldTimeDurationSeconds)),
		FJsonObjectBP(static_cast<float>(RealWorldTimeFPS))
	};

	FJsonObjectBP JsonObject = USerializeBPLib::TMapToJson(Keys, Values);
	FString JsonStr = USerializeBPLib::JsonToStr(JsonObject);
	FString JsonFilename = MakeFilenameNewWithFolder("metadata", ".json", bWarmUp);

	AsyncTask(ENamedThreads::AnyThread, [JsonStr, JsonFilename]()
	{
		UVisionBPLib::SaveData(JsonStr, JsonFilename);
	});
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////// Neo Trajector Render System /////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void AFusionCamCaptureActor::PrepareTrajectoryRecord(AActor * Target, float FPS)
{
	SetDefaultParamsForTargetCamera();

	UnifiedTargetLocation = GetTargetLocationWithOffset(Target);
	FVector SensorLocation = TargetSensor->GetSensorLocation();
	float Distance = (UnifiedTargetLocation - SensorLocation).Size();
	const float FocalRegion = 1 * 100;
	TargetSensor->SetFocalParams(FMath::Max(Distance - FocalRegion/2, 100.0f), FocalRegion);

	UE_LOG(LogUnrealCV, Log, TEXT("FusionCamCaptureActor: Set all quality settings to maximum for recording (Lumen GI and Reflections enabled)"));

	if (RecordingDataTypes.bRecordWithoutTarget && !IsValid(BackupSensor) && IsValid(TargetSensor))
	{
		BackupCameraID = URecordingBPLib::CreateFreeCamera(
			this,
			TargetSensor->GetSensorLocation(),
			TargetSensor->GetSensorRotation()
		);

		if (BackupCameraID >= 0)
		{
			BackupSensor = URecordingBPLib::GetCameraByID(BackupCameraID);
			if (IsValid(BackupSensor))
			{
				UE_LOG(LogUnrealCV, Log, TEXT("FusionCamCaptureActor: Created BackupSensor via URecordingBPLib, CameraID=%d"), BackupCameraID);
			}
			else
			{
				UE_LOG(LogUnrealCV, Error, TEXT("FusionCamCaptureActor: Failed to get BackupSensor from CameraID=%d"), BackupCameraID);
			}
		}
		else
		{
			UE_LOG(LogUnrealCV, Error, TEXT("FusionCamCaptureActor: Failed to create BackupCamera"));
		}
	}


	if (RecordingDataTypes.bRecordWithoutTarget && IsValid(BackupSensor))
	{
		CopySensorSettings(TargetSensor, BackupSensor);
	}
}

void AFusionCamCaptureActor::StartTrajectoryRecord(const FString& FileName, ECameraTrajectoryType TrajectoryType, AActor* Target, int32 FPS, int32 InNumFrames, int32 RandomSeed, bool bPauseWorldTime)
{
	UE_LOG(LogUnrealCV, Warning, TEXT("[CHECKPOINT] ========== StartTrajectoryRecord START =========="));
	UE_LOG(LogUnrealCV, Warning, TEXT("[CHECKPOINT] StartTrajectoryRecord - FileName: %s, FPS: %d, NumFrames: %d"), *FileName, FPS, InNumFrames);

	if (!IsValid(TargetSensor))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("FusionCamCaptureActor: TargetSensor is not set!"));
		return;
	}

	if (!IsValid(Target))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("FusionCamCaptureActor: Target is null for trajectory recording"));
		return;
	}

	if (bIsRecording)
	{
		StopRecord();
	}

	if (MovieQualityRenderer && MovieQualityRenderer->IsInitialized())
	{
		MovieQualityRenderer->RestoreQualitySettings();
		MovieQualityRenderer->Shutdown();
		MovieQualityRenderer = nullptr;
		UE_LOG(LogUnrealCV, Log, TEXT("FusionCamCaptureActor: MovieQualityRenderer shutdown and quality settings restored"));
	}


	if (!MovieQualityRenderer)
	{
		MovieQualityRenderer = NewObject<UMovieQualityRenderSubsystem>(this);
	}

	if (MovieQualityRenderer && !MovieQualityRenderer->IsInitialized())
	{
		FIntPoint Resolution(TargetSensor->GetFilmWidth(), TargetSensor->GetFilmHeight());
		MovieQualityRenderer->Initialize(GetWorld(), Resolution);
		MovieQualityRenderer->ApplyMovieQualitySettings();
		UE_LOG(LogUnrealCV, Log, TEXT("FusionCamCaptureActor: MovieQualityRenderer initialized at %dx%d"), Resolution.X, Resolution.Y);
	}

	PrepareTrajectoryRecord(Target, FPS);

	bPaused = false;
	RecordFileName = FileName;
	RecordFPS = FPS;
	ElapsedSteps = 0;
	bIsRecording = true;
	TargetForeground = Target;
	bPauseWorldDuringRecord = bPauseWorldTime;
	WarmUpElapsedFrames = 0;

	RealWorldTimeRecordingStart = FDateTime::Now();

	OriginalCameraLocation = TargetSensor->GetSensorLocation();
	OriginalCameraRotation = TargetSensor->GetSensorRotation();

	// Record foreground start position for moving foreground tracking
	ForegroundStartPos = TargetForeground->GetActorLocation();

	UE_LOG(LogUnrealCV, Log, TEXT("FusionCamCaptureActor: Recording foreground start position: (%.2f, %.2f, %.2f)"),
		ForegroundStartPos.X, ForegroundStartPos.Y, ForegroundStartPos.Z);
	CurrentTrajectory = CalculateTrajectory(TrajectoryType, Target, InNumFrames, RandomSeed);


	SaveOverviewMetadata();

#if PLATFORM_WINDOWS
	if (RecordingSettings.bEnableH264Encoding && RecordingDataTypes.bRecordRGB)
	{
		InitializeH264Encoder(FPS);
	}
#endif

	if (RecordingDataTypes.bRecordAudio)
	{
		StartAudioRecord();
	}

	RenderTrajectory(CurrentTrajectory, bPauseWorldTime);
}

void AFusionCamCaptureActor::StartSimpleRecording(const FString& FileName, int32 FPS, float DurationSeconds)
{
	if (!IsValid(TargetSensor))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("StartSimpleRecording: TargetSensor is not set!"));
		return;
	}

	if (FPS <= 0 || DurationSeconds <= 0.0f)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("StartSimpleRecording: Invalid FPS or Duration (FPS=%d, Duration=%.2f)"), FPS, DurationSeconds);
		return;
	}

	if (bIsRecording)
	{
		StopRecord();
	}

	int32 TotalFrames = FMath::CeilToInt(FPS * DurationSeconds);

	TArray<FCameraPose> SimpleTrajectory;
	SimpleTrajectory.Reserve(TotalFrames);

	FVector CurrentLocation = TargetSensor->GetSensorLocation();
	FRotator CurrentRotation = TargetSensor->GetSensorRotation();

	for (int32 i = 0; i < TotalFrames; i++)
	{
		FCameraPose Pose;
		Pose.Location = CurrentLocation;
		Pose.Rotation = CurrentRotation;
		Pose.bManageTransform = false;
		Pose.DesiredEstTimeDilation = 1.0f;
		SimpleTrajectory.Add(Pose);
	}

	bPaused = false;
	RecordFileName = FileName;
	RecordFPS = FPS;
	ElapsedSteps = 0;
	bIsRecording = true;
	TargetForeground = nullptr;
	bPauseWorldDuringRecord = false;
	WarmUpElapsedFrames = 0;
	NumFrames = TotalFrames;

	RealWorldTimeRecordingStart = FDateTime::Now();

	CurrentTrajectory = SimpleTrajectory;

	SaveOverviewMetadata();

	UE_LOG(LogUnrealCV, Log, TEXT("StartSimpleRecording: FileName=%s, FPS=%d, Duration=%.2fs, TotalFrames=%d"),
		*FileName, FPS, DurationSeconds, TotalFrames);

	bool bPauseWorldTime = false;
	RenderTrajectory(CurrentTrajectory, bPauseWorldTime);
}

void AFusionCamCaptureActor::SetDefaultParamsForTargetCamera()
{
	if (!IsValid(TargetSensor))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("FusionCamCaptureActor: TargetSensor is not set!"));
		return;
	}
	TargetSensor->GetLitCamSensor()->ConfigureMaxQualityLumen();
	TargetSensor->SetReflectionMethod(EReflectionMethod::Type::Lumen);
    TargetSensor->SetGlobalIlluminationMethod(EDynamicGlobalIlluminationMethod::Type::Lumen);
	// TargetSensor->SetReflectionMethod(EReflectionMethod::Type::ScreenSpace);
    // TargetSensor->SetGlobalIlluminationMethod(EDynamicGlobalIlluminationMethod::Type::ScreenSpace);

    TargetSensor->SetExposureMethod(EAutoExposureMethod::AEM_Histogram);
    // TargetSensor->SetExposureMethod(EAutoExposureMethod::AEM_Basic);
    // TargetSensor->SetExposureMethod(EAutoExposureMethod::AEM_Manual);
	// TargetSensor->SetAutoExposureSpeed(0.5f, 0.5f); 
	// TargetSensor->SetAutoExposureSpeed(5.f, 8.f); 
	TargetSensor->SetAutoExposureSpeed(20.f, 20.f); 
	// TargetSensor->SetExposureBias(0.0f);
	
	TargetSensor->SetProjectionType(ECameraProjectionMode::Type::Perspective);

	// TargetSensor->SetMotionBlurParams(0.5f, 50.0f, 50.0f, 24.0f);
	TargetSensor->SetMotionBlurParams(0.0f, 0.0f, 0.0f, 0.0f);

	// TargetSensor->SetSensorFOV(FMath::RandRange(40.0f, 55.0f));
	// TargetSensor->SetSensorFOV(55.0f);

	TargetSensor->SetFocalParams(500.0f, 50.0f);

	TargetSensor->SetChromaticAberration(0.5f);

	TargetSensor->SetConvolutionBloom(EBloomMethod::BM_FFT, nullptr, 1.5f);
	// TargetSensor->SetConvolutionBloom(EBloomMethod::BM_SOG, nullptr, 1.5f);

	TargetSensor->SetVignetteIntensity(0.1f);

	// TargetSensor->GetLitCamSensor()->CleanCaptureCache();
	// TargetSensor->GetDepthCamSensor()->CleanCaptureCache();
	// TargetSensor->GetAnnotationCamSensor()->CleanCaptureCache();
	// TargetSensor->GetNormalCamSensor()->CleanCaptureCache();
	// TargetSensor->GetFlowCamSensor()->CleanCaptureCache();
}

void AFusionCamCaptureActor::CopySensorSettings(UFusionCamSensor* Source, UFusionCamSensor* Target)
{
	if (!IsValid(Source) || !IsValid(Target))
	{
		return;
	}

	Target->SetFilmSize(Source->GetFilmWidth(), Source->GetFilmHeight());
	Target->SetSensorFOV(Source->GetSensorFOV());
	Target->SetProjectionType(ECameraProjectionMode::Type::Perspective);

	Target->SetReflectionMethod(Source->GetReflectionMethod());
	Target->SetGlobalIlluminationMethod(Source->GetGlobalIlluminationMethod());
	Target->SetExposureMethod(Source->GetExposureMethod());

	float ExposureSpeedDown, ExposureSpeedUp;
	Source->GetAutoExposureSpeed(ExposureSpeedDown, ExposureSpeedUp);
	Target->SetAutoExposureSpeed(ExposureSpeedDown, ExposureSpeedUp);

	float MotionBlurAmount, MotionBlurMax, MotionBlurPerObjectSize;
	int MotionBlurTargetFPS;
	Source->GetMotionBlurParams(MotionBlurAmount, MotionBlurMax, MotionBlurPerObjectSize, MotionBlurTargetFPS);
	Target->SetMotionBlurParams(MotionBlurAmount, MotionBlurMax, MotionBlurPerObjectSize, MotionBlurTargetFPS);

	float FocalDistance, FocalRegion;
	Source->GetFocalParams(FocalDistance, FocalRegion);
	Target->SetFocalParams(FocalDistance, FocalRegion);

	Target->SetChromaticAberration(Source->GetChromaticAberration());
	Target->SetVignetteIntensity(Source->GetVignetteIntensity());

	EBloomMethod BloomMethod;
	float BloomIntensity;
	Source->GetBloomParams(BloomMethod, BloomIntensity);
	Target->SetConvolutionBloom(BloomMethod, nullptr, BloomIntensity);
}

#if PLATFORM_WINDOWS
void AFusionCamCaptureActor::InitializeH264Encoder(int32 FPS)
{
	MP4OutputPath = FPaths::Combine(FPaths::ConvertRelativePathToFull(FinalDataFolder, RecordFileName), TEXT("rgb.mp4"));

	FUnrealCVMP4EncoderOptions Options;
	Options.OutputFilename = MP4OutputPath;
	Options.Width = TargetSensor->GetFilmWidth();
	Options.Height = TargetSensor->GetFilmHeight();
	Options.FrameRate = FFrameRate(FPS, 1);

	Options.EncodingRateControl = EUnrealCVMP4EncodeRateControlMode::VariableBitRate;
	Options.CommonMeanBitRate = RecordingSettings.VideoEncoder.MeanBitRate;
	Options.CommonMaxBitRate = RecordingSettings.VideoEncoder.MaxBitRate;
	Options.CommonQualityVsSpeed = RecordingSettings.VideoEncoder.QualityVsSpeed;
	Options.EncodingProfile = EUnrealCVMP4EncodeProfile::High;
	Options.EncodingLevel = EUnrealCVMP4EncodeLevel::Auto;

	Options.bIncludeAudio = false;

	MP4Encoder = MakeUnique<FUnrealCVMP4Encoder>(Options);

	if (MP4Encoder->Initialize())
	{
		MP4EncodedFrameCount = 0;
		UE_LOG(LogUnrealCV, Log, TEXT("H.264 Encoder initialized: %s (%dx%d @ %d fps)"),
			*MP4OutputPath, Options.Width, Options.Height, FPS);
	}
	else
	{
		UE_LOG(LogUnrealCV, Error, TEXT("Failed to initialize H.264 encoder"));
		MP4Encoder.Reset();
	}
}
#endif

TArray<AFusionCamCaptureActor::FCameraPose> AFusionCamCaptureActor::CalculateTrajectory(ECameraTrajectoryType TrajectoryType, AActor* Target, int32 InNumFrames, int32 RandomSeed)
{
	switch (TrajectoryType)
	{
	case ECameraTrajectoryType::RotateLeft45:
		return CalculateRotateLeft(Target, InNumFrames, 45.0f, 0.0f);
	case ECameraTrajectoryType::RotateLeft30:
		return CalculateRotateLeft(Target, InNumFrames, 30.0f, 0.0f);
	case ECameraTrajectoryType::RotateRight45:
		return CalculateRotateRight(Target, InNumFrames, -45.0f, 0.0f);
	case ECameraTrajectoryType::RotateRight30:
		return CalculateRotateRight(Target, InNumFrames, -30.0f, 0.0f);
	case ECameraTrajectoryType::RotateUp45:
		return CalculateRotateUp(Target, InNumFrames, 45.0f, 0.0f);
	case ECameraTrajectoryType::RotateUp30:
		return CalculateRotateUp(Target, InNumFrames, 30.0f, 0.0f);
	case ECameraTrajectoryType::Rotate360:
		return CalculateRotate360(Target, InNumFrames, 0.0f);
	case ECameraTrajectoryType::ZoomIn:
		return CalculateZoomIn(Target, InNumFrames, 0.0f);
	case ECameraTrajectoryType::ZoomOut:
		return CalculateZoomOut(Target, InNumFrames, 0.0f);
	case ECameraTrajectoryType::RandomDirection1:
	case ECameraTrajectoryType::RandomDirection2:
	case ECameraTrajectoryType::RandomDirection3:
	case ECameraTrajectoryType::RandomDirection4:
		return CalculateRandomDirection(Target, InNumFrames, RandomSeed, 0.0f);
	case ECameraTrajectoryType::RenderOnly:
	case ECameraTrajectoryType::RenderOnly5S:
		return AddHandheldShake(CalculateRenderOnly(InNumFrames));
	case ECameraTrajectoryType::RenderLeftRotate:
		return AddHandheldShake(CalculateRenderLeftRotate(InNumFrames));
	case ECameraTrajectoryType::RenderRightRotate:
		return AddHandheldShake(CalculateRenderRightRotate(InNumFrames));
	case ECameraTrajectoryType::RenderRotateLeft:
		return AddHandheldShake(CalculateRotateLeft(Target, InNumFrames, 30.0f, 1.0f));
	case ECameraTrajectoryType::RenderRotateRight:
		return AddHandheldShake(CalculateRotateRight(Target, InNumFrames, -30.0f, 1.0f));
	case ECameraTrajectoryType::RenderRotateUp:
		return AddHandheldShake(CalculateRotateUp(Target, InNumFrames, 30.0f, 1.0f));
	default:
		UE_LOG(LogUnrealCV, Error, TEXT("Unknown trajectory type"));
		return TArray<FCameraPose>();
	}
}

void AFusionCamCaptureActor::RenderTrajectory(const TArray<FCameraPose>& Trajectory, bool bPauseWorldTime)
{
	if (Trajectory.Num() == 0)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("RenderTrajectory: Empty trajectory"));
		return;
	}
	TimeDilationBackUp = GetWorld()->GetWorldSettings()->TimeDilation;

	// if (bPauseWorldTime)
	// {
	// 	GetWorld()->GetFirstPlayerController()->SetPause(true);

	// 	ElapsedSteps = 0;
	// 	for (int i = 0; i < Trajectory.Num(); i++)
	// 	{
	// 		if (Trajectory[i].bManageTransform)
	// 		{
	// 			TargetSensor->SetSensorLocation(Trajectory[i].Location);
	// 			TargetSensor->SetSensorRotation(Trajectory[i].Rotation);
	// 		}

	// 		RecordFrame();
	// 		ElapsedSteps++;
	// 	}

	// 	GetWorld()->GetFirstPlayerController()->SetPause(false);
	// 	TargetSensor->SetSensorLocation(OriginalCameraLocation);
	// 	TargetSensor->SetSensorRotation(OriginalCameraRotation);

	// 	if (RecordingDataTypes.bRecordAudio)
	// 	{
	// 		StopAudioRecord();
	// 	}

	// 	TriggerVideoGeneration();

	// 	bIsRecording = false;
	// 	TargetForeground = nullptr;
	// 	CurrentTrajectory.Empty();
	// 	CurrentTrajectoryIndex = 0;
	// }
	// else
	{
		CurrentTrajectoryIndex = 0;
		ElapsedSteps = 0;

		float TimePerFrame = 1.0f / RecordFPS;

		OnTimerRecord();

		GetWorld()->GetTimerManager().SetTimer(
			TimerHandle_Record,
			this,
			&AFusionCamCaptureActor::OnTimerRecord,
			TimePerFrame,
			true
		);
	}
}

// ========== Individual Trajectory Calculation Functions ==========

FVector AFusionCamCaptureActor::GetTargetLocationWithOffset(AActor* Target)
{
	if (!IsValid(Target))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("GetTargetLocationWithOffset: Target actor is invalid"));
		return FVector::ZeroVector;
	}
	FVector TargetLocation = Target->GetActorLocation();
	FBox ActorBounds = Target->GetComponentsBoundingBox();
	if (ActorBounds.IsValid)
	{
		TargetLocation.Z = ActorBounds.Min.Z + (ActorBounds.Max.Z - ActorBounds.Min.Z) * 0.86;
	}
	else
	{
		UE_LOG(LogUnrealCV, Error, TEXT("!ActorBounds.IsValid"));
	}
	return TargetLocation;
}

TArray<AFusionCamCaptureActor::FCameraPose> AFusionCamCaptureActor::AddRotateBufferFrames(const TArray<FCameraPose>& CoreTrajectory)
{
	if (CoreTrajectory.Num() == 0)
	{
		return CoreTrajectory;
	}

	int32 BufferFrames = FMath::RoundToInt(ROTATE_BUFFER_DURATION_SECONDS * RecordFPS);
	if (BufferFrames <= 0)
	{
		return CoreTrajectory;
	}

	TArray<FCameraPose> FullTrajectory;
	FullTrajectory.Reserve(BufferFrames * 2 + CoreTrajectory.Num());

	const FCameraPose& FirstPose = CoreTrajectory[0];
	for (int32 i = 0; i < BufferFrames; i++)
	{
		FCameraPose BufferPose;
		BufferPose.Location = FirstPose.Location;
		BufferPose.Rotation = FirstPose.Rotation;
		BufferPose.bManageTransform = false;
		BufferPose.DesiredEstTimeDilation = 1.0f;
		FullTrajectory.Add(BufferPose);
	}

	FullTrajectory.Append(CoreTrajectory);

	const FCameraPose& LastPose = CoreTrajectory.Last();
	for (int32 i = 0; i < BufferFrames; i++)
	{
		FCameraPose BufferPose;
		BufferPose.Location = LastPose.Location;
		BufferPose.Rotation = LastPose.Rotation;
		BufferPose.bManageTransform = false;
		BufferPose.DesiredEstTimeDilation = 1.0f;
		FullTrajectory.Add(BufferPose);
	}

	NumFrames = FullTrajectory.Num();
	return FullTrajectory;
}

TArray<AFusionCamCaptureActor::FCameraPose> AFusionCamCaptureActor::AddHandheldShake(const TArray<FCameraPose>& InputTrajectory)
{
	if (InputTrajectory.Num() == 0)
	{
		return InputTrajectory;
	}

	const int32 MIN_INTERVAL = 4;
	const int32 MAX_INTERVAL = 40;
	// const float SHAKE_LOCATION_MAGNITUDE_CM = 0.8f;
	// const float SHAKE_ROTATION_MAGNITUDE_DEG = 0.5f;
	const float SHAKE_LOCATION_MAGNITUDE_CM = 0.8f;
	const float SHAKE_ROTATION_MAGNITUDE_DEG = 0.8f;

	int32 NumFramesInput = InputTrajectory.Num();

	TArray<int32> KeyframeIndices;
	TArray<FVector> LocationKeyframes;
	TArray<FRotator> RotationKeyframes;

	KeyframeIndices.Add(0);
	LocationKeyframes.Add(FVector::ZeroVector);
	RotationKeyframes.Add(FRotator::ZeroRotator);

	int32 CurrentFrame = 0;
	while (CurrentFrame < NumFramesInput - 1)
	{
		int32 Interval = FMath::RandRange(MIN_INTERVAL, MAX_INTERVAL);
		CurrentFrame = FMath::Min(CurrentFrame + Interval, NumFramesInput - 1);
		KeyframeIndices.Add(CurrentFrame);

		FVector LocOffset(
			FMath::FRandRange(-SHAKE_LOCATION_MAGNITUDE_CM, SHAKE_LOCATION_MAGNITUDE_CM),
			FMath::FRandRange(-SHAKE_LOCATION_MAGNITUDE_CM, SHAKE_LOCATION_MAGNITUDE_CM),
			FMath::FRandRange(-SHAKE_LOCATION_MAGNITUDE_CM * 0.5f, SHAKE_LOCATION_MAGNITUDE_CM * 0.5f)
		);
		LocationKeyframes.Add(LocOffset);

		FRotator RotOffset(
			FMath::FRandRange(-SHAKE_ROTATION_MAGNITUDE_DEG, SHAKE_ROTATION_MAGNITUDE_DEG),
			FMath::FRandRange(-SHAKE_ROTATION_MAGNITUDE_DEG, SHAKE_ROTATION_MAGNITUDE_DEG),
			FMath::FRandRange(-SHAKE_ROTATION_MAGNITUDE_DEG * 0.6f, SHAKE_ROTATION_MAGNITUDE_DEG * 0.6f)
		);
		RotationKeyframes.Add(RotOffset);
	}

	TArray<FCameraPose> ShakenTrajectory;
	ShakenTrajectory.Reserve(NumFramesInput);

	for (int32 i = 0; i < NumFramesInput; i++)
	{
		const FCameraPose& OriginalPose = InputTrajectory[i];
		FCameraPose ShakenPose = OriginalPose;

		int32 KeyIndex1 = 0;
		for (int32 k = 0; k < KeyframeIndices.Num() - 1; k++)
		{
			if (i >= KeyframeIndices[k] && i <= KeyframeIndices[k + 1])
			{
				KeyIndex1 = k;
				break;
			}
		}

		int32 KeyIndex2 = FMath::Min(KeyIndex1 + 1, KeyframeIndices.Num() - 1);
		int32 Frame1 = KeyframeIndices[KeyIndex1];
		int32 Frame2 = KeyframeIndices[KeyIndex2];

		float Alpha = 0.0f;
		if (Frame2 > Frame1)
		{
			Alpha = float(i - Frame1) / float(Frame2 - Frame1);
		}
		float SmoothedAlpha = Alpha * Alpha * (3.0f - 2.0f * Alpha);

		FVector LocalShake = FMath::Lerp(LocationKeyframes[KeyIndex1], LocationKeyframes[KeyIndex2], SmoothedAlpha);
		FVector WorldShake = OriginalPose.Rotation.RotateVector(LocalShake);
		ShakenPose.Location = OriginalPose.Location + WorldShake;

		FRotator RotShake = FMath::Lerp(RotationKeyframes[KeyIndex1], RotationKeyframes[KeyIndex2], SmoothedAlpha);
		ShakenPose.Rotation = OriginalPose.Rotation + RotShake;

		ShakenPose.bManageTransform = true;

		ShakenTrajectory.Add(ShakenPose);
	}

	return ShakenTrajectory;
}

TArray<AFusionCamCaptureActor::FCameraPose> AFusionCamCaptureActor::CalculateRotateLeft(AActor* Target, int32 InNumFrames, float TotalRotationDeg, float DesiredEstTimeDilation)
{
	TArray<FCameraPose> CoreTrajectory;
	FVector TargetLocation = UnifiedTargetLocation;
	FVector OriginalLocation = TargetSensor->GetSensorLocation();
	FRotator OriginalRotation = TargetSensor->GetSensorRotation();

	FVector Offset = OriginalLocation - TargetLocation;
	FRotator InitRotation = OriginalRotation - (TargetLocation - OriginalLocation).Rotation();

	NumFrames = InNumFrames;
	float DegreesPerFrame = TotalRotationDeg / (NumFrames - 1);

	for (int i = 0; i < NumFrames; i++)
	{
		float CurrentDeg = FMath::Min(DegreesPerFrame * i, TotalRotationDeg);

		FQuat RotationQuat = FQuat(FVector::UpVector, FMath::DegreesToRadians(CurrentDeg));
		FVector NewOffset = RotationQuat.RotateVector(Offset);
		FVector NewLocation = TargetLocation + NewOffset;
		FRotator NewRotation = (TargetLocation - NewLocation).Rotation() + InitRotation;

		FCameraPose Pose;
		Pose.Location = NewLocation;
		Pose.Rotation = NewRotation;
		Pose.DesiredEstTimeDilation = DesiredEstTimeDilation;
		CoreTrajectory.Add(Pose);
	}

	return AddRotateBufferFrames(CoreTrajectory);
}


TArray<AFusionCamCaptureActor::FCameraPose> AFusionCamCaptureActor::CalculateRotateRight(AActor* Target, int32 InNumFrames, float TotalRotationDeg, float DesiredEstTimeDilation)
{
	TArray<FCameraPose> CoreTrajectory;
	FVector TargetLocation = UnifiedTargetLocation;
	FVector OriginalLocation = TargetSensor->GetSensorLocation();
	FRotator OriginalRotation = TargetSensor->GetSensorRotation();

	FVector Offset = OriginalLocation - TargetLocation;
	FRotator InitRotation = OriginalRotation - (TargetLocation - OriginalLocation).Rotation();

	NumFrames = InNumFrames;
	float DegreesPerFrame = FMath::Abs(TotalRotationDeg) / (NumFrames - 1);

	for (int i = 0; i < NumFrames; i++)
	{
		float CurrentDeg = FMath::Max(DegreesPerFrame * i * -1.0, TotalRotationDeg);

		FQuat RotationQuat = FQuat(FVector::UpVector, FMath::DegreesToRadians(CurrentDeg));
		FVector NewOffset = RotationQuat.RotateVector(Offset);
		FVector NewLocation = TargetLocation + NewOffset;
		FRotator NewRotation = (TargetLocation - NewLocation).Rotation() + InitRotation;

		FCameraPose Pose;
		Pose.Location = NewLocation;
		Pose.Rotation = NewRotation;
		Pose.DesiredEstTimeDilation = DesiredEstTimeDilation;
		CoreTrajectory.Add(Pose);
	}

	return AddRotateBufferFrames(CoreTrajectory);
}

TArray<AFusionCamCaptureActor::FCameraPose> AFusionCamCaptureActor::CalculateRotateUp(AActor* Target, int32 InNumFrames, float TotalRotationDeg, float DesiredEstTimeDilation)
{
	TArray<FCameraPose> CoreTrajectory;
	FVector TargetLocation = UnifiedTargetLocation;
	FVector OriginalLocation = TargetSensor->GetSensorLocation();
	FRotator OriginalRotation = TargetSensor->GetSensorRotation();

	FVector Offset = OriginalLocation - TargetLocation;
	FRotator InitRotation = OriginalRotation - (TargetLocation - OriginalLocation).Rotation();

	FVector ToCamera = Offset.GetSafeNormal();
	FVector RightVector = FVector::CrossProduct(ToCamera, FVector::UpVector).GetSafeNormal();

	NumFrames = InNumFrames;
	float DegreesPerFrame = TotalRotationDeg / (NumFrames - 1);

	for (int i = 0; i < NumFrames; i++)
	{
		float CurrentDeg = FMath::Min(DegreesPerFrame * i, TotalRotationDeg);

		FQuat RotationQuat = FQuat(RightVector, FMath::DegreesToRadians(CurrentDeg));
		FVector NewOffset = RotationQuat.RotateVector(Offset);
		FVector NewLocation = TargetLocation + NewOffset;
		FRotator NewRotation = (TargetLocation - NewLocation).Rotation() + InitRotation;

		FCameraPose Pose;
		Pose.Location = NewLocation;
		Pose.Rotation = NewRotation;
		Pose.DesiredEstTimeDilation = DesiredEstTimeDilation;
		CoreTrajectory.Add(Pose);
	}

	return AddRotateBufferFrames(CoreTrajectory);
}

TArray<AFusionCamCaptureActor::FCameraPose> AFusionCamCaptureActor::CalculateRotate360(AActor* Target, int32 InNumFrames, float DesiredEstTimeDilation)
{
	TArray<FCameraPose> CoreTrajectory;
	FVector TargetLocation = UnifiedTargetLocation;
	FVector OriginalLocation = TargetSensor->GetSensorLocation();
	FRotator OriginalRotation = TargetSensor->GetSensorRotation();

	FVector Offset = OriginalLocation - TargetLocation;
	FRotator InitRotation = OriginalRotation - (TargetLocation - OriginalLocation).Rotation();

	float TotalRotationDeg = 360.0f;
	NumFrames = InNumFrames;
	float DegreesPerFrame = TotalRotationDeg / (NumFrames - 1);

	for (int i = 0; i < NumFrames; i++)
	{
		float CurrentDeg = FMath::Min(DegreesPerFrame * i, TotalRotationDeg);

		FQuat RotationQuat = FQuat(FVector::UpVector, FMath::DegreesToRadians(CurrentDeg));
		FVector NewOffset = RotationQuat.RotateVector(Offset);
		FVector NewLocation = TargetLocation + NewOffset;
		FRotator NewRotation = (TargetLocation - NewLocation).Rotation() + InitRotation;

		FCameraPose Pose;
		Pose.Location = NewLocation;
		Pose.Rotation = NewRotation;
		Pose.DesiredEstTimeDilation = DesiredEstTimeDilation;
		Pose.PausedTickIntervalCoef = 10.0f;
		CoreTrajectory.Add(Pose);
	}

	return AddRotateBufferFrames(CoreTrajectory);
}

TArray<AFusionCamCaptureActor::FCameraPose> AFusionCamCaptureActor::CalculateZoomIn(AActor* Target, int32 InNumFrames, float DesiredEstTimeDilation)
{
	TArray<FCameraPose> Trajectory;
	FVector TargetLocation = UnifiedTargetLocation;
	FVector OriginalLocation = TargetSensor->GetSensorLocation();
	FRotator OriginalRotation = TargetSensor->GetSensorRotation();

	FVector Offset = OriginalLocation - TargetLocation;
	float OriginalDistance = Offset.Size();
	float MinDistance = OriginalDistance * 0.5f;

	NumFrames = InNumFrames;

	for (int i = 0; i < NumFrames; i++)
	{
		float Alpha = static_cast<float>(i) / (NumFrames - 1);
		float CurrentDistance = FMath::Lerp(OriginalDistance, MinDistance, Alpha);

		FVector NewOffset = Offset.GetSafeNormal() * CurrentDistance;
		FVector NewLocation = TargetLocation + NewOffset;

		FCameraPose Pose;
		Pose.Location = NewLocation;
		Pose.Rotation = OriginalRotation;
		Pose.DesiredEstTimeDilation = DesiredEstTimeDilation;
		Trajectory.Add(Pose);
	}

	return AddRotateBufferFrames(Trajectory);
}

TArray<AFusionCamCaptureActor::FCameraPose> AFusionCamCaptureActor::CalculateZoomOut(AActor* Target, int32 InNumFrames, float DesiredEstTimeDilation)
{
	TArray<FCameraPose> Trajectory;
	FVector TargetLocation = UnifiedTargetLocation;
	FVector OriginalLocation = TargetSensor->GetSensorLocation();
	FRotator OriginalRotation = TargetSensor->GetSensorRotation();

	FVector Offset = OriginalLocation - TargetLocation;
	float OriginalDistance = Offset.Size();
	float MaxDistance = OriginalDistance * 1.4f;

	NumFrames = InNumFrames;

	for (int i = 0; i < NumFrames; i++)
	{
		float Alpha = static_cast<float>(i) / (NumFrames - 1);
		float CurrentDistance = FMath::Lerp(OriginalDistance, MaxDistance, Alpha);

		FVector NewOffset = Offset.GetSafeNormal() * CurrentDistance;
		FVector NewLocation = TargetLocation + NewOffset;

		FCameraPose Pose;
		Pose.Location = NewLocation;
		Pose.Rotation = OriginalRotation;
		Pose.DesiredEstTimeDilation = DesiredEstTimeDilation;
		Trajectory.Add(Pose);
	}

	return AddRotateBufferFrames(Trajectory);
}

TArray<AFusionCamCaptureActor::FCameraPose> AFusionCamCaptureActor::CalculateRandomDirection(AActor* Target, int32 InNumFrames, int32 RandomSeed, float DesiredEstTimeDilation)
{
	TArray<FCameraPose> Trajectory;
	FVector TargetLocation = UnifiedTargetLocation;
	FVector OriginalLocation = TargetSensor->GetSensorLocation();
	FRotator OriginalRotation = TargetSensor->GetSensorRotation();

	FRandomStream RandomStream;
	if (RandomSeed >= 0)
	{
		RandomStream.Initialize(RandomSeed);
	}
	else
	{
		RandomStream.Initialize(FMath::Rand());
	}

	FVector Offset = OriginalLocation - TargetLocation;
	FRotator InitRotation = OriginalRotation - (TargetLocation - OriginalLocation).Rotation();

	float VerticalTiltStrength = RandomStream.FRandRange(-0.08f, 0.4f);
	int HoriParam = RandomStream.FRandRange(-1, 1) > 0 ? 1 : -1;

	float TotalRotationDeg = RandomStream.FRandRange(15.0f, 45.0f);
	NumFrames = InNumFrames;
	float DegreesPerFrame = TotalRotationDeg / (NumFrames - 1);

	bool bVaryDistance = false;
	float OriginalDistance = Offset.Size();
	float DistanceVariation = RandomStream.FRandRange(0.7f, 1.3f);

	for (int i = 0; i < NumFrames; i++)
	{
		float Alpha = static_cast<float>(i) / (NumFrames - 1);
		float CurrentDeg = FMath::Min(DegreesPerFrame * i, TotalRotationDeg);

		FVector ToCamera = Offset.GetSafeNormal();
		FVector RightVector = FVector::CrossProduct(ToCamera, FVector::UpVector).GetSafeNormal();
		FQuat RotationQuatHori = FQuat(FVector::UpVector, FMath::DegreesToRadians(CurrentDeg * HoriParam));
		float PitchDeg = CurrentDeg * VerticalTiltStrength;
		FQuat RotationQuatUp = FQuat(RightVector, FMath::DegreesToRadians(PitchDeg));
		FQuat FinalQuat = RotationQuatUp * RotationQuatHori;
		FVector NewOffset = FinalQuat.RotateVector(Offset);

		if (bVaryDistance)
		{
			float CurrentDistance = FMath::Lerp(OriginalDistance, OriginalDistance * DistanceVariation, Alpha);
			NewOffset = NewOffset.GetSafeNormal() * CurrentDistance;
		}

		FVector NewLocation = TargetLocation + NewOffset;
		FRotator NewRotation = (TargetLocation - NewLocation).Rotation() + InitRotation;

		FCameraPose Pose;
		Pose.Location = NewLocation;
		Pose.Rotation = NewRotation;
		Pose.DesiredEstTimeDilation = DesiredEstTimeDilation;
		Trajectory.Add(Pose);
	}

	return AddRotateBufferFrames(Trajectory);
}

TArray<AFusionCamCaptureActor::FCameraPose> AFusionCamCaptureActor::CalculateRenderOnly(int32 InNumFrames)
{
	TArray<FCameraPose> Trajectory;
	NumFrames = InNumFrames;

	FVector SensorLocation = TargetSensor->GetSensorLocation();
	FRotator SensorRotation = TargetSensor->GetSensorRotation();
	for (int i = 0; i < NumFrames; i++)
	{
		FCameraPose Pose;
		// Pose.Location = OriginalCameraLocation;
		// Pose.Rotation = OriginalCameraRotation;
		Pose.Location = SensorLocation;
		Pose.Rotation = SensorRotation;
		Pose.bManageTransform = false;
		Trajectory.Add(Pose);
	}

	return Trajectory;
}

TArray<AFusionCamCaptureActor::FCameraPose> AFusionCamCaptureActor::CalculateRenderLeftRotate(int32 InNumFrames)
{
	TArray<FCameraPose> Trajectory;
	NumFrames = InNumFrames;

	FVector SensorLocation = TargetSensor->GetSensorLocation();
	FRotator SensorRotation = TargetSensor->GetSensorRotation();
	float FOV = TargetSensor->GetSensorFOV();

	float StartYaw = -FOV / 2.0f;
	float EndYaw = FOV / 2.0f;

	for (int i = 0; i < NumFrames; i++)
	{
		float Alpha = static_cast<float>(i) / FMath::Max(NumFrames - 1, 1);
		float CurrentYaw = FMath::Lerp(StartYaw, EndYaw, Alpha);

		FRotator NewRotation = SensorRotation;
		NewRotation.Yaw += CurrentYaw;

		FCameraPose Pose;
		Pose.Location = SensorLocation;
		Pose.Rotation = NewRotation;
		Pose.bManageTransform = true;
		Pose.DesiredEstTimeDilation = 1.0f;
		Trajectory.Add(Pose);
	}

	return Trajectory;
}

TArray<AFusionCamCaptureActor::FCameraPose> AFusionCamCaptureActor::CalculateRenderRightRotate(int32 InNumFrames)
{
	TArray<FCameraPose> Trajectory;
	NumFrames = InNumFrames;

	FVector SensorLocation = TargetSensor->GetSensorLocation();
	FRotator SensorRotation = TargetSensor->GetSensorRotation();
	float FOV = TargetSensor->GetSensorFOV();

	float StartYaw = FOV / 2.0f;
	float EndYaw = -FOV / 2.0f;

	for (int i = 0; i < NumFrames; i++)
	{
		float Alpha = static_cast<float>(i) / FMath::Max(NumFrames - 1, 1);
		float CurrentYaw = FMath::Lerp(StartYaw, EndYaw, Alpha);

		FRotator NewRotation = SensorRotation;
		NewRotation.Yaw += CurrentYaw;

		FCameraPose Pose;
		Pose.Location = SensorLocation;
		Pose.Rotation = NewRotation;
		Pose.bManageTransform = true;
		Pose.DesiredEstTimeDilation = 1.0f;
		Trajectory.Add(Pose);
	}

	return Trajectory;
}


void AFusionCamCaptureActor::TriggerVideoGeneration()
{
	if (!RecordingSettings.bAutoGenerateVideo)
	{
		UE_LOG(LogUnrealCV, Display, TEXT("FusionCamCaptureActor: Auto video generation disabled"));
		return;
	}

	if (RecordingSettings.VideoGenScriptPath.IsEmpty())
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("FusionCamCaptureActor: RecordingSettings.VideoGenScriptPath not set, searching for genvid.py"));

		FString AutoScriptPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("genvid.py"));

		if (FPaths::FileExists(AutoScriptPath))
		{
			RecordingSettings.VideoGenScriptPath = AutoScriptPath;
			UE_LOG(LogUnrealCV, Display, TEXT("FusionCamCaptureActor: Found genvid.py at %s"), *RecordingSettings.VideoGenScriptPath);
		}
		else
		{
			UE_LOG(LogUnrealCV, Error, TEXT("FusionCamCaptureActor: Cannot find genvid.py at %s"), *AutoScriptPath);
			return;
		}
	}

	FString InputDir = FPaths::ConvertRelativePathToFull(FinalDataFolder, RecordFileName);
	int32 ProcessID = 0;

	bool bSuccess = FPythonExecutor::ExecuteGenvidScript(
		RecordingSettings.VideoGenScriptPath,
		InputDir,
		RecordFPS,
		TEXT(""),
		&ProcessID
	);

	if (bSuccess)
	{
		UE_LOG(LogUnrealCV, Display, TEXT("FusionCamCaptureActor: Video generation started (PID: %d) for folder: %s"), ProcessID, *InputDir);
	}
	else
	{
		UE_LOG(LogUnrealCV, Error, TEXT("FusionCamCaptureActor: Failed to start video generation for folder: %s"), *InputDir);
	}
}
