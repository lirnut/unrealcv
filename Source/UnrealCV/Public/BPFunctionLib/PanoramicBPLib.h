#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PanoramicBPLib.generated.h"

UCLASS()
class UNREALCV_API UPanoramicBPLib : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Panoramic")
	static class UPanoramicCamSensor* SpawnPanoramicCamera(
		UObject* WorldContextObject,
		FVector Location,
		int32 CubemapResolution = 1024
	);

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Panoramic")
	static void CapturePanoramicToFile(
		class UPanoramicCamSensor* PanoramicSensor,
		const FString& OutputPath,
		int32 EquirectWidth = 4096,
		int32 EquirectHeight = 2048
	);

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Panoramic")
	static void GetPanoramicPixelData(
		class UPanoramicCamSensor* PanoramicSensor,
		TArray<FColor>& OutPixelData,
		int32& OutWidth,
		int32& OutHeight,
		int32 EquirectWidth = 4096,
		int32 EquirectHeight = 2048
	);

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Panoramic")
	static void DestroyPanoramicCamera(class UPanoramicCamSensor* PanoramicSensor);
};
