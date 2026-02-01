#pragma once
#include "CommandDispatcher.h"
#include "CommandHandler.h"

class FPakHandler : public FCommandHandler
{
public:
	void RegisterCommands();

	FExecStatus MountPak(const TArray<FString>& Args);
	FExecStatus UnmountPak(const TArray<FString>& Args);
	FExecStatus GetMountedPaks(const TArray<FString>& Args);
	FExecStatus IsPakMounted(const TArray<FString>& Args);
	FExecStatus ScanPakAssets(const TArray<FString>& Args);
	FExecStatus LoadAsset(const TArray<FString>& Args);
	FExecStatus GetAssetsInPath(const TArray<FString>& Args);
	FExecStatus RegisterToAssetPool(const TArray<FString>& Args);
};
