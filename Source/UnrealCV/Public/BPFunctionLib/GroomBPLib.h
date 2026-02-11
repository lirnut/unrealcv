#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GroomBPLib.generated.h"

class UGroomComponent;

UCLASS()
class UNREALCV_API UGroomBPLib : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "UnrealCV|Groom")
    static bool SetHairGravity(AActor* Actor, FVector GravityVector);

    UFUNCTION(BlueprintCallable, Category = "UnrealCV|Groom")
    static bool SetHairAirDrag(AActor* Actor, float AirDrag);

    UFUNCTION(BlueprintCallable, Category = "UnrealCV|Groom")
    static bool SetHairAirVelocity(AActor* Actor, FVector AirVelocity);

    UFUNCTION(BlueprintCallable, Category = "UnrealCV|Groom")
    static bool SetHairBendDamping(AActor* Actor, float BendDamping);

    UFUNCTION(BlueprintCallable, Category = "UnrealCV|Groom")
    static bool SetHairBendStiffness(AActor* Actor, float BendStiffness);

    UFUNCTION(BlueprintCallable, Category = "UnrealCV|Groom")
    static bool SetHairStrandsViscosity(AActor* Actor, float StrandsViscosity);

    UFUNCTION(BlueprintCallable, Category = "UnrealCV|Groom")
    static bool DisableHairGravity(AActor* Actor);

    UFUNCTION(BlueprintCallable, Category = "UnrealCV|Groom")
    static bool SetHairSimulationEnabled(AActor* Actor, bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "UnrealCV|Groom")
    static bool ResetHairSimulation(AActor* Actor);

};
