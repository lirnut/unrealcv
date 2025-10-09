#pragma once

#include "CommandHandler.h"

/** Handle vget/vset /captureactor/ commands for FusionCamCaptureActor */
class FCaptureActorHandler : public FCommandHandler
{
public:
	void RegisterCommands();

private:
	class AFusionCamCaptureActor* GetCaptureActor(const TArray<FString>& Args, FExecStatus& Status);

	FExecStatus ListCaptureActors(const TArray<FString>& Args);

	FExecStatus StartRecord(const TArray<FString>& Args);

	FExecStatus StartBulletTimeRecord(const TArray<FString>& Args);

	FExecStatus StopRecord(const TArray<FString>& Args);

	FExecStatus GetRecordStatus(const TArray<FString>& Args);

	FExecStatus SetTargetSensor(const TArray<FString>& Args);

	FExecStatus SetDataFolder(const TArray<FString>& Args);

	FExecStatus EnableDataType(const TArray<FString>& Args);
};
