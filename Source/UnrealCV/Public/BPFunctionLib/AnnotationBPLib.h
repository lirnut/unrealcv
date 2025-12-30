#pragma once

#include "Runtime/Engine/Classes/Kismet/BlueprintFunctionLibrary.h"
#include "AnnotationBPLib.generated.h"

UCLASS()
class UNREALCV_API UAnnotationBPLib : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "unrealcv|Annotation")
	static void AnnotateActor(AActor* Actor);

	UFUNCTION(BlueprintCallable, Category = "unrealcv|Annotation")
	static void AnnotateWorld();
};
