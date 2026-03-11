// Copyright (c) 2025 UnrealCV Team
// Audio Source Sensor for capturing audio from a specific audio component
#pragma once

#include "CoreMinimal.h"
#include "Sensor/AudioSensor/BaseAudioSensor.h"
#include "Components/AudioComponent.h"
#include "AudioSourceSensor.generated.h"

/**
 * Delegate for per-source audio capture events
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSourceAudioCaptured, int32, SourceId, const TArray<float>&, AudioData, float, Timestamp);

/**
 * Audio source listener implementation
 * Captures audio data from a specific source in the audio mixer
 */
class FSourceAudioListener : public ISourceBufferListener
{
public:
    FSourceAudioListener(UBaseAudioSensor* InOwnerSensor);
    virtual ~FSourceAudioListener();

    // ISourceBufferListener interface
    virtual void OnNewBuffer(const FOnNewBufferParams& InParams) override;
    virtual void OnSourceReleased(const int32 InSourceId) override;

    // Get the last captured audio data
    void GetCapturedAudio(TArray<float>& OutAudio);
    void ClearCapturedAudio();

    // Get current source info
    int32 GetCurrentSourceId() const { return CurrentSourceId.load(); }
    bool IsCapturing() const { return bIsCapturing.load(); }

    void StartCapture();
    void StopCapture();

private:
    /** Owner sensor for callbacks */
    TWeakObjectPtr<UBaseAudioSensor> OwnerSensor;

    /** Captured audio data (thread-safe) */
    TArray<float> CapturedAudio;
    FCriticalSection AudioDataCriticalSection;

    /** Current source being captured */
    std::atomic<int32> CurrentSourceId;

    /** Whether currently capturing */
    std::atomic<bool> bIsCapturing;

    /** Whether to zero buffer after capture */
    bool bShouldZeroBuffer;

    /** Timestamp of last capture */
    std::atomic<double> LastCaptureTime;
};

/**
 * Audio Source Sensor - Captures audio from a specific audio component
 * Uses ISourceBufferListener to get pre-distance-attenuation audio data
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UNREALCV_API UAudioSourceSensor : public UBaseAudioSensor
{
    GENERATED_BODY()

public:
    UAudioSourceSensor(const FObjectInitializer& ObjectInitializer);

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** Start capturing from a specific audio component */
    UFUNCTION(BlueprintCallable, Category = "unrealcv")
    void StartCaptureFromComponent(UAudioComponent* TargetAudioComponent);

    /** Start capturing from a specific actor (uses first found AudioComponent) */
    UFUNCTION(BlueprintCallable, Category = "unrealcv")
    void StartCaptureFromActor(AActor* TargetActor);

    /** Stop capturing and detach from source */
    virtual void StopCapture() override;

    /** Get the target audio component */
    UFUNCTION(BlueprintPure, Category = "unrealcv")
    UAudioComponent* GetTargetAudioComponent() const { return TargetAudioComp; }

    /** Set target audio component */
    UFUNCTION(BlueprintCallable, Category = "unrealcv")
    void SetTargetAudioComponent(UAudioComponent* InAudioComponent);

    /** Get captured audio with source-specific metadata */
    FAudioCaptureData GetCapturedAudio() const override;

    /** Get the current source ID being captured */
    UFUNCTION(BlueprintPure, Category = "unrealcv")
    int32 GetCurrentSourceId() const;

    /** Check if currently attached to a valid source */
    UFUNCTION(BlueprintPure, Category = "unrealcv")
    bool IsAttachedToSource() const;

    /** Event dispatched when audio data is captured */
    UPROPERTY(BlueprintAssignable, Category = "unrealcv")
    FOnSourceAudioCaptured OnSourceAudioCaptured;

protected:
    /** Attach to the target audio component */
    void AttachToAudioComponent();

    /** Detach from current audio component */
    void DetachFromAudioComponent();

    /** Called when the audio source is released */
    void HandleSourceReleased();

protected:
    /** Target audio component to capture from */
    UPROPERTY(BlueprintReadOnly, Category = "unrealcv")
    TObjectPtr<UAudioComponent> TargetAudioComp;

    /** Whether to zero the audio buffer after capturing (prevents audio from playing in world) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "unrealcv")
    bool bZeroBufferAfterCapture;

    /** Whether to capture audio before distance attenuation is applied */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "unrealcv")
    bool bCapturePreAttenuation;

    /** The source audio listener */
    TSharedPtr<FSourceAudioListener> SourceListener;

    /** Shared listener pointer for AudioComponent */
    FSharedISourceBufferListenerPtr SharedListenerPtr;
};
