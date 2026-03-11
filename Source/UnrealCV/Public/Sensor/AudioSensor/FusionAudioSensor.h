// Copyright (c) 2025 UnrealCV Team
#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Sensor/AudioSensor/BaseAudioSensor.h"
#include "Sensor/AudioSensor/AudioSourceSensor.h"
#include "Sensor/AudioSensor/AmbientAudioSensor.h"
#include "FusionAudioSensor.generated.h"

UENUM(BlueprintType)
enum class EAudioCapturePreset : uint8
{
    /** Capture all audio in the world (ambient) */
    Ambient,
    /** Capture from a specific target source */
    SingleSource,
    /** Capture both ambient and a specific source separately */
    SplitCapture,
    /** Capture from multiple sources (mixed) */
    MultiSource
};

// Delegate declarations
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAudioCaptureFinished);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAudioDataReceived, float, Timestamp, int32, NumFrames);

/**
 * Fusion Audio Sensor - Combines multiple audio sensors for comprehensive audio capture
 * Provides a unified interface for capturing audio from different sources simultaneously
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UNREALCV_API UFusionAudioSensor : public USceneComponent
{
    GENERATED_BODY()

public:
    UFusionAudioSensor(const FObjectInitializer& ObjectInitializer);

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** Initialize the fusion audio sensor with the specified preset */
    UFUNCTION(BlueprintCallable, Category = "unrealcv|Audio")
    void Initialize(EAudioCapturePreset Preset);

    /** Start capturing audio based on current configuration */
    UFUNCTION(BlueprintCallable, Category = "unrealcv|Audio")
    void StartCapture();

    /** Stop all audio capture */
    UFUNCTION(BlueprintCallable, Category = "unrealcv|Audio")
    void StopCapture();

    /** Check if currently capturing */
    UFUNCTION(BlueprintPure, Category = "unrealcv|Audio")
    bool IsCapturing() const;

    /** Get ambient audio data */
    UFUNCTION(BlueprintPure, Category = "unrealcv|Audio")
    FAudioCaptureData GetAmbientAudioData() const;

    /** Get source-specific audio data */
    UFUNCTION(BlueprintPure, Category = "unrealcv|Audio")
    FAudioCaptureData GetSourceAudioData() const;

    /** Get all captured audio data and clear buffers */
    UFUNCTION(BlueprintCallable, Category = "unrealcv|Audio")
    void FlushAllAudioData(FAudioCaptureData& OutAmbient, FAudioCaptureData& OutSource);

    /** Set the target audio component for source capture */
    UFUNCTION(BlueprintCallable, Category = "unrealcv|Audio")
    void SetTargetAudioComponent(UAudioComponent* InTargetAudioComponent);

    /** Get the target audio component */
    UFUNCTION(BlueprintPure, Category = "unrealcv|Audio")
    UAudioComponent* GetTargetAudioComponent() const;

    /** Start capturing ambient audio only */
    UFUNCTION(BlueprintCallable, Category = "unrealcv|Audio")
    void StartAmbientCapture();

    /** Stop capturing ambient audio */
    UFUNCTION(BlueprintCallable, Category = "unrealcv|Audio")
    void StopAmbientCapture();

    /** Start capturing from the target source */
    UFUNCTION(BlueprintCallable, Category = "unrealcv|Audio")
    void StartSourceCapture();

    /** Stop capturing from source */
    UFUNCTION(BlueprintCallable, Category = "unrealcv|Audio")
    void StopSourceCapture();

    /** Save ambient audio to WAV file */
    UFUNCTION(BlueprintCallable, Category = "unrealcv|Audio")
    void SaveAmbientAudioToFile(const FString& Filename);

    /** Save source audio to WAV file */
    UFUNCTION(BlueprintCallable, Category = "unrealcv|Audio")
    void SaveSourceAudioToFile(const FString& Filename);

    /** Save both audio streams to separate files */
    UFUNCTION(BlueprintCallable, Category = "unrealcv|Audio")
    void SaveAllAudioToFiles(const FString& AmbientFilename, const FString& SourceFilename);

    /** Get the ambient audio sensor (for advanced use) */
    UFUNCTION(BlueprintCallable, Category = "unrealcv|Audio")
    UAmbientAudioSensor* GetAmbientAudioSensor() const { return AmbientSensor; }

    /** Get the source audio sensor (for advanced use) */
    UFUNCTION(BlueprintCallable, Category = "unrealcv|Audio")
    UAudioSourceSensor* GetSourceAudioSensor() const { return SourceSensor; }

    /** Set sample rate for all sensors */
    UFUNCTION(BlueprintCallable, Category = "unrealcv|Audio")
    void SetSampleRate(int32 InSampleRate);

    /** Set number of channels for all sensors */
    UFUNCTION(BlueprintCallable, Category = "unrealcv|Audio")
    void SetNumChannels(int32 InNumChannels);

    /** Set maximum capture duration */
    UFUNCTION(BlueprintCallable, Category = "unrealcv|Audio")
    void SetMaxCaptureDuration(float InMaxDuration);

    /** Event: Called when ambient capture finishes */
    UPROPERTY(BlueprintAssignable, Category = "unrealcv|Audio")
    FOnAudioCaptureFinished OnAmbientCaptureFinished;

    /** Event: Called when source capture finishes */
    UPROPERTY(BlueprintAssignable, Category = "unrealcv|Audio")
    FOnAudioCaptureFinished OnSourceCaptureFinished;

    /** Event: Called when new audio data is received from ambient sensor */
    UPROPERTY(BlueprintAssignable, Category = "unrealcv|Audio")
    FOnAudioDataReceived OnAmbientDataReceived;

    /** Event: Called when new audio data is received from source sensor */
    UPROPERTY(BlueprintAssignable, Category = "unrealcv|Audio")
    FOnAudioDataReceived OnSourceDataReceived;

protected:
    /** Ambient audio sensor */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "unrealcv|Audio")
    TObjectPtr<UAmbientAudioSensor> AmbientSensor;

    /** Source-specific audio sensor */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "unrealcv|Audio")
    TObjectPtr<UAudioSourceSensor> SourceSensor;

    /** Current capture preset */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "unrealcv|Audio")
    EAudioCapturePreset CapturePreset;

    /** Whether to auto-start capture on begin play */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "unrealcv|Audio")
    bool bAutoStartCapture;

    /** Target audio component for source capture */
    UPROPERTY(BlueprintReadOnly, Category = "unrealcv|Audio")
    TObjectPtr<UAudioComponent> TargetAudioComponent;

    /** Whether to save audio data to files automatically on capture finish */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "unrealcv|Audio")
    bool bAutoSaveToFile;

    /** Base filename for auto-save (will append _ambient.wav and _source.wav) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "unrealcv|Audio")
    FString AutoSaveBaseFilename;

private:
    /** Delegate callbacks */
    void HandleAmbientCaptureFinished();
    void HandleSourceCaptureFinished();
    void HandleAmbientDataReceived(float Timestamp, int32 NumFrames);
    void HandleSourceDataReceived(float Timestamp, int32 NumFrames);

    /** Helper function to save audio data to WAV file */
    void SaveAudioToWavFile(const FAudioCaptureData& AudioData, const FString& Filename);
};
