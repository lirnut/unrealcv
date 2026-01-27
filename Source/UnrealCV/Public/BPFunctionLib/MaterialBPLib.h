#pragma once

#include "CoreMinimal.h"
#include "Runtime/Engine/Classes/Kismet/BlueprintFunctionLibrary.h"
#include "MaterialBPLib.generated.h"

UCLASS()
class UNREALCV_API UMaterialBPLib : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Material")
	static void ShowOnlyActorMaterial(AActor* TargetActor, UWorld* World);

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Material")
	static void ShowOnlyActorsMaterial(const TArray<AActor*>& TargetActors, UWorld* World);

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Material")
	static void RestoreAllActorMaterials();

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Material")
	static void CacheSceneActors(UWorld* World);

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Material")
	static void ClearCache();

private:
	struct FMaterialBackup
	{
		TWeakObjectPtr<UPrimitiveComponent> Component;
		TArray<UMaterialInterface*> OriginalMaterials;
	};

	static TArray<FMaterialBackup> MaterialBackups;
	static TArray<TWeakObjectPtr<AActor>> CachedActors;
	static UMaterial* OpaqueMaterial;

	static void EnsureOpaqueMaterialLoaded();
	static void ReplaceActorMaterials(AActor* Actor);
};
