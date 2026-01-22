#pragma once

#include "CoreMinimal.h"
#include "CommandHandler.h"

class FDatasetAutomationHandler : public FCommandHandler
{
public:
	FDatasetAutomationHandler() {}

	void RegisterCommands();

	FExecStatus GetTaskName(const TArray<FString>& Args);
	FExecStatus SetTaskName(const TArray<FString>& Args);
};
