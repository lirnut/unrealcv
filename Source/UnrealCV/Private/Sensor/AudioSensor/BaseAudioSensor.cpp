// Copyright (c) 2025 UnrealCV Team

#include "Sensor/AudioSensor/BaseAudioSensor.h"
#include "UnrealcvLog.h"
#include "Audio.h"
#include "AudioDevice.h"
#include "Engine/Engine.h"

DEFINE_LOG_CATEGORY(LogAudioSensorBase);

UBaseAudioSensor::UBaseAudioSensor(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
    , CaptureMode(EAudioCaptureMode::Ambient)
    , SampleRate(48000)
    , NumChannels(2)
    , MaxCaptureDuration(0.0f)
    , bAutoStartCapture(false)
    , bIsCapturing(false)
    , AudioDevice(nullptr)
    , CurrentCaptureTime(0.0f)
    , StartTimestamp(0.0)
    , DeviceId(0)
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UBaseAudioSensor::BeginPlay()
{
    Super::BeginPlay();

    // Get the audio device
    if (GEngine)
    {
        AudioDevice = GEngine->GetMainAudioDeviceRaw();
        if (AudioDevice)
        {
            DeviceId = AudioDevice->DeviceID;
            SampleRate = AudioDevice->GetSampleRate();
        }
    }

    if (bAutoStartCapture)
    {
        StartCapture();
    }
}

void UBaseAudioSensor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopCapture();
    Super::EndPlay(EndPlayReason);
}

void UBaseAudioSensor::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bIsCapturing)
    {
        CurrentCaptureTime += DeltaTime;

        // Check if max duration reached
        if (MaxCaptureDuration > 0.0f && CurrentCaptureTime >= MaxCaptureDuration)
        {
            UE_LOG(LogAudioSensorBase, Log, TEXT("AudioSensor: Max capture duration reached"));
            StopCapture();
            OnCaptureFinished();
        }
    }
}

void UBaseAudioSensor::StartCapture()
{
    if (bIsCapturing)
    {
        UE_LOG(LogAudioSensorBase, Warning, TEXT("AudioSensor: Already capturing"));
        return;
    }

    InitializeCapture();

    bIsCapturing = true;
    CurrentCaptureTime = 0.0f;
    StartTimestamp = FPlatformTime::Seconds();

    UE_LOG(LogAudioSensorBase, Log, TEXT("AudioSensor: Capture started (Mode: %d, SampleRate: %d, Channels: %d)"),
        static_cast<int32>(CaptureMode), SampleRate, NumChannels);
}

void UBaseAudioSensor::StopCapture()
{
    if (!bIsCapturing)
    {
        return;
    }

    bIsCapturing = false;
    ShutdownCapture();

    UE_LOG(LogAudioSensorBase, Log, TEXT("AudioSensor: Capture stopped (Duration: %.3f seconds)"), CurrentCaptureTime);
}

FAudioCaptureData UBaseAudioSensor::GetCapturedAudio() const
{
    FScopeLock Lock(&AudioBufferCriticalSection);

    FAudioCaptureData Data;
    Data.Samples = CapturedAudio;
    Data.NumChannels = NumChannels;
    Data.SampleRate = SampleRate;
    Data.Timestamp = StartTimestamp;
    Data.Duration = CurrentCaptureTime;

    return Data;
}

FAudioCaptureData UBaseAudioSensor::FlushCapturedAudio()
{
    FScopeLock Lock(&AudioBufferCriticalSection);

    FAudioCaptureData Data;
    Data.Samples = MoveTemp(CapturedAudio);
    Data.NumChannels = NumChannels;
    Data.SampleRate = SampleRate;
    Data.Timestamp = StartTimestamp;
    Data.Duration = CurrentCaptureTime;

    CapturedAudio.Empty();

    return Data;
}

void UBaseAudioSensor::ClearCapturedAudio()
{
    FScopeLock Lock(&AudioBufferCriticalSection);
    CapturedAudio.Reset();
}

void UBaseAudioSensor::InitializeCapture()
{
    // Base implementation - subclasses override
    ClearCapturedAudio();
}

void UBaseAudioSensor::ShutdownCapture()
{
    // Base implementation - subclasses override
}

void UBaseAudioSensor::ProcessAudioData(const float* AudioData, int32 NumSamples, int32 InNumChannels)
{
    if (!AudioData || NumSamples <= 0)
    {
        return;
    }

    FScopeLock Lock(&AudioBufferCriticalSection);

    int32 StartIndex = CapturedAudio.Num();
    CapturedAudio.AddUninitialized(NumSamples);
    FMemory::Memcpy(&CapturedAudio[StartIndex], AudioData, NumSamples * sizeof(float));

    // Check if we need to trim the buffer based on max duration
    if (MaxCaptureDuration > 0.0f)
    {
        int32 MaxSamples = static_cast<int32>(MaxCaptureDuration * SampleRate * InNumChannels);
        if (CapturedAudio.Num() > MaxSamples)
        {
            // Remove oldest samples
            int32 ExcessSamples = CapturedAudio.Num() - MaxSamples;
            CapturedAudio.RemoveAt(0, ExcessSamples);
        }
    }
}
