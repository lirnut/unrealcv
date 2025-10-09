// Weichao Qiu @ 2018
// Modified for FusionCamSensor-specific recording
#include "FusionCamCaptureActor.h"
#include "Runtime/Core/Public/Misc/Paths.h"
#include "Runtime/Engine/Classes/Engine/World.h"
#include "Runtime/CoreUObject/Public/UObject/ConstructorHelpers.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"
#include "Runtime/Engine/Classes/Components/MaterialBillboardComponent.h"
#include "Runtime/Engine/Public/TimerManager.h"
#include "Runtime/Engine/Classes/Engine/Engine.h"
#include "Materials/Material.h"

#include "FusionCamSensor.h"
#include "BPFunctionLib/VisionBPLib.h"
#include "BPFunctionLib/SerializeBPLib.h"
#include "Controller/ActorController.h"
#include "UnrealcvLog.h"
#include "UnrealcvServer.h"
#include "AudioMixerDevice.h"
#include "Utils/Serialization.h"
#include "Utils/ImageUtil.h"
#include "Misc/FileHelper.h"
#include "Serialization/BufferArchive.h"

AFusionCamCaptureActor::AFusionCamCaptureActor()
{
	PrimaryActorTick.bCanEverTick = true;

	// Initialize default values
	bIsRecording = false;
	bAddTimestamp = true;
	bRecordRGB = true;
	bRecordMask = true;
	bRecordDepth = false;
	bRecordNormal = false;
	bRecordFlow = false;
	bRecordMetadata = true;
	bRecordAudio = true;
	bRecordWithoutTarget = false;
	BulletTimeSpeedDeg = 2.0f;
	bUseBulletTime = false;
	BulletTimeState = EBulletTimeState::Waiting;
	ElapsedSteps = 0;
	ElapsedTime = 0.0f;
	TargetToHide = nullptr;

	// Create billboard for editor visibility
	Billboard = CreateDefaultSubobject<UMaterialBillboardComponent>(TEXT("BillboardComponent"));
	if (!IsRunningCommandlet() && (Billboard != nullptr))
	{
		static ConstructorHelpers::FObjectFinder<UMaterial> MaterialAsset(TEXT("/Engine/EditorMaterials/HelpActorMaterial"));
		Billboard->AddElement(MaterialAsset.Object, nullptr, false, 32.0f, 32.0f, nullptr);
		Billboard->bIsEditorOnly = true;
		Billboard->bHiddenInGame = true;
	}
	RootComponent = Billboard;
}

void AFusionCamCaptureActor::BeginPlay()
{
	Super::BeginPlay();

	if (bAddTimestamp)
	{
		FString TimestampStr = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M"));
		FinalDataFolder = FPaths::Combine(DataFolder.Path, TimestampStr);
	}
	else
	{
		FinalDataFolder = DataFolder.Path;
	}
}

void AFusionCamCaptureActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AFusionCamCaptureActor::StartRecord(const FString& FileName, float Duration, int32 FPS, AActor* Target)
{
	if (!IsValid(TargetSensor))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("FusionCamCaptureActor: TargetSensor is not set!"));
		return;
	}

	if (bIsRecording)
	{
		StopRecord();
	}

	AWorldSettings* WorldSettings = GetWorld()->GetWorldSettings();
	TimeDilation = WorldSettings->TimeDilation;

	RecordFileName = FileName;
	RecordDuration = Duration;
	RecordFPS = FPS;
	TimePerFrame = 1.0f / FPS;
	ElapsedTime = 0.0f;
	ElapsedSteps = 0;
	bIsRecording = true;
	TargetToHide = Target;
	BulletTimeState = EBulletTimeState::Waiting;

	UE_LOG(LogUnrealCV, Display, TEXT("FusionCamCaptureActor: Start recording to %s"), *FileName);

	// Start audio recording if enabled
	if (bRecordAudio)
	{
		StartAudioRecord();
	}

	// Record first frame immediately
	OnTimerRecord();

	// Set up timer for subsequent frames
	GetWorld()->GetTimerManager().SetTimer(
		TimerHandle_Record,
		this,
		&AFusionCamCaptureActor::OnTimerRecord,
		TimePerFrame,
		true
	);
}

void AFusionCamCaptureActor::StartBulletTimeRecord(const FString& FileName, float Duration, int32 FPS, AActor* Target)
{
	if (!IsValid(Target))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("FusionCamCaptureActor: Target is null in StartBulletTimeRecord"));
		return;
	}

	bUseBulletTime = true;
	StartRecord(FileName, Duration, FPS, Target);
}

void AFusionCamCaptureActor::StopRecord()
{
	if (bIsRecording)
	{
		UE_LOG(LogUnrealCV, Display, TEXT("FusionCamCaptureActor: Stop recording. %d frames recorded."), ElapsedSteps);

		if (bRecordAudio)
		{
			StopAudioRecord();
		}

		GetWorld()->GetTimerManager().ClearTimer(TimerHandle_Record);
		bIsRecording = false;
		TargetToHide = nullptr;
		bUseBulletTime = false;
		BulletTimeState = EBulletTimeState::Waiting;
	}
}

void AFusionCamCaptureActor::OnTimerRecord()
{
	if (!IsValid(TargetSensor))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("FusionCamCaptureActor: TargetSensor became invalid during recording!"));
		StopRecord();
		return;
	}

	if (ElapsedTime >= RecordDuration)
	{
		StopRecord();
		return;
	}

	// Smooth time dilation adjustment
	AWorldSettings* WorldSettings = GetWorld()->GetWorldSettings();
	if (FMath::Abs(TimeDilation - WorldSettings->TimeDilation) < 0.05f)
	{
		WorldSettings->SetTimeDilation(TimeDilation);
	}
	else if (TimeDilation != WorldSettings->TimeDilation)
	{
		WorldSettings->SetTimeDilation(0.2f * TimeDilation + 0.8f * WorldSettings->TimeDilation);
	}

	// Handle bullet time recording
	if (bUseBulletTime && (BulletTimeState == EBulletTimeState::Waiting) && (ElapsedTime > (RecordDuration / 2)))
	{
		RecordBulletTimeSequence();
		BulletTimeState = EBulletTimeState::Finished;
		return;
	}

	// Record normal frame
	RecordFrame();

	ElapsedTime += TimePerFrame;
	ElapsedSteps++;
}

void AFusionCamCaptureActor::RecordFrame()
{
	FScopeLock Lock(&RecordCriticalSection);

	int32 Width, Height;

	// Record RGB and Mask together for efficiency
	if (bRecordRGB && bRecordMask)
	{
		TArray<FColor> DataRGB, DataMask;
		TargetSensor->GetLitSeg(DataRGB, DataMask, Width, Height);

		FString FileNameRGB = MakeFilename("rgb", ".png");
		FString FileNameMask = MakeFilename("mask", ".png");
		SerializeData(DataRGB, Width, Height, FileNameRGB);
		SerializeData(DataMask, Width, Height, FileNameMask);

		// Record version without target if requested
		if (bRecordWithoutTarget && IsValid(TargetToHide))
		{
			TArray<FColor> DataRGBNoTarget;
			FActorController TargetController(TargetToHide);
			TargetController.Hide();
			TargetSensor->GetLit(DataRGBNoTarget, Width, Height);
			TargetController.Show();

			FString FileNameNoTarget = MakeFilename("rgb_no_target", ".png");
			SerializeData(DataRGBNoTarget, Width, Height, FileNameNoTarget);
		}
	}
	else if (bRecordRGB)
	{
		TArray<FColor> DataRGB;
		TargetSensor->GetLit(DataRGB, Width, Height);
		FString FileNameRGB = MakeFilename("rgb", ".png");
		SerializeData(DataRGB, Width, Height, FileNameRGB);
	}
	else if (bRecordMask)
	{
		TArray<FColor> DataMask;
		TargetSensor->GetSeg(DataMask, Width, Height);
		FString FileNameMask = MakeFilename("mask", ".png");
		SerializeData(DataMask, Width, Height, FileNameMask);
	}

	// Record depth
	if (bRecordDepth)
	{
		TArray<float> DepthData;
		TargetSensor->GetDepth(DepthData, Width, Height);
		FString DepthFilename = MakeFilename("depth", ".npy");
		UVisionBPLib::SaveNpy(DepthData, Width, Height, DepthFilename);
	}

	// Record normal
	if (bRecordNormal)
	{
		TArray<FColor> NormalData;
		TargetSensor->GetNormal(NormalData, Width, Height);
		FString NormalFilename = MakeFilename("normal", ".png");
		SerializeData(NormalData, Width, Height, NormalFilename);
	}

	// Record optical flow
	if (bRecordFlow)
	{
		TArray<FColor> FlowData;
		TargetSensor->GetFlow(FlowData, Width, Height);
		FString FlowFilename = MakeFilename("flow", ".png");
		SerializeData(FlowData, Width, Height, FlowFilename);
	}

	// Record camera metadata
	if (bRecordMetadata)
	{
		SaveCameraMetadata();
	}
}

void AFusionCamCaptureActor::RecordBulletTimeSequence()
{
	if (!IsValid(TargetToHide))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("FusionCamCaptureActor: No target for bullet time recording"));
		return;
	}

	UE_LOG(LogUnrealCV, Display, TEXT("FusionCamCaptureActor: Starting bullet time sequence"));

	// Save original camera transform
	FVector OriginalLocation = TargetSensor->GetSensorLocation();
	FRotator OriginalRotation = TargetSensor->GetSensorRotation();

	// Pause the world
	GetWorld()->GetFirstPlayerController()->SetPause(true);

	// Calculate rotation parameters
	FVector TargetLocation = TargetToHide->GetActorLocation();
	FVector Offset = OriginalLocation - TargetLocation;
	FRotator InitRotation = OriginalRotation - (TargetLocation - OriginalLocation).Rotation();

	float BulletTimeElapsedDeg = 0.0f;
	TArray<TArray<FColor>> DataRGBFrames, DataMaskFrames, DataRGBNoTargetFrames;

	// Capture 360° rotation
	while (BulletTimeElapsedDeg + BulletTimeSpeedDeg <= 360.0f)
	{
		BulletTimeElapsedDeg += BulletTimeSpeedDeg;

		// Rotate camera around target
		FQuat RotationQuat = FQuat(FVector::UpVector, FMath::DegreesToRadians(BulletTimeSpeedDeg));
		Offset = RotationQuat.RotateVector(Offset);
		FVector NewCameraLocation = TargetLocation + Offset;
		FRotator NewRotation = (TargetLocation - NewCameraLocation).Rotation() + InitRotation;

		TargetSensor->SetSensorLocation(NewCameraLocation);
		TargetSensor->SetSensorRotation(NewRotation);

		// Capture frame with target
		int Width, Height;
		TArray<FColor> DataRGB, DataMask;
		TargetSensor->GetLitSeg(DataRGB, DataMask, Width, Height);
		DataRGBFrames.Add(DataRGB);
		DataMaskFrames.Add(DataMask);

		// Capture frame without target if requested
		if (bRecordWithoutTarget)
		{
			TArray<FColor> DataRGBNoTarget;
			FActorController TargetController(TargetToHide);
			TargetController.Hide();
			TargetSensor->GetLit(DataRGBNoTarget, Width, Height);
			TargetController.Show();
			DataRGBNoTargetFrames.Add(DataRGBNoTarget);
		}
	}

	// Resume the world
	GetWorld()->GetFirstPlayerController()->SetPause(false);

	// Restore original camera transform
	TargetSensor->SetSensorLocation(OriginalLocation);
	TargetSensor->SetSensorRotation(OriginalRotation);

	// Save all frames
	int Width = TargetSensor->GetFilmWidth();
	int Height = TargetSensor->GetFilmHeight();

	for (int i = 0; i < DataRGBFrames.Num(); i++)
	{
		FString FileNameRGB = MakeFilename("rgb", ".png");
		FString FileNameMask = MakeFilename("mask", ".png");

		SerializeData(DataRGBFrames[i], Width, Height, FileNameRGB);
		SerializeData(DataMaskFrames[i], Width, Height, FileNameMask);

		if (bRecordWithoutTarget && i < DataRGBNoTargetFrames.Num())
		{
			FString FileNameNoTarget = MakeFilename("rgb_no_target", ".png");
			SerializeData(DataRGBNoTargetFrames[i], Width, Height, FileNameNoTarget);
		}

		ElapsedSteps++;
	}

	UE_LOG(LogUnrealCV, Display, TEXT("FusionCamCaptureActor: Bullet time sequence complete, %d frames captured"), DataRGBFrames.Num());
}

void AFusionCamCaptureActor::StartAudioRecord()
{
	Audio::FMixerDevice* MixerDevice = GetAudioMixer();
	if (MixerDevice == nullptr)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("FusionCamCaptureActor: Failed to start audio recording - no audio mixer"));
		return;
	}

	MixerDevice->StartRecording(nullptr, 100.0f);
	UE_LOG(LogUnrealCV, Display, TEXT("FusionCamCaptureActor: Audio recording started"));
}

void AFusionCamCaptureActor::StopAudioRecord()
{
	Audio::FMixerDevice* MixerDevice = GetAudioMixer();
	if (MixerDevice == nullptr)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("FusionCamCaptureActor: Failed to stop audio recording - no audio mixer"));
		return;
	}

	float NumChannels = 1.0f;
	float SampleRate = 44100.0f;
	Audio::FAlignedFloatBuffer& RecordedBuffer = MixerDevice->StopRecording(nullptr, NumChannels, SampleRate);

	// Convert float audio to PCM16
	TArray<int16> PCM16Data;
	PCM16Data.Reserve(RecordedBuffer.Num());

	for (float Sample : RecordedBuffer)
	{
		float Clamped = FMath::Clamp(Sample, -1.0f, 1.0f);
		PCM16Data.Add(static_cast<int16>(Clamped * 32767.0f));
	}

	// Save as WAV file
	FString WavFileName = MakeFilename("audio", ".wav");
	FBufferArchive WaveData;

	int32 NumSamples = PCM16Data.Num();
	int32 NumBytes = NumSamples * sizeof(int16);

	// Write WAV header
	WaveData.Serialize(const_cast<char*>("RIFF"), 4);
	int32 ChunkSize = 36 + NumBytes;
	WaveData << ChunkSize;
	WaveData.Serialize(const_cast<char*>("WAVE"), 4);

	// fmt chunk
	WaveData.Serialize(const_cast<char*>("fmt "), 4);
	int32 SubChunk1Size = 16;
	WaveData << SubChunk1Size;
	int16 AudioFormat = 1; // PCM
	WaveData << AudioFormat;
	int16 Channels = static_cast<int16>(NumChannels);
	WaveData << Channels;
	int32 SR = static_cast<int32>(SampleRate);
	WaveData << SR;
	int32 ByteRate = SR * Channels * sizeof(int16);
	WaveData << ByteRate;
	int16 BlockAlign = Channels * sizeof(int16);
	WaveData << BlockAlign;
	int16 BitsPerSample = 16;
	WaveData << BitsPerSample;

	// data chunk
	WaveData.Serialize(const_cast<char*>("data"), 4);
	WaveData << NumBytes;
	WaveData.Serialize(PCM16Data.GetData(), NumBytes);

	// Save to file
	FFileHelper::SaveArrayToFile(WaveData, *WavFileName);
	WaveData.FlushCache();
	WaveData.Empty();

	UE_LOG(LogUnrealCV, Display, TEXT("FusionCamCaptureActor: Audio saved to %s"), *WavFileName);
}

Audio::FMixerDevice* AFusionCamCaptureActor::GetAudioMixer()
{
	if (!IsValid(TargetSensor))
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	FVector CamLocation = TargetSensor->GetSensorLocation();
	FRotator CamRotation = TargetSensor->GetSensorRotation();

	FAudioDevice* AudioDevice = World->GetAudioDeviceRaw();
	if (!AudioDevice)
	{
		return nullptr;
	}

	FTransform ListenerTransform(CamRotation, CamLocation);
	AudioDevice->SetListener(World, 0, ListenerTransform, 0.0f);

	return static_cast<Audio::FMixerDevice*>(AudioDevice);
}

FString AFusionCamCaptureActor::MakeFilename(FString DataType, FString FileExtension)
{
	// Find the position to insert frame number
	int32 Index;
	if (!RecordFileName.FindLastChar(TEXT('.'), Index))
	{
		Index = RecordFileName.Len();
	}

	// Create filename with frame number and data type
	FString FileName = RecordFileName;
	FString InsertStr = FString::Printf(TEXT("%d_%s"), ElapsedSteps, *DataType);
	FileName.InsertAt(Index, InsertStr);

	// Combine with output folder
	FileName = FPaths::ConvertRelativePathToFull(FinalDataFolder, FileName);

	return FileName;
}

void AFusionCamCaptureActor::SaveCameraMetadata()
{
	if (!IsValid(TargetSensor))
	{
		return;
	}

	TArray<FString> Keys = {
		"FrameNumber",
		"Location",
		"Rotation",
		"FilmWidth",
		"FilmHeight",
		"FOV"
	};

	TArray<FJsonObjectBP> Values = {
		FJsonObjectBP(ElapsedSteps),
		FJsonObjectBP(TargetSensor->GetSensorLocation()),
		FJsonObjectBP(TargetSensor->GetSensorRotation()),
		FJsonObjectBP(TargetSensor->GetFilmWidth()),
		FJsonObjectBP(TargetSensor->GetFilmHeight()),
		FJsonObjectBP(TargetSensor->GetSensorFOV())
	};

	FJsonObjectBP JsonObject = USerializeBPLib::TMapToJson(Keys, Values);
	FString JsonStr = USerializeBPLib::JsonToStr(JsonObject);
	FString JsonFilename = MakeFilename("metadata", ".json");

	UVisionBPLib::SaveData(JsonStr, JsonFilename);
}
