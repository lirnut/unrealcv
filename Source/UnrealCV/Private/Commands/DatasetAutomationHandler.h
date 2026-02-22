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
	FExecStatus SetSequence(const TArray<FString>& Args);
	FExecStatus GetSequence(const TArray<FString>& Args);
	FExecStatus StartAutomation(const TArray<FString>& Args);
	FExecStatus StopAutomation(const TArray<FString>& Args);
	FExecStatus GetStatus(const TArray<FString>& Args);

	FExecStatus SetCurrentSceneForegroundActor(const TArray<FString>& Args);
	FExecStatus SetCurrentScenePrimaryCamera(const TArray<FString>& Args);
	FExecStatus SetCurrentSceneSceneCategory(const TArray<FString>& Args);
	FExecStatus SetCurrentSceneForegroundSubcategory(const TArray<FString>& Args);
	FExecStatus SetCurrentSceneOccluderCategory(const TArray<FString>& Args);

	FExecStatus SetConfigTotalScenes(const TArray<FString>& Args);
	FExecStatus SetConfigOutputDirectory(const TArray<FString>& Args);
	FExecStatus SetConfigTrajectoryFPS(const TArray<FString>& Args);
	FExecStatus SetConfigNumFrames(const TArray<FString>& Args);
	FExecStatus SetConfigBLoadSceneParamsFromJson(const TArray<FString>& Args);
	FExecStatus SetConfigForegroundMoveSpeed(const TArray<FString>& Args);
	FExecStatus SetConfigForegroundMoveAngleOffset(const TArray<FString>& Args);
	FExecStatus SetConfigRecordingOptions(const TArray<FString>& Args);
	FExecStatus GetCommandHistory(const TArray<FString>& Args);
};
