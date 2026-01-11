#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MetaHumanBPLib.generated.h"

struct FBatchContext
{
	TArray<FString> AllPaths;
	FString AnimBlueprintPath;
	int32 BatchSize;
	int32 CurrentIndex;
	TArray<FString> SuccessfulPaths;
	TArray<AActor*> SpawnedActors;
	bool bIsSpawn;
	bool bCancelled;
	FTimerHandle TimerHandle;
};

UCLASS()
class UNREALCV_API UMetaHumanBPLib : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "UnrealCV|MetaHuman")
	static TArray<FString> GetAllMetaHumanBlueprintPaths();

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|MetaHuman")
	static TArray<FString> FilterBatchGeneratedMetaHumans(const TArray<FString>& AllPaths);

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|MetaHuman")
	static bool SetMetaHumanAnimationBlueprint(const FString& MetaHumanBPPath, const FString& AnimBlueprintPath);

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|MetaHuman")
	static void SetupAllMetaHumansWithAnimation(const FString& AnimBlueprintPath = TEXT("/Game/MetaHumans/ABP_RandomIdle.ABP_RandomIdle_C"));

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|MetaHuman")
	static void SpawnAllMetaHumansToMap(const FString& AnimBlueprintPath = TEXT("/Game/MetaHumans/ABP_RandomIdle.ABP_RandomIdle_C"));

	// UFUNCTION(BlueprintCallable, Category = "UnrealCV|MetaHuman")
	// static void SetupAllMetaHumansWithAnimationAsync(const FString& AnimBlueprintPath = TEXT("/Game/MetaHumans/ABP_RandomIdle.ABP_RandomIdle_C"), int32 BatchSize = 5);

	// UFUNCTION(BlueprintCallable, Category = "UnrealCV|MetaHuman")
	// static void SpawnAllMetaHumansToMapAsync(const FString& AnimBlueprintPath = TEXT("/Game/MetaHumans/ABP_RandomIdle.ABP_RandomIdle_C"), int32 BatchSize = 3);

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|MetaHuman")
	static void CancelAsyncOperation();




	static TUniquePtr<FBatchContext> GBatchContext;

	static void ProcessBatch(FBatchContext* Context);

	static int32 BatchSize;
};
