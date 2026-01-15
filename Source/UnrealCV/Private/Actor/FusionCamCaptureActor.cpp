// shc @ 2025
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
#include "LitCamSensor.h"
#include "DepthCamSensor.h"
#include "AnnotationCamSensor.h"
#include "NormalCamSensor.h"
#include "FlowCamSensor.h"
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
#include "LineTraceBPlib.h"

// static const float ROTATE_BUFFER_DURATION_SECONDS = 2.0f;
static const float ROTATE_BUFFER_DURATION_SECONDS = 0.0f;
static const int32 ROTATE_NUM_FRAMES_OVERRIDE = 121;
static const int32 WARM_UP_FRAMES = 5;

AFusionCamCaptureActor::AFusionCamCaptureActor()
{
	PrimaryActorTick.bCanEverTick = true;

	bIsRecording = false;
	bAddTimestamp = true;
	bRecordRGB = true;
	bRecordMask = true;
	bRecordDepth = false;
	bRecordNormal = false;
	bRecordFlow = false;
	bRecordOneObjectMask = true;
	bRecordMetadata = true;
	bRecordAudio = true;
	bRecordWithoutTarget = false;
	ElapsedSteps = 0;
	TargetToHide = nullptr;
	NumFrames = 0;

	TimeDilation = 0.25f;
	// TimeDilation = 0.1f;
	// TimeDilation = 1.0f;
	TimeDilationBackUp = 1.0f;

	bAutoGenerateVideo = true;
	CondaEnvName = TEXT("uezoo");
	VideoGenScriptPath = TEXT("");

	CurrentTrajectoryIndex = 0;
	bPauseWorldDuringRecord = false;
	WarmUpFrames = WARM_UP_FRAMES;
	WarmUpElapsedFrames = 0;

	// OriginalCameraLocation = 
	// OriginalCameraRotation = 

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
		if (IsValid(TargetSensor))
		{
			bool LocationManaged = false;
			for (int i = 0; i <= FMath::Min(CurrentTrajectoryIndex - 1, CurrentTrajectory.Num() - 1); i++)
			{
				if (CurrentTrajectory[i].bManageTransform)
				{
					LocationManaged = true;
					break;
				}
			}
			if (LocationManaged)
			{
				TargetSensor->SetSensorLocation(OriginalCameraLocation);
				TargetSensor->SetSensorRotation(OriginalCameraRotation);
			}
		}

     	TargetSensor->GetLitCamSensor()->CleanCaptureCache();
      	TargetSensor->GetDepthCamSensor()->CleanCaptureCache();
      	TargetSensor->GetAnnotationCamSensor()->CleanCaptureCache();
      	TargetSensor->GetNormalCamSensor()->CleanCaptureCache();
      	TargetSensor->GetFlowCamSensor()->CleanCaptureCache();

		UE_LOG(LogUnrealCV, Display, TEXT("FusionCamCaptureActor: Stop recording. %d frames recorded. Real Duration: %.2fs, Real FPS: %.2f"),
			ElapsedSteps, RealWorldTimeDurationSeconds, RealWorldTimeFPS);

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

		if (TimeDilationBackUp > 0.0f)
		{
			GetWorld()->GetWorldSettings()->SetTimeDilation(TimeDilationBackUp);
		}
		// GetWorld()->GetWorldSettings()->SetTimeDilation(1.0f);

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

	auto MoveTo = [this] (
		FVector CurrentLocation,
		FVector DesiredLocation,
		FRotator Rotation
	)
	{
		// FVector CurrentLocation = TargetSensor->GetSensorLocation();
		// FVector DesiredLocation = CurrentTrajectory[CurrentTrajectoryIndex].Location;

		// bool Hit = false;
		// FHitResult HitResult;
		// const float CameraRadius = 5.f;
		// const ECollisionChannel TraceChannel = ECC_WorldStatic;
		// // const ECollisionChannel TraceChannel = ECC_WorldStatic |ECC_WorldDynamic;

		// FVector SafeLocation = ULineTraceBPlib::SolveCameraSweepSlide(
		// 	this,                // WorldContextObject
		// 	CurrentLocation,     // Start
		// 	DesiredLocation,     // End
		// 	CameraRadius,
		// 	TraceChannel,
		// 	Hit,
		// 	HitResult
		// );

		// TargetSensor->SetSensorLocation(SafeLocation);
		// if (!Hit)
		// {
		// 	TargetSensor->SetSensorRotation(Rotation);
		// }

		TargetSensor->SetSensorLocation(DesiredLocation);
		TargetSensor->SetSensorRotation(Rotation);
	};

	// if (WarmUpElapsedFrames < WarmUpFrames)
	// {
	// 	if (CurrentTrajectory[CurrentTrajectoryIndex].bManageTransform)
	// 	{
	// 		TargetSensor->SetSensorLocation(CurrentTrajectory[CurrentTrajectoryIndex].Location);
	// 		TargetSensor->SetSensorRotation(CurrentTrajectory[CurrentTrajectoryIndex].Rotation);
	// 	}
	// 	// CurrentTrajectoryIndex++;
	// 	WarmUpElapsedFrames++;
	// 	return;
	// }

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
				// TargetSensor->SetSensorLocation(CurrentTrajectory[CurrentTrajectoryIndex].Location);
				// TargetSensor->SetSensorRotation(CurrentTrajectory[CurrentTrajectoryIndex].Rotation);
				MoveTo(
					TargetSensor->GetSensorLocation(),
					CurrentTrajectory[CurrentTrajectoryIndex].Location,
					CurrentTrajectory[CurrentTrajectoryIndex].Rotation
				);
			}

			RecordFrame();


			if (WarmUpElapsedFrames < WarmUpFrames)
			{
				WarmUpElapsedFrames++;
			}
			else 
			{
				ElapsedSteps++;
				CurrentTrajectoryIndex++;
			}
		}
	}
	else
	{
		if (CurrentTrajectory[CurrentTrajectoryIndex].bManageTransform)
		{
			// TargetSensor->SetSensorLocation(CurrentTrajectory[CurrentTrajectoryIndex].Location);
			// TargetSensor->SetSensorRotation(CurrentTrajectory[CurrentTrajectoryIndex].Rotation);
			MoveTo(
				TargetSensor->GetSensorLocation(),
				CurrentTrajectory[CurrentTrajectoryIndex].Location,
				CurrentTrajectory[CurrentTrajectoryIndex].Rotation
			);
		}

		RecordFrame();

		if (WarmUpElapsedFrames < WarmUpFrames)
		{
			WarmUpElapsedFrames++;
		}
		else 
		{
			ElapsedSteps++;
			CurrentTrajectoryIndex++;
		}
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


	if (CurrentTrajectoryIndex >= CurrentTrajectory.Num())
	{
		UE_LOG(LogUnrealCV, Log, TEXT("FusionCamCaptureActor: Stop recording normally. CurrentTrajectoryIndex = %d, NumFrames = %d"), CurrentTrajectoryIndex, NumFrames);
		StopRecord();
		return;
	}
}

void AFusionCamCaptureActor::RecordFrame()
{
	FScopeLock Lock(&RecordCriticalSection);

	if (bRecordRGB)
	{
		FString FileNameRGB = MakeFilenameNew("rgb", ".png");
		TargetSensor->SaveLitToFile(FileNameRGB);
	}

	if (bRecordMask)
	{
		FString FileNameMask = MakeFilenameNew("mask", ".png");
		TargetSensor->SaveSegToFile(FileNameMask);
	}

	if (bRecordDepth)
	{
		FString DepthFilename = MakeFilenameNew("depth", ".npy");
		TargetSensor->SaveDepthToFile(DepthFilename);
	}

	if (bRecordNormal)
	{
		FString NormalFilename = MakeFilenameNew("normal", ".png");
		TargetSensor->SaveNormalToFile(NormalFilename);
	}

	if (bRecordFlow)
	{
		FString FlowFilename = MakeFilenameNew("flow", ".png");
		TargetSensor->SaveFlowToFile(FlowFilename);
	}

	if (bRecordOneObjectMask && IsValid(TargetToHide))
	{
		FString OneObjFilename = MakeFilenameNew("oneobjmask", ".png");
		TargetSensor->SaveOneObjMaskToFile(TargetToHide, OneObjFilename);
	}

	if (bRecordWithoutTarget && IsValid(TargetToHide))
	{
		TargetToHide->SetActorHiddenInGame(true);

		if (bRecordRGB)
		{
			FString FileNameRGB = MakeFilenameNew("rgb_woTarget", ".png");
			TargetSensor->SaveLitToFile(FileNameRGB);
		}

		if (bRecordMask)
		{
			FString FileNameMask = MakeFilenameNew("mask_woTarget", ".png");
			TargetSensor->SaveSegToFile(FileNameMask);
		}

		if (bRecordDepth)
		{
			FString DepthFilename = MakeFilenameNew("depth_woTarget", ".npy");
			TargetSensor->SaveDepthToFile(DepthFilename);
		}

		if (bRecordNormal)
		{
			FString NormalFilename = MakeFilenameNew("normal_woTarget", ".png");
			TargetSensor->SaveNormalToFile(NormalFilename);
		}

		if (bRecordFlow)
		{
			FString FlowFilename = MakeFilenameNew("flow_woTarget", ".png");
			TargetSensor->SaveFlowToFile(FlowFilename);
		}

		TargetToHide->SetActorHiddenInGame(false);
	}

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
	FString FileBaseName = FString::Printf(TEXT("%d_%s.%s"), ElapsedSteps, *DataType, *FileExtension);
	// FString FileFolder = FString::Printf(TEXT("%d_%s.%s"), ElapsedSteps, *DataType, *FileExtension);
	FileName = FPaths::Combine(FileName, FileBaseName);
	// Combine with output folder
	FileName = FPaths::ConvertRelativePathToFull(FinalDataFolder, FileName);

	return FileName;
}

FString AFusionCamCaptureActor::MakeFilenameNewWithFolder(FString DataType, FString FileExtension)
{
	// Find the position to insert frame number
	

	if (FileExtension.StartsWith(".")) {
		FileExtension.RemoveAt(0);
	}

	// Create filename with frame number and data type
	FString FileName = RecordFileName;
	FString FileBaseName = FString::Printf(TEXT("%d_%s.%s"), ElapsedSteps, *DataType, *FileExtension);
	FString FileFolder = FString::Printf(TEXT("%s"), *DataType);
	FileName = FPaths::Combine(FileName, FileFolder);
	FileName = FPaths::Combine(FileName, FileBaseName);
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
	FMatrix RotationMatrix = FRotationMatrix::Make(Rotation);

	float ExposureSpeedDown = 0.0f, ExposureSpeedUp = 0.0f;
	TargetSensor->GetAutoExposureSpeed(ExposureSpeedDown, ExposureSpeedUp);

	float MotionBlurAmount = 0.0f, MotionBlurMax = 0.0f, MotionBlurPerObjectSize = 0.0f;
	int MotionBlurTargetFPS = 0;
	TargetSensor->GetMotionBlurParams(MotionBlurAmount, MotionBlurMax, MotionBlurPerObjectSize, MotionBlurTargetFPS);

	float FocalDistance = 0.0f, FocalRegion = 0.0f;
	TargetSensor->GetFocalParams(FocalDistance, FocalRegion);

	float ChromaticAberration = TargetSensor->GetChromaticAberration();
	float Vignette = TargetSensor->GetVignetteIntensity();

	EBloomMethod BloomMethod = EBloomMethod::BM_FFT;
	float BloomIntensity = 0.0f;
	TargetSensor->GetBloomParams(BloomMethod, BloomIntensity);

	// Calculate focal length in pixels from FOV
	float Focal = FMath::Max(Width, Height) / 2.0f / FMath::Tan(FOVRadians / 2.0f);		
	float fx = Focal;
	float fy = Focal;
	float cx = Width / 2.0f;
	float cy = Height / 2.0f;
	TArray K{
		USerializeBPLib::ArrayToJson({FJsonObjectBP(fx)   , FJsonObjectBP(0.0f) , FJsonObjectBP(cx)}), 
		USerializeBPLib::ArrayToJson({FJsonObjectBP(0.0f) , FJsonObjectBP(fy)   , FJsonObjectBP(cy)}), 
		USerializeBPLib::ArrayToJson({FJsonObjectBP(0.0f) , FJsonObjectBP(0.0f) , FJsonObjectBP(1.0f)})
	};

	auto ReflectionMethodToString = [](EReflectionMethod::Type Method) -> FString {
		switch (Method) {
			case EReflectionMethod::None: return TEXT("None");
			case EReflectionMethod::Lumen: return TEXT("Lumen");
			case EReflectionMethod::ScreenSpace: return TEXT("ScreenSpace");
			default: return TEXT("Unknown");
		}
	};

	auto GIMethodToString = [](EDynamicGlobalIlluminationMethod::Type Method) -> FString {
		switch (Method) {
			case EDynamicGlobalIlluminationMethod::None: return TEXT("None");
			case EDynamicGlobalIlluminationMethod::Lumen: return TEXT("Lumen");
			case EDynamicGlobalIlluminationMethod::ScreenSpace: return TEXT("ScreenSpace");
			default: return TEXT("Unknown");
		}
	};

	auto ExposureMethodToString = [](EAutoExposureMethod Method) -> FString {
		switch (Method) {
			case EAutoExposureMethod::AEM_Manual: return TEXT("Manual");
			case EAutoExposureMethod::AEM_Histogram: return TEXT("Histogram");
			default: return TEXT("Unknown");
		}
	};

	auto BloomMethodToString = [](EBloomMethod Method) -> FString {
		switch (Method) {
			case EBloomMethod::BM_FFT: return TEXT("FFT");
			case EBloomMethod::BM_SOG: return TEXT("SOG");
			default: return TEXT("Unknown");
		}
	};

	TMap<FString, FString> CameraSettingsStringMap;
	CameraSettingsStringMap.Add("ReflectionMethod", ReflectionMethodToString(TargetSensor->GetReflectionMethod()));
	CameraSettingsStringMap.Add("GlobalIlluminationMethod", GIMethodToString(TargetSensor->GetGlobalIlluminationMethod()));
	CameraSettingsStringMap.Add("ExposureMethod", ExposureMethodToString(TargetSensor->GetExposureMethod()));
	CameraSettingsStringMap.Add("BloomMethod", BloomMethodToString(BloomMethod));

	TMap<FString, float> CameraSettingsMap;
	CameraSettingsMap.Add("FieldOfView", FOV);
	CameraSettingsMap.Add("ImageWidth", static_cast<float>(Width));
	CameraSettingsMap.Add("ImageHeight", static_cast<float>(Height));
	CameraSettingsMap.Add("AutoExposureSpeedDown", ExposureSpeedDown);
	CameraSettingsMap.Add("AutoExposureSpeedUp", ExposureSpeedUp);
	CameraSettingsMap.Add("MotionBlurAmount", MotionBlurAmount);
	CameraSettingsMap.Add("MotionBlurMax", MotionBlurMax);
	CameraSettingsMap.Add("MotionBlurPerObjectSize", MotionBlurPerObjectSize);
	CameraSettingsMap.Add("MotionBlurTargetFPS", static_cast<float>(MotionBlurTargetFPS));
	CameraSettingsMap.Add("DepthOfFieldFocalDistance", FocalDistance);
	CameraSettingsMap.Add("DepthOfFieldFocalRegion", FocalRegion);
	CameraSettingsMap.Add("ChromaticAberrationIntensity", ChromaticAberration);
	CameraSettingsMap.Add("VignetteIntensity", Vignette);
	CameraSettingsMap.Add("BloomIntensity", BloomIntensity);

	TArray RotationArray = {
		USerializeBPLib::ArrayToJson({static_cast<float>(RotationMatrix.M[0][0]), static_cast<float>(RotationMatrix.M[0][1]), static_cast<float>(RotationMatrix.M[0][2])}),
		USerializeBPLib::ArrayToJson({static_cast<float>(RotationMatrix.M[1][0]), static_cast<float>(RotationMatrix.M[1][1]), static_cast<float>(RotationMatrix.M[1][2])}),
		USerializeBPLib::ArrayToJson({static_cast<float>(RotationMatrix.M[2][0]), static_cast<float>(RotationMatrix.M[2][1]), static_cast<float>(RotationMatrix.M[2][2])})
	};

	auto TranslationArray = USerializeBPLib::VectorToJson({Location.X, Location.Y, Location.Z});

	// Calculate w2c_colmap matrix: COLMAP world-to-camera transformation
	// Step 1: Build 4x4 c2w_unreal (camera-to-world in Unreal coordinates)
	FMatrix c2w_unreal = FMatrix::Identity;
	c2w_unreal.M[0][0] = RotationMatrix.M[0][0]; c2w_unreal.M[0][1] = RotationMatrix.M[0][1]; c2w_unreal.M[0][2] = RotationMatrix.M[0][2]; c2w_unreal.M[0][3] = Location.X;
	c2w_unreal.M[1][0] = RotationMatrix.M[1][0]; c2w_unreal.M[1][1] = RotationMatrix.M[1][1]; c2w_unreal.M[1][2] = RotationMatrix.M[1][2]; c2w_unreal.M[1][3] = Location.Y;
	c2w_unreal.M[2][0] = RotationMatrix.M[2][0]; c2w_unreal.M[2][1] = RotationMatrix.M[2][1]; c2w_unreal.M[2][2] = RotationMatrix.M[2][2]; c2w_unreal.M[2][3] = Location.Z;
	// M[3][0-3] already [0, 0, 0, 1] from Identity

	// Step 2: Invert to get w2c_unreal (world-to-camera in Unreal coordinates)
	FMatrix w2c_unreal = c2w_unreal.Inverse();

	// Step 3: Create coordinate system transformation matrices
	// T_cam_unreal_to_colmap: Y↔X swap, Z→-Z flip (camera frame conversion)
	FMatrix T_cam_unreal_to_colmap = FMatrix::Identity;
	T_cam_unreal_to_colmap.M[0][0] = 0; T_cam_unreal_to_colmap.M[0][1] = 1;  // X ← Y
	T_cam_unreal_to_colmap.M[1][0] = 1; T_cam_unreal_to_colmap.M[1][1] = 0;  // Y ← X
	T_cam_unreal_to_colmap.M[2][2] = -1;  // Z ← -Z

	// T_world_colmap_to_unreal: Y→-Y flip (world frame conversion)
	FMatrix T_world_colmap_to_unreal = FMatrix::Identity;
	T_world_colmap_to_unreal.M[1][1] = -1;  // Y ← -Y

	// Step 4: Compute final COLMAP w2c matrix
	FMatrix w2c_colmap = T_cam_unreal_to_colmap * w2c_unreal * T_world_colmap_to_unreal;

	// Step 5: Convert 4x4 matrix to JSON array (array of 4 rows, each as 3-element vector)
	TArray<FJsonObjectBP> W2CColmapArray;
	for (int32 i = 0; i < 4; ++i) {
		FJsonObjectBP RowJson = USerializeBPLib::ArrayToJson({
			FJsonObjectBP(static_cast<float>(w2c_colmap.M[i][0])),
			FJsonObjectBP(static_cast<float>(w2c_colmap.M[i][1])),
			FJsonObjectBP(static_cast<float>(w2c_colmap.M[i][2]))
		});
		W2CColmapArray.Add(RowJson);
	}

	TArray<FString> ExtrinsicsKeys = {"RotationMatrix", "Translation", "w2c_colmap"};
	TArray<FJsonObjectBP> ExtrinsicsValues;
	ExtrinsicsValues.Add(FJsonObjectBP(RotationArray));
	ExtrinsicsValues.Add(TranslationArray);
	ExtrinsicsValues.Add(USerializeBPLib::ArrayToJson(W2CColmapArray));

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

	TMap<FString, FColor> AllAnnotationColors;
	FColor AnnotationColor;
	check(IsValid(SceneHandle.ForegroundActor))
	AUnrealcvWorldController* WorldController = FUnrealcvServer::Get().WorldController.Get();
	check(IsValid(WorldController))
	FObjectAnnotator::GetAnnotationColor(SceneHandle.ForegroundActor, AnnotationColor);
	AllAnnotationColors = FObjectAnnotator::GetAnnotationColors();


	FString ForegroundColor = FString::Printf(TEXT("%d,%d,%d"), AnnotationColor.R, AnnotationColor.G, AnnotationColor.B);
	TMap<FString, FString> ColorMap;
	for (const TPair<FString, FColor>& KV : AllAnnotationColors)
	{
		FString ColorJson = FString::Printf(TEXT("%d,%d,%d"), KV.Value.R, KV.Value.G, KV.Value.B);
		ColorMap.Add(KV.Key, ColorJson);
	}


	TArray<FString> Keys = {
		"FrameNumber",
		"VideoName",
		"Resolution",
		"Width",
		"Height",
		"FrameCount",
		"RecordedFPS",
		"FOV",
		"SceneCategory",
		"ForegroundCategory",
		"ForegroundSubcategory",
		"ForegroundObjectMetadata",
		"ForegroundLocation",
		"ForegroundRotation",
		"OccluderMetaDataList",
		"OcclusionRatio",
		"CameraLocation",
		"CameraRotation",
		"CameraSettings",
		"CameraSettingsString",
		"IntrinsicsMatrix",
		"Extrinsics",
		"ForegroundColor",
		"AnnotationColors",
		"RealWorldTimeRecordingStart",
		"RealWorldTimeRecordingEnd",
		"RealWorldTimeDurationSeconds",
		"RealWorldTimeFPS"
	};

	RealWorldTimeRecordingEnd = FDateTime::Now();
	RealWorldTimeDurationSeconds = (RealWorldTimeRecordingEnd - RealWorldTimeRecordingStart).GetTotalSeconds();
	if (RealWorldTimeDurationSeconds > 0.0)
	{
		RealWorldTimeFPS = ElapsedSteps / RealWorldTimeDurationSeconds;
	}
	else
	{
		RealWorldTimeFPS = 0.0;
	}
	FString ResolutionStr = FString::Printf(TEXT("%dx%d"), Width, Height);
	FString RealWorldTimeStartStr = RealWorldTimeRecordingStart.ToString(TEXT("%Y-%m-%d %H:%M:%S"));
	FString RealWorldTimeEndStr = RealWorldTimeRecordingEnd.ToString(TEXT("%Y-%m-%d %H:%M:%S"));

	FVector ForegroundLocation = FVector::Zero();
	FRotator ForegroundRotation = FRotator::ZeroRotator;
	if (IsValid(SceneHandle.ForegroundActor))
	{
		ForegroundLocation = SceneHandle.ForegroundActor->GetActorLocation();
		ForegroundRotation = SceneHandle.ForegroundActor->GetActorRotation();
	}

	TArray<FJsonObjectBP> Values = {
		FJsonObjectBP(NumFrames),
		FJsonObjectBP(RecordFileName),
		FJsonObjectBP(ResolutionStr),
		FJsonObjectBP(Width),
		FJsonObjectBP(Height),
		FJsonObjectBP(ElapsedSteps),
		FJsonObjectBP(RecordFPS),
		FJsonObjectBP(FOV),
		FJsonObjectBP(SceneHandle.SceneCategory),
		FJsonObjectBP(SceneHandle.ForegroundCategory),
		FJsonObjectBP(SceneHandle.ForegroundSubcategory),
		FJsonObjectBP(SceneHandle.ForegroundObjectMetadata),
		FJsonObjectBP(ForegroundLocation),
		FJsonObjectBP(ForegroundRotation),
		FJsonObjectBP(OccluderArray),
		FJsonObjectBP(SceneHandle.OcclusionRatio),
		FJsonObjectBP(Location),
		FJsonObjectBP(Rotation),
		FJsonObjectBP(CameraSettingsMap),
		FJsonObjectBP(CameraSettingsStringMap),
		K,
		FJsonObjectBP(ExtrinsicsKeys, ExtrinsicsValues),
		FJsonObjectBP(ForegroundColor),
		FJsonObjectBP(ColorMap),
		FJsonObjectBP(RealWorldTimeStartStr),
		FJsonObjectBP(RealWorldTimeEndStr),
		FJsonObjectBP(static_cast<float>(RealWorldTimeDurationSeconds)),
		FJsonObjectBP(static_cast<float>(RealWorldTimeFPS))
	};

	FJsonObjectBP JsonObject = USerializeBPLib::TMapToJson(Keys, Values);
	FString JsonStr = USerializeBPLib::JsonToStr(JsonObject);
	FString JsonFilename = MakeFilenameNewWithFolder("metadata", ".json");

    AsyncTask(ENamedThreads::AnyThread, [JsonStr, JsonFilename]()
	{
		UVisionBPLib::SaveData(JsonStr, JsonFilename);
	});
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////// Neo Trajector Render System /////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// ========== Camera Trajectory Recording Implementation ==========

void AFusionCamCaptureActor::PrepareTrajectoryRecord(AActor * Target, float FPS)
{
	SetDefaultParamsForTargetCamera();
	TargetSensor->SetSensorFOV(FMath::RandRange(40.0f, 55.0f));
	// TargetSensor->SetMotionBlurParams(0.5f, 50.0f, 50.0f, static_cast<float>(FPS));
	// calculate the range from TargetSensor to Target
	UnifiedTargetLocation = GetTargetLocationWithRandomHeight(Target);
	FVector SensorLocation = TargetSensor->GetSensorLocation();
	float Distance = (UnifiedTargetLocation - SensorLocation).Size();
	const float FocalRegion = 1 * 100;
	TargetSensor->SetFocalParams(FMath::Max(Distance - FocalRegion/2, 100.0f), FocalRegion);

	UE_LOG(LogUnrealCV, Log, TEXT("FusionCamCaptureActor: Set all quality settings to maximum for recording (Lumen GI and Reflections enabled)"));



	static const TArray<FIntPoint> Resolutions = {
		FIntPoint(1920, 1080),
		// FIntPoint(640, 480),
		// FIntPoint(480, 640),
	};
	const FIntPoint& ChosenRes = Resolutions[FMath::RandRange(0, Resolutions.Num() - 1)];


	// TargetSensor->GetDepthCamSensor()->bIgnoreTransparentObjects = true;
	TargetSensor->SetFilmSize(ChosenRes.X, ChosenRes.Y);
	
	// Adjust camera to roughly aim at the target with ±15 degrees noise
	FVector CameraToTarget = (UnifiedTargetLocation - TargetSensor->GetSensorLocation()).GetSafeNormal();
	FRotator TargetRotation = CameraToTarget.Rotation();

	// Add ±15 degrees noise to pitch, yaw, and roll
	float NoisePitch = FMath::RandRange(-4.0f, 4.0f);
	float NoiseYaw = FMath::RandRange(-1.0f, 1.0f);
	float NoiseRoll = FMath::RandRange(-4.0f, 4.0f);

	FRotator NoisyRotation = TargetRotation + FRotator(NoisePitch, NoiseYaw, NoiseRoll);
	TargetSensor->SetSensorRotation(NoisyRotation);
}

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
	
	if ((UnifiedTargetLocation - Target->GetActorLocation()).Length() > 1000)
	{
		UnifiedTargetLocation = GetTargetLocationWithRandomHeight(Target);
		if (TrajectoryType != ECameraTrajectoryType::RenderOnly && TrajectoryType != ECameraTrajectoryType::RenderOnly5S)
			UE_LOG(LogUnrealCV, Warning, TEXT("Warning, (UnifiedTargetLocation - Target->GetActorLocation()).Length() > 1000, likely not prepared, call PrepareTrajectoryRecord first"));
	}

	float DegreesPerFrame = DegreesPerSecond / FPS;

	RecordFileName = FileName;
	RecordFPS = FPS;
	ElapsedSteps = 0;
	bIsRecording = true;
	TargetToHide = Target;
	bPauseWorldDuringRecord = bPauseWorldTime;
	WarmUpElapsedFrames = 0;
	WarmUpFrames = WARM_UP_FRAMES;

	RealWorldTimeRecordingStart = FDateTime::Now();

	OriginalCameraLocation = TargetSensor->GetSensorLocation();
	OriginalCameraRotation = TargetSensor->GetSensorRotation();

	CurrentTrajectory = CalculateTrajectory(TrajectoryType, Target, DegreesPerFrame, RandomSeed);

	if (bRecordAudio)
	{
		StartAudioRecord();
	}

	RenderTrajectory(CurrentTrajectory, bPauseWorldTime);
}

void AFusionCamCaptureActor::StartSimpleRecording(const FString& FileName, int32 FPS, float DurationSeconds)
{
	if (!IsValid(TargetSensor))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("StartSimpleRecording: TargetSensor is not set!"));
		return;
	}

	if (FPS <= 0 || DurationSeconds <= 0.0f)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("StartSimpleRecording: Invalid FPS or Duration (FPS=%d, Duration=%.2f)"), FPS, DurationSeconds);
		return;
	}

	if (bIsRecording)
	{
		StopRecord();
	}

	int32 TotalFrames = FMath::CeilToInt(FPS * DurationSeconds);

	TArray<FCameraPose> SimpleTrajectory;
	SimpleTrajectory.Reserve(TotalFrames);

	FVector CurrentLocation = TargetSensor->GetSensorLocation();
	FRotator CurrentRotation = TargetSensor->GetSensorRotation();

	for (int32 i = 0; i < TotalFrames; i++)
	{
		FCameraPose Pose;
		Pose.Location = CurrentLocation;
		Pose.Rotation = CurrentRotation;
		Pose.bManageTransform = false;
		Pose.DesiredEstTimeDilation = 1.0f;
		SimpleTrajectory.Add(Pose);
	}

	RecordFileName = FileName;
	RecordFPS = FPS;
	ElapsedSteps = 0;
	bIsRecording = true;
	TargetToHide = nullptr;
	bPauseWorldDuringRecord = false;
	WarmUpElapsedFrames = 0;
	WarmUpFrames = 0;
	NumFrames = TotalFrames;

	RealWorldTimeRecordingStart = FDateTime::Now();

	CurrentTrajectory = SimpleTrajectory;

	UE_LOG(LogUnrealCV, Log, TEXT("StartSimpleRecording: FileName=%s, FPS=%d, Duration=%.2fs, TotalFrames=%d"),
		*FileName, FPS, DurationSeconds, TotalFrames);

	bool bPauseWorldTime = false;
	RenderTrajectory(CurrentTrajectory, bPauseWorldTime);
}

void AFusionCamCaptureActor::SetDefaultParamsForTargetCamera()
{
	if (!IsValid(TargetSensor))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("FusionCamCaptureActor: TargetSensor is not set!"));
		return;
	}


	static auto CVarForceLOD = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ForceLOD"));
	// static auto CVarViewDistanceScale = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ViewDistanceScale"));
	// static auto CVarShadowQuality = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ShadowQuality"));
	// static auto CVarPostProcessQuality = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PostProcessQuality"));
	// static auto CVarTextureQuality = IConsoleManager::Get().FindConsoleVariable(TEXT("r.TextureQuality"));
	// static auto CVarEffectsQuality = IConsoleManager::Get().FindConsoleVariable(TEXT("r.EffectsQuality"));
	// static auto CVarFoliageQuality = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FoliageQuality"));
	// static auto CVarShadingQuality = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ShadingQuality"));
	// static auto CVarAntiAliasingQuality = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AntiAliasingQuality"));
	// static auto CVarMotionBlurQuality = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MotionBlurQuality"));
	// static auto CVarAmbientOcclusionLevels = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AmbientOcclusionLevels"));
	// static auto CVarSSRQuality = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SSR.Quality"));
	// static auto CVarBloomQuality = IConsoleManager::Get().FindConsoleVariable(TEXT("r.BloomQuality"));
	// static auto CVarDepthOfFieldQuality = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DepthOfFieldQuality"));
	// static auto CVarLightShaftQuality = IConsoleManager::Get().FindConsoleVariable(TEXT("r.LightShaftQuality"));
	// static auto CVarRefractionQuality = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RefractionQuality"));
	// static auto CVarTranslucencyLightingVolume = IConsoleManager::Get().FindConsoleVariable(TEXT("r.TranslucencyLightingVolume"));
	// static auto CVarMaxAnisotropy = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MaxAnisotropy"));
	// static auto CVarDynamicGlobalIlluminationMethod = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DynamicGlobalIlluminationMethod"));
	// static auto CVarLumenReflectionsAllow = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.Reflections.Allow"));

	if (CVarForceLOD) { CVarForceLOD->Set(0); }
	// if (CVarViewDistanceScale) { CVarViewDistanceScale->Set(1.0f); }
	// if (CVarShadowQuality) { CVarShadowQuality->Set(5); }
	// if (CVarPostProcessQuality) { CVarPostProcessQuality->Set(5); }
	// if (CVarTextureQuality) { CVarTextureQuality->Set(5); }
	// if (CVarEffectsQuality) { CVarEffectsQuality->Set(5); }
	// if (CVarFoliageQuality) { CVarFoliageQuality->Set(5); }
	// if (CVarShadingQuality) { CVarShadingQuality->Set(5); }
	// if (CVarAntiAliasingQuality) { CVarAntiAliasingQuality->Set(5); }
	// if (CVarMotionBlurQuality) { CVarMotionBlurQuality->Set(4); }
	// if (CVarAmbientOcclusionLevels) { CVarAmbientOcclusionLevels->Set(3); }
	// if (CVarSSRQuality) { CVarSSRQuality->Set(4); }
	// if (CVarBloomQuality) { CVarBloomQuality->Set(5); }
	// if (CVarDepthOfFieldQuality) { CVarDepthOfFieldQuality->Set(4); }
	// if (CVarLightShaftQuality) { CVarLightShaftQuality->Set(1); }
	// if (CVarRefractionQuality) { CVarRefractionQuality->Set(2); }
	// if (CVarTranslucencyLightingVolume) { CVarTranslucencyLightingVolume->Set(1); }
	// if (CVarMaxAnisotropy) { CVarMaxAnisotropy->Set(16); }
	// if (CVarDynamicGlobalIlluminationMethod) { CVarDynamicGlobalIlluminationMethod->Set(1); }
	// if (CVarLumenReflectionsAllow) { CVarLumenReflectionsAllow->Set(1); }


	TargetSensor->SetReflectionMethod(EReflectionMethod::Type::Lumen);
    TargetSensor->SetGlobalIlluminationMethod(EDynamicGlobalIlluminationMethod::Type::Lumen);

    // TargetSensor->SetExposureMethod(EAutoExposureMethod::AEM_Histogram);
    TargetSensor->SetExposureMethod(EAutoExposureMethod::AEM_Basic);
    // TargetSensor->SetExposureMethod(EAutoExposureMethod::AEM_Manual);
	// TargetSensor->SetAutoExposureSpeed(0.5f, 0.5f); 
	TargetSensor->SetAutoExposureSpeed(5.f, 8.f); 
	// TargetSensor->SetExposureBias(0.0f);
	
	TargetSensor->SetProjectionType(ECameraProjectionMode::Type::Perspective);

	// TargetSensor->SetMotionBlurParams(0.5f, 50.0f, 50.0f, 24.0f);

	// TargetSensor->SetSensorFOV(FMath::RandRange(40.0f, 55.0f));
	TargetSensor->SetSensorFOV(55.0f);

	TargetSensor->SetFocalParams(500.0f, 50.0f);

	TargetSensor->SetChromaticAberration(0.5f);

	TargetSensor->SetConvolutionBloom(EBloomMethod::BM_FFT, nullptr, 1.5f);
	// TargetSensor->SetConvolutionBloom(EBloomMethod::BM_SOG, nullptr, 1.5f);

	TargetSensor->SetVignetteIntensity(0.1f);

	TargetSensor->GetLitCamSensor()->CleanCaptureCache();
	TargetSensor->GetDepthCamSensor()->CleanCaptureCache();
	TargetSensor->GetAnnotationCamSensor()->CleanCaptureCache();
	TargetSensor->GetNormalCamSensor()->CleanCaptureCache();
	TargetSensor->GetFlowCamSensor()->CleanCaptureCache();
}


TArray<AFusionCamCaptureActor::FCameraPose> AFusionCamCaptureActor::CalculateTrajectory(ECameraTrajectoryType TrajectoryType, AActor* Target, float DegreesPerFrame, int32 RandomSeed)
{
	switch (TrajectoryType)
	{
	case ECameraTrajectoryType::RotateLeft45:
		return CalculateRotateLeft(Target, DegreesPerFrame, 45.0f);
	case ECameraTrajectoryType::RotateLeft30:
		return CalculateRotateLeft(Target, DegreesPerFrame, 30.0f);
	case ECameraTrajectoryType::RotateRight45:
		return CalculateRotateRight(Target, DegreesPerFrame, -45.0f);
	case ECameraTrajectoryType::RotateRight30:
		return CalculateRotateRight(Target, DegreesPerFrame, -30.0f);
	case ECameraTrajectoryType::RotateUp45:
		return CalculateRotateUp(Target, DegreesPerFrame, 45.0f);
	case ECameraTrajectoryType::RotateUp30:
		return CalculateRotateUp(Target, DegreesPerFrame, 30.0f);
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
		// return CalculateRenderOnly(10.0);
	case ECameraTrajectoryType::RenderOnly5S:
		return CalculateRenderOnly(5.0);
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

FVector AFusionCamCaptureActor::GetTargetLocationWithRandomHeight(AActor* Target) const
{
	FVector TargetLocation = Target->GetActorLocation();
	float RandomHeight = FMath::RandRange(155.0f, 175.0f);
	TargetLocation.Z += RandomHeight;
	return TargetLocation;
}

TArray<AFusionCamCaptureActor::FCameraPose> AFusionCamCaptureActor::AddRotateBufferFrames(const TArray<FCameraPose>& CoreTrajectory)
{
	if (CoreTrajectory.Num() == 0)
	{
		return CoreTrajectory;
	}

	int32 BufferFrames = FMath::RoundToInt(ROTATE_BUFFER_DURATION_SECONDS * RecordFPS);
	if (BufferFrames <= 0)
	{
		return CoreTrajectory;
	}

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

TArray<AFusionCamCaptureActor::FCameraPose> AFusionCamCaptureActor::CalculateRotateLeft(AActor* Target, float DegreesPerFrame, float TotalRotationDeg)
{
	TArray<FCameraPose> CoreTrajectory;
	FVector TargetLocation = UnifiedTargetLocation;
	FVector OriginalLocation = TargetSensor->GetSensorLocation();
	FRotator OriginalRotation = TargetSensor->GetSensorRotation();

	FVector Offset = OriginalLocation - TargetLocation;
	FRotator InitRotation = OriginalRotation - (TargetLocation - OriginalLocation).Rotation();

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


TArray<AFusionCamCaptureActor::FCameraPose> AFusionCamCaptureActor::CalculateRotateRight(AActor* Target, float DegreesPerFrame, float TotalRotationDeg)
{
	TArray<FCameraPose> CoreTrajectory;
	FVector TargetLocation = UnifiedTargetLocation;
	FVector OriginalLocation = TargetSensor->GetSensorLocation();
	FRotator OriginalRotation = TargetSensor->GetSensorRotation();

	FVector Offset = OriginalLocation - TargetLocation;
	FRotator InitRotation = OriginalRotation - (TargetLocation - OriginalLocation).Rotation();

	NumFrames = FMath::CeilToInt(FMath::Abs(TotalRotationDeg) / DegreesPerFrame) + 1;
	if (ROTATE_NUM_FRAMES_OVERRIDE > 0)
	{
		NumFrames = ROTATE_NUM_FRAMES_OVERRIDE;
		DegreesPerFrame = FMath::Abs(TotalRotationDeg) / (NumFrames - 1);
	}

	for (int i = 0; i < NumFrames; i++)
	{
		float CurrentDeg = FMath::Max(DegreesPerFrame * i * -1.0, TotalRotationDeg);

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

TArray<AFusionCamCaptureActor::FCameraPose> AFusionCamCaptureActor::CalculateRotateUp(AActor* Target, float DegreesPerFrame, float TotalRotationDeg)
{
	TArray<FCameraPose> CoreTrajectory;
	FVector TargetLocation = UnifiedTargetLocation;
	FVector OriginalLocation = TargetSensor->GetSensorLocation();
	FRotator OriginalRotation = TargetSensor->GetSensorRotation();

	FVector Offset = OriginalLocation - TargetLocation;
	FRotator InitRotation = OriginalRotation - (TargetLocation - OriginalLocation).Rotation();

	FVector ToCamera = Offset.GetSafeNormal();
	FVector RightVector = FVector::CrossProduct(ToCamera, FVector::UpVector).GetSafeNormal();

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
	FVector TargetLocation = UnifiedTargetLocation;
	FVector OriginalLocation = TargetSensor->GetSensorLocation();
	FRotator OriginalRotation = TargetSensor->GetSensorRotation();

	FVector Offset = OriginalLocation - TargetLocation;
	FRotator InitRotation = OriginalRotation - (TargetLocation - OriginalLocation).Rotation();

	float TotalRotationDeg = 360.0f;
	NumFrames = FMath::CeilToInt(TotalRotationDeg / DegreesPerFrame) + 1;
	if (ROTATE_NUM_FRAMES_OVERRIDE > 0)
	{
		NumFrames = ROTATE_NUM_FRAMES_OVERRIDE;
		// DegreesPerFrame = TotalRotationDeg / (NumFrames - 1);
		DegreesPerFrame = TotalRotationDeg / (NumFrames - 2);
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
	FVector TargetLocation = UnifiedTargetLocation;
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
		// FRotator NewRotation = (TargetLocation - NewLocation).Rotation();

		FCameraPose Pose;
		Pose.Location = NewLocation;
		Pose.Rotation = OriginalRotation;
		Pose.DesiredEstTimeDilation = 0.0f;
		Trajectory.Add(Pose);
	}

	return AddRotateBufferFrames(Trajectory);
}

TArray<AFusionCamCaptureActor::FCameraPose> AFusionCamCaptureActor::CalculateZoomOut(AActor* Target, float DegreesPerFrame)
{
	TArray<FCameraPose> Trajectory;
	FVector TargetLocation = UnifiedTargetLocation;
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
		// FRotator NewRotation = (TargetLocation - NewLocation).Rotation();

		FCameraPose Pose;
		Pose.Location = NewLocation;
		Pose.Rotation = OriginalRotation;
		Pose.DesiredEstTimeDilation = 0.0f;
		Trajectory.Add(Pose);
	}

	return AddRotateBufferFrames(Trajectory);
}

TArray<AFusionCamCaptureActor::FCameraPose> AFusionCamCaptureActor::CalculateRandomDirection(AActor* Target, float DegreesPerFrame, int32 RandomSeed)
{
	TArray<FCameraPose> Trajectory;
	FVector TargetLocation = UnifiedTargetLocation;
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

	float VerticalTiltStrength = RandomStream.FRandRange(-0.08f, 0.4f);
	int HoriParam = RandomStream.FRandRange(-1, 1) > 0 ? 1 : -1;

	float TotalRotationDeg = RandomStream.FRandRange(15.0f, 45.0f);
	NumFrames = FMath::CeilToInt(TotalRotationDeg / DegreesPerFrame) + 1;
	if (ROTATE_NUM_FRAMES_OVERRIDE > 0)
	{
		NumFrames = ROTATE_NUM_FRAMES_OVERRIDE;
		DegreesPerFrame = TotalRotationDeg / (NumFrames - 1);
	}

	bool bVaryDistance = false;
	float OriginalDistance = Offset.Size();
	float DistanceVariation = RandomStream.FRandRange(0.7f, 1.3f);

	for (int i = 0; i < NumFrames; i++)
	{
		float Alpha = static_cast<float>(i) / (NumFrames - 1);
		float CurrentDeg = FMath::Min(DegreesPerFrame * i, TotalRotationDeg);

		// FQuat RotationQuat = FQuat(RandomAxis, FMath::DegreesToRadians(CurrentDeg));
		// FVector NewOffset = RotationQuat.RotateVector(Offset);

		FVector ToCamera = Offset.GetSafeNormal();
		FVector RightVector = FVector::CrossProduct(ToCamera, FVector::UpVector).GetSafeNormal();
		FQuat RotationQuatHori = FQuat(FVector::UpVector, FMath::DegreesToRadians(CurrentDeg * HoriParam));
		float PitchDeg = CurrentDeg * VerticalTiltStrength;
		FQuat RotationQuatUp = FQuat(RightVector, FMath::DegreesToRadians(PitchDeg));
		FQuat FinalQuat = RotationQuatUp * RotationQuatHori;
		FVector NewOffset = FinalQuat.RotateVector(Offset);

		if (bVaryDistance)
		{
			float CurrentDistance = FMath::Lerp(OriginalDistance, OriginalDistance * DistanceVariation, Alpha);
			NewOffset = NewOffset.GetSafeNormal() * CurrentDistance;
		}

		// float NewHeight = 25.;
		FVector NewLocation = TargetLocation + NewOffset;
		// if (NewLocation.Z < NewHeight) 
		// {
		// 	NewLocation.Z = NewHeight;
		// }
		FRotator NewRotation = (TargetLocation - NewLocation).Rotation() + InitRotation;

		FCameraPose Pose;
		Pose.Location = NewLocation;
		Pose.Rotation = NewRotation;
		Pose.DesiredEstTimeDilation = 0.0f;
		Trajectory.Add(Pose);
	}

	return AddRotateBufferFrames(Trajectory);
}

TArray<AFusionCamCaptureActor::FCameraPose> AFusionCamCaptureActor::CalculateRenderOnly(float Time)
{
	TArray<FCameraPose> Trajectory;
	NumFrames = RecordFPS * Time;
	if (ROTATE_NUM_FRAMES_OVERRIDE > 0)
	{
		NumFrames = ROTATE_NUM_FRAMES_OVERRIDE;
	}

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
