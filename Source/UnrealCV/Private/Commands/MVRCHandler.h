#pragma once
#include "CommandDispatcher.h"
#include "CommandHandler.h"

class FMVRCHandler : public FCommandHandler
{
public:
	void RegisterCommands();

	FExecStatus GetUseSyncCapture(const TArray<FString>& Args);
	FExecStatus SetUseSyncCapture(const TArray<FString>& Args);
};
