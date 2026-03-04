#pragma once

#include "LitCamSensor.h"
#include "OneObjectLitCamSensor.generated.h"

UCLASS(meta = (BlueprintSpawnableComponent))
class UNREALCV_API UOneObjectLitCamSensor : public ULitCamSensor
{
	GENERATED_BODY()

public:
	UOneObjectLitCamSensor(const FObjectInitializer& ObjectInitializer);

	virtual void InitTextureTarget(int InFilmWidth, int InFilmHeight) override;

private:
	/** Post process material for one object opacity */
	UPROPERTY()
	UMaterial* OneObjOpacityMaterial;
};
