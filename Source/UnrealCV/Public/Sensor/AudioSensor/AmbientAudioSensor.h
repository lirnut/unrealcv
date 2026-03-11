// Copyright (c) 2025 UnrealCV Team
// Ambient Audio Sensor for capturing world audio
#pragma once

#include "CoreMinimal.h"
#include "Sensor/AudioSensor/BaseAudioSensor.h"
#include "Sound/SoundSubmix.h"
#include "AmbientAudioSensor.generated.h"

/**
 * Ambient Audio Sensor - Captures the overall audio output from the world
 * This captures the mixed audio that would normally go to the speakers
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UNREALCV_API UAmbientAudioSensor : public UBaseAudioSensor
{
    GENERATED_BODY()

public:
    UAmbientAudioSensor(const FObjectInitializer& ObjectInitializer);

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** Start capturing ambient audio */
    virtual void StartCapture() override;

    /** Stop capturing ambient audio */
    virtual void StopCapture() override;

    /** Get captured audio from the main output */
    FAudioCaptureData GetCapturedAudio() const override;

    /** Get captured audio and clear buffer */
    FAudioCaptureData FlushCapturedAudio() override;

protected:
    /** Initialize submix buffer listener */
    void InitializeSubmixListener();

    /** Shutdown submix buffer listener */
    void ShutdownSubmixListener();

    /** Called when audio data is received from submix */
    void OnSubmixAudioReceived(const TArray<float>& AudioData, int32 InNumChannels, int32 InSampleRate, double InTimestamp);

protected:
    /** Whether to capture from master submix or a specific submix */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "unrealcv")
    bool bUseMasterSubmix;

    /** Specific submix to capture from (if not using master) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "unrealcv")
    TObjectPtr<USoundSubmix> TargetSubmix;

    /** Whether to include all submixes in the capture */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "unrealcv")
    bool bCaptureAllSubmixes;

    /** Internal submix buffer listener class */
    class FSubmixAudioListener;
    TSharedPtr<FSubmixAudioListener> SubmixListener;
};
