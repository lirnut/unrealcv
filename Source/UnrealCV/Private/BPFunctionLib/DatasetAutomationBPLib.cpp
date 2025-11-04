// Copyright 2025 UnrealCV Team. All Rights Reserved.
#include "DatasetAutomationBPLib.h"
#include "SceneCompositionBPLib.h"
#include "RecordingBPLib.h"
#include "UnrealcvLog.h"
#include "Engine/World.h"

FAutomationConfig UDatasetAutomationBPLib::CurrentConfig;
FAutomationStatus UDatasetAutomationBPLib::CurrentStatus;
FSceneHandle UDatasetAutomationBPLib::CurrentScene;
UWorld* UDatasetAutomationBPLib::WorldContext = nullptr;
FTimerHandle UDatasetAutomationBPLib::AutomationTimerHandle;

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

	TransitionToState(EDatasetGenerationState::GeneratingScene);

	WorldContext->GetTimerManager().SetTimer(
		AutomationTimerHandle,
		FTimerDelegate::CreateStatic(&UDatasetAutomationBPLib::AutoTick),
		0.016f,
		true
	);

	UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Started batch generation (%d scenes)"), Config.TotalScenes);

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

	if (CurrentStatus.State == EDatasetGenerationState::Recording ||
		CurrentStatus.State == EDatasetGenerationState::WaitingForRecordingComplete)
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
	case EDatasetGenerationState::GeneratingScene:
		StateString = TEXT("Generating Scene");
		break;
	case EDatasetGenerationState::Recording:
		StateString = TEXT("Recording");
		break;
	case EDatasetGenerationState::WaitingForRecordingComplete:
		StateString = TEXT("Waiting for Recording");
		break;
	case EDatasetGenerationState::CleaningUp:
		StateString = TEXT("Cleaning Up");
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

	ProcessState(0.016f);

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
	case EDatasetGenerationState::GeneratingScene:
	{
		if (CurrentStatus.CurrentSceneIndex >= CurrentStatus.TotalScenes)
		{
			TransitionToState(EDatasetGenerationState::Completed);
			UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Completed all %d scenes"), CurrentStatus.TotalScenes);
			return;
		}

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

		if (!Success)
		{
			CurrentStatus.ErrorMessage = FString::Printf(
				TEXT("Failed to generate scene %d"), CurrentStatus.CurrentSceneIndex);
			TransitionToState(EDatasetGenerationState::Error);
			UE_LOG(LogUnrealCV, Error, TEXT("DatasetAutomation: %s"), *CurrentStatus.ErrorMessage);
			return;
		}

		TransitionToState(EDatasetGenerationState::Recording);
		break;
	}

	case EDatasetGenerationState::Recording:
	{
		FString FileName = GenerateFileName(CurrentStatus.CurrentSceneIndex);
		CurrentStatus.CurrentFileName = FileName;

		bool RecordingStarted = false;

		if (CurrentConfig.bUseTrajectory)
		{
			RecordingStarted = URecordingBPLib::StartTrajectoryRecording(
				CurrentConfig.CameraID,
				FileName,
				CurrentConfig.TrajectoryType,
				CurrentScene.ForegroundActor,
				CurrentConfig.TrajectoryFPS,
				CurrentConfig.TrajectoryDegreesPerSecond,
				-1
			);
		}
		else if (CurrentConfig.bUseBulletTime)
		{
			RecordingStarted = URecordingBPLib::StartBulletTimeRecording(
				CurrentConfig.CameraID,
				FileName,
				CurrentConfig.RecordingDuration,
				CurrentConfig.RecordingFPS,
				CurrentScene.ForegroundActor,
				1.0f
			);
		}
		else
		{
			RecordingStarted = URecordingBPLib::StartNormalRecording(
				CurrentConfig.CameraID,
				FileName,
				CurrentConfig.RecordingDuration,
				CurrentConfig.RecordingFPS,
				nullptr,
				1.0f
			);
		}

		if (!RecordingStarted)
		{
			CurrentStatus.ErrorMessage = FString::Printf(
				TEXT("Failed to start recording for scene %d"), CurrentStatus.CurrentSceneIndex);
			TransitionToState(EDatasetGenerationState::Error);
			UE_LOG(LogUnrealCV, Error, TEXT("DatasetAutomation: %s"), *CurrentStatus.ErrorMessage);
			return;
		}

		TransitionToState(EDatasetGenerationState::WaitingForRecordingComplete);
		break;
	}

	case EDatasetGenerationState::WaitingForRecordingComplete:
	{
		bool StillRecording = URecordingBPLib::IsRecording(CurrentConfig.CameraID);

		if (!StillRecording)
		{
			TransitionToState(EDatasetGenerationState::CleaningUp);
		}
		break;
	}

	case EDatasetGenerationState::CleaningUp:
	{
		USceneCompositionBPLib::ClearScene(CurrentScene);
		CurrentScene = FSceneHandle();

		CurrentStatus.CurrentSceneIndex++;
		CurrentStatus.Progress = (float)CurrentStatus.CurrentSceneIndex / (float)CurrentStatus.TotalScenes;

		UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Completed scene %d/%d (%.1f%%)"),
			CurrentStatus.CurrentSceneIndex, CurrentStatus.TotalScenes, CurrentStatus.Progress * 100.0f);

		TransitionToState(EDatasetGenerationState::GeneratingScene);
		break;
	}

	case EDatasetGenerationState::Error:
	case EDatasetGenerationState::Completed:
	case EDatasetGenerationState::Idle:
	default:
		break;
	}
}

FString UDatasetAutomationBPLib::GenerateFileName(int32 Index)
{
	FString FileType = TEXT("normal");
	if (CurrentConfig.bUseTrajectory)
	{
		FileType = CurrentConfig.TrajectoryType;
	}
	else if (CurrentConfig.bUseBulletTime)
	{
		FileType = TEXT("bullettime");
	}

	return FString::Printf(TEXT("%s/%s_%04d"),
		*CurrentConfig.OutputDirectory, *FileType, Index);
}
