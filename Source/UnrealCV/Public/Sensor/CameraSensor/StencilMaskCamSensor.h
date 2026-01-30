#pragma once

#include "BaseCameraSensor.h"
#include "StencilMaskCamSensor.generated.h"

UCLASS(meta = (BlueprintSpawnableComponent))
class UNREALCV_API UStencilMaskCamSensor : public UBaseCameraSensor
{
	GENERATED_BODY()

public:
	UStencilMaskCamSensor(const FObjectInitializer& ObjectInitializer);

	virtual void InitTextureTarget(int FilmWidth, int FilmHeight) override;

	void CaptureStencilMask(TArray<FColor>& Image, int& Width, int& Height);
	void CaptureStencilMaskToFile(FString Filename);

	void SetupForActor(AActor* TargetActor);
	void Cleanup(AActor* TargetActor);

private:
	UMaterial* StencilMaskMaterial;
};
