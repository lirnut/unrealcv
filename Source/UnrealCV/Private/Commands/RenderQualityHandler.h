#pragma once
#include "CommandDispatcher.h"
#include "CommandHandler.h"

class FMQRCHandler : public FCommandHandler
{
public:
	void RegisterCommands();

	FExecStatus GetAntiAliasingMethod(const TArray<FString>& Args);
	FExecStatus SetAntiAliasingMethod(const TArray<FString>& Args);

	FExecStatus GetExposureMethod(const TArray<FString>& Args);
	FExecStatus SetExposureMethod(const TArray<FString>& Args);

	FExecStatus GetExposureBias(const TArray<FString>& Args);
	FExecStatus SetExposureBias(const TArray<FString>& Args);

	FExecStatus GetMotionBlur(const TArray<FString>& Args);
	FExecStatus SetMotionBlur(const TArray<FString>& Args);

	FExecStatus GetLumenQuality(const TArray<FString>& Args);
	FExecStatus SetLumenQuality(const TArray<FString>& Args);

	FExecStatus GetSaturation(const TArray<FString>& Args);
	FExecStatus SetSaturation(const TArray<FString>& Args);

	FExecStatus GetContrast(const TArray<FString>& Args);
	FExecStatus SetContrast(const TArray<FString>& Args);

	FExecStatus GetGamma(const TArray<FString>& Args);
	FExecStatus SetGamma(const TArray<FString>& Args);

	FExecStatus GetGain(const TArray<FString>& Args);
	FExecStatus SetGain(const TArray<FString>& Args);

	FExecStatus GetAutoExposureMinBrightness(const TArray<FString>& Args);
	FExecStatus SetAutoExposureMinBrightness(const TArray<FString>& Args);

	FExecStatus GetAutoExposureMaxBrightness(const TArray<FString>& Args);
	FExecStatus SetAutoExposureMaxBrightness(const TArray<FString>& Args);
};
