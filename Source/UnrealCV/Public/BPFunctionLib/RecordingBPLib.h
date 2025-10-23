// Weichao Qiu @ 2018
// Recording control function library for Blueprint/C++ access
#pragma once

#include "Runtime/Engine/Classes/Kismet/BlueprintFunctionLibrary.h"
#include "Actor/FusionCamCaptureActor.h"
#include "RecordingBPLib.generated.h"


/**
 * Blueprint Function Library for controlling camera recording without TCP server.
 * Wraps the recording functionality from CameraHandler for direct Blueprint/C++ access.
 */
UCLASS()
class UNREALCV_API URecordingBPLib : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
private:
	static AFusionCamCaptureActor* PrepareRecording(int32 CameraID);


public:
	/**
	 * Start normal recording from a specific camera.
	 * @param CameraID The ID of the camera sensor (0 for player camera, 1+ for spawned cameras)
	 * @param FileName Output file path (e.g., "C:/Output/video.mp4")
	 * @param Duration Recording duration in seconds
	 * @param FPS Frames per second (typically 30 or 60)
	 * @param TargetToHide Optional actor to hide during recording (for dataset generation)
	 * @return True if recording started successfully, false otherwise
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Recording")
	static bool StartNormalRecording(
		int32 CameraID,
		const FString& FileName,
		float Duration,
		int32 FPS,
		AActor* TargetToHide = nullptr,
		float TimeDilation = 1.0f
	);

	/**
	 * Start bullet time recording (360° rotation around target).
	 * Camera will orbit around the target actor during recording.
	 * @param CameraID The ID of the camera sensor
	 * @param FileName Output file path (e.g., "C:/Output/bullettime.mp4")
	 * @param Duration Recording duration in seconds
	 * @param FPS Frames per second
	 * @param Target Target actor to orbit around (required for bullet time)
	 * @return True if recording started successfully, false otherwise
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Recording")
	static bool StartBulletTimeRecording(
		int32 CameraID,
		const FString& FileName,
		float Duration,
		int32 FPS,
		AActor* Target,
		float TimeDilation = 1.0f
	);

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Recording")
	static bool StartBulletTimeOnlyRecording(
		int32 CameraID,
		const FString& FileName,
		float Duration,
		int32 FPS,
		AActor* Target,
		float TimeDilation = 1.0f
	);



	/**
	 * Stop recording for a specific camera.
	 * @param CameraID The ID of the camera sensor
	 * @return True if recording stopped successfully, false if camera wasn't recording
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Recording")
	static bool StopRecording(int32 CameraID);

	/**
	 * Check if a specific camera is currently recording.
	 * @param CameraID The ID of the camera sensor
	 * @return True if camera is recording, false otherwise
	 */
	UFUNCTION(BlueprintPure, Category = "UnrealCV|Recording")
	static bool IsRecording(int32 CameraID);

	/**
	 * Get recording progress for a specific camera.
	 * @param CameraID The ID of the camera sensor
	 * @param OutProgress Output parameter: progress from 0.0 to 1.0
	 * @param OutFrameCount Output parameter: number of frames recorded so far
	 * @return True if camera is recording, false otherwise
	 */
	UFUNCTION(BlueprintPure, Category = "UnrealCV|Recording")
	static bool GetRecordingProgress(
		int32 CameraID,
		float& OutProgress,
		int32& OutFrameCount
	);

	/**
	 * Get list of all available camera sensors in the scene.
	 * @return Array of camera sensor objects
	 */
	UFUNCTION(BlueprintPure, Category = "UnrealCV|Recording")
	static TArray<class UFusionCamSensor*> GetAllCameras();

	/**
	 * Get camera sensor by ID.
	 * @param CameraID The ID of the camera sensor
	 * @return Camera sensor object, or nullptr if not found
	 */
	UFUNCTION(BlueprintPure, Category = "UnrealCV|Recording")
	static class UFusionCamSensor* GetCameraByID(int32 CameraID);

	/**
	 * Get the name/description of a camera sensor.
	 * @param CameraID The ID of the camera sensor
	 * @return Camera name (e.g., "Camera_0", "PlayerCamera")
	 */
	UFUNCTION(BlueprintPure, Category = "UnrealCV|Recording")
	static FString GetCameraName(int32 CameraID);

	/**
	 * Get the number of available cameras in the scene.
	 * @return Number of camera sensors
	 */
	UFUNCTION(BlueprintPure, Category = "UnrealCV|Recording")
	static int32 GetCameraCount();
};
