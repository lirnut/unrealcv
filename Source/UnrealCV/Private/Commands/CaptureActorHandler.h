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
};
