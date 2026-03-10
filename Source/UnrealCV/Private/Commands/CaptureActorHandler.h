#pragma once

#include "CommandHandler.h"
#include "FusionCamCaptureActor.h"

class FCaptureActorHandler : public FCommandHandler
{
public:
	void RegisterCommands();

private:
	FExecStatus SpawnFreeCamera(const TArray<FString>& Args);

	FExecStatus GetTimeDilation(const TArray<FString>& Args);

	FExecStatus SetTimeDilation(const TArray<FString>& Args);

	FExecStatus PrintAssetPool(const TArray<FString>& Args);

	FExecStatus StartSimpleRecording(const TArray<FString>& Args);

	FExecStatus IsRecording(const TArray<FString>& Args);

	FExecStatus StopRecording(const TArray<FString>& Args);

	FExecStatus GetUseMovieQualityRendering(const TArray<FString>& Args);

	FExecStatus SetUseMovieQualityRendering(const TArray<FString>& Args);

	FExecStatus GetRecordViaViewport(const TArray<FString>& Args);

	FExecStatus SetRecordViaViewport(const TArray<FString>& Args);

	FExecStatus GetVideoEncoderBitrate(const TArray<FString>& Args);

	FExecStatus SetVideoEncoderBitrate(const TArray<FString>& Args);

	FExecStatus GetH264Encoding(const TArray<FString>& Args);

	FExecStatus SetH264Encoding(const TArray<FString>& Args);

	FExecStatus GetAutoGenerateVideo(const TArray<FString>& Args);

	FExecStatus SetAutoGenerateVideo(const TArray<FString>& Args);

	FExecStatus GetWarmUpFrames(const TArray<FString>& Args);

	FExecStatus SetWarmUpFrames(const TArray<FString>& Args);

	FExecStatus GetVideoGenScriptPath(const TArray<FString>& Args);

	FExecStatus SetVideoGenScriptPath(const TArray<FString>& Args);

	FExecStatus GetPausedTickInterval(const TArray<FString>& Args);

	FExecStatus SetPausedTickInterval(const TArray<FString>& Args);

	// Instance-based settings (require camera ID)
	FExecStatus GetAddTimestamp(const TArray<FString>& Args);

	FExecStatus SetAddTimestamp(const TArray<FString>& Args);

	FExecStatus GetTrackForegroundMovement(const TArray<FString>& Args);

	FExecStatus SetTrackForegroundMovement(const TArray<FString>& Args);

	FExecStatus GetForegroundMoveSpeed(const TArray<FString>& Args);

	FExecStatus SetForegroundMoveSpeed(const TArray<FString>& Args);

	FExecStatus GetForegroundMoveAngleOffset(const TArray<FString>& Args);

	FExecStatus SetForegroundMoveAngleOffset(const TArray<FString>& Args);

	FExecStatus GetTargetHeightOffset(const TArray<FString>& Args);

	FExecStatus SetTargetHeightOffset(const TArray<FString>& Args);

	FExecStatus GetBulletTimeSpeedDeg(const TArray<FString>& Args);

	FExecStatus SetBulletTimeSpeedDeg(const TArray<FString>& Args);

	FExecStatus GetPaused(const TArray<FString>& Args);

	FExecStatus SetPaused(const TArray<FString>& Args);

	// Random trajectory distance variation settings (global)
	FExecStatus GetRandomTrajectoryDistanceVariationProbability(const TArray<FString>& Args);

	FExecStatus SetRandomTrajectoryDistanceVariationProbability(const TArray<FString>& Args);

	FExecStatus GetDistanceVariationMin(const TArray<FString>& Args);

	FExecStatus SetDistanceVariationMin(const TArray<FString>& Args);

	FExecStatus GetDistanceVariationMax(const TArray<FString>& Args);

	FExecStatus SetDistanceVariationMax(const TArray<FString>& Args);
};
