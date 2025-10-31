// Copyright (c) 2025 UnrealCV
// Navigation control function library for Blueprint/C++ access
#pragma once

#include "Runtime/Engine/Classes/Kismet/BlueprintFunctionLibrary.h"
#include "Actor/NavAgentController.h"
#include "NavigationBPLib.generated.h"

/**
 * Blueprint Function Library for controlling agent navigation without TCP server.
 * Provides easy access to NavAgentController functionality from Blueprints and C++.
 *
 * Example usage in C++:
 *   UNavigationBPLib::StartAutonomousNavigation(MyCharacter, 1000.0f);
 *
 * Example usage in Blueprint:
 *   Create a UI button → Call "Start Autonomous Navigation" with target actor and radius
 */
UCLASS()
class UNREALCV_API UNavigationBPLib : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ========== Agent Control Functions ==========

	/**
	 * Start autonomous navigation for an agent.
	 * The agent will explore randomly within the specified radius.
	 *
	 * @param Agent The actor to control (must have 'nav_to_goal' Blueprint function)
	 * @param Radius Max distance for goal generation
	 * @return The created NavAgentController actor, or nullptr if failed
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Navigation", meta = (WorldContext = "WorldContextObject"))
	static ANavAgentController* StartAutonomousNavigation(
		UObject* WorldContextObject,
		AActor* Agent,
		float Radius = 1000.0f
	);

	/**
	 * Navigate an agent to a specific position.
	 * The agent will use NavMesh pathfinding to reach the target.
	 *
	 * @param Agent The actor to control
	 * @param TargetLocation World position to navigate to
	 * @return The created NavAgentController actor, or nullptr if failed
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Navigation", meta = (WorldContext = "WorldContextObject"))
	static ANavAgentController* NavigateAgentToPosition(
		UObject* WorldContextObject,
		AActor* Agent,
		FVector TargetLocation
	);

	/**
	 * Stop navigation for an agent.
	 * Finds the NavAgentController controlling this agent and stops it.
	 *
	 * @param Agent The actor to stop
	 * @return True if navigation was stopped, false if agent was not navigating
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Navigation", meta = (WorldContext = "WorldContextObject"))
	static bool StopNavigation(
		UObject* WorldContextObject,
		AActor* Agent
	);

	/**
	 * Check if an agent is currently navigating.
	 *
	 * @param Agent The actor to check
	 * @return True if agent is navigating, false otherwise
	 */
	UFUNCTION(BlueprintPure, Category = "UnrealCV|Navigation", meta = (WorldContext = "WorldContextObject"))
	static bool IsNavigating(
		UObject* WorldContextObject,
		AActor* Agent
	);

	/**
	 * Get the NavAgentController controlling a specific agent.
	 *
	 * @param Agent The actor being controlled
	 * @return The NavAgentController, or nullptr if not found
	 */
	UFUNCTION(BlueprintPure, Category = "UnrealCV|Navigation", meta = (WorldContext = "WorldContextObject"))
	static ANavAgentController* GetNavController(
		UObject* WorldContextObject,
		AActor* Agent
	);

	// ========== Batch Control Functions ==========

	/**
	 * Start autonomous navigation for multiple agents.
	 * Convenient for controlling crowds or multiple NPCs.
	 *
	 * @param Agents Array of actors to control
	 * @param Radius Max distance for goal generation
	 * @return Array of created NavAgentController actors
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Navigation", meta = (WorldContext = "WorldContextObject"))
	static TArray<ANavAgentController*> StartAutonomousNavigationBatch(
		UObject* WorldContextObject,
		const TArray<AActor*>& Agents,
		float Radius = 1000.0f
	);

	/**
	 * Stop navigation for multiple agents.
	 *
	 * @param Agents Array of actors to stop
	 * @return Number of agents successfully stopped
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Navigation", meta = (WorldContext = "WorldContextObject"))
	static int32 StopNavigationBatch(
		UObject* WorldContextObject,
		const TArray<AActor*>& Agents
	);

	/**
	 * Get all NavAgentController actors in the scene.
	 *
	 * @return Array of all NavAgentController actors
	 */
	UFUNCTION(BlueprintPure, Category = "UnrealCV|Navigation", meta = (WorldContext = "WorldContextObject"))
	static TArray<ANavAgentController*> GetAllNavControllers(UObject* WorldContextObject);

	// ========== Utility Functions ==========

	/**
	 * Check if a position is reachable on the NavMesh.
	 *
	 * @param Position World position to check
	 * @param QueryExtent Search extent (default 500x500x500)
	 * @return True if position is on NavMesh, false otherwise
	 */
	UFUNCTION(BlueprintPure, Category = "UnrealCV|Navigation", meta = (WorldContext = "WorldContextObject"))
	static bool IsPositionReachable(
		UObject* WorldContextObject,
		FVector Position,
		FVector QueryExtent = FVector(500, 500, 500)
	);

	/**
	 * Get a random reachable position on NavMesh within radius.
	 * Useful for goal generation or spawn point selection.
	 *
	 * @param Origin Center position
	 * @param Radius Search radius
	 * @param OutPosition Output reachable position
	 * @return True if a position was found, false otherwise
	 */
	UFUNCTION(BlueprintPure, Category = "UnrealCV|Navigation", meta = (WorldContext = "WorldContextObject"))
	static bool GetRandomReachablePosition(
		UObject* WorldContextObject,
		FVector Origin,
		float Radius,
		FVector& OutPosition
	);

	/**
	 * Project a position onto the NavMesh (find closest point on NavMesh).
	 *
	 * @param Position Position to project
	 * @param OutProjectedPosition Output position on NavMesh
	 * @param QueryExtent Search extent
	 * @return True if projection succeeded, false otherwise
	 */
	UFUNCTION(BlueprintPure, Category = "UnrealCV|Navigation", meta = (WorldContext = "WorldContextObject"))
	static bool ProjectPositionToNavMesh(
		UObject* WorldContextObject,
		FVector Position,
		FVector& OutProjectedPosition,
		FVector QueryExtent = FVector(500, 500, 500)
	);

private:
	/**
	 * Find existing NavAgentController for an agent.
	 * @return NavAgentController, or nullptr if not found
	 */
	static ANavAgentController* FindExistingController(UObject* WorldContextObject, AActor* Agent);

	/**
	 * Create or reuse a NavAgentController for an agent.
	 * @return NavAgentController, or nullptr if failed
	 */
	static ANavAgentController* GetOrCreateController(UObject* WorldContextObject, AActor* Agent);
};
