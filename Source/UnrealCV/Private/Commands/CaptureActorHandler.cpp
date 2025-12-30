#include "CaptureActorHandler.h"
#include "RecordingBPLib.h"
#include "SensorBPLib.h"
#include "UnrealcvServer.h"
#include "UnrealcvLog.h"
#include "AssetPoolManager.h"

void FCaptureActorHandler::RegisterCommands()
{
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
	Help = "Start simple recording without camera movement: vset /captureactor/[id]/record [filename] [fps] [duration_seconds]";
	CommandDispatcher->BindCommand("vset /captureactor/[uint]/record [str] [uint] [float]", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FCaptureActorHandler::IsRecording);
	Help = "Check if a camera is currently recording: vget /captureactor/[id]/is_recording";
	CommandDispatcher->BindCommand("vget /captureactor/[uint]/is_recording", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FCaptureActorHandler::StopRecording);
	Help = "Stop recording for a camera: vset /captureactor/[id]/stop_record";
	CommandDispatcher->BindCommand("vset /captureactor/[uint]/stop_record", Cmd, Help);
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
	return FExecStatus::OK(FString::Printf(TEXT("Time dilation set to %.2f"), URecordingBPLib::GetTimeDilation()));
}

FExecStatus FCaptureActorHandler::PrintAssetPool(const TArray<FString>& Args)
{
	FAssetPoolManager& AssetPool = FAssetPoolManager::Get();
	AssetPool.PrintAssetPoolSummary();
	return FExecStatus::OK("Asset pool printed to debug log");
}

FExecStatus FCaptureActorHandler::StartSimpleRecording(const TArray<FString>& Args)
{
	if (Args.Num() < 3)
	{
		return FExecStatus::Error("Usage: vset /captureactor/[id]/record [filename] [fps] [duration_seconds]");
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

	bool bSuccess = URecordingBPLib::StartSimpleRecording(IDString, FileName, FPS, DurationSeconds);

	if (bSuccess)
	{
		int32 TotalFrames = FMath::CeilToInt(FPS * DurationSeconds);
		return FExecStatus::OK(FString::Printf(TEXT("Recording started: Camera %s, File: %s, FPS: %d, Frames: %d"),
			*IDString, *FileName, FPS, TotalFrames));
	}
	else
	{
		return FExecStatus::Error(FString::Printf(TEXT("Failed to start recording for camera %s"), *IDString));
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
