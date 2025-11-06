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
#include "Utils/PythonExecutor.h"
#include "Misc/FileHelper.h"
#include "Serialization/BufferArchive.h"

static const float ROTATE_BUFFER_DURATION_SECONDS = 2.0f;
static const int32 ROTATE_NUM_FRAMES_OVERRIDE = 121;

AFusionCamCaptureActor::AFusionCamCaptureActor()
{
	PrimaryActorTick.bCanEverTick = true;

	bIsRecording = false;
	bAddTimestamp = true;
	bRecordRGB = true;
	bRecordMask = true;
	bRecordDepth = true;
	bRecordNormal = false;
	bRecordFlow = false;
	bRecordMetadata = true;
	bRecordAudio = true;
	bRecordWithoutTarget = false;
	ElapsedSteps = 0;
	TargetToHide = nullptr;
	NumFrames = 0;

	TimeDilation = 0.2f;
	TimeDilationBackUp = 1.0f;

	bAutoGenerateVideo = true;
	CondaEnvName = TEXT("uezoo");
	VideoGenScriptPath = TEXT("");

	CurrentTrajectoryIndex = 0;
	bPauseWorldDuringRecord = true;

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

void AFusionCamCaptureActor::SetSceneHandle(const FSceneHandle& InSceneHandle)
{
	SceneHandle = InSceneHandle;
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
		CurrentTrajectory.Empty();
		CurrentTrajectoryIndex = 0;

		if (bPauseWorldDuringRecord)
		{
			GetWorld()->GetFirstPlayerController()->SetPause(false);
		}

		// if (TimeDilationBackUp > 0.0f)
		// {
		// 	GetWorld()->GetWorldSettings()->SetTimeDilation(TimeDilationBackUp);
		// }
		GetWorld()->GetWorldSettings()->SetTimeDilation(1.0f);

		if (IsValid(TargetSensor))
		{
			TargetSensor->SetSensorLocation(OriginalCameraLocation);
			TargetSensor->SetSensorRotation(OriginalCameraRotation);
		}

		TriggerVideoGeneration();
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

	if (CurrentTrajectoryIndex >= CurrentTrajectory.Num())
	{
		UE_LOG(LogUnrealCV, Log, TEXT("FusionCamCaptureActor: Stop recording normally. CurrentTrajectoryIndex = %d, NumFrames = %d"), CurrentTrajectoryIndex, NumFrames);
		StopRecord();
		return;
	}

	float EffectiveTimeDilation = TimeDilation * CurrentTrajectory[CurrentTrajectoryIndex].DesiredEstTimeDilation;

	AWorldSettings* WorldSettings = GetWorld()->GetWorldSettings();

	if (FMath::IsNearlyZero(EffectiveTimeDilation))
	{
		WorldSettings->SetTimeDilation(0.0f);

		while (CurrentTrajectoryIndex < CurrentTrajectory.Num() &&
			   FMath::IsNearlyZero(TimeDilation * CurrentTrajectory[CurrentTrajectoryIndex].DesiredEstTimeDilation))
		{
			if (CurrentTrajectory[CurrentTrajectoryIndex].bManageTransform)
			{
				TargetSensor->SetSensorLocation(CurrentTrajectory[CurrentTrajectoryIndex].Location);
				TargetSensor->SetSensorRotation(CurrentTrajectory[CurrentTrajectoryIndex].Rotation);
			}

			RecordFrame();

			CurrentTrajectoryIndex++;
			ElapsedSteps++;
		}
	}
	else
	{
		if (CurrentTrajectory[CurrentTrajectoryIndex].bManageTransform)
		{
			TargetSensor->SetSensorLocation(CurrentTrajectory[CurrentTrajectoryIndex].Location);
			TargetSensor->SetSensorRotation(CurrentTrajectory[CurrentTrajectoryIndex].Rotation);
		}

		RecordFrame();

		CurrentTrajectoryIndex++;
		ElapsedSteps++;
	}

	if (CurrentTrajectoryIndex < CurrentTrajectory.Num())
	{
		EffectiveTimeDilation = TimeDilation * CurrentTrajectory[CurrentTrajectoryIndex].DesiredEstTimeDilation;
		if (FMath::Abs(EffectiveTimeDilation - WorldSettings->TimeDilation) < 0.05f)
		{
			WorldSettings->SetTimeDilation(EffectiveTimeDilation);
		}
		else if (EffectiveTimeDilation != WorldSettings->TimeDilation)
		{
			WorldSettings->SetTimeDilation(0.2f * EffectiveTimeDilation + 0.8f * WorldSettings->TimeDilation);
		}
	}
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
		SerializeData(DepthData, Width, Height, DepthFilename);

		TArray<FColor> DepthPreview;
		ConvertDepthToPreview(DepthData, DepthPreview);
		FString DepthPreviewFilename = MakeFilenameNew("depth_preview", ".png");
		SerializeData(DepthPreview, Width, Height, DepthPreviewFilename);
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

	FVector Location = TargetSensor->GetSensorLocation();
	FRotator Rotation = TargetSensor->GetSensorRotation();
	float FOV = TargetSensor->GetSensorFOV();
	int32 Width = TargetSensor->GetFilmWidth();
	int32 Height = TargetSensor->GetFilmHeight();

	float FOVRadians = FMath::DegreesToRadians(FOV);
	float FocalLengthX = Width / (2.0f * FMath::Tan(FOVRadians / 2.0f));
	float FocalLengthY = FocalLengthX;
	float PrincipalPointX = Width / 2.0f;
	float PrincipalPointY = Height / 2.0f;

	FMatrix RotationMatrix = FRotationMatrix::Make(Rotation);

	TMap<FString, float> IntrinsicsMap;
	IntrinsicsMap.Add("fx", FocalLengthX);
	IntrinsicsMap.Add("fy", FocalLengthY);
	IntrinsicsMap.Add("cx", PrincipalPointX);
	IntrinsicsMap.Add("cy", PrincipalPointY);
	IntrinsicsMap.Add("fov", FOV);
	IntrinsicsMap.Add("width", static_cast<float>(Width));
	IntrinsicsMap.Add("height", static_cast<float>(Height));

	TArray<float> RotationArray;
	for (int i = 0; i < 3; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			RotationArray.Add(RotationMatrix.M[i][j]);
		}
	}

	TArray<float> TranslationArray;
	TranslationArray.Add(Location.X);
	TranslationArray.Add(Location.Y);
	TranslationArray.Add(Location.Z);

	TArray<FString> ExtrinsicsKeys = {"rotation_matrix", "translation"};
	TArray<FJsonObjectBP> ExtrinsicsValues;

	TArray<FJsonObjectBP> RotMatrixArray;
	for (float Val : RotationArray)
	{
		RotMatrixArray.Add(FJsonObjectBP(Val));
	}
	ExtrinsicsValues.Add(FJsonObjectBP(RotMatrixArray));

	TArray<FJsonObjectBP> TransArray;
	for (float Val : TranslationArray)
	{
		TransArray.Add(FJsonObjectBP(Val));
	}
	ExtrinsicsValues.Add(FJsonObjectBP(TransArray));

	TArray<FJsonObjectBP> OccluderArray;
	for (const FOccluderMetadata& Occluder : SceneHandle.OccluderMetadataList)
	{
		TArray<FString> Keys;
		TArray<FJsonObjectBP> Values;
		for (auto& Pair : Occluder.Metadata)
		{
			Keys.Add(Pair.Key);
			Values.Add(FJsonObjectBP(Pair.Value));
		}
		FJsonObjectBP OccluderJson = USerializeBPLib::TMapToJson(Keys, Values);
		OccluderArray.Add(OccluderJson);
	}

	TArray<FString> Keys = {
		"FrameNumber",
		"VideoName",
		"Resolution",
		"Width",
		"Height",
		"FrameCount",
		"SceneCategory",
		"ForegroundCategory",
		"ForegroundSubcategory",
		"ForegroundObjectMetadata",
		"OccluderMetaDataList",
		"OcclusionRatio",
		"Location",
		"Rotation",
		"FOV",
		"Intrinsics",
		"Extrinsics"
	};

	FString ResolutionStr = FString::Printf(TEXT("%dx%d"), Width, Height);

	TArray<FJsonObjectBP> Values = {
		FJsonObjectBP(NumFrames),
		FJsonObjectBP(RecordFileName),
		FJsonObjectBP(ResolutionStr),
		FJsonObjectBP(Width),
		FJsonObjectBP(Height),
		FJsonObjectBP(ElapsedSteps),
		FJsonObjectBP(SceneHandle.SceneCategory),
		FJsonObjectBP(SceneHandle.ForegroundCategory),
		FJsonObjectBP(SceneHandle.ForegroundSubcategory),
		FJsonObjectBP(SceneHandle.ForegroundObjectMetadata),
		FJsonObjectBP(OccluderArray),
		FJsonObjectBP(SceneHandle.OcclusionRatio),
		FJsonObjectBP(Location),
		FJsonObjectBP(Rotation),
		FJsonObjectBP(FOV),
		FJsonObjectBP(IntrinsicsMap),
		FJsonObjectBP(ExtrinsicsKeys, ExtrinsicsValues)
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

void AFusionCamCaptureActor::StartTrajectoryRecord(const FString& FileName, ECameraTrajectoryType TrajectoryType, AActor* Target, int32 FPS, float DegreesPerSecond, int32 RandomSeed, bool bPauseWorldTime)
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

	float DegreesPerFrame = DegreesPerSecond / FPS;

	RecordFileName = FileName;
	RecordFPS = FPS;
	ElapsedSteps = 0;
	bIsRecording = true;
	TargetToHide = Target;
	bPauseWorldDuringRecord = bPauseWorldTime;

	static const TArray<FIntPoint> Resolutions = {
		// FIntPoint(1920, 1080),
		FIntPoint(640, 480),
		// FIntPoint(480, 640),
	};
	const FIntPoint& ChosenRes = Resolutions[FMath::RandRange(0, Resolutions.Num() - 1)];

	TargetSensor->SetFilmSize(ChosenRes.X, ChosenRes.Y);	
	OriginalCameraLocation = TargetSensor->GetSensorLocation();
	OriginalCameraRotation = TargetSensor->GetSensorRotation();

	CurrentTrajectory = CalculateTrajectory(TrajectoryType, Target, DegreesPerFrame, RandomSeed);

	if (bRecordAudio)
	{
		StartAudioRecord();
	}

	RenderTrajectory(CurrentTrajectory, bPauseWorldTime);
}

TArray<AFusionCamCaptureActor::FCameraPose> AFusionCamCaptureActor::CalculateTrajectory(ECameraTrajectoryType TrajectoryType, AActor* Target, float DegreesPerFrame, int32 RandomSeed)
{
	switch (TrajectoryType)
	{
	case ECameraTrajectoryType::RotateLeft45:
		return CalculateRotateLeft45(Target, DegreesPerFrame);
	case ECameraTrajectoryType::RotateRight45:
		return CalculateRotateRight45(Target, DegreesPerFrame);
	case ECameraTrajectoryType::RotateUp45:
		return CalculateRotateUp45(Target, DegreesPerFrame);
	case ECameraTrajectoryType::Rotate360:
		return CalculateRotate360(Target, DegreesPerFrame);
	case ECameraTrajectoryType::ZoomIn:
		return CalculateZoomIn(Target, DegreesPerFrame);
	case ECameraTrajectoryType::ZoomOut:
		return CalculateZoomOut(Target, DegreesPerFrame);
	case ECameraTrajectoryType::RandomDirection1:
	case ECameraTrajectoryType::RandomDirection2:
	case ECameraTrajectoryType::RandomDirection3:
	case ECameraTrajectoryType::RandomDirection4:
		return CalculateRandomDirection(Target, DegreesPerFrame, RandomSeed);
	case ECameraTrajectoryType::RenderOnly:
		return CalculateRenderOnly();
	default:
		UE_LOG(LogUnrealCV, Error, TEXT("Unknown trajectory type"));
		return TArray<FCameraPose>();
	}
}

void AFusionCamCaptureActor::RenderTrajectory(const TArray<FCameraPose>& Trajectory, bool bPauseWorldTime)
{
	if (Trajectory.Num() == 0)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("RenderTrajectory: Empty trajectory"));
		return;
	}
	TimeDilationBackUp = GetWorld()->GetWorldSettings()->TimeDilation;

	// if (bPauseWorldTime)
	// {
	// 	GetWorld()->GetFirstPlayerController()->SetPause(true);

	// 	ElapsedSteps = 0;
	// 	for (int i = 0; i < Trajectory.Num(); i++)
	// 	{
	// 		if (Trajectory[i].bManageTransform)
	// 		{
	// 			TargetSensor->SetSensorLocation(Trajectory[i].Location);
	// 			TargetSensor->SetSensorRotation(Trajectory[i].Rotation);
	// 		}

	// 		RecordFrame();
	// 		ElapsedSteps++;
	// 	}

	// 	GetWorld()->GetFirstPlayerController()->SetPause(false);
	// 	TargetSensor->SetSensorLocation(OriginalCameraLocation);
	// 	TargetSensor->SetSensorRotation(OriginalCameraRotation);

	// 	if (bRecordAudio)
	// 	{
	// 		StopAudioRecord();
	// 	}

	// 	TriggerVideoGeneration();

	// 	bIsRecording = false;
	// 	TargetToHide = nullptr;
	// 	CurrentTrajectory.Empty();
	// 	CurrentTrajectoryIndex = 0;
	// }
	// else
	{
		CurrentTrajectoryIndex = 0;
		ElapsedSteps = 0;

		float TimePerFrame = 1.0f / RecordFPS;

		OnTimerRecord();

		GetWorld()->GetTimerManager().SetTimer(
			TimerHandle_Record,
			this,
			&AFusionCamCaptureActor::OnTimerRecord,
			TimePerFrame,
			true
		);
	}
}

// ========== Individual Trajectory Calculation Functions ==========

TArray<AFusionCamCaptureActor::FCameraPose> AFusionCamCaptureActor::AddRotateBufferFrames(const TArray<FCameraPose>& CoreTrajectory)
{
	if (CoreTrajectory.Num() == 0)
	{
		return CoreTrajectory;
	}

	int32 BufferFrames = FMath::RoundToInt(ROTATE_BUFFER_DURATION_SECONDS * RecordFPS);
	TArray<FCameraPose> FullTrajectory;
	FullTrajectory.Reserve(BufferFrames * 2 + CoreTrajectory.Num());

	const FCameraPose& FirstPose = CoreTrajectory[0];
	for (int32 i = 0; i < BufferFrames; i++)
	{
		FCameraPose BufferPose;
		BufferPose.Location = FirstPose.Location;
		BufferPose.Rotation = FirstPose.Rotation;
		BufferPose.bManageTransform = false;
		BufferPose.DesiredEstTimeDilation = 1.0f;
		FullTrajectory.Add(BufferPose);
	}

	FullTrajectory.Append(CoreTrajectory);

	const FCameraPose& LastPose = CoreTrajectory.Last();
	for (int32 i = 0; i < BufferFrames; i++)
	{
		FCameraPose BufferPose;
		BufferPose.Location = LastPose.Location;
		BufferPose.Rotation = LastPose.Rotation;
		BufferPose.bManageTransform = false;
		BufferPose.DesiredEstTimeDilation = 1.0f;
		FullTrajectory.Add(BufferPose);
	}

	NumFrames = FullTrajectory.Num();
	return FullTrajectory;
}

TArray<AFusionCamCaptureActor::FCameraPose> AFusionCamCaptureActor::CalculateRotateLeft45(AActor* Target, float DegreesPerFrame)
{
	TArray<FCameraPose> CoreTrajectory;
	FVector TargetLocation = Target->GetActorLocation();
	FVector OriginalLocation = TargetSensor->GetSensorLocation();
	FRotator OriginalRotation = TargetSensor->GetSensorRotation();

	FVector Offset = OriginalLocation - TargetLocation;
	FRotator InitRotation = OriginalRotation - (TargetLocation - OriginalLocation).Rotation();

	float TotalRotationDeg = 45.0f;
	NumFrames = FMath::CeilToInt(TotalRotationDeg / DegreesPerFrame) + 1;
	if (ROTATE_NUM_FRAMES_OVERRIDE > 0)
	{
		NumFrames = ROTATE_NUM_FRAMES_OVERRIDE;
		DegreesPerFrame = TotalRotationDeg / (NumFrames - 1);
	}

	for (int i = 0; i < NumFrames; i++)
	{
		float CurrentDeg = FMath::Min(DegreesPerFrame * i, TotalRotationDeg);

		FQuat RotationQuat = FQuat(FVector::UpVector, FMath::DegreesToRadians(CurrentDeg));
		FVector NewOffset = RotationQuat.RotateVector(Offset);
		FVector NewLocation = TargetLocation + NewOffset;
		FRotator NewRotation = (TargetLocation - NewLocation).Rotation() + InitRotation;

		FCameraPose Pose;
		Pose.Location = NewLocation;
		Pose.Rotation = NewRotation;
		Pose.DesiredEstTimeDilation = 0.0f;
		CoreTrajectory.Add(Pose);
	}

	return AddRotateBufferFrames(CoreTrajectory);
}

TArray<AFusionCamCaptureActor::FCameraPose> AFusionCamCaptureActor::CalculateRotateRight45(AActor* Target, float DegreesPerFrame)
{
	TArray<FCameraPose> CoreTrajectory;
	FVector TargetLocation = Target->GetActorLocation();
	FVector OriginalLocation = TargetSensor->GetSensorLocation();
	FRotator OriginalRotation = TargetSensor->GetSensorRotation();

	FVector Offset = OriginalLocation - TargetLocation;
	FRotator InitRotation = OriginalRotation - (TargetLocation - OriginalLocation).Rotation();

	float TotalRotationDeg = -45.0f;
	NumFrames = FMath::CeilToInt(FMath::Abs(TotalRotationDeg) / DegreesPerFrame) + 1;
	if (ROTATE_NUM_FRAMES_OVERRIDE > 0)
	{
		NumFrames = ROTATE_NUM_FRAMES_OVERRIDE;
		DegreesPerFrame = TotalRotationDeg / (NumFrames - 1);
	}

	for (int i = 0; i < NumFrames; i++)
	{
		float CurrentDeg = FMath::Max(DegreesPerFrame * i * -1.0f, TotalRotationDeg);

		FQuat RotationQuat = FQuat(FVector::UpVector, FMath::DegreesToRadians(CurrentDeg));
		FVector NewOffset = RotationQuat.RotateVector(Offset);
		FVector NewLocation = TargetLocation + NewOffset;
		FRotator NewRotation = (TargetLocation - NewLocation).Rotation() + InitRotation;

		FCameraPose Pose;
		Pose.Location = NewLocation;
		Pose.Rotation = NewRotation;
		Pose.DesiredEstTimeDilation = 0.0f;
		CoreTrajectory.Add(Pose);
	}

	return AddRotateBufferFrames(CoreTrajectory);
}

TArray<AFusionCamCaptureActor::FCameraPose> AFusionCamCaptureActor::CalculateRotateUp45(AActor* Target, float DegreesPerFrame)
{
	TArray<FCameraPose> CoreTrajectory;
	FVector TargetLocation = Target->GetActorLocation();
	FVector OriginalLocation = TargetSensor->GetSensorLocation();
	FRotator OriginalRotation = TargetSensor->GetSensorRotation();

	FVector Offset = OriginalLocation - TargetLocation;
	FRotator InitRotation = OriginalRotation - (TargetLocation - OriginalLocation).Rotation();

	FVector ToCamera = Offset.GetSafeNormal();
	FVector RightVector = FVector::CrossProduct(ToCamera, FVector::UpVector).GetSafeNormal();

	float TotalRotationDeg = 45.0f;
	NumFrames = FMath::CeilToInt(TotalRotationDeg / DegreesPerFrame) + 1;
	if (ROTATE_NUM_FRAMES_OVERRIDE > 0)
	{
		NumFrames = ROTATE_NUM_FRAMES_OVERRIDE;
		DegreesPerFrame = TotalRotationDeg / (NumFrames - 1);
	}

	for (int i = 0; i < NumFrames; i++)
	{
		float CurrentDeg = FMath::Min(DegreesPerFrame * i, TotalRotationDeg);

		FQuat RotationQuat = FQuat(RightVector, FMath::DegreesToRadians(CurrentDeg));
		FVector NewOffset = RotationQuat.RotateVector(Offset);
		FVector NewLocation = TargetLocation + NewOffset;
		FRotator NewRotation = (TargetLocation - NewLocation).Rotation() + InitRotation;

		FCameraPose Pose;
		Pose.Location = NewLocation;
		Pose.Rotation = NewRotation;
		Pose.DesiredEstTimeDilation = 0.0f;
		CoreTrajectory.Add(Pose);
	}

	return AddRotateBufferFrames(CoreTrajectory);
}

TArray<AFusionCamCaptureActor::FCameraPose> AFusionCamCaptureActor::CalculateRotate360(AActor* Target, float DegreesPerFrame)
{
	TArray<FCameraPose> CoreTrajectory;
	FVector TargetLocation = Target->GetActorLocation();
	FVector OriginalLocation = TargetSensor->GetSensorLocation();
	FRotator OriginalRotation = TargetSensor->GetSensorRotation();

	FVector Offset = OriginalLocation - TargetLocation;
	FRotator InitRotation = OriginalRotation - (TargetLocation - OriginalLocation).Rotation();

	float TotalRotationDeg = 360.0f;
	NumFrames = FMath::CeilToInt(TotalRotationDeg / DegreesPerFrame) + 1;
	if (ROTATE_NUM_FRAMES_OVERRIDE > 0)
	{
		NumFrames = ROTATE_NUM_FRAMES_OVERRIDE;
		DegreesPerFrame = TotalRotationDeg / (NumFrames - 1);
	}

	for (int i = 0; i < NumFrames; i++)
	{
		float CurrentDeg = FMath::Min(DegreesPerFrame * i, TotalRotationDeg);

		FQuat RotationQuat = FQuat(FVector::UpVector, FMath::DegreesToRadians(CurrentDeg));
		FVector NewOffset = RotationQuat.RotateVector(Offset);
		FVector NewLocation = TargetLocation + NewOffset;
		FRotator NewRotation = (TargetLocation - NewLocation).Rotation() + InitRotation;

		FCameraPose Pose;
		Pose.Location = NewLocation;
		Pose.Rotation = NewRotation;
		Pose.DesiredEstTimeDilation = 0.0f;
		CoreTrajectory.Add(Pose);
	}

	return AddRotateBufferFrames(CoreTrajectory);
}

TArray<AFusionCamCaptureActor::FCameraPose> AFusionCamCaptureActor::CalculateZoomIn(AActor* Target, float DegreesPerFrame)
{
	TArray<FCameraPose> Trajectory;
	FVector TargetLocation = Target->GetActorLocation();
	FVector OriginalLocation = TargetSensor->GetSensorLocation();
	FRotator OriginalRotation = TargetSensor->GetSensorRotation();

	FVector Offset = OriginalLocation - TargetLocation;
	float OriginalDistance = Offset.Size();
	float MinDistance = OriginalDistance * 0.5f;

	NumFrames = 121;

	for (int i = 0; i < NumFrames; i++)
	{
		float Alpha = static_cast<float>(i) / (NumFrames - 1);
		float CurrentDistance = FMath::Lerp(OriginalDistance, MinDistance, Alpha);

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

TArray<AFusionCamCaptureActor::FCameraPose> AFusionCamCaptureActor::CalculateZoomOut(AActor* Target, float DegreesPerFrame)
{
	TArray<FCameraPose> Trajectory;
	FVector TargetLocation = Target->GetActorLocation();
	FVector OriginalLocation = TargetSensor->GetSensorLocation();
	FRotator OriginalRotation = TargetSensor->GetSensorRotation();

	FVector Offset = OriginalLocation - TargetLocation;
	float OriginalDistance = Offset.Size();
	float MaxDistance = OriginalDistance * 2.0f;

	NumFrames = 121;

	for (int i = 0; i < NumFrames; i++)
	{
		float Alpha = static_cast<float>(i) / (NumFrames - 1);
		float CurrentDistance = FMath::Lerp(OriginalDistance, MaxDistance, Alpha);

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

TArray<AFusionCamCaptureActor::FCameraPose> AFusionCamCaptureActor::CalculateRandomDirection(AActor* Target, float DegreesPerFrame, int32 RandomSeed)
{
	TArray<FCameraPose> Trajectory;
	FVector TargetLocation = Target->GetActorLocation();
	FVector OriginalLocation = TargetSensor->GetSensorLocation();
	FRotator OriginalRotation = TargetSensor->GetSensorRotation();

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

	float RandomYaw = RandomStream.FRandRange(-180.0f, 180.0f);
	float RandomPitch = RandomStream.FRandRange(0.0f, 45.0f);
	FVector RandomAxis = FRotator(RandomPitch, RandomYaw, 0.0f).Vector();

	float TotalRotationDeg = RandomStream.FRandRange(30.0f, 90.0f);
	NumFrames = FMath::CeilToInt(TotalRotationDeg / DegreesPerFrame) + 1;
	if (ROTATE_NUM_FRAMES_OVERRIDE > 0)
	{
		NumFrames = ROTATE_NUM_FRAMES_OVERRIDE;
		DegreesPerFrame = TotalRotationDeg / (NumFrames - 1);
	}

	bool bVaryDistance = RandomStream.FRand() > 0.5f;
	float OriginalDistance = Offset.Size();
	float DistanceVariation = RandomStream.FRandRange(0.7f, 1.3f);

	for (int i = 0; i < NumFrames; i++)
	{
		float Alpha = static_cast<float>(i) / (NumFrames - 1);
		float CurrentDeg = FMath::Min(DegreesPerFrame * i, TotalRotationDeg);

		FQuat RotationQuat = FQuat(RandomAxis, FMath::DegreesToRadians(CurrentDeg));
		FVector NewOffset = RotationQuat.RotateVector(Offset);

		if (bVaryDistance)
		{
			float CurrentDistance = FMath::Lerp(OriginalDistance, OriginalDistance * DistanceVariation, Alpha);
			NewOffset = NewOffset.GetSafeNormal() * CurrentDistance;
		}

		float NewHeight = 0.;
		FVector NewLocation = TargetLocation + NewOffset;
		if (NewLocation.Z < NewHeight) 
		{
			NewLocation.Z = NewHeight;
		}
		FRotator NewRotation = (TargetLocation - NewLocation).Rotation() + InitRotation;

		FCameraPose Pose;
		Pose.Location = NewLocation;
		Pose.Rotation = NewRotation;
		Trajectory.Add(Pose);
	}

	return Trajectory;
}

TArray<AFusionCamCaptureActor::FCameraPose> AFusionCamCaptureActor::CalculateRenderOnly()
{
	TArray<FCameraPose> Trajectory;
	NumFrames = RecordFPS * 10;

	for (int i = 0; i < NumFrames; i++)
	{
		FCameraPose Pose;
		Pose.bManageTransform = false;
		Trajectory.Add(Pose);
	}

	return Trajectory;
}


void AFusionCamCaptureActor::TriggerVideoGeneration()
{
	if (!bAutoGenerateVideo)
	{
		UE_LOG(LogUnrealCV, Display, TEXT("FusionCamCaptureActor: Auto video generation disabled"));
		return;
	}

	if (VideoGenScriptPath.IsEmpty())
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("FusionCamCaptureActor: VideoGenScriptPath not set, searching for genvid.py"));

		FString PluginBaseDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectPluginsDir() / TEXT("unrealcv/Source/uezoo"));
		FString AutoScriptPath = FPaths::Combine(PluginBaseDir, TEXT("genvid.py"));

		if (FPaths::FileExists(AutoScriptPath))
		{
			VideoGenScriptPath = AutoScriptPath;
			UE_LOG(LogUnrealCV, Display, TEXT("FusionCamCaptureActor: Found genvid.py at %s"), *VideoGenScriptPath);
		}
		else
		{
			UE_LOG(LogUnrealCV, Error, TEXT("FusionCamCaptureActor: Cannot find genvid.py at %s"), *AutoScriptPath);
			return;
		}
	}

	FString InputDir = FPaths::ConvertRelativePathToFull(FinalDataFolder, RecordFileName);
	int32 ProcessID = 0;

	bool bSuccess = FPythonExecutor::ExecuteGenvidScript(
		VideoGenScriptPath,
		InputDir,
		RecordFPS,
		CondaEnvName,
		&ProcessID
	);

	if (bSuccess)
	{
		UE_LOG(LogUnrealCV, Display, TEXT("FusionCamCaptureActor: Video generation started (PID: %d) for folder: %s"), ProcessID, *InputDir);
	}
	else
	{
		UE_LOG(LogUnrealCV, Error, TEXT("FusionCamCaptureActor: Failed to start video generation for folder: %s"), *InputDir);
	}
}
