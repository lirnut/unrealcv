// Weichao Qiu @ 2018
// Modified for FusionCamSensor-specific recording
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JsonObjectBP.h"
#include "FusionCamCaptureActor.generated.h"

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
	float TimeDilation;
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

	// Audio recording
	void StartAudioRecord();
	void StopAudioRecord();
	class Audio::FMixerDevice* GetAudioMixer();

	// Utility functions
	FString MakeFilename(FString DataType, FString FileExtension);
	void SaveCameraMetadata();

private:
	UPROPERTY()
	class UMaterialBillboardComponent* Billboard;
};
