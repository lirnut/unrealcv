// shc @ 2025
// Recording control function library for Blueprint/C++ access
#include "RecordingBPLib.h"
#include "SensorBPLib.h"
#include "FusionCamSensor.h"
#include "Actor/FusionCamCaptureActor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "FusionCameraActor.h"
#include "UnrealcvLog.h"
#include "Utils/PythonExecutor.h"

// Static map to track recording actors (camera ID -> capture actor)
// This replaces the need to access CameraHandler's private map
TMap<FString, AFusionCamCaptureActor*> URecordingBPLib::GlobalCameraRecordingActors;

// Static variable for time dilation control
float URecordingBPLib::GlobalTimeDilation = 1.0f;

AFusionCamCaptureActor* URecordingBPLib::PrepareRecording(int32 CameraID)
{
	// Get the camera sensor
	UFusionCamSensor* FusionCamSensor = USensorBPLib::GetSensorById(CameraID);
	if (!IsValid(FusionCamSensor))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("URecordingBPLib::StartNormalRecording: Invalid camera ID %d"), CameraID);
		return nullptr;
	}
	FString CID = USensorBPLib::GetSensorNewFormatID(FusionCamSensor);

	// Check if this camera is already recording
	if (GlobalCameraRecordingActors.Contains(CID))
	{
		AFusionCamCaptureActor* ExistingActor = GlobalCameraRecordingActors[CID];
		if (IsValid(ExistingActor) && ExistingActor->IsRecording())
		{
			UE_LOG(LogUnrealCV, Warning, TEXT("URecordingBPLib::StartNormalRecording: Camera %s is already recording"), *CID);
			ExistingActor->StopRecord();
			if (ExistingActor->IsRecording())
			{
				UE_LOG(LogUnrealCV, Error, TEXT("URecordingBPLib::StartNormalRecording: Camera %s is already recording"), *CID);
				return nullptr;
			}
		}
		// // Clean up stale actor
		// if (IsValid(ExistingActor))
		// {
		// 	ExistingActor->Destroy();
		// }
		GlobalCameraRecordingActors.Remove(CID);
	}

	// Get world
	UWorld* World = FusionCamSensor->GetWorld();
	if (!IsValid(World))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("URecordingBPLib::StartNormalRecording: Cannot get world from camera %s"), *CID);
		return nullptr;
	}

	// Spawn new CaptureActor for this camera
	AFusionCamCaptureActor* CaptureActor = World->SpawnActor<AFusionCamCaptureActor>();
	if (!IsValid(CaptureActor))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("URecordingBPLib::StartNormalRecording: Failed to spawn FusionCamCaptureActor for camera %s"), *CID);
		return nullptr;
	}

	// Configure CaptureActor
	CaptureActor->TargetSensor = FusionCamSensor;
	CaptureActor->TimeDilation = GlobalTimeDilation;

	// Store the mapping
	GlobalCameraRecordingActors.Add(CID, CaptureActor);
	return CaptureActor;
}

bool URecordingBPLib::StopRecording(int32 CameraID)
{
	// Get the camera sensor
	UFusionCamSensor* FusionCamSensor = USensorBPLib::GetSensorById(CameraID);
	if (!IsValid(FusionCamSensor))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("URecordingBPLib::StopRecording: Invalid camera ID %d"), CameraID);
		return false;
	}
	FString CID = USensorBPLib::GetSensorNewFormatID(FusionCamSensor);
	// Check if we have a CaptureActor for this camera
	if (!GlobalCameraRecordingActors.Contains(CID))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("URecordingBPLib::StopRecording: Camera %s is not recording"), *CID);
		return false;
	}

	AFusionCamCaptureActor* CaptureActor = GlobalCameraRecordingActors[CID];
	if (!IsValid(CaptureActor))
	{
		// CaptureActor was destroyed, clean up the mapping
		GlobalCameraRecordingActors.Remove(CID);
		return false;
	}

	if (CaptureActor->IsRecording()){
		// Stop recording
		UE_LOG(LogUnrealCV, Log, TEXT("URecordingBPLib::StopRecording: Stopping camera %s"), *CID);
		CaptureActor->StopRecord();
	}

	// // Destroy the actor and clean up
	// CaptureActor->Destroy();
	GlobalCameraRecordingActors.Remove(CID);

	return true;
}

bool URecordingBPLib::IsRecording(int32 CameraID)
{

	// Get the camera sensor
	UFusionCamSensor* FusionCamSensor = USensorBPLib::GetSensorById(CameraID);
	if (!IsValid(FusionCamSensor))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("URecordingBPLib::IsRecording: Invalid camera ID %d"), CameraID);
		return false;
	}
	FString CID = USensorBPLib::GetSensorNewFormatID(FusionCamSensor);
	// Check if we have a CaptureActor for this camera
	if (!GlobalCameraRecordingActors.Contains(CID))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("URecordingBPLib::IsRecording: Camera %s does not have a recording actor"), *CID);
		return false;
	}

	AFusionCamCaptureActor* CaptureActor = GlobalCameraRecordingActors[CID];
	if (!IsValid(CaptureActor))
	{
		// CaptureActor was destroyed, clean up the mapping
		GlobalCameraRecordingActors.Remove(CID);
		return false;
	}

	return CaptureActor->IsRecording();
}

bool URecordingBPLib::StopRecording(const FString& IDString)
{
	return StopRecording(USensorBPLib::GetIndexByAnyID(IDString));
}

bool URecordingBPLib::IsRecording(const FString& IDString)
{
	return IsRecording(USensorBPLib::GetIndexByAnyID(IDString));
}


TArray<UFusionCamSensor*> URecordingBPLib::GetAllCameras()
{
	return USensorBPLib::GetFusionSensorList();
}

UFusionCamSensor* URecordingBPLib::GetCameraByID(int32 CameraID)
{
	return USensorBPLib::GetSensorById(CameraID);
}

FString URecordingBPLib::GetCameraName(int32 CameraID)
{
	UFusionCamSensor* Camera = USensorBPLib::GetSensorById(CameraID);
	if (!IsValid(Camera))
	{
		return FString::Printf(TEXT("Invalid Camera %d"), CameraID);
	}

	// Try to get a meaningful name from the actor
	AActor* Owner = Camera->GetOwner();
	if (IsValid(Owner))
	{
		return FString::Printf(TEXT("Camera %d (%s)"), CameraID, *Owner->GetName());
	}

	return FString::Printf(TEXT("Camera %d"), CameraID);
}

int32 URecordingBPLib::GetCameraCount()
{
	TArray<UFusionCamSensor*> Cameras = USensorBPLib::GetFusionSensorList();
	return Cameras.Num();
}

int32 URecordingBPLib::CreateFreeCamera(
	UObject* WorldContextObject,
	FVector Location,
	FRotator Rotation)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	AActor* Actor = World->SpawnActor(AFusionCameraActor::StaticClass());
	if (!IsValid(Actor))
	{
		return -1;
	}

	Actor->SetActorLocation(Location);
	Actor->SetActorRotation(Rotation);

	TArray<UFusionCamSensor*> AllCameras = USensorBPLib::GetFusionSensorList();
	int32 CameraID = AllCameras.Num() - 1;

	UE_LOG(LogUnrealCV, Log, TEXT("URecordingBPLib::CreateFreeCamera: Created camera ID %d at (%.1f, %.1f, %.1f)"),
		CameraID, Location.X, Location.Y, Location.Z);

	return CameraID;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////// Neo Trajector Render System /////////////////////////////////////////////

// ========== Camera Trajectory Recording Implementation ==========

bool URecordingBPLib::ParseTrajectoryType(const FString& TrajectoryTypeStr, ECameraTrajectoryType& OutTrajectoryType)
{
	// Normalize input: lowercase and trim whitespace
	FString Normalized = TrajectoryTypeStr.ToLower().TrimStartAndEnd();

	// Map string to enum
	static const TMap<FString, ECameraTrajectoryType> TrajectoryMap = {
		// Fixed trajectories
		{TEXT("rotate_left_45"),    ECameraTrajectoryType::RotateLeft45},
		{TEXT("rotate_left_30"),    ECameraTrajectoryType::RotateLeft30},
		{TEXT("rotate_right_45"),    ECameraTrajectoryType::RotateRight45},
		{TEXT("rotate_right_30"),    ECameraTrajectoryType::RotateRight30},
		{TEXT("rotate_up_45"),    ECameraTrajectoryType::RotateUp45},
		{TEXT("rotate_up_30"),    ECameraTrajectoryType::RotateUp30},
		{TEXT("rotate_360"),    ECameraTrajectoryType::Rotate360},

		{TEXT("zoom_in"),  ECameraTrajectoryType::ZoomIn},
		{TEXT("zoom_out"),  ECameraTrajectoryType::ZoomOut},

		{TEXT("random_1"),       ECameraTrajectoryType::RandomDirection1},
		{TEXT("random_2"),       ECameraTrajectoryType::RandomDirection2},
		{TEXT("random_3"),       ECameraTrajectoryType::RandomDirection3},
		{TEXT("random_4"),       ECameraTrajectoryType::RandomDirection4},

		{TEXT("render_only"),       ECameraTrajectoryType::RenderOnly},
		{TEXT("render_only_5s"),       ECameraTrajectoryType::RenderOnly5S},
	};

	const ECameraTrajectoryType* Found = TrajectoryMap.Find(Normalized);
	if (Found)
	{
		OutTrajectoryType = *Found;
		return true;
	}

	UE_LOG(LogUnrealCV, Error, TEXT("Invalid trajectory type: %s"), *TrajectoryTypeStr);
	return false;
}

bool URecordingBPLib::StartTrajectoryRecording(
	int32 CameraID,
	const FString& FileName,
	const FString& TrajectoryType,
	AActor* Target,
	int32 FPS,
	float DegreesPerSecond,
	int32 RandomSeed)
{
	// Validate target
	if (!IsValid(Target))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("StartTrajectoryRecording: Target actor is null"));
		return false;
	}

	// Parse trajectory type
	ECameraTrajectoryType TrajectoryEnum;
	if (!ParseTrajectoryType(TrajectoryType, TrajectoryEnum))
	{
		return false;
	}

	// Prepare recording (reuse existing function)
	AFusionCamCaptureActor* CaptureActor = PrepareRecording(CameraID);
	if (!IsValid(CaptureActor))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("StartTrajectoryRecording: Failed to prepare recording for camera %d"), CameraID);
		return false;
	}

	bool PauseWorldTime = false;

	// Start trajectory recording
	UE_LOG(LogUnrealCV, Log, TEXT("StartTrajectoryRecording: Camera %d, File: %s, Type: %s, FPS: %d, Deg/s: %.2f, Target: %s"),
		CameraID, *FileName, *TrajectoryType, FPS, DegreesPerSecond, *Target->GetName());

	CaptureActor->StartTrajectoryRecord(FileName, TrajectoryEnum, Target, FPS, DegreesPerSecond, RandomSeed, PauseWorldTime);

	return true;
}

TArray<FString> URecordingBPLib::GetSupportedTrajectoryTypes()
{
	return {
		// Fixed trajectories (canonical names)
		TEXT("rotate_left_45"),
		TEXT("rotate_right_45"),
		TEXT("rotate_up_45"),
		TEXT("rotate_360"),
		TEXT("zoom_in"),
		TEXT("zoom_out"),
		// Random trajectories
		TEXT("random_1"),
		TEXT("random_2"),
		TEXT("random_3"),
		TEXT("random_4"),
		// Render only trajectory
		TEXT("render_only")
	};
}

bool URecordingBPLib::StartSimpleRecording(int32 CameraID, const FString& FileName, int32 FPS, float DurationSeconds)
{
	AFusionCamCaptureActor* CaptureActor = PrepareRecording(CameraID);
	if (!IsValid(CaptureActor))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("StartSimpleRecording: Failed to prepare recording for camera %d"), CameraID);
		return false;
	}

	UE_LOG(LogUnrealCV, Log, TEXT("StartSimpleRecording: Camera %d, File: %s, FPS: %d, Duration: %.2fs"),
		CameraID, *FileName, FPS, DurationSeconds);

	CaptureActor->StartSimpleRecording(FileName, FPS, DurationSeconds);
	return true;
}

bool URecordingBPLib::StartSimpleRecording(const FString& IDString, const FString& FileName, int32 FPS, float DurationSeconds)
{
	return StartSimpleRecording(IDString, FileName, FPS, DurationSeconds, true, false, false, false, false);
}

bool URecordingBPLib::StartSimpleRecording(const FString& IDString, const FString& FileName, int32 FPS, float DurationSeconds,
	bool bRecordLit, bool bRecordMask, bool bRecordNormal, bool bRecordDepth, bool bRecordFlow)
{
	AFusionCamCaptureActor* CaptureActor = PrepareRecording(USensorBPLib::GetIndexByAnyID(IDString));
	if (!IsValid(CaptureActor))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("StartSimpleRecording: Failed to prepare recording for camera %s"), *IDString);
		return false;
	}

	CaptureActor->bRecordRGB = bRecordLit;
	CaptureActor->bRecordMask = bRecordMask;
	CaptureActor->bRecordNormal = bRecordNormal;
	CaptureActor->bRecordDepth = bRecordDepth;
	CaptureActor->bRecordFlow = bRecordFlow;

	UE_LOG(LogUnrealCV, Log, TEXT("StartSimpleRecording: Camera %s, File: %s, FPS: %d, Duration: %.2fs, RGB:%d Mask:%d Normal:%d Depth:%d Flow:%d"),
		*IDString, *FileName, FPS, DurationSeconds, bRecordLit, bRecordMask, bRecordNormal, bRecordDepth, bRecordFlow);

	CaptureActor->StartSimpleRecording(FileName, FPS, DurationSeconds);
	return true;
}


// ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ///////////////////////////////////////////// Video Generation Configuration ///////////////////////////////////////////
// ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// bool URecordingBPLib::SetAutoGenerateVideo(int32 CameraID, bool bEnabled)
// {
// 	if (!GlobalCameraRecordingActors.Contains(CameraID))
// 	{
// 		UE_LOG(LogUnrealCV, Warning, TEXT("SetAutoGenerateVideo: No recording actor found for camera %d"), CameraID);
// 		return false;
// 	}

// 	AFusionCamCaptureActor* CaptureActor = GlobalCameraRecordingActors[CameraID];
// 	if (!IsValid(CaptureActor))
// 	{
// 		GlobalCameraRecordingActors.Remove(CameraID);
// 		return false;
// 	}

// 	CaptureActor->bAutoGenerateVideo = bEnabled;
// 	UE_LOG(LogUnrealCV, Display, TEXT("SetAutoGenerateVideo: Camera %d auto-generate video: %s"), 
// 		CameraID, bEnabled ? TEXT("enabled") : TEXT("disabled"));
// 	return true;
// }

// bool URecordingBPLib::ConfigureVideoGeneration(
// 	int32 CameraID,
// 	const FString& ScriptPath,
// 	const FString& CondaEnvName)
// {
// 	if (!GlobalCameraRecordingActors.Contains(CameraID))
// 	{
// 		UE_LOG(LogUnrealCV, Warning, TEXT("ConfigureVideoGeneration: No recording actor found for camera %d"), CameraID);
// 		return false;
// 	}

// 	AFusionCamCaptureActor* CaptureActor = GlobalCameraRecordingActors[CameraID];
// 	if (!IsValid(CaptureActor))
// 	{
// 		GlobalCameraRecordingActors.Remove(CameraID);
// 		return false;
// 	}

// 	if (!ScriptPath.IsEmpty())
// 	{
// 		CaptureActor->VideoGenScriptPath = ScriptPath;
// 	}

// 	if (!CondaEnvName.IsEmpty())
// 	{
// 		CaptureActor->CondaEnvName = CondaEnvName;
// 	}

// 	UE_LOG(LogUnrealCV, Display, TEXT("ConfigureVideoGeneration: Camera %d configured (Script: %s, Env: %s)"),
// 		CameraID, *CaptureActor->VideoGenScriptPath, *CaptureActor->CondaEnvName);

// 	return true;
// }

// bool URecordingBPLib::GenerateVideoFromImages(
// 	const FString& FolderPath,
// 	int32 FPS,
// 	const FString& ScriptPath,
// 	const FString& CondaEnvName)
// {
// 	FString ActualScriptPath = ScriptPath;
// 	if (ActualScriptPath.IsEmpty())
// 	{
// 		FString PluginBaseDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectPluginsDir() / TEXT("unrealcv/Source/uezoo"));
// 		ActualScriptPath = FPaths::Combine(PluginBaseDir, TEXT("genvid.py"));

// 		if (!FPaths::FileExists(ActualScriptPath))
// 		{
// 			UE_LOG(LogUnrealCV, Error, TEXT("GenerateVideoFromImages: Cannot find genvid.py at %s"), *ActualScriptPath);
// 			return false;
// 		}
// 	}

// 	FString ActualCondaEnv = CondaEnvName.IsEmpty() ? TEXT("uezoo") : CondaEnvName;

// 	int32 ProcessID = 0;
// 	bool bSuccess = FPythonExecutor::ExecuteGenvidScript(
// 		ActualScriptPath,
// 		FolderPath,
// 		FPS,
// 		ActualCondaEnv,
// 		&ProcessID
// 	);

// 	if (bSuccess)
// 	{
// 		UE_LOG(LogUnrealCV, Display, TEXT("GenerateVideoFromImages: Video generation started (PID: %d) for folder: %s"), 
// 			ProcessID, *FolderPath);
// 	}
// 	else
// 	{
// 		UE_LOG(LogUnrealCV, Error, TEXT("GenerateVideoFromImages: Failed to start video generation for folder: %s"), *FolderPath);
// 	}

// 	return bSuccess;
// }

float URecordingBPLib::GetTimeDilation()
{
	return GlobalTimeDilation;
}

void URecordingBPLib::SetTimeDilation(float Value)
{
	GlobalTimeDilation = FMath::Clamp(Value, 0.1f, 10.0f);
	UE_LOG(LogUnrealCV, Log, TEXT("URecordingBPLib::SetTimeDilation: Set to %.2f"), GlobalTimeDilation);
}
