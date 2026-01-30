#pragma once

#include "BaseCameraSensor.h"
#include "OneObjMaskCamSensor.generated.h"

UCLASS(meta = (BlueprintSpawnableComponent))
class UNREALCV_API UOneObjMaskCamSensor : public UBaseCameraSensor
{
	GENERATED_BODY()

public:
	UOneObjMaskCamSensor(const FObjectInitializer& ObjectInitializer);

	virtual void InitTextureTarget(int FilmWidth, int FilmHeight) override;

	void CaptureOneObjMask(TArray<FColor>& Image, int& Width, int& Height);
	void CaptureOneObjMaskToFile(FString Filename);

	void SetupForActor(AActor* TargetActor);
	void Cleanup(AActor* TargetActor);

private:
	UMaterial* OneObjMaskMaterial;
};
