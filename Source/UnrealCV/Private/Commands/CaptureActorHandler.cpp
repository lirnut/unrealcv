#include "CaptureActorHandler.h"
#include "RecordingBPLib.h"
#include "SensorBPLib.h"
#include "UnrealcvServer.h"
#include "UnrealcvLog.h"
#include "AssetPoolManager.h"

void FCaptureActorHandler::RegisterCommands()
{
	auto BindCommandDualCameraID = [this](
		const FString& FormatStr,
		FDispatcherDelegate Delegate,
		const FString& HelpStr)
	{
		if (!FormatStr.Contains(TEXT("[camera_id]")))
		{
			UE_LOG(LogTemp, Error, TEXT("FormatStr must contain [camera_id] placeholder: %s"), *FormatStr);
			check(false);
		}

		FString UintFormatStr = FormatStr.Replace(TEXT("[camera_id]"), TEXT("[uint]"));
		CommandDispatcher->BindCommand(UintFormatStr, Delegate, HelpStr);

		FString StrFormatStr = FormatStr.Replace(TEXT("[camera_id]"), TEXT("[str]"));
		CommandDispatcher->BindCommand(StrFormatStr, Delegate, HelpStr);
	};

	FDispatcherDelegate Cmd;
	FString Help;

	Cmd = FDispatcherDelegate::CreateRaw(this, &FCaptureActorHandler::SpawnFreeCamera);
	Help = "Spawn a free camera at world origin (0, 0, 0)";
	CommandDispatcher->BindCommand("vset /captureactor/spawn_free_cam", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FCaptureActorHandler::SetTimeDilation);
	Help = "Set time dilation for recording (0.1 to 10.0, default 1.0)";
	CommandDispatcher->BindCommand("vset /captureactor/time_dilation [float]", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FCaptureActorHandler::PrintAssetPool);
	Help = "Print all available assets in the asset pool (Category => Path pairs)";
	CommandDispatcher->BindCommand("vget /captureactor/asset_pool", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FCaptureActorHandler::StartSimpleRecording);
	Help = "Start simple recording: vset /captureactor/[id]/record [output_folder] [fps] [duration_seconds] [record_options]";
	Help += "\nRecord options: {lit|rgb},{object_mask|seg},normal,depth,optical_flow";
	Help += "\nExample: vset /captureactor/0/record ./output 30 10 lit,rgb,object_mask,normal (or empty for default lit only)";
	BindCommandDualCameraID("vset /captureactor/[camera_id]/record [str] [uint] [float]", Cmd, Help);
	BindCommandDualCameraID("vset /captureactor/[camera_id]/record [str] [uint] [float] [str]", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FCaptureActorHandler::IsRecording);
	Help = "Check if a camera is currently recording: vget /captureactor/[id]/is_recording";
	BindCommandDualCameraID("vget /captureactor/[camera_id]/is_recording", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FCaptureActorHandler::StopRecording);
	Help = "Stop recording for a camera: vset /captureactor/[id]/stop_record";
	BindCommandDualCameraID("vset /captureactor/[camera_id]/stop_record", Cmd, Help);
}

FExecStatus FCaptureActorHandler::SpawnFreeCamera(const TArray<FString>& Args)
{
	UWorld* World = FUnrealcvServer::Get().GetWorld();
	if (!IsValid(World))
	{
		return FExecStatus::Error("Cannot get world");
	}

	int32 CameraID = URecordingBPLib::CreateFreeCamera(World, FVector::ZeroVector, FRotator::ZeroRotator);

	if (CameraID < 0)
	{
		return FExecStatus::Error("Failed to create free camera");
	}

	return FExecStatus::OK(FString::Printf(TEXT("%d"), CameraID));
}

FExecStatus FCaptureActorHandler::SetTimeDilation(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		return FExecStatus::Error("Usage: vset /captureactor/time_dilation [float]");
	}

	float TimeDilation = FCString::Atof(*Args[0]);

	if (TimeDilation <= 0.0f)
	{
		return FExecStatus::Error("Time dilation must be positive");
	}

	URecordingBPLib::SetTimeDilation(TimeDilation);
	// return FExecStatus::OK(FString::Printf(TEXT("Time dilation set to %.2f"), URecordingBPLib::GetTimeDilation()));
	return FExecStatus::OK();
}

FExecStatus FCaptureActorHandler::PrintAssetPool(const TArray<FString>& Args)
{
	FAssetPoolManager& AssetPool = FAssetPoolManager::Get();
	AssetPool.PrintAssetPoolSummary();
	return FExecStatus::OK();
}

FExecStatus FCaptureActorHandler::StartSimpleRecording(const TArray<FString>& Args)
{
	if (Args.Num() < 4)
	{
		return FExecStatus::Error("Usage: vset /captureactor/[id]/record [filename] [fps] [duration_seconds] [record_options]");
	}

	FString IDString = Args[0];
	FString FileName = Args[1];
	int32 FPS = FCString::Atoi(*Args[2]);
	float DurationSeconds = FCString::Atof(*Args[3]);

	if (FPS <= 0)
	{
		return FExecStatus::Error(FString::Printf(TEXT("Invalid FPS: %d (must be > 0)"), FPS));
	}

	if (DurationSeconds <= 0.0f)
	{
		return FExecStatus::Error(FString::Printf(TEXT("Invalid duration: %.2f (must be > 0)"), DurationSeconds));
	}

	UFusionCamSensor* FusionCamSensor = USensorBPLib::GetSensorByAnyID(IDString);
	if (!IsValid(FusionCamSensor))
	{
		return FExecStatus::Error(FString::Printf(TEXT("Invalid camera ID: %s"), *IDString));
	}

	bool bRecordLit = true;
	bool bRecordMask = false;
	bool bRecordNormal = false;
	bool bRecordDepth = false;
	bool bRecordFlow = false;

	if (Args.Num() >= 5 && !Args[4].IsEmpty())
	{
		ParseRecordingOptions(Args[4], bRecordLit, bRecordMask, bRecordNormal, bRecordDepth, bRecordFlow);
	}

	bool bSuccess = URecordingBPLib::StartSimpleRecording(IDString, FileName, FPS, DurationSeconds,
		bRecordLit, bRecordMask, bRecordNormal, bRecordDepth, bRecordFlow);

	if (bSuccess)
	{
		int32 TotalFrames = FMath::CeilToInt(FPS * DurationSeconds);
		FString RecordTypes = TEXT("(");
		if (bRecordLit) RecordTypes += TEXT("lit,");
		if (bRecordMask) RecordTypes += TEXT("mask,");
		if (bRecordNormal) RecordTypes += TEXT("normal,");
		if (bRecordDepth) RecordTypes += TEXT("depth,");
		if (bRecordFlow) RecordTypes += TEXT("flow,");
		if (RecordTypes.Len() > 1)
		{
			RecordTypes = RecordTypes.Left(RecordTypes.Len() - 1);
		}
		RecordTypes += TEXT(")");
		UE_LOG(LogUnrealCV, Warning, TEXT("Recording started: Camera %s, File: %s, FPS: %d, Frames: %d, Types: %s"),
			*IDString, *FileName, FPS, TotalFrames, *RecordTypes);
		return FExecStatus::OK();
	}
	else
	{
		return FExecStatus::Error(FString::Printf(TEXT("Failed to start recording for camera %s"), *IDString));
	}
}

void FCaptureActorHandler::ParseRecordingOptions(const FString& OptionsStr, bool& bRecordLit, bool& bRecordMask,
	bool& bRecordNormal, bool& bRecordDepth, bool& bRecordFlow)
{
	bRecordLit = false;
	bRecordMask = false;
	bRecordNormal = false;
	bRecordDepth = false;
	bRecordFlow = false;

	if (OptionsStr.IsEmpty())
	{
		bRecordLit = true;
		return;
	}

	TArray<FString> Options;
	OptionsStr.ParseIntoArray(Options, TEXT(","), true);

	for (const FString& Option : Options)
	{
		FString Trimmed = Option.TrimStartAndEnd().ToLower();

		if (Trimmed == TEXT("lit") || Trimmed == TEXT("rgb"))
		{
			bRecordLit = true;
		}
		else if (Trimmed == TEXT("object_mask") || Trimmed == TEXT("seg"))
		{
			bRecordMask = true;
		}
		else if (Trimmed == TEXT("normal"))
		{
			bRecordNormal = true;
		}
		else if (Trimmed == TEXT("depth"))
		{
			bRecordDepth = true;
		}
		else if (Trimmed == TEXT("optical_flow"))
		{
			bRecordFlow = true;
		}
	}

	if (!bRecordLit && !bRecordMask && !bRecordNormal && !bRecordDepth && !bRecordFlow)
	{
		bRecordLit = true;
	}
}

FExecStatus FCaptureActorHandler::IsRecording(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		return FExecStatus::Error("Usage: vget /captureactor/[id]/is_recording");
	}

	FString IDString = Args[0];

	UFusionCamSensor* FusionCamSensor = USensorBPLib::GetSensorByAnyID(IDString);
	if (!IsValid(FusionCamSensor))
	{
		return FExecStatus::Error(FString::Printf(TEXT("Invalid camera ID: %s"), *IDString));
	}

	bool bIsRecording = URecordingBPLib::IsRecording(IDString);

	if (bIsRecording)
	{
		return FExecStatus::OK("true");
	}
	else
	{
		return FExecStatus::OK("false");
	}
}

FExecStatus FCaptureActorHandler::StopRecording(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		return FExecStatus::Error("Usage: vset /captureactor/[id]/stop_record");
	}

	FString IDString = Args[0];

	bool bSuccess = URecordingBPLib::StopRecording(IDString);

	if (bSuccess)
	{
		return FExecStatus::OK(FString::Printf(TEXT("Recording stopped for camera %s"), *IDString));
	}
	else
	{
		return FExecStatus::Error(FString::Printf(TEXT("Camera %s is not recording"), *IDString));
	}
}
