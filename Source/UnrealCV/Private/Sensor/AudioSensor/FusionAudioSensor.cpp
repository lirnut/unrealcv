// Copyright (c) 2025 UnrealCV Team

#include "FusionAudioSensor.h"
#include "UnrealcvLog.h"
#include "Sound/SoundWave.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY(LogFusionAudioSensor);

UFusionAudioSensor::UFusionAudioSensor(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
    , CapturePreset(EAudioCapturePreset::Ambient)
    , bAutoStartCapture(false)
    , TargetAudioComponent(nullptr)
    , bAutoSaveToFile(false)
{
    PrimaryComponentTick.bCanEverTick = false;

    // Create ambient audio sensor
    FString ComponentName = FString::Printf(TEXT("%s_%s"), *GetName(), TEXT("AmbientSensor"));
    AmbientSensor = CreateDefaultSubobject<UAmbientAudioSensor>(*ComponentName);
    AmbientSensor->SetupAttachment(this);

    // Create source audio sensor
    ComponentName = FString::Printf(TEXT("%s_%s"), *GetName(), TEXT("SourceSensor"));
    SourceSensor = CreateDefaultSubobject<UAudioSourceSensor>(*ComponentName);
    SourceSensor->SetupAttachment(this);
}

void UFusionAudioSensor::BeginPlay()
{
    Super::BeginPlay();

    // Initialize based on preset
    Initialize(CapturePreset);

    // Auto-start if requested
    if (bAutoStartCapture)
    {
        StartCapture();
    }
}

void UFusionAudioSensor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopCapture();
    Super::EndPlay(EndPlayReason);
}

void UFusionAudioSensor::Initialize(EAudioCapturePreset Preset)
{
    CapturePreset = Preset;

    switch (CapturePreset)
    {
    case EAudioCapturePreset::Ambient:
        // Only ambient sensor active
        break;

    case EAudioCapturePreset::SingleSource:
        // Only source sensor active
        break;

    case EAudioCapturePreset::SplitCapture:
        // Both sensors active
        break;

    case EAudioCapturePreset::MultiSource:
        // Source sensor for primary, ambient as backup
        break;
    }

    UE_LOG(LogFusionAudioSensor, Log, TEXT("FusionAudioSensor initialized with preset %d"), static_cast<int32>(CapturePreset));
}

void UFusionAudioSensor::StartCapture()
{
    UE_LOG(LogFusionAudioSensor, Log, TEXT("Starting capture with preset %d"), static_cast<int32>(CapturePreset));

    switch (CapturePreset)
    {
    case EAudioCapturePreset::Ambient:
        StartAmbientCapture();
        break;

    case EAudioCapturePreset::SingleSource:
        StartSourceCapture();
        break;

    case EAudioCapturePreset::SplitCapture:
    case EAudioCapturePreset::MultiSource:
        StartAmbientCapture();
        StartSourceCapture();
        break;
    }
}

void UFusionAudioSensor::StopCapture()
{
    UE_LOG(LogFusionAudioSensor, Log, TEXT("Stopping all capture"));

    StopAmbientCapture();
    StopSourceCapture();

    // Auto-save if enabled
    if (bAutoSaveToFile && !AutoSaveBaseFilename.IsEmpty())
    {
        SaveAllAudioToFiles(
            AutoSaveBaseFilename + TEXT("_ambient.wav"),
            AutoSaveBaseFilename + TEXT("_source.wav")
        );
    }
}

bool UFusionAudioSensor::IsCapturing() const
{
    bool bAmbientCapturing = IsValid(AmbientSensor) && AmbientSensor->IsCapturing();
    bool bSourceCapturing = IsValid(SourceSensor) && SourceSensor->IsCapturing();

    switch (CapturePreset)
    {
    case EAudioCapturePreset::Ambient:
        return bAmbientCapturing;
    case EAudioCapturePreset::SingleSource:
        return bSourceCapturing;
    case EAudioCapturePreset::SplitCapture:
    case EAudioCapturePreset::MultiSource:
        return bAmbientCapturing || bSourceCapturing;
    default:
        return false;
    }
}

FAudioCaptureData UFusionAudioSensor::GetAmbientAudioData() const
{
    if (IsValid(AmbientSensor))
    {
        return AmbientSensor->GetCapturedAudio();
    }
    return FAudioCaptureData();
}

FAudioCaptureData UFusionAudioSensor::GetSourceAudioData() const
{
    if (IsValid(SourceSensor))
    {
        return SourceSensor->GetCapturedAudio();
    }
    return FAudioCaptureData();
}

void UFusionAudioSensor::FlushAllAudioData(FAudioCaptureData& OutAmbient, FAudioCaptureData& OutSource)
{
    if (IsValid(AmbientSensor))
    {
        OutAmbient = AmbientSensor->FlushCapturedAudio();
    }

    if (IsValid(SourceSensor))
    {
        OutSource = SourceSensor->FlushCapturedAudio();
    }
}

void UFusionAudioSensor::SetTargetAudioComponent(UAudioComponent* InTargetAudioComponent)
{
    this->TargetAudioComponent = InTargetAudioComponent;

    if (IsValid(SourceSensor))
    {
        SourceSensor->SetTargetAudioComponent(InTargetAudioComponent);
    }

    UE_LOG(LogFusionAudioSensor, Log, TEXT("Target audio component set to %s"),
        InTargetAudioComponent ? *InTargetAudioComponent->GetName() : TEXT("None"));
}

UAudioComponent* UFusionAudioSensor::GetTargetAudioComponent() const
{
    return TargetAudioComponent;
}

void UFusionAudioSensor::StartAmbientCapture()
{
    if (IsValid(AmbientSensor))
    {
        AmbientSensor->StartCapture();
        UE_LOG(LogFusionAudioSensor, Log, TEXT("Ambient capture started"));
    }
}

void UFusionAudioSensor::StopAmbientCapture()
{
    if (IsValid(AmbientSensor))
    {
        AmbientSensor->StopCapture();
        UE_LOG(LogFusionAudioSensor, Log, TEXT("Ambient capture stopped"));
    }
}

void UFusionAudioSensor::StartSourceCapture()
{
    if (!IsValid(SourceSensor))
    {
        return;
    }

    if (IsValid(TargetAudioComponent))
    {
        SourceSensor->StartCaptureFromComponent(TargetAudioComponent);
        UE_LOG(LogFusionAudioSensor, Log, TEXT("Source capture started from %s"), *TargetAudioComponent->GetName());
    }
    else
    {
        // Try to find audio component from owner
        if (AActor* Owner = GetOwner())
        {
            UAudioComponent* AudioComp = Owner->FindComponentByClass<UAudioComponent>();
            if (AudioComp)
            {
                SetTargetAudioComponent(AudioComp);
                SourceSensor->StartCaptureFromComponent(AudioComp);
                UE_LOG(LogFusionAudioSensor, Log, TEXT("Source capture started from owner audio component"));
            }
            else
            {
                UE_LOG(LogFusionAudioSensor, Warning, TEXT("No audio component found for source capture"));
            }
        }
    }
}

void UFusionAudioSensor::StopSourceCapture()
{
    if (IsValid(SourceSensor))
    {
        SourceSensor->StopCapture();
        UE_LOG(LogFusionAudioSensor, Log, TEXT("Source capture stopped"));
    }
}

void UFusionAudioSensor::SaveAmbientAudioToFile(const FString& Filename)
{
    FAudioCaptureData AudioData = GetAmbientAudioData();
    if (AudioData.Samples.Num() == 0)
    {
        UE_LOG(LogFusionAudioSensor, Warning, TEXT("No ambient audio data to save"));
        return;
    }

    SaveAudioToWavFile(AudioData, Filename);
    UE_LOG(LogFusionAudioSensor, Log, TEXT("Ambient audio saved to %s"), *Filename);
}

void UFusionAudioSensor::SaveSourceAudioToFile(const FString& Filename)
{
    FAudioCaptureData AudioData = GetSourceAudioData();
    if (AudioData.Samples.Num() == 0)
    {
        UE_LOG(LogFusionAudioSensor, Warning, TEXT("No source audio data to save"));
        return;
    }

    SaveAudioToWavFile(AudioData, Filename);
    UE_LOG(LogFusionAudioSensor, Log, TEXT("Source audio saved to %s"), *Filename);
}

void UFusionAudioSensor::SaveAllAudioToFiles(const FString& AmbientFilename, const FString& SourceFilename)
{
    SaveAmbientAudioToFile(AmbientFilename);
    SaveSourceAudioToFile(SourceFilename);
}

void UFusionAudioSensor::SetSampleRate(int32 InSampleRate)
{
    // Note: Actual sample rate is determined by the audio device
    // This sets the desired sample rate for internal processing
    if (IsValid(AmbientSensor))
    {
        // AmbientSensor->SampleRate = InSampleRate;
    }
    if (IsValid(SourceSensor))
    {
        // SourceSensor->SampleRate = InSampleRate;
    }
}

void UFusionAudioSensor::SetNumChannels(int32 InNumChannels)
{
    if (IsValid(AmbientSensor))
    {
        // AmbientSensor->NumChannels = InNumChannels;
    }
    if (IsValid(SourceSensor))
    {
        // SourceSensor->NumChannels = InNumChannels;
    }
}

void UFusionAudioSensor::SetMaxCaptureDuration(float InMaxDuration)
{
    if (IsValid(AmbientSensor))
    {
        AmbientSensor->SetMaxCaptureDuration(InMaxDuration);
    }
    if (IsValid(SourceSensor))
    {
        SourceSensor->SetMaxCaptureDuration(InMaxDuration);
    }
}

void UFusionAudioSensor::HandleAmbientCaptureFinished()
{
    OnAmbientCaptureFinished.Broadcast();
}

void UFusionAudioSensor::HandleSourceCaptureFinished()
{
    OnSourceCaptureFinished.Broadcast();
}

void UFusionAudioSensor::HandleAmbientDataReceived(float Timestamp, int32 NumFrames)
{
    OnAmbientDataReceived.Broadcast(Timestamp, NumFrames);
}

void UFusionAudioSensor::HandleSourceDataReceived(float Timestamp, int32 NumFrames)
{
    OnSourceDataReceived.Broadcast(Timestamp, NumFrames);
}

// Helper function to save audio data to WAV file
void UFusionAudioSensor::SaveAudioToWavFile(const FAudioCaptureData& AudioData, const FString& Filename)
{
    // Build WAV header
    struct FWAVHeader
    {
        char RIFF[4] = { 'R', 'I', 'F', 'F' };
        uint32 FileSize = 0;
        char WAVE[4] = { 'W', 'A', 'V', 'E' };
        char fmt[4] = { 'f', 'm', 't', ' ' };
        uint32 Subchunk1Size = 16;
        uint16 AudioFormat = 1; // PCM
        uint16 NumChannels = 2;
        uint32 SampleRate = 48000;
        uint32 ByteRate = 0;
        uint16 BlockAlign = 0;
        uint16 BitsPerSample = 16;
        char data[4] = { 'd', 'a', 't', 'a' };
        uint32 Subchunk2Size = 0;
    };

    // Convert float to 16-bit PCM
    TArray<int16> PCMData;
    PCMData.Reserve(AudioData.Samples.Num());
    for (float Sample : AudioData.Samples)
    {
        PCMData.Add(static_cast<int16>(FMath::Clamp(Sample, -1.0f, 1.0f) * 32767.0f));
    }

    // Build header
    FWAVHeader Header;
    Header.NumChannels = AudioData.NumChannels;
    Header.SampleRate = AudioData.SampleRate;
    Header.BitsPerSample = 16;
    Header.ByteRate = Header.SampleRate * Header.NumChannels * (Header.BitsPerSample / 8);
    Header.BlockAlign = Header.NumChannels * (Header.BitsPerSample / 8);
    Header.Subchunk2Size = PCMData.Num() * sizeof(int16);
    Header.FileSize = 36 + Header.Subchunk2Size;

    // Write to file
    TArray<uint8> FileData;
    FileData.Append((uint8*)&Header, sizeof(Header));
    FileData.Append((uint8*)PCMData.GetData(), PCMData.Num() * sizeof(int16));

    FString FullPath = FPaths::ProjectSavedDir() / Filename;
    if (!FFileHelper::SaveArrayToFile(FileData, *FullPath))
    {
        UE_LOG(LogFusionAudioSensor, Error, TEXT("Failed to save audio to %s"), *FullPath);
    }
}
