// Weichao Qiu @ 2017
#pragma once

#include "BaseCameraSensor.h"
#include "AnnotationCamSensor.generated.h"

/** Annotation sensor, utilize the UAnnnotationComponent */
UCLASS(meta = (BlueprintSpawnableComponent))
class UNREALCV_API UAnnotationCamSensor : public UBaseCameraSensor
{
	GENERATED_BODY()

public:
	UAnnotationCamSensor(const FObjectInitializer& ObjectInitializer);

	static void GetAnnotationComponents(UWorld* World, TArray<TWeakObjectPtr<UPrimitiveComponent> >& ComponentList);

	static void SetCacheEnabled(bool bEnabled);
	static void ClearCache();

	void CaptureSeg(TArray<FColor>& ImageData, int& Width, int& Height);

	void CaptureSegToFile(const FString& Filename);

	void InitTextureTarget(int FilmWidth, int FilmHeight);

private:
	static TMap<UWorld*, TArray<TWeakObjectPtr<UPrimitiveComponent>>> CachedAnnotationComponents;
	static TMap<UWorld*, int32> CachedWorldFrameNumbers;
	static bool bCacheEnabled;

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction * T);
};
