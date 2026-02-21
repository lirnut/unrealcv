#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SpawnBPLib.generated.h"

UCLASS()
class UNREALCV_API USpawnBPLib : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Spawn")
	static AActor* SpawnActorFromPath(
		UObject* WorldContextObject,
		const FString& AssetPath,
		FVector Location = FVector::ZeroVector,
		FRotator Rotation = FRotator::ZeroRotator
	);
};
