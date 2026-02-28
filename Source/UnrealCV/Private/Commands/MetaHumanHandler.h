#pragma once
#include "CommandDispatcher.h"
#include "CommandHandler.h"

class FMetaHumanHandler : public FCommandHandler
{
public:
	void RegisterCommands();

	FExecStatus GetAllMetaHumanPaths(const TArray<FString>& Args);
	FExecStatus UpdateCache(const TArray<FString>& Args);
	FExecStatus GetCachePath(const TArray<FString>& Args);
	FExecStatus FilterBatchGenerated(const TArray<FString>& Args);
};
