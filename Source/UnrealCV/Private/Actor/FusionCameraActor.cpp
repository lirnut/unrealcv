// Weichao Qiu @ 2017

#include "FusionCameraActor.h"

AFusionCameraActor::AFusionCameraActor()
	: Super()
{
	PrimaryActorTick.bCanEverTick = true;

	FusionCamSensor = CreateDefaultSubobject<UFusionCamSensor>(TEXT("FusionCameraSensor"));
	RootComponent = FusionCamSensor;

	TrackedActor = nullptr;
	bEnableTracking = false;
	DesiredDistance = 300.0f;
	HorizontalAngleOffset = 0.0f;
	TrackingGain = 0.1f;
}

TArray<FString> AFusionCameraActor::GetSensorNames()
{
	return { this->GetName() };
}

TArray<UFusionCamSensor*> AFusionCameraActor::GetSensors()
{
	return { this->FusionCamSensor };
}

void AFusionCameraActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bEnableTracking && TrackedActor)
	{
		UpdateTrackingPosition();
	}
}

void AFusionCameraActor::StartTracking(AActor* Target, float Distance, float AngleOffset, float Gain)
{
	if (Target)
	{
		TrackedActor = Target;
		DesiredDistance = Distance;
		HorizontalAngleOffset = AngleOffset;
		TrackingGain = FMath::Clamp(Gain, 0.0f, 1.0f);
		bEnableTracking = true;
	}
}

void AFusionCameraActor::StopTracking()
{
	bEnableTracking = false;
	TrackedActor = nullptr;
}

void AFusionCameraActor::UpdateTrackingPosition()
{
	FVector TargetLocation = TrackedActor->GetActorLocation();
	FRotator TargetRotation = TrackedActor->GetActorRotation();

	FRotator OffsetRotation = TargetRotation;
	OffsetRotation.Yaw += HorizontalAngleOffset;

	FVector Offset = OffsetRotation.Vector() * (-DesiredDistance);
	FVector DesiredLocation = TargetLocation + Offset;

	FVector CurrentLocation = GetActorLocation();
	FVector NewLocation = FMath::Lerp(CurrentLocation, DesiredLocation, TrackingGain);

	FRotator LookAtRotation = (TargetLocation - NewLocation).Rotation();
	FRotator NewRotation = FMath::Lerp(GetActorRotation(), LookAtRotation, TrackingGain);

	SetActorLocationAndRotation(NewLocation, NewRotation);
	// FusionCamSensor->SetSensorLocation(NewLocation);
	// FusionCamSensor->SetSensorRotation(NewRotation);
}