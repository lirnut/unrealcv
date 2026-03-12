// Copyright (c) 2025 UnrealCV Team

#include "Sensor/AudioSensor/AmbientAudioSensor.h"
#include "UnrealcvLog.h"
#include "Audio.h"
#include "AudioDevice.h"
#include "Sound/SoundSubmix.h"
#include "Engine/Engine.h"
#include "Async/Async.h"
#include "ISubmixBufferListener.h"

DEFINE_LOG_CATEGORY(LogAmbientAudioSensor);

/**
 * Internal submix buffer listener class
 */
class UAmbientAudioSensor::FSubmixAudioListener : public ISubmixBufferListener
{
public:
    FSubmixAudioListener(UAmbientAudioSensor* InOwner)
        : Owner(InOwner)
        , SampleRate(48000)
        , NumChannels(2)
        , bIsCapturing(false)
    {
    }

    virtual ~FSubmixAudioListener()
    {
    }

    void StartCapture()
    {
        bIsCapturing.store(true);
        ClearBuffer();
    }

    void StopCapture()
    {
        bIsCapturing.store(false);
    }

    void ClearBuffer()
    {
        FScopeLock Lock(&BufferCriticalSection);
        AudioBuffer.Reset();
    }

    void GetCapturedAudio(TArray<float>& OutAudio)
    {
        FScopeLock Lock(&BufferCriticalSection);
        OutAudio = AudioBuffer;
    }

    TArray<float> FlushCapturedAudio()
    {
        FScopeLock Lock(&BufferCriticalSection);
        TArray<float> Result = MoveTemp(AudioBuffer);
        AudioBuffer.Empty();
        return Result;
    }

    // ISubmixBufferListener interface
    virtual void OnNewSubmixBuffer(const USoundSubmix* OwningSubmix, float* AudioData, int32 InNumSamples,
        int32 InNumChannels, int32 InSampleRate, double InAudioClock) override
    {
        if (!bIsCapturing.load() || !AudioData || InNumSamples <= 0)
        {
            return;
        }

        // First time received - log for debugging
        static int32 CallbackCount = 0;
        CallbackCount++;
        if (CallbackCount <= 3)
        {
            UE_LOG(LogAmbientAudioSensor, Log, TEXT("FSubmixAudioListener::OnNewSubmixBuffer called! Samples=%d, Channels=%d, SampleRate=%d (Callback #%d)"),
                InNumSamples, InNumChannels, InSampleRate, CallbackCount);
        }

        // Update format info
        SampleRate = InSampleRate;
        NumChannels = InNumChannels;

        // Copy audio data
        {
            FScopeLock Lock(&BufferCriticalSection);
            int32 StartIndex = AudioBuffer.Num();
            AudioBuffer.AddUninitialized(InNumSamples);
            FMemory::Memcpy(&AudioBuffer[StartIndex], AudioData, InNumSamples * sizeof(float));
        }

        // Notify owner
        if (UAmbientAudioSensor* Sensor = Owner.Get())
        {
            // Copy data for callback
            TArray<float> CallbackData;
            CallbackData.AddUninitialized(InNumSamples);
            FMemory::Memcpy(CallbackData.GetData(), AudioData, InNumSamples * sizeof(float));

            AsyncTask(ENamedThreads::GameThread, [Sensor, CallbackData, InNumChannels, InSampleRate, InAudioClock]()
            {
                if (IsValid(Sensor))
                {
                    Sensor->OnSubmixAudioReceived(CallbackData, InNumChannels, InSampleRate, InAudioClock);
                }
            });
        }
    }

    int32 GetSampleRate() const { return SampleRate; }
    int32 GetNumChannels() const { return NumChannels; }
    bool IsCapturing() const { return bIsCapturing.load(); }

private:
    TWeakObjectPtr<UAmbientAudioSensor> Owner;
    TArray<float> AudioBuffer;
    mutable FCriticalSection BufferCriticalSection;
    int32 SampleRate;
    int32 NumChannels;
    std::atomic<bool> bIsCapturing;
};

// UAmbientAudioSensor Implementation

UAmbientAudioSensor::UAmbientAudioSensor(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
    , bUseMasterSubmix(true)
    , TargetSubmix(nullptr)
    , bCaptureAllSubmixes(false)
{
    // Default to ambient capture mode
    CaptureMode = EAudioCaptureMode::Ambient;
}

void UAmbientAudioSensor::BeginPlay()
{
    Super::BeginPlay();

    // Create submix listener
    SubmixListener = MakeShared<FSubmixAudioListener>(this);
}

void UAmbientAudioSensor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopCapture();
    Super::EndPlay(EndPlayReason);
}

void UAmbientAudioSensor::StartCapture()
{
    if (bIsCapturing)
    {
        UE_LOG(LogAmbientAudioSensor, Warning, TEXT("AmbientAudioSensor: Already capturing"));
        return;
    }

    // Initialize the submix listener
    InitializeSubmixListener();

    // Start the base capture
    Super::StartCapture();

    // Start the submix listener
    if (SubmixListener.IsValid())
    {
        SubmixListener->StartCapture();
    }

    // Apply volume exclusion (mute excluded audio components)
    ApplyExclusionVolumes();
}

void UAmbientAudioSensor::StopCapture()
{
    if (!bIsCapturing)
    {
        return;
    }

    // Stop the submix listener
    if (SubmixListener.IsValid())
    {
        SubmixListener->StopCapture();
    }

    // Shutdown submix listener
    ShutdownSubmixListener();

    // Stop base capture
    Super::StopCapture();

    // Restore original volumes
    RestoreExclusionVolumes();
}

FAudioCaptureData UAmbientAudioSensor::GetCapturedAudio() const
{
    FAudioCaptureData Data;

    if (SubmixListener.IsValid())
    {
        TArray<float> AudioData;
        SubmixListener->GetCapturedAudio(AudioData);

        FScopeLock Lock(&AudioBufferCriticalSection);
        Data.Samples = AudioData;
        Data.NumChannels = SubmixListener->GetNumChannels();
        Data.SampleRate = SubmixListener->GetSampleRate();
        Data.Timestamp = StartTimestamp;
        Data.Duration = CurrentCaptureTime;
    }
    else
    {
        Data = Super::GetCapturedAudio();
    }

    return Data;
}

FAudioCaptureData UAmbientAudioSensor::FlushCapturedAudio()
{
    FAudioCaptureData Data;

    if (SubmixListener.IsValid())
    {
        TArray<float> AudioData = SubmixListener->FlushCapturedAudio();

        FScopeLock Lock(&AudioBufferCriticalSection);
        Data.Samples = AudioData;
        Data.NumChannels = SubmixListener->GetNumChannels();
        Data.SampleRate = SubmixListener->GetSampleRate();
        Data.Timestamp = StartTimestamp;
        Data.Duration = CurrentCaptureTime;
    }
    else
    {
        Data = Super::FlushCapturedAudio();
    }

    return Data;
}

void UAmbientAudioSensor::InitializeSubmixListener()
{
    if (!SubmixListener.IsValid())
    {
        SubmixListener = MakeShared<FSubmixAudioListener>(this);
    }

    // Get audio device
    FAudioDevice* AudioDevicePtr = GEngine ? GEngine->GetMainAudioDeviceRaw() : nullptr;
    if (!AudioDevicePtr)
    {
        UE_LOG(LogAmbientAudioSensor, Error, TEXT("AmbientAudioSensor: No audio device available - audio capture will not work"));
        UE_LOG(LogAmbientAudioSensor, Error, TEXT("Please ensure: 1) Audio is enabled in project settings, 2) Audio Mixer plugin is enabled"));
        return;
    }

    // Register with submix
    USoundSubmix* SubmixToUse = nullptr;
    if (bUseMasterSubmix || !TargetSubmix)
    {
        // Use the master submix
        SubmixToUse = &AudioDevicePtr->GetMainSubmixObject();
        UE_LOG(LogAmbientAudioSensor, Log, TEXT("AmbientAudioSensor: Using Master Submix (via GetMainSubmixObject)"));
    }
    else
    {
        SubmixToUse = TargetSubmix;
        UE_LOG(LogAmbientAudioSensor, Log, TEXT("AmbientAudioSensor: Using custom submix %s"), *TargetSubmix->GetName());
    }

    if (SubmixToUse && SubmixListener.IsValid())
    {
        // Check if submix is valid (not CDO)
        const bool bIsClassDefaultObject = SubmixToUse->IsA(USoundSubmix::StaticClass()) && SubmixToUse->HasAnyFlags(RF_ClassDefaultObject);
        if (bIsClassDefaultObject)
        {
            UE_LOG(LogAmbientAudioSensor, Error, TEXT("AmbientAudioSensor: GetMainSubmixObject() returned Class Default Object - Audio Mixer may not be enabled!"));
            UE_LOG(LogAmbientAudioSensor, Error, TEXT("Please enable Audio Mixer: Project Settings -> Engine -> Audio -> Audio Device Module Name = AudioMixer"));
            UE_LOG(LogAmbientAudioSensor, Error, TEXT("Or add: +AudioDeviceModuleName=AudioMixer in DefaultEngine.ini [/Script/WindowsTargetPlatform.WindowsTargetSettings]"));
            return;
        }

        // Register this listener with the submix using TSharedRef
        TSharedRef<ISubmixBufferListener, ESPMode::ThreadSafe> ListenerRef = SubmixListener.ToSharedRef();
        AudioDevicePtr->RegisterSubmixBufferListener(ListenerRef, *SubmixToUse);

        UE_LOG(LogAmbientAudioSensor, Log, TEXT("AmbientAudioSensor: Registered with submix %s"),
            *SubmixToUse->GetName());

        // Log warning if no audio data is received after some time
        UE_LOG(LogAmbientAudioSensor, Log, TEXT("AmbientAudioSensor: StartCapture called - if no audio data is received, check that AudioComponent is playing and Audio Mixer is enabled"));
    }
}

void UAmbientAudioSensor::ShutdownSubmixListener()
{
    // Unregister from submix
    if (SubmixListener.IsValid())
    {
        FAudioDevice* AudioDevicePtr = GEngine ? GEngine->GetMainAudioDeviceRaw() : nullptr;
        if (AudioDevicePtr)
        {
            USoundSubmix* SubmixToUse = nullptr;
            if (bUseMasterSubmix || !TargetSubmix)
            {
                SubmixToUse = &AudioDevicePtr->GetMainSubmixObject();
            }
            else
            {
                SubmixToUse = TargetSubmix;
            }

            if (SubmixToUse)
            {
                TSharedRef<ISubmixBufferListener, ESPMode::ThreadSafe> ListenerRef = SubmixListener.ToSharedRef();
                AudioDevicePtr->UnregisterSubmixBufferListener(ListenerRef, *SubmixToUse);
            }
        }
    }
}

void UAmbientAudioSensor::OnSubmixAudioReceived(const TArray<float>& AudioData, int32 InNumChannels, int32 InSampleRate, double InTimestamp)
{
    // Process received audio data
    if (AudioData.Num() > 0)
    {
        // Store in main buffer
        ProcessAudioData(AudioData.GetData(), AudioData.Num(), InNumChannels);

        // Update format
        NumChannels = InNumChannels;
        SampleRate = InSampleRate;

        // Notify
        OnAudioDataReceived(static_cast<float>(InTimestamp), AudioData.Num() / InNumChannels);
    }
}

void UAmbientAudioSensor::SetExcludedAudioComponent(UAudioComponent* AudioComp)
{
    if (AudioComp && !ExcludedAudioComponents.Contains(AudioComp))
    {
        ExcludedAudioComponents.Add(AudioComp);
        UE_LOG(LogAmbientAudioSensor, Log, TEXT("AmbientAudioSensor: Added excluded audio component %s"), *AudioComp->GetName());
    }
}

void UAmbientAudioSensor::ClearExcludedAudioComponents()
{
    ExcludedAudioComponents.Empty();
    UE_LOG(LogAmbientAudioSensor, Log, TEXT("AmbientAudioSensor: Cleared excluded audio components"));
}

void UAmbientAudioSensor::ApplyExclusionVolumes()
{
    for (UAudioComponent* AudioComp : ExcludedAudioComponents)
    {
        if (IsValid(AudioComp))
        {
            // Store original volume (VolumeMultiplier is a UPROPERTY)
            float OriginalVolume = AudioComp->VolumeMultiplier;
            OriginalVolumes.Add(AudioComp, OriginalVolume);

            // Mute the audio component
            AudioComp->SetVolumeMultiplier(0.0f);
            UE_LOG(LogAmbientAudioSensor, Log, TEXT("AmbientAudioSensor: Muted audio component %s (original volume: %.2f)"),
                *AudioComp->GetName(), OriginalVolume);
        }
    }
}

void UAmbientAudioSensor::RestoreExclusionVolumes()
{
    for (auto& [AudioComp, OriginalVolume] : OriginalVolumes)
    {
        if (IsValid(AudioComp))
        {
            AudioComp->SetVolumeMultiplier(OriginalVolume);
            UE_LOG(LogAmbientAudioSensor, Log, TEXT("AmbientAudioSensor: Restored volume for %s to %.2f"),
                *AudioComp->GetName(), OriginalVolume);
        }
    }
    OriginalVolumes.Empty();
}
