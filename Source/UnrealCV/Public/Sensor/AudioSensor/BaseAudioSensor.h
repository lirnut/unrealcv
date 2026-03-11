// Copyright (c) 2025 UnrealCV Team
// Audio Sensor for capturing audio from the game world
#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Audio.h"
#include "Sound/SoundAttenuation.h"
#include "BaseAudioSensor.generated.h"

UENUM(BlueprintType)
enum class EAudioCaptureMode : uint8
{
    /** Capture all ambient audio in the world */
    Ambient,
    /** Capture audio from a specific source attached to this component */
    AttachedSource,
    /** Capture audio from a specific actor's audio component */
    TargetActor,
    /** Capture audio from all sources within a radius */
    Proximity
};

UENUM(BlueprintType)
enum class EAudioBufferFormat : uint8
{
    /** 32-bit float PCM */
    Float32,
    /** 16-bit integer PCM */
    Int16,
    /** 8-bit integer PCM */
    Int8
};

/**
 * Struct containing captured audio data
 */
USTRUCT(BlueprintType)
struct FAudioCaptureData
{
    GENERATED_BODY()

    /** Raw audio samples (interleaved for multi-channel) */
    UPROPERTY(BlueprintReadOnly, Category = "unrealcv")
    TArray<float> Samples;

    /** Number of channels */
    UPROPERTY(BlueprintReadOnly, Category = "unrealcv")
    int32 NumChannels;

    /** Sample rate in Hz */
    UPROPERTY(BlueprintReadOnly, Category = "unrealcv")
    int32 SampleRate;

    /** Duration in seconds */
    UPROPERTY(BlueprintReadOnly, Category = "unrealcv")
    float Duration;

    /** Timestamp when captured */
    UPROPERTY(BlueprintReadOnly, Category = "unrealcv")
    float Timestamp;

    FAudioCaptureData()
        : NumChannels(2)
        , SampleRate(48000)
        , Duration(0.0f)
        , Timestamp(0.0f)
    {}

    /** Get number of sample frames */
    int32 GetNumFrames() const
    {
        return NumChannels > 0 ? Samples.Num() / NumChannels : 0;
    }
};

/**
 * Base class for audio sensors
 * Provides common functionality for capturing audio from the game world
 */
UCLASS(Abstract, Blueprintable)
class UNREALCV_API UBaseAudioSensor : public USceneComponent
{
    GENERATED_BODY()

public:
    UBaseAudioSensor(const FObjectInitializer& ObjectInitializer);

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    /** Start capturing audio */
    UFUNCTION(BlueprintCallable, Category = "unrealcv")
    virtual void StartCapture();

    /** Stop capturing audio */
    UFUNCTION(BlueprintCallable, Category = "unrealcv")
    virtual void StopCapture();

    /** Check if currently capturing */
    UFUNCTION(BlueprintPure, Category = "unrealcv")
    bool IsCapturing() const { return bIsCapturing; }

    /** Get the latest captured audio data */
    UFUNCTION(BlueprintPure, Category = "unrealcv")
    virtual FAudioCaptureData GetCapturedAudio() const;

    /** Get all captured audio data and clear the buffer */
    UFUNCTION(BlueprintCallable, Category = "unrealcv")
    virtual FAudioCaptureData FlushCapturedAudio();

    /** Clear captured audio buffer */
    UFUNCTION(BlueprintCallable, Category = "unrealcv")
    void ClearCapturedAudio();

    /** Set capture duration (0 = unlimited) */
    UFUNCTION(BlueprintCallable, Category = "unrealcv")
    void SetMaxCaptureDuration(float InMaxDuration) { MaxCaptureDuration = InMaxDuration; }

    /** Get current capture settings */
    UFUNCTION(BlueprintPure, Category = "unrealcv")
    int32 GetSampleRate() const { return SampleRate; }

    UFUNCTION(BlueprintPure, Category = "unrealcv")
    int32 GetNumChannels() const { return NumChannels; }

    UFUNCTION(BlueprintPure, Category = "unrealcv")
    EAudioCaptureMode GetCaptureMode() const { return CaptureMode; }

    /** Called when capture buffer is full or max duration reached */
    UFUNCTION(BlueprintImplementableEvent, Category = "unrealcv")
    void OnCaptureFinished();

    /** Called when new audio data is available */
    UFUNCTION(BlueprintImplementableEvent, Category = "unrealcv")
    void OnAudioDataReceived(float Timestamp, int32 NumFrames);

protected:
    /** Initialize the audio capture */
    virtual void InitializeCapture();

    /** Shutdown the audio capture */
    virtual void ShutdownCapture();

    /** Process captured audio (called from audio thread) */
    virtual void ProcessAudioData(const float* AudioData, int32 NumSamples, int32 InNumChannels);

    /** Capture mode */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "unrealcv")
    EAudioCaptureMode CaptureMode;

    /** Target sample rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "unrealcv")
    int32 SampleRate;

    /** Number of channels to capture */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "unrealcv")
    int32 NumChannels;

    /** Maximum capture duration in seconds (0 = unlimited) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "unrealcv")
    float MaxCaptureDuration;

    /** Whether to automatically start capture on begin play */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "unrealcv")
    bool bAutoStartCapture;

    /** Whether this sensor is currently capturing */
    std::atomic<bool> bIsCapturing;

    /** Audio device reference */
    FAudioDevice* AudioDevice;

    /** Captured audio data (thread-safe access) */
    TArray<float> CapturedAudio;
    mutable FCriticalSection AudioBufferCriticalSection;

    /** Current capture time */
    float CurrentCaptureTime;

    /** Start timestamp */
    double StartTimestamp;

    /** Device ID for the audio device */
    uint32 DeviceId;
};
