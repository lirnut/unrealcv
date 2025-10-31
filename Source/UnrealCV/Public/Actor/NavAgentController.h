// Copyright (c) 2025 UnrealCV
// Navigation Agent Controller for autonomous character movement
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NavAgentController.generated.h"

/**
 * Navigation mode for agent control
 */
UENUM(BlueprintType)
enum class ENavMode : uint8
{
	/** Agent navigates autonomously to random goals within radius */
	Autonomous UMETA(DisplayName = "Autonomous Navigation"),

	/** Agent navigates to specific target position */
	ToTarget UMETA(DisplayName = "Navigate to Target"),

	/** Agent navigation is paused */
	Paused UMETA(DisplayName = "Paused")
};

/**
 * Navigation Agent Controller
 *
 * This actor controls character movement using UE5's NavMesh system.
 * It provides two navigation modes:
 * 1. Autonomous: Agent randomly explores within a radius
 * 2. ToTarget: Agent navigates to a specific position
 *
 * This replaces the Python Nav2GoalAgent with C++ for better performance.
 * Uses UE5's built-in NavMesh for pathfinding and obstacle avoidance.
 *
 * Usage:
 *   - Spawn this actor in level or via C++
 *   - Set ControlledAgent to the character you want to control
 *   - Call StartAutonomousNav() or NavigateToPosition()
 *   - The actor will automatically manage navigation via Tick
 *
 * Benefits over Python Nav2GoalAgent:
 *   - No network latency
 *   - Automatic obstacle avoidance via NavMesh
 *   - No stuck detection needed (NavMesh handles it)
 *   - More reliable path planning
 */
UCLASS()
class UNREALCV_API ANavAgentController : public AActor
{
	GENERATED_BODY()

public:
	ANavAgentController();

	virtual void Tick(float DeltaTime) override;
	virtual void BeginPlay() override;

	// ========== Public API Functions ==========

	/**
	 * Start autonomous navigation - agent will explore randomly
	 * @param Radius - Max distance from current position for goal generation
	 */
	UFUNCTION(BlueprintCallable, Category = "Navigation")
	void StartAutonomousNav(float Radius = 1000.0f);

	/**
	 * Navigate to a specific world position
	 * @param TargetLocation - World position to navigate to
	 */
	UFUNCTION(BlueprintCallable, Category = "Navigation")
	void NavigateToPosition(FVector TargetLocation);

	/**
	 * Stop navigation - agent stops moving
	 */
	UFUNCTION(BlueprintCallable, Category = "Navigation")
	void StopNavigation();

	/**
	 * Check if agent is currently navigating
	 */
	UFUNCTION(BlueprintCallable, Category = "Navigation")
	bool IsNavigating() const { return NavMode != ENavMode::Paused; }

	/**
	 * Get current navigation mode
	 */
	UFUNCTION(BlueprintCallable, Category = "Navigation")
	ENavMode GetNavMode() const { return NavMode; }

	/**
	 * Get current goal position
	 */
	UFUNCTION(BlueprintCallable, Category = "Navigation")
	FVector GetCurrentGoal() const { return CurrentGoal; }

	// ========== Configuration ==========

	/** The agent (Character/Pawn) to control */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation|Target")
	AActor* ControlledAgent;

	/** Distance threshold to consider goal "reached" */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation|Settings")
	float ReachThreshold = 50.0f;

	/** Max steps before generating new goal (timeout) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation|Settings")
	int32 MaxSteps = 200;

	/** Default radius for autonomous navigation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation|Settings")
	float DefaultNavRadius = 1000.0f;

	/** Minimum radius for autonomous navigation (prevents generating goals too close) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation|Settings")
	float MinNavRadius = 200.0f;

	/** Enable debug visualization */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation|Debug")
	bool bDebugDraw = false;

	/** Debug draw duration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation|Debug")
	float DebugDrawDuration = 0.1f;

protected:
	// ========== Internal State ==========

	/** Current navigation mode */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Navigation|State")
	ENavMode NavMode;

	/** Current goal position */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Navigation|State")
	FVector CurrentGoal;

	/** Last known position (for stuck detection) */
	FVector LastPosition;

	/** Step counter for timeout */
	int32 StepCounter;

	/** Whether we have a valid goal */
	bool bHasValidGoal;

	/** Radius for autonomous navigation */
	float AutonomousRadius;

	// ========== Internal Functions ==========

	/**
	 * Generate a new random goal using NavMesh
	 * @return true if a valid goal was found
	 */
	bool GenerateRandomGoal();

	/**
	 * Check if agent has reached current goal
	 */
	bool CheckReachedGoal() const;

	/**
	 * Send navigation command to the controlled agent
	 * Calls the Blueprint function: nav_to_goal
	 */
	void SendNavCommand();

	/**
	 * Draw debug visualization
	 */
	void DrawDebug();
};
