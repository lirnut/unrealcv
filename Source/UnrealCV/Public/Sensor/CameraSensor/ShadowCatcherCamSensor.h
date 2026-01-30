#pragma once

#include "BaseCameraSensor.h"
#include "ShadowCatcherCamSensor.generated.h"

UCLASS(meta = (BlueprintSpawnableComponent))
class UNREALCV_API UShadowCatcherCamSensor : public UBaseCameraSensor
{
	GENERATED_BODY()

public:
	UShadowCatcherCamSensor(const FObjectInitializer& ObjectInitializer);

	virtual void InitTextureTarget(int FilmWidth, int FilmHeight) override;

	void CaptureShadowCatcher(TArray<FColor>& Image, int& Width, int& Height);
	void CaptureShadowCatcherToFile(FString Filename);

	void SetupForActor(AActor* TargetActor, UWorld* World);
	void CleanupGroundPlane();
	void Cleanup(AActor* TargetActor);

private:
	void CreateGroundPlane(UWorld* World);
	void UpdateGroundPlaneTransform(AActor* TargetActor);

	UPROPERTY()
	AActor* GroundPlaneActor;
	UMaterial* ShadowCatcherMaterial;

	TArray<TWeakObjectPtr<AActor>> HiddenActors;
};
