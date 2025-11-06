// Weichao Qiu @ 2017
#pragma once

#include "FusionCamSensor.h"
#include "Actor/CamSensorActor.h"
#include "FusionCameraActor.generated.h"

UCLASS()
class UNREALCV_API AFusionCameraActor : public ACamSensorActor
{
	GENERATED_BODY()

public:
	AFusionCameraActor();

	virtual void Tick(float DeltaTime) override;

	virtual TArray<FString> GetSensorNames();

	virtual TArray<UFusionCamSensor*> GetSensors();

	UFUNCTION(BlueprintCallable, Category = "Tracking")
	void StartTracking(AActor* Target, float Distance = 300.0f, float AngleOffset = 0.0f, float Gain = 0.1f);

	UFUNCTION(BlueprintCallable, Category = "Tracking")
	void StopTracking();

private:
	UPROPERTY(Category = AFusionCameraActor, VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess = "true"))
	UFusionCamSensor* FusionCamSensor;

	UPROPERTY()
	AActor* TrackedActor;

	float DesiredDistance;
	float HorizontalAngleOffset;
	float TrackingGain;
	bool bEnableTracking;

	void UpdateTrackingPosition();
};
