#include "CaptureActorHandler.h"
#include "FusionCamCaptureActor.h"
#include "FusionCamSensor.h"
#include "UnrealcvServer.h"
#include "Utils/UObjectUtils.h"
#include "SL.h"
#include "EngineUtils.h"

void FCaptureActorHandler::RegisterCommands()
{
	FDispatcherDelegate Cmd;
	FString Help;

	Cmd = FDispatcherDelegate::CreateRaw(this, &FCaptureActorHandler::ListCaptureActors);
	Help = "List all FusionCamCaptureActors in the scene";
	CommandDispatcher->BindCommand("vget /captureactor/list", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FCaptureActorHandler::StartRecord);
	Help = "Start recording: vset /captureactor/[uint]/start_record [str:filename] [float:duration] [int:fps]";
	CommandDispatcher->BindCommand("vset /captureactor/[uint]/start_record [str] [float] [float]", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FCaptureActorHandler::StartRecord);
	Help = "Start recording with target: vset /captureactor/[uint]/start_record [str:filename] [float:duration] [int:fps] [str:target_id]";
	CommandDispatcher->BindCommand("vset /captureactor/[uint]/start_record [str] [float] [float] [str]", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FCaptureActorHandler::StartBulletTimeRecord);
	Help = "Start bullet time recording: vset /captureactor/[uint]/bullet_time_record [str:filename] [float:duration] [int:fps] [str:target_id]";
	CommandDispatcher->BindCommand("vset /captureactor/[uint]/bullet_time_record [str] [float] [float] [str]", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FCaptureActorHandler::StopRecord);
	Help = "Stop recording: vset /captureactor/[uint]/stop_record";
	CommandDispatcher->BindCommand("vset /captureactor/[uint]/stop_record", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FCaptureActorHandler::GetRecordStatus);
	Help = "Get recording status: vget /captureactor/[uint]/status";
	CommandDispatcher->BindCommand("vget /captureactor/[uint]/status", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FCaptureActorHandler::SetTargetSensor);
	Help = "Set target sensor: vset /captureactor/[uint]/target_sensor [uint:sensor_id]";
	CommandDispatcher->BindCommand("vset /captureactor/[uint]/target_sensor [uint]", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FCaptureActorHandler::SetDataFolder);
	Help = "Set output folder: vset /captureactor/[uint]/folder [str:path]";
	CommandDispatcher->BindCommand("vset /captureactor/[uint]/folder [str]", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FCaptureActorHandler::EnableDataType);
	Help = "Enable/disable data type: vset /captureactor/[uint]/enable [str:type] [str:on/off] (types: rgb, mask, depth, normal, flow, audio, metadata)";
	CommandDispatcher->BindCommand("vset /captureactor/[uint]/enable [str] [str]", Cmd, Help);
}

AFusionCamCaptureActor* FCaptureActorHandler::GetCaptureActor(const TArray<FString>& Args, FExecStatus& Status)
{
	if (Args.Num() < 1)
	{
		Status = FExecStatus::Error("Actor ID is required");
		return nullptr;
	}

	int ActorId = FCString::Atoi(*Args[0]);
	UWorld* World = FUnrealcvServer::Get().GetWorld();

	int CurrentId = 0;
	for (TActorIterator<AFusionCamCaptureActor> ActorItr(World); ActorItr; ++ActorItr)
	{
		if (CurrentId == ActorId)
		{
			Status = FExecStatus::OK();
			return *ActorItr;
		}
		CurrentId++;
	}

	Status = FExecStatus::Error(FString::Printf(TEXT("CaptureActor %d not found"), ActorId));
	return nullptr;
}

FExecStatus FCaptureActorHandler::ListCaptureActors(const TArray<FString>& Args)
{
	UWorld* World = FUnrealcvServer::Get().GetWorld();
	TArray<FString> ActorList;

	int ActorId = 0;
	for (TActorIterator<AFusionCamCaptureActor> ActorItr(World); ActorItr; ++ActorItr)
	{
		AFusionCamCaptureActor* Actor = *ActorItr;
		FString ActorInfo = FString::Printf(TEXT("%d: %s (Recording: %s)"),
			ActorId,
			*Actor->GetName(),
			Actor->IsRecording() ? TEXT("Yes") : TEXT("No")
		);
		ActorList.Add(ActorInfo);
		ActorId++;
	}

	if (ActorList.Num() == 0)
	{
		return FExecStatus::OK("No FusionCamCaptureActors found");
	}

	return FExecStatus::OK(FString::Join(ActorList, TEXT("\n")));
}

FExecStatus FCaptureActorHandler::StartRecord(const TArray<FString>& Args)
{
	SL::get().print("FCaptureActorHandler::StartRecord called");

	FExecStatus ExecStatus = FExecStatus::OK();
	AFusionCamCaptureActor* CaptureActor = GetCaptureActor(Args, ExecStatus);
	if (!IsValid(CaptureActor))
	{
		return ExecStatus;
	}

	// Parse arguments: vset /captureactor/{id}/start_record {filename} {duration} {fps} [target_id]
	if (Args.Num() < 4)
	{
		return FExecStatus::Error("Usage: vset /captureactor/{id}/start_record {filename} {duration} {fps} [target_id]");
	}

	FString FileName = Args[1];
	float Duration = FCString::Atof(*Args[2]);
	int32 FPS = FCString::Atoi(*Args[3]);

	AActor* Target = nullptr;
	if (Args.Num() >= 5)
	{
		FString TargetId = Args[4];
		Target = GetActorById(FUnrealcvServer::Get().GetWorld(), TargetId);
		if (!IsValid(Target))
		{
			return FExecStatus::Error(FString::Printf(TEXT("Target actor '%s' not found"), *TargetId));
		}
	}

	// Validate parameters
	if (Duration <= 0 || FPS <= 0)
	{
		return FExecStatus::Error("Duration and FPS must be positive");
	}

	CaptureActor->StartRecord(FileName, Duration, FPS, Target);

	FString ResponseMsg = FString::Printf(
		TEXT("Recording started: %s, Duration: %.2fs, FPS: %d"),
		*FileName, Duration, FPS
	);

	SL::get().printf("FCaptureActorHandler::StartRecord completed: %s", TCHAR_TO_UTF8(*ResponseMsg));
	return FExecStatus::OK(ResponseMsg);
}

FExecStatus FCaptureActorHandler::StartBulletTimeRecord(const TArray<FString>& Args)
{
	SL::get().print("FCaptureActorHandler::StartBulletTimeRecord called");

	FExecStatus ExecStatus = FExecStatus::OK();
	AFusionCamCaptureActor* CaptureActor = GetCaptureActor(Args, ExecStatus);
	if (!IsValid(CaptureActor))
	{
		return ExecStatus;
	}

	// Parse arguments: vset /captureactor/{id}/bullet_time_record {filename} {duration} {fps} {target_id}
	if (Args.Num() < 5)
	{
		return FExecStatus::Error("Usage: vset /captureactor/{id}/bullet_time_record {filename} {duration} {fps} {target_id}");
	}

	FString FileName = Args[1];
	float Duration = FCString::Atof(*Args[2]);
	int32 FPS = FCString::Atoi(*Args[3]);
	FString TargetId = Args[4];

	AActor* Target = GetActorById(FUnrealcvServer::Get().GetWorld(), TargetId);
	if (!IsValid(Target))
	{
		return FExecStatus::Error(FString::Printf(TEXT("Target actor '%s' not found"), *TargetId));
	}

	// Validate parameters
	if (Duration <= 0 || FPS <= 0)
	{
		return FExecStatus::Error("Duration and FPS must be positive");
	}

	CaptureActor->StartBulletTimeRecord(FileName, Duration, FPS, Target);

	FString ResponseMsg = FString::Printf(
		TEXT("Bullet time recording started: %s, Duration: %.2fs, FPS: %d, Target: %s"),
		*FileName, Duration, FPS, *TargetId
	);

	SL::get().printf("FCaptureActorHandler::StartBulletTimeRecord completed: %s", TCHAR_TO_UTF8(*ResponseMsg));
	return FExecStatus::OK(ResponseMsg);
}

FExecStatus FCaptureActorHandler::StopRecord(const TArray<FString>& Args)
{
	FExecStatus ExecStatus = FExecStatus::OK();
	AFusionCamCaptureActor* CaptureActor = GetCaptureActor(Args, ExecStatus);
	if (!IsValid(CaptureActor))
	{
		return ExecStatus;
	}

	if (!CaptureActor->IsRecording())
	{
		return FExecStatus::OK("Not currently recording");
	}

	CaptureActor->StopRecord();
	return FExecStatus::OK("Recording stopped");
}

FExecStatus FCaptureActorHandler::GetRecordStatus(const TArray<FString>& Args)
{
	FExecStatus ExecStatus = FExecStatus::OK();
	AFusionCamCaptureActor* CaptureActor = GetCaptureActor(Args, ExecStatus);
	if (!IsValid(CaptureActor))
	{
		return ExecStatus;
	}

	FString StatusMsg = CaptureActor->IsRecording() ? TEXT("Recording") : TEXT("Stopped");
	return FExecStatus::OK(StatusMsg);
}

FExecStatus FCaptureActorHandler::SetTargetSensor(const TArray<FString>& Args)
{
	FExecStatus ExecStatus = FExecStatus::OK();
	AFusionCamCaptureActor* CaptureActor = GetCaptureActor(Args, ExecStatus);
	if (!IsValid(CaptureActor))
	{
		return ExecStatus;
	}

	if (Args.Num() < 2)
	{
		return FExecStatus::Error("Sensor ID is required");
	}

	int SensorId = FCString::Atoi(*Args[1]);

	// Find the sensor by ID
	UWorld* World = FUnrealcvServer::Get().GetWorld();
	int CurrentId = 0;
	UFusionCamSensor* FoundSensor = nullptr;

	for (TActorIterator<AActor> ActorItr(World); ActorItr; ++ActorItr)
	{
		TArray<UFusionCamSensor*> Sensors = UFusionCamSensor::GetComponents(*ActorItr);
		for (UFusionCamSensor* Sensor : Sensors)
		{
			if (CurrentId == SensorId)
			{
				FoundSensor = Sensor;
				break;
			}
			CurrentId++;
		}
		if (FoundSensor) break;
	}

	if (!IsValid(FoundSensor))
	{
		return FExecStatus::Error(FString::Printf(TEXT("Sensor %d not found"), SensorId));
	}

	CaptureActor->TargetSensor = FoundSensor;
	return FExecStatus::OK(FString::Printf(TEXT("Target sensor set to %d"), SensorId));
}

FExecStatus FCaptureActorHandler::SetDataFolder(const TArray<FString>& Args)
{
	FExecStatus ExecStatus = FExecStatus::OK();
	AFusionCamCaptureActor* CaptureActor = GetCaptureActor(Args, ExecStatus);
	if (!IsValid(CaptureActor))
	{
		return ExecStatus;
	}

	if (Args.Num() < 2)
	{
		return FExecStatus::Error("Folder path is required");
	}

	FString FolderPath = Args[1];
	CaptureActor->DataFolder.Path = FolderPath;

	return FExecStatus::OK(FString::Printf(TEXT("Data folder set to: %s"), *FolderPath));
}

FExecStatus FCaptureActorHandler::EnableDataType(const TArray<FString>& Args)
{
	FExecStatus ExecStatus = FExecStatus::OK();
	AFusionCamCaptureActor* CaptureActor = GetCaptureActor(Args, ExecStatus);
	if (!IsValid(CaptureActor))
	{
		return ExecStatus;
	}

	if (Args.Num() < 3)
	{
		return FExecStatus::Error("Usage: vset /captureactor/{id}/enable {type} {on/off}");
	}

	FString DataType = Args[1].ToLower();
	FString EnableStr = Args[2].ToLower();

	bool bEnable = (EnableStr == TEXT("on") || EnableStr == TEXT("true") || EnableStr == TEXT("1"));

	if (DataType == TEXT("rgb"))
	{
		CaptureActor->bRecordRGB = bEnable;
	}
	else if (DataType == TEXT("mask"))
	{
		CaptureActor->bRecordMask = bEnable;
	}
	else if (DataType == TEXT("depth"))
	{
		CaptureActor->bRecordDepth = bEnable;
	}
	else if (DataType == TEXT("normal"))
	{
		CaptureActor->bRecordNormal = bEnable;
	}
	else if (DataType == TEXT("flow"))
	{
		CaptureActor->bRecordFlow = bEnable;
	}
	else if (DataType == TEXT("audio"))
	{
		CaptureActor->bRecordAudio = bEnable;
	}
	else if (DataType == TEXT("metadata"))
	{
		CaptureActor->bRecordMetadata = bEnable;
	}
	else
	{
		return FExecStatus::Error(FString::Printf(
			TEXT("Unknown data type '%s'. Valid types: rgb, mask, depth, normal, flow, audio, metadata"),
			*DataType
		));
	}

	return FExecStatus::OK(FString::Printf(TEXT("%s recording %s"), *DataType, bEnable ? TEXT("enabled") : TEXT("disabled")));
}
