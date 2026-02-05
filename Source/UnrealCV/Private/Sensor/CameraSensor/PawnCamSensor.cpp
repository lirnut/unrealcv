// Weichao Qiu @ 2017

#include "PawnCamSensor.h"
#include "Runtime/Engine/Classes/GameFramework/Pawn.h"
#include "Runtime/Engine/Classes/GameFramework/Controller.h"
// #include "LitCamSensor.h"
#include "BaseCameraSensor.h"

UPawnCamSensor::UPawnCamSensor(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	this->PrimaryComponentTick.bCanEverTick = true;
	// UE_LOG(LogTemp, Warning, TEXT("UPawnCamSensor::Constructor: this=%p, bCanEverTick=%d, bStartWithTickEnabled=%d"),
	// 	this, this->PrimaryComponentTick.bCanEverTick, this->PrimaryComponentTick.bStartWithTickEnabled);

	for (UBaseCameraSensor* Sensor : FusionSensors)
	{
		if (!IsValid(Sensor))
		{
			// UE_LOG(LogTemp, Error, TEXT("UPawnCamSensor::UPawnCamSensor: Sensor %p within PawnCamSensor is invalid. this: %p"), Sensor, this);
		 	continue;
		}
		// Sensor->bAbsoluteLocation = true;
		// Sensor->bAbsoluteRotation = true;
		Sensor->SetUsingAbsoluteLocation(true);
		Sensor->SetUsingAbsoluteRotation(true);
		// UE_LOG(LogTemp, Warning, TEXT("UPawnCamSensor::Constructor: Set Sensor[%s] to use absolute location/rotation"), *Sensor->GetName());
	}
}

void UPawnCamSensor::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction * T)
{
	Super::TickComponent(DeltaTime, TickType, T);
	// UE_LOG(LogTemp, Warning, TEXT("UPawnCamSensor::TickComponent: Called! this=%p, DeltaTime=%f"), this, DeltaTime);
	// Make the world location and view rotation is the same as the Pawn viewpoint
	FVector EyeLocation;
	FRotator EyeRotation;

	AActor *Owner = this->GetOwner();
	APawn *Pawn = Cast<APawn>(Owner);
	if (!IsValid(Pawn))
	{
		// UE_LOG(LogTemp, Error, TEXT("UPawnCamSensor::TickComponent: Pawn is invalid. this: %p"), this);
		return;
	}

	// I want to get the Viewport, not really the pawn view.
	// Note: https://answers.unrealengine.com/questions/5155/getting-editor-viewport-camera.html
	// GEngine->GameViewport->Viewport
	Pawn->GetActorEyesViewPoint(EyeLocation, EyeRotation);
	// UE_LOG(LogTemp, Warning, TEXT("UPawnCamSensor::TickComponent: EyeLocation=%s, EyeRotation=%s"),
	// 	*EyeLocation.ToString(), *EyeRotation.ToString());
	// GetPlayerViewpoint(EyeLocation, EyeRotation);
	// FRotator CompRotation = this->GetComponentRotation();
	// FVector CompLocation = this->GetComponentLocation();
	// ScreenLog(FString::Printf(TEXT("Rotation, Actor Eye: %s, Component: %s"), *EyeRotation.ToString(), *CompRotation.ToString()));
	// ScreenLog(FString::Printf(TEXT("Location, Actor Eye: %s, Component: %s"), *EyeLocation.ToString(), *CompLocation.ToString()));

	// TODO: any alternative implementation
	this->SetWorldLocation(EyeLocation);
	this->SetWorldRotation(EyeRotation);
	this->UpdateChildTransforms();
	// FVector ActualLocation = this->GetComponentLocation();
	// FRotator ActualRotation = this->GetComponentRotation();
	// UE_LOG(LogTemp, Warning, TEXT("UPawnCamSensor::TickComponent: After Set - ActualLocation=%s, ActualRotation=%s"),
	// 	*ActualLocation.ToString(), *ActualRotation.ToString());

	// USceneComponent* Parent = this->LitCamSensor->GetAttachParent();
	// if (Parent != this)
	// {
	// 	UE_LOG(LogTemp, Warning, TEXT("Unexpected attach"));
	// }
	// FTransform RelativeTransform = this->LitCamSensor->GetRelativeTransform();
	// FVector LitLocation = this->LitCamSensor->GetSensorLocation();
	// FRotator LitRotation = this->LitCamSensor->GetSensorRotation();
	// FVector ThisLocation = this->GetSensorLocation();
	// FRotator ThisRotation = this->GetSensorRotation();
	// this->LitCamSensor->SetSensorLocation(EyeLocation);
	// this->LitCamSensor->SetSensorRotation(EyeRotation);

	for (UBaseCameraSensor* Sensor : FusionSensors)
	{
		if (!IsValid(Sensor))
		{
			UE_LOG(LogTemp, Error, TEXT("SetSensorLocation: Sensor %p within PawnCamSensor is invalid. this: %p"), Sensor, this);
		 	continue;
		}
		// FVector BeforeLoc = Sensor->GetSensorLocation();
		Sensor->SetSensorLocation(EyeLocation);
		Sensor->SetSensorRotation(EyeRotation);
		// FVector AfterLoc = Sensor->GetSensorLocation();
		// UE_LOG(LogTemp, Warning, TEXT("UPawnCamSensor::TickComponent: Sensor[%s] BeforeLoc=%s, AfterLoc=%s"),
		// 	*Sensor->GetName(), *BeforeLoc.ToString(), *AfterLoc.ToString());
	}
	// TODO: check this with a player pawn
	// if (CompLocation != EyeLocation || CompRotation != EyeRotation)
	// {
	// ScreenLog(TEXT("Pawn Camera Sensor is not correctly mounted, location or rotation mismatch"));
	// }

}

void UPawnCamSensor::SetSensorLocation(FVector Location)
{
	// Super::SetSensorLocation(NewLocation, bSweep, OutSweepHitResult, Teleport);
	AActor *Owner = this->GetOwner();
	APawn *Pawn = Cast<APawn>(Owner);
	if (!IsValid(Pawn))
	{
		UE_LOG(LogTemp, Error, TEXT("UPawnCamSensor::SetSensorLocation: Pawn is invalid. this: %p"), this);
		return;
	}
	bool Sweep = false;
	Pawn->SetActorLocation(Location, Sweep, NULL, ETeleportType::TeleportPhysics);
}

void UPawnCamSensor::SetSensorRotation(FRotator Rotation)
{
	AActor *Owner = this->GetOwner();
	APawn *Pawn = Cast<APawn>(Owner);
	if (!IsValid(Pawn))
	{
		UE_LOG(LogTemp, Error, TEXT("UPawnCamSensor::SetSensorRotation: Pawn is invalid. this: %p"), this);
		return;
	}
	AController* Controller = Pawn->GetController();
	Controller->ClientSetRotation(Rotation);
}
