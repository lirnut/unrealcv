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

    UFUNCTION(BlueprintCallable, Category = "UnrealCV|Groom")
    static bool PauseHairSimulation(AActor* Actor);

    UFUNCTION(BlueprintCallable, Category = "UnrealCV|Groom")
    static bool ResumeHairSimulation(AActor* Actor);

    UFUNCTION(BlueprintCallable, Category = "UnrealCV|Groom")
    static bool SetHairSimulationTimeDilation(AActor* Actor, float TimeDilation);

    /**
     * Force update Groom rendering state when TimeDilation=0 or game is paused.
     * This ensures shadows and voxelization are updated correctly.
     *
     * @param Actor - The actor containing GroomComponents
     * @param bUpdateTransform - Mark render transform dirty (bounds recalculation)
     * @param bUpdateDynamicData - Mark dynamic data dirty (buffer swap for skinning/deformation)
     * @param bInvalidateRenderState - Fully invalidate and recreate render state
     * @param bUpdateHairGroupsDesc - Update hair groups description (shadow density, raytracing settings)
     * @return true if any GroomComponent was found and updated
     */
    UFUNCTION(BlueprintCallable, Category = "UnrealCV|Groom")
    static bool UpdateGroomRenderState(
        AActor* Actor,
        bool bUpdateTransform = true,
        bool bUpdateDynamicData = true,
        bool bInvalidateRenderState = false,
        bool bUpdateHairGroupsDesc = false);

    /**
     * Force refresh all Groom shadows in the world.
     * Call this after moving camera when game is paused.
     */
    UFUNCTION(BlueprintCallable, Category = "UnrealCV|Groom")
    static bool RefreshAllGroomShadows(UObject* WorldContextObject);

};
