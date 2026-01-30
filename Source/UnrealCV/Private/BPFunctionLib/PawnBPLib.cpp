#include "PawnBPLib.h"
#include "Runtime/Engine/Classes/GameFramework/Pawn.h"
#include "Runtime/Engine/Classes/GameFramework/PlayerController.h"
#include "Runtime/Engine/Classes/GameFramework/Controller.h"
#include "UnrealcvLog.h"

APawn* UPawnBPLib::GetPlayerPawn(UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("GetPlayerPawn: Invalid world context"));
		return nullptr;
	}

	APlayerController* PlayerController = World->GetFirstPlayerController();
	if (!IsValid(PlayerController))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("GetPlayerPawn: Invalid PlayerController"));
		return nullptr;
	}

	APawn* Pawn = PlayerController->GetPawn();
	if (!IsValid(Pawn))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("GetPlayerPawn: Invalid Pawn"));
		return nullptr;
	}

	return Pawn;
}

bool UPawnBPLib::SetPawnLocation(UObject* WorldContextObject, FVector Location, bool bSweep)
{
	APawn* Pawn = GetPlayerPawn(WorldContextObject);
	if (!IsValid(Pawn))
	{
		return false;
	}

	bool bSuccess = Pawn->SetActorLocation(Location, bSweep, nullptr, ETeleportType::TeleportPhysics);
	if (bSuccess)
	{
		UE_LOG(LogUnrealCV, Log, TEXT("SetPawnLocation: Set location to (%f, %f, %f)"),
			Location.X, Location.Y, Location.Z);
	}
	else
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("SetPawnLocation: Failed to set location"));
	}

	return bSuccess;
}

FVector UPawnBPLib::GetPawnLocation(UObject* WorldContextObject)
{
	APawn* Pawn = GetPlayerPawn(WorldContextObject);
	if (!IsValid(Pawn))
	{
		return FVector::ZeroVector;
	}

	FVector Location = Pawn->GetActorLocation();
	UE_LOG(LogUnrealCV, Log, TEXT("GetPawnLocation: Current location is (%f, %f, %f)"),
		Location.X, Location.Y, Location.Z);

	return Location;
}

bool UPawnBPLib::SetPawnRotation(UObject* WorldContextObject, FRotator Rotation)
{
	APawn* Pawn = GetPlayerPawn(WorldContextObject);
	if (!IsValid(Pawn))
	{
		return false;
	}

	AController* Controller = Pawn->GetController();
	if (!IsValid(Controller))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("SetPawnRotation: Invalid Controller"));
		return false;
	}

	Controller->ClientSetRotation(Rotation);
	UE_LOG(LogUnrealCV, Log, TEXT("SetPawnRotation: Set rotation to (Pitch=%f, Yaw=%f, Roll=%f)"),
		Rotation.Pitch, Rotation.Yaw, Rotation.Roll);

	return true;
}

FRotator UPawnBPLib::GetPawnRotation(UObject* WorldContextObject)
{
	APawn* Pawn = GetPlayerPawn(WorldContextObject);
	if (!IsValid(Pawn))
	{
		return FRotator::ZeroRotator;
	}

	AController* Controller = Pawn->GetController();
	if (!IsValid(Controller))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("GetPawnRotation: Invalid Controller"));
		return FRotator::ZeroRotator;
	}

	FRotator Rotation = Controller->GetControlRotation();
	UE_LOG(LogUnrealCV, Log, TEXT("GetPawnRotation: Current rotation is (Pitch=%f, Yaw=%f, Roll=%f)"),
		Rotation.Pitch, Rotation.Yaw, Rotation.Roll);

	return Rotation;
}
