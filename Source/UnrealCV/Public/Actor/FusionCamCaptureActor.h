// Weichao Qiu @ 2018
// Modified for FusionCamSensor-specific recording
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JsonObjectBP.h"
#include "BPFunctionLib\SceneCompositionBPLib.h"
#include "MovieQualityRenderSubsystem.h"
#include "Encoder/UnrealCVMP4Encoder.h"
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

USTRUCT(BlueprintType)
struct FRecordingDataTypesConfig
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Recording")
	bool bRecordRGB = true;

	UPROPERTY(BlueprintReadWrite, Category = "Recording")
	bool bRecordMask = false;

	UPROPERTY(BlueprintReadWrite, Category = "Recording")
	bool bRecordDepth = false;

	UPROPERTY(BlueprintReadWrite, Category = "Recording")
	bool bRecordNormal = false;

	UPROPERTY(BlueprintReadWrite, Category = "Recording")
	bool bRecordFlow = false;

	UPROPERTY(BlueprintReadWrite, Category = "Recording")
	bool bRecordOneObjectMask = false;

	UPROPERTY(BlueprintReadWrite, Category = "Recording")
	bool bRecordOneObjectLit = false;

	UPROPERTY(BlueprintReadWrite, Category = "Recording")
	bool bRecordShadowCatcher = false;

	UPROPERTY(BlueprintReadWrite, Category = "Recording")
	bool bRecordStencilMask = false;

	UPROPERTY(BlueprintReadWrite, Category = "Recording")
	bool bRecordMetadata = true;

	UPROPERTY(BlueprintReadWrite, Category = "Recording")
	bool bRecordAudio = true;

	UPROPERTY(BlueprintReadWrite, Category = "Recording")
	bool bRecordWithoutTarget = false;

	static FRecordingDataTypesConfig MakeTrajectoryConfig()
	{
		FRecordingDataTypesConfig Config;
		Config.bRecordAudio = false;
		Config.bRecordRGB = true;
		Config.bRecordMask = true;
		Config.bRecordOneObjectLit = true;
		Config.bRecordMetadata = true;
		return Config;
	}

	static FRecordingDataTypesConfig MakeOmnimatteConfig()
	{
		FRecordingDataTypesConfig Config;
		Config.bRecordAudio = true;
		Config.bRecordRGB = true;
		Config.bRecordMask = true;
		Config.bRecordOneObjectMask = true;
		Config.bRecordShadowCatcher = true;
		Config.bRecordStencilMask = true;
		Config.bRecordMetadata = true;
		Config.bRecordWithoutTarget = true;
		return Config;
	}

	static FRecordingDataTypesConfig MakeSpeedTestConfig()
	{
		FRecordingDataTypesConfig Config;
		Config.bRecordRGB = true;
		Config.bRecordMetadata = true;
		return Config;
	}

	static FRecordingDataTypesConfig ParseRecordingOptions(const FString& OptionsStr)
	{
		FRecordingDataTypesConfig Config;

		if (OptionsStr.IsEmpty())
		{
			Config.bRecordRGB = true;
			return Config;
		}

		TArray<FString> Options;
		OptionsStr.ParseIntoArray(Options, TEXT(","), true);

		for (const FString& Option : Options)
		{
			FString Trimmed = Option.TrimStartAndEnd().ToLower();

			if (Trimmed == TEXT("lit") || Trimmed == TEXT("rgb"))
			{
				Config.bRecordRGB = true;
			}
			else if (Trimmed == TEXT("object_mask") || Trimmed == TEXT("seg") || Trimmed == TEXT("mask"))
			{
				Config.bRecordMask = true;
			}
			else if (Trimmed == TEXT("normal"))
			{
				Config.bRecordNormal = true;
			}
			else if (Trimmed == TEXT("depth"))
			{
				Config.bRecordDepth = true;
			}
			else if (Trimmed == TEXT("optical_flow") || Trimmed == TEXT("flow"))
			{
				Config.bRecordFlow = true;
			}
			else if (Trimmed == TEXT("one_object_mask") || Trimmed == TEXT("oneobjmask"))
			{
				Config.bRecordOneObjectMask = true;
			}
			else if (Trimmed == TEXT("one_object_lit") || Trimmed == TEXT("oneobjlit"))
			{
				Config.bRecordOneObjectLit = true;
			}
			else if (Trimmed == TEXT("shadow_catcher") || Trimmed == TEXT("shadowcatcher"))
			{
				Config.bRecordShadowCatcher = true;
			}
			else if (Trimmed == TEXT("stencil_mask") || Trimmed == TEXT("stencilmask"))
			{
				Config.bRecordStencilMask = true;
			}
			else if (Trimmed == TEXT("metadata"))
			{
				Config.bRecordMetadata = true;
			}
			else if (Trimmed == TEXT("audio"))
			{
				Config.bRecordAudio = true;
			}
			else if (Trimmed == TEXT("without_target") || Trimmed == TEXT("woTarget"))
			{
				Config.bRecordWithoutTarget = true;
			}
		}

		if (!Config.bRecordRGB && !Config.bRecordMask && !Config.bRecordNormal &&
		    !Config.bRecordDepth && !Config.bRecordFlow && !Config.bRecordOneObjectMask &&
		    !Config.bRecordOneObjectLit && !Config.bRecordShadowCatcher && !Config.bRecordStencilMask)
		{
			Config.bRecordRGB = true;
		}

		return Config;
	}
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
	void StartTrajectoryRecord(const FString& FileName, ECameraTrajectoryType TrajectoryType, AActor* Target, int32 FPS = 30, int32 InNumFrames = 121, int32 RandomSeed = -1, bool bPauseWorldTime = false);

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

	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	void ApplyRecordingConfig(const FRecordingDataTypesConfig& InConfig);

	// ========== Configuration ==========

	/** The FusionCamSensor to record from */
	UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture")
	class UFusionCamSensor* TargetSensor;

	/** Backup sensor for recording without target (created automatically when bRecordWithoutTarget is true) */
	UPROPERTY()
	class UFusionCamSensor* BackupSensor;

	/** Backup camera ID for destroying camera when done */
	int32 BackupCameraID;

	/** Use MRQ rendering pipeline for RGB (higher quality) */
	UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture")
	bool bUseMovieQualityRendering;

	/** Output folder for recorded files */
	UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture")
	FDirectoryPath DataFolder;

	/** Add timestamp to folder name */
	UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture")
	bool bAddTimestamp;

	UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture")
	bool bTrackForegroundMovement;

	UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture")
	float ForegroundMoveSpeed;

	UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture")
	float ForegroundMoveAngleOffset;

	UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture")
	float TargetHeightOffset;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "FusionCamCapture| Data Types")
	FRecordingDataTypesConfig RecordingDataTypes;

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


	FVector GetTargetLocationWithOffset(AActor* Target);

	static void CopySensorSettings(UFusionCamSensor* Source, UFusionCamSensor* Target);

	void TriggerVideoGeneration();
	int32 GetCurrentTrajectoryIndex() const { return CurrentTrajectoryIndex; }

protected:
	UMovieQualityRenderSubsystem* MovieQualityRenderer;

	TUniquePtr<class FUnrealCVMP4Encoder> MP4Encoder;
	FString MP4OutputPath;
	int32 MP4EncodedFrameCount;
	bool bEnableH264Encoding;

	// Recording state
	float TimeDilationBackUp;
	FTimerHandle TimerHandle_Record;
	FCriticalSection RecordCriticalSection;
	bool bIsRecording;
	int32 ElapsedSteps;
	FString RecordFileName;
	FString FinalDataFolder;
	int32 RecordFPS;
	AActor* TargetForeground;
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

	/** Foreground actor position at recording start (for moving foreground tracking) */
	FVector ForegroundStartPos;

	void OnTimerRecord();
	void RecordFrame(bool bWarmUp = false);
	void UpdateFocalDistance();

	void SetDefaultParamsForTargetCamera();

	// ========== Trajectory Calculation Functions (Separated from Rendering) ==========
	TArray<FCameraPose> CalculateTrajectory(ECameraTrajectoryType TrajectoryType, AActor* Target, int32 InNumFrames, int32 RandomSeed);
	void RenderTrajectory(const TArray<FCameraPose>& Trajectory, bool bPauseWorldTime);

	// Individual trajectory calculation functions
	TArray<FCameraPose> CalculateRotateLeft(AActor* Target, int32 InNumFrames, float RotationDegs);
	// TArray<FCameraPose> CalculateRotateLeft45(AActor* Target, int32 InNumFrames);
	// TArray<FCameraPose> CalculateRotateLeft30(AActor* Target, int32 InNumFrames);
	TArray<FCameraPose> CalculateRotateRight(AActor* Target, int32 InNumFrames, float RotationDegs);
	// TArray<FCameraPose> CalculateRotateRight45(AActor* Target, int32 InNumFrames);
	// TArray<FCameraPose> CalculateRotateRight30(AActor* Target, int32 InNumFrames);
	TArray<FCameraPose> CalculateRotateUp(AActor* Target, int32 InNumFrames, float RotationDegs);
	// TArray<FCameraPose> CalculateRotateUp45(AActor* Target, int32 InNumFrames);
	// TArray<FCameraPose> CalculateRotateUp30(AActor* Target, int32 InNumFrames);
	TArray<FCameraPose> CalculateRotate360(AActor* Target, int32 InNumFrames);
	TArray<FCameraPose> CalculateZoomIn(AActor* Target, int32 InNumFrames);
	TArray<FCameraPose> CalculateZoomOut(AActor* Target, int32 InNumFrames);
	TArray<FCameraPose> CalculateRandomDirection(AActor* Target, int32 InNumFrames, int32 RandomSeed);
	TArray<FCameraPose> CalculateRenderOnly(int32 InNumFrames);
	TArray<FCameraPose> AddRotateBufferFrames(const TArray<FCameraPose>& CoreTrajectory);
	TArray<FCameraPose> AddHandheldShake(const TArray<FCameraPose>& InputTrajectory);

	// Audio recording
	void StartAudioRecord();
	void StopAudioRecord();
	class Audio::FMixerDevice* GetAudioMixer();

	// Utility functions
	FString MakeFilenameNew(FString DataType, FString FileExtension);
	FString MakeFilenameNewWithFolder(FString DataType, FString FileExtension);
	void SaveOverviewMetadata();
	void SaveCameraMetadata();

private:
	UPROPERTY()
	class UMaterialBillboardComponent* Billboard;
};
