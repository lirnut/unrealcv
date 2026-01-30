#include "LightBPLib.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/SkyLight.h"
#include "Components/SkyLightComponent.h"
#include "EngineUtils.h"
#include "UnrealcvLog.h"

bool ULightBPLib::SetDirectionalLightIntensity(UObject* WorldContextObject, float Intensity)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("SetDirectionalLightIntensity: Invalid world context"));
		return false;
	}

	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		ADirectionalLight* Light = *It;
		if (Light && Light->GetLightComponent())
		{
			Light->GetLightComponent()->SetIntensity(Intensity);
			UE_LOG(LogUnrealCV, Log, TEXT("SetDirectionalLightIntensity: Set intensity to %f"), Intensity);
			return true;
		}
	}

	UE_LOG(LogUnrealCV, Warning, TEXT("SetDirectionalLightIntensity: No DirectionalLight found in world"));
	return false;
}

float ULightBPLib::GetDirectionalLightIntensity(UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("GetDirectionalLightIntensity: Invalid world context"));
		return -1.0f;
	}

	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		ADirectionalLight* Light = *It;
		if (Light && Light->GetLightComponent())
		{
			float Intensity = Light->GetLightComponent()->Intensity;
			UE_LOG(LogUnrealCV, Log, TEXT("GetDirectionalLightIntensity: Current intensity is %f"), Intensity);
			return Intensity;
		}
	}

	UE_LOG(LogUnrealCV, Warning, TEXT("GetDirectionalLightIntensity: No DirectionalLight found in world"));
	return -1.0f;
}

bool ULightBPLib::SetSkyLightIntensity(UObject* WorldContextObject, float Intensity)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("SetSkyLightIntensity: Invalid world context"));
		return false;
	}

	for (TActorIterator<ASkyLight> It(World); It; ++It)
	{
		ASkyLight* Light = *It;
		if (Light && Light->GetLightComponent())
		{
			Light->GetLightComponent()->SetIntensity(Intensity);
			UE_LOG(LogUnrealCV, Log, TEXT("SetSkyLightIntensity: Set intensity to %f"), Intensity);
			return true;
		}
	}

	UE_LOG(LogUnrealCV, Warning, TEXT("SetSkyLightIntensity: No SkyLight found in world"));
	return false;
}

float ULightBPLib::GetSkyLightIntensity(UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("GetSkyLightIntensity: Invalid world context"));
		return -1.0f;
	}

	for (TActorIterator<ASkyLight> It(World); It; ++It)
	{
		ASkyLight* Light = *It;
		if (Light && Light->GetLightComponent())
		{
			float Intensity = Light->GetLightComponent()->Intensity;
			UE_LOG(LogUnrealCV, Log, TEXT("GetSkyLightIntensity: Current intensity is %f"), Intensity);
			return Intensity;
		}
	}

	UE_LOG(LogUnrealCV, Warning, TEXT("GetSkyLightIntensity: No SkyLight found in world"));
	return -1.0f;
}
