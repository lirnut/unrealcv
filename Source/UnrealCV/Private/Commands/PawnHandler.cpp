#include "PawnHandler.h"
#include "UnrealcvServer.h"
#include "PawnBPLib.h"
#include "Utils/StrFormatter.h"

void FPawnHandler::RegisterCommands()
{
	CommandDispatcher->BindCommand(
		"vget /pawn/location",
		FDispatcherDelegate::CreateRaw(this, &FPawnHandler::GetPawnLocation),
		"Get player pawn location [x, y, z]"
	);

	CommandDispatcher->BindCommand(
		"vset /pawn/location [float] [float] [float]",
		FDispatcherDelegate::CreateRaw(this, &FPawnHandler::SetPawnLocation),
		"Set player pawn location [x, y, z]"
	);

	CommandDispatcher->BindCommand(
		"vget /pawn/rotation",
		FDispatcherDelegate::CreateRaw(this, &FPawnHandler::GetPawnRotation),
		"Get player pawn rotation [pitch, yaw, roll]"
	);

	CommandDispatcher->BindCommand(
		"vset /pawn/rotation [float] [float] [float]",
		FDispatcherDelegate::CreateRaw(this, &FPawnHandler::SetPawnRotation),
		"Set player pawn rotation [pitch, yaw, roll]"
	);
}

FExecStatus FPawnHandler::GetPawnLocation(const TArray<FString>& Args)
{
	UWorld* World = FUnrealcvServer::Get().GetWorld();
	if (!World)
	{
		return FExecStatus::Error(TEXT("Cannot get world"));
	}

	FVector Location = UPawnBPLib::GetPawnLocation(World);
	if (Location == FVector::ZeroVector)
	{
		return FExecStatus::Error(TEXT("Failed to get pawn location"));
	}

	FStrFormatter Ar;
	Ar << Location;
	return FExecStatus::OK(Ar.ToString());
}

FExecStatus FPawnHandler::SetPawnLocation(const TArray<FString>& Args)
{
	UWorld* World = FUnrealcvServer::Get().GetWorld();
	if (!World)
	{
		return FExecStatus::Error(TEXT("Cannot get world"));
	}

	if (Args.Num() != 3)
	{
		return FExecStatus::Error(TEXT("Invalid arguments, expected: [x] [y] [z]"));
	}

	float X = FCString::Atof(*Args[0]);
	float Y = FCString::Atof(*Args[1]);
	float Z = FCString::Atof(*Args[2]);
	FVector Location = FVector(X, Y, Z);

	bool bSuccess = UPawnBPLib::SetPawnLocation(World, Location, false);
	if (bSuccess)
	{
		return FExecStatus::OK(FString::Printf(TEXT("Set pawn location to (%.2f, %.2f, %.2f)"), X, Y, Z));
	}
	else
	{
		return FExecStatus::Error(TEXT("Failed to set pawn location"));
	}
}

FExecStatus FPawnHandler::GetPawnRotation(const TArray<FString>& Args)
{
	UWorld* World = FUnrealcvServer::Get().GetWorld();
	if (!World)
	{
		return FExecStatus::Error(TEXT("Cannot get world"));
	}

	FRotator Rotation = UPawnBPLib::GetPawnRotation(World);
	if (Rotation == FRotator::ZeroRotator)
	{
		return FExecStatus::Error(TEXT("Failed to get pawn rotation"));
	}

	FStrFormatter Ar;
	Ar << Rotation;
	return FExecStatus::OK(Ar.ToString());
}

FExecStatus FPawnHandler::SetPawnRotation(const TArray<FString>& Args)
{
	UWorld* World = FUnrealcvServer::Get().GetWorld();
	if (!World)
	{
		return FExecStatus::Error(TEXT("Cannot get world"));
	}

	if (Args.Num() != 3)
	{
		return FExecStatus::Error(TEXT("Invalid arguments, expected: [pitch] [yaw] [roll]"));
	}

	float Pitch = FCString::Atof(*Args[0]);
	float Yaw = FCString::Atof(*Args[1]);
	float Roll = FCString::Atof(*Args[2]);
	FRotator Rotation = FRotator(Pitch, Yaw, Roll);

	bool bSuccess = UPawnBPLib::SetPawnRotation(World, Rotation);
	if (bSuccess)
	{
		return FExecStatus::OK(FString::Printf(TEXT("Set pawn rotation to (%.2f, %.2f, %.2f)"), Pitch, Yaw, Roll));
	}
	else
	{
		return FExecStatus::Error(TEXT("Failed to set pawn rotation"));
	}
}
