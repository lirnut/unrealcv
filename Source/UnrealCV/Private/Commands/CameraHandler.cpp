// Weichao Qiu @ 2017
// This is unrealcv command API for FusionSensor
#include "CameraHandler.h"
#include "Runtime/Engine/Classes/GameFramework/Pawn.h"
#include "Runtime/Engine/Classes/Engine/World.h"
#include "Runtime/Engine/Classes/Engine/GameViewportClient.h"
#include "Runtime/Engine/Classes/GameFramework/Controller.h"
#include "Misc/Paths.h"

// #include "AudioDevice.h"
// #include "AudioMixerDevice.h"
// #include "Sound/SoundWave.h"
// #include "Misc/FileHelper.h"
// #include "Misc/Paths.h"
// #include <Serialization/BufferArchive.h>
#include "SL.h"
#include "ST.h"
#include "Utils/UObjectUtils.h"
#include "Controller/ActorController.h"
#include "AnnotationCamSensor.h"
#include "LitCamSensor.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"

#include "CommandDispatcher.h"
#include "FusionCamSensor.h"
#include "Serialization.h"
#include "Utils/StrFormatter.h"
#include "PlayerViewMode.h"
#include "WorldController.h"
#include "ImageUtil.h"
#include "SensorBPLib.h"
#include "FusionCameraActor.h"
#include "Actor/FusionCamCaptureActor.h"
#include "Actor/CameraMotionController.h"

#include "UnrealcvStats.h"
#include "UnrealClient.h"
#include "UnrealcvLog.h"

DECLARE_CYCLE_STAT(TEXT("FCameraHandler::GetCameraLit"), STAT_GetCameraLit, STATGROUP_UnrealCV);
DECLARE_CYCLE_STAT(TEXT("FCameraHandler::SaveData"), STAT_SaveData, STATGROUP_UnrealCV);

UFusionCamSensor* FCameraHandler::GetCamera(const TArray<FString>& Args, FExecStatus& Status)
{
	if (Args.Num() < 1)
	{
		FString Msg = TEXT("No sensor id is available");
		UE_LOG(LogTemp, Warning, TEXT("%s"), *Msg);
		Status = FExecStatus::Error(Msg);
		return nullptr;
	}
	int SensorId = FCString::Atoi(*Args[0]);
	UFusionCamSensor* FusionSensor = USensorBPLib::GetSensorById(SensorId);
	if (!IsValid(FusionSensor)) 
	{
		FString Msg = TEXT("Invalid sensor id");
		UE_LOG(LogTemp, Warning, TEXT("%s"), *Msg);
		Status = FExecStatus::Error(Msg);
		return nullptr;
	}
	return FusionSensor;
}


/** vget /sensors , List all sensors in the world */
FExecStatus FCameraHandler::GetCameraList(const TArray<FString>& Args)
{
	TArray<UFusionCamSensor*> GameWorldSensorList = USensorBPLib::GetFusionSensorList();

	FString StrSensorList;
	for (UFusionCamSensor* Sensor : GameWorldSensorList)
	{
		StrSensorList += FString::Printf(TEXT("%s "), *Sensor->GetName());
	}
	return FExecStatus::OK(StrSensorList);
}


FExecStatus FCameraHandler::GetCameraLocation(const TArray<FString>& Args)
{
	FExecStatus Status = FExecStatus::OK();
	UFusionCamSensor* FusionCamSensor = GetCamera(Args, Status);
	if (!IsValid(FusionCamSensor)) return Status; 

	FStrFormatter Ar;
	FVector Location = FusionCamSensor->GetSensorLocation();
	Ar << Location;

	return FExecStatus::OK(Ar.ToString());
}

FExecStatus FCameraHandler::SetCameraLocation(const TArray<FString>& Args)
{
	FExecStatus Status = FExecStatus::OK();
	UFusionCamSensor* FusionCamSensor = GetCamera(Args, Status);
	if (!IsValid(FusionCamSensor)) return Status; 

	// Should I set the component loction or the actor location?
	if (Args.Num() != 4) return FExecStatus::GetInvalidArgument(); // ID, X, Y, Z

	float X = FCString::Atof(*Args[1]), Y = FCString::Atof(*Args[2]), Z = FCString::Atof(*Args[3]);
	FVector Location = FVector(X, Y, Z);

	if (Args[0] == "0")
	{
		// Note: For camera 0, we want to change the player location

		bool Sweep = false;
		// Note: If sweep is true, the object can not move through another object
		// Note: It will check invalid location and move back a bit.
		APawn* Pawn = FUnrealcvServer::Get().GetPawn();
		if (!IsValid(Pawn))
		{
			UE_LOG(LogTemp, Warning, TEXT("The Pawn of the scene is invalid."));
			return FExecStatus::GetInvalidArgument();
		}
		Pawn->SetActorLocation(Location, Sweep, NULL, ETeleportType::TeleportPhysics);
	}
	else
	{
		FusionCamSensor->SetSensorLocation(Location);
	}

	return FExecStatus::OK();
}

FExecStatus FCameraHandler::GetCameraRotation(const TArray<FString>& Args)
{
	FExecStatus Status = FExecStatus::OK();
	UFusionCamSensor* FusionCamSensor = GetCamera(Args, Status);
	if (!IsValid(FusionCamSensor)) return Status; 

	FRotator Rotation = FusionCamSensor->GetSensorRotation();
	FStrFormatter Ar;
	Ar << Rotation;

	return FExecStatus::OK(Ar.ToString());
}

FExecStatus FCameraHandler::SetCameraRotation(const TArray<FString>& Args)
{
	FExecStatus Status = FExecStatus::OK();
	UFusionCamSensor* FusionCamSensor = GetCamera(Args, Status);
	if (!IsValid(FusionCamSensor)) return Status; 

	if (Args.Num() != 4) return FExecStatus::GetInvalidArgument(); // ID, X, Y, Z
	float Pitch = FCString::Atof(*Args[1]), Yaw = FCString::Atof(*Args[2]), Roll = FCString::Atof(*Args[3]);
	FRotator Rotator = FRotator(Pitch, Yaw, Roll);

	// Note: For camera 0, we want to change the player rotation
	if (Args[0] == "0")
	{
		APawn* Pawn = FUnrealcvServer::Get().GetPawn();
		if (!IsValid(Pawn))
		{
			UE_LOG(LogTemp, Warning, TEXT("The Pawn of the scene is invalid."));
			return FExecStatus::GetInvalidArgument();
		}
		AController* Controller = Pawn->GetController();
		if (!IsValid(Controller))
		{
			UE_LOG(LogTemp, Warning, TEXT("The Controller of the Pawn is invalid."));
			return FExecStatus::GetInvalidArgument();
		}
		Controller->ClientSetRotation(Rotator); // Teleport action
	}
	else
	{
		FusionCamSensor->SetSensorRotation(Rotator);
	}

	return FExecStatus::OK();
}


// // TODO: Move this to utility library
// EFilenameType FCameraHandler::ParseFilenameType(const FString& Filename)
// {
// 	bool bIncludeDot = false;
// 	FString FileExtension = FPaths::GetExtension(Filename);
// 	FileExtension.ToLowerInline();

// 	// A hacky way to check whether the input is just a file extension
// 	int DotIndex;
// 	if (!Filename.FindChar('.', DotIndex)) FileExtension = Filename;

// 	if (FileExtension == Filename) // The filename only contains extension, which means the binary mode
// 	{
// 		if (FileExtension == TEXT("png")) return EFilenameType::PngBinary;
// 		if (FileExtension == TEXT("bmp")) return EFilenameType::BmpBinary;
// 		if (FileExtension == TEXT("npy")) return EFilenameType::NpyBinary;
// 	}
// 	else
// 	{
// 		if (FileExtension == TEXT("png")) return EFilenameType::Png;
// 		if (FileExtension == TEXT("bmp")) return EFilenameType::Bmp;
// 		if (FileExtension == TEXT("npy")) return EFilenameType::Npy;
// 		if (FileExtension == TEXT("exr")) return EFilenameType::Exr;
// 	}
// 	return EFilenameType::Invalid;
// }

// /** Serialize data according to filename format */
// FExecStatus FCameraHandler::SerializeData(const TArray<FColor>& Data, int Width, int Height, const FString& Filename)
// {
// 	static FImageUtil ImageUtil;
// 	EFilenameType FilenameType = ParseFilenameType(Filename);

// 	TArray<uint8> BinaryData;
// 	switch (FilenameType)
// 	{
// 	case EFilenameType::BmpBinary:
// 		ImageUtil.ConvertToBmp(Data, Width, Height, BinaryData);
// 		return FExecStatus::Binary(BinaryData);
// 	case EFilenameType::Bmp:
// 		ImageUtil.SaveBmpFile(Data, Width, Height, Filename);
// 		return FExecStatus::OK(Filename);
// 	case EFilenameType::PngBinary:
// 		ImageUtil.ConvertToPng(Data, Width, Height, BinaryData);
// 		return FExecStatus::Binary(BinaryData);
// 	case EFilenameType::Png:
// 		ImageUtil.SavePngFile(Data, Width, Height, Filename);
// 		return FExecStatus::OK(Filename);
// 	}
// 	return FExecStatus::Error(FString::Printf(TEXT("Invalid filename type, filename %s"), *Filename));
// }

// FExecStatus FCameraHandler::SerializeData(const TArray<FFloat16Color>& Data, int Width, int Height, const FString& Filename)
// {
// 	static FImageUtil ImageUtil;
// 	EFilenameType FilenameType = ParseFilenameType(Filename);

// 	TArray<uint8> BinaryData;
// 	int Channel = Data.Num() / (Width * Height);
// 	switch (FilenameType)
// 	{
// 	case EFilenameType::NpyBinary:
// 		BinaryData = FSerializationUtils::Array2Npy(Data, Width, Height, Channel);
// 		return FExecStatus::Binary(BinaryData);
// 	case EFilenameType::Npy:
// 		BinaryData = FSerializationUtils::Array2Npy(Data, Width, Height, Channel);
// 		ImageUtil.SaveFile(BinaryData, Filename);
// 		return FExecStatus::OK(Filename);
// 	}
// 	return FExecStatus::Error(FString::Printf(TEXT("Invalid filename type, filename %s"), *Filename));
// }

// FExecStatus FCameraHandler::SerializeData(const TArray<float>& Data, int Width, int Height, const FString& Filename)
// {
// 	static FImageUtil ImageUtil;
// 	EFilenameType FilenameType = ParseFilenameType(Filename);

// 	TArray<uint8> BinaryData;
// 	int Channel = Data.Num() / (Width * Height);
// 	switch (FilenameType)
// 	{
// 	case EFilenameType::NpyBinary:
// 		BinaryData = FSerializationUtils::Array2Npy(Data, Width, Height, Channel);
// 		return FExecStatus::Binary(BinaryData);
// 	case EFilenameType::Npy:
// 		BinaryData = FSerializationUtils::Array2Npy(Data, Width, Height, Channel);
// 		ImageUtil.SaveFile(BinaryData, Filename);
// 		return FExecStatus::OK(Filename);
// 	}
// 	return FExecStatus::Error(FString::Printf(TEXT("Invalid filename type, filename %s"), *Filename));
// }

template<class T>
void FCameraHandler::SaveData(const TArray<T>& Data, int Width, int Height,
	const TArray<FString>& Args, FExecStatus& Status)
{
	SCOPE_CYCLE_COUNTER(STAT_SaveData);

	if (Args.Num() != 2)
	{
		Status = FExecStatus::Error("Filename can not be empty");
		return;
	}
	FString Filename = Args[1];
	if (Data.Num() == 0)
	{
		Status = FExecStatus::Error("Captured data is empty");
		return;
	}
	Status = SerializeData(Data, Width, Height, Filename);
	return;
}


FExecStatus FCameraHandler::GetCameraLit(const TArray<FString>& Args)
{
	double StartTime = FPlatformTime::Seconds();
	SCOPE_CYCLE_COUNTER(STAT_GetCameraLit);

	FExecStatus ExecStatus = FExecStatus::OK();
	UFusionCamSensor* FusionCamSensor = GetCamera(Args, ExecStatus);
	if (!IsValid(FusionCamSensor)) return ExecStatus;

	if (Args.Num() != 2)
	{
		return FExecStatus::Error("Filename can not be empty");
	}
	FString Filename = Args[1];

	EFilenameType FilenameType = ParseFilenameType(Filename);

	if (FilenameType == EFilenameType::Png && FusionCamSensor->GetUseFastCapture())
	{
		FusionCamSensor->SaveLitToFile(Filename);
		return FExecStatus::OK(Filename);
	}

	TArray<FColor> Data;
	int Width, Height;
	FusionCamSensor->GetLit(Data, Width, Height);
	double SaveDataStartTime = FPlatformTime::Seconds();
	SaveData(Data, Width, Height, Args, ExecStatus);
	UE_LOG(LogTemp, Log, TEXT("GetCameraLit Cmd cost time: %f, in which SaveData cost time: %f"), FPlatformTime::Seconds() - StartTime, FPlatformTime::Seconds() - SaveDataStartTime);
	return ExecStatus;
}

FExecStatus FCameraHandler::GetCameraDepth(const TArray<FString>& Args)
{
	FExecStatus ExecStatus = FExecStatus::OK();
	UFusionCamSensor* FusionCamSensor = GetCamera(Args, ExecStatus);
	if (!IsValid(FusionCamSensor)) return ExecStatus;

	if (Args.Num() != 2)
	{
		return FExecStatus::Error("Filename can not be empty");
	}
	FString Filename = Args[1];

	EFilenameType FilenameType = ParseFilenameType(Filename);

	if (FilenameType == EFilenameType::Npy && FusionCamSensor->GetUseFastCapture())
	{
		FusionCamSensor->SaveDepthToFile(Filename);
		return FExecStatus::OK(Filename);
	}

	TArray<float> Data;
	int Width, Height;
	FusionCamSensor->GetDepth(Data, Width, Height);
	SaveData(Data, Width, Height, Args, ExecStatus);
	return ExecStatus;
}


FExecStatus FCameraHandler::GetCameraNormal(const TArray<FString>& Args)
{
	FExecStatus ExecStatus = FExecStatus::OK();
	UFusionCamSensor* FusionCamSensor = GetCamera(Args, ExecStatus);
	if (!IsValid(FusionCamSensor)) return ExecStatus;

	if (Args.Num() != 2)
	{
		return FExecStatus::Error("Filename can not be empty");
	}
	FString Filename = Args[1];

	EFilenameType FilenameType = ParseFilenameType(Filename);

	if (FilenameType == EFilenameType::Png && FusionCamSensor->GetUseFastCapture())
	{
		FusionCamSensor->SaveNormalToFile(Filename);
		return FExecStatus::OK(Filename);
	}

	TArray<FColor> Data;
	int Width, Height;
	FusionCamSensor->GetNormal(Data, Width, Height);
	SaveData(Data, Width, Height, Args, ExecStatus);
	return ExecStatus;
}

FExecStatus FCameraHandler::GetCameraFlow(const TArray<FString>& Args)
{
	FExecStatus ExecStatus = FExecStatus::OK();
	UFusionCamSensor* FusionCamSensor = GetCamera(Args, ExecStatus);
	if (!IsValid(FusionCamSensor)) return ExecStatus;

	if (Args.Num() != 2)
	{
		return FExecStatus::Error("Filename can not be empty");
	}
	FString Filename = Args[1];

	EFilenameType FilenameType = ParseFilenameType(Filename);

	if (FilenameType == EFilenameType::Png && FusionCamSensor->GetUseFastCapture())
	{
		FusionCamSensor->SaveFlowToFile(Filename);
		return FExecStatus::OK(Filename);
	}

	TArray<FColor> Data;
	int Width, Height;
	FusionCamSensor->GetFlow(Data, Width, Height);
	if (Data.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: Flow data is empty (if you are using old character/drone blueprints, you have to rebuild this project, because the flow sensor in FusionCamSensor is a component of the blueprint.)"), *FString(__FUNCTION__));
	}
	SaveData(Data, Width, Height, Args, ExecStatus);
	return ExecStatus;
}

FExecStatus FCameraHandler::GetCameraSeg(const TArray<FString>& Args)
{
	FExecStatus ExecStatus = FExecStatus::OK();
	UFusionCamSensor* FusionCamSensor = GetCamera(Args, ExecStatus);
	if (!IsValid(FusionCamSensor)) return ExecStatus;

	if (Args.Num() != 2)
	{
		return FExecStatus::Error("Filename can not be empty");
	}
	FString Filename = Args[1];

	EFilenameType FilenameType = ParseFilenameType(Filename);

	if (FilenameType == EFilenameType::Png && FusionCamSensor->GetUseFastCapture())
	{
		FusionCamSensor->SaveSegToFile(Filename);
		return FExecStatus::OK(Filename);
	}

	TArray<FColor> Data;
	int Width, Height;
	FusionCamSensor->GetSeg(Data, Width, Height);

	SaveData(Data, Width, Height, Args, ExecStatus);
	return ExecStatus;
}

FExecStatus FCameraHandler::GetUseFastCapture(const TArray<FString>& Args)
{
	FExecStatus ExecStatus = FExecStatus::OK();
	UFusionCamSensor* FusionCamSensor = GetCamera(Args, ExecStatus);
	if (!IsValid(FusionCamSensor)) return ExecStatus;

	bool bUseFastCapture = FusionCamSensor->GetUseFastCapture();
	FString Result = bUseFastCapture ? TEXT("1") : TEXT("0");
	return FExecStatus::OK(Result);
}

FExecStatus FCameraHandler::SetUseFastCapture(const TArray<FString>& Args)
{
	FExecStatus ExecStatus = FExecStatus::OK();
	UFusionCamSensor* FusionCamSensor = GetCamera(Args, ExecStatus);
	if (!IsValid(FusionCamSensor)) return ExecStatus;

	if (Args.Num() != 2)
	{
		return FExecStatus::Error("Usage: vset /camera/[uint]/use_fast_capture [0|1]");
	}

	bool bEnable = FCString::Atoi(*Args[1]) != 0;
	FusionCamSensor->SetUseFastCapture(bEnable);
	return FExecStatus::OK(FString::Printf(TEXT("FastCapture set to %d"), bEnable ? 1 : 0));
}

FExecStatus FCameraHandler::MoveTo(const TArray<FString>& Args)
{
	// FExecStatus ExecStatus = FExecStatus::OK();
	// UFusionCamSensor* FusionCamSensor = GetCamera(Args, ExecStatus);
	// if (!IsValid(FusionCamSensor)) return ExecStatus; 

	/** The API for Character, Pawn and Actor are different */
	if (Args.Num() != 4) // ID, X, Y, Z
	{
		return FExecStatus::GetInvalidArgument();
	}
	if (Args[0] != "0")
	{
		return FExecStatus::Error("MoveTo only supports the player camera with id 0");
	}

	float X = FCString::Atof(*Args[1]), Y = FCString::Atof(*Args[2]), Z = FCString::Atof(*Args[3]);
	FVector Location = FVector(X, Y, Z);

	bool Sweep = true;
	// if sweep is true, the object can not move through another object
	// Check invalid location and move back a bit.
	bool Success = FUnrealcvServer::Get().GetPawn()->SetActorLocation(Location, Sweep, NULL, ETeleportType::TeleportPhysics);

	return FExecStatus::OK();
}


/** vget /screenshot [filename] */
FExecStatus FCameraHandler::GetScreenshot(const TArray<FString>& Args)
{
	FString Filename = Args[0];

	UWorld* World = FUnrealcvServer::Get().GetWorld();
	UGameViewportClient* ViewportClient = World->GetGameViewport();

	bool bScreenshotSuccessful = false;
	FViewport* InViewport = ViewportClient->Viewport;
	ViewportClient->GetEngineShowFlags()->SetMotionBlur(false);
	FIntVector Size(InViewport->GetSizeXY().X, InViewport->GetSizeXY().Y, 0);

	TArray<FColor> Bitmap;
	bScreenshotSuccessful = GetViewportScreenShot(InViewport, Bitmap);
	// InViewport->ReadFloat16Pixels

	// Ensure that all pixels' alpha is set to 255
	for (auto& Color : Bitmap)
	{
		Color.A = 255;
	}
	// TODO: Need to blend alpha, a bit weird from screen.

	FExecStatus ExecStatus = SerializeData(Bitmap, Size.X, Size.Y, Filename);
	return ExecStatus;
}

FExecStatus FCameraHandler::SetPlayerViewMode(const TArray<FString>& Args)
{
	TWeakObjectPtr<AUnrealcvWorldController> WorldController = FUnrealcvServer::Get().WorldController;
	return WorldController->PlayerViewMode->SetMode(Args);
}

FExecStatus FCameraHandler::GetPlayerViewMode(const TArray<FString>& Args)
{
	TWeakObjectPtr<AUnrealcvWorldController> WorldController = FUnrealcvServer::Get().WorldController;
	return WorldController->PlayerViewMode->GetMode(Args);
}

FExecStatus FCameraHandler::GetFOV(const TArray<FString>& Args)
{
	FExecStatus Status = FExecStatus::GetInvalidArgument();
	UFusionCamSensor* FusionCamSensor = GetCamera(Args, Status);
	if (!IsValid(FusionCamSensor)) return FExecStatus::GetInvalidArgument();

	if (Args.Num() != 1) return FExecStatus::GetInvalidArgument(); // ID

	float FOV = FusionCamSensor->GetSensorFOV();
	FString Res = FString::Printf(TEXT("%f"), FOV);
	return FExecStatus::OK(Res);
}

FExecStatus FCameraHandler::SetFOV(const TArray<FString>& Args)
{
	FExecStatus Status = FExecStatus::GetInvalidArgument();
	UFusionCamSensor* FusionCamSensor = GetCamera(Args, Status);
	if (!IsValid(FusionCamSensor)) return FExecStatus::GetInvalidArgument();

	if (Args.Num() != 2) return FExecStatus::GetInvalidArgument(); // ID, FOV

	float FOV = FCString::Atof(*Args[1]);
	FusionCamSensor->SetSensorFOV(FOV);
	return FExecStatus::OK();
}

FExecStatus FCameraHandler::SpawnCamera(const TArray<FString>& Args)
{
	UWorld* GameWorld = FUnrealcvServer::Get().GetWorld();
	AActor* Actor = GameWorld->SpawnActor(AFusionCameraActor::StaticClass());
	if (IsValid(Actor))
	{
		return FExecStatus::OK(Actor->GetName());
	}
	else
	{
		return FExecStatus::Error("Failed to spawn actor");
	}
}

FExecStatus FCameraHandler::GetSize(const TArray<FString>& Args)
{
	FExecStatus Status = FExecStatus::GetInvalidArgument();
	UFusionCamSensor* FusionCamSensor = GetCamera(Args, Status);
	if (!IsValid(FusionCamSensor)) return FExecStatus::GetInvalidArgument();

	if (Args.Num() != 1) return FExecStatus::GetInvalidArgument(); // ID

	int Width = FusionCamSensor->GetFilmWidth();
	int Height = FusionCamSensor->GetFilmHeight();
	FString Res = FString::Printf(TEXT("%d %d"), Width, Height);
	return FExecStatus::OK(Res);
}

FExecStatus FCameraHandler::SetSize(const TArray<FString>& Args)
{
	FExecStatus Status = FExecStatus::GetInvalidArgument();
	UFusionCamSensor* FusionCamSensor = GetCamera(Args, Status);
	if (!IsValid(FusionCamSensor)) return FExecStatus::GetInvalidArgument();

	if (Args.Num() != 3) return FExecStatus::GetInvalidArgument(); // ID, Width, Height

	int Width = FCString::Atof(*Args[1]);
	int Height = FCString::Atof(*Args[2]);
	FusionCamSensor->SetFilmSize(Width, Height);
	return FExecStatus::OK();
}

FExecStatus FCameraHandler::SetProjectionType(const TArray<FString>& Args)
{
	if (Args.Num() != 2) return FExecStatus::GetInvalidArgument();

	FExecStatus Status = FExecStatus::GetInvalidArgument();
	UFusionCamSensor* FusionCamSensor = GetCamera(Args, Status);
	FString ProjectionType = Args[1];
	if (ProjectionType.ToLower() == "perspective")
	{
		FusionCamSensor->SetProjectionType(ECameraProjectionMode::Type::Perspective);
		return FExecStatus::OK();
	}
	else if (ProjectionType.ToLower() == "orthographic")
	{
		FusionCamSensor->SetProjectionType(ECameraProjectionMode::Type::Orthographic);
		return FExecStatus::OK();
	}
	else
	{
		FString ErrorMsg = FString::Printf(TEXT("Can not support camera mode %s, available options are perspective and orthographic"), *ProjectionType);
		return FExecStatus::Error(ErrorMsg);
	}
}

FExecStatus FCameraHandler::SetOrthoWidth(const TArray<FString>& Args)
{
	if (Args.Num() != 2) return FExecStatus::GetInvalidArgument();

	FExecStatus Status = FExecStatus::GetInvalidArgument();
	UFusionCamSensor* FusionCamSensor = GetCamera(Args, Status);

	int OrthoWidth = FCString::Atof(*Args[1]);
	FusionCamSensor->SetOrthoWidth(OrthoWidth);
	return FExecStatus::OK();
}

FExecStatus FCameraHandler::SetExposureMethod(const TArray<FString>& Args)
{
    FExecStatus Status = FExecStatus::GetInvalidArgument();
	UFusionCamSensor* FusionCamSensor = GetCamera(Args, Status);
	if (!IsValid(FusionCamSensor)) return FExecStatus::GetInvalidArgument();
	if (Args.Num() != 2) return FExecStatus::GetInvalidArgument(); // exposure value
	FString ExposureType = Args[1];
	if (ExposureType.ToLower() == "histogram")
    {
        FusionCamSensor->SetExposureMethod(EAutoExposureMethod::AEM_Histogram);
        return FExecStatus::OK();
    }
    else if  (ExposureType.ToLower() == "basic")
    {
        FusionCamSensor->SetExposureMethod(EAutoExposureMethod::AEM_Basic);
        return FExecStatus::OK();
    }
    else if  (ExposureType.ToLower() == "manual")
    {
        FusionCamSensor->SetExposureMethod(EAutoExposureMethod::AEM_Manual);
        return FExecStatus::OK();
    }
    else
    {
        FString ErrorMsg = FString::Printf(TEXT("Can not support auto exposure mode %s, available options are true and false"), *ExposureType);
        return FExecStatus::Error(ErrorMsg);
    }
}

FExecStatus FCameraHandler::SetLitSource(const TArray<FString>& Args)
{
    FExecStatus Status = FExecStatus::GetInvalidArgument();
    UFusionCamSensor* FusionCamSensor = GetCamera(Args, Status);
    if (!IsValid(FusionCamSensor)) return FExecStatus::GetInvalidArgument();
    if (Args.Num() != 2) return FExecStatus::GetInvalidArgument();
    FString LitSource = Args[1];
    if (LitSource.ToLower() == "ftc_hdr")
    {
        FusionCamSensor->SetLitCaptureSource(ESceneCaptureSource::SCS_FinalToneCurveHDR);
        return FExecStatus::OK();
    }
    else if (LitSource.ToLower() == "fc_hdr")
    {
        FusionCamSensor->SetLitCaptureSource(ESceneCaptureSource::SCS_FinalColorHDR);
        return FExecStatus::OK();
    }
    else if (LitSource.ToLower() == "sc_hdr")
    {
        FusionCamSensor->SetLitCaptureSource(ESceneCaptureSource::SCS_SceneColorHDR);
        return FExecStatus::OK();
    }
    else if (LitSource.ToLower() == "scna_hdr")
    {
        FusionCamSensor->SetLitCaptureSource(ESceneCaptureSource::SCS_SceneColorHDRNoAlpha);
        return FExecStatus::OK();
    }
    else if (LitSource.ToLower() == "ldr")
    {
        FusionCamSensor->SetLitCaptureSource(ESceneCaptureSource::SCS_FinalColorLDR);
        return FExecStatus::OK();
    }
    else if (LitSource.ToLower() == "base")
    {
        FusionCamSensor->SetLitCaptureSource(ESceneCaptureSource::SCS_BaseColor);
        return FExecStatus::OK();
    }
    else if (LitSource.ToLower() == "color_depth")
    {
        FusionCamSensor->SetLitCaptureSource(ESceneCaptureSource::SCS_SceneDepth);
        return FExecStatus::OK();
    }
    else if (LitSource.ToLower() == "scene_depth")
    {
        FusionCamSensor->SetLitCaptureSource(ESceneCaptureSource::SCS_SceneDepth);
        return FExecStatus::OK();
    }
    else if (LitSource.ToLower() == "device_depth")
    {
        FusionCamSensor->SetLitCaptureSource(ESceneCaptureSource::SCS_DeviceDepth);
        return FExecStatus::OK();
    }
    else if (LitSource.ToLower() == "normal")
    {
        FusionCamSensor->SetLitCaptureSource(ESceneCaptureSource::SCS_Normal);
        return FExecStatus::OK();
    }
    else
    {
        FString ErrorMsg = FString::Printf(TEXT("Can not support lit source %s, available options are ftc_hdr, fc_hdr, sc_hdr, scna_hdr, ldr, base, color_depth, scene_depth, device_depth, normal"), *LitSource);
        return FExecStatus::Error(ErrorMsg);
    }
}

FExecStatus FCameraHandler::SetReflectionMethod(const TArray<FString>& Args)
{
    FExecStatus Status = FExecStatus::GetInvalidArgument();
    UFusionCamSensor* FusionCamSensor = GetCamera(Args, Status);
    if (!IsValid(FusionCamSensor)) return FExecStatus::GetInvalidArgument();
    if (Args.Num() != 2) return FExecStatus::GetInvalidArgument();
    FString ReflectionMethod = Args[1];
    if (ReflectionMethod.ToLower() == "none")
    {
        FusionCamSensor->SetReflectionMethod(EReflectionMethod::Type::None);
        return FExecStatus::OK();
    }
    else if (ReflectionMethod.ToLower() == "lumen")
    {
        FusionCamSensor->SetReflectionMethod(EReflectionMethod::Type::Lumen);
        return FExecStatus::OK();
    }
    else if (ReflectionMethod.ToLower() == "screen_space")
    {
        FusionCamSensor->SetReflectionMethod(EReflectionMethod::Type::ScreenSpace);
        return FExecStatus::OK();
    }
    else
    {
        FString ErrorMsg = FString::Printf(TEXT("Can not support reflection method %s, available options are none, lumen, screen_space."), *ReflectionMethod);
        return FExecStatus::Error(ErrorMsg);
    }
}

FExecStatus FCameraHandler::SetGlobalIlluminationMethod(const TArray<FString>& Args)
{
    FExecStatus Status = FExecStatus::GetInvalidArgument();
    UFusionCamSensor* FusionCamSensor = GetCamera(Args, Status);
    if (!IsValid(FusionCamSensor)) return FExecStatus::GetInvalidArgument();
    if (Args.Num() != 2) return FExecStatus::GetInvalidArgument();
    FString IlluminationMethod = Args[1];
    if (IlluminationMethod.ToLower() == "none")
    {
        FusionCamSensor->SetGlobalIlluminationMethod(EDynamicGlobalIlluminationMethod::Type::None);
        return FExecStatus::OK();
    }
    else if (IlluminationMethod.ToLower() == "lumen")
    {
        FusionCamSensor->SetGlobalIlluminationMethod(EDynamicGlobalIlluminationMethod::Type::Lumen);
        return FExecStatus::OK();
    }
    else if (IlluminationMethod.ToLower() == "screen_space")
    {
        FusionCamSensor->SetGlobalIlluminationMethod(EDynamicGlobalIlluminationMethod::Type::ScreenSpace);
        return FExecStatus::OK();
    }
    else if (IlluminationMethod.ToLower() == "plugin")
    {
        FusionCamSensor->SetGlobalIlluminationMethod(EDynamicGlobalIlluminationMethod::Type::Plugin);
        return FExecStatus::OK();
    }
    else
    {
        FString ErrorMsg = FString::Printf(TEXT("Can not support global illumination method %s, available options are none, lumen, screen_space, plugin."), *IlluminationMethod);
        return FExecStatus::Error(ErrorMsg);
    }
}

FExecStatus FCameraHandler::SetExposureBias(const TArray<FString>& Args)
{
    FExecStatus Status = FExecStatus::GetInvalidArgument();
    UFusionCamSensor* FusionCamSensor = GetCamera(Args, Status);
    if (!IsValid(FusionCamSensor)) return FExecStatus::GetInvalidArgument();
    if (Args.Num() != 2) return FExecStatus::GetInvalidArgument(); // exposure value
    float ExposureBias = FCString::Atof(*Args[1]);
    FusionCamSensor->SetExposureBias(ExposureBias);
    return FExecStatus::OK();
}

FExecStatus FCameraHandler::SetAutoExposureSpeed(const TArray<FString>& Args)
{
    FExecStatus Status = FExecStatus::GetInvalidArgument();
    UFusionCamSensor* FusionCamSensor = GetCamera(Args, Status);
    if (!IsValid(FusionCamSensor)) return FExecStatus::GetInvalidArgument();
    if (Args.Num() != 3) return FExecStatus::GetInvalidArgument(); // exposure value
    float SpeedDown = FCString::Atof(*Args[1]);
    float SpeedUp = FCString::Atof(*Args[2]);
    FusionCamSensor->SetAutoExposureSpeed(SpeedDown, SpeedUp);
    return FExecStatus::OK();
}

FExecStatus FCameraHandler::SetAutoExposureBrightness(const TArray<FString>& Args)
{
    FExecStatus Status = FExecStatus::GetInvalidArgument();
    UFusionCamSensor* FusionCamSensor = GetCamera(Args, Status);
    if (!IsValid(FusionCamSensor)) return FExecStatus::GetInvalidArgument();
    if (Args.Num() != 3) return FExecStatus::GetInvalidArgument();
    float MinBrightness = FCString::Atof(*Args[1]);
    float MaxBrightness = FCString::Atof(*Args[2]);
    FusionCamSensor->SetAutoExposureBrightness(MinBrightness, MaxBrightness);
    return FExecStatus::OK();
}


FExecStatus FCameraHandler::SetApplyPhysicalCameraExposure(const TArray<FString>& Args)
{
    FExecStatus Status = FExecStatus::GetInvalidArgument();
    UFusionCamSensor* FusionCamSensor = GetCamera(Args, Status);
    if (!IsValid(FusionCamSensor)) return FExecStatus::GetInvalidArgument();
    if (Args.Num() != 2) return FExecStatus::GetInvalidArgument();
    int ApplyPhysicalCameraExposure = FCString::Atoi(*Args[1]);
    FusionCamSensor->SetApplyPhysicalCameraExposure(ApplyPhysicalCameraExposure);
    return FExecStatus::OK();
}

FExecStatus FCameraHandler::SetMotionBlurParams(const TArray<FString>& Args)
{
    FExecStatus Status = FExecStatus::GetInvalidArgument();
    UFusionCamSensor* FusionCamSensor = GetCamera(Args, Status);
    if (!IsValid(FusionCamSensor)) return FExecStatus::GetInvalidArgument();
    if (Args.Num() != 5) return FExecStatus::GetInvalidArgument(); // motion blur amount, max, per object, fps
    float MotionBlurAmount = FCString::Atof(*Args[1]);
    float MotionBlurMax = FCString::Atof(*Args[2]);
    float MotionBlurPerObject = FCString::Atof(*Args[3]);
    int MotionBlurFPS = FCString::Atoi(*Args[4]);
    FusionCamSensor->SetMotionBlurParams(MotionBlurAmount, MotionBlurMax, MotionBlurPerObject, MotionBlurFPS);
    return FExecStatus::OK();
}

FExecStatus FCameraHandler::SetFocalParams(const TArray<FString>& Args)
{
    FExecStatus Status = FExecStatus::GetInvalidArgument();
    UFusionCamSensor* FusionCamSensor = GetCamera(Args, Status);
    if (!IsValid(FusionCamSensor)) return FExecStatus::GetInvalidArgument();
    if (Args.Num() != 3) return FExecStatus::GetInvalidArgument(); // exposure value
    float FocalDistance = FCString::Atof(*Args[1]);
    float FocalRange = FCString::Atof(*Args[2]);
    FusionCamSensor->SetFocalParams(FocalDistance, FocalRange);
    return FExecStatus::OK();
}


// void SetSubmix(AActor* Actor, USoundSubmix* TargetSubmix) {
// 	TArray<UAudioComponent*> AudioComponents;
// 	Actor->GetComponents<UAudioComponent>(AudioComponents);

// 	if (AudioComponents.Num() == 0)
// 	{
// 		UE_LOG(LogTemp, Warning, TEXT("Actor %s has no audio components!"), *Actor->GetName());
// 	}
// 	else
// 	{
// 		// 2. �����ǵ���� Submix ��Ϊ TargetSubmix
// 		for (UAudioComponent* AudioComp : AudioComponents)
// 		{
// 			if (AudioComp && AudioComp->Sound)
// 			{
// 				// ������Ƶ��������ǵ�ר�� Submix
// 				AudioComp->Sound->SoundSubmix = TargetSubmix;

// 				UE_LOG(LogTemp, Log, TEXT("Redirected %s's audio to TargetSubmix"), *Actor->GetName());
// 			}
// 		}
// 	}

// }

FExecStatus FCameraHandler::SetCameraAudioRecord(const TArray<FString>& Args)
{
	FExecStatus ExecStatus = FExecStatus::OK();
	if (Args.Num() < 1)
	{
		FString Msg = TEXT("No sensor id is available");
		UE_LOG(LogTemp, Warning, TEXT("%s"), *Msg);
		ExecStatus = FExecStatus::Error(Msg);
		return ExecStatus;
	}

	if (Args.Num() < 2)
	{
		FString Msg = TEXT("No on/off command available");
		UE_LOG(LogTemp, Warning, TEXT("%s"), *Msg);
		ExecStatus = FExecStatus::Error(Msg);
		return ExecStatus;
	}

	bool StartRecord;
	if (Args[1] == FString("on")) {
		StartRecord = true;
	}
	else if (Args[1] == FString("off")) {
		StartRecord = false;
	}
	else {
		FString Msg = FString::Printf(TEXT("Invalid on/off command '%s'"), *Args[1]);
		UE_LOG(LogTemp, Warning, TEXT("%s"), *Msg);
		ExecStatus = FExecStatus::Error(Msg);
		return ExecStatus;
	}

	if (StartRecord) {
		// return StartCameraAudioRecord(Args);
		return ExecStatus;
	}
	else {
		// return StopCameraAudioRecord(Args);
		return ExecStatus;
	}

}


FExecStatus FCameraHandler::GetHWObs(const TArray<FString>& Args) {
	return GetHWObsV3(Args);
}

FExecStatus FCameraHandler::GetHWObsV3(const TArray<FString>& Args)
{
    auto t_func_start = std::chrono::steady_clock::now();
    SL::get().print("FCameraHandler::GetHWObsV3 called");

    {
        ScopedStepTimer _t("Arg validation");
        // ԭ�߼�������У��
    }

    FExecStatus ExecStatus = FExecStatus::OK();
    if (Args.Num() != 3) {
        FString Msg = TEXT("Invalid command length.");
        SL::get().print(TCHAR_TO_UTF8(*Msg));
        ExecStatus = FExecStatus::Error(Msg);
        return ExecStatus;
    }

    FString TargetId = Args[2];
    FString FileName = Args[1];

    int32 index;
    {
        ScopedStepTimer _t("Check file name has extension");
        if (!FileName.FindLastChar(TEXT('.'), index)) {
            FString msg = TEXT("File name is not a path, binary is not supported.");
            SL::get().print(TCHAR_TO_UTF8(*msg));
            ExecStatus = FExecStatus::Error(msg);
            return ExecStatus;
        }
    }

    UFusionCamSensor* FusionCamSensor = nullptr;
    {
        ScopedStepTimer _t("GetCamera()");
        FusionCamSensor = GetCamera(Args, ExecStatus);
    }
    if (!IsValid(FusionCamSensor)) { return ExecStatus; }

    UAnnotationCamSensor* AnnotationCamSensor = nullptr;
    ULitCamSensor*       LitCamSensor = nullptr;
    {
        ScopedStepTimer _t("Get sub-sensors from FusionCamSensor");
        AnnotationCamSensor = FusionCamSensor->GetAnnotationCamSensor();
        LitCamSensor        = FusionCamSensor->GetLitCamSensor();
    }

    // Lit target check + init
    {
        ScopedStepTimer _t("LitCamSensor::CheckTextureTarget()");
        // ����ʱ��һ�μ��
        (void)LitCamSensor->CheckTextureTarget();
    }
    {
        ScopedStepTimer _t("LitCamSensor::InitTextureTarget (if needed)");
        if (LitCamSensor->CheckTextureTarget()) {
            LitCamSensor->InitTextureTarget(FusionCamSensor->GetFilmWidth(), FusionCamSensor->GetFilmHeight());
            if (!LitCamSensor->CheckTextureTarget()) {
                SL::get().print("LitCamSensor InitTextureTarget failed.");
                ExecStatus = FExecStatus::Error("LitCamSensor InitTextureTarget failed.");
                SL::get().printf("[TIMER] total elapsed before failure: %.3f ms", ms_since(t_func_start));
                return ExecStatus;
            }
        }
    }

    // Annotation target check + init
    {
        ScopedStepTimer _t("AnnotationCamSensor::CheckTextureTarget()");
        (void)AnnotationCamSensor->CheckTextureTarget();
    }
    {
        ScopedStepTimer _t("AnnotationCamSensor::InitTextureTarget (if needed)");
        if (!AnnotationCamSensor->CheckTextureTarget()) {
            AnnotationCamSensor->InitTextureTarget(FusionCamSensor->GetFilmWidth(), FusionCamSensor->GetFilmHeight());
            if (!AnnotationCamSensor->CheckTextureTarget()) {
                SL::get().print("AnnotationCamSensor InitTextureTarget failed.");
                ExecStatus = FExecStatus::Error("AnnotationCamSensor InitTextureTarget failed.");
                SL::get().printf("[TIMER] total elapsed before failure: %.3f ms", ms_since(t_func_start));
                return ExecStatus;
            }
        }
    }

    TArray<FColor> DataRGB, DataM, DataRGBNoTarget;

    FString FileNameRGB = FileName;       { ScopedStepTimer _t("Build FileNameRGB");       FileNameRGB.InsertAt(index, TEXT("_rgb")); }
    FString FileNameM = FileName;         { ScopedStepTimer _t("Build FileNameM");         FileNameM.InsertAt(index, TEXT("_mask")); }
    FString FileNameRGBNoTarget = FileName; { ScopedStepTimer _t("Build FileNameRGBNoTarget"); FileNameRGBNoTarget.InsertAt(index, TEXT("_rgb_no_target")); }

    { ScopedStepTimer _t("LitCamSensor::CaptureScene() #1"); LitCamSensor->CaptureScene(); }

    TArray<TWeakObjectPtr<UPrimitiveComponent>> ComponentList;
    {
        ScopedStepTimer _t("AnnotationCamSensor::GetAnnotationComponents()");
        AnnotationCamSensor->GetAnnotationComponents(this->GetWorld(), ComponentList);
    }

    {
        ScopedStepTimer _t("Assign ShowOnlyComponents");
        AnnotationCamSensor->ShowOnlyComponents = ComponentList;
    }

    { ScopedStepTimer _t("AnnotationCamSensor::CaptureScene()"); AnnotationCamSensor->CaptureScene(); }

    AActor* Target = nullptr;
    {
        ScopedStepTimer _t("GetActorById()");
        Target = GetActorById(FUnrealcvServer::Get().GetWorld(), TargetId);
    }
    if (!Target) {
        ExecStatus = FExecStatus::Error("Can not find target");
        SL::get().print("Can not find target");
        SL::get().printf("[TIMER] total elapsed before failure: %.3f ms", ms_since(t_func_start));
        return ExecStatus;
    }

    FActorController TargetController(Target);

    {
        ScopedStepTimer _t("LitCamSensor::ReadCaptureResults(DataRGB)");
        LitCamSensor->ReadCaptureResults(DataRGB);
    }

    int32 LitW = LitCamSensor->GetFilmWidth();
    int32 LitH = LitCamSensor->GetFilmHeight();

    if (DataRGB.Num() == LitW * LitH) {
        ScopedStepTimer _t("SerializeData RGB");
        SerializeData(DataRGB, LitW, LitH, FileNameRGB);
    } else {
        SL::get().print("DataRGB size is not equal to LitW * LitH");
    }
    SL::get().printf("FCameraHandler::GetHWObs DataRGB size: %d, width: %d, height: %d", DataRGB.Num(), LitW, LitH);

    { ScopedStepTimer _t("TargetController.Hide()"); TargetController.Hide(); }

    { ScopedStepTimer _t("LitCamSensor::CaptureScene() #2 (no target)"); LitCamSensor->CaptureScene(); }

    {
        ScopedStepTimer _t("AnnotationCamSensor::ReadCaptureResults(DataM)");
        AnnotationCamSensor->ReadCaptureResults(DataM);
    }

    int32 SegW = AnnotationCamSensor->GetFilmWidth();
    int32 SegH = AnnotationCamSensor->GetFilmHeight();
    SL::get().printf("FCameraHandler::GetHWObs DataM size: %d, width: %d, height: %d", DataM.Num(), SegW, SegH);


    if (DataM.Num() == SegW * SegH) {
        ScopedStepTimer _t("SerializeData MASK");
        SerializeData(DataM, SegW, SegH, FileNameM);
    } else {
        SL::get().print("DataM size is not equal to SegW * SegH");
    }

    {
        ScopedStepTimer _t("LitCamSensor::ReadCaptureResults(DataRGBNoTarget)");
        LitCamSensor->ReadCaptureResults(DataRGBNoTarget);
    }
    SL::get().printf("FCameraHandler::GetHWObs DataRGBNoTarget size: %d, width: %d, height: %d", DataRGBNoTarget.Num(), LitW, LitH);

    { ScopedStepTimer _t("TargetController.Show()"); TargetController.Show(); }

    if (DataRGBNoTarget.Num() == LitW * LitH) {
        ScopedStepTimer _t("SerializeData RGB_NO_TARGET");
        SerializeData(DataRGBNoTarget, LitW, LitH, FileNameRGBNoTarget);
    } else {
        SL::get().print("DataRGBNoTarget size is not equal to LitW * LitH");
    }

    SL::get().printf("[TIMER] GetHWObsV3 total elapsed: %.3f ms", ms_since(t_func_start));
    return FExecStatus::OK(FileNameRGB + TEXT(",") + FileNameM + TEXT(",") + FileNameRGBNoTarget);
}


// FExecStatus FCameraHandler::GetHWObsV2(const TArray<FString>& Args)
// {
// 	SL::get().print("FCameraHandler::GetHWObs called");
// 	FExecStatus ExecStatus = FExecStatus::OK();
// 	if (Args.Num() != 3) {
// 		FString Msg = TEXT("Invalid command length.");
// 		SL::get().print(TCHAR_TO_UTF8(*Msg));
// 		ExecStatus = FExecStatus::Error(Msg);
// 		return ExecStatus;
// 	}

// 	FString TargetId = Args[2];
// 	FString FileName = Args[1];
// 	int32 index;
// 	if (!FileName.FindLastChar(TEXT('.'), index)) {
// 		FString msg = TEXT("File name is not a path, binary is not supported.");
// 		SL::get().print(TCHAR_TO_UTF8(*msg));
// 		ExecStatus = FExecStatus::Error(msg);
// 		return ExecStatus;
// 	}


// 	AActor* Target = GetActorById(FUnrealcvServer::Get().GetWorld(), TargetId);
// 	if (!Target) {
// 		ExecStatus = FExecStatus::Error("Can not find target");
// 		SL::get().print("Can not find target");
// 		return ExecStatus;
// 	}
// 	FActorController TargetController(Target);

// 	UFusionCamSensor* FusionCamSensor = GetCamera(Args, ExecStatus);
// 	if (!IsValid(FusionCamSensor)) { return ExecStatus; }

// 	TArray<FColor> DataRGB, DataM;
// 	int Width, Height;
// 	FString FileNameRGB = FileName; FileNameRGB.InsertAt(index, TEXT("_rgb"));
// 	FString FileNameM = FileName; FileNameM.InsertAt(index, TEXT("_mask"));
// 	FusionCamSensor->GetLitSeg(DataRGB, DataM, Width, Height);
// 	SL::get().printf("FCameraHandler::GetHWObs DataM size: %d, width: %d, height: %d", DataM.Num(), Width, Height);
// 	SL::get().printf("FCameraHandler::GetHWObs DataRGB size: %d, width: %d, height: %d", DataRGB.Num(), Width, Height);

// 	TArray<FColor> DataRGBNoTarget;
// 	FString FileNameRGBNoTarget = FileName; FileNameRGBNoTarget.InsertAt(index, TEXT("_rgb_no_target"));
// 	TargetController.Hide();
// 	FusionCamSensor->GetLit(DataRGBNoTarget, Width, Height);
// 	TargetController.Show();

// 	SerializeData(DataRGB, Width, Height, FileNameRGB);
// 	SerializeData(DataM, Width, Height, FileNameM);
// 	SerializeData(DataRGBNoTarget, Width, Height, FileNameRGBNoTarget);

// 	return FExecStatus::OK(FileNameRGB + TEXT(",") + FileNameM + TEXT(",") + FileNameRGBNoTarget);
// }

FExecStatus FCameraHandler::GetHWObsV1(const TArray<FString>& Args)
{
	SL::get().print("FCameraHandler::GetHWObs called");
	FExecStatus ExecStatus = FExecStatus::OK();
	if (Args.Num() != 3) {
		FString Msg = TEXT("Invalid command length.");
		SL::get().print(TCHAR_TO_UTF8(*Msg));
		ExecStatus = FExecStatus::Error(Msg);
		return ExecStatus;
	}

	FString TargetId = Args[2];
	FString FileName = Args[1];
	int32 index;
	if (!FileName.FindLastChar(TEXT('.'), index)) {
		FString msg = TEXT("File name is not a path, binary is not supported.");
		SL::get().print(TCHAR_TO_UTF8(*msg));
		ExecStatus = FExecStatus::Error(msg);
		return ExecStatus;
	}


	AActor* Target = GetActorById(FUnrealcvServer::Get().GetWorld(), TargetId);
	if (!Target) {
		ExecStatus = FExecStatus::Error("Can not find target");
		SL::get().print("Can not find target");
		return ExecStatus;
	}
	FActorController TargetController(Target);

	UFusionCamSensor* FusionCamSensor = GetCamera(Args, ExecStatus);
	if (!IsValid(FusionCamSensor)) { return ExecStatus; }

	TArray<FColor> DataRGB;
	FString FileNameRGB = FileName; FileNameRGB.InsertAt(index, TEXT("_rgb"));
	int Width, Height;
	FusionCamSensor->GetLit(DataRGB, Width, Height);
	SL::get().printf("FCameraHandler::GetHWObs DataRGB size: %d, width: %d, height: %d", DataRGB.Num(), Width, Height);

	
	TArray<FColor> DataM;
	FString FileNameM = FileName; FileNameM.InsertAt(index, TEXT("_mask"));
	FusionCamSensor->GetSeg(DataM, Width, Height);
	SL::get().printf("FCameraHandler::GetHWObs DataM size: %d, width: %d, height: %d", DataM.Num(), Width, Height);

	TArray<FColor> DataRGBNoTarget;
	FString FileNameRGBNoTarget = FileName; FileNameRGBNoTarget.InsertAt(index, TEXT("_rgb_no_target"));
	TargetController.Hide();
	FusionCamSensor->GetLit(DataRGBNoTarget, Width, Height);
	TargetController.Show();

	SerializeData(DataRGB, Width, Height, FileNameRGB);
	SerializeData(DataM, Width, Height, FileNameM);
	SerializeData(DataRGBNoTarget, Width, Height, FileNameRGBNoTarget);

	return FExecStatus::OK(FileNameRGB + TEXT(",") + FileNameM + TEXT(",") + FileNameRGBNoTarget);
}


FExecStatus FCameraHandler::GetCameraOneObjMask(const TArray<FString>& Args)
{
	SL::get().print("FCameraHandler::GetCameraObjMask called");
	FExecStatus ExecStatus = FExecStatus::OK();
	if (Args.Num() != 3) {
		FString Msg = TEXT("Invalid command length.");
		SL::get().print(TCHAR_TO_UTF8(*Msg));
		ExecStatus = FExecStatus::Error(Msg);
		return ExecStatus;
	}

	FString ObjectId = Args[2];
	FString FileName = Args[1];


	UFusionCamSensor* FusionCamSensor = GetCamera(Args, ExecStatus);
	if (!IsValid(FusionCamSensor)) {
		SL::get().print("FCameraHandler::GetCameraObjMask error, FusionCamSensor is not valid");
		return ExecStatus;
	}


	TArray<FColor> Data;
	int Width, Height;
	FusionCamSensor->GetObjMask(ObjectId, Data, Width, Height);

	if (Data.Num() == 0)
	{
		ExecStatus = FExecStatus::Error("Captured data is empty");
		SL::get().print("FCameraHandler::GetCameraObjMask error, Captured data is empty");
		return ExecStatus;
	}
	ExecStatus = SerializeData(Data, Width, Height, FileName);
	SL::get().print("FCameraHandler::GetCameraObjMask returned");
	return ExecStatus;
}


// FExecStatus FCameraHandler::StartRecord(const TArray<FString>& Args)
// {
// 	SL::get().print("FCameraHandler::StartRecord called");

// 	FExecStatus ExecStatus = FExecStatus::OK();
// 	AActor* Target = nullptr;
// 	if (Args.Num() == 5) {
// 		FString TargetId = Args[4];
// 		Target = GetActorById(FUnrealcvServer::Get().GetWorld(), TargetId);
// 		if (!Target) {
// 			ExecStatus = FExecStatus::Error("Can not find target");
// 			SL::get().print("Can not find target");
// 			return ExecStatus;
// 		}
// 	}
// 	else if (Args.Num() != 4) {
// 		FString Msg = TEXT("Invalid command length.");
// 		SL::get().print(TCHAR_TO_UTF8(*Msg));
// 		ExecStatus = FExecStatus::Error(Msg);
// 		return ExecStatus;
// 	}

// 	FString FileName = Args[1];
// 	int32 index;
// 	if (!FileName.FindLastChar(TEXT('.'), index)) {
// 		FString msg = TEXT("File name is not a path, binary is not supported.");
// 		SL::get().print(TCHAR_TO_UTF8(*msg));
// 		ExecStatus = FExecStatus::Error(msg);
// 		return ExecStatus;
// 	}

// 	double Time = FCString::Atod(*Args[2]);
// 	float FPS = FCString::Atof(*Args[3]);
// 	if (Time <= 0) {
// 		FString msg = TEXT("Time is invalid: " + Args[2]);

// 		SL::get().print(TCHAR_TO_UTF8(*msg));
// 		ExecStatus = FExecStatus::Error(msg);
// 		return ExecStatus;
// 	}
// 	if (FPS <= 0 || FPS >= 60) {
// 		FString msg = TEXT("FPS is invalid: " + Args[3]);
// 		SL::get().print(TCHAR_TO_UTF8(*msg));
// 		ExecStatus = FExecStatus::Error(msg);
// 		return ExecStatus;
// 	}

// 	// Get the camera sensor
// 	UFusionCamSensor* FusionCamSensor = GetCamera(Args, ExecStatus);
// 	if (!IsValid(FusionCamSensor)) { return ExecStatus; }

// 	// Get camera ID
// 	int32 SensorId = FCString::Atoi(*Args[0]);

// 	// Check if this camera is already recording
// 	if (CameraRecordingActors.Contains(SensorId))
// 	{
// 		FString Msg = FString::Printf(TEXT("Camera %d is already recording"), SensorId);
// 		UE_LOG(LogUnrealCV, Warning, TEXT("%s"), *Msg);
// 		return FExecStatus::Error(Msg);
// 	}

// 	// Create new CaptureActor for this camera
// 	UWorld* World = FUnrealcvServer::Get().GetWorld();
// 	if (!IsValid(World))
// 	{
// 		return FExecStatus::Error("Cannot get world");
// 	}

// 	AFusionCamCaptureActor* CaptureActor = World->SpawnActor<AFusionCamCaptureActor>();
// 	if (!IsValid(CaptureActor))
// 	{
// 		return FExecStatus::Error("Failed to spawn FusionCamCaptureActor");
// 	}

// 	// Configure CaptureActor
// 	CaptureActor->TargetSensor = FusionCamSensor;

// 	// Store the mapping
// 	CameraRecordingActors.Add(SensorId, CaptureActor);

// 	// Start recording
// 	SL::get().printf("FCameraHandler::StartRecord: FileName: %s, Time: %lf, FPS: %lf", TCHAR_TO_UTF8(*FileName), Time, FPS);
// 	CaptureActor->StartRecord(FileName, Time, FPS, Target);

// 	// save cmd
//     FString Content = FString::Printf(TEXT("vset /camera/%s/record %s %s %s"), *Args[0], *Args[1], *Args[2], *Args[3]);
// 	FString CmdFileName = FileName;
// 	CmdFileName.RemoveAt(index, FileName.Len() - index);
// 	CmdFileName += TEXT(".cmd.txt");
//     FFileHelper::SaveStringToFile(Content, *CmdFileName);

// 	SL::get().print("FCameraHandler::StartRecord returned");
//     return FExecStatus::OK();
// }

// FExecStatus FCameraHandler::StartBulletTimeRecord(const TArray<FString>& Args)
// {
// 	SL::get().print("FCameraHandler::StartBulletTimeRecord called");

// 	FExecStatus ExecStatus = FExecStatus::OK();
// 	AActor* Target = nullptr;
// 	if (Args.Num() == 5) {
// 		FString TargetId = Args[4];
// 		Target = GetActorById(FUnrealcvServer::Get().GetWorld(), TargetId);
// 		if (!Target) {
// 			ExecStatus = FExecStatus::Error("Can not find target");
// 			SL::get().print("Can not find target");
// 			return ExecStatus;
// 		}
// 	}
// 	else {
// 		FString Msg = TEXT("Invalid command length.");
// 		SL::get().print(TCHAR_TO_UTF8(*Msg));
// 		ExecStatus = FExecStatus::Error(Msg);
// 		return ExecStatus;
// 	}

// 	FString FileName = Args[1];
// 	int32 index;
// 	if (!FileName.FindLastChar(TEXT('.'), index)) {
// 		FString msg = TEXT("File name is not a path, binary is not supported.");
// 		SL::get().print(TCHAR_TO_UTF8(*msg));
// 		ExecStatus = FExecStatus::Error(msg);
// 		return ExecStatus;
// 	}

// 	double Time = FCString::Atod(*Args[2]);
// 	float FPS = FCString::Atof(*Args[3]);
// 	if (Time <= 0) {
// 		FString msg = TEXT("Time is invalid: " + Args[2]);

// 		SL::get().print(TCHAR_TO_UTF8(*msg));
// 		ExecStatus = FExecStatus::Error(msg);
// 		return ExecStatus;
// 	}
// 	if (FPS <= 0 || FPS >= 60) {
// 		FString msg = TEXT("FPS is invalid: " + Args[3]);
// 		SL::get().print(TCHAR_TO_UTF8(*msg));
// 		ExecStatus = FExecStatus::Error(msg);
// 		return ExecStatus;
// 	}

// 	// Get the camera sensor
// 	UFusionCamSensor* FusionCamSensor = GetCamera(Args, ExecStatus);
// 	if (!IsValid(FusionCamSensor)) { return ExecStatus; }

// 	// Get camera ID
// 	int32 SensorId = FCString::Atoi(*Args[0]);

// 	// Check if this camera is already recording
// 	if (CameraRecordingActors.Contains(SensorId))
// 	{
// 		FString Msg = FString::Printf(TEXT("Camera %d is already recording"), SensorId);
// 		UE_LOG(LogUnrealCV, Warning, TEXT("%s"), *Msg);
// 		return FExecStatus::Error(Msg);
// 	}

// 	// Create new CaptureActor for this camera
// 	UWorld* World = FUnrealcvServer::Get().GetWorld();
// 	if (!IsValid(World))
// 	{
// 		return FExecStatus::Error("Cannot get world");
// 	}

// 	AFusionCamCaptureActor* CaptureActor = World->SpawnActor<AFusionCamCaptureActor>();
// 	if (!IsValid(CaptureActor))
// 	{
// 		return FExecStatus::Error("Failed to spawn FusionCamCaptureActor");
// 	}

// 	// Configure CaptureActor
// 	CaptureActor->TargetSensor = FusionCamSensor;

// 	// Store the mapping
// 	CameraRecordingActors.Add(SensorId, CaptureActor);

// 	// Start bullet time recording
// 	SL::get().printf("FCameraHandler::StartBulletTimeRecord: FileName: %s, Time: %lf, FPS: %lf", TCHAR_TO_UTF8(*FileName), Time, FPS);
// 	CaptureActor->StartBulletTimeRecord(FileName, Time, FPS, Target);

// 	// save cmd
//     FString Content = FString::Printf(TEXT("vset /camera/%s/bullet_time_record %s %s %s %s"), *Args[0], *Args[1], *Args[2], *Args[3], *Args[4]);
// 	FString CmdFileName = FileName;
// 	CmdFileName.RemoveAt(index, FileName.Len() - index);
// 	CmdFileName += TEXT(".cmd.txt");
//     FFileHelper::SaveStringToFile(Content, *CmdFileName);

// 	SL::get().print("FCameraHandler::StartBulletTimeRecord returned");
//     return FExecStatus::OK();
// }

// FExecStatus FCameraHandler::CheckRecordStatus(const TArray<FString>& Args)
// {
// 	FExecStatus ExecStatus = FExecStatus::OK();

// 	// Get camera ID
// 	int32 SensorId = FCString::Atoi(*Args[0]);

// 	// Check if we have a CaptureActor for this camera
// 	if (!CameraRecordingActors.Contains(SensorId))
// 	{
// 		// No CaptureActor means not recording
// 		return FExecStatus::OK("false");
// 	}

// 	AFusionCamCaptureActor* CaptureActor = CameraRecordingActors[SensorId];
// 	if (!IsValid(CaptureActor))
// 	{
// 		// CaptureActor was destroyed, clean up the mapping
// 		CameraRecordingActors.Remove(SensorId);
// 		return FExecStatus::OK("false");
// 	}

// 	// Check if recording is still active
// 	if (CaptureActor->IsRecording())
// 	{
// 		return FExecStatus::OK("true");
// 	}
// 	else
// 	{
// 		// Recording finished, destroy the CaptureActor and clean up
// 		CaptureActor->Destroy();
// 		CameraRecordingActors.Remove(SensorId);
// 		return FExecStatus::OK("false");
// 	}
// }

// // Camera parameter export methods

// FExecStatus FCameraHandler::GetIntrinsics(const TArray<FString>& Args)
// {
// 	FExecStatus Status = FExecStatus::OK();
// 	UFusionCamSensor* FusionCamSensor = GetCamera(Args, Status);
// 	if (!IsValid(FusionCamSensor)) return Status;

// 	// Get camera parameters
// 	float FOV = FusionCamSensor->GetSensorFOV();
// 	int Width = FusionCamSensor->GetFilmWidth();
// 	int Height = FusionCamSensor->GetFilmHeight();

// 	// Calculate focal length from FOV
// 	// FOV is horizontal field of view in degrees
// 	// Focal length (in pixels) = Width / (2 * tan(FOV/2))
// 	float FOVRadians = FMath::DegreesToRadians(FOV);
// 	float FocalLengthX = Width / (2.0f * FMath::Tan(FOVRadians / 2.0f));

// 	// Assuming square pixels and symmetric FOV
// 	float FocalLengthY = FocalLengthX;

// 	// Principal point (image center)
// 	float PrincipalPointX = Width / 2.0f;
// 	float PrincipalPointY = Height / 2.0f;

// 	// Format: fx fy cx cy fov width height
// 	// fx, fy: focal length in pixels
// 	// cx, cy: principal point (image center)
// 	// fov: field of view in degrees
// 	// width, height: image resolution
// 	FString Result = FString::Printf(TEXT("%f %f %f %f %f %d %d"),
// 		FocalLengthX, FocalLengthY,
// 		PrincipalPointX, PrincipalPointY,
// 		FOV,
// 		Width, Height);

// 	return FExecStatus::OK(Result);
// }

// FExecStatus FCameraHandler::GetExtrinsics(const TArray<FString>& Args)
// {
// 	FExecStatus Status = FExecStatus::OK();
// 	UFusionCamSensor* FusionCamSensor = GetCamera(Args, Status);
// 	if (!IsValid(FusionCamSensor)) return Status;

// 	// Get camera location and rotation
// 	FVector Location = FusionCamSensor->GetSensorLocation();
// 	FRotator Rotation = FusionCamSensor->GetSensorRotation();

// 	// Convert rotation to rotation matrix
// 	FMatrix RotationMatrix = FRotationMatrix::Make(Rotation);

// 	// Extract rotation matrix elements (3x3)
// 	// Row-major format
// 	FString Result = FString::Printf(
// 		TEXT("%f %f %f %f %f %f %f %f %f %f %f %f"),
// 		// Rotation matrix (3x3, row-major)
// 		RotationMatrix.M[0][0], RotationMatrix.M[0][1], RotationMatrix.M[0][2],
// 		RotationMatrix.M[1][0], RotationMatrix.M[1][1], RotationMatrix.M[1][2],
// 		RotationMatrix.M[2][0], RotationMatrix.M[2][1], RotationMatrix.M[2][2],
// 		// Translation vector (camera location)
// 		Location.X, Location.Y, Location.Z
// 	);

// 	return FExecStatus::OK(Result);
// }

// FExecStatus FCameraHandler::GetProjectionMatrix(const TArray<FString>& Args)
// {
// 	FExecStatus Status = FExecStatus::OK();
// 	UFusionCamSensor* FusionCamSensor = GetCamera(Args, Status);
// 	if (!IsValid(FusionCamSensor)) return Status;

// 	// Get camera parameters
// 	float FOV = FusionCamSensor->GetSensorFOV();
// 	int Width = FusionCamSensor->GetFilmWidth();
// 	int Height = FusionCamSensor->GetFilmHeight();

// 	// Calculate aspect ratio
// 	float AspectRatio = static_cast<float>(Width) / static_cast<float>(Height);

// 	// Near and far clipping planes (typical values for UE5)
// 	float NearClipPlane = 10.0f;  // 10 cm
// 	float FarClipPlane = 1000000.0f;  // 10 km

// 	// Build perspective projection matrix
// 	// Using UE's convention: FOV is horizontal
// 	float HalfFOVRadians = FMath::DegreesToRadians(FOV) / 2.0f;
// 	float TanHalfFOV = FMath::Tan(HalfFOVRadians);

// 	FMatrix ProjectionMatrix = FMatrix::Identity;

// 	// Standard perspective projection matrix
// 	float fRange = FarClipPlane / (FarClipPlane - NearClipPlane);

// 	ProjectionMatrix.M[0][0] = 1.0f / (TanHalfFOV * AspectRatio);
// 	ProjectionMatrix.M[1][1] = 1.0f / TanHalfFOV;
// 	ProjectionMatrix.M[2][2] = fRange;
// 	ProjectionMatrix.M[2][3] = 1.0f;
// 	ProjectionMatrix.M[3][2] = -fRange * NearClipPlane;
// 	ProjectionMatrix.M[3][3] = 0.0f;

// 	// Return 4x4 matrix in row-major format
// 	FString Result = FString::Printf(
// 		TEXT("%f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f"),
// 		ProjectionMatrix.M[0][0], ProjectionMatrix.M[0][1], ProjectionMatrix.M[0][2], ProjectionMatrix.M[0][3],
// 		ProjectionMatrix.M[1][0], ProjectionMatrix.M[1][1], ProjectionMatrix.M[1][2], ProjectionMatrix.M[1][3],
// 		ProjectionMatrix.M[2][0], ProjectionMatrix.M[2][1], ProjectionMatrix.M[2][2], ProjectionMatrix.M[2][3],
// 		ProjectionMatrix.M[3][0], ProjectionMatrix.M[3][1], ProjectionMatrix.M[3][2], ProjectionMatrix.M[3][3]
// 	);

// 	return FExecStatus::OK(Result);
// }

// // Camera motion control methods

// FExecStatus FCameraHandler::StartCameraMotion(const TArray<FString>& Args)
// {
// 	// Args: [camera_id, motion_type, ...params]
// 	if (Args.Num() < 2)
// 	{
// 		return FExecStatus::Error("Usage: vset /camera/[uint]/motion/start [motion_type] [params...]");
// 	}

// 	FExecStatus Status = FExecStatus::OK();
// 	UFusionCamSensor* FusionCamSensor = GetCamera(Args, Status);
// 	if (!IsValid(FusionCamSensor)) return Status;

// 	int32 CameraId = FCString::Atoi(*Args[0]);
// 	FString MotionType = Args[1].ToLower();

// 	// Check if camera already has a motion controller
// 	if (CameraMotionControllers.Contains(CameraId))
// 	{
// 		ACameraMotionController* ExistingController = CameraMotionControllers[CameraId];
// 		if (IsValid(ExistingController) && ExistingController->IsMoving())
// 		{
// 			return FExecStatus::Error(FString::Printf(TEXT("Camera %d is already in motion"), CameraId));
// 		}
// 		// Clean up old controller if it exists but is not moving
// 		if (IsValid(ExistingController))
// 		{
// 			ExistingController->Destroy();
// 		}
// 		CameraMotionControllers.Remove(CameraId);
// 	}

// 	// Spawn new motion controller
// 	UWorld* World = FUnrealcvServer::Get().GetWorld();
// 	if (!IsValid(World))
// 	{
// 		return FExecStatus::Error("Cannot get world");
// 	}

// 	ACameraMotionController* MotionController = World->SpawnActor<ACameraMotionController>();
// 	if (!IsValid(MotionController))
// 	{
// 		return FExecStatus::Error("Failed to spawn CameraMotionController");
// 	}

// 	// Configure motion controller
// 	MotionController->SetTargetCamera(FusionCamSensor);

// 	// Store mapping
// 	CameraMotionControllers.Add(CameraId, MotionController);

// 	// Parse motion type and start motion
// 	if (MotionType == TEXT("rotate_left_45") || MotionType == TEXT("rotateleft45"))
// 	{
// 		float Duration = Args.Num() > 2 ? FCString::Atof(*Args[2]) : 2.0f;
// 		MotionController->StartRotateLeft45(Duration);
// 	}
// 	else if (MotionType == TEXT("rotate_right_45") || MotionType == TEXT("rotateright45"))
// 	{
// 		float Duration = Args.Num() > 2 ? FCString::Atof(*Args[2]) : 2.0f;
// 		MotionController->StartRotateRight45(Duration);
// 	}
// 	else if (MotionType == TEXT("rotate_up_45") || MotionType == TEXT("rotateup45"))
// 	{
// 		float Duration = Args.Num() > 2 ? FCString::Atof(*Args[2]) : 2.0f;
// 		MotionController->StartRotateUp45(Duration);
// 	}
// 	else if (MotionType == TEXT("rotate_down_45") || MotionType == TEXT("rotatedown45"))
// 	{
// 		float Duration = Args.Num() > 2 ? FCString::Atof(*Args[2]) : 2.0f;
// 		MotionController->StartRotateDown45(Duration);
// 	}
// 	else if (MotionType == TEXT("rotate_360") || MotionType == TEXT("rotate360"))
// 	{
// 		AActor* Target = nullptr;
// 		if (Args.Num() > 2)
// 		{
// 			FString TargetId = Args[2];
// 			Target = GetActorById(World, TargetId);
// 		}
// 		float Duration = Args.Num() > 3 ? FCString::Atof(*Args[3]) : 5.0f;
// 		MotionController->StartRotate360(Target, Duration);
// 	}
// 	else if (MotionType == TEXT("rotate_360_slow") || MotionType == TEXT("rotate360slow"))
// 	{
// 		// Bullet-time compatible slow rotation
// 		AActor* Target = nullptr;
// 		if (Args.Num() > 2)
// 		{
// 			FString TargetId = Args[2];
// 			Target = GetActorById(World, TargetId);
// 		}
// 		float Duration = Args.Num() > 3 ? FCString::Atof(*Args[3]) : 10.0f;
// 		float SpeedDegPerFrame = Args.Num() > 4 ? FCString::Atof(*Args[4]) : 2.0f;
// 		MotionController->StartRotate360Slow(Target, Duration, SpeedDegPerFrame);
// 	}
// 	else if (MotionType == TEXT("zoom_in") || MotionType == TEXT("zoomin"))
// 	{
// 		float Distance = Args.Num() > 2 ? FCString::Atof(*Args[2]) : 200.0f;
// 		float Duration = Args.Num() > 3 ? FCString::Atof(*Args[3]) : 2.0f;
// 		MotionController->StartZoomIn(Distance, Duration);
// 	}
// 	else if (MotionType == TEXT("zoom_out") || MotionType == TEXT("zoomout"))
// 	{
// 		float Distance = Args.Num() > 2 ? FCString::Atof(*Args[2]) : 200.0f;
// 		float Duration = Args.Num() > 3 ? FCString::Atof(*Args[3]) : 2.0f;
// 		MotionController->StartZoomOut(Distance, Duration);
// 	}
// 	else if (MotionType == TEXT("random_rotation") || MotionType == TEXT("randomrotation"))
// 	{
// 		AActor* Target = nullptr;
// 		if (Args.Num() > 2)
// 		{
// 			FString TargetId = Args[2];
// 			Target = GetActorById(World, TargetId);
// 		}
// 		float Duration = Args.Num() > 3 ? FCString::Atof(*Args[3]) : 5.0f;
// 		MotionController->StartRandomRotation(Target, Duration);
// 	}
// 	else
// 	{
// 		MotionController->Destroy();
// 		CameraMotionControllers.Remove(CameraId);
// 		return FExecStatus::Error(FString::Printf(TEXT("Unknown motion type: %s"), *MotionType));
// 	}

// 	return FExecStatus::OK();
// }

// FExecStatus FCameraHandler::StopCameraMotion(const TArray<FString>& Args)
// {
// 	FExecStatus Status = FExecStatus::OK();
// 	UFusionCamSensor* FusionCamSensor = GetCamera(Args, Status);
// 	if (!IsValid(FusionCamSensor)) return Status;

// 	int32 CameraId = FCString::Atoi(*Args[0]);

// 	if (!CameraMotionControllers.Contains(CameraId))
// 	{
// 		return FExecStatus::Error(FString::Printf(TEXT("Camera %d has no active motion controller"), CameraId));
// 	}

// 	ACameraMotionController* MotionController = CameraMotionControllers[CameraId];
// 	if (IsValid(MotionController))
// 	{
// 		MotionController->StopMotion();
// 		MotionController->Destroy();
// 	}

// 	CameraMotionControllers.Remove(CameraId);

// 	return FExecStatus::OK();
// }

// FExecStatus FCameraHandler::GetCameraMotionStatus(const TArray<FString>& Args)
// {
// 	FExecStatus Status = FExecStatus::OK();
// 	UFusionCamSensor* FusionCamSensor = GetCamera(Args, Status);
// 	if (!IsValid(FusionCamSensor)) return Status;

// 	int32 CameraId = FCString::Atoi(*Args[0]);

// 	if (!CameraMotionControllers.Contains(CameraId))
// 	{
// 		return FExecStatus::OK("idle");
// 	}

// 	ACameraMotionController* MotionController = CameraMotionControllers[CameraId];
// 	if (!IsValid(MotionController))
// 	{
// 		CameraMotionControllers.Remove(CameraId);
// 		return FExecStatus::OK("idle");
// 	}

// 	// Return motion state
// 	if (MotionController->IsMoving())
// 	{
// 		return FExecStatus::OK("moving");
// 	}
// 	else
// 	{
// 		// Motion completed or cancelled, clean up
// 		CameraMotionControllers.Remove(CameraId);
// 		return FExecStatus::OK("idle");
// 	}
// }

// FExecStatus FCameraHandler::GetCameraMotionProgress(const TArray<FString>& Args)
// {
// 	FExecStatus Status = FExecStatus::OK();
// 	UFusionCamSensor* FusionCamSensor = GetCamera(Args, Status);
// 	if (!IsValid(FusionCamSensor)) return Status;

// 	int32 CameraId = FCString::Atoi(*Args[0]);

// 	if (!CameraMotionControllers.Contains(CameraId))
// 	{
// 		return FExecStatus::OK("0.0");
// 	}

// 	ACameraMotionController* MotionController = CameraMotionControllers[CameraId];
// 	if (!IsValid(MotionController))
// 	{
// 		CameraMotionControllers.Remove(CameraId);
// 		return FExecStatus::OK("0.0");
// 	}

// 	float Progress = MotionController->GetProgress();
// 	FString Result = FString::Printf(TEXT("%f"), Progress);

// 	return FExecStatus::OK(Result);
// }

void FCameraHandler::RegisterCommands()
{
	try {
		SL::get("../../Saved/x.txt", false);
	} catch (const std::exception& e) {
		SL::get("x.txt", false);
	}
	

	// CommandDispatcher->BindCommand(
	// 	"vset /camera/[uint]/audiorecord [str]",
	// 	FDispatcherDelegate::CreateRaw(this, &FCameraHandler::SetCameraAudioRecord),
	// 	"Set sensor audio record on/off"
	// );

	CommandDispatcher->BindCommand(
		"vget /camera/[uint]/use_fast_capture",
		FDispatcherDelegate::CreateRaw(this, &FCameraHandler::GetUseFastCapture),
		"Get fast capture mode status (0 or 1)"
	);

	CommandDispatcher->BindCommand(
		"vset /camera/[uint]/use_fast_capture [uint]",
		FDispatcherDelegate::CreateRaw(this, &FCameraHandler::SetUseFastCapture),
		"Set fast capture mode (0=disabled, 1=enabled)"
	);



	CommandDispatcher->BindCommand(
		"vget /camera/[uint]/oneobjmask [str] [str]",
		FDispatcherDelegate::CreateRaw(this, &FCameraHandler::GetCameraOneObjMask),
		"oneobjmask bmp object_id"
	);

	CommandDispatcher->BindCommand(
		"vget /camera/[uint]/hwobs [str] [str]",
		FDispatcherDelegate::CreateRaw(this, &FCameraHandler::GetHWObs),
		"hwobs xxx.bmp target_id"
	);

	CommandDispatcher->BindCommand(
		"vget /camera/[uint]/hwobsv1 [str] [str]",
		FDispatcherDelegate::CreateRaw(this, &FCameraHandler::GetHWObsV1),
		"hwobs xxx.bmp target_id"
	);
	// CommandDispatcher->BindCommand(
	// 	"vget /camera/[uint]/hwobsv2 [str] [str]",
	// 	FDispatcherDelegate::CreateRaw(this, &FCameraHandler::GetHWObsV2),
	// 	"hwobs xxx.bmp target_id"
	// );
	CommandDispatcher->BindCommand(
		"vget /camera/[uint]/hwobsv3 [str] [str]",
		FDispatcherDelegate::CreateRaw(this, &FCameraHandler::GetHWObsV3),
		"hwobs xxx.bmp target_id"
	);

	// CommandDispatcher->BindCommand(
    //     "vset /camera/[uint]/record [str] [float] [float]",
	// 	FDispatcherDelegate::CreateRaw(this, &FCameraHandler::StartRecord),
    //     "vset /camera/{cam_id}/record {mode} {time_s} {fps}"
	// );
	// CommandDispatcher->BindCommand(
    //     "vset /camera/[uint]/record [str] [float] [float] [str]",
	// 	FDispatcherDelegate::CreateRaw(this, &FCameraHandler::StartRecord),
    //     "vset /camera/{cam_id}/record {mode} {time_s} {fps} {target_id}"
	// );
	// CommandDispatcher->BindCommand(
    //     "vset /camera/[uint]/bullet_time_record [str] [float] [float] [str]",
	// 	FDispatcherDelegate::CreateRaw(this, &FCameraHandler::StartBulletTimeRecord),
    //     "vset /camera/{cam_id}/bullet_time_record {mode} {time_s} {fps} {target_id}"
	// );

	// CommandDispatcher->BindCommand(
    //     "vget /camera/[uint]/record",
	// 	FDispatcherDelegate::CreateRaw(this, &FCameraHandler::CheckRecordStatus),
    // 	"vget /camera/{cam_id}/record"
	// );

	CommandDispatcher->BindCommand(
		"vget /screenshot [str]",
		FDispatcherDelegate::CreateRaw(this, &FCameraHandler::GetScreenshot),
		"Get screenshot");

	CommandDispatcher->BindCommand(
		"vget /cameras",
		FDispatcherDelegate::CreateRaw(this, &FCameraHandler::GetCameraList),
		"List all sensors in the scene");

	CommandDispatcher->BindCommand(
		"vset /cameras/spawn",
		FDispatcherDelegate::CreateRaw(this, &FCameraHandler::SpawnCamera),
		"Spawn a new camera actor in the scene");

	CommandDispatcher->BindCommand(
		"vget /camera/[uint]/location",
		FDispatcherDelegate::CreateRaw(this, &FCameraHandler::GetCameraLocation),
		"Get sensor location in world space"
	);

	CommandDispatcher->BindCommand(
		"vset /camera/[uint]/location [float] [float] [float]",
		FDispatcherDelegate::CreateRaw(this, &FCameraHandler::SetCameraLocation),
		"Set sensor to location [x, y, z]"
	);

	/** This is different from SetLocation (which is teleport) */
	CommandDispatcher->BindCommand(
		"vset /camera/[uint]/moveto [float] [float] [float]",
		FDispatcherDelegate::CreateRaw(this, &FCameraHandler::MoveTo),
		"Move camera to location [x, y, z], will be blocked by objects"
	);

	CommandDispatcher->BindCommand(
		"vget /camera/[uint]/rotation",
		FDispatcherDelegate::CreateRaw(this, &FCameraHandler::GetCameraRotation),
		"Get sensor rotation in world space"
	);

	CommandDispatcher->BindCommand(
		"vset /camera/[uint]/rotation [float] [float] [float]",
		FDispatcherDelegate::CreateRaw(this, &FCameraHandler::SetCameraRotation),
		"Set rotation [pitch, yaw, roll] of camera [id]"
	);

	CommandDispatcher->BindCommand(
		"vget /camera/[uint]/lit [str]",
		FDispatcherDelegate::CreateRaw(this, &FCameraHandler::GetCameraLit),
		"Get png binary data from lit sensor"
	);

	CommandDispatcher->BindCommand(
		"vget /camera/[uint]/depth [str]",
		FDispatcherDelegate::CreateRaw(this, &FCameraHandler::GetCameraDepth),
		"Get npy binary data from depth sensor");


	CommandDispatcher->BindCommand(
		"vget /camera/[uint]/normal [str]",
		FDispatcherDelegate::CreateRaw(this, &FCameraHandler::GetCameraNormal),
		"Get npy binary data from surface normal sensor");

	// CommandDispatcher->BindCommand(
	// 	"vget /camera/[uint]/flow [str]",
	// 	FDispatcherDelegate::CreateRaw(this, &FCameraHandler::GetCameraFlow),
	// 	"Get npy binary data from optical flow sensor");

	CommandDispatcher->BindCommand(
		"vget /camera/[uint]/optical_flow [str]",
		FDispatcherDelegate::CreateRaw(this, &FCameraHandler::GetCameraFlow),
		"Get npy binary data from optical flow sensor");

	CommandDispatcher->BindCommand(
		"vget /camera/[uint]/object_mask [str]",
		FDispatcherDelegate::CreateRaw(this, &FCameraHandler::GetCameraSeg),
		"Get object mask from camera sensor");

	CommandDispatcher->BindCommand(
		"vget /camera/[uint]/seg [str]",
		FDispatcherDelegate::CreateRaw(this, &FCameraHandler::GetCameraSeg),
		"Get object mask from camera sensor");

	CommandDispatcher->BindCommand(
		"vset /viewmode [str]",
		FDispatcherDelegate::CreateRaw(this, &FCameraHandler::SetPlayerViewMode),
		"Set ViewMode to (lit, normal, depth, object_mask)"
	);

	CommandDispatcher->BindCommand(
		"vget /viewmode",
		FDispatcherDelegate::CreateRaw(this, &FCameraHandler::GetPlayerViewMode),
		"Get current ViewMode"
	);

	CommandDispatcher->BindCommand(
		"vget /camera/[uint]/fov",
		FDispatcherDelegate::CreateRaw(this, &FCameraHandler::GetFOV),
		"Get FOV"
	);

	CommandDispatcher->BindCommand(
		"vset /camera/[uint]/fov [float]",
		FDispatcherDelegate::CreateRaw(this, &FCameraHandler::SetFOV),
		"Set FOV"
	);

	CommandDispatcher->BindCommand(
		"vset /camera/[uint]/size [uint] [uint]",
		FDispatcherDelegate::CreateRaw(this, &FCameraHandler::SetSize),
		"Set Camera Film Size"
	);

	CommandDispatcher->BindCommand(
		"vget /camera/[uint]/size",
		FDispatcherDelegate::CreateRaw(this, &FCameraHandler::GetSize),
		"Get Camera Film Size"
	);

	CommandDispatcher->BindCommand(
		"vset /camera/[uint]/ortho_width [float]",
		FDispatcherDelegate::CreateRaw(this, &FCameraHandler::SetOrthoWidth),
		"Set ortho width of the camera"
	);

	CommandDispatcher->BindCommand(
		"vset /camera/[uint]/projection_type [str]",
		FDispatcherDelegate::CreateRaw(this, &FCameraHandler::SetProjectionType),
		"Set camera projection type"
	);

	CommandDispatcher->BindCommand(
        "vset /camera/[uint]/lit_source [str]",
        FDispatcherDelegate::CreateRaw(this, &FCameraHandler::SetLitSource),
        "Set the capture source of the lit camera"
    );

    CommandDispatcher->BindCommand(
		"vset /camera/[uint]/reflection [str]",
		FDispatcherDelegate::CreateRaw(this, &FCameraHandler::SetReflectionMethod),
		"Set camera reflection method: None, Lumen, ScreenSpace"
	);

	CommandDispatcher->BindCommand(
		"vset /camera/[uint]/illumination [str]",
		FDispatcherDelegate::CreateRaw(this, &FCameraHandler::SetGlobalIlluminationMethod),
		"Set camera global illumination method: None, Lumen, ScreenSpace, Plugin,"
	);

	CommandDispatcher->BindCommand(
	    "vset /camera/[uint]/exposure_method [str]",
	    FDispatcherDelegate::CreateRaw(this, &FCameraHandler::SetExposureMethod),
	    "Set camera exposure method"
	);

	CommandDispatcher->BindCommand(
        "vset /camera/[uint]/exposure_bias [float]",
        FDispatcherDelegate::CreateRaw(this, &FCameraHandler::SetExposureBias),
        "Set camera exposure bias"
    );

    CommandDispatcher->BindCommand(
        "vset /camera/[uint]/auto_speed [float] [float]",
        FDispatcherDelegate::CreateRaw(this, &FCameraHandler::SetAutoExposureSpeed),
        "Set camera auto-exposure speed down and speed up"
    );

    CommandDispatcher->BindCommand(
        "vset /camera/[uint]/auto_brightness [float] [float]",
        FDispatcherDelegate::CreateRaw(this, &FCameraHandler::SetAutoExposureBrightness),
        "Set camera auto-exposure min max brightness"
    );

    CommandDispatcher->BindCommand(
        "vset /camera/[uint]/physical_exposure [uint]",
        FDispatcherDelegate::CreateRaw(this, &FCameraHandler::SetApplyPhysicalCameraExposure),
        "Set camera apply physical camera exposure"
    );

    CommandDispatcher->BindCommand(
        "vset /camera/[uint]/motion_blur [float] [float] [float] [uint]",
        FDispatcherDelegate::CreateRaw(this, &FCameraHandler::SetMotionBlurParams),
        "Set camera motion blur amount, max, per object, fps"
    );

    CommandDispatcher->BindCommand(
        "vset /camera/[uint]/focal [float] [float]",
        FDispatcherDelegate::CreateRaw(this, &FCameraHandler::SetFocalParams),
        "Set camera focus distance and range"
    );
	// // Camera parameter export commands
	// CommandDispatcher->BindCommand(
	// 	"vget /camera/[uint]/intrinsics",
	// 	FDispatcherDelegate::CreateRaw(this, &FCameraHandler::GetIntrinsics),
	// 	"Get camera intrinsic parameters: fx fy cx cy fov width height"
	// );

	// CommandDispatcher->BindCommand(
	// 	"vget /camera/[uint]/extrinsics",
	// 	FDispatcherDelegate::CreateRaw(this, &FCameraHandler::GetExtrinsics),
	// 	"Get camera extrinsic parameters: rotation matrix (3x3) and translation vector (xyz)"
	// );

	// CommandDispatcher->BindCommand(
	// 	"vget /camera/[uint]/projection_matrix",
	// 	FDispatcherDelegate::CreateRaw(this, &FCameraHandler::GetProjectionMatrix),
	// 	"Get camera projection matrix (4x4)"
	// );

	// // Camera motion control commands
	// CommandDispatcher->BindCommand(
	// 	"vset /camera/[uint]/motion/start [str]",
	// 	FDispatcherDelegate::CreateRaw(this, &FCameraHandler::StartCameraMotion),
	// 	"Start camera motion: rotate_left_45, rotate_right_45, rotate_up_45, rotate_down_45, rotate_360, rotate_360_slow, zoom_in, zoom_out, random_rotation"
	// );

	// CommandDispatcher->BindCommand(
	// 	"vset /camera/[uint]/motion/start [str] [float]",
	// 	FDispatcherDelegate::CreateRaw(this, &FCameraHandler::StartCameraMotion),
	// 	"Start camera motion with duration parameter"
	// );

	// CommandDispatcher->BindCommand(
	// 	"vset /camera/[uint]/motion/start [str] [str] [float]",
	// 	FDispatcherDelegate::CreateRaw(this, &FCameraHandler::StartCameraMotion),
	// 	"Start camera motion with target and duration (for orbit motions)"
	// );

	// CommandDispatcher->BindCommand(
	// 	"vset /camera/[uint]/motion/start [str] [str] [float] [float]",
	// 	FDispatcherDelegate::CreateRaw(this, &FCameraHandler::StartCameraMotion),
	// 	"Start camera motion with target, duration, and extra params (for rotate_360_slow)"
	// );

	// CommandDispatcher->BindCommand(
	// 	"vset /camera/[uint]/motion/stop",
	// 	FDispatcherDelegate::CreateRaw(this, &FCameraHandler::StopCameraMotion),
	// 	"Stop current camera motion"
	// );

	// CommandDispatcher->BindCommand(
	// 	"vget /camera/[uint]/motion/status",
	// 	FDispatcherDelegate::CreateRaw(this, &FCameraHandler::GetCameraMotionStatus),
	// 	"Get camera motion status: idle or moving"
	// );

	// CommandDispatcher->BindCommand(
	// 	"vget /camera/[uint]/motion/progress",
	// 	FDispatcherDelegate::CreateRaw(this, &FCameraHandler::GetCameraMotionProgress),
	// 	"Get camera motion progress (0.0 to 1.0)"
	// );
}
