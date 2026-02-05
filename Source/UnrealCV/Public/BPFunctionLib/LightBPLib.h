#pragma once

#include "Runtime/Engine/Classes/Kismet/BlueprintFunctionLibrary.h"
#include "LightBPLib.generated.h"

UCLASS()
class UNREALCV_API ULightBPLib : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Light")
	static bool SetDirectionalLightIntensity(UObject* WorldContextObject, float Intensity);

	UFUNCTION(BlueprintPure, Category = "UnrealCV|Light")
	static float GetDirectionalLightIntensity(UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Light")
	static bool SetSkyLightIntensity(UObject* WorldContextObject, float Intensity);

	UFUNCTION(BlueprintPure, Category = "UnrealCV|Light")
	static float GetSkyLightIntensity(UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Light")
	static bool SetDirectionalLightCastDeepShadow(UObject* WorldContextObject, bool bCastDeepShadow);

	UFUNCTION(BlueprintPure, Category = "UnrealCV|Light")
	static bool GetDirectionalLightCastDeepShadow(UObject* WorldContextObject);
};
