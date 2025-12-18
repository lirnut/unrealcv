#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MetaHumanBPLib.generated.h"

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
	static TArray<FString> SetupAllMetaHumansWithAnimation(const FString& AnimBlueprintPath = TEXT("/Game/MetaHumans/ABP_RandomIdle.ABP_RandomIdle_C"));

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|MetaHuman")
	static TArray<AActor*> SpawnAllMetaHumansToMap(const FString& AnimBlueprintPath = TEXT("/Game/MetaHumans/ABP_RandomIdle.ABP_RandomIdle_C"));
};
