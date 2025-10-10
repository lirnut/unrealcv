// Camera Motion Controller implementation

#include "CameraMotionController.h"
#include "FusionCamSensor.h"
#include "Kismet/KismetMathLibrary.h"
#include "UnrealcvLog.h"

ACameraMotionController::ACameraMotionController()
{
	PrimaryActorTick.bCanEverTick = true;

	TargetCamera = nullptr;
	CurrentMotionType = ECameraMotionType::Idle;
	MotionState = ECameraMotionState::Idle;
	OrbitTarget = nullptr;

	MotionDuration = 0.0f;
	ElapsedTime = 0.0f;
	RotationAngleDeg = 0.0f;
	RotationSpeedDegPerSec = 0.0f;
	ZoomDistance = 0.0f;
	ZoomSpeed = 0.0f;
}

void ACameraMotionController::BeginPlay()
{
	Super::BeginPlay();
}

void ACameraMotionController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (MotionState == ECameraMotionState::Moving)
	{
		UpdateMotion(DeltaTime);
	}
}

void ACameraMotionController::SetTargetCamera(UFusionCamSensor* Camera)
{
	TargetCamera = Camera;
}

float ACameraMotionController::GetProgress() const
{
	if (MotionDuration <= 0.0f)
		return 0.0f;

	return FMath::Clamp(ElapsedTime / MotionDuration, 0.0f, 1.0f);
}

void ACameraMotionController::StartRotationMotion(float AngleDeg, float Duration, bool bYaw, bool bPitch, bool bRoll)
{
	if (!IsValid(TargetCamera))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("CameraMotionController: No target camera set"));
		return;
	}

	RotationAngleDeg = AngleDeg;
	MotionDuration = Duration;
	RotationSpeedDegPerSec = AngleDeg / Duration;
	ElapsedTime = 0.0f;

	MotionState = ECameraMotionState::Moving;
}

void ACameraMotionController::StartRotateLeft45(float Duration)
{
	CurrentMotionType = ECameraMotionType::RotateLeft45;
	StartRotationMotion(-45.0f, Duration, true, false, false);
	UE_LOG(LogUnrealCV, Log, TEXT("CameraMotionController: Started RotateLeft45"));
}

void ACameraMotionController::StartRotateRight45(float Duration)
{
	CurrentMotionType = ECameraMotionType::RotateRight45;
	StartRotationMotion(45.0f, Duration, true, false, false);
	UE_LOG(LogUnrealCV, Log, TEXT("CameraMotionController: Started RotateRight45"));
}

void ACameraMotionController::StartRotateUp45(float Duration)
{
	CurrentMotionType = ECameraMotionType::RotateUp45;
	StartRotationMotion(45.0f, Duration, false, true, false);
	UE_LOG(LogUnrealCV, Log, TEXT("CameraMotionController: Started RotateUp45"));
}

void ACameraMotionController::StartRotateDown45(float Duration)
{
	CurrentMotionType = ECameraMotionType::RotateDown45;
	StartRotationMotion(-45.0f, Duration, false, true, false);
	UE_LOG(LogUnrealCV, Log, TEXT("CameraMotionController: Started RotateDown45"));
}

void ACameraMotionController::StartRotate360(AActor* Target, float Duration)
{
	if (!IsValid(TargetCamera))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("CameraMotionController: No target camera set"));
		return;
	}

	CurrentMotionType = ECameraMotionType::Rotate360;
	OrbitTarget = Target;
	MotionDuration = Duration;
	ElapsedTime = 0.0f;
	RotationAngleDeg = 360.0f;
	RotationSpeedDegPerSec = 360.0f / Duration;

	// Setup orbit parameters
	FVector CameraLocation = TargetCamera->GetSensorLocation();

	if (IsValid(OrbitTarget))
	{
		OrbitCenter = OrbitTarget->GetActorLocation();
	}
	else
	{
		// Orbit around a point in front of camera
		FRotator CameraRotation = TargetCamera->GetSensorRotation();
		OrbitCenter = CameraLocation + CameraRotation.Vector() * 500.0f;
	}

	InitialCameraOffset = CameraLocation - OrbitCenter;
	InitialCameraRotation = TargetCamera->GetSensorRotation();

	MotionState = ECameraMotionState::Moving;
	UE_LOG(LogUnrealCV, Log, TEXT("CameraMotionController: Started Rotate360"));
}

void ACameraMotionController::StartRotate360Slow(AActor* Target, float Duration, float SpeedDegPerFrame)
{
	// Same as Rotate360, but explicitly for bullet-time recording
	// SpeedDegPerFrame can be used to sync with frame capture rate
	CurrentMotionType = ECameraMotionType::Rotate360Slow;
	StartRotate360(Target, Duration);

	// Override speed if specified
	if (SpeedDegPerFrame > 0.0f)
	{
		RotationSpeedDegPerSec = SpeedDegPerFrame * 30.0f; // Assuming 30 FPS
	}

	UE_LOG(LogUnrealCV, Log, TEXT("CameraMotionController: Started Rotate360Slow (bullet-time compatible)"));
}

void ACameraMotionController::StartZoomIn(float Distance, float Duration)
{
	if (!IsValid(TargetCamera))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("CameraMotionController: No target camera set"));
		return;
	}

	CurrentMotionType = ECameraMotionType::ZoomIn;
	ZoomDistance = -Distance; // Negative for zoom in (move forward)
	MotionDuration = Duration;
	ZoomSpeed = ZoomDistance / Duration;
	ElapsedTime = 0.0f;

	MotionState = ECameraMotionState::Moving;
	UE_LOG(LogUnrealCV, Log, TEXT("CameraMotionController: Started ZoomIn"));
}

void ACameraMotionController::StartZoomOut(float Distance, float Duration)
{
	if (!IsValid(TargetCamera))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("CameraMotionController: No target camera set"));
		return;
	}

	CurrentMotionType = ECameraMotionType::ZoomOut;
	ZoomDistance = Distance; // Positive for zoom out (move backward)
	MotionDuration = Duration;
	ZoomSpeed = ZoomDistance / Duration;
	ElapsedTime = 0.0f;

	MotionState = ECameraMotionState::Moving;
	UE_LOG(LogUnrealCV, Log, TEXT("CameraMotionController: Started ZoomOut"));
}

void ACameraMotionController::StartRandomRotation(AActor* Target, float Duration)
{
	if (!IsValid(TargetCamera))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("CameraMotionController: No target camera set"));
		return;
	}

	CurrentMotionType = ECameraMotionType::RandomRotation;
	OrbitTarget = Target;
	MotionDuration = Duration;
	ElapsedTime = 0.0f;

	// Random rotation angle and axis
	RotationAngleDeg = FMath::RandRange(90.0f, 360.0f);
	RotationSpeedDegPerSec = RotationAngleDeg / Duration;

	// Setup orbit with random initial offset
	FVector CameraLocation = TargetCamera->GetSensorLocation();

	if (IsValid(OrbitTarget))
	{
		OrbitCenter = OrbitTarget->GetActorLocation();
	}
	else
	{
		FRotator CameraRotation = TargetCamera->GetSensorRotation();
		OrbitCenter = CameraLocation + CameraRotation.Vector() * 500.0f;
	}

	InitialCameraOffset = CameraLocation - OrbitCenter;
	InitialCameraRotation = TargetCamera->GetSensorRotation();

	MotionState = ECameraMotionState::Moving;
	UE_LOG(LogUnrealCV, Log, TEXT("CameraMotionController: Started RandomRotation"));
}

void ACameraMotionController::StopMotion()
{
	MotionState = ECameraMotionState::Cancelled;
	CurrentMotionType = ECameraMotionType::Idle;
	ElapsedTime = 0.0f;

	UE_LOG(LogUnrealCV, Log, TEXT("CameraMotionController: Motion stopped"));
}

void ACameraMotionController::UpdateMotion(float DeltaTime)
{
	if (!IsValid(TargetCamera))
	{
		StopMotion();
		return;
	}

	ElapsedTime += DeltaTime;

	// Check completion
	if (ElapsedTime >= MotionDuration)
	{
		CompleteMotion();
		return;
	}

	// Update based on motion type
	switch (CurrentMotionType)
	{
	case ECameraMotionType::RotateLeft45:
	case ECameraMotionType::RotateRight45:
	case ECameraMotionType::RotateUp45:
	case ECameraMotionType::RotateDown45:
		UpdateRotation(DeltaTime);
		break;

	case ECameraMotionType::Rotate360:
	case ECameraMotionType::Rotate360Slow:
	case ECameraMotionType::RandomRotation:
		UpdateRotate360(DeltaTime);
		break;

	case ECameraMotionType::ZoomIn:
	case ECameraMotionType::ZoomOut:
		UpdateZoom(DeltaTime);
		break;

	default:
		break;
	}
}

void ACameraMotionController::UpdateRotation(float DeltaTime)
{
	FRotator CurrentRotation = TargetCamera->GetSensorRotation();
	float DeltaAngle = RotationSpeedDegPerSec * DeltaTime;

	// Apply rotation based on motion type
	if (CurrentMotionType == ECameraMotionType::RotateLeft45 ||
		CurrentMotionType == ECameraMotionType::RotateRight45)
	{
		CurrentRotation.Yaw += DeltaAngle;
	}
	else if (CurrentMotionType == ECameraMotionType::RotateUp45 ||
			 CurrentMotionType == ECameraMotionType::RotateDown45)
	{
		CurrentRotation.Pitch += DeltaAngle;
	}

	TargetCamera->SetSensorRotation(CurrentRotation);
}

void ACameraMotionController::UpdateRotate360(float DeltaTime)
{
	float DeltaAngle = RotationSpeedDegPerSec * DeltaTime;
	float CurrentAngle = (ElapsedTime / MotionDuration) * RotationAngleDeg;

	// Rotate camera offset around orbit center
	FQuat RotationQuat = FQuat(FVector::UpVector, FMath::DegreesToRadians(DeltaAngle));
	FVector NewOffset = RotationQuat.RotateVector(InitialCameraOffset);

	// Update camera offset for next frame
	InitialCameraOffset = NewOffset;

	// Calculate new camera location
	FVector NewCameraLocation = OrbitCenter + NewOffset;
	TargetCamera->SetSensorLocation(NewCameraLocation);

	// Update camera rotation to face orbit center
	FRotator LookAtRotation = (OrbitCenter - NewCameraLocation).Rotation();
	FRotator NewRotation = LookAtRotation + InitialCameraRotation - (OrbitCenter - (OrbitCenter + InitialCameraOffset)).Rotation();
	TargetCamera->SetSensorRotation(NewRotation);
}

void ACameraMotionController::UpdateZoom(float DeltaTime)
{
	FVector CameraLocation = TargetCamera->GetSensorLocation();
	FRotator CameraRotation = TargetCamera->GetSensorRotation();

	// Move camera along forward vector
	float DeltaDistance = ZoomSpeed * DeltaTime;
	FVector ForwardVector = CameraRotation.Vector();
	FVector NewLocation = CameraLocation + ForwardVector * DeltaDistance;

	TargetCamera->SetSensorLocation(NewLocation);
}

void ACameraMotionController::UpdateRandomRotation(float DeltaTime)
{
	// Similar to Rotate360, but with random variations
	UpdateRotate360(DeltaTime);

	// Add slight random wobble
	FRotator CurrentRotation = TargetCamera->GetSensorRotation();
	CurrentRotation.Pitch += FMath::FRandRange(-0.5f, 0.5f);
	CurrentRotation.Yaw += FMath::FRandRange(-0.5f, 0.5f);
	TargetCamera->SetSensorRotation(CurrentRotation);
}

void ACameraMotionController::CompleteMotion()
{
	MotionState = ECameraMotionState::Completed;
	CurrentMotionType = ECameraMotionType::Idle;

	UE_LOG(LogUnrealCV, Log, TEXT("CameraMotionController: Motion completed"));

	// Auto-cleanup: mark for destruction after completion
	// Give it a short delay in case status needs to be queried
	FTimerHandle DestroyTimerHandle;
	GetWorld()->GetTimerManager().SetTimer(
		DestroyTimerHandle,
		[this]() { this->Destroy(); },
		1.0f,
		false
	);
}
