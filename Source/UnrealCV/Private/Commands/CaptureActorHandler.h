#pragma once

#include "CommandHandler.h"

class FCaptureActorHandler : public FCommandHandler
{
public:
	void RegisterCommands();

private:
	FExecStatus SpawnFreeCamera(const TArray<FString>& Args);

	FExecStatus SetTimeDilation(const TArray<FString>& Args);

	FExecStatus PrintAssetPool(const TArray<FString>& Args);
};
