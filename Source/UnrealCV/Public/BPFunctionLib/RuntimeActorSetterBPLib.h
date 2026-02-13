#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RuntimeActorSetterBPLib.generated.h"

UCLASS()
class UNREALCV_API URuntimeActorSetterBPLib : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "UnrealCV|RuntimeActor")
    static bool SetAffectDistanceFieldLighting(AActor* Actor, bool bAffect);

    UFUNCTION(BlueprintCallable, Category = "UnrealCV|RuntimeActor")
    static bool SetCastShadow(AActor* Actor, bool bCastShadow);

    UFUNCTION(BlueprintCallable, Category = "UnrealCV|RuntimeActor")
    static bool SetMovable(AActor* Actor, bool bMovable);

    UFUNCTION(BlueprintCallable, Category = "UnrealCV|RuntimeActor")
    static bool SetTickableWhenPaused(AActor* Actor, bool bTickable);

    UFUNCTION(BlueprintCallable, Category = "UnrealCV|RuntimeActor")
    static bool GetActorComponents(AActor* Actor, TArray<UPrimitiveComponent*>& OutComponents);

private:
    static void CollectPrimitiveComponentsRecursive(USceneComponent* SceneComponent, TArray<UPrimitiveComponent*>& OutComponents);
    static void FindAllPrimitiveComponentsInActor(AActor* Actor, TArray<UPrimitiveComponent*>& OutComponents);
};
