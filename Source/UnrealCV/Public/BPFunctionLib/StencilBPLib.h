#pragma once

#include "CoreMinimal.h"
#include "Runtime/Engine/Classes/Kismet/BlueprintFunctionLibrary.h"
#include "StencilBPLib.generated.h"

UCLASS()
class UNREALCV_API UStencilBPLib : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Stencil")
	static void EnableCustomDepthForActor(AActor* TargetActor, int32 StencilValue = 1);

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Stencil")
	static void DisableCustomDepthForActor(AActor* TargetActor);

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Stencil")
	static void EnableCustomDepthForActors(const TArray<AActor*>& TargetActors, int32 StencilValue = 1);

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Stencil")
	static void DisableCustomDepthForActors(const TArray<AActor*>& TargetActors);

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Stencil")
	static void SetCustomDepthStencilValue(AActor* TargetActor, int32 StencilValue);

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Stencil")
	static int32 GetCustomDepthStencilValue(AActor* TargetActor);

// private:
	// struct FStencilBackup
	// {
	// 	TWeakObjectPtr<UPrimitiveComponent> Component;
	// 	bool bOriginalRenderCustomDepth;
	// 	int32 OriginalStencilValue;
	// };

	// static TArray<FStencilBackup> StencilBackups;
};
