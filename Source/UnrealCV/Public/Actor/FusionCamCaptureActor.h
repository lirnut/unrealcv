// Weichao Qiu @ 2018
// Modified for FusionCamSensor-specific recording
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JsonObjectBP.h"
#include "BPFunctionLib\SceneCompositionBPLib.h"
#include "FusionCamCaptureActor.generated.h"

/**
 * Camera trajectory types for SOW camera movement requirements
 * 6 fixed trajectories + 4 random trajectories = 10 per scene
 */
UENUM(BlueprintType)
enum class ECameraTrajectoryType : uint8
{
	RotateLeft45 UMETA(DisplayName = "Rotate Left 45°"),
	RotateLeft30 UMETA(DisplayName = "Rotate Left 30°"),
	RotateRight45 UMETA(DisplayName = "Rotate Right 45°"),
	RotateRight30 UMETA(DisplayName = "Rotate Right 30°"),
	RotateUp45 UMETA(DisplayName = "Rotate Up 45°"),
	RotateUp30 UMETA(DisplayName = "Rotate Up 30°"),
	Rotate360 UMETA(DisplayName = "Rotate 360°"),
	ZoomIn UMETA(DisplayName = "Zoom In"),
	ZoomOut UMETA(DisplayName = "Zoom Out"),
	RandomDirection1 UMETA(DisplayName = "Random Direction 1"),
	RandomDirection2 UMETA(DisplayName = "Random Direction 2"),
	RandomDirection3 UMETA(DisplayName = "Random Direction 3"),
	RandomDirection4 UMETA(DisplayName = "Random Direction 4"),
	RenderOnly UMETA(DisplayName = "Render Only (No Camera Movement)"),
	RenderOnly5S UMETA(DisplayName = "Render Only 5S (No Camera Movement)")
};

/**
 * An actor to capture video and data from a specific FusionCamSensor.
 * This actor moves the recording logic from FusionCamSensor to maintain better OOD.
 * Unlike DataCaptureActor which records from ALL sensors, this records from ONE sensor.
 */
UCLASS()
class UNREALCV_API AFusionCamCaptureActor : public AActor
{
	GENERATED_BODY()

public:
	AFusionCamCaptureActor();

	virtual void Tick(float DeltaTime) override;
	virtual void BeginPlay() override;


	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	void PrepareTrajectoryRecord(AActor * Target, float FPs);

	// ========== Recording Control (Neo Unified System) ==========
	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	void StartTrajectoryRecord(const FString& FileName, ECameraTrajectoryType TrajectoryType, AActor* Target, int32 FPS = 30, float DegreesPerSecond = 36.0f, int32 RandomSeed = -1, bool bPauseWorldTime = false);

	// ========== Simple Recording (No Camera Movement) ==========
	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	void StartSimpleRecording(const FString& FileName, int32 FPS, float DurationSeconds);

	/** Stop current recording */
	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	void StopRecord();

	/** Check if currently recording */
	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	bool IsRecording() const { return bIsRecording; }

	/** Set scene handle for next recording session */
	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	void SetSceneHandle(const FSceneHandle& InSceneHandle);

	// ========== Configuration ==========

	/** The FusionCamSensor to record from */
	UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture")
	class UFusionCamSensor* TargetSensor;

	/** Output folder for recorded files */
	UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture")
	FDirectoryPath DataFolder;

	/** Add timestamp to folder name */
	UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture")
	bool bAddTimestamp;

	/** Record RGB images */
	UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture| Data Types")
	bool bRecordRGB;

	/** Record segmentation masks */
	UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture| Data Types")
	bool bRecordMask;

	/** Record depth data */
	UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture| Data Types")
	bool bRecordDepth;

	/** Record normal data */
	UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture| Data Types")
	bool bRecordNormal;

	/** Record optical flow data */
	UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture| Data Types")
	bool bRecordFlow;

	UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture| Data Types")
	bool bRecordOneObjectMask;

	/** Record camera metadata (location, rotation, FOV, etc.) */
	UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture| Data Types")
	bool bRecordMetadata;

	/** Record audio from camera position */
	UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture| Data Types")
	bool bRecordAudio;

	/** Record version without target actor (for dataset generation) */
	UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture| Data Types")
	bool bRecordWithoutTarget;

	/** Bullet time rotation speed in degrees per frame */
	UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture| Bullet Time")
	float BulletTimeSpeedDeg;


	/** Bullet time rotation speed in degrees per frame */
	UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture| Data Types")
	float TimeDilation;

	/** Number of warm-up frames before recording starts (trajectory positions applied but no data recorded) */
	UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture| Recording")
	int32 WarmUpFrames;

	/** Automatically generate video from image sequences after recording */
	UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture| Video Generation")
	bool bAutoGenerateVideo;

	/** Path to genvid.py script for video generation */
	UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture| Video Generation")
	FString VideoGenScriptPath;

	/** Conda environment name for Python execution */
	UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture| Video Generation")
	FString CondaEnvName;


	static FVector GetTargetLocationWithRandomHeight(AActor* Target);

	void TriggerVideoGeneration();
	int32 GetCurrentTrajectoryIndex() const { return CurrentTrajectoryIndex; }

protected:
	// Recording state
	float TimeDilationBackUp;
	FTimerHandle TimerHandle_Record;
	FCriticalSection RecordCriticalSection;
	bool bIsRecording;
	int32 ElapsedSteps;
	FString RecordFileName;
	FString FinalDataFolder;
	int32 RecordFPS;
	AActor* TargetToHide;
	int32 NumFrames;

	FDateTime RealWorldTimeRecordingStart;
	FDateTime RealWorldTimeRecordingEnd;
	double RealWorldTimeDurationSeconds;
	double RealWorldTimeFPS;
	// bool bUseSaveToFileAPI;

	// Unified target location for trajectory calculations
	FVector UnifiedTargetLocation;

	struct FCameraPose
	{
		FVector Location;
		FRotator Rotation;
		bool bManageTransform = true;
		float DesiredEstTimeDilation = 1.0f;
	};
	TArray<FCameraPose> CurrentTrajectory;
	int32 CurrentTrajectoryIndex;
	bool bPauseWorldDuringRecord;
	FVector OriginalCameraLocation;
	FRotator OriginalCameraRotation;
	int32 WarmUpElapsedFrames;

	FSceneHandle SceneHandle;

	void OnTimerRecord();
	void RecordFrame();


	void SetDefaultParamsForTargetCamera();

	// ========== Trajectory Calculation Functions (Separated from Rendering) ==========
	TArray<FCameraPose> CalculateTrajectory(ECameraTrajectoryType TrajectoryType, AActor* Target, float DegreesPerFrame, int32 RandomSeed);
	void RenderTrajectory(const TArray<FCameraPose>& Trajectory, bool bPauseWorldTime);

	// Individual trajectory calculation functions
	TArray<FCameraPose> CalculateRotateLeft(AActor* Target, float DegreesPerFrame, float RotationDegs);
	// TArray<FCameraPose> CalculateRotateLeft45(AActor* Target, float DegreesPerFrame);
	// TArray<FCameraPose> CalculateRotateLeft30(AActor* Target, float DegreesPerFrame);
	TArray<FCameraPose> CalculateRotateRight(AActor* Target, float DegreesPerFrame, float RotationDegs);
	// TArray<FCameraPose> CalculateRotateRight45(AActor* Target, float DegreesPerFrame);
	// TArray<FCameraPose> CalculateRotateRight30(AActor* Target, float DegreesPerFrame);
	TArray<FCameraPose> CalculateRotateUp(AActor* Target, float DegreesPerFrame, float RotationDegs);
	// TArray<FCameraPose> CalculateRotateUp45(AActor* Target, float DegreesPerFrame);
	// TArray<FCameraPose> CalculateRotateUp30(AActor* Target, float DegreesPerFrame);
	TArray<FCameraPose> CalculateRotate360(AActor* Target, float DegreesPerFrame);
	TArray<FCameraPose> CalculateZoomIn(AActor* Target, float DegreesPerFrame);
	TArray<FCameraPose> CalculateZoomOut(AActor* Target, float DegreesPerFrame);
	TArray<FCameraPose> CalculateRandomDirection(AActor* Target, float DegreesPerFrame, int32 RandomSeed);
	TArray<FCameraPose> CalculateRenderOnly(float Time);
	TArray<FCameraPose> AddRotateBufferFrames(const TArray<FCameraPose>& CoreTrajectory);

	// Audio recording
	void StartAudioRecord();
	void StopAudioRecord();
	class Audio::FMixerDevice* GetAudioMixer();

	// Utility functions
	FString MakeFilename(FString DataType, FString FileExtension);
	FString MakeFilenameNew(FString DataType, FString FileExtension);
	FString MakeFilenameNewWithFolder(FString DataType, FString FileExtension);
	void SaveCameraMetadata();

private:
	UPROPERTY()
	class UMaterialBillboardComponent* Billboard;
};
