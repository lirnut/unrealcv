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

	FExecStatus StartSimpleRecording(const TArray<FString>& Args);

	void ParseRecordingOptions(const FString& OptionsStr, bool& bRecordLit, bool& bRecordMask,
		bool& bRecordNormal, bool& bRecordDepth, bool& bRecordFlow);

	FExecStatus IsRecording(const TArray<FString>& Args);

	FExecStatus StopRecording(const TArray<FString>& Args);
};
