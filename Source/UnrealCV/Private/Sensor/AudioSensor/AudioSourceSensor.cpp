// Copyright (c) 2025 UnrealCV Team

#include "Sensor/AudioSensor/AudioSourceSensor.h"
#include "UnrealcvLog.h"
#include "Audio.h"
#include "AudioMixerDevice.h"
#include "AudioThread.h"
#include "Sound/SoundSubmix.h"
#include "Engine/Engine.h"

// Debug category
DEFINE_LOG_CATEGORY(LogAudioSensor);

// FSourceAudioListener Implementation

FSourceAudioListener::FSourceAudioListener(UBaseAudioSensor* InOwnerSensor)
    : OwnerSensor(InOwnerSensor)
    , CurrentSourceId(INDEX_NONE)
    , bIsCapturing(false)
    , bShouldZeroBuffer(false)
    , LastCaptureTime(0.0)
{
}

FSourceAudioListener::~FSourceAudioListener()
{
    StopCapture();
}

void FSourceAudioListener::OnNewBuffer(const FOnNewBufferParams& InParams)
{
    if (!bIsCapturing.load())
    {
        return;
    }

    // Validate parameters
    if (!InParams.AudioData || InParams.NumSamples <= 0 || InParams.NumChannels <= 0)
    {
        return;
    }

    // Check if this is a new source
    int32 ExpectedSourceId = INDEX_NONE;
    if (CurrentSourceId.compare_exchange_strong(ExpectedSourceId, InParams.SourceId))
    {
        UE_LOG(LogAudioSensor, Log, TEXT("SourceAudioListener: Started capturing from source %d (%d channels, %d Hz)"),
            InParams.SourceId, InParams.NumChannels, InParams.SampleRate);
    }

    // Only capture from the source we're tracking
    if (CurrentSourceId.load() != InParams.SourceId)
    {
        return;
    }

    // Store the audio data
    {
        FScopeLock Lock(&AudioDataCriticalSection);

        int32 StartIndex = CapturedAudio.Num();
        CapturedAudio.AddUninitialized(InParams.NumSamples);
        FMemory::Memcpy(&CapturedAudio[StartIndex], InParams.AudioData, InParams.NumSamples * sizeof(float));
    }

    // Update timestamp
    LastCaptureTime.store(FPlatformTime::Seconds(), std::memory_order_relaxed);

    // Trigger callback on owner sensor
    if (UBaseAudioSensor* Sensor = OwnerSensor.Get())
    {
        // Queue delegate call on game thread
        FAudioCaptureData CaptureData;
        CaptureData.NumChannels = InParams.NumChannels;
        CaptureData.SampleRate = InParams.SampleRate;
        CaptureData.Timestamp = LastCaptureTime.load();

        AsyncTask(ENamedThreads::GameThread, [Sensor, CaptureData]()
        {
            if (IsValid(Sensor))
            {
                Sensor->OnAudioDataReceived(CaptureData.Timestamp, CaptureData.GetNumFrames());
            }
        });
    }
}

void FSourceAudioListener::OnSourceReleased(const int32 InSourceId)
{
    if (InSourceId == CurrentSourceId.load())
    {
        UE_LOG(LogAudioSensor, Log, TEXT("SourceAudioListener: Source %d released"), InSourceId);

        CurrentSourceId.store(INDEX_NONE);

        if (UBaseAudioSensor* Sensor = OwnerSensor.Get())
        {
            AsyncTask(ENamedThreads::GameThread, [Sensor]()
            {
                if (IsValid(Sensor))
                {
                    Sensor->OnCaptureFinished();
                }
            });
        }
    }
}

void FSourceAudioListener::GetCapturedAudio(TArray<float>& OutAudio)
{
    FScopeLock Lock(&AudioDataCriticalSection);
    OutAudio = CapturedAudio;
}

void FSourceAudioListener::ClearCapturedAudio()
{
    FScopeLock Lock(&AudioDataCriticalSection);
    CapturedAudio.Reset();
}

void FSourceAudioListener::StartCapture()
{
    bIsCapturing.store(true);
    ClearCapturedAudio();
    CurrentSourceId.store(INDEX_NONE);
    UE_LOG(LogAudioSensor, Log, TEXT("SourceAudioListener: Capture started"));
}

void FSourceAudioListener::StopCapture()
{
    bIsCapturing.store(false);
    CurrentSourceId.store(INDEX_NONE);
    UE_LOG(LogAudioSensor, Log, TEXT("SourceAudioListener: Capture stopped"));
}

// UAudioSourceSensor Implementation

UAudioSourceSensor::UAudioSourceSensor(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
    , TargetAudioComp(nullptr)
    , bZeroBufferAfterCapture(false)
    , bCapturePreAttenuation(true)
{
    // Default to attached source mode
    CaptureMode = EAudioCaptureMode::AttachedSource;

    // Create the source listener
    SourceListener = MakeShared<FSourceAudioListener>(this);
}

void UAudioSourceSensor::BeginPlay()
{
    Super::BeginPlay();

    // Auto-attach to parent actor's audio component if available
    if (!TargetAudioComp)
    {
        if (AActor* OwnerActor = GetOwner())
        {
            TargetAudioComp = OwnerActor->FindComponentByClass<UAudioComponent>();
        }
    }

    // Auto start capture if requested
    if (bAutoStartCapture && TargetAudioComp)
    {
        StartCaptureFromComponent(TargetAudioComp);
    }
}

void UAudioSourceSensor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopCapture();
    Super::EndPlay(EndPlayReason);
}

void UAudioSourceSensor::StartCaptureFromComponent(UAudioComponent* TargetAudioComponent)
{
    if (!TargetAudioComponent)
    {
        UE_LOG(LogAudioSensor, Warning, TEXT("AudioSourceSensor: Cannot start capture - no target audio component"));
        return;
    }

    // Stop any existing capture
    StopCapture();

    // Set new target
    TargetAudioComp = TargetAudioComponent;

    // Attach to the audio component
    AttachToAudioComponent();

    // Start the listener
    if (SourceListener.IsValid())
    {
        SourceListener->StartCapture();
    }

    bIsCapturing = true;
    StartTimestamp = FPlatformTime::Seconds();

    UE_LOG(LogAudioSensor, Log, TEXT("AudioSourceSensor: Started capture from component %s"),
        *TargetAudioComponent->GetName());
}

void UAudioSourceSensor::StartCaptureFromActor(AActor* TargetActor)
{
    if (!TargetActor)
    {
        UE_LOG(LogAudioSensor, Warning, TEXT("AudioSourceSensor: Cannot start capture - no target actor"));
        return;
    }

    // Find first audio component
    UAudioComponent* AudioComp = TargetActor->FindComponentByClass<UAudioComponent>();
    if (!AudioComp)
    {
        UE_LOG(LogAudioSensor, Warning, TEXT("AudioSourceSensor: Target actor %s has no AudioComponent"),
            *TargetActor->GetName());
        return;
    }

    StartCaptureFromComponent(AudioComp);
}

void UAudioSourceSensor::StopCapture()
{
    if (!bIsCapturing)
    {
        return;
    }

    bIsCapturing = false;

    // Stop the listener
    if (SourceListener.IsValid())
    {
        SourceListener->StopCapture();
    }

    // Detach from audio component
    DetachFromAudioComponent();

    UE_LOG(LogAudioSensor, Log, TEXT("AudioSourceSensor: Capture stopped"));
}

void UAudioSourceSensor::SetTargetAudioComponent(UAudioComponent* InAudioComponent)
{
    if (IsCapturing())
    {
        UE_LOG(LogAudioSensor, Warning, TEXT("AudioSourceSensor: Cannot change target while capturing. Stop capture first."));
        return;
    }

    TargetAudioComp = InAudioComponent;
}

FAudioCaptureData UAudioSourceSensor::GetCapturedAudio() const
{
    FAudioCaptureData Data;

    if (SourceListener.IsValid())
    {
        TArray<float> AudioData;
        SourceListener->GetCapturedAudio(AudioData);

        FScopeLock Lock(&AudioBufferCriticalSection);
        Data.Samples = AudioData;
        Data.NumChannels = NumChannels;
        Data.SampleRate = SampleRate;
        Data.Timestamp = StartTimestamp;
        Data.Duration = CurrentCaptureTime;
    }

    return Data;
}

int32 UAudioSourceSensor::GetCurrentSourceId() const
{
    if (SourceListener.IsValid())
    {
        return SourceListener->GetCurrentSourceId();
    }
    return INDEX_NONE;
}

bool UAudioSourceSensor::IsAttachedToSource() const
{
    return SourceListener.IsValid() && SourceListener->IsCapturing() && GetCurrentSourceId() != INDEX_NONE;
}

void UAudioSourceSensor::AttachToAudioComponent()
{
    if (!IsValid(TargetAudioComp))
    {
        return;
    }

    // Create shared pointer for the AudioComponent
    SharedListenerPtr = StaticCastSharedPtr<ISourceBufferListener>(SourceListener);

    // Set the listener on the audio component
    TargetAudioComp->SetSourceBufferListener(SharedListenerPtr, bZeroBufferAfterCapture);

    UE_LOG(LogAudioSensor, Verbose, TEXT("AudioSourceSensor: Attached listener to %s (ZeroBuffer: %s)"),
        *TargetAudioComp->GetName(), bZeroBufferAfterCapture ? TEXT("true") : TEXT("false"));
}

void UAudioSourceSensor::DetachFromAudioComponent()
{
    if (IsValid(TargetAudioComp))
    {
        // Clear the listener
        TargetAudioComp->SetSourceBufferListener(nullptr, false);
        UE_LOG(LogAudioSensor, Verbose, TEXT("AudioSourceSensor: Detached from %s"), *TargetAudioComp->GetName());
    }

    SharedListenerPtr.Reset();
}

void UAudioSourceSensor::HandleSourceReleased()
{
    UE_LOG(LogAudioSensor, Log, TEXT("AudioSourceSensor: Source released"));

    // Notify via delegate
    OnSourceAudioCaptured.Broadcast(GetCurrentSourceId(), TArray<float>(), FPlatformTime::Seconds());
}
