#include "DatasetAutomationHandler.h"
#include "DatasetAutomationBPLib.h"
#include "SensorBPLib.h"
#include "Actor/FusionCamCaptureActor.h"
#include "Utils/UObjectUtils.h"
#include "UnrealcvLog.h"

void FDatasetAutomationHandler::RegisterCommands()
{
	FDispatcherDelegate Cmd;
	FString Help;

	Cmd = FDispatcherDelegate::CreateRaw(this, &FDatasetAutomationHandler::GetTaskName);
	Help = "Get current task name (Trajectory or Omnimatte)";
	CommandDispatcher->BindCommand(TEXT("vget /datasetautomation/task_name"), Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FDatasetAutomationHandler::SetTaskName);
	Help = "Set task name (Trajectory or Omnimatte)";
	CommandDispatcher->BindCommand(TEXT("vset /datasetautomation/task_name [str]"), Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FDatasetAutomationHandler::SetSequence);
	Help = "Set command sequence from JSON";
	CommandDispatcher->BindCommand(TEXT("vset /datasetautomation/sequence [Anything]"), Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FDatasetAutomationHandler::GetSequence);
	Help = "Get current command sequence";
	CommandDispatcher->BindCommand(TEXT("vget /datasetautomation/sequence"), Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FDatasetAutomationHandler::StartAutomation);
	Help = "Start automation with current configuration";
	CommandDispatcher->BindCommand(TEXT("vset /datasetautomation/start [str]"), Cmd, Help);
	CommandDispatcher->BindCommand(TEXT("vset /datasetautomation/start"), Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FDatasetAutomationHandler::StopAutomation);
	Help = "Stop automation";
	CommandDispatcher->BindCommand(TEXT("vset /datasetautomation/stop"), Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FDatasetAutomationHandler::GetStatus);
	Help = "Get automation status";
	CommandDispatcher->BindCommand(TEXT("vget /datasetautomation/status"), Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FDatasetAutomationHandler::SetCurrentSceneForegroundActor);
	Help = "Set current scene foreground actor by name";
	CommandDispatcher->BindCommand(TEXT("vset /datasetautomation/currentscene/foreground_actor [str]"), Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FDatasetAutomationHandler::SetCurrentScenePrimaryCamera);
	Help = "Set current scene primary camera ID";
	CommandDispatcher->BindCommand(TEXT("vset /datasetautomation/currentscene/primary_camera [uint]"), Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FDatasetAutomationHandler::SetCurrentSceneSceneCategory);
	Help = "Set current scene category";
	CommandDispatcher->BindCommand(TEXT("vset /datasetautomation/currentscene/scene_category [str]"), Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FDatasetAutomationHandler::SetCurrentSceneForegroundSubcategory);
	Help = "Set current scene foreground subcategory";
	CommandDispatcher->BindCommand(TEXT("vset /datasetautomation/currentscene/foreground_subcategory [str]"), Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FDatasetAutomationHandler::SetCurrentSceneOccluderCategory);
	Help = "Set current scene occluder category";
	CommandDispatcher->BindCommand(TEXT("vset /datasetautomation/currentscene/occluder_category [str]"), Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FDatasetAutomationHandler::SetConfigTotalScenes);
	Help = "Set config total scenes";
	CommandDispatcher->BindCommand(TEXT("vset /datasetautomation/config/total_scenes [uint]"), Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FDatasetAutomationHandler::SetConfigOutputDirectory);
	Help = "Set config output directory";
	CommandDispatcher->BindCommand(TEXT("vset /datasetautomation/config/output_directory"), Cmd, Help);
	CommandDispatcher->BindCommand(TEXT("vset /datasetautomation/config/output_directory [str]"), Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FDatasetAutomationHandler::SetConfigTrajectoryFPS);
	Help = "Set config trajectory FPS";
	CommandDispatcher->BindCommand(TEXT("vset /datasetautomation/config/trajectory_fps [uint]"), Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FDatasetAutomationHandler::SetConfigNumFrames);
	Help = "Set config number of frames for trajectory recording";
	CommandDispatcher->BindCommand(TEXT("vset /datasetautomation/config/num_frames [uint]"), Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FDatasetAutomationHandler::SetConfigBLoadSceneParamsFromJson);
	Help = "Set config bLoadSceneParamsFromJson";
	CommandDispatcher->BindCommand(TEXT("vset /datasetautomation/config/b_load_scene_params_from_json [bool]"), Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FDatasetAutomationHandler::SetConfigForegroundMoveSpeed);
	Help = "Set foreground movement speed in cm/s";
	CommandDispatcher->BindCommand(TEXT("vset /datasetautomation/config/foreground_move_speed [float]"), Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FDatasetAutomationHandler::SetConfigForegroundMoveAngleOffset);
	Help = "Set foreground movement angle offset in degrees (0=forward, 90=right, -90=left, 180=backward)";
	CommandDispatcher->BindCommand(TEXT("vset /datasetautomation/config/foreground_move_angle_offset [float]"), Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FDatasetAutomationHandler::SetConfigRecordingOptions);
	Help = "Set recording data types options (comma-separated): lit/rgb, mask/seg, normal, depth, flow, oneobjmask, oneobjlit, oneobjgroomlit, shadowcatcher, stencilmask, metadata, audio, woTarget";
	Help += "\nExample: vset /datasetautomation/config/recording_options lit,mask,oneobjlit,metadata";
	CommandDispatcher->BindCommand(TEXT("vset /datasetautomation/config/recording_options [str]"), Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FDatasetAutomationHandler::GetCommandHistory);
	Help = "Get command execution history";
	CommandDispatcher->BindCommand(TEXT("vget /datasetautomation/history"), Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FDatasetAutomationHandler::SetConfigExitOnComplete);
	Help = "Set config bExitOnComplete (exit process when batch generation completes)";
	CommandDispatcher->BindCommand(TEXT("vset /datasetautomation/config/b_exit_on_complete [bool]"), Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FDatasetAutomationHandler::GetConfigExitOnComplete);
	Help = "Get config bExitOnComplete";
	CommandDispatcher->BindCommand(TEXT("vget /datasetautomation/config/b_exit_on_complete"), Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FDatasetAutomationHandler::SetForegroundPath);
	Help = "Set foreground actor path (overrides JSON config)";
	CommandDispatcher->BindCommand(TEXT("vset /datasetautomation/foreground_path [str]"), Cmd, Help);
	CommandDispatcher->BindCommand(TEXT("vset /datasetautomation/foreground_path"), Cmd, Help);
}

FExecStatus FDatasetAutomationHandler::GetTaskName(const TArray<FString>& Args)
{
	FString TaskName = UDatasetAutomationBPLib::GetTaskName();
	return FExecStatus::OK(TaskName);
}

FExecStatus FDatasetAutomationHandler::SetTaskName(const TArray<FString>& Args)
{
	if (Args.Num() == 1)
	{
		FString TaskName = Args[0];
		bool Success = UDatasetAutomationBPLib::SetTaskName(TaskName);

		if (Success)
		{
			return FExecStatus::OK(FString::Printf(TEXT("Task name set to '%s'"), *TaskName));
		}
		else
		{
			return FExecStatus::Error(TEXT("Invalid task name."));
		}
	}
	else
	{
		return FExecStatus::Error(TEXT("Expect argument: task_name (Trajectory or Omnimatte)"));
	}
}

FExecStatus FDatasetAutomationHandler::SetSequence(const TArray<FString>& Args)
{
	if (Args.Num() != 1)
	{
		return FExecStatus::Error(TEXT("Expect JSON string argument"));
	}

	FString ErrorMessage;
	bool Success = UDatasetAutomationBPLib::ParseCommandSequenceJson(Args[0], ErrorMessage);

	if (Success)
	{
		return FExecStatus::OK(TEXT("Command sequence parsed successfully"));
	}
	else
	{
		return FExecStatus::Error(FString::Printf(TEXT("Failed to parse JSON: %s"), *ErrorMessage));
	}
}

FExecStatus FDatasetAutomationHandler::GetSequence(const TArray<FString>& Args)
{
	FString Summary = UDatasetAutomationBPLib::GetCommandQueueSummary();
	return FExecStatus::OK(Summary);
}

FExecStatus FDatasetAutomationHandler::StartAutomation(const TArray<FString>& Args)
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return FExecStatus::Error(TEXT("Invalid world context"));
	}

	bool Success = UDatasetAutomationBPLib::StartBatchGeneration(World);
	if (Success)
	{
		return FExecStatus::OK(FString::Printf(TEXT("Started: %d scenes to %s"),
			UDatasetAutomationBPLib::CurrentConfig.TotalScenes,
			*UDatasetAutomationBPLib::CurrentConfig.OutputDirectory));
	}
	else
	{
		return FExecStatus::Error(TEXT("Failed to start automation"));
	}
}

FExecStatus FDatasetAutomationHandler::StopAutomation(const TArray<FString>& Args)
{
	UDatasetAutomationBPLib::StopBatchGeneration();
	return FExecStatus::OK(TEXT("Automation stopped"));
}

FExecStatus FDatasetAutomationHandler::GetStatus(const TArray<FString>& Args)
{
	FString Status = UDatasetAutomationBPLib::GetAutomationStatusString();
	return FExecStatus::OK(Status);
}

FExecStatus FDatasetAutomationHandler::SetCurrentSceneForegroundActor(const TArray<FString>& Args)
{
	if (Args.Num() != 1)
	{
		return FExecStatus::Error(TEXT("Usage: vset /datasetautomation/currentscene/foreground_actor [ActorName]"));
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return FExecStatus::Error(TEXT("Invalid world context"));
	}

	AActor* Actor = GetActorById(World, Args[0]);
	if (!IsValid(Actor))
	{
		return FExecStatus::Error(FString::Printf(TEXT("Actor not found: %s"), *Args[0]));
	}

	UDatasetAutomationBPLib::CurrentScene.ForegroundActor = Actor;
	UDatasetAutomationBPLib::CurrentScene.ForegroundObjectMetadata = {};

	return FExecStatus::OK(FString::Printf(TEXT("CurrentScene.ForegroundActor = %s"), *Args[0]));
}

FExecStatus FDatasetAutomationHandler::SetCurrentScenePrimaryCamera(const TArray<FString>& Args)
{
	if (Args.Num() != 1)
	{
		return FExecStatus::Error(TEXT("Usage: vset /datasetautomation/currentscene/primary_camera [CameraID]"));
	}

	int32 CameraID = FCString::Atoi(*Args[0]);
	UFusionCamSensor* Camera = USensorBPLib::GetSensorById(CameraID);
	if (!IsValid(Camera))
	{
		return FExecStatus::Error(FString::Printf(TEXT("Camera not found: %d"), CameraID));
	}

	UDatasetAutomationBPLib::CurrentScene.CameraID = CameraID;

	return FExecStatus::OK(FString::Printf(TEXT("CurrentScene.CameraID = %d"), CameraID));
}

FExecStatus FDatasetAutomationHandler::SetCurrentSceneSceneCategory(const TArray<FString>& Args)
{
	if (Args.Num() != 1)
	{
		return FExecStatus::Error(TEXT("Usage: vset /datasetautomation/currentscene/scene_category [Category]"));
	}

	UDatasetAutomationBPLib::CurrentScene.SceneCategory = Args[0];
	return FExecStatus::OK(FString::Printf(TEXT("CurrentScene.SceneCategory = %s"), *Args[0]));
}

FExecStatus FDatasetAutomationHandler::SetCurrentSceneForegroundSubcategory(const TArray<FString>& Args)
{
	if (Args.Num() != 1)
	{
		return FExecStatus::Error(TEXT("Usage: vset /datasetautomation/currentscene/foreground_subcategory [SubCategory]"));
	}

	UDatasetAutomationBPLib::CurrentScene.ForegroundSubcategory = Args[0];
	return FExecStatus::OK(FString::Printf(TEXT("CurrentScene.ForegroundSubcategory = %s"), *Args[0]));
}

FExecStatus FDatasetAutomationHandler::SetCurrentSceneOccluderCategory(const TArray<FString>& Args)
{
	if (Args.Num() != 1)
	{
		return FExecStatus::Error(TEXT("Usage: vset /datasetautomation/currentscene/occluder_category [Category]"));
	}

	UDatasetAutomationBPLib::CurrentScene.OccluderCategory = Args[0];
	return FExecStatus::OK(FString::Printf(TEXT("CurrentScene.OccluderCategory = %s"), *Args[0]));
}

FExecStatus FDatasetAutomationHandler::SetConfigTotalScenes(const TArray<FString>& Args)
{
	if (Args.Num() != 1)
	{
		return FExecStatus::Error(TEXT("Usage: vset /datasetautomation/config/total_scenes [N]"));
	}

	int32 Value = FCString::Atoi(*Args[0]);
	UDatasetAutomationBPLib::CurrentConfig.TotalScenes = Value;
	return FExecStatus::OK(FString::Printf(TEXT("Config.TotalScenes = %d"), Value));
}

FExecStatus FDatasetAutomationHandler::SetConfigOutputDirectory(const TArray<FString>& Args)
{
	FString OutputDirectory;

	if (Args.Num() == 0)
	{
		OutputDirectory = FPaths::ProjectSavedDir() / TEXT("DatasetAutomationOutputDirectory");
	}
	else if (Args.Num() == 1)
	{
		OutputDirectory = Args[0];
	}
	else
	{
		return FExecStatus::Error(TEXT("Usage: vset /datasetautomation/config/output_directory [Path] (Path is optional, defaults to Saved/DatasetAutomationOutputDirectory)"));
	}

	UDatasetAutomationBPLib::CurrentConfig.OutputDirectory = OutputDirectory;
	return FExecStatus::OK(FString::Printf(TEXT("Config.OutputDirectory = %s"), *OutputDirectory));
}

FExecStatus FDatasetAutomationHandler::SetConfigTrajectoryFPS(const TArray<FString>& Args)
{
	if (Args.Num() != 1)
	{
		return FExecStatus::Error(TEXT("Usage: vset /datasetautomation/config/trajectory_fps [FPS]"));
	}

	int32 Value = FCString::Atoi(*Args[0]);
	UDatasetAutomationBPLib::CurrentConfig.TrajectoryFPS = Value;
	return FExecStatus::OK(FString::Printf(TEXT("Config.TrajectoryFPS = %d"), Value));
}

FExecStatus FDatasetAutomationHandler::SetConfigNumFrames(const TArray<FString>& Args)
{
	if (Args.Num() != 1)
	{
		return FExecStatus::Error(TEXT("Usage: vset /datasetautomation/config/num_frames [N]"));
	}

	int32 Value = FCString::Atoi(*Args[0]);
	if (Value <= 0)
	{
		return FExecStatus::Error(TEXT("NumFrames must be greater than 0"));
	}

	UDatasetAutomationBPLib::CurrentConfig.NumFrames = Value;
	return FExecStatus::OK(FString::Printf(TEXT("Config.NumFrames = %d"), Value));
}

FExecStatus FDatasetAutomationHandler::SetConfigBLoadSceneParamsFromJson(const TArray<FString>& Args)
{
	if (Args.Num() != 1)
	{
		return FExecStatus::Error(TEXT("Usage: vset /datasetautomation/config/b_load_scene_params_from_json [true/false]"));
	}

	bool Value = Args[0].ToLower() == TEXT("true") || Args[0] == TEXT("1");
	UDatasetAutomationBPLib::CurrentConfig.bLoadSceneParamsFromJson = Value;
	return FExecStatus::OK(FString::Printf(TEXT("Config.bLoadSceneParamsFromJson = %s"), Value ? TEXT("true") : TEXT("false")));
}

FExecStatus FDatasetAutomationHandler::SetConfigForegroundMoveSpeed(const TArray<FString>& Args)
{
	if (Args.Num() != 1)
	{
		return FExecStatus::Error(TEXT("Usage: vset /datasetautomation/config/foreground_move_speed [Speed]"));
	}

	float Value = FCString::Atof(*Args[0]);
	UDatasetAutomationBPLib::CurrentStatus.ForegroundMoveSpeed = Value;
	return FExecStatus::OK(FString::Printf(TEXT("Status.ForegroundMoveSpeed = %.2f cm/s"), Value));
}

FExecStatus FDatasetAutomationHandler::SetConfigForegroundMoveAngleOffset(const TArray<FString>& Args)
{
	if (Args.Num() != 1)
	{
		return FExecStatus::Error(TEXT("Usage: vset /datasetautomation/config/foreground_move_angle_offset [Angle]"));
	}

	float Value = FCString::Atof(*Args[0]);
	UDatasetAutomationBPLib::CurrentStatus.ForegroundMoveAngleOffset = Value;
	return FExecStatus::OK(FString::Printf(TEXT("Status.ForegroundMoveAngleOffset = %.2f degrees"), Value));
}

FExecStatus FDatasetAutomationHandler::SetConfigRecordingOptions(const TArray<FString>& Args)
{
	if (Args.Num() != 1)
	{
		return FExecStatus::Error(TEXT("Usage: vset /datasetautomation/config/recording_options [OptionsString]"));
	}

	FRecordingDataTypesConfig RecordingConfig = FRecordingDataTypesConfig::ParseRecordingOptions(Args[0]);
	UDatasetAutomationBPLib::CurrentConfig.RecordingConfig = RecordingConfig;

	TArray<FString> EnabledTypes;
	if (RecordingConfig.bRecordRGB) EnabledTypes.Add(TEXT("RGB"));
	if (RecordingConfig.bRecordMask) EnabledTypes.Add(TEXT("Mask"));
	if (RecordingConfig.bRecordDepth) EnabledTypes.Add(TEXT("Depth"));
	if (RecordingConfig.bRecordNormal) EnabledTypes.Add(TEXT("Normal"));
	if (RecordingConfig.bRecordFlow) EnabledTypes.Add(TEXT("Flow"));
	if (RecordingConfig.bRecordOneObjectMask) EnabledTypes.Add(TEXT("OneObjMask"));
	if (RecordingConfig.bRecordOneObjectLit) EnabledTypes.Add(TEXT("OneObjLit"));
	if (RecordingConfig.bRecordOneObjectGroomLit) EnabledTypes.Add(TEXT("OneObjGroomLit"));
	if (RecordingConfig.bRecordShadowCatcher) EnabledTypes.Add(TEXT("ShadowCatcher"));
	if (RecordingConfig.bRecordStencilMask) EnabledTypes.Add(TEXT("StencilMask"));
	if (RecordingConfig.bRecordMetadata) EnabledTypes.Add(TEXT("Metadata"));
	if (RecordingConfig.bRecordAudio) EnabledTypes.Add(TEXT("Audio"));
	if (RecordingConfig.bRecordWithoutTarget) EnabledTypes.Add(TEXT("WithoutTarget"));

	FString Summary = FString::Join(EnabledTypes, TEXT(", "));
	return FExecStatus::OK(FString::Printf(TEXT("Config.RecordingOptions set to: %s"), *Summary));
}

FExecStatus FDatasetAutomationHandler::GetCommandHistory(const TArray<FString>& Args)
{
	const auto& History = UDatasetAutomationBPLib::GetCommandHistory();
	TArray<FString> Results;

	for (int32 i = 0; i < History.Num(); i++)
	{
		const auto& Cmd = History[i];
		FString Line = FString::Printf(TEXT("[%d] %s %s %s"),
			i,
			*Cmd.Timestamp.ToString(),
			*Cmd.Step.Command,
			*Cmd.Step.StringParam);
		Results.Add(Line);
	}

	return FExecStatus::OK(FString::Join(Results, TEXT("\n")));
}

FExecStatus FDatasetAutomationHandler::SetConfigExitOnComplete(const TArray<FString>& Args)
{
	if (Args.Num() != 1)
	{
		return FExecStatus::Error(TEXT("Usage: vset /datasetautomation/config/b_exit_on_complete [true/false]"));
	}

	bool Value = Args[0].ToLower() == TEXT("true") || Args[0] == TEXT("1");
	UDatasetAutomationBPLib::CurrentConfig.bExitOnComplete = Value;
	return FExecStatus::OK(FString::Printf(TEXT("Config.bExitOnComplete = %s"), Value ? TEXT("true") : TEXT("false")));
}

FExecStatus FDatasetAutomationHandler::GetConfigExitOnComplete(const TArray<FString>& Args)
{
	bool Value = UDatasetAutomationBPLib::CurrentConfig.bExitOnComplete;
	return FExecStatus::OK(Value ? TEXT("true") : TEXT("false"));
}

FExecStatus FDatasetAutomationHandler::SetForegroundPath(const TArray<FString>& Args)
{
	FString ForegroundPath;
	if (Args.Num() == 0)
	{
		ForegroundPath = TEXT("");
	}
	else if (Args.Num() == 1)
	{
		ForegroundPath = Args[0];
	}
	else
	{
		return FExecStatus::Error(TEXT("Usage: vset /datasetautomation/foreground_path [Path]"));
	}

	UDatasetAutomationBPLib::CurrentConfig.SceneParams.ForegroundPathSpec = ForegroundPath;
	if (ForegroundPath.IsEmpty())
	{
		return FExecStatus::OK(TEXT("Config.SceneParams.ForegroundPathSpec cleared"));
	}
	else
	{
		return FExecStatus::OK(FString::Printf(TEXT("Config.SceneParams.ForegroundPathSpec = %s"), *ForegroundPath));
	}
}
