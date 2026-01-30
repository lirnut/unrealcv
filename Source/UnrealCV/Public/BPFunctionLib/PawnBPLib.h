#pragma once

#include "Runtime/Engine/Classes/Kismet/BlueprintFunctionLibrary.h"
#include "PawnBPLib.generated.h"

UCLASS()
class UNREALCV_API UPawnBPLib : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Pawn")
	static bool SetPawnLocation(UObject* WorldContextObject, FVector Location, bool bSweep = false);

	UFUNCTION(BlueprintPure, Category = "UnrealCV|Pawn")
	static FVector GetPawnLocation(UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Pawn")
	static bool SetPawnRotation(UObject* WorldContextObject, FRotator Rotation);

	UFUNCTION(BlueprintPure, Category = "UnrealCV|Pawn")
	static FRotator GetPawnRotation(UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "UnrealCV|Pawn")
	static APawn* GetPlayerPawn(UObject* WorldContextObject);
};
