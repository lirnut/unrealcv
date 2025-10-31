// Copyright (c) 2025 UnrealCV
// Navigation Agent Controller for autonomous character movement
#include "NavAgentController.h"
#include "NavigationSystem.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/OutputDeviceNull.h"
#include "UnrealcvLog.h"

ANavAgentController::ANavAgentController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// Initialize state
	NavMode = ENavMode::Paused;
	CurrentGoal = FVector::ZeroVector;
	LastPosition = FVector::ZeroVector;
	StepCounter = 0;
	bHasValidGoal = false;
	AutonomousRadius = DefaultNavRadius;

	// Set root component
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ANavAgentController::BeginPlay()
{
	Super::BeginPlay();

	if (!ControlledAgent)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("NavAgentController: No ControlledAgent set"));
	}
}

void ANavAgentController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Early exit if paused or no agent
	if (NavMode == ENavMode::Paused || !ControlledAgent)
	{
		return;
	}

	StepCounter++;
	FVector CurrentPosition = ControlledAgent->GetActorLocation();

	// Check if we need a new goal
	bool bNeedNewGoal = false;

	if (!bHasValidGoal)
	{
		bNeedNewGoal = true;
	}
	else if (CheckReachedGoal())
	{
		UE_LOG(LogUnrealCV, Log, TEXT("NavAgentController: Reached goal"));
		bNeedNewGoal = true;
	}
	else if (StepCounter > MaxSteps)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("NavAgentController: Timeout, generating new goal"));
		bNeedNewGoal = true;
	}

	// Generate new goal if needed
	if (bNeedNewGoal)
	{
		if (NavMode == ENavMode::Autonomous)
		{
			if (GenerateRandomGoal())
			{
				SendNavCommand();
				StepCounter = 0;
			}
			else
			{
				UE_LOG(LogUnrealCV, Error, TEXT("NavAgentController: Failed to generate random goal"));
			}
		}
		else if (NavMode == ENavMode::ToTarget)
		{
			// In ToTarget mode, if we reached the goal, stop
			UE_LOG(LogUnrealCV, Log, TEXT("NavAgentController: Reached target position, stopping"));
			StopNavigation();
		}
	}

	// Draw debug
	if (bDebugDraw)
	{
		DrawDebug();
	}

	LastPosition = CurrentPosition;
}

// ========== Public API Functions ==========

void ANavAgentController::StartAutonomousNav(float Radius)
{
	if (!ControlledAgent)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("NavAgentController: Cannot start navigation, no ControlledAgent set"));
		return;
	}

	NavMode = ENavMode::Autonomous;
	AutonomousRadius = Radius;
	bHasValidGoal = false;
	StepCounter = 0;

	// Enable tick
	SetActorTickEnabled(true);

	UE_LOG(LogUnrealCV, Log, TEXT("NavAgentController: Started autonomous navigation with radius %.1f"), Radius);
}

void ANavAgentController::NavigateToPosition(FVector TargetLocation)
{
	if (!ControlledAgent)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("NavAgentController: Cannot navigate, no ControlledAgent set"));
		return;
	}

	// Validate that the target is reachable on NavMesh
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("NavAgentController: Navigation system not found"));
		return;
	}

	FNavLocation ProjectedLocation;
	bool bSuccess = NavSys->ProjectPointToNavigation(TargetLocation, ProjectedLocation, FVector(500, 500, 500));

	if (!bSuccess)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("NavAgentController: Target position not on NavMesh, using closest point"));
		// Try to find closest point on NavMesh
		bSuccess = NavSys->GetRandomReachablePointInRadius(TargetLocation, 500.0f, ProjectedLocation);
		if (!bSuccess)
		{
			UE_LOG(LogUnrealCV, Error, TEXT("NavAgentController: Cannot find reachable point near target"));
			return;
		}
	}

	NavMode = ENavMode::ToTarget;
	CurrentGoal = ProjectedLocation.Location;
	bHasValidGoal = true;
	StepCounter = 0;

	// Send command immediately
	SendNavCommand();

	// Enable tick
	SetActorTickEnabled(true);

	UE_LOG(LogUnrealCV, Log, TEXT("NavAgentController: Navigating to target position %s"), *CurrentGoal.ToString());
}

void ANavAgentController::StopNavigation()
{
	NavMode = ENavMode::Paused;
	bHasValidGoal = false;
	SetActorTickEnabled(false);

	UE_LOG(LogUnrealCV, Log, TEXT("NavAgentController: Navigation stopped"));
}

// ========== Internal Functions ==========

bool ANavAgentController::GenerateRandomGoal()
{
	if (!ControlledAgent)
	{
		return false;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("NavAgentController: Navigation system not found"));
		return false;
	}

	FVector AgentLocation = ControlledAgent->GetActorLocation();
	FNavLocation ResultLocation;

	// Try to get a random reachable point within radius
	// First try with full radius
	bool bSuccess = NavSys->GetRandomReachablePointInRadius(
		AgentLocation,
		AutonomousRadius,
		ResultLocation
	);

	// If failed, try with smaller radius
	if (!bSuccess)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("NavAgentController: Failed to find goal at radius %.1f, trying smaller radius"), AutonomousRadius);
		bSuccess = NavSys->GetRandomReachablePointInRadius(
			AgentLocation,
			AutonomousRadius * 0.5f,
			ResultLocation
		);
	}

	// Check if goal is not too close
	if (bSuccess)
	{
		float Distance = FVector::Dist2D(AgentLocation, ResultLocation.Location);
		if (Distance < MinNavRadius)
		{
			// Goal too close, try again
			bSuccess = NavSys->GetRandomReachablePointInRadius(
				AgentLocation,
				AutonomousRadius,
				ResultLocation
			);
		}
	}

	if (bSuccess)
	{
		CurrentGoal = ResultLocation.Location;
		bHasValidGoal = true;
		UE_LOG(LogUnrealCV, Verbose, TEXT("NavAgentController: Generated new goal at %s (distance: %.1f)"),
			*CurrentGoal.ToString(),
			FVector::Dist2D(AgentLocation, CurrentGoal));
		return true;
	}
	else
	{
		UE_LOG(LogUnrealCV, Error, TEXT("NavAgentController: Failed to generate random goal"));
		bHasValidGoal = false;
		return false;
	}
}

bool ANavAgentController::CheckReachedGoal() const
{
	if (!ControlledAgent || !bHasValidGoal)
	{
		return false;
	}

	FVector CurrentPosition = ControlledAgent->GetActorLocation();
	float Distance = FVector::Dist2D(CurrentPosition, CurrentGoal);

	return Distance < ReachThreshold;
}

void ANavAgentController::SendNavCommand()
{
	if (!ControlledAgent || !bHasValidGoal)
	{
		return;
	}

	// Call Blueprint function: nav_to_goal
	// This mimics the Python API: vbp {obj} nav_to_goal {x} {y} {z}

	FOutputDeviceNull NullOutput;
	FString Command = FString::Printf(
		TEXT("nav_to_goal %f %f %f"),
		CurrentGoal.X,
		CurrentGoal.Y,
		CurrentGoal.Z
	);

	bool bSuccess = ControlledAgent->CallFunctionByNameWithArguments(*Command, NullOutput, nullptr, true);

	if (bSuccess)
	{
		UE_LOG(LogUnrealCV, Verbose, TEXT("NavAgentController: Sent nav command to %s: %s"),
			*ControlledAgent->GetName(),
			*Command);
	}
	else
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("NavAgentController: Failed to send nav command to %s. Make sure the actor has 'nav_to_goal' Blueprint function."),
			*ControlledAgent->GetName());
	}
}

void ANavAgentController::DrawDebug()
{
	if (!ControlledAgent || !bHasValidGoal)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FVector AgentLocation = ControlledAgent->GetActorLocation();

	// Draw line from agent to goal
	FColor LineColor = (NavMode == ENavMode::Autonomous) ? FColor::Green : FColor::Blue;
	DrawDebugLine(
		World,
		AgentLocation,
		CurrentGoal,
		LineColor,
		false,
		DebugDrawDuration,
		0,
		2.0f
	);

	// Draw goal sphere
	DrawDebugSphere(
		World,
		CurrentGoal,
		ReachThreshold,
		12,
		LineColor,
		false,
		DebugDrawDuration,
		0,
		1.0f
	);

	// Draw agent position
	DrawDebugSphere(
		World,
		AgentLocation,
		30.0f,
		12,
		FColor::Yellow,
		false,
		DebugDrawDuration
	);

	// Draw navigation radius in autonomous mode
	if (NavMode == ENavMode::Autonomous)
	{
		DrawDebugCircle(
			World,
			AgentLocation,
			AutonomousRadius,
			32,
			FColor::Cyan,
			false,
			DebugDrawDuration,
			0,
			1.0f,
			FVector(0, 1, 0),
			FVector(1, 0, 0)
		);
	}

	// Draw text info
	FString DebugText = FString::Printf(
		TEXT("Mode: %s\nSteps: %d/%d\nDist: %.1f"),
		NavMode == ENavMode::Autonomous ? TEXT("Autonomous") : TEXT("ToTarget"),
		StepCounter,
		MaxSteps,
		FVector::Dist2D(AgentLocation, CurrentGoal)
	);

	DrawDebugString(
		World,
		AgentLocation + FVector(0, 0, 150),
		DebugText,
		nullptr,
		FColor::White,
		DebugDrawDuration,
		true
	);
}
