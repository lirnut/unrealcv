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
public:
	static AFusionCamCaptureActor* PrepareRecording(int32 CameraID);

	// /**
	//  * Start normal recording from a specific camera.
	//  * @param CameraID The ID of the camera sensor (0 for player camera, 1+ for spawned cameras)
	//  * @param FileName Output file path (e.g., "C:/Output/video.mp4")
	//  * @param Duration Recording duration in seconds
	//  * @param FPS Frames per second (typically 30 or 60)
	//  * @param TargetToHide Optional actor to hide during recording (for dataset generation)
	//  * @return True if recording started successfully, false otherwise
	//  */
	// UFUNCTION(BlueprintCallable, Category = "UnrealCV|Recording")
	// static bool StartNormalRecording(
	// 	int32 CameraID,
	// 	const FString& FileName,
	// 	float Duration,
	// 	int32 FPS,
	// 	AActor* TargetToHide = nullptr,
	// 	float TimeDilation = 1.0f
	// );

	// /**
	//  * Start bullet time recording (360° rotation around target).
	//  * Camera will orbit around the target actor during recording.
	//  * @param CameraID The ID of the camera sensor
	//  * @param FileName Output file path (e.g., "C:/Output/bullettime.mp4")
	//  * @param Duration Recording duration in seconds
	//  * @param FPS Frames per second
	//  * @param Target Target actor to orbit around (required for bullet time)
	//  * @return True if recording started successfully, false otherwise
	//  */
	// UFUNCTION(BlueprintCallable, Category = "UnrealCV|Recording")
	// static bool StartBulletTimeRecording(
	// 	int32 CameraID,
	// 	const FString& FileName,
	// 	float Duration,
	// 	int32 FPS,
	// 	AActor* Target,
	// 	float TimeDilation = 1.0f
	// );

	// UFUNCTION(BlueprintCallable, Category = "UnrealCV|Recording")
	// static bool StartBulletTimeOnlyRecording(
	// 	int32 CameraID,
	// 	const FString& FileName,
	// 	float Duration,
	// 	int32 FPS,
	// 	AActor* Target,
	// 	float TimeDilation = 1.0f
	// );



	/**
	 * Stop recording for a specific camera.
	 * @param CameraID The ID of the camera sensor
	 * @return True if recording stopped successfully, false if camera wasn't recording
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Recording")
	static bool StopRecording(int32 CameraID);

	/**
	 * Stop recording for a specific camera (supports new format ID like ActorName_UUID).
	 * @param IDString Camera ID in old format (integer) or new format (ActorName_UUID)
	 * @return True if recording stopped successfully, false if camera wasn't recording
	 */
	static bool StopRecording(const FString& IDString);

	/**
	 * Check if a specific camera is currently recording.
	 * @param CameraID The ID of the camera sensor
	 * @return True if camera is recording, false otherwise
	 */
	UFUNCTION(BlueprintPure, Category = "UnrealCV|Recording")
	static bool IsRecording(int32 CameraID);

	/**
	 * Check if a specific camera is currently recording (supports new format ID).
	 * @param IDString Camera ID in old format (integer) or new format (ActorName_UUID)
	 * @return True if camera is recording, false otherwise
	 */
	static bool IsRecording(const FString& IDString);

	// /**
	//  * Get recording progress for a specific camera.
	//  * @param CameraID The ID of the camera sensor
	//  * @param OutProgress Output parameter: progress from 0.0 to 1.0
	//  * @param OutFrameCount Output parameter: number of frames recorded so far
	//  * @return True if camera is recording, false otherwise
	//  */
	// UFUNCTION(BlueprintPure, Category = "UnrealCV|Recording")
	// static bool GetRecordingProgress(
	// 	int32 CameraID,
	// 	float& OutProgress,
	// 	int32& OutFrameCount
	// );

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

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Recording", meta = (WorldContext = "WorldContextObject"))
	static int32 CreateFreeCamera(
		UObject* WorldContextObject,
		FVector Location = FVector::ZeroVector,
		FRotator Rotation = FRotator::ZeroRotator
	);

	// ========== Camera Trajectory Recording (SOW Requirements) ==========

	/**
	 * Start camera trajectory recording with string-based trajectory type.
	 * Supports all 10 SOW trajectory types through a single unified API.
	 *
	 * @param CameraID The ID of the camera sensor
	 * @param FileName Output file path prefix (e.g., "C:/Output/trajectory")
	 * @param TrajectoryType Trajectory type as string:
	 *   - Fixed trajectories: "rotate_left_45", "rotate_right_45", "rotate_up_45",
	 *                        "rotate_360", "zoom_in", "zoom_out"
	 *   - Random trajectories: "random_1", "random_2", "random_3", "random_4"
	 * @param Target Target actor to orbit/focus on (required)
	 * @param FPS Frames per second for trajectory rendering (default 30)
	 * @param DegreesPerSecond Rotation speed in degrees per second (default 36 deg/s = 10s for 360°)
	 * @param RandomSeed Random seed for random trajectories (optional, -1 for auto)
	 * @return True if recording started successfully, false otherwise
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Recording|Trajectory")
	static bool StartTrajectoryRecording(
		int32 CameraID,
		const FString& FileName,
		const FString& TrajectoryType,
		AActor* Target,
		int32 FPS = 30,
		float DegreesPerSecond = 36.0f,
		int32 RandomSeed = -1
	);

	/**
	 * Get list of all supported trajectory types.
	 * @return Array of trajectory type names
	 */
	UFUNCTION(BlueprintPure, Category = "UnrealCV|Recording|Trajectory")
	static TArray<FString> GetSupportedTrajectoryTypes();

	// ========== Simple Recording (No Camera Movement) ==========

	/**
	 * Start simple recording without camera movement.
	 * Camera remains in its current position and rotation throughout recording.
	 *
	 * @param CameraID The ID of the camera sensor
	 * @param FileName Output file path prefix (e.g., "C:/Output/simple_record")
	 * @param FPS Frames per second for recording
	 * @param DurationSeconds Recording duration in seconds
	 * @return True if recording started successfully, false otherwise
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Recording|Simple")
	static bool StartSimpleRecording(
		int32 CameraID,
		const FString& FileName,
		int32 FPS,
		float DurationSeconds
	);

	/**
	 * Start recording a simple video from a camera without camera motion (supports new format ID).
	 *
	 * @param IDString Camera ID in old format (integer) or new format (ActorName_UUID)
	 * @param FileName Output file path prefix (e.g., "C:/Output/simple_record")
	 * @param FPS Frames per second for recording
	 * @param DurationSeconds Recording duration in seconds
	 * @return True if recording started successfully, false otherwise
	 */
	static bool StartSimpleRecording(
		const FString& IDString,
		const FString& FileName,
		int32 FPS,
		float DurationSeconds
	);

	// // ========== Video Generation Configuration ==========

	// /**
	//  * Enable or disable automatic video generation after recording completes.
	//  * @param CameraID The ID of the camera sensor
	//  * @param bEnabled Whether to auto-generate videos
	//  * @return True if setting was applied successfully
	//  */
	// UFUNCTION(BlueprintCallable, Category = "UnrealCV|Recording|VideoGen")
	// static bool SetAutoGenerateVideo(int32 CameraID, bool bEnabled);

	// /**
	//  * Configure video generation script path and conda environment.
	//  * @param CameraID The ID of the camera sensor
	//  * @param ScriptPath Path to genvid.py (leave empty for auto-detection)
	//  * @param CondaEnvName Conda environment name (default: "uezoo")
	//  * @return True if configuration was applied successfully
	//  */
	// UFUNCTION(BlueprintCallable, Category = "UnrealCV|Recording|VideoGen")
	// static bool ConfigureVideoGeneration(
	// 	int32 CameraID,
	// 	const FString& ScriptPath = TEXT(""),
	// 	const FString& CondaEnvName = TEXT("uezoo")
	// );

	// /**
	//  * Manually trigger video generation for a specific folder.
	//  * @param FolderPath Path to folder containing image sequences
	//  * @param FPS Frames per second for video generation
	//  * @param ScriptPath Path to genvid.py (leave empty for auto-detection)
	//  * @param CondaEnvName Conda environment name (default: "uezoo")
	//  * @return True if video generation was triggered successfully
	//  */
	// UFUNCTION(BlueprintCallable, Category = "UnrealCV|Recording|VideoGen")
	// static bool GenerateVideoFromImages(
	// 	const FString& FolderPath,
	// 	int32 FPS = 30,
	// 	const FString& ScriptPath = TEXT(""),
	// 	const FString& CondaEnvName = TEXT("uezoo")
	// );

	/**
	 * Convert string trajectory type to enum.
	 * @param TrajectoryTypeStr String representation of trajectory type
	 * @param OutTrajectoryType Output enum value
	 * @return True if conversion successful, false if invalid string
	 */
	static bool ParseTrajectoryType(const FString& TrajectoryTypeStr, ECameraTrajectoryType& OutTrajectoryType);
	static float GetTimeDilation();
	static void SetTimeDilation(float Value);

private:
	// Static map to track recording actors (camera ID -> capture actor)
	// This replaces the need to access CameraHandler's private map
	static TMap<FString, AFusionCamCaptureActor*> GlobalCameraRecordingActors;

	// Static variable for time dilation control
	static float GlobalTimeDilation;
};
