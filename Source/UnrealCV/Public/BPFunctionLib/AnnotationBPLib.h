#pragma once

#include "Runtime/Engine/Classes/Kismet/BlueprintFunctionLibrary.h"
#include "AnnotationBPLib.generated.h"

class UPrimitiveComponent;

UCLASS()
class UNREALCV_API UAnnotationBPLib : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "unrealcv|Annotation")
	static void AnnotateActor(AActor* Actor);

	UFUNCTION(BlueprintCallable, Category = "unrealcv|Annotation")
	static void AnnotateWorld();

	UFUNCTION(BlueprintCallable, Category = "unrealcv|Annotation")
	static void DeannotateWorld();

	static void GetAnnotationComponents(
		UWorld* World,
		TArray<TWeakObjectPtr<UPrimitiveComponent>>& OutComponentList);

	static void SetAnnotationCacheEnabled(bool bEnabled);

	static void ClearAnnotationCache();

private:
	static TMap<UWorld*, TArray<TWeakObjectPtr<UPrimitiveComponent>>> CachedAnnotationComponents;
	static TMap<UWorld*, int32> CachedWorldFrameNumbers;
	static bool bCacheEnabled;
};
