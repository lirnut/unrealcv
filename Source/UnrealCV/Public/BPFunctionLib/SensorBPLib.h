// Weichao Qiu @ 2018
#pragma once

#include "Runtime/Engine/Classes/Kismet/BlueprintFunctionLibrary.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "SensorBPLib.generated.h"

class UFusionCamSensor;

/** Sensor ID Manager - Static singleton for managing new format stable camera IDs (CID_xxxxxxxx) */
class UNREALCV_API FCameraIDManager
{
public:
	static FCameraIDManager& Get();

	// void RegisterSensor(UFusionCamSensor* Sensor);
	// void UnregisterSensor(UFusionCamSensor* Sensor);
	TArray<UFusionCamSensor*> Sync();

	UFusionCamSensor* GetSensorByAnyID(const FString& IDString);
	int32 GetIndexByAnyID(const FString& IDString);

	FString GetNewFormatID(UFusionCamSensor* Sensor);

	void PrintIDMappings() const;

private:
	FCameraIDManager();

	FString GenerateUUID(UFusionCamSensor* Sensor) const;

	TMap<UFusionCamSensor*, FString> SensorToCameraIDMap;
	TSet<FString> UsedCameraIDs;
};

/** Sensor related function library */
UCLASS()
class UNREALCV_API USensorBPLib : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "unrealcv")
	static TArray<UFusionCamSensor*> GetFusionSensorList();

	UFUNCTION(BlueprintPure, Category = "unrealcv")
	static UFusionCamSensor* GetSensorById(int SensorId);

	UFUNCTION(BlueprintPure, Category = "unrealcv")
	static UFusionCamSensor* GetSensorByAnyID(const FString& IDString);
	
	UFUNCTION(BlueprintPure, Category = "unrealcv")
	static int32 GetIndexByAnyID(const FString& IDString);

	UFUNCTION(BlueprintPure, Category = "unrealcv")
	static TArray<FString> GetFusionSensorListWithNewIDs();

	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	static FString GetSensorNewFormatID(UFusionCamSensor* Sensor);

	UFUNCTION(BlueprintPure, Category = "unrealcv")
	static int32 GetSensorOldFormatID(const FString& NewFormatID);

	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	static void PrintCameraIDMappings();
};