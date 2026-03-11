// Copyright (c) 2025 UnrealCV Team
// Example usage of AudioSensor
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sensor/AudioSensor/BaseAudioSensor.h"
#include "Sensor/AudioSensor/AudioSourceSensor.h"
#include "Sensor/AudioSensor/AmbientAudioSensor.h"
#include "AudioSensorExample.generated.h"

/**
 * Example actor demonstrating AudioSensor usage
 */
UCLASS()
class AAudioSensorExample : public AActor
{
    GENERATED_BODY()

public:
    AAudioSensorExample(const FObjectInitializer& ObjectInitializer);

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** Start capturing ambient audio from the world */
    UFUNCTION(BlueprintCallable, Category = "AudioSensorExample")
    void StartAmbientCapture();

    /** Stop ambient audio capture */
    UFUNCTION(BlueprintCallable, Category = "AudioSensorExample")
    void StopAmbientCapture();

    /** Start capturing from the attached audio component */
    UFUNCTION(BlueprintCallable, Category = "AudioSensorExample")
    void StartSourceCapture();

    /** Stop source audio capture */
    UFUNCTION(BlueprintCallable, Category = "AudioSensorExample")
    void StopSourceCapture();

    /** Get the captured audio data */
    UFUNCTION(BlueprintCallable, Category = "AudioSensorExample")
    FAudioCaptureData GetCapturedAudio();

    /** Play the captured audio (for testing) */
    UFUNCTION(BlueprintCallable, Category = "AudioSensorExample")
    void PlayCapturedAudio();

    /** Save captured audio to WAV file */
    UFUNCTION(BlueprintCallable, Category = "AudioSensorExample")
    void SaveAudioToFile(const FString& Filename);

protected:
    /** Audio component for generating test audio */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AudioSensorExample")
    TObjectPtr<UAudioComponent> TestAudioComponent;

    /** Ambient audio sensor */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AudioSensorExample")
    TObjectPtr<UAmbientAudioSensor> AmbientSensor;

    /** Source audio sensor */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AudioSensorExample")
    TObjectPtr<UAudioSourceSensor> SourceSensor;

    /** Sound to play for testing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AudioSensorExample")
    TObjectPtr<USoundBase> TestSound;
};
