#include "MVRCHandler.h"
#include "Sensor/CameraSensor/MainViewportRenderComponent.h"
#include "Utils/StrFormatter.h"

void FMVRCHandler::RegisterCommands()
{
	CommandDispatcher->BindCommand(
		TEXT("vget /mvrc/use_sync_capture"),
		FDispatcherDelegate::CreateRaw(this, &FMVRCHandler::GetUseSyncCapture),
		TEXT("Get MainViewportRenderComponent sync capture mode status (0 or 1)")
	);

	CommandDispatcher->BindCommand(
		TEXT("vset /mvrc/use_sync_capture [uint]"),
		FDispatcherDelegate::CreateRaw(this, &FMVRCHandler::SetUseSyncCapture),
		TEXT("Set MainViewportRenderComponent sync capture mode (0=disabled, 1=enabled)")
	);
}

FExecStatus FMVRCHandler::GetUseSyncCapture(const TArray<FString>& Args)
{
	bool bUseSyncCapture = UMainViewportRenderComponent::GlobalSettings.bUseSyncCapture;
	FString Result = bUseSyncCapture ? TEXT("1") : TEXT("0");
	return FExecStatus::OK(Result);
}

FExecStatus FMVRCHandler::SetUseSyncCapture(const TArray<FString>& Args)
{
	if (Args.Num() != 1)
	{
		return FExecStatus::Error("Usage: vset /mvrc/use_sync_capture [0|1]");
	}

	bool bEnable = FCString::Atoi(*Args[0]) != 0;
	UMainViewportRenderComponent::GlobalSettings.bUseSyncCapture = bEnable;
	return FExecStatus::OK(FString::Printf(TEXT("MVRC SyncCapture set to %d"), bEnable ? 1 : 0));
}
