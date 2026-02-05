#pragma once

#include "CoreMinimal.h"

class IAnnotatorImpl
{
public:
	virtual ~IAnnotatorImpl() = default;

	virtual void AnnotateWorld(UWorld* World) = 0;
	virtual void DeannotateWorld(UWorld* World) = 0;
	virtual int32 SetAnnotationColor(AActor* Actor, const FColor& AnnotationColor) = 0;
	virtual void GetAnnotationColor(AActor* Actor, FColor& AnnotationColor) = 0;
	virtual TMap<FString, FColor> GetAnnotationColors() = 0;
};
