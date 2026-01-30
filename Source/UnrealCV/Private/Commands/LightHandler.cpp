#include "LightHandler.h"
#include "UnrealcvServer.h"
#include "LightBPLib.h"
#include "Utils/StrFormatter.h"

void FLightHandler::RegisterCommands()
{
	CommandDispatcher->BindCommand(
		"vget /light/directional/intensity",
		FDispatcherDelegate::CreateRaw(this, &FLightHandler::GetDirectionalLightIntensity),
		"Get DirectionalLight intensity"
	);

	CommandDispatcher->BindCommand(
		"vset /light/directional/intensity [float]",
		FDispatcherDelegate::CreateRaw(this, &FLightHandler::SetDirectionalLightIntensity),
		"Set DirectionalLight intensity"
	);

	CommandDispatcher->BindCommand(
		"vget /light/skylight/intensity",
		FDispatcherDelegate::CreateRaw(this, &FLightHandler::GetSkyLightIntensity),
		"Get SkyLight intensity"
	);

	CommandDispatcher->BindCommand(
		"vset /light/skylight/intensity [float]",
		FDispatcherDelegate::CreateRaw(this, &FLightHandler::SetSkyLightIntensity),
		"Set SkyLight intensity"
	);
}

FExecStatus FLightHandler::GetDirectionalLightIntensity(const TArray<FString>& Args)
{
	UWorld* World = FUnrealcvServer::Get().GetWorld();
	if (!World)
	{
		return FExecStatus::Error(TEXT("Cannot get world"));
	}

	float Intensity = ULightBPLib::GetDirectionalLightIntensity(World);
	if (Intensity < 0.0f)
	{
		return FExecStatus::Error(TEXT("Failed to get DirectionalLight intensity"));
	}

	return FExecStatus::OK(FString::Printf(TEXT("%.2f"), Intensity));
}

FExecStatus FLightHandler::SetDirectionalLightIntensity(const TArray<FString>& Args)
{
	UWorld* World = FUnrealcvServer::Get().GetWorld();
	if (!World)
	{
		return FExecStatus::Error(TEXT("Cannot get world"));
	}

	float Intensity = FCString::Atof(*Args[0]);
	bool bSuccess = ULightBPLib::SetDirectionalLightIntensity(World, Intensity);

	if (bSuccess)
	{
		return FExecStatus::OK(FString::Printf(TEXT("Set DirectionalLight intensity to %.2f"), Intensity));
	}
	else
	{
		return FExecStatus::Error(TEXT("Failed to set DirectionalLight intensity"));
	}
}

FExecStatus FLightHandler::GetSkyLightIntensity(const TArray<FString>& Args)
{
	UWorld* World = FUnrealcvServer::Get().GetWorld();
	if (!World)
	{
		return FExecStatus::Error(TEXT("Cannot get world"));
	}

	float Intensity = ULightBPLib::GetSkyLightIntensity(World);
	if (Intensity < 0.0f)
	{
		return FExecStatus::Error(TEXT("Failed to get SkyLight intensity"));
	}

	return FExecStatus::OK(FString::Printf(TEXT("%.2f"), Intensity));
}

FExecStatus FLightHandler::SetSkyLightIntensity(const TArray<FString>& Args)
{
	UWorld* World = FUnrealcvServer::Get().GetWorld();
	if (!World)
	{
		return FExecStatus::Error(TEXT("Cannot get world"));
	}

	float Intensity = FCString::Atof(*Args[0]);
	bool bSuccess = ULightBPLib::SetSkyLightIntensity(World, Intensity);

	if (bSuccess)
	{
		return FExecStatus::OK(FString::Printf(TEXT("Set SkyLight intensity to %.2f"), Intensity));
	}
	else
	{
		return FExecStatus::Error(TEXT("Failed to set SkyLight intensity"));
	}
}
