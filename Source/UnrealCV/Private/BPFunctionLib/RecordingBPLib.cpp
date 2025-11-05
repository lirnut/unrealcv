// Weichao Qiu @ 2018
// Recording control function library for Blueprint/C++ access
#include "RecordingBPLib.h"
#include "SensorBPLib.h"
#include "FusionCamSensor.h"
#include "Actor/FusionCamCaptureActor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UnrealcvLog.h"
#include "Utils/PythonExecutor.h"

// Static map to track recording actors (camera ID -> capture actor)
// This replaces the need to access CameraHandler's private map
static TMap<int32, AFusionCamCaptureActor*> GlobalCameraRecordingActors;

AFusionCamCaptureActor* URecordingBPLib::PrepareRecording(int32 CameraID)
{
	// Get the camera sensor
	UFusionCamSensor* FusionCamSensor = USensorBPLib::GetSensorById(CameraID);
	if (!IsValid(FusionCamSensor))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("URecordingBPLib::StartNormalRecording: Invalid camera ID %d"), CameraID);
		return nullptr;
	}

	// Check if this camera is already recording
	if (GlobalCameraRecordingActors.Contains(CameraID))
	{
		AFusionCamCaptureActor* ExistingActor = GlobalCameraRecordingActors[CameraID];
		if (IsValid(ExistingActor) && ExistingActor->IsRecording())
		{
			UE_LOG(LogUnrealCV, Warning, TEXT("URecordingBPLib::StartNormalRecording: Camera %d is already recording"), CameraID);
			return nullptr;
		}
		// // Clean up stale actor
		// if (IsValid(ExistingActor))
		// {
		// 	ExistingActor->Destroy();
		// }
		GlobalCameraRecordingActors.Remove(CameraID);
	}

	// Get world
	UWorld* World = FusionCamSensor->GetWorld();
	if (!IsValid(World))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("URecordingBPLib::StartNormalRecording: Cannot get world from camera %d"), CameraID);
		return nullptr;
	}

	// Spawn new CaptureActor for this camera
	AFusionCamCaptureActor* CaptureActor = World->SpawnActor<AFusionCamCaptureActor>();
	if (!IsValid(CaptureActor))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("URecordingBPLib::StartNormalRecording: Failed to spawn FusionCamCaptureActor for camera %d"), CameraID);
		return nullptr;
	}

	// Configure CaptureActor
	CaptureActor->TargetSensor = FusionCamSensor;

	// Store the mapping
	GlobalCameraRecordingActors.Add(CameraID, CaptureActor);
	return CaptureActor;
}

bool URecordingBPLib::StartNormalRecording(
	int32 CameraID,
	const FString& FileName,
	float Duration,
	int32 FPS,
	AActor* TargetToHide,
	float TimeDilation)
{
	AFusionCamCaptureActor* CaptureActor = PrepareRecording(CameraID);
	if (!IsValid(CaptureActor))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("URecordingBPLib::StartNormalRecording: Failed to prepare recording for camera %d"), CameraID);
		return false;
	}
	CaptureActor->TimeDilation = TimeDilation;
	// Start recording
	UE_LOG(LogUnrealCV, Log, TEXT("URecordingBPLib::StartNormalRecording: Camera %d, File: %s, Duration: %.2fs, FPS: %d"), CameraID, *FileName, Duration, FPS);
	CaptureActor->StartRecord(FileName, Duration, FPS, TargetToHide);

	return true;
}

bool URecordingBPLib::StartBulletTimeRecording(
	int32 CameraID,
	const FString& FileName,
	float Duration,
	int32 FPS,
	AActor* Target,
	float TimeDilation)
{
	AFusionCamCaptureActor* CaptureActor = PrepareRecording(CameraID);
	if (!IsValid(CaptureActor))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("URecordingBPLib::StartBulletTimeRecording: Failed to prepare recording for camera %d"), CameraID);
		return false;
	}

	CaptureActor->TimeDilation = TimeDilation;
	// Start bullet time recording
	UE_LOG(LogUnrealCV, Log, TEXT("URecordingBPLib::StartBulletTimeRecording: Camera %d, File: %s, Duration: %.2fs, FPS: %d, Target: %s"),
		CameraID, *FileName, Duration, FPS, *Target->GetName());
	CaptureActor->StartBulletTimeRecord(FileName, Duration, FPS, Target);

	return true;
}

bool URecordingBPLib::StartBulletTimeOnlyRecording(
	int32 CameraID,
	const FString& FileName,
	float Duration,
	int32 FPS,
	AActor* Target,
	float TimeDilation)
{
	AFusionCamCaptureActor* CaptureActor = PrepareRecording(CameraID);
	if (!IsValid(CaptureActor))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("URecordingBPLib::StartBulletTimeRecording: Failed to prepare recording for camera %d"), CameraID);
		return false;
	}

	CaptureActor->TimeDilation = TimeDilation;
	// Start bullet time recording
	UE_LOG(LogUnrealCV, Log, TEXT("URecordingBPLib::StartBulletTimeRecording: Camera %d, File: %s, Duration: %.2fs, FPS: %d, Target: %s"),
		CameraID, *FileName, Duration, FPS, *Target->GetName());
	CaptureActor->StartBulletTimeRecordOnly(FileName, Duration, FPS, Target);

	return true;
}

bool URecordingBPLib::StopRecording(int32 CameraID)
{
	// Check if we have a CaptureActor for this camera
	if (!GlobalCameraRecordingActors.Contains(CameraID))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("URecordingBPLib::StopRecording: Camera %d is not recording"), CameraID);
		return false;
	}

	AFusionCamCaptureActor* CaptureActor = GlobalCameraRecordingActors[CameraID];
	if (!IsValid(CaptureActor))
	{
		// CaptureActor was destroyed, clean up the mapping
		GlobalCameraRecordingActors.Remove(CameraID);
		return false;
	}

	// {
	// 	// Stop recording
	// 	UE_LOG(LogUnrealCV, Log, TEXT("URecordingBPLib::StopRecording: Stopping camera %d"), CameraID);
	// 	CaptureActor->StopRecord();

	// 	// Destroy the actor and clean up
	// 	CaptureActor->Destroy();
	// }
	GlobalCameraRecordingActors.Remove(CameraID);

	return true;
}

bool URecordingBPLib::IsRecording(int32 CameraID)
{
	// Check if we have a CaptureActor for this camera
	if (!GlobalCameraRecordingActors.Contains(CameraID))
	{
		return false;
	}

	AFusionCamCaptureActor* CaptureActor = GlobalCameraRecordingActors[CameraID];
	if (!IsValid(CaptureActor))
	{
		// CaptureActor was destroyed, clean up the mapping
		GlobalCameraRecordingActors.Remove(CameraID);
		return false;
	}

	return CaptureActor->IsRecording();
}

bool URecordingBPLib::GetRecordingProgress(
	int32 CameraID,
	float& OutProgress,
	int32& OutFrameCount)
{
	OutProgress = 0.0f;
	OutFrameCount = 0;

	// Check if we have a CaptureActor for this camera
	if (!GlobalCameraRecordingActors.Contains(CameraID))
	{
		return false;
	}

	AFusionCamCaptureActor* CaptureActor = GlobalCameraRecordingActors[CameraID];
	if (!IsValid(CaptureActor))
	{
		// CaptureActor was destroyed, clean up the mapping
		GlobalCameraRecordingActors.Remove(CameraID);
		return false;
	}

	if (!CaptureActor->IsRecording())
	{
		return false;
	}

	// TODO: Add progress tracking to AFusionCamCaptureActor
	// For now, we can only report if recording is active
	// You may need to add GetProgress() and GetFrameCount() methods to AFusionCamCaptureActor

	OutProgress = 0.5f; // Placeholder
	OutFrameCount = 0;  // Placeholder

	return true;
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

// int32 URecordingBPLib::CreateFreeCamera(
// 	UObject* WorldContextObject,
// 	FVector Location,
// 	FRotator Rotation)
// {
// 	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
// 	if (!World)
// 	{
// 		UE_LOG(LogUnrealCV, Error, TEXT("URecordingBPLib::CreateFreeCamera: Invalid world context"));
// 		return -1;
// 	}

// 	APlayerController* PlayerController = World->GetFirstPlayerController();
// 	if (!PlayerController)
// 	{
// 		UE_LOG(LogUnrealCV, Error, TEXT("URecordingBPLib::CreateFreeCamera: No player controller found"));
// 		return -1;
// 	}

// 	APawn* Pawn = PlayerController->GetPawn();
// 	if (!Pawn)
// 	{
// 		UE_LOG(LogUnrealCV, Error, TEXT("URecordingBPLib::CreateFreeCamera: No pawn found"));
// 		return -1;
// 	}

// 	UFusionCamSensor* Sensor = NewObject<UFusionCamSensor>(Pawn, UFusionCamSensor::StaticClass());
// 	if (!IsValid(Sensor))
// 	{
// 		UE_LOG(LogUnrealCV, Error, TEXT("URecordingBPLib::CreateFreeCamera: Failed to create sensor"));
// 		return -1;
// 	}

// 	Sensor->AttachToComponent(Pawn->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
// 	Sensor->RegisterComponent();
// 	Sensor->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
// 	Sensor->SetWorldLocationAndRotation(Location, Rotation);

// 	TArray<UFusionCamSensor*> AllCameras = USensorBPLib::GetFusionSensorList();
// 	int32 CameraID = AllCameras.Num() - 1;

// 	UE_LOG(LogUnrealCV, Log, TEXT("URecordingBPLib::CreateFreeCamera: Created camera ID %d at (%.1f, %.1f, %.1f), attached to pawn '%s'"),
// 		CameraID, Location.X, Location.Y, Location.Z, *Pawn->GetName());

// 	return CameraID;
// }

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
		{TEXT("rotate_right_45"),    ECameraTrajectoryType::RotateRight45},
		{TEXT("rotate_up_45"),    ECameraTrajectoryType::RotateUp45},
		{TEXT("rotate_360"),    ECameraTrajectoryType::Rotate360},

		{TEXT("zoom_in"),  ECameraTrajectoryType::ZoomIn},
		{TEXT("zoom_out"),  ECameraTrajectoryType::ZoomOut},

		{TEXT("random_1"),       ECameraTrajectoryType::RandomDirection1},
		{TEXT("random_2"),       ECameraTrajectoryType::RandomDirection2},
		{TEXT("random_3"),       ECameraTrajectoryType::RandomDirection3},
		{TEXT("random_4"),       ECameraTrajectoryType::RandomDirection4},
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

	// Start trajectory recording
	UE_LOG(LogUnrealCV, Log, TEXT("StartTrajectoryRecording: Camera %d, File: %s, Type: %s, FPS: %d, Deg/s: %.2f, Target: %s"),
		CameraID, *FileName, *TrajectoryType, FPS, DegreesPerSecond, *Target->GetName());

	CaptureActor->StartTrajectoryRecord(FileName, TrajectoryEnum, Target, FPS, DegreesPerSecond, RandomSeed);

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
		TEXT("random_4")
	};
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
