// Copyright 2025 UnrealCV Team. All Rights Reserved.
#include "DatasetAutomationBPLib.h"
#include "Utils/GenericTickableObject.h"
#include "SceneCompositionBPLib.h"
#include "RecordingBPLib.h"
#include "SensorBPLib.h"
#include "AnnotationBPLib.h"
#include "FusionCameraActor.h"
#include "NavAgentController.h"
#include "Engine/World.h"
#include "UnrealcvServer.h"
#include "UnrealcvLog.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformTime.h"

FAutomationConfig UDatasetAutomationBPLib::CurrentConfig;
FAutomationStatus UDatasetAutomationBPLib::CurrentStatus;
FSceneHandle UDatasetAutomationBPLib::CurrentScene;
UWorld* UDatasetAutomationBPLib::WorldContext = nullptr;

TArray<FString> UDatasetAutomationBPLib::ActiveCameraPool;
// TMap<FString, bool> UDatasetAutomationBPLib::CameraRecordingState;

TArray<FAutomationStep> UDatasetAutomationBPLib::CommandQueue;
int32 UDatasetAutomationBPLib::CurrentCommandIndex = 0;
int32 UDatasetAutomationBPLib::CurrentSceneCounter = 0;
FString UDatasetAutomationBPLib::CurrentSceneID = TEXT("");
double UDatasetAutomationBPLib::DelayStartTime = 0.0;
double UDatasetAutomationBPLib::DelayDuration = 0.0;
FGenericTickableObject* UDatasetAutomationBPLib::TickableObject = nullptr;

void UDatasetAutomationBPLib::BuildCommandSequenceForScene()
{
	CommandQueue.Empty();

	CommandQueue.Add(FAutomationStep(TEXT("create_scene")));
	CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT(""), 5.0f));
	CommandQueue.Add(FAutomationStep(TEXT("prepare_record")));
	CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT(""), 10.0f));
	// CommandQueue.Add(FAutomationStep(TEXT("record_trajectory"), TEXT("render_only_5s")));
	CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT(""), 1.0f));
	// CommandQueue.Add(FAutomationStep(TEXT("annotate_world")));
	CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT(""), 1.0f));
	CommandQueue.Add(FAutomationStep(TEXT("record_trajectory"), TEXT("render_only")));
	CommandQueue.Add(FAutomationStep(TEXT("special_wait"), TEXT(""), FMath::RandRange(50.f, 70.f)));
	// CommandQueue.Add(FAutomationStep(TEXT("set_pause"), TEXT("true")));
	CommandQueue.Add(FAutomationStep(TEXT("record_trajectory"), TEXT("rotate_left_30")));
	CommandQueue.Add(FAutomationStep(TEXT("record_trajectory"), TEXT("rotate_right_30")));
	CommandQueue.Add(FAutomationStep(TEXT("record_trajectory"), TEXT("rotate_up_30")));
	CommandQueue.Add(FAutomationStep(TEXT("record_trajectory"), TEXT("rotate_360")));
	CommandQueue.Add(FAutomationStep(TEXT("record_trajectory"), TEXT("zoom_in")));
	CommandQueue.Add(FAutomationStep(TEXT("record_trajectory"), TEXT("zoom_out")));
	CommandQueue.Add(FAutomationStep(TEXT("record_trajectory"), TEXT("random_1")));
	CommandQueue.Add(FAutomationStep(TEXT("record_trajectory"), TEXT("random_2")));
	CommandQueue.Add(FAutomationStep(TEXT("record_trajectory"), TEXT("random_3")));
	CommandQueue.Add(FAutomationStep(TEXT("record_trajectory"), TEXT("random_4")));
	// CommandQueue.Add(FAutomationStep(TEXT("set_pause"), TEXT("false")));
	CommandQueue.Add(FAutomationStep(TEXT("sync_all_cameras")));
	CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT(""), 2.0f));
	// CommandQueue.Add(FAutomationStep(TEXT("save_videos")));

	// CommandQueue.Add(FAutomationStep(TEXT("record_trajectory"), TEXT("render_only")));
	// CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT(""), 2.0f));

	// CommandQueue.Add(FAutomationStep(TEXT("record_nav_track")));
	// CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT(""), 2.0f));

	CommandQueue.Add(FAutomationStep(TEXT("clear_scene")));

	CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT(""), 0.5f));

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

		if (CurrentConfig.bLoadSceneParamsFromJson)
		{
			FString JsonFilePath = FPaths::ProjectSavedDir() / TEXT("SceneComposition.json");
			if (!USceneCompositionBPLib::CreateSceneParamsFromJson(WorldContext, JsonFilePath, CurrentConfig.SceneParams))
			{
				UE_LOG(LogUnrealCV, Error, TEXT("DatasetAutomation: Failed to load scene params from JSON"));
				TransitionToState(EDatasetGenerationState::Error);
				return;
			}
		}

		bool Success = USceneCompositionBPLib::GenerateRandomScene(
			WorldContext,
			CurrentConfig.SceneParams,
			CurrentScene
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
	else if (Step.Command == TEXT("prepare_record"))
	{
		int32 CameraID = CurrentConfig.SceneParams.CameraID;
		UFusionCamSensor* Sensor = USensorBPLib::GetSensorById(CameraID);
		if (!IsValid(Sensor))
		{
			UE_LOG(LogUnrealCV, Error, TEXT("prepare_record: Failed to get sensor for camera %d"), CameraID);
			TransitionToState(EDatasetGenerationState::Error);
			return;
		}

		FString PrimaryCameraID = USensorBPLib::GetSensorNewFormatID(Sensor);
		ActiveCameraPool.Empty();
		ActiveCameraPool.Add(PrimaryCameraID);


		UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Prepared_record, primary camera is : %s"), *PrimaryCameraID);
		ExecuteNextCommand();
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
			UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Fired trajectory recording: %s -> %s"), *TrajectoryType, *OutputPath);
			ExecuteNextCommand();
		}
		else
		{
			CurrentStatus.ErrorMessage = FString::Printf(TEXT("Failed to start recording: %s"), *TrajectoryType);
			TransitionToState(EDatasetGenerationState::Error);
			UE_LOG(LogUnrealCV, Error, TEXT("DatasetAutomation: %s"), *CurrentStatus.ErrorMessage);
		}
	}
	else if (Step.Command == TEXT("special_wait"))
	{
		check(CurrentScene.CameraID >= 0);
		auto* PriamaryCam = USensorBPLib::GetSensorById(CurrentScene.CameraID);
		check(PriamaryCam);
		auto* PriCaptureActor = URecordingBPLib::GetCaptureActor(USensorBPLib::GetSensorNewFormatID(PriamaryCam));
		check(PriCaptureActor);
		
		float TriggerIndex = Step.FloatParam;
		if (PriCaptureActor->GetCurrentTrajectoryIndex() >= static_cast<int32>(TriggerIndex))
		{
			ExecuteNextCommand();
		}
		// wait for next call
		UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Special wait for trajectory index: %f"), TriggerIndex);
	}
	else if (Step.Command == TEXT("set_pause"))
	{
		if (Step.StringParam == "true")
		{
			FUnrealcvServer::Get().GetWorld()->GetFirstPlayerController()->SetPause(true);
			ExecuteNextCommand();
		}
		else if (Step.StringParam == "false")
		{
			FUnrealcvServer::Get().GetWorld()->GetFirstPlayerController()->SetPause(false);
			ExecuteNextCommand();
		}
		else
		{
			UE_LOG(LogUnrealCV, Error, TEXT("DatasetAutomation: Invalid pause parameter: %s"), *Step.StringParam);
			TransitionToState(EDatasetGenerationState::Error);
			return;
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

		AFusionCameraActor* CameraActor = GetFusionCameraActor(CurrentConfig.SceneParams.CameraID);
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
		DelayStartTime = FPlatformTime::Seconds();
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
			BuildCommandSequenceForScene();
			ExecuteNextCommand();
		}
	}
	else if (Step.Command == TEXT("sync_all_cameras"))
	{
		UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Synchronizing all cameras..."));
		TransitionToState(EDatasetGenerationState::WaitingAsync);
	}
	else if (Step.Command == TEXT("save_videos"))
	{
		for (auto& CID : ActiveCameraPool)
		{
			AFusionCamCaptureActor* CaptureActor = URecordingBPLib::GetCaptureActor(CID);
			if (IsValid(CaptureActor))
			{
				CaptureActor->TriggerVideoGeneration();
			}
		}
		ExecuteNextCommand();
	}
	else if (Step.Command == TEXT("annotate_world"))
	{
		UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Annotating world: %s"), *CurrentSceneID);
		UAnnotationBPLib::AnnotateWorld();
		ExecuteNextCommand();
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

	ActiveCameraPool.Empty();
	// CameraRecordingState.Empty();

	BuildCommandSequenceForScene();
	CurrentCommandIndex = 0;
	CurrentSceneCounter = 0;

	if (!TickableObject)
	{
		TickableObject = new FGenericTickableObject();
		TickableObject->SetTickCallback(&UDatasetAutomationBPLib::OnTick);
	}
	TickableObject->Activate();

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

	if (TickableObject)
	{
		TickableObject->Deactivate();
	}

	for (const FString& CID : ActiveCameraPool)
	{
		URecordingBPLib::StopRecording(CID);
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

void UDatasetAutomationBPLib::OnTick(double RealDeltaTime)
{
	if (!IsRunning())
	{
		if (CurrentStatus.State == EDatasetGenerationState::Completed ||
			CurrentStatus.State == EDatasetGenerationState::Error)
		{
			if (TickableObject)
			{
				TickableObject->Deactivate();
			}
		}
		return;
	}

	ProcessState(RealDeltaTime);
}

void UDatasetAutomationBPLib::TransitionToState(EDatasetGenerationState NewState)
{
	CurrentStatus.State = NewState;
}

void UDatasetAutomationBPLib::ProcessState(double RealDeltaTime)
{
	UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: ProcessState: %s"), *GetAutomationStatusString());
	switch (CurrentStatus.State)
	{
	case EDatasetGenerationState::ExecutingCommand:
	{
		// UE_LOG(LogUnrealCV, Warning, TEXT("DatasetAutomation: Executing no command (%.2fs real time)"), RealDeltaTime);
		if (CurrentCommandIndex >= CommandQueue.Num())
		{
			UE_LOG(LogUnrealCV, Warning, TEXT("DatasetAutomation: Command index out of bounds"));
			return;
		}
		const FAutomationStep& Step = CommandQueue[CurrentCommandIndex];
		UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Executing command %d/%d: %s"),
			CurrentCommandIndex + 1, CommandQueue.Num(), *Step.Command);
		ExecuteCommand(Step);
		break;
	}

	case EDatasetGenerationState::WaitingAsync:
	{
		double CurrentRealTime = FPlatformTime::Seconds();
		double ElapsedRealTime = CurrentRealTime - DelayStartTime;
		bool DelayComplete = (ElapsedRealTime >= DelayDuration);
		bool AllCamerasIdle = AreAllCamerasIdle();

		UE_LOG(LogUnrealCV, Warning, TEXT("DatasetAutomation: Waiting (%.2fs/%.2fs real time), AllCamerasIdle: %d"), ElapsedRealTime, DelayDuration, AllCamerasIdle);

		if (AllCamerasIdle && DelayComplete)
		{
			if (IsValid(CurrentScene.NavController) && CurrentScene.NavController->IsNavigating())
			{
				CurrentScene.NavController->StopNavigation();
			}

			ExecuteNextCommand();
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
	AActor* Target = CurrentScene.ForegroundActor;
	int32 FPS = CurrentConfig.TrajectoryFPS;
	float DegreesPerSecond = CurrentConfig.TrajectoryDegreesPerSecond;
	int32 RandomSeed = -1;

	if (!IsValid(Target))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("StartTrajectoryRecording: Target actor is null"));
		return false;
	}

	int32 AllocatedCID = USensorBPLib::GetIndexByAnyID(GetIdleCamera());
	auto* AllocatedCam = USensorBPLib::GetSensorById(AllocatedCID);
	check(AllocatedCam);
	check(AllocatedCID >= 0);

	AFusionCamCaptureActor* CaptureActor = URecordingBPLib::PrepareRecording(AllocatedCID);
	if (!IsValid(CaptureActor))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("StartTrajectoryRecording: Failed to prepare recording for camera %d"), AllocatedCID);
		return false;
	}

	// Crucial
	CaptureActor->SetSceneHandle(CurrentScene);



	int32 PrimaryCameraID = CurrentConfig.SceneParams.CameraID;
	if (AllocatedCID != PrimaryCameraID)
	{
		auto* PrimaryCam = USensorBPLib::GetSensorById(PrimaryCameraID);
		check(PrimaryCam);
		AllocatedCam->SetSensorLocation(PrimaryCam->GetSensorLocation());
		AllocatedCam->SetSensorRotation(PrimaryCam->GetSensorRotation());
	}
	else
	{
		// Adjust camera to roughly aim at the target with ±15 degrees noise
		FVector CameraToTarget = (CaptureActor->GetTargetLocationWithRandomHeight(Target) - AllocatedCam->GetSensorLocation()).GetSafeNormal();
		FRotator TargetRotation = CameraToTarget.Rotation();

		// Add ±15 degrees noise to pitch, yaw, and roll
		float NoisePitch = FMath::RandRange(-4.0f, 4.0f);
		float NoiseYaw = FMath::RandRange(-1.0f, 1.0f);
		float NoiseRoll = FMath::RandRange(-4.0f, 4.0f);

		FRotator NoisyRotation = TargetRotation + FRotator(NoisePitch, NoiseYaw, NoiseRoll);
		AllocatedCam->SetSensorRotation(NoisyRotation);
	}




	ECameraTrajectoryType TrajectoryEnum;
	if (!URecordingBPLib::ParseTrajectoryType(TrajectoryType, TrajectoryEnum))
	{
		return false;
	}

	UE_LOG(LogUnrealCV, Log, TEXT("StartTrajectoryRecording: Camera %d, File: %s, Type: %s, FPS: %d, Deg/s: %.2f, Target: %s"),
		AllocatedCID, *FileName, *TrajectoryType, FPS, DegreesPerSecond, *Target->GetName());
	check(Target);
	check(FPS > 0);
	check(DegreesPerSecond > 0);
	CaptureActor->StartTrajectoryRecord(FileName, TrajectoryEnum, Target, FPS, DegreesPerSecond, RandomSeed, false);
	return true;
}

bool UDatasetAutomationBPLib::SetMap(UObject* WorldContextObject, const FString& MapName)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("SetMap: Invalid world context"));
		return false;
	}

	if (MapName.IsEmpty())
	{
		UE_LOG(LogUnrealCV, Error, TEXT("SetMap: Map name cannot be empty"));
		return false;
	}

	UGameplayStatics::OpenLevel(World, FName(*MapName));
	UGameplayStatics::FlushLevelStreaming(World);
	UE_LOG(LogUnrealCV, Log, TEXT("SetMap: Loading map '%s'"), *MapName);

	return true;
}

FString UDatasetAutomationBPLib::GetIdleCamera()
{


	if (!URecordingBPLib::IsRecording(CurrentScene.CameraID))
	{
		return USensorBPLib::GetSensorNewFormatID(USensorBPLib::GetSensorById(CurrentScene.CameraID));
	}

	// // check(ActiveCameraPool.Num() > 0);
	// for (const FString& CID : ActiveCameraPool)
	// {
	// 	// bool* pIsRecording = CameraRecordingState.Find(CID);
	// 	// if (pIsRecording && !(*pIsRecording))
	// 	// {
	// 	// 	return CID;
	// 	// }
	// 	bool IsRecording = URecordingBPLib::IsRecording(CID);
	// 	if (!IsRecording)
	// 	{
	// 		return CID;
	// 	}
	// }

	if (!WorldContext || !IsValid(CurrentScene.ForegroundActor))
	{
		return TEXT("");
	}

	int32 NewCameraID = URecordingBPLib::CreateFreeCamera(WorldContext);
	if (NewCameraID < 0)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("GetIdleCameraFromPool: Failed to create new free camera"));
		return TEXT("");
	}

	UFusionCamSensor* Sensor = USensorBPLib::GetSensorById(NewCameraID);
	if (!IsValid(Sensor))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("GetIdleCameraFromPool: Failed to get sensor for new camera %d"), NewCameraID);
		return TEXT("");
	}

	FString NewCID = USensorBPLib::GetSensorNewFormatID(Sensor);
	ActiveCameraPool.Add(NewCID);

	UE_LOG(LogUnrealCV, Log, TEXT("GetIdleCameraFromPool: Created new camera %s (ID: %d)"), *NewCID, NewCameraID);
	return NewCID;
}

bool UDatasetAutomationBPLib::AreAllCamerasIdle()
{
	for (const FString& CID : ActiveCameraPool)
	{
		if (URecordingBPLib::IsRecording(CID))
		{
			return false;
		}
	}
	return true;
}

