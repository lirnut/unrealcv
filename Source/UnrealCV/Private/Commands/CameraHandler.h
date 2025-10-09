#pragma once

#include "CommandHandler.h"

/** Handle vget/vset /camera/ commands */
class FCameraHandler : public FCommandHandler
{
public:
	void RegisterCommands();

private:
	class UFusionCamSensor* GetCamera(const TArray<FString>& Args, FExecStatus& Status);

	FExecStatus GetCameraList(const TArray<FString>& Args);

	FExecStatus SpawnCamera(const TArray<FString>& Args);

	FExecStatus GetCameraLocation(const TArray<FString>& Args);

	FExecStatus SetCameraLocation(const TArray<FString>& Args);

	FExecStatus GetCameraRotation(const TArray<FString>& Args);

	FExecStatus SetCameraRotation(const TArray<FString>& Args);

	template<class T>
	void SaveData(const TArray<T>& Data, int Width, int Height,
		const TArray<FString>& Args, FExecStatus& Status);

	FExecStatus GetCameraLit(const TArray<FString>& Args);

	FExecStatus GetCameraDepth(const TArray<FString>& Args);

	FExecStatus GetCameraNormal(const TArray<FString>& Args);

	FExecStatus GetCameraFlow(const TArray<FString>& Args);

	FExecStatus GetCameraObjMask(const TArray<FString>& Args);

	FExecStatus MoveTo(const TArray<FString>& Args);

	FExecStatus GetScreenshot(const TArray<FString>& Args);

	FExecStatus SetPlayerViewMode(const TArray<FString>& Args);

	FExecStatus GetPlayerViewMode(const TArray<FString>& Args);

	FExecStatus GetFOV(const TArray<FString>& Args);

	FExecStatus SetFOV(const TArray<FString>& Args);

	FExecStatus GetSize(const TArray<FString>& Args);

	FExecStatus SetSize(const TArray<FString>& Args);

	FExecStatus SetProjectionType(const TArray<FString>& Args);

	FExecStatus SetOrthoWidth(const TArray<FString>& Args);

    FExecStatus SetLitSource(const TArray<FString>& Args);

    FExecStatus SetReflectionMethod(const TArray<FString>& Args);

    FExecStatus SetGlobalIlluminationMethod(const TArray<FString>& Args);

	FExecStatus SetExposureMethod(const TArray<FString>& Args);

	FExecStatus SetExposureBias(const TArray<FString>& Args);

	FExecStatus SetAutoExposureSpeed(const TArray<FString>& Args);

	FExecStatus SetAutoExposureBrightness(const TArray<FString>& Args);

	FExecStatus SetApplyPhysicalCameraExposure(const TArray<FString>& Args);

	FExecStatus SetMotionBlurParams(const TArray<FString>& Args);

	FExecStatus SetFocalParams(const TArray<FString>& Args);

	FExecStatus SetCameraAudioRecord(const TArray<FString>& Args);
	// FExecStatus StartCameraAudioRecord(const TArray<FString>& Args);
	// FExecStatus StopCameraAudioRecord(const TArray<FString>& Args);
	// Audio::FMixerDevice* GetAudioMixer(int SensorId, FExecStatus& ExecStatus);

	FExecStatus GetHWObs(const TArray<FString>& Args);
	FExecStatus GetHWObsV1(const TArray<FString>& Args);
	FExecStatus GetHWObsV2(const TArray<FString>& Args);
	FExecStatus GetHWObsV3(const TArray<FString>& Args);

	FExecStatus GetCameraOneObjMask(const TArray<FString>& Args);

	FExecStatus StartRecord(const TArray<FString>& Args);
	FExecStatus StartBulletTimeRecord(const TArray<FString>& Args);
	FExecStatus CheckRecordStatus(const TArray<FString>& Args);

private:
	// Map camera ID to its associated CaptureActor for automatic lifecycle management
	TMap<int32, class AFusionCamCaptureActor*> CameraRecordingActors;
};
