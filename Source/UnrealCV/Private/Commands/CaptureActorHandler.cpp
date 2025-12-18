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
