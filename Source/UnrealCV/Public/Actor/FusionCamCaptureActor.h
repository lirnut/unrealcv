// Weichao Qiu @ 2018
// Modified for FusionCamSensor-specific recording
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JsonObjectBP.h"
#include "FusionCamCaptureActor.generated.h"

/**
 * Camera trajectory types for SOW camera movement requirements
 * 6 fixed trajectories + 4 random trajectories = 10 per scene
 */
UENUM(BlueprintType)
enum class ECameraTrajectoryType : uint8
{
	RotateLeft45 UMETA(DisplayName = "Rotate Left 45°"),
	RotateRight45 UMETA(DisplayName = "Rotate Right 45°"),
	RotateUp45 UMETA(DisplayName = "Rotate Up 45°"),
	Rotate360 UMETA(DisplayName = "Rotate 360°"),
	ZoomIn UMETA(DisplayName = "Zoom In"),
	ZoomOut UMETA(DisplayName = "Zoom Out"),
	RandomDirection1 UMETA(DisplayName = "Random Direction 1"),
	RandomDirection2 UMETA(DisplayName = "Random Direction 2"),
	RandomDirection3 UMETA(DisplayName = "Random Direction 3"),
	RandomDirection4 UMETA(DisplayName = "Random Direction 4")
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

	// ========== Recording Control ==========

	/** Start recording video from the target sensor */
	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	void StartRecord(const FString& FileName, float Duration, int32 FPS, AActor* TargetToHide = nullptr);

	/** Start recording with bullet time effect (360° rotation around target) */
	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	void StartBulletTimeRecord(const FString& FileName, float Duration, int32 FPS, AActor* Target);
	/** Start recording with bullet time effect (360° rotation around target) */
	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	void StartBulletTimeRecordOnly(const FString& FileName, float Duration, int32 FPS, AActor* Target);

	// ========== Camera Trajectory Recording (SOW Requirements) ==========

	/**
	 * Start camera trajectory recording
	 * @param FileName - Output filename prefix
	 * @param TrajectoryType - Type of camera movement
	 * @param Target - Target actor to orbit/focus on
	 * @param FPS - Frames per second for trajectory rendering (default 30)
	 * @param DegreesPerSecond - Rotation speed in degrees per second (default 36 deg/s = 10s for 360°)
	 * @param RandomSeed - Seed for random trajectories (optional)
	 */
	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	void StartTrajectoryRecord(const FString& FileName, ECameraTrajectoryType TrajectoryType, AActor* Target, int32 FPS = 30, float DegreesPerSecond = 36.0f, int32 RandomSeed = -1);

	/** Stop current recording */
	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	void StopRecord();

	/** Check if currently recording */
	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	bool IsRecording() const { return bIsRecording; }

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

	/** Automatically generate video from image sequences after recording */
	UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture| Video Generation")
	bool bAutoGenerateVideo;

	/** Path to genvid.py script for video generation */
	UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture| Video Generation")
	FString VideoGenScriptPath;

	/** Conda environment name for Python execution */
	UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture| Video Generation")
	FString CondaEnvName;

protected:
	// Recording state
	FTimerHandle TimerHandle_Record;
	FCriticalSection RecordCriticalSection;
	bool bIsRecording;
	float TimePerFrame;
	float ElapsedTime;
	int32 ElapsedSteps;
	FString RecordFileName;
	FString FinalDataFolder;
	int32 RecordFPS;
	float RecordDuration;
	float TimeDilationBackUp;
	AActor* TargetToHide;

	// Bullet time state
	enum class EBulletTimeState : uint8
	{
		Waiting,
		BulletTime,
		Finished
	};
	EBulletTimeState BulletTimeState;
	bool bUseBulletTime;

	// Recording callbacks
	void OnTimerRecord();
	void RecordFrame();
	void RecordBulletTimeSequence();

	// ========== Trajectory Calculation Functions (Separated from Rendering) ==========

	/**
	 * Represents a single camera pose in a trajectory
	 */
	struct FCameraPose
	{
		FVector Location;
		FRotator Rotation;
	};

	/**
	 * Calculate camera trajectory based on type
	 * @param TrajectoryType - Type of camera movement
	 * @param Target - Target actor to orbit/focus on
	 * @param DegreesPerFrame - Rotation speed in degrees per frame
	 * @param RandomSeed - Seed for random trajectories
	 * @return Array of camera poses
	 */
	TArray<FCameraPose> CalculateTrajectory(ECameraTrajectoryType TrajectoryType, AActor* Target, float DegreesPerFrame, int32 RandomSeed);

	/**
	 * Generic trajectory rendering function
	 * @param Trajectory - Array of camera poses to render
	 * Renders all frames in the trajectory and saves data
	 */
	void RenderTrajectory(const TArray<FCameraPose>& Trajectory);

	// Individual trajectory calculation functions
	TArray<FCameraPose> CalculateRotateLeft45(AActor* Target, float DegreesPerFrame);
	TArray<FCameraPose> CalculateRotateRight45(AActor* Target, float DegreesPerFrame);
	TArray<FCameraPose> CalculateRotateUp45(AActor* Target, float DegreesPerFrame);
	TArray<FCameraPose> CalculateRotate360(AActor* Target, float DegreesPerFrame);
	TArray<FCameraPose> CalculateZoomIn(AActor* Target, float DegreesPerFrame);
	TArray<FCameraPose> CalculateZoomOut(AActor* Target, float DegreesPerFrame);
	TArray<FCameraPose> CalculateRandomDirection(AActor* Target, float DegreesPerFrame, int32 RandomSeed);

	// Audio recording
	void StartAudioRecord();
	void StopAudioRecord();
	class Audio::FMixerDevice* GetAudioMixer();

	// Utility functions
	FString MakeFilename(FString DataType, FString FileExtension);
	FString MakeFilenameNew(FString DataType, FString FileExtension);
	void SaveCameraMetadata();
	void TriggerVideoGeneration();

private:
	UPROPERTY()
	class UMaterialBillboardComponent* Billboard;
};
