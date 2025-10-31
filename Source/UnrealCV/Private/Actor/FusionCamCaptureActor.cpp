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

	TimeDilation = 1.0f;
	TimeDilationBackUp = 1.0f; 

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
	TimeDilationBackUp = WorldSettings->TimeDilation;

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

void AFusionCamCaptureActor::StartBulletTimeRecordOnly(const FString& FileName, float Duration, int32 FPS, AActor* Target)
{
	if (!IsValid(Target))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("FusionCamCaptureActor: Target is null in StartBulletTimeRecord"));
		return;
	}
	AWorldSettings* WorldSettings = GetWorld()->GetWorldSettings();
	TimeDilationBackUp = WorldSettings->TimeDilation;

	RecordFileName = FileName;
	RecordDuration = Duration;
	RecordFPS = FPS;
	TimePerFrame = 1.0f / FPS;
	ElapsedTime = 0.0f;
	ElapsedSteps = 0;
	bIsRecording = true;
	TargetToHide = Target;

	if (bRecordAudio)
	{
		StartAudioRecord();
	}
	RecordBulletTimeSequence();
	if (bRecordAudio)
	{
		StopAudioRecord();
	}
	bIsRecording = false;
	TargetToHide = nullptr;
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
		GetWorld()->GetWorldSettings()->SetTimeDilation(TimeDilationBackUp);
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

		FString FileNameRGB = MakeFilenameNew("rgb", ".png");
		FString FileNameMask = MakeFilenameNew("mask", ".png");
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

			FString FileNameNoTarget = MakeFilenameNew("rgb_no_target", ".png");
			SerializeData(DataRGBNoTarget, Width, Height, FileNameNoTarget);
		}
	}
	else if (bRecordRGB)
	{
		TArray<FColor> DataRGB;
		TargetSensor->GetLit(DataRGB, Width, Height);
		FString FileNameRGB = MakeFilenameNew("rgb", ".png");
		SerializeData(DataRGB, Width, Height, FileNameRGB);
	}
	else if (bRecordMask)
	{
		TArray<FColor> DataMask;
		TargetSensor->GetSeg(DataMask, Width, Height);
		FString FileNameMask = MakeFilenameNew("mask", ".png");
		SerializeData(DataMask, Width, Height, FileNameMask);
	}

	// Record depth
	if (bRecordDepth)
	{
		TArray<float> DepthData;
		TargetSensor->GetDepth(DepthData, Width, Height);
		FString DepthFilename = MakeFilenameNew("depth", ".npy");
		UVisionBPLib::SaveNpy(DepthData, Width, Height, DepthFilename);
	}

	// Record normal
	if (bRecordNormal)
	{
		TArray<FColor> NormalData;
		TargetSensor->GetNormal(NormalData, Width, Height);
		FString NormalFilename = MakeFilenameNew("normal", ".png");
		SerializeData(NormalData, Width, Height, NormalFilename);
	}

	// Record optical flow
	if (bRecordFlow)
	{
		TArray<FColor> FlowData;
		TargetSensor->GetFlow(FlowData, Width, Height);
		FString FlowFilename = MakeFilenameNew("flow", ".png");
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
		FString FileNameRGB = MakeFilenameNew("rgb", ".png");
		FString FileNameMask = MakeFilenameNew("mask", ".png");

		SerializeData(DataRGBFrames[i], Width, Height, FileNameRGB);
		SerializeData(DataMaskFrames[i], Width, Height, FileNameMask);

		if (bRecordWithoutTarget && i < DataRGBNoTargetFrames.Num())
		{
			FString FileNameNoTarget = MakeFilenameNew("rgb_no_target", ".png");
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
	FString WavFileName = MakeFilenameNew("audio", ".wav");
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
FString AFusionCamCaptureActor::MakeFilenameNew(FString DataType, FString FileExtension)
{
	// Find the position to insert frame number
	

	if (FileExtension.StartsWith(".")) {
		FileExtension.RemoveAt(0);
	}

	// Create filename with frame number and data type
	FString FileName = RecordFileName;
	FString InsertStr = FString::Printf(TEXT("%d_%s.%s"), ElapsedSteps, *DataType, *FileExtension);
	FileName = FPaths::Combine(FileName, InsertStr);
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
	FString JsonFilename = MakeFilenameNew("metadata", ".json");

	UVisionBPLib::SaveData(JsonStr, JsonFilename);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////// Neo Trajector Render System /////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// ========== Camera Trajectory Recording Implementation ==========

void AFusionCamCaptureActor::StartTrajectoryRecord(const FString& FileName, ECameraTrajectoryType TrajectoryType, AActor* Target, int32 NumFrames, int32 RandomSeed)
{
	if (!IsValid(TargetSensor))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("FusionCamCaptureActor: TargetSensor is not set!"));
		return;
	}

	if (!IsValid(Target))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("FusionCamCaptureActor: Target is null for trajectory recording"));
		return;
	}

	if (bIsRecording)
	{
		StopRecord();
	}

	// Setup recording state
	RecordFileName = FileName;
	ElapsedSteps = 0;
	bIsRecording = true;
	TargetToHide = Target;

	// Calculate trajectory
	TArray<FCameraPose> Trajectory = CalculateTrajectory(TrajectoryType, Target, NumFrames, RandomSeed);

	// Render trajectory
	RenderTrajectory(Trajectory);

	// Cleanup
	bIsRecording = false;
	TargetToHide = nullptr;

	UE_LOG(LogUnrealCV, Display, TEXT("FusionCamCaptureActor: Trajectory recording complete - %d frames"), Trajectory.Num());
}

TArray<AFusionCamCaptureActor::FCameraPose> AFusionCamCaptureActor::CalculateTrajectory(ECameraTrajectoryType TrajectoryType, AActor* Target, int32 NumFrames, int32 RandomSeed)
{
	switch (TrajectoryType)
	{
	case ECameraTrajectoryType::RotateLeft45:
		return CalculateRotateLeft45(Target, NumFrames);
	case ECameraTrajectoryType::RotateRight45:
		return CalculateRotateRight45(Target, NumFrames);
	case ECameraTrajectoryType::RotateUp45:
		return CalculateRotateUp45(Target, NumFrames);
	case ECameraTrajectoryType::Rotate360:
		return CalculateRotate360(Target, NumFrames);
	case ECameraTrajectoryType::ZoomIn:
		return CalculateZoomIn(Target, NumFrames);
	case ECameraTrajectoryType::ZoomOut:
		return CalculateZoomOut(Target, NumFrames);
	case ECameraTrajectoryType::RandomDirection1:
	case ECameraTrajectoryType::RandomDirection2:
	case ECameraTrajectoryType::RandomDirection3:
	case ECameraTrajectoryType::RandomDirection4:
		return CalculateRandomDirection(Target, NumFrames, RandomSeed);
	default:
		UE_LOG(LogUnrealCV, Error, TEXT("Unknown trajectory type"));
		return TArray<FCameraPose>();
	}
}

void AFusionCamCaptureActor::RenderTrajectory(const TArray<FCameraPose>& Trajectory)
{
	if (Trajectory.Num() == 0)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("RenderTrajectory: Empty trajectory"));
		return;
	}

	// Save original camera transform
	FVector OriginalLocation = TargetSensor->GetSensorLocation();
	FRotator OriginalRotation = TargetSensor->GetSensorRotation();

	// Pause the world for consistent rendering
	GetWorld()->GetFirstPlayerController()->SetPause(true);

	// Render each frame
	for (int i = 0; i < Trajectory.Num(); i++)
	{
		// Set camera pose
		TargetSensor->SetSensorLocation(Trajectory[i].Location);
		TargetSensor->SetSensorRotation(Trajectory[i].Rotation);

		// Render frame
		RecordFrame();
	}

	// Resume the world
	GetWorld()->GetFirstPlayerController()->SetPause(false);

	// Restore original camera transform
	TargetSensor->SetSensorLocation(OriginalLocation);
	TargetSensor->SetSensorRotation(OriginalRotation);
}

// ========== Individual Trajectory Calculation Functions ==========

TArray<AFusionCamCaptureActor::FCameraPose> AFusionCamCaptureActor::CalculateRotateLeft45(AActor* Target, int32 NumFrames)
{
	TArray<FCameraPose> Trajectory;
	FVector TargetLocation = Target->GetActorLocation();
	FVector OriginalLocation = TargetSensor->GetSensorLocation();
	FRotator OriginalRotation = TargetSensor->GetSensorRotation();

	// Calculate offset from target
	FVector Offset = OriginalLocation - TargetLocation;
	FRotator InitRotation = OriginalRotation - (TargetLocation - OriginalLocation).Rotation();

	float TotalRotationDeg = 45.0f;
	float DegPerFrame = TotalRotationDeg / (NumFrames - 1);

	for (int i = 0; i < NumFrames; i++)
	{
		float CurrentDeg = DegPerFrame * i;

		// Rotate around Z-axis (left rotation)
		FQuat RotationQuat = FQuat(FVector::UpVector, FMath::DegreesToRadians(CurrentDeg));
		FVector NewOffset = RotationQuat.RotateVector(Offset);
		FVector NewLocation = TargetLocation + NewOffset;
		FRotator NewRotation = (TargetLocation - NewLocation).Rotation() + InitRotation;

		FCameraPose Pose;
		Pose.Location = NewLocation;
		Pose.Rotation = NewRotation;
		Trajectory.Add(Pose);
	}

	return Trajectory;
}

TArray<AFusionCamCaptureActor::FCameraPose> AFusionCamCaptureActor::CalculateRotateRight45(AActor* Target, int32 NumFrames)
{
	TArray<FCameraPose> Trajectory;
	FVector TargetLocation = Target->GetActorLocation();
	FVector OriginalLocation = TargetSensor->GetSensorLocation();
	FRotator OriginalRotation = TargetSensor->GetSensorRotation();

	FVector Offset = OriginalLocation - TargetLocation;
	FRotator InitRotation = OriginalRotation - (TargetLocation - OriginalLocation).Rotation();

	float TotalRotationDeg = -45.0f; // Negative for right rotation
	float DegPerFrame = TotalRotationDeg / (NumFrames - 1);

	for (int i = 0; i < NumFrames; i++)
	{
		float CurrentDeg = DegPerFrame * i;

		FQuat RotationQuat = FQuat(FVector::UpVector, FMath::DegreesToRadians(CurrentDeg));
		FVector NewOffset = RotationQuat.RotateVector(Offset);
		FVector NewLocation = TargetLocation + NewOffset;
		FRotator NewRotation = (TargetLocation - NewLocation).Rotation() + InitRotation;

		FCameraPose Pose;
		Pose.Location = NewLocation;
		Pose.Rotation = NewRotation;
		Trajectory.Add(Pose);
	}

	return Trajectory;
}

TArray<AFusionCamCaptureActor::FCameraPose> AFusionCamCaptureActor::CalculateRotateUp45(AActor* Target, int32 NumFrames)
{
	TArray<FCameraPose> Trajectory;
	FVector TargetLocation = Target->GetActorLocation();
	FVector OriginalLocation = TargetSensor->GetSensorLocation();
	FRotator OriginalRotation = TargetSensor->GetSensorRotation();

	FVector Offset = OriginalLocation - TargetLocation;
	FRotator InitRotation = OriginalRotation - (TargetLocation - OriginalLocation).Rotation();

	// Calculate right vector for pitch rotation
	FVector ToCamera = Offset.GetSafeNormal();
	FVector RightVector = FVector::CrossProduct(FVector::UpVector, ToCamera).GetSafeNormal();

	float TotalRotationDeg = 45.0f;
	float DegPerFrame = TotalRotationDeg / (NumFrames - 1);

	for (int i = 0; i < NumFrames; i++)
	{
		float CurrentDeg = DegPerFrame * i;

		// Rotate around right vector (pitch up)
		FQuat RotationQuat = FQuat(RightVector, FMath::DegreesToRadians(CurrentDeg));
		FVector NewOffset = RotationQuat.RotateVector(Offset);
		FVector NewLocation = TargetLocation + NewOffset;
		FRotator NewRotation = (TargetLocation - NewLocation).Rotation() + InitRotation;

		FCameraPose Pose;
		Pose.Location = NewLocation;
		Pose.Rotation = NewRotation;
		Trajectory.Add(Pose);
	}

	return Trajectory;
}

TArray<AFusionCamCaptureActor::FCameraPose> AFusionCamCaptureActor::CalculateRotate360(AActor* Target, int32 NumFrames)
{
	TArray<FCameraPose> Trajectory;
	FVector TargetLocation = Target->GetActorLocation();
	FVector OriginalLocation = TargetSensor->GetSensorLocation();
	FRotator OriginalRotation = TargetSensor->GetSensorRotation();

	FVector Offset = OriginalLocation - TargetLocation;
	FRotator InitRotation = OriginalRotation - (TargetLocation - OriginalLocation).Rotation();

	float TotalRotationDeg = 360.0f;
	float DegPerFrame = TotalRotationDeg / (NumFrames - 1);

	for (int i = 0; i < NumFrames; i++)
	{
		float CurrentDeg = DegPerFrame * i;

		FQuat RotationQuat = FQuat(FVector::UpVector, FMath::DegreesToRadians(CurrentDeg));
		FVector NewOffset = RotationQuat.RotateVector(Offset);
		FVector NewLocation = TargetLocation + NewOffset;
		FRotator NewRotation = (TargetLocation - NewLocation).Rotation() + InitRotation;

		FCameraPose Pose;
		Pose.Location = NewLocation;
		Pose.Rotation = NewRotation;
		Trajectory.Add(Pose);
	}

	return Trajectory;
}

TArray<AFusionCamCaptureActor::FCameraPose> AFusionCamCaptureActor::CalculateZoomIn(AActor* Target, int32 NumFrames)
{
	TArray<FCameraPose> Trajectory;
	FVector TargetLocation = Target->GetActorLocation();
	FVector OriginalLocation = TargetSensor->GetSensorLocation();
	FRotator OriginalRotation = TargetSensor->GetSensorRotation();

	FVector Offset = OriginalLocation - TargetLocation;
	float OriginalDistance = Offset.Size();
	float MinDistance = OriginalDistance * 0.5f; // Zoom to 50% of original distance

	for (int i = 0; i < NumFrames; i++)
	{
		float Alpha = static_cast<float>(i) / (NumFrames - 1);
		float CurrentDistance = FMath::Lerp(OriginalDistance, MinDistance, Alpha);

		// Move camera closer while maintaining direction
		FVector NewOffset = Offset.GetSafeNormal() * CurrentDistance;
		FVector NewLocation = TargetLocation + NewOffset;
		FRotator NewRotation = (TargetLocation - NewLocation).Rotation();

		FCameraPose Pose;
		Pose.Location = NewLocation;
		Pose.Rotation = NewRotation;
		Trajectory.Add(Pose);
	}

	return Trajectory;
}

TArray<AFusionCamCaptureActor::FCameraPose> AFusionCamCaptureActor::CalculateZoomOut(AActor* Target, int32 NumFrames)
{
	TArray<FCameraPose> Trajectory;
	FVector TargetLocation = Target->GetActorLocation();
	FVector OriginalLocation = TargetSensor->GetSensorLocation();
	FRotator OriginalRotation = TargetSensor->GetSensorRotation();

	FVector Offset = OriginalLocation - TargetLocation;
	float OriginalDistance = Offset.Size();
	float MaxDistance = OriginalDistance * 2.0f; // Zoom out to 200% of original distance

	for (int i = 0; i < NumFrames; i++)
	{
		float Alpha = static_cast<float>(i) / (NumFrames - 1);
		float CurrentDistance = FMath::Lerp(OriginalDistance, MaxDistance, Alpha);

		// Move camera farther while maintaining direction
		FVector NewOffset = Offset.GetSafeNormal() * CurrentDistance;
		FVector NewLocation = TargetLocation + NewOffset;
		FRotator NewRotation = (TargetLocation - NewLocation).Rotation();

		FCameraPose Pose;
		Pose.Location = NewLocation;
		Pose.Rotation = NewRotation;
		Trajectory.Add(Pose);
	}

	return Trajectory;
}

TArray<AFusionCamCaptureActor::FCameraPose> AFusionCamCaptureActor::CalculateRandomDirection(AActor* Target, int32 NumFrames, int32 RandomSeed)
{
	TArray<FCameraPose> Trajectory;
	FVector TargetLocation = Target->GetActorLocation();
	FVector OriginalLocation = TargetSensor->GetSensorLocation();
	FRotator OriginalRotation = TargetSensor->GetSensorRotation();

	// Initialize random stream with seed
	FRandomStream RandomStream;
	if (RandomSeed >= 0)
	{
		RandomStream.Initialize(RandomSeed);
	}
	else
	{
		RandomStream.Initialize(FMath::Rand());
	}

	FVector Offset = OriginalLocation - TargetLocation;
	FRotator InitRotation = OriginalRotation - (TargetLocation - OriginalLocation).Rotation();

	// Generate random rotation axis (combination of yaw and pitch)
	float RandomYaw = RandomStream.FRandRange(-180.0f, 180.0f);
	float RandomPitch = RandomStream.FRandRange(-45.0f, 45.0f);
	FVector RandomAxis = FRotator(RandomPitch, RandomYaw, 0.0f).Vector();

	// Random rotation magnitude
	float TotalRotationDeg = RandomStream.FRandRange(30.0f, 90.0f);
	float DegPerFrame = TotalRotationDeg / (NumFrames - 1);

	// Random distance variation (optional)
	bool bVaryDistance = RandomStream.FRand() > 0.5f;
	float OriginalDistance = Offset.Size();
	float DistanceVariation = RandomStream.FRandRange(0.7f, 1.3f);

	for (int i = 0; i < NumFrames; i++)
	{
		float Alpha = static_cast<float>(i) / (NumFrames - 1);
		float CurrentDeg = DegPerFrame * i;

		// Rotate around random axis
		FQuat RotationQuat = FQuat(RandomAxis, FMath::DegreesToRadians(CurrentDeg));
		FVector NewOffset = RotationQuat.RotateVector(Offset);

		// Optionally vary distance
		if (bVaryDistance)
		{
			float CurrentDistance = FMath::Lerp(OriginalDistance, OriginalDistance * DistanceVariation, Alpha);
			NewOffset = NewOffset.GetSafeNormal() * CurrentDistance;
		}

		FVector NewLocation = TargetLocation + NewOffset;
		FRotator NewRotation = (TargetLocation - NewLocation).Rotation() + InitRotation;

		FCameraPose Pose;
		Pose.Location = NewLocation;
		Pose.Rotation = NewRotation;
		Trajectory.Add(Pose);
	}

	return Trajectory;
}

