// Copyright (c) 2025 UnrealCV
// Navigation control function library for Blueprint/C++ access
#include "NavigationBPLib.h"
#include "NavigationSystem.h"
#include "EngineUtils.h"
#include "UnrealcvLog.h"

// ========== Agent Control Functions ==========

ANavAgentController* UNavigationBPLib::StartAutonomousNavigation(
	UObject* WorldContextObject,
	AActor* Agent,
	float Radius)
{
	if (!Agent || !WorldContextObject)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("NavigationBPLib: Invalid agent or world context"));
		return nullptr;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("NavigationBPLib: Failed to get world"));
		return nullptr;
	}

	// Get or create controller
	ANavAgentController* Controller = GetOrCreateController(WorldContextObject, Agent);
	if (!Controller)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("NavigationBPLib: Failed to create NavAgentController"));
		return nullptr;
	}

	// Start autonomous navigation
	Controller->StartAutonomousNav(Radius);

	return Controller;
}

ANavAgentController* UNavigationBPLib::NavigateAgentToPosition(
	UObject* WorldContextObject,
	AActor* Agent,
	FVector TargetLocation)
{
	if (!Agent || !WorldContextObject)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("NavigationBPLib: Invalid agent or world context"));
		return nullptr;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("NavigationBPLib: Failed to get world"));
		return nullptr;
	}

	// Get or create controller
	ANavAgentController* Controller = GetOrCreateController(WorldContextObject, Agent);
	if (!Controller)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("NavigationBPLib: Failed to create NavAgentController"));
		return nullptr;
	}

	// Navigate to position
	Controller->NavigateToPosition(TargetLocation);

	return Controller;
}

bool UNavigationBPLib::StopNavigation(UObject* WorldContextObject, AActor* Agent)
{
	ANavAgentController* Controller = FindExistingController(WorldContextObject, Agent);
	if (Controller)
	{
		Controller->StopNavigation();
		return true;
	}

	return false;
}

bool UNavigationBPLib::IsNavigating(UObject* WorldContextObject, AActor* Agent)
{
	ANavAgentController* Controller = FindExistingController(WorldContextObject, Agent);
	if (Controller)
	{
		return Controller->IsNavigating();
	}

	return false;
}

ANavAgentController* UNavigationBPLib::GetNavController(UObject* WorldContextObject, AActor* Agent)
{
	return FindExistingController(WorldContextObject, Agent);
}

// ========== Batch Control Functions ==========

TArray<ANavAgentController*> UNavigationBPLib::StartAutonomousNavigationBatch(
	UObject* WorldContextObject,
	const TArray<AActor*>& Agents,
	float Radius)
{
	TArray<ANavAgentController*> Controllers;

	for (AActor* Agent : Agents)
	{
		if (!Agent)
		{
			continue;
		}

		ANavAgentController* Controller = StartAutonomousNavigation(WorldContextObject, Agent, Radius);
		if (Controller)
		{
			Controllers.Add(Controller);
		}
	}

	UE_LOG(LogUnrealCV, Log, TEXT("NavigationBPLib: Started autonomous navigation for %d/%d agents"),
		Controllers.Num(), Agents.Num());

	return Controllers;
}

int32 UNavigationBPLib::StopNavigationBatch(UObject* WorldContextObject, const TArray<AActor*>& Agents)
{
	int32 StoppedCount = 0;

	for (AActor* Agent : Agents)
	{
		if (StopNavigation(WorldContextObject, Agent))
		{
			StoppedCount++;
		}
	}

	UE_LOG(LogUnrealCV, Log, TEXT("NavigationBPLib: Stopped navigation for %d/%d agents"),
		StoppedCount, Agents.Num());

	return StoppedCount;
}

TArray<ANavAgentController*> UNavigationBPLib::GetAllNavControllers(UObject* WorldContextObject)
{
	TArray<ANavAgentController*> Controllers;

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		return Controllers;
	}

	for (TActorIterator<ANavAgentController> It(World); It; ++It)
	{
		Controllers.Add(*It);
	}

	return Controllers;
}

// ========== Utility Functions ==========

bool UNavigationBPLib::IsPositionReachable(
	UObject* WorldContextObject,
	FVector Position,
	FVector QueryExtent)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		return false;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys)
	{
		return false;
	}

	FNavLocation ProjectedLocation;
	bool bSuccess = NavSys->ProjectPointToNavigation(Position, ProjectedLocation, QueryExtent);

	return bSuccess;
}

bool UNavigationBPLib::GetRandomReachablePosition(
	UObject* WorldContextObject,
	FVector Origin,
	float Radius,
	FVector& OutPosition)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		return false;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys)
	{
		return false;
	}

	FNavLocation ResultLocation;
	bool bSuccess = NavSys->GetRandomReachablePointInRadius(Origin, Radius, ResultLocation);

	if (bSuccess)
	{
		OutPosition = ResultLocation.Location;
	}

	return bSuccess;
}

bool UNavigationBPLib::ProjectPositionToNavMesh(
	UObject* WorldContextObject,
	FVector Position,
	FVector& OutProjectedPosition,
	FVector QueryExtent)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		return false;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys)
	{
		return false;
	}

	FNavLocation ProjectedLocation;
	bool bSuccess = NavSys->ProjectPointToNavigation(Position, ProjectedLocation, QueryExtent);

	if (bSuccess)
	{
		OutProjectedPosition = ProjectedLocation.Location;
	}

	return bSuccess;
}

// ========== Private Helper Functions ==========

ANavAgentController* UNavigationBPLib::FindExistingController(UObject* WorldContextObject, AActor* Agent)
{
	if (!Agent || !WorldContextObject)
	{
		return nullptr;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		return nullptr;
	}

	// Search for existing controller
	for (TActorIterator<ANavAgentController> It(World); It; ++It)
	{
		ANavAgentController* Controller = *It;
		if (Controller && Controller->ControlledAgent == Agent)
		{
			return Controller;
		}
	}

	return nullptr;
}

ANavAgentController* UNavigationBPLib::GetOrCreateController(UObject* WorldContextObject, AActor* Agent)
{
	// Try to find existing controller
	ANavAgentController* Controller = FindExistingController(WorldContextObject, Agent);
	if (Controller)
	{
		UE_LOG(LogUnrealCV, Verbose, TEXT("NavigationBPLib: Reusing existing controller for %s"), *Agent->GetName());
		return Controller;
	}

	// Create new controller
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = FName(*FString::Printf(TEXT("NavController_%s"), *Agent->GetName()));
	SpawnParams.Owner = Agent;

	Controller = World->SpawnActor<ANavAgentController>(ANavAgentController::StaticClass(), SpawnParams);
	if (Controller)
	{
		Controller->ControlledAgent = Agent;
		UE_LOG(LogUnrealCV, Log, TEXT("NavigationBPLib: Created new controller for %s"), *Agent->GetName());
	}
	else
	{
		UE_LOG(LogUnrealCV, Error, TEXT("NavigationBPLib: Failed to spawn NavAgentController"));
	}

	return Controller;
}
