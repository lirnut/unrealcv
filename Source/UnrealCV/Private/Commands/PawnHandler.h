#pragma once

#include "CommandHandler.h"

class FPawnHandler : public FCommandHandler
{
public:
	void RegisterCommands();

private:
	FExecStatus GetPawnLocation(const TArray<FString>& Args);
	FExecStatus SetPawnLocation(const TArray<FString>& Args);
	FExecStatus GetPawnRotation(const TArray<FString>& Args);
	FExecStatus SetPawnRotation(const TArray<FString>& Args);
};
