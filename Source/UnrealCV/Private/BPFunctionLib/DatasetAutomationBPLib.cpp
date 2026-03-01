// Copyright 2025 UnrealCV Team. All Rights Reserved.
#include "DatasetAutomationBPLib.h"
#include "Utils/GenericTickableObject.h"
#include "Utils/UObjectUtils.h"
#include "SceneCompositionBPLib.h"
#include "RecordingBPLib.h"
#include "SensorBPLib.h"
#include "AnnotationBPLib.h"
#include "AnimationBPLib.h"
#include "GroomBPLib.h"
#include "FusionCameraActor.h"
#include "NavAgentController.h"
#include "Engine/World.h"
#include "UnrealcvServer.h"
#include "UnrealcvLog.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "JsonConfigHelper.h"
#include "MovieQualityRenderComponent.h"

FAutomationConfig UDatasetAutomationBPLib::CurrentConfig;
FAutomationStatus UDatasetAutomationBPLib::CurrentStatus;
FSceneHandle UDatasetAutomationBPLib::CurrentScene;
UWorld* UDatasetAutomationBPLib::WorldContext = nullptr;

TArray<FString> UDatasetAutomationBPLib::ActiveCameraPool;
// TMap<FString, bool> UDatasetAutomationBPLib::CameraRecordingState;

TArray<FAutomationStep> UDatasetAutomationBPLib::CommandQueue;
TArray<FCompletedCommand> UDatasetAutomationBPLib::CommandHistory;
int32 UDatasetAutomationBPLib::CurrentCommandIndex = -1;
int32 UDatasetAutomationBPLib::CurrentSceneCounter = 0;
FString UDatasetAutomationBPLib::CurrentSceneID = TEXT("");
// FString UDatasetAutomationBPLib::TaskName = TEXT("Trajectory");
FString UDatasetAutomationBPLib::TaskName = TEXT("Matting");
// FString UDatasetAutomationBPLib::TaskName = TEXT("SpeedTest");
double UDatasetAutomationBPLib::DelayStartTime = 0.0;
double UDatasetAutomationBPLib::DelayDuration = 0.0;
FGenericTickableObject* UDatasetAutomationBPLib::TickableObject = nullptr;

TArray<FAutomationStep> UDatasetAutomationBPLib::ExternalCommandQueue;

static float GetForegroundActorTopZ()
{
	if (!IsValid(UDatasetAutomationBPLib::CurrentScene.ForegroundActor))
	{
		return 170.0f;
		UE_LOG(LogUnrealCV, Error, TEXT("!IsValid(UDatasetAutomationBPLib::CurrentScene.ForegroundActor"));
	}

	FBox ActorBounds = UDatasetAutomationBPLib::CurrentScene.ForegroundActor->GetComponentsBoundingBox();
	if (ActorBounds.IsValid)
	{
		return ActorBounds.Min.Z + (ActorBounds.Max.Z - ActorBounds.Min.Z) * 0.86;
	}
	UE_LOG(LogUnrealCV, Error, TEXT("!ActorBounds.IsValid"));
	return 170.0f;
}

void UDatasetAutomationBPLib::BuildCommandSequenceForScene()
{
	CommandQueue.Empty();

	if (ExternalCommandQueue.Num() > 0)
	{
		CommandQueue = ExternalCommandQueue;
		UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Using external command sequence with %d commands"), CommandQueue.Num());
		return;
	}
	CommandQueue.Empty();

	if (TaskName == TEXT("Trajectory"))
	{
		CurrentConfig.RecordingConfig = FRecordingDataTypesConfig::MakeTrajectoryConfig();
		CurrentConfig.NumFrames = 121;
		CurrentConfig.TrajectoryFPS = 30;
 		CurrentConfig.ForegroundMoveSpeed = 0.0f;
		CommandQueue.Add(FAutomationStep(TEXT("vrun"), TEXT("vset /mqrc/render_immediately true")));
		CommandQueue.Add(FAutomationStep(TEXT("vrun"), TEXT("vset /captureactor/time_dilation 0.5")));
		CommandQueue.Add(FAutomationStep(TEXT("load_scene_param_json")));
		CommandQueue.Add(FAutomationStep(TEXT("random_scene_param_camera_height"), TEXT("160 175")));
		CommandQueue.Add(FAutomationStep(TEXT("random_scene_param_camera_angle_offset"), TEXT("-15 15")));
		CommandQueue.Add(FAutomationStep(TEXT("random_scene_param_camera_distance"), TEXT("250 400")));
		CommandQueue.Add(FAutomationStep(TEXT("create_scene")));
		CommandQueue.Add(FAutomationStep(TEXT("render_every_frame"), TEXT("true")));
		CommandQueue.Add(FAutomationStep(TEXT("set_animation_bp"), TEXT("/Game/MetaHumans/ABP_RandomIdle.ABP_RandomIdle_C")));
		CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT("5.0")));
		CommandQueue.Add(FAutomationStep(TEXT("random_resolution"), TEXT("1920x1080")));
		CommandQueue.Add(FAutomationStep(TEXT("random_fov"), TEXT("40 55")));
		CommandQueue.Add(FAutomationStep(TEXT("aim_camera_at_foreground"), TEXT("155 175")));
		CommandQueue.Add(FAutomationStep(TEXT("add_camera_rotation_noise"), TEXT("0.0 1.0 4.0")));
		CommandQueue.Add(FAutomationStep(TEXT("prepare_record")));
		CommandQueue.Add(FAutomationStep(TEXT("sync_pawn_to_primary_camera")));
		CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT("10.0")));
		// CommandQueue.Add(FAutomationStep(TEXT("record_trajectory"), TEXT("render_only_5s")));
		CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT("1.0")));
		// CommandQueue.Add(FAutomationStep(TEXT("annotate_world")));
		CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT("1.0")));
		CommandQueue.Add(FAutomationStep(TEXT("record_trajectory"), TEXT("render_only")));
		float SyncFrame = FMath::RandRange(50.f, 70.f);
		CommandQueue.Add(FAutomationStep(TEXT("special_wait"), FString::SanitizeFloat(SyncFrame)));
		CommandQueue.Add(FAutomationStep(TEXT("set_pause"), TEXT("true")));
		CommandQueue.Add(FAutomationStep(TEXT("record_trajectory"), TEXT("rotate_left_30")));
		CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT("5.0")));
		CommandQueue.Add(FAutomationStep(TEXT("record_trajectory"), TEXT("rotate_right_30")));
		CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT("5.0")));
		CommandQueue.Add(FAutomationStep(TEXT("record_trajectory"), TEXT("rotate_up_30")));
		CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT("5.0")));
		CommandQueue.Add(FAutomationStep(TEXT("record_trajectory"), TEXT("rotate_360")));
		CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT("5.0")));
		CommandQueue.Add(FAutomationStep(TEXT("record_trajectory"), TEXT("zoom_in")));
		CommandQueue.Add(FAutomationStep(TEXT("sync_secondary_cameras")));
		CommandQueue.Add(FAutomationStep(TEXT("record_trajectory"), TEXT("zoom_out")));
		CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT("5.0")));
		CommandQueue.Add(FAutomationStep(TEXT("record_trajectory"), TEXT("random_1")));
		CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT("5.0")));
		CommandQueue.Add(FAutomationStep(TEXT("record_trajectory"), TEXT("random_2")));
		CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT("5.0")));
		CommandQueue.Add(FAutomationStep(TEXT("record_trajectory"), TEXT("random_3")));
		CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT("5.0")));
		CommandQueue.Add(FAutomationStep(TEXT("record_trajectory"), TEXT("random_4")));

		CommandQueue.Add(FAutomationStep(TEXT("sync_secondary_cameras")));
		CommandQueue.Add(FAutomationStep(TEXT("set_pause"), TEXT("false")));
		CommandQueue.Add(FAutomationStep(TEXT("set_time_dilation"), TEXT("1.0")));
		CommandQueue.Add(FAutomationStep(TEXT("sync_all_cameras")));
		CommandQueue.Add(FAutomationStep(TEXT("set_time_dilation"), TEXT("1.0")));
		CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT("1.0")));
		// CommandQueue.Add(FAutomationStep(TEXT("save_videos")));

		// CommandQueue.Add(FAutomationStep(TEXT("record_nav_track")));
		// CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT(""), 2.0f));

		CommandQueue.Add(FAutomationStep(TEXT("clear_scene")));
		CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT("0.5")));
		CommandQueue.Add(FAutomationStep(TEXT("increment_counter")));
		// CommandQueue.Add(FAutomationStep(TEXT("load_random_level_every_n_scenes"), TEXT("1")));
		CommandQueue.Add(FAutomationStep(TEXT("check_completion")));
	}
	else if (TaskName == TEXT("Matting"))
	{
		CurrentConfig.RecordingConfig = FRecordingDataTypesConfig::MakeMattingConfig();
		CurrentConfig.NumFrames = 90;
		CurrentConfig.TrajectoryFPS = 30;
 		CurrentConfig.ForegroundMoveSpeed = 70.0f;
		CurrentConfig.ForegroundMoveAngleOffset = 90.0f;
		CommandQueue.Add(FAutomationStep(TEXT("vrun"), TEXT("vset /captureactor/time_dilation 0.65")));
		CommandQueue.Add(FAutomationStep(TEXT("load_scene_param_json")));
		CommandQueue.Add(FAutomationStep(TEXT("random_scene_param_camera_height"), TEXT("120 155")));
		CommandQueue.Add(FAutomationStep(TEXT("random_scene_param_camera_angle_offset"), TEXT("-60 60")));
		CommandQueue.Add(FAutomationStep(TEXT("random_scene_param_camera_distance"), TEXT("75 100")));
		CommandQueue.Add(FAutomationStep(TEXT("create_scene")));
		// CommandQueue.Add(FAutomationStep(TEXT("render_every_frame"), TEXT("true")));
		// CommandQueue.Add(FAutomationStep(TEXT("set_animation_bp"), TEXT("/Game/MetaHumans/ABP_RandomHeadMovement.ABP_RandomHeadMovement_C")));
		CommandQueue.Add(FAutomationStep(TEXT("set_animation_bp"), TEXT("/Game/MetaHumans/ABP_Run.ABP_Run_C")));
		CommandQueue.Add(FAutomationStep(TEXT("prepare_groom")));
		CommandQueue.Add(FAutomationStep(TEXT("sync_pawn_to_primary_camera")));
		CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT("5.0")));
		
		if(FMath::RandRange(0.0f, 100.0f) < 50.0f)
		{
			CommandQueue.Add(FAutomationStep(TEXT("random_resolution"), TEXT("1080x1920")));
			CommandQueue.Add(FAutomationStep(TEXT("random_fov"), TEXT("50 60")));
		}
		else
		{
			CommandQueue.Add(FAutomationStep(TEXT("random_resolution"), TEXT("1920x1080")));
			CommandQueue.Add(FAutomationStep(TEXT("random_fov"), TEXT("80 90")));
		}
		
		CommandQueue.Add(FAutomationStep(TEXT("aim_camera_at_foreground"), TEXT("125 175")));
		CommandQueue.Add(FAutomationStep(TEXT("add_camera_rotation_noise"), TEXT("4.0 0.5 2.0")));
		CommandQueue.Add(FAutomationStep(TEXT("prepare_record")));
		CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT("10.0")));
		CommandQueue.Add(FAutomationStep(TEXT("record_trajectory"), TEXT("render_only")));
		CommandQueue.Add(FAutomationStep(TEXT("sync_all_cameras")));
		CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT("1.0")));
		CommandQueue.Add(FAutomationStep(TEXT("clear_scene")));
		CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT("0.5")));
		CommandQueue.Add(FAutomationStep(TEXT("increment_counter")));
		// CommandQueue.Add(FAutomationStep(TEXT("load_random_level_every_n_scenes"), TEXT("1")));
		CommandQueue.Add(FAutomationStep(TEXT("check_completion")));
	}
	else if (TaskName == TEXT("Omnimatte"))
	{
		CurrentConfig.RecordingConfig = FRecordingDataTypesConfig::MakeOmnimatteConfig();
		CurrentConfig.NumFrames = 240;
		CurrentConfig.TrajectoryFPS = 24;
 		CurrentConfig.ForegroundMoveSpeed = 0.0f;
		CommandQueue.Add(FAutomationStep(TEXT("vrun"), TEXT("vset /captureactor/time_dilation 0.5")));
		CommandQueue.Add(FAutomationStep(TEXT("load_scene_param_json")));
		CommandQueue.Add(FAutomationStep(TEXT("random_scene_param_camera_height"), TEXT("160 175")));
		CommandQueue.Add(FAutomationStep(TEXT("random_scene_param_camera_angle_offset"), TEXT("-15 15")));
		CommandQueue.Add(FAutomationStep(TEXT("random_scene_param_camera_distance"), TEXT("250 400")));
		CommandQueue.Add(FAutomationStep(TEXT("create_scene")));
		CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT("5.0")));
		CommandQueue.Add(FAutomationStep(TEXT("random_resolution"), TEXT("640x480 480x640")));
		CommandQueue.Add(FAutomationStep(TEXT("random_fov"), TEXT("40 55")));
		CommandQueue.Add(FAutomationStep(TEXT("aim_camera_at_foreground"), TEXT("155 175")));
		CommandQueue.Add(FAutomationStep(TEXT("add_camera_rotation_noise"), TEXT("4.0 1.0 4.0")));
		CommandQueue.Add(FAutomationStep(TEXT("prepare_record")));
		CommandQueue.Add(FAutomationStep(TEXT("sync_pawn_to_primary_camera")));
		CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT("10.0")));
		CommandQueue.Add(FAutomationStep(TEXT("record_trajectory"), TEXT("render_only")));
		CommandQueue.Add(FAutomationStep(TEXT("sync_all_cameras")));

		CommandQueue.Add(FAutomationStep(TEXT("clear_scene")));
		CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT("0.5")));
		CommandQueue.Add(FAutomationStep(TEXT("increment_counter")));
		CommandQueue.Add(FAutomationStep(TEXT("check_completion")));
	}
	else if (TaskName == "SpeedTest")
	{
		CurrentConfig.RecordingConfig = FRecordingDataTypesConfig::MakeSpeedTestConfig();
		CurrentConfig.NumFrames = 240;
		CurrentConfig.TrajectoryFPS = 60;
 		CurrentConfig.ForegroundMoveSpeed = 0.0f;
		CommandQueue.Add(FAutomationStep(TEXT("load_scene_param_json")));
		CommandQueue.Add(FAutomationStep(TEXT("random_scene_param_camera_height"), TEXT("160 175")));
		CommandQueue.Add(FAutomationStep(TEXT("random_scene_param_camera_angle_offset"), TEXT("-15 15")));
		CommandQueue.Add(FAutomationStep(TEXT("random_scene_param_camera_distance"), TEXT("250 400")));
		CommandQueue.Add(FAutomationStep(TEXT("create_scene")));
		CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT("8.0")));
		CommandQueue.Add(FAutomationStep(TEXT("random_resolution"), TEXT("1920x1080")));
		CommandQueue.Add(FAutomationStep(TEXT("random_fov"), TEXT("60")));
		CommandQueue.Add(FAutomationStep(TEXT("aim_camera_at_foreground"), TEXT("155 175")));
		CommandQueue.Add(FAutomationStep(TEXT("add_camera_rotation_noise"), TEXT("4.0 1.0 4.0")));
		CommandQueue.Add(FAutomationStep(TEXT("prepare_record")));
		CommandQueue.Add(FAutomationStep(TEXT("sync_pawn_to_primary_camera")));
		CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT("6.0")));
		// CommandQueue.Add(FAutomationStep(TEXT("record_trajectory"), TEXT("render_only")));
		// CommandQueue.Add(FAutomationStep(TEXT("sync_all_cameras")));
		CommandQueue.Add(FAutomationStep(TEXT("record_trajectory"), TEXT("rotate_left_30")));
		CommandQueue.Add(FAutomationStep(TEXT("sync_all_cameras")));
		CommandQueue.Add(FAutomationStep(TEXT("clear_scene")));
		CommandQueue.Add(FAutomationStep(TEXT("delay"), TEXT("0.5")));
		CommandQueue.Add(FAutomationStep(TEXT("increment_counter")));
		CommandQueue.Add(FAutomationStep(TEXT("check_completion")));
	}
	else
	{
		UE_LOG(LogUnrealCV, Error, TEXT("DatasetAutomation: Invalid task name"));
		check(false);
	}

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
	if (CurrentCommandIndex >= 0 && CurrentCommandIndex < CommandQueue.Num())
	{
		FCompletedCommand Record;
		Record.Step = CommandQueue[CurrentCommandIndex];
		Record.Timestamp = FDateTime::Now();
		CommandHistory.Add(Record);
	}

	CurrentCommandIndex++;
	check(CurrentCommandIndex >= 0);

	if (CurrentCommandIndex >= CommandQueue.Num())
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("DatasetAutomation: Command index out of bounds"));
		return;
	}

	const FAutomationStep& Step = CommandQueue[CurrentCommandIndex];
	UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Executing command %d/%d: %s"),
		CurrentCommandIndex, CommandQueue.Num(), *Step.Command);
	ExecuteCommand(Step);
}

void UDatasetAutomationBPLib::ExecuteCommand(const FAutomationStep& Step)
{
	TransitionToState(EDatasetGenerationState::ExecutingCommand);

	// I know this huge if-else is really ugly,
	// but it's not performance sensitive,
	// so I'd rather keep it
	if (Step.Command == TEXT("create_scene"))
	{
		CurrentSceneID = GenerateSceneID(CurrentSceneCounter);

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
	else if (Step.Command == TEXT("load_scene_param_json"))
	{
		FString JsonFilePath = USceneCompositionBPLib::GetSceneConfigFilePath();
		if (!USceneCompositionBPLib::CreateSceneParamsFromJson(WorldContext, JsonFilePath, CurrentConfig.SceneParams))
		{
			UE_LOG(LogUnrealCV, Error, TEXT("DatasetAutomation: Failed to load scene params from JSON"));
			TransitionToState(EDatasetGenerationState::Error);
			return;
		}

		UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Loaded scene params from JSON"));
		ExecuteNextCommand();
	}
	else if (Step.Command == TEXT("random_scene_param_camera_height"))
	{
		TArray<FString> Args;
		Step.StringParam.ParseIntoArray(Args, TEXT(" "));
		if (Args.Num() >= 2)
		{
			float Min = FCString::Atof(*Args[0]);
			float Max = FCString::Atof(*Args[1]);
			CurrentConfig.SceneParams.AutoPositionCameraHeight = FMath::RandRange(Min, Max);
		}
		else
		{
			UE_LOG(LogUnrealCV, Error, TEXT("DatasetAutomation: Invalid Argument"));
			TransitionToState(EDatasetGenerationState::Error);
			return;
		}
		UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Random camera height: %.1f"), CurrentConfig.SceneParams.AutoPositionCameraHeight);
		ExecuteNextCommand();
	}
	else if (Step.Command == TEXT("random_scene_param_camera_angle_offset"))
	{
		TArray<FString> Args;
		Step.StringParam.ParseIntoArray(Args, TEXT(" "));
		if (Args.Num() >= 2)
		{
			float Min = FCString::Atof(*Args[0]);
			float Max = FCString::Atof(*Args[1]);
			CurrentConfig.SceneParams.AutoPositionCameraAngleOffset = FMath::RandRange(Min, Max);
		}
		else
		{
			UE_LOG(LogUnrealCV, Error, TEXT("DatasetAutomation: Invalid Argument"));
			TransitionToState(EDatasetGenerationState::Error);
			return;
		}
		UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Random camera angle offset: %.1f"), CurrentConfig.SceneParams.AutoPositionCameraAngleOffset);
		ExecuteNextCommand();
	}
	else if (Step.Command == TEXT("random_scene_param_camera_distance"))
	{
		float Distance;
		TArray<FString> Args;
		Step.StringParam.ParseIntoArray(Args, TEXT(" "));
		if (Args.Num() >= 2)
		{
			float Min = FCString::Atof(*Args[0]);
			float Max = FCString::Atof(*Args[1]);
			Distance = FMath::RandRange(Min, Max);
		}
		else if (Args.Num() == 1)
		{
			Distance = FCString::Atof(*Args[0]);
		}
		else
		{
			UE_LOG(LogUnrealCV, Error, TEXT("DatasetAutomation: Invalid Argument"));
			TransitionToState(EDatasetGenerationState::Error);
			return;
		}

		CurrentConfig.SceneParams.AutoPositionCameraDistance = Distance;
		UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Random camera distance: %.1f"), CurrentConfig.SceneParams.AutoPositionCameraDistance);
		ExecuteNextCommand();
	}
	else if (Step.Command == TEXT("random_fov"))
	{
		TArray<FString> Args;
		Step.StringParam.ParseIntoArray(Args, TEXT(" "));
		if (Args.Num() >= 2)
		{
			float Min = FCString::Atof(*Args[0]);
			float Max = FCString::Atof(*Args[1]);
			CurrentStatus.ChosenFOV = FMath::RandRange(Min, Max);
		}
		else if (Args.Num() == 1)
		{
			CurrentStatus.ChosenFOV = FCString::Atof(*Args[0]);
		}
		else
		{
			UE_LOG(LogUnrealCV, Error, TEXT("DatasetAutomation: Invalid Argument"));
			TransitionToState(EDatasetGenerationState::Error);
			return;
		}
		UE_LOG(LogUnrealCV, Log, TEXT("random_fov: %.1f"), CurrentStatus.ChosenFOV);
		ExecuteNextCommand();
	}
	else if (Step.Command == TEXT("random_resolution"))
	{
		TArray<FString> Args;
		Step.StringParam.ParseIntoArray(Args, TEXT(" "));
		int32 OptionIndex = 0;
		if (Args.Num() >= 2)
		{
			OptionIndex = FMath::RandRange(0, Args.Num() - 1);
		}
		FString Wh = Args[OptionIndex];
		TArray<FString> Parts;
		Wh.ParseIntoArray(Parts, TEXT("x"));
		if (Parts.Num() == 2)
		{
			CurrentStatus.ChosenRes.X = FCString::Atoi(*Parts[0]);
			CurrentStatus.ChosenRes.Y = FCString::Atoi(*Parts[1]);
		}
		UE_LOG(LogUnrealCV, Log, TEXT("random_resolution: %dx%d"), CurrentStatus.ChosenRes.X, CurrentStatus.ChosenRes.Y);
		ExecuteNextCommand();
	}
	else if (Step.Command == TEXT("aim_camera_at_foreground"))
	{
		int32 PrimaryCameraID = CurrentConfig.SceneParams.CameraID;
		UFusionCamSensor* PrimaryCam = USensorBPLib::GetSensorById(PrimaryCameraID);
		if (!IsValid(PrimaryCam))
		{
			UE_LOG(LogUnrealCV, Error, TEXT("Primary camera not found"));
			TransitionToState(EDatasetGenerationState::Error);
			return;
		}

		if (!IsValid(CurrentScene.ForegroundActor))
		{
			UE_LOG(LogUnrealCV, Error, TEXT("DatasetAutomation: ForegroundActor is null"));
			TransitionToState(EDatasetGenerationState::Error);
			return;
		}

		FVector TargetLocation = CurrentScene.ForegroundActor->GetActorLocation();
		TargetLocation.Z = GetForegroundActorTopZ();

		FVector CameraToTarget = (TargetLocation - PrimaryCam->GetSensorLocation()).GetSafeNormal();
		FRotator TargetRotation = CameraToTarget.Rotation();
		TargetRotation.Roll = 0.0f;
		PrimaryCam->SetSensorRotation(TargetRotation);

		UE_LOG(LogUnrealCV, Log, TEXT("aim_camera_at_foreground: Aiming at top Z=%.1f (bounds-based)"),
			TargetLocation.Z);
		ExecuteNextCommand();
	}
	else if (Step.Command == TEXT("add_camera_rotation_noise"))
	{
		TArray<FString> Args;
		Step.StringParam.ParseIntoArray(Args, TEXT(" "));
		if (Args.Num() >= 3)
		{
			
			
			float RollRange = FCString::Atof(*Args[0]);
			float PitchRange = FCString::Atof(*Args[1]);
			float YawRange = FCString::Atof(*Args[2]);

			int32 PrimaryCameraID = CurrentConfig.SceneParams.CameraID;
			UFusionCamSensor* PrimaryCam = USensorBPLib::GetSensorById(PrimaryCameraID);
			if (!IsValid(PrimaryCam))
			{
				UE_LOG(LogUnrealCV, Error, TEXT("add_camera_rotation_noise: Primary camera not found"));
				TransitionToState(EDatasetGenerationState::Error);
				return;
			}

			FRotator CurrentRotation = PrimaryCam->GetSensorRotation();
			float NoisePitch = FMath::RandRange(-PitchRange, PitchRange);
			float NoiseYaw = FMath::RandRange(-YawRange, YawRange);
			float NoiseRoll = FMath::RandRange(-RollRange, RollRange);
			FRotator NoisyRotation = CurrentRotation + FRotator(NoisePitch, NoiseYaw, NoiseRoll);
			PrimaryCam->SetSensorRotation(NoisyRotation);

			UE_LOG(LogUnrealCV, Log, TEXT("add_camera_rotation_noise: Applied Pitch=%.2f, Yaw=%.2f, Roll=%.2f"),
				NoisePitch, NoiseYaw, NoiseRoll);
		}
		else
		{
			UE_LOG(LogUnrealCV, Error, TEXT("add_camera_rotation_noise: Invalid arguments, expected 'PitchRange YawRange RollRange'"));
			TransitionToState(EDatasetGenerationState::Error);
			return;
		}
		ExecuteNextCommand();
	}
	else if (Step.Command == TEXT("render_every_frame"))
	{
		TArray<FString> Args;
		Step.StringParam.ParseIntoArray(Args, TEXT(" "));
		if (Args.Num() >= 0)
		{
			bool positive;
			if (Args[0] == "false")
			{
				positive = false;
			}
			else if (Args[0] == "true")
			{
				positive = true;
			}
			else
			{
				UE_LOG(LogUnrealCV, Error, TEXT("render_every_frame bad arg %s"), *Args[0]);
				TransitionToState(EDatasetGenerationState::Error);
				return;
			}

			int32 CameraID = CurrentConfig.SceneParams.CameraID;
			UFusionCamSensor* Sensor = USensorBPLib::GetSensorById(CameraID);
			Sensor->GetMovieQualityRenderer()->bRenderEveryFrame = positive;
		}
		else
		{
			UE_LOG(LogUnrealCV, Error, TEXT("render_every_frame bad arg "));
			TransitionToState(EDatasetGenerationState::Error);
			return;
		}
		ExecuteNextCommand();
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
		// Sensor->GetMovieQualityRenderer()->bRenderEveryFrame = true;
		// Sensor->GetMovieQualityRenderer()->NumWarmup = 1;

		FString PrimaryCameraID = USensorBPLib::GetSensorNewFormatID(Sensor);
		ActiveCameraPool.Empty();
		ActiveCameraPool.Add(PrimaryCameraID);
		UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Prepared_record, primary camera is : %s"), *PrimaryCameraID);

		// NOTE: ChosenRes and ChosenFOV are now set by random_fov/random_resolution commands
		// before prepare_record. See command sequence examples in cmd.md.
		//
		// if (TaskName == "Trajectory" || TaskName == "Matting")
		// {
		// 	CurrentStatus.ChosenRes = {1920, 1080};
		// 	CurrentStatus.ChosenFOV = FMath::RandRange(40.0f, 55.0f);
		// }
		// else if (TaskName == "Omnimatte")
		// {
		// 	const static TArray<FIntPoint> Resolutions = {
		// 		FIntPoint(640, 480),
		// 		FIntPoint(480, 640),
		// 	};
		// 	CurrentStatus.ChosenRes = Resolutions[FMath::RandRange(0, Resolutions.Num() - 1)];
		// 	CurrentStatus.ChosenFOV = FMath::RandRange(40.0f, 55.0f);
		// }
		// else if (TaskName == "SpeedTest")
		// {
		// 	CurrentStatus.ChosenRes = {1920, 1080};
		// 	CurrentStatus.ChosenFOV = 60.0f;
		// }
		// else
		// {
		// 	UE_LOG(LogUnrealCV, Error, TEXT("Invalid task name '%s'."), *TaskName);
		// 	CurrentStatus.ErrorMessage = FString::Printf(TEXT("Invalide TaskName: %s"), *TaskName);
		// 	TransitionToState(EDatasetGenerationState::Error);
		// }

		ExecuteNextCommand();
	}
	else if (Step.Command == TEXT("record_trajectory"))
	{

		UE_LOG(LogTemp, Warning, TEXT("-1"));
		FString TrajectoryType = Step.StringParam;
		FString OutputPath = GenerateOutputPath(CurrentSceneID, TrajectoryType);

		UE_LOG(LogTemp, Warning, TEXT("-1"));
		bool RecordingStarted = StartTrajectoryRecording(
			OutputPath,
			TrajectoryType
		);

		UE_LOG(LogTemp, Warning, TEXT("-1"));
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
		
		float TriggerIndex = FCString::Atof(*Step.StringParam);
		UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Special wait for trajectory index: %f, current pri cam traj index: %d"), TriggerIndex, PriCaptureActor->GetCurrentTrajectoryIndex());
		if (PriCaptureActor->GetCurrentTrajectoryIndex() >= static_cast<int32>(TriggerIndex))
		{
			UE_LOG(LogUnrealCV, Warning, TEXT("DatasetAutomation: Special wait condition met, execute next command"));
			FString SyncFrameOutputPath = FString::Printf(TEXT("%s/%s/SyncFrameNum.txt"), *CurrentConfig.OutputDirectory, *CurrentSceneID);
			// write the sync frame number to the file
			FFileHelper::SaveStringToFile(FString::Printf(TEXT("%d"), PriCaptureActor->GetCurrentTrajectoryIndex()), *SyncFrameOutputPath);
			ExecuteNextCommand();
		}
		// wait for next call
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
	else if (Step.Command == TEXT("load_level"))
	{
		FString LevelName = Step.StringParam;
		if (LevelName.IsEmpty())
		{
			UE_LOG(LogUnrealCV, Error, TEXT("DatasetAutomation: load_level requires level name"));
			TransitionToState(EDatasetGenerationState::Error);
			return;
		}

		FUnrealcvServer::Get().WorldController->OpenLevel(FName(*LevelName));
		UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Loading level: %s"), *LevelName);
		ExecuteNextCommand();
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
		DelayDuration = FCString::Atof(*Step.StringParam);
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
			if (CurrentConfig.bExitOnComplete)
			{
				UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: bExitOnComplete is true, exiting process..."));
				StopBatchGeneration();
				FGenericPlatformMisc::RequestExit(true);
			}
		}
		else
		{
			CurrentCommandIndex = -1;
			BuildCommandSequenceForScene();
			ExecuteNextCommand();
		}
	}
	else if (Step.Command == TEXT("set_time_dilation"))
	{
		auto* World = FUnrealcvServer::Get().GetGameWorld();
		if (IsValid(World))
		{
			float Dilation = FCString::Atof(*Step.StringParam);
			World->GetWorldSettings()->SetTimeDilation(Dilation);
			UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Set time dilation to %.2f"), Dilation);
		}
		else
		{
			UE_LOG(LogUnrealCV, Error, TEXT("DatasetAutomation: Failed to set time dilation, world is invalid"));
		}
		ExecuteNextCommand();
	}
	else if (Step.Command == TEXT("sync_secondary_cameras"))
	{
		bool AllFinished = true;
		for (const FString& CID : ActiveCameraPool)
		{
			if (USensorBPLib::GetIndexByAnyID(CID) == CurrentConfig.SceneParams.CameraID)
			{
				continue;
			}
			if (URecordingBPLib::IsRecording(CID))
			{
				AllFinished = false;
				break;
			}
		}
		
		if (AllFinished)
		{
			UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: All secondary cameras finished recording"));
			ExecuteNextCommand();
		}
		else
		{
			UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Waiting for secondary cameras to finish recording"));
		}
	}
	else if (Step.Command == TEXT("sync_all_cameras"))
	{
		UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Synchronizing all cameras..."));
		bool AllCamerasIdle = AreAllCamerasIdle();
		if (AllCamerasIdle)
		{
			UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: All cameras are idle, proceeding to next command"));
			ExecuteNextCommand();
		}
		else
		{
			UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Waiting for all cameras to finish recording"));
		}
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
	else if (Step.Command == TEXT("set_animation_bp"))
	{
		FString AnimBPPath = Step.StringParam;
		if (AnimBPPath.IsEmpty())
		{
			UE_LOG(LogUnrealCV, Error, TEXT("DatasetAutomation: set_animation_bp requires AnimBP path"));
			TransitionToState(EDatasetGenerationState::Error);
			return;
		}

		if (!IsValid(CurrentScene.ForegroundActor))
		{
			UE_LOG(LogUnrealCV, Error, TEXT("DatasetAutomation: set_animation_bp - ForegroundActor is null"));
			TransitionToState(EDatasetGenerationState::Error);
			return;
		}

		bool bSuccess = UAnimationBPLib::SetActorAnimationBlueprint(CurrentScene.ForegroundActor, AnimBPPath);
		if (!bSuccess)
		{
			CurrentStatus.ErrorMessage = FString::Printf(TEXT("Failed to set animation blueprint '%s' for ForegroundActor"), *AnimBPPath);
			TransitionToState(EDatasetGenerationState::Error);
			UE_LOG(LogUnrealCV, Error, TEXT("DatasetAutomation: %s"), *CurrentStatus.ErrorMessage);
			return;
		}
		UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Set animation blueprint '%s' for ForegroundActor"), *AnimBPPath);
		ExecuteNextCommand();
	}
	else if (Step.Command == TEXT("set_animation_seq"))
	{
		FString SequencePath = Step.StringParam;
		if (SequencePath.IsEmpty())
		{
			UE_LOG(LogUnrealCV, Error, TEXT("DatasetAutomation: set_animation_seq requires SequencePath"));
			TransitionToState(EDatasetGenerationState::Error);
			return;
		}

		if (!IsValid(CurrentScene.ForegroundActor))
		{
			UE_LOG(LogUnrealCV, Error, TEXT("DatasetAutomation: set_animation_seq - ForegroundActor is null"));
			TransitionToState(EDatasetGenerationState::Error);
			return;
		}

		bool bSuccess = UAnimationBPLib::SetActorAnimationSequence(CurrentScene.ForegroundActor, SequencePath);
		if (!bSuccess)
		{
			CurrentStatus.ErrorMessage = FString::Printf(TEXT("Failed to set animation sequence '%s' for ForegroundActor"), *SequencePath);
			TransitionToState(EDatasetGenerationState::Error);
			UE_LOG(LogUnrealCV, Error, TEXT("DatasetAutomation: %s"), *CurrentStatus.ErrorMessage);
			return;
		}
		UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Set animation sequence '%s' for ForegroundActor (looping)"), *SequencePath);
		ExecuteNextCommand();
	}
	else if (Step.Command == TEXT("prepare_groom"))
	{
		if (!IsValid(CurrentScene.ForegroundActor))
		{
			UE_LOG(LogUnrealCV, Error, TEXT("DatasetAutomation: prepare_groom - ForegroundActor is null"));
			TransitionToState(EDatasetGenerationState::Error);
			return;
		}

		if (TaskName == TEXT("Matting"))
		{
			// UGroomBPLib::SetHairAirVelocity(CurrentScene.ForegroundActor, FVector(0, 0, 200));
			UGroomBPLib::SetHairBendDamping(CurrentScene.ForegroundActor, 0.000001f);
			UGroomBPLib::SetHairBendStiffness(CurrentScene.ForegroundActor, 0.01f);
			// UGroomBPLib::SetHairAirDrag(CurrentScene.ForegroundActor, 0.5f);
			UGroomBPLib::SetHairGravity(CurrentScene.ForegroundActor, {0.0f, 0.0f, 100.0f});
			UGroomBPLib::SetHairStrandsViscosity(CurrentScene.ForegroundActor, 0.4f);
			UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Prepared groom for Matting (AirVelocity=(0,0,200), BendDamping=0, BendStiffness=0.1, AirDrag=0.5)"));
		}
		else
		{
			UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Skipped groom preparation for task '%s'"), *TaskName);
		}
		ExecuteNextCommand();
	}
	else if (Step.Command == TEXT("load_random_level_every_n_scenes"))
	{
		int32 N = FCString::Atoi(*Step.StringParam);
		if (N <= 0)
		{
			UE_LOG(LogUnrealCV, Error, TEXT("DatasetAutomation: load_random_level_every_n_scenes requires N > 0"));
			TransitionToState(EDatasetGenerationState::Error);
			return;
		}

		if (CurrentSceneCounter % N == 0 && CurrentSceneCounter > 0)
		{
			FString JsonFilePath = USceneCompositionBPLib::GetSceneConfigFilePath();
			FString JsonFileContent;
			if (!FFileHelper::LoadFileToString(JsonFileContent, *JsonFilePath))
			{
				UE_LOG(LogUnrealCV, Error, TEXT("DatasetAutomation: Failed to read JSON file '%s'"), *JsonFilePath);
				TransitionToState(EDatasetGenerationState::Error);
				return;
			}

			TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonFileContent);
			if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
			{
				UE_LOG(LogUnrealCV, Error, TEXT("DatasetAutomation: Failed to parse JSON from '%s'"), *JsonFilePath);
				TransitionToState(EDatasetGenerationState::Error);
				return;
			}

			TArray<FString> EnabledLevels;
			for (const auto& Pair : JsonObject->Values)
			{
				if (Pair.Value->Type == EJson::Object)
				{
					TSharedPtr<FJsonObject> LevelConfig = Pair.Value->AsObject();
					bool bEnabled = false;
					if (LevelConfig->TryGetBoolField(TEXT("Enabled"), bEnabled) && bEnabled)
					{
						EnabledLevels.Add(Pair.Key);
					}
				}
			}

			if (EnabledLevels.Num() == 0)
			{
				UE_LOG(LogUnrealCV, Error, TEXT("DatasetAutomation: No enabled levels found in JSON"));
				TransitionToState(EDatasetGenerationState::Error);
				return;
			}

			FString CurrentMapPath = WorldContext->GetMapName();
			FString CurrentMapName = FJsonConfigHelper::ExtractMapNameFromPath(CurrentMapPath);

			TArray<FString> AvailableLevels;
			for (const FString& LevelName : EnabledLevels)
			{
				if (CurrentMapName.Find(LevelName) == INDEX_NONE)
				{
					AvailableLevels.Add(LevelName);
				}
			}

			if (AvailableLevels.Num() == 0)
			{
				UE_LOG(LogUnrealCV, Error, TEXT("DatasetAutomation: No other levels available, staying on current level"));
				ExecuteNextCommand();
				return;
			}

			int32 RandomIndex = FMath::RandRange(0, AvailableLevels.Num() - 1);
			FString SelectedLevel = AvailableLevels[RandomIndex];

			UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Loading random level: %s (scene %d, every %d scenes)"),
				*SelectedLevel, CurrentSceneCounter, N);

			FString PreviousLevelName = CurrentMapName;
			FUnrealcvServer::Get().WorldController->OpenLevel(FName(*SelectedLevel));

			FString NewMapPath = WorldContext->GetMapName();
			FString NewMapName = FJsonConfigHelper::ExtractMapNameFromPath(NewMapPath);

			if (NewMapName == PreviousLevelName)
			{
				UE_LOG(LogUnrealCV, Error, TEXT("DatasetAutomation: OpenLevel failed - Level name unchanged: %s"), *NewMapName);
			}

			if (NewMapName.Find(SelectedLevel) == INDEX_NONE)
			{
				UE_LOG(LogUnrealCV, Error, TEXT("DatasetAutomation: OpenLevel failed - New level '%s' does not contain target name '%s'"), *NewMapName, *SelectedLevel);
			}

			ExecuteNextCommand();
		}
		else
		{
			UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Skipping level load (scene %d, trigger every %d scenes)"),
				CurrentSceneCounter, N);
			ExecuteNextCommand();
		}
	}
	else if (Step.Command == TEXT("sync_pawn_to_primary_camera"))
	{
		int32 PrimaryCameraID = CurrentConfig.SceneParams.CameraID;
		UFusionCamSensor* PrimaryCam = USensorBPLib::GetSensorById(PrimaryCameraID);
		if (!IsValid(PrimaryCam))
		{
			UE_LOG(LogUnrealCV, Error, TEXT("sync_pawn_to_primary_camera: Failed to get primary camera %d"), PrimaryCameraID);
			TransitionToState(EDatasetGenerationState::Error);
			return;
		}

		UWorld* World = FUnrealcvServer::Get().GetGameWorld();
		if (!World)
		{
			UE_LOG(LogUnrealCV, Error, TEXT("sync_pawn_to_primary_camera: Failed to get world"));
			TransitionToState(EDatasetGenerationState::Error);
			return;
		}

		APlayerController* PlayerController = World->GetFirstPlayerController();
		if (!IsValid(PlayerController))
		{
			UE_LOG(LogUnrealCV, Error, TEXT("sync_pawn_to_primary_camera: Failed to get PlayerController"));
			TransitionToState(EDatasetGenerationState::Error);
			return;
		}

		APawn* Pawn = PlayerController->GetPawn();
		if (!IsValid(Pawn))
		{
			UE_LOG(LogUnrealCV, Error, TEXT("sync_pawn_to_primary_camera: Failed to get Pawn"));
			TransitionToState(EDatasetGenerationState::Error);
			return;
		}

		FVector CameraLocation = PrimaryCam->GetSensorLocation();
		FRotator CameraRotation = PrimaryCam->GetSensorRotation();

		Pawn->SetActorLocation(CameraLocation, false, nullptr, ETeleportType::TeleportPhysics);
		PlayerController->ClientSetRotation(CameraRotation);

		UE_LOG(LogUnrealCV, Log, TEXT("sync_pawn_to_primary_camera: Synced Pawn to camera %d at location (%.2f, %.2f, %.2f) rotation (%.2f, %.2f, %.2f)"),
			PrimaryCameraID, CameraLocation.X, CameraLocation.Y, CameraLocation.Z,
			CameraRotation.Pitch, CameraRotation.Yaw, CameraRotation.Roll);

		ExecuteNextCommand();
	}
	else if (Step.Command == TEXT("vrun"))
	{
		FString Command = Step.StringParam;
		UE_LOG(LogUnrealCV, Log, TEXT("Executing console command: %s"), *Command);
		UWorld* World = FUnrealcvServer::Get().GetGameWorld();
		if (World && World->GetFirstPlayerController())
		{
			FString Result = World->GetFirstPlayerController()->ConsoleCommand(Command, true);
			if (!Result.IsEmpty())
			{
				UE_LOG(LogUnrealCV, Log, TEXT("Command result: %s"), *Result);
			}
			else
			{
				UE_LOG(LogUnrealCV, Log, TEXT("Command executed (no result)"));
			}
		}
		else
		{
			UE_LOG(LogUnrealCV, Error, TEXT("Failed to get World or PlayerController"));
		}
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
	UObject* WorldContextObject)
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

	// CurrentConfig = Config;
	CurrentStatus = FAutomationStatus();
	CurrentStatus.TotalScenes = CurrentConfig.TotalScenes;
	CurrentStatus.CurrentSceneIndex = 0;

	ActiveCameraPool.Empty();
	// CameraRecordingState.Empty();

	BuildCommandSequenceForScene();
	CurrentCommandIndex = -1;
	CurrentSceneCounter = 0;

	if (!TickableObject)
	{
		TickableObject = new FGenericTickableObject();
		TickableObject->SetTickCallback(&UDatasetAutomationBPLib::OnTick);
	}
	TickableObject->Activate();

	UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Started batch generation (%d scenes)"), CurrentConfig.TotalScenes);

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
		UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: ProcessState: Executing command %d/%d: %s"),
			CurrentCommandIndex, CommandQueue.Num(), *Step.Command);
		ExecuteCommand(Step);
		break;
	}

	case EDatasetGenerationState::WaitingAsync:
	{
		double CurrentRealTime = FPlatformTime::Seconds();
		double ElapsedRealTime = CurrentRealTime - DelayStartTime;
		bool DelayComplete = (ElapsedRealTime >= DelayDuration);

		UE_LOG(LogUnrealCV, Warning, TEXT("DatasetAutomation: Waiting (%.2fs/%.2fs real time)"), ElapsedRealTime, DelayDuration);

		if (DelayComplete)
		{
			// if (IsValid(CurrentScene.NavController) && CurrentScene.NavController->IsNavigating())
			// {
			// 	CurrentScene.NavController->StopNavigation();
			// }

			ExecuteNextCommand();
		}
		break;
	}

	case EDatasetGenerationState::Error:
		break;
	case EDatasetGenerationState::Completed:
		break;
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
	UE_LOG(LogTemp, Warning, TEXT("1"));
	AActor* Target = CurrentScene.ForegroundActor;
	int32 FPS = CurrentConfig.TrajectoryFPS;
	int32 NumFrames = CurrentConfig.NumFrames;
	int32 RandomSeed = -1;

	if (!IsValid(Target))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("StartTrajectoryRecording: Target actor is null"));
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("1"));
	int32 AllocatedCID = USensorBPLib::GetIndexByAnyID(GetIdleCamera());
	auto* AllocatedCam = USensorBPLib::GetSensorById(AllocatedCID);
	check(AllocatedCam);
	check(AllocatedCID >= 0);

	UE_LOG(LogTemp, Warning, TEXT("1"));
	AFusionCamCaptureActor* CaptureActor = URecordingBPLib::PrepareRecording(AllocatedCID);
	if (!IsValid(CaptureActor))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("StartTrajectoryRecording: Failed to prepare recording for camera %d"), AllocatedCID);
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("1"));
	// Crucial
	CaptureActor->SetSceneHandle(CurrentScene);



	UE_LOG(LogTemp, Warning, TEXT("1"));
	int32 PrimaryCameraID = CurrentConfig.SceneParams.CameraID;
	if (AllocatedCID != PrimaryCameraID)
	{
		auto* PrimaryCam = USensorBPLib::GetSensorById(PrimaryCameraID);
		check(PrimaryCam);
		AllocatedCam->SetSensorLocation(PrimaryCam->GetSensorLocation());
		AllocatedCam->SetSensorRotation(PrimaryCam->GetSensorRotation());
	}

	CaptureActor->TargetHeightOffset = CurrentStatus.RandomTargetHeight;

	CaptureActor->ApplyRecordingConfig(CurrentConfig.RecordingConfig);

	AllocatedCam->SetFilmSize(CurrentStatus.ChosenRes.X, CurrentStatus.ChosenRes.Y);
	AllocatedCam->SetSensorFOV(CurrentStatus.ChosenFOV);

	UE_LOG(LogTemp, Warning, TEXT("4"));

	ECameraTrajectoryType TrajectoryEnum;
	if (!URecordingBPLib::ParseTrajectoryType(TrajectoryType, TrajectoryEnum))
	{
		return false;
	}

	UE_LOG(LogUnrealCV, Log, TEXT("StartTrajectoryRecording: Camera %d, File: %s, Type: %s, FPS: %d, NumFrames: %d, Target: %s"),
		AllocatedCID, *FileName, *TrajectoryType, FPS, NumFrames, *Target->GetName());
	check(Target);
	check(FPS > 0);
	check(NumFrames > 0);

	// FixMe: while mult-cam recording, the forground offset can be tick multiple times
	CaptureActor->ForegroundMoveSpeed = CurrentConfig.ForegroundMoveSpeed;
	CaptureActor->ForegroundMoveAngleOffset = CurrentConfig.ForegroundMoveAngleOffset;

	CaptureActor->StartTrajectoryRecord(FileName, TrajectoryEnum, Target, FPS, NumFrames, RandomSeed, false);
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

bool UDatasetAutomationBPLib::SetTaskName(const FString& InTaskName)
{
	// static const TSet<FString> ValidTaskNames = { TEXT("Trajectory"), TEXT("Omnimatte") };

	// if (!ValidTaskNames.Contains(InTaskName))
	// {
	// 	UE_LOG(LogUnrealCV, Error, TEXT("SetTaskName: Invalid task name '%s'. Must be 'Trajectory' or 'Omnimatte'"), *InTaskName);
	// 	return false;
	// }

	TaskName = InTaskName;
	UE_LOG(LogUnrealCV, Log, TEXT("SetTaskName: Task name set to '%s'"), *TaskName);
	return true;
}

FString UDatasetAutomationBPLib::GetTaskName()
{
	return TaskName;
}

void UDatasetAutomationBPLib::SetRecordingConfig(const FRecordingDataTypesConfig& InConfig)
{
	CurrentConfig.RecordingConfig = InConfig;
}

FRecordingDataTypesConfig UDatasetAutomationBPLib::GetRecordingConfig()
{
	return CurrentConfig.RecordingConfig;
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

bool UDatasetAutomationBPLib::ParseVector3D(const FString& Str, FVector& OutVector)
{
	TArray<FString> Parts;
	Str.ParseIntoArray(Parts, TEXT(","));
	if (Parts.Num() != 3)
	{
		return false;
	}

	float X, Y, Z;
	if (!LexTryParseString(X, *Parts[0]) ||
		!LexTryParseString(Y, *Parts[1]) ||
		!LexTryParseString(Z, *Parts[2]))
	{
		return false;
	}

	OutVector = FVector(X, Y, Z);
	return true;
}

bool UDatasetAutomationBPLib::SetExternalCommandSequence(const TArray<FAutomationStep>& Sequence)
{
	ExternalCommandQueue = Sequence;
	UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Set external command sequence with %d commands"), ExternalCommandQueue.Num());
	return true;
}

bool UDatasetAutomationBPLib::ParseCommandSequenceJson(const FString& JsonContent, FString& OutErrorMessage)
{
	OutErrorMessage.Empty();
	ExternalCommandQueue.Empty();

	TSharedPtr<FJsonObject> JsonRoot;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);

	if (!FJsonSerializer::Deserialize(Reader, JsonRoot) || !JsonRoot.IsValid())
	{
		OutErrorMessage = TEXT("Invalid JSON format");
		UE_LOG(LogUnrealCV, Error, TEXT("DatasetAutomation: Invalid JSON format"));
		return false;
	}

	if (JsonRoot->HasField(TEXT("task")))
	{
		FString TaskStr = JsonRoot->GetStringField(TEXT("task"));
		TaskName = TaskStr;
		UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Parsed task name from JSON: %s"), *TaskName);
	}

	if (!JsonRoot->HasField(TEXT("commands")))
	{
		OutErrorMessage = TEXT("Missing 'commands' field");
		UE_LOG(LogUnrealCV, Error, TEXT("DatasetAutomation: Missing 'commands' field"));
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> Commands = JsonRoot->GetArrayField(TEXT("commands"));

	for (const auto& CmdValue : Commands)
	{
		if (CmdValue->Type != EJson::Object)
		{
			OutErrorMessage = TEXT("Command element is not an object");
			UE_LOG(LogUnrealCV, Error, TEXT("DatasetAutomation: Command element is not an object"));
			return false;
		}

		TSharedPtr<FJsonObject> CmdJson = CmdValue->AsObject();

		if (!CmdJson->HasField(TEXT("cmd")))
		{
			OutErrorMessage = TEXT("Command object missing 'cmd' field");
			UE_LOG(LogUnrealCV, Error, TEXT("DatasetAutomation: Command object missing 'cmd' field"));
			return false;
		}

		FString CmdName = CmdJson->GetStringField(TEXT("cmd"));
		FString Params = CmdJson->HasField(TEXT("params")) ? CmdJson->GetStringField(TEXT("params")) : TEXT("");

		ExternalCommandQueue.Add(FAutomationStep(CmdName, Params));
	}

	UE_LOG(LogUnrealCV, Log, TEXT("DatasetAutomation: Parsed %d commands from JSON"), ExternalCommandQueue.Num());
	return true;
}

FString UDatasetAutomationBPLib::GetCommandQueueSummary()
{
	const TArray<FAutomationStep>& Queue = ExternalCommandQueue;
	FString Summary = FString::Printf(TEXT("Queue: %d commands (external)\n"), Queue.Num());

	for (int32 i = 0; i < Queue.Num(); i++)
	{
		const FAutomationStep& Step = Queue[i];
		if (!Step.StringParam.IsEmpty())
		{
			Summary += FString::Printf(TEXT("%d: %s %s\n"), i, *Step.Command, *Step.StringParam);
		}
		else
		{
			Summary += FString::Printf(TEXT("%d: %s\n"), i, *Step.Command);
		}
	}
	return Summary;
}

const TArray<FCompletedCommand>& UDatasetAutomationBPLib::GetCommandHistory()
{
	return CommandHistory;
}

