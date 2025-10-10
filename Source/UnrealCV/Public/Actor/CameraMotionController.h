// Camera Motion Controller for UnrealCV
// Handles programmatic camera trajectories (rotation, zoom, etc.)
// Separates camera movement from recording logic (better OOD)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CameraMotionController.generated.h"

/** Motion types supported by the controller */
UENUM(BlueprintType)
enum class ECameraMotionType : uint8
{
	Idle,
	RotateLeft45,
	RotateRight45,
	RotateUp45,
	RotateDown45,
	Rotate360,
	Rotate360Slow,  // For bullet-time recording
	ZoomIn,
	ZoomOut,
	RandomRotation,
	CustomTrajectory
};

/** Motion state */
UENUM(BlueprintType)
enum class ECameraMotionState : uint8
{
	Idle,
	Moving,
	Completed,
	Cancelled
};

/**
 * ACameraMotionController
 *
 * Manages programmatic camera trajectories for FusionCamSensor.
 * Decouples camera movement from recording logic.
 *
 * Usage:
 *   1. Spawn controller: World->SpawnActor<ACameraMotionController>()
 *   2. Set target: MotionController->SetTargetCamera(FusionCamSensor)
 *   3. Start motion: MotionController->StartRotate360()
 *   4. Query status: MotionController->IsMoving(), GetProgress()
 *   5. Stop: MotionController->StopMotion() or wait for auto-complete
 */
UCLASS()
class UNREALCV_API ACameraMotionController : public AActor
{
	GENERATED_BODY()

public:
	ACameraMotionController();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

public:
	/** Set the target camera to control */
	UFUNCTION(BlueprintCallable, Category = "CameraMotion")
	void SetTargetCamera(class UFusionCamSensor* Camera);

	/** Start rotation left by 45 degrees */
	UFUNCTION(BlueprintCallable, Category = "CameraMotion")
	void StartRotateLeft45(float Duration = 2.0f);

	/** Start rotation right by 45 degrees */
	UFUNCTION(BlueprintCallable, Category = "CameraMotion")
	void StartRotateRight45(float Duration = 2.0f);

	/** Start rotation up by 45 degrees */
	UFUNCTION(BlueprintCallable, Category = "CameraMotion")
	void StartRotateUp45(float Duration = 2.0f);

	/** Start rotation down by 45 degrees */
	UFUNCTION(BlueprintCallable, Category = "CameraMotion")
	void StartRotateDown45(float Duration = 2.0f);

	/** Start 360 degree rotation around target */
	UFUNCTION(BlueprintCallable, Category = "CameraMotion")
	void StartRotate360(AActor* Target = nullptr, float Duration = 5.0f);

	/** Start slow 360 degree rotation (for bullet-time recording) */
	UFUNCTION(BlueprintCallable, Category = "CameraMotion")
	void StartRotate360Slow(AActor* Target = nullptr, float Duration = 10.0f, float SpeedDegPerFrame = 2.0f);

	/** Start zoom in motion */
	UFUNCTION(BlueprintCallable, Category = "CameraMotion")
	void StartZoomIn(float Distance = 200.0f, float Duration = 2.0f);

	/** Start zoom out motion */
	UFUNCTION(BlueprintCallable, Category = "CameraMotion")
	void StartZoomOut(float Distance = 200.0f, float Duration = 2.0f);

	/** Start random rotation motion */
	UFUNCTION(BlueprintCallable, Category = "CameraMotion")
	void StartRandomRotation(AActor* Target = nullptr, float Duration = 5.0f);

	/** Stop current motion */
	UFUNCTION(BlueprintCallable, Category = "CameraMotion")
	void StopMotion();

	/** Check if motion is active */
	UFUNCTION(BlueprintCallable, Category = "CameraMotion")
	bool IsMoving() const { return MotionState == ECameraMotionState::Moving; }

	/** Get motion progress (0.0 to 1.0) */
	UFUNCTION(BlueprintCallable, Category = "CameraMotion")
	float GetProgress() const;

	/** Get current motion state */
	UFUNCTION(BlueprintCallable, Category = "CameraMotion")
	ECameraMotionState GetMotionState() const { return MotionState; }

	/** Get current motion type */
	UFUNCTION(BlueprintCallable, Category = "CameraMotion")
	ECameraMotionType GetMotionType() const { return CurrentMotionType; }

private:
	/** Target camera sensor to control */
	UPROPERTY()
	class UFusionCamSensor* TargetCamera;

	/** Current motion type */
	ECameraMotionType CurrentMotionType;

	/** Current motion state */
	ECameraMotionState MotionState;

	/** Target actor for orbit motions */
	UPROPERTY()
	AActor* OrbitTarget;

	/** Motion timing */
	float MotionDuration;
	float ElapsedTime;

	/** Motion parameters */
	float RotationAngleDeg;      // Total rotation angle
	float RotationSpeedDegPerSec; // Rotation speed
	float ZoomDistance;          // Zoom distance
	float ZoomSpeed;             // Zoom speed

	// For 360 rotation
	FVector InitialCameraOffset;  // Offset from target
	FRotator InitialCameraRotation; // Initial rotation
	FVector OrbitCenter;          // Center of rotation

	/** Update motion per tick */
	void UpdateMotion(float DeltaTime);

	/** Motion update functions */
	void UpdateRotation(float DeltaTime);
	void UpdateRotate360(float DeltaTime);
	void UpdateZoom(float DeltaTime);
	void UpdateRandomRotation(float DeltaTime);

	/** Complete current motion */
	void CompleteMotion();

	/** Start generic rotation motion */
	void StartRotationMotion(float AngleDeg, float Duration, bool bYaw = true, bool bPitch = false, bool bRoll = false);
};
