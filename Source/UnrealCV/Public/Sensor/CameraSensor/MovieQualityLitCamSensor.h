#pragma once

#include "CoreMinimal.h"
#include "MovieQualityRenderComponent.h"
#include "MovieQualityLitCamSensor.generated.h"

UCLASS(meta = (BlueprintSpawnableComponent))
class UNREALCV_API UMovieQualityLitCamSensor : public UMovieQualityRenderComponent
{
	GENERATED_BODY()

public:
	UMovieQualityLitCamSensor();

	virtual void BeginPlay() override;

	void CaptureLit(TArray<FColor>& Image, int& Width, int& Height);
	void CaptureLitToFile(const FString& Filename);

protected:
	virtual void SetPostProcessSettings(FPostProcessSettings& PPSettings) override;

private:
	UPROPERTY()
	class UTextureRenderTarget2D* SyncRenderTarget;
};
