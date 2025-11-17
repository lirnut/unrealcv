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
};
