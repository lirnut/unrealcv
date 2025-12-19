#include "CaptureActorHandler.h"
#include "RecordingBPLib.h"
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

	uint32 CameraID = FCString::Atoi(*Args[0]);
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

	bool bSuccess = URecordingBPLib::StartSimpleRecording(CameraID, FileName, FPS, DurationSeconds);

	if (bSuccess)
	{
		int32 TotalFrames = FMath::CeilToInt(FPS * DurationSeconds);
		return FExecStatus::OK(FString::Printf(TEXT("Recording started: Camera %d, File: %s, FPS: %d, Frames: %d"),
			CameraID, *FileName, FPS, TotalFrames));
	}
	else
	{
		return FExecStatus::Error(FString::Printf(TEXT("Failed to start recording for camera %d"), CameraID));
	}
}
