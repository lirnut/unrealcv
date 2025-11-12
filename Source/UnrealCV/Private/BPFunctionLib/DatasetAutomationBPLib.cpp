// Copyright 2025 UnrealCV Team. All Rights Reserved.
#include "DatasetAutomationBPLib.h"
#include "SceneCompositionBPLib.h"
#include "RecordingBPLib.h"
#include "SensorBPLib.h"
#include "FusionCameraActor.h"
#include "NavAgentController.h"
#include "UnrealcvLog.h"
#include "Engine/World.h"

FAutomationConfig UDatasetAutomationBPLib::CurrentConfig;
FAutomationStatus UDatasetAutomationBPLib::CurrentStatus;
FSceneHandle UDatasetAutomationBPLib::CurrentScene;
UWorld* UDatasetAutomationBPLib::WorldContext = nullptr;
FTimerHandle UDatasetAutomationBPLib::AutomationTimerHandle;

TArray<FAutomationStep> UDatasetAutomationBPLib::CommandQueue;
int32 UDatasetAutomationBPLib::CurrentCommandIndex = 0;
int32 UDatasetAutomationBPLib::CurrentSceneCounter = 0;
FString UDatasetAutomationBPLib::CurrentSceneID = TEXT("");
float UDatasetAutomationBPLib::DelayTimer = 0.0f;
float UDatasetAutomationBPLib::DelayDuration = 0.0f;

void UDatasetAutomationBPLib::BuildCommandSequenceForScene()
{
	CommandQueue.Empty();

	CommandQueue.Add(FAutomationStep(TEXT("create_scene")));

	// CommandQueue.Add(FAutomationStep(TEXT("record_trajectory"), TEXT("rotate_left_45")));
	// CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT(""), 2.0f));
	// CommandQueue.Add(FAutomationStep(TEXT("record_trajectory"), TEXT("rotate_right_45")));
	// CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT(""), 2.0f));
	// CommandQueue.Add(FAutomationStep(TEXT("record_trajectory"), TEXT("rotate_up_45")));
	// CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT(""), 2.0f));
	// CommandQueue.Add(FAutomationStep(TEXT("record_trajectory"), TEXT("rotate_360")));
	// CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT(""), 2.0f));
	// CommandQueue.Add(FAutomationStep(TEXT("record_trajectory"), TEXT("zoom_in")));
	// CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT(""), 2.0f));
	// CommandQueue.Add(FAutomationStep(TEXT("record_trajectory"), TEXT("zoom_out")));
	// CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT(""), 2.0f));

	// CommandQueue.Add(FAutomationStep(TEXT("record_trajectory"), TEXT("random_1")));
	// CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT(""), 2.0f));
	// CommandQueue.Add(FAutomationStep(TEXT("record_trajectory"), TEXT("random_2")));
	// CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT(""), 2.0f));
	// CommandQueue.Add(FAutomationStep(TEXT("record_trajectory"), TEXT("random_3")));
	// CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT(""), 2.0f));
	// CommandQueue.Add(FAutomationStep(TEXT("record_trajectory"), TEXT("random_4")));
	// CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT(""), 2.0f));

	CommandQueue.Add(FAutomationStep(TEXT("record_trajectory"), TEXT("render_only")));
	CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT(""), 2.0f));

	CommandQueue.Add(FAutomationStep(TEXT("record_nav_track")));
	CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT(""), 2.0f));

	CommandQueue.Add(FAutomationStep(TEXT("clear_scene")));

	CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT(""), 2.0f));

	CommandQueue.Add(FAutomationStep(TEXT("increment_counter")));

	CommandQueue.Add(FAutomationStep(TEXT("check_completion")));

	UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Built command sequence with %d commands"), CommandQueue.Num());
}

FString UDatasetAutomationBPLib::GenerateSceneID(int32 SceneIndex)
{
	FGuid UUID = FGuid::NewGuid();
	FString UUIDShort = UUID.ToString().Left(8);
	return FString::Printf(TEXT("scene_%04d_%s"), SceneIndex, *UUIDShort);
}

FString UDatasetAutomationBPLib::GenerateOutputPath(const FString& SceneID, const FString& TrajectoryType)
{
	return FString::Printf(TEXT("%s/%s/%s"),
		*CurrentConfig.OutputDirectory,
		*SceneID,
		*TrajectoryType);
}

void UDatasetAutomationBPLib::ExecuteNextCommand()
{
	if (CurrentCommandIndex >= CommandQueue.Num())
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("DatasetAutomation: Command index out of bounds"));
		return;
	}

	const FAutomationStep& Step = CommandQueue[CurrentCommandIndex];
	UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Executing command %d/%d: %s"),
		CurrentCommandIndex + 1, CommandQueue.Num(), *Step.Command);

	CurrentCommandIndex++;
	ExecuteCommand(Step);
}

void UDatasetAutomationBPLib::ExecuteCommand(const FAutomationStep& Step)
{
	TransitionToState(EDatasetGenerationState::ExecutingCommand);

	if (Step.Command == TEXT("create_scene"))
	{
		CurrentSceneID = GenerateSceneID(CurrentSceneCounter);

		bool Success = USceneCompositionBPLib::GenerateRandomScene(
			WorldContext,
			CurrentConfig.SpawnAreaMin,
			CurrentConfig.SpawnAreaMax,
			CurrentConfig.ForegroundCategory,
			CurrentConfig.OccluderCategory,
			CurrentConfig.OccluderCount,
			CurrentConfig.CameraID,
			CurrentScene,
			true
		);

		if (Success)
		{
			UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Created scene: %s"), *CurrentSceneID);
			ExecuteNextCommand();
		}
		else
		{
			CurrentStatus.ErrorMessage = FString::Printf(TEXT("Failed to create scene %d"), CurrentSceneCounter);
			TransitionToState(EDatasetGenerationState::Error);
			UE_LOG(LogUnrealCV, Error, TEXT("DatasetAutomation: %s"), *CurrentStatus.ErrorMessage);
		}
	}
	else if (Step.Command == TEXT("record_trajectory"))
	{
		FString TrajectoryType = Step.StringParam;
		FString OutputPath = GenerateOutputPath(CurrentSceneID, TrajectoryType);

		bool RecordingStarted = StartTrajectoryRecording(
			OutputPath,
			TrajectoryType
		);

		if (RecordingStarted)
		{
			UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Recording trajectory: %s -> %s"), *TrajectoryType, *OutputPath);
			CurrentStatus.CurrentFileName = OutputPath;
			TransitionToState(EDatasetGenerationState::WaitingAsync);
		}
		else
		{
			CurrentStatus.ErrorMessage = FString::Printf(TEXT("Failed to start recording: %s"), *TrajectoryType);
			TransitionToState(EDatasetGenerationState::Error);
			UE_LOG(LogUnrealCV, Error, TEXT("DatasetAutomation: %s"), *CurrentStatus.ErrorMessage);
		}
	}
	else if (Step.Command == TEXT("record_nav_track"))
	{
		if (!CurrentScene.bHasNavigation || !IsValid(CurrentScene.NavController))
		{
			UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Scene has no navigation (not Blueprint), skipping nav-track"));
			ExecuteNextCommand();
			return;
		}

		AFusionCameraActor* CameraActor = GetFusionCameraActor(CurrentConfig.CameraID);
		if (!IsValid(CameraActor))
		{
			UE_LOG(LogUnrealCV, Warning, TEXT("DatasetAutomation: Camera is not FusionCameraActor, skipping nav-track"));
			ExecuteNextCommand();
			return;
		}

		float TrackingDistance = FMath::RandRange(300.0f, 600.0f);
		float TrackingAngleOffset = FMath::RandRange(-45.0f, 45.0f);
		float TrackingGain = FMath::RandRange(0.1f, 0.2f);

		CurrentScene.NavController->StartAutonomousNav();

		CameraActor->StartTracking(
			CurrentScene.ForegroundActor,
			TrackingDistance,
			TrackingAngleOffset,
			TrackingGain
		);

		FString OutputPath = GenerateOutputPath(CurrentSceneID, TEXT("nav_track"));
		bool RecordingStarted = StartTrajectoryRecording(
			OutputPath,
			TEXT("render_only")
		);

		if (RecordingStarted)
		{
			UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Nav-Track recording (D:%.1f A:%.1f G:%.2f) -> %s"),
				TrackingDistance, TrackingAngleOffset, TrackingGain, *OutputPath);
			CurrentStatus.CurrentFileName = OutputPath;
			TransitionToState(EDatasetGenerationState::WaitingAsync);
		}
		else
		{
			CurrentScene.NavController->StopNavigation();
			CameraActor->StopTracking();

			CurrentStatus.ErrorMessage = TEXT("Failed to start Nav-Track recording");
			TransitionToState(EDatasetGenerationState::Error);
		}
	}
	else if (Step.Command == TEXT("clear_scene"))
	{
		USceneCompositionBPLib::ClearScene(CurrentScene);
		CurrentScene = FSceneHandle();
		UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Cleared scene: %s"), *CurrentSceneID);
		ExecuteNextCommand();
	}
	else if (Step.Command == TEXT("delay"))
	{
		DelayTimer = 0.0f;
		DelayDuration = Step.FloatParam;
		UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Delaying %.1f seconds"), DelayDuration);
		TransitionToState(EDatasetGenerationState::WaitingAsync);
	}
	else if (Step.Command == TEXT("increment_counter"))
	{
		CurrentSceneCounter++;
		CurrentStatus.CurrentSceneIndex = CurrentSceneCounter;
		CurrentStatus.Progress = (float)CurrentSceneCounter / (float)CurrentStatus.TotalScenes;
		UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Scene counter: %d/%d (%.1f%%)"),
			CurrentSceneCounter, CurrentStatus.TotalScenes, CurrentStatus.Progress * 100.0f);
		ExecuteNextCommand();
	}
	else if (Step.Command == TEXT("check_completion"))
	{
		if (CurrentSceneCounter >= CurrentStatus.TotalScenes)
		{
			TransitionToState(EDatasetGenerationState::Completed);
			UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Completed all %d scenes"), CurrentStatus.TotalScenes);
		}
		else
		{
			CurrentCommandIndex = 0;
			ExecuteNextCommand();
		}
	}
	else
	{
		CurrentStatus.ErrorMessage = FString::Printf(TEXT("Unknown command: %s"), *Step.Command);
		TransitionToState(EDatasetGenerationState::Error);
		UE_LOG(LogUnrealCV, Error, TEXT("DatasetAutomation: %s"), *CurrentStatus.ErrorMessage);
	}
}


bool UDatasetAutomationBPLib::StartBatchGeneration(
	UObject* WorldContextObject,
	const FAutomationConfig& Config)
{
	if (CurrentStatus.State != EDatasetGenerationState::Idle)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("DatasetAutomation: Already running, stop current batch first"));
		return false;
	}

	WorldContext = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!WorldContext)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("DatasetAutomation: Invalid world context"));
		return false;
	}

	CurrentConfig = Config;
	CurrentStatus = FAutomationStatus();
	CurrentStatus.TotalScenes = Config.TotalScenes;
	CurrentStatus.CurrentSceneIndex = 0;

	BuildCommandSequenceForScene();
	CurrentCommandIndex = 0;
	CurrentSceneCounter = 0;

	WorldContext->GetTimerManager().SetTimer(
		AutomationTimerHandle,
		FTimerDelegate::CreateStatic(&UDatasetAutomationBPLib::AutoTick),
		0.5f,
		true
	);

	UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Started batch generation (%d scenes)"), Config.TotalScenes);

	ExecuteNextCommand();

	return true;
}

void UDatasetAutomationBPLib::StopBatchGeneration()
{
	if (CurrentStatus.State == EDatasetGenerationState::Idle)
	{
		return;
	}

	if (WorldContext && AutomationTimerHandle.IsValid())
	{
		WorldContext->GetTimerManager().ClearTimer(AutomationTimerHandle);
	}

	// This can cause duplicate scene destruction
	// USceneCompositionBPLib::ClearScene(CurrentScene);

	if (CurrentStatus.State == EDatasetGenerationState::WaitingAsync)
	{
		URecordingBPLib::StopRecording(CurrentConfig.CameraID);
	}

	TransitionToState(EDatasetGenerationState::Idle);
	UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Stopped at scene %d/%d"),
		CurrentStatus.CurrentSceneIndex, CurrentStatus.TotalScenes);
}

FAutomationStatus UDatasetAutomationBPLib::GetAutomationStatus()
{
	return CurrentStatus;
}

FString UDatasetAutomationBPLib::GetAutomationStatusString()
{
	FString StateString;
	switch (CurrentStatus.State)
	{
	case EDatasetGenerationState::Idle:
		StateString = TEXT("Idle");
		break;
	case EDatasetGenerationState::ExecutingCommand:
		StateString = TEXT("Executing Command");
		break;
	case EDatasetGenerationState::WaitingAsync:
		StateString = TEXT("Waiting");
		break;
	case EDatasetGenerationState::Completed:
		StateString = TEXT("Completed");
		break;
	case EDatasetGenerationState::Error:
		StateString = TEXT("Error");
		break;
	default:
		StateString = TEXT("Unknown");
		break;
	}

	if (CurrentStatus.State == EDatasetGenerationState::Error)
	{
		return FString::Printf(TEXT("[%s] %s"),
			*StateString, *CurrentStatus.ErrorMessage);
	}
	else if (CurrentStatus.State == EDatasetGenerationState::Completed)
	{
		return FString::Printf(TEXT("[%s] %d/%d scenes (100%%)"),
			*StateString, CurrentStatus.TotalScenes, CurrentStatus.TotalScenes);
	}
	else if (CurrentStatus.State == EDatasetGenerationState::Idle)
	{
		return FString::Printf(TEXT("[%s]"), *StateString);
	}
	else
	{
		return FString::Printf(TEXT("[%s] Scene %d/%d (%.1f%%) - %s"),
			*StateString,
			CurrentStatus.CurrentSceneIndex + 1,
			CurrentStatus.TotalScenes,
			CurrentStatus.Progress * 100.0f,
			*CurrentStatus.CurrentFileName);
	}
}

bool UDatasetAutomationBPLib::IsRunning()
{
	return CurrentStatus.State != EDatasetGenerationState::Idle &&
		   CurrentStatus.State != EDatasetGenerationState::Completed &&
		   CurrentStatus.State != EDatasetGenerationState::Error;
}

void UDatasetAutomationBPLib::TickAutomation(UObject* WorldContextObject, float DeltaTime)
{
	if (!IsRunning())
	{
		return;
	}

	ProcessState(DeltaTime);
}

void UDatasetAutomationBPLib::AutoTick()
{
	if (!IsRunning())
	{
		if (WorldContext && AutomationTimerHandle.IsValid())
		{
			WorldContext->GetTimerManager().ClearTimer(AutomationTimerHandle);
		}
		return;
	}

	ProcessState(0.5f);

	if (CurrentStatus.State == EDatasetGenerationState::Completed ||
		CurrentStatus.State == EDatasetGenerationState::Error)
	{
		if (WorldContext && AutomationTimerHandle.IsValid())
		{
			WorldContext->GetTimerManager().ClearTimer(AutomationTimerHandle);
		}
	}
}

void UDatasetAutomationBPLib::TransitionToState(EDatasetGenerationState NewState)
{
	CurrentStatus.State = NewState;
}

void UDatasetAutomationBPLib::ProcessState(float DeltaTime)
{
	switch (CurrentStatus.State)
	{
	case EDatasetGenerationState::ExecutingCommand:
		break;

	case EDatasetGenerationState::WaitingAsync:
	{
		bool RecordingComplete = !URecordingBPLib::IsRecording(CurrentConfig.CameraID);
		bool DelayComplete = (DelayTimer >= DelayDuration);

		if (RecordingComplete && DelayComplete)
		{
			if (IsValid(CurrentScene.NavController) && CurrentScene.NavController->IsNavigating())
			{
				CurrentScene.NavController->StopNavigation();
			}

			AFusionCameraActor* CameraActor = GetFusionCameraActor(CurrentConfig.CameraID);
			if (IsValid(CameraActor))
			{
				CameraActor->StopTracking();
			}

			ExecuteNextCommand();
		}
		else
		{
			DelayTimer += DeltaTime;
		}
		break;
	}

	case EDatasetGenerationState::Error:
	case EDatasetGenerationState::Completed:
	case EDatasetGenerationState::Idle:
	default:
		break;
	}
}

AFusionCameraActor* UDatasetAutomationBPLib::GetFusionCameraActor(int32 CameraID)
{
	UFusionCamSensor* Sensor = USensorBPLib::GetSensorById(CameraID);
	if (!IsValid(Sensor))
	{
		return nullptr;
	}

	AActor* Owner = Sensor->GetOwner();
	return Cast<AFusionCameraActor>(Owner);
}


bool UDatasetAutomationBPLib::StartTrajectoryRecording(
	const FString& FileName,
	const FString& TrajectoryType)
{

	int32 CameraID = CurrentConfig.CameraID;
	AActor* Target = CurrentScene.ForegroundActor;
	int32 FPS = CurrentConfig.TrajectoryFPS;
	float DegreesPerSecond = CurrentConfig.TrajectoryDegreesPerSecond;
	int32 RandomSeed = -1;
	// Validate target
	if (!IsValid(Target))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("StartTrajectoryRecording: Target actor is null"));
		return false;
	}

	// Parse trajectory type
	ECameraTrajectoryType TrajectoryEnum;
	if (!URecordingBPLib::ParseTrajectoryType(TrajectoryType, TrajectoryEnum))
	{
		return false;
	}

	// Prepare recording (reuse existing function)
	AFusionCamCaptureActor* CaptureActor = URecordingBPLib::PrepareRecording(CameraID);
	if (!IsValid(CaptureActor))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("StartTrajectoryRecording: Failed to prepare recording for camera %d"), CameraID);
		return false;
	}
	CaptureActor->SetSceneHandle(CurrentScene);

	bool PauseWorldTime = false;

	// Start trajectory recording
	UE_LOG(LogUnrealCV, Log, TEXT("StartTrajectoryRecording: Camera %d, File: %s, Type: %s, FPS: %d, Deg/s: %.2f, Target: %s"),
		CameraID, *FileName, *TrajectoryType, FPS, DegreesPerSecond, *Target->GetName());

	CaptureActor->StartTrajectoryRecord(FileName, TrajectoryEnum, Target, FPS, DegreesPerSecond, RandomSeed, PauseWorldTime);

	return true;
}