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

	static AFusionCamCaptureActor* GetCaptureActor(FString CID);

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Recording")
	static bool StopRecording(int32 CameraID);

	static bool StopRecording(const FString& IDString);

	UFUNCTION(BlueprintPure, Category = "UnrealCV|Recording")
	static bool IsRecording(int32 CameraID);

	static bool IsRecording(const FString& IDString);

	UFUNCTION(BlueprintPure, Category = "UnrealCV|Recording")
	static TArray<class UFusionCamSensor*> GetAllCameras();

	UFUNCTION(BlueprintPure, Category = "UnrealCV|Recording")
	static class UFusionCamSensor* GetCameraByID(int32 CameraID);

	UFUNCTION(BlueprintPure, Category = "UnrealCV|Recording")
	static FString GetCameraName(int32 CameraID);

	UFUNCTION(BlueprintPure, Category = "UnrealCV|Recording")
	static int32 GetCameraCount();

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Recording", meta = (WorldContext = "WorldContextObject"))
	static int32 CreateFreeCamera(
		UObject* WorldContextObject,
		FVector Location = FVector::ZeroVector,
		FRotator Rotation = FRotator::ZeroRotator
	);

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

	UFUNCTION(BlueprintPure, Category = "UnrealCV|Recording|Trajectory")
	static TArray<FString> GetSupportedTrajectoryTypes();

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Recording|Simple")
	// static bool StartSimpleRecording(
	// 	int32 CameraID,
	// 	const FString& FileName,
	// 	int32 FPS,
	// 	float DurationSeconds
	// );
	// static bool StartSimpleRecording(
	// 	const FString& IDString,
	// 	const FString& FileName,
	// 	int32 FPS,
	// 	float DurationSeconds,
	// 	bool bRecordLit = true,
	// 	bool bRecordMask = false,
	// 	bool bRecordNormal = false,
	// 	bool bRecordDepth = false,
	// 	bool bRecordFlow = false
	// );
	// static bool StartSimpleRecording(
	// 	const FString& IDString,
	// 	const FString& FileName,
	// 	int32 FPS,
	// 	float DurationSeconds
	// );
	static bool StartSimpleRecording(
		const FString& IDString,
		const FString& FileName,
		int32 FPS,
		float DurationSeconds,
		const FRecordingDataTypesConfig& RecordingConfig
	);
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
