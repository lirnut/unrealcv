#pragma once

#include "CommandHandler.h"

class FLightHandler : public FCommandHandler
{
public:
	void RegisterCommands();

private:
	FExecStatus GetDirectionalLightIntensity(const TArray<FString>& Args);
	FExecStatus SetDirectionalLightIntensity(const TArray<FString>& Args);
	FExecStatus GetSkyLightIntensity(const TArray<FString>& Args);
	FExecStatus SetSkyLightIntensity(const TArray<FString>& Args);
};
