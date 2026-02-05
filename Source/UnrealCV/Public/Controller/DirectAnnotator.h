#pragma once

#include "CoreMinimal.h"
#include "Controller/IAnnotatorImpl.h"
#include "Runtime/Engine/Classes/GameFramework/Actor.h"

class FColorGenerator;

class UNREALCV_API FDirectAnnotator : public IAnnotatorImpl
{
public:
	FDirectAnnotator();
	virtual ~FDirectAnnotator() override;

	virtual void AnnotateWorld(UWorld* World) override;
	virtual void DeannotateWorld(UWorld* World) override;
	virtual int32 SetAnnotationColor(AActor* Actor, const FColor& AnnotationColor) override;
	virtual void GetAnnotationColor(AActor* Actor, FColor& AnnotationColor) override;
	virtual TMap<FString, FColor> GetAnnotationColors() override { return AnnotationColors; }

private:
	void GetAnnotableActors(UWorld* World, TArray<AActor*>& ActorArray);
	FColor GetDefaultColor(AActor* Actor);
	void SetPrimitiveAnnotationData(UPrimitiveComponent* Primitive, const FColor& AnnotationColor, int32 ActorID);

	TMap<FString, FColor> AnnotationColors;
	TSharedPtr<FColorGenerator> ColorGenerator;
};
