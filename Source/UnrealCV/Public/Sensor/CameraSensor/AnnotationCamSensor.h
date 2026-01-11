// Weichao Qiu @ 2017
#pragma once

#include "BaseCameraSensor.h"
#include "AnnotationCamSensor.generated.h"

UCLASS(meta = (BlueprintSpawnableComponent))
class UNREALCV_API UAnnotationCamSensor : public UBaseCameraSensor
{
	GENERATED_BODY()

public:
	UAnnotationCamSensor(const FObjectInitializer& ObjectInitializer);

	void CaptureSeg(TArray<FColor>& ImageData, int& Width, int& Height);

	void CaptureSegToFile(const FString& Filename);

	void InitTextureTarget(int FilmWidth, int FilmHeight);


 	UPROPERTY()
 	bool bUseShowOnlyComponentsOverride;
 	UPROPERTY()
 	TArray<TWeakObjectPtr<UPrimitiveComponent> > ShowOnlyComponentsOverride;

private:
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction * T);

	void PrepareShowOnlyComponents();
};
