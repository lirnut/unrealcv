// Copyright (c) 2025 UnrealCV Team

#include "AudioSensorExample.h"
#include "Audio.h"
#include "Sound/SoundWaveProcedural.h"
#include "Kismet/GameplayStatics.h"

AAudioSensorExample::AAudioSensorExample(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Create test audio component
    TestAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("TestAudioComponent"));
    TestAudioComponent->SetupAttachment(RootComponent);
    TestAudioComponent->bAutoActivate = false;

    // Create ambient audio sensor
    AmbientSensor = CreateDefaultSubobject<UAmbientAudioSensor>(TEXT("AmbientSensor"));
    AmbientSensor->SetupAttachment(RootComponent);

    // Create source audio sensor
    SourceSensor = CreateDefaultSubobject<UAudioSourceSensor>(TEXT("SourceSensor"));
    SourceSensor->SetupAttachment(RootComponent);
}

void AAudioSensorExample::BeginPlay()
{
    Super::BeginPlay();

    // Set up the source sensor to capture from our test audio component
    if (IsValid(SourceSensor))
    {
        SourceSensor->SetTargetAudioComponent(TestAudioComponent);
    }

    // Optional: Auto-play test sound
    if (IsValid(TestAudioComponent) && IsValid(TestSound))
    {
        TestAudioComponent->SetSound(TestSound);
    }
}

void AAudioSensorExample::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Stop all capture
    StopAmbientCapture();
    StopSourceCapture();

    Super::EndPlay(EndPlayReason);
}

void AAudioSensorExample::StartAmbientCapture()
{
    if (IsValid(AmbientSensor))
    {
        UE_LOG(LogTemp, Log, TEXT("Starting ambient audio capture"));
        AmbientSensor->StartCapture();
    }
}

void AAudioSensorExample::StopAmbientCapture()
{
    if (IsValid(AmbientSensor))
    {
        UE_LOG(LogTemp, Log, TEXT("Stopping ambient audio capture"));
        AmbientSensor->StopCapture();
    }
}

void AAudioSensorExample::StartSourceCapture()
{
    if (IsValid(SourceSensor) && IsValid(TestAudioComponent))
    {
        UE_LOG(LogTemp, Log, TEXT("Starting source audio capture from %s"), *TestAudioComponent->GetName());

        // Start playing the sound if not already playing
        if (!TestAudioComponent->IsPlaying())
        {
            TestAudioComponent->Play();
        }

        SourceSensor->StartCaptureFromComponent(TestAudioComponent);
    }
}

void AAudioSensorExample::StopSourceCapture()
{
    if (IsValid(SourceSensor))
    {
        UE_LOG(LogTemp, Log, TEXT("Stopping source audio capture"));
        SourceSensor->StopCapture();
    }
}

FAudioCaptureData AAudioSensorExample::GetCapturedAudio()
{
    // Try source sensor first, then ambient
    if (IsValid(SourceSensor))
    {
        return SourceSensor->GetCapturedAudio();
    }
    else if (IsValid(AmbientSensor))
    {
        return AmbientSensor->GetCapturedAudio();
    }

    return FAudioCaptureData();
}

void AAudioSensorExample::PlayCapturedAudio()
{
    FAudioCaptureData AudioData = GetCapturedAudio();
    if (AudioData.Samples.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("No audio data to play"));
        return;
    }

    // Create a procedural sound wave
    USoundWaveProcedural* SoundWave = NewObject<USoundWaveProcedural>();
    SoundWave->SetSampleRate(AudioData.SampleRate);
    SoundWave->NumChannels = AudioData.NumChannels;
    SoundWave->Duration = AudioData.Duration;
    SoundWave->SoundGroup = SOUNDGROUP_Default;
    SoundWave->bLooping = false;

    // Convert float samples to 16-bit PCM
    TArray<uint8> PCMData;
    PCMData.Reserve(AudioData.Samples.Num() * sizeof(int16));

    for (float Sample : AudioData.Samples)
    {
        // Convert float (-1.0 to 1.0) to int16 (-32768 to 32767)
        int16 IntSample = static_cast<int16>(FMath::Clamp(Sample, -1.0f, 1.0f) * 32767.0f);
        PCMData.Add((uint8)(IntSample & 0xFF));
        PCMData.Add((uint8)((IntSample >> 8) & 0xFF));
    }

    // Queue the audio data
    SoundWave->QueueAudio(PCMData.GetData(), PCMData.Num());

    // Play the sound
    UGameplayStatics::PlaySound2D(this, SoundWave);

    UE_LOG(LogTemp, Log, TEXT("Playing captured audio: %d frames, %d channels, %d Hz, %.3f seconds"),
        AudioData.GetNumFrames(), AudioData.NumChannels, AudioData.SampleRate, AudioData.Duration);
}

void AAudioSensorExample::SaveAudioToFile(const FString& Filename)
{
    FAudioCaptureData AudioData = GetCapturedAudio();
    if (AudioData.Samples.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("No audio data to save"));
        return;
    }

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
    if (FFileHelper::SaveArrayToFile(FileData, *FullPath))
    {
        UE_LOG(LogTemp, Log, TEXT("Saved audio to %s (%d frames, %d channels, %d Hz, %.3f seconds)"),
            *FullPath, AudioData.GetNumFrames(), AudioData.NumChannels, AudioData.SampleRate, AudioData.Duration);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to save audio to %s"), *FullPath);
    }
}
