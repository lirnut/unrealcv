// Copyright (c) 2025 UnrealCV
// shc 2025
// Agent navigation command handler
#include "AgentNavHandler.h"
#include "BPFunctionLib/NavigationBPLib.h"
#include "Actor/NavAgentController.h"
#include "Utils/UObjectUtils.h"
#include "UnrealcvLog.h"
#include "EngineUtils.h"

void FAgentNavHandler::RegisterCommands()
{
	FDispatcherDelegate Cmd;
	FString Help;

	// vset /agent/[str]/nav/start [float]
	Cmd = FDispatcherDelegate::CreateRaw(this, &FAgentNavHandler::StartAutonomousNav);
	Help = "Start autonomous navigation for an agent with specified radius";
	CommandDispatcher->BindCommand("vset /agent/[str]/nav/start [float]", Cmd, Help);

	// vset /agent/[str]/nav/goto [float] [float] [float]
	Cmd = FDispatcherDelegate::CreateRaw(this, &FAgentNavHandler::NavigateToPosition);
	Help = "Navigate agent to a specific position (x y z)";
	CommandDispatcher->BindCommand("vset /agent/[str]/nav/goto [float] [float] [float]", Cmd, Help);

	// vset /agent/[str]/nav/stop
	Cmd = FDispatcherDelegate::CreateRaw(this, &FAgentNavHandler::StopNav);
	Help = "Stop agent navigation";
	CommandDispatcher->BindCommand("vset /agent/[str]/nav/stop", Cmd, Help);

	// vget /agent/[str]/nav/status
	Cmd = FDispatcherDelegate::CreateRaw(this, &FAgentNavHandler::GetNavStatus);
	Help = "Get agent navigation status";
	CommandDispatcher->BindCommand("vget /agent/[str]/nav/status", Cmd, Help);
}

FExecStatus FAgentNavHandler::StartAutonomousNav(const TArray<FString>& Args)
{
	if (Args.Num() != 2)
	{
		return FExecStatus::Error("Usage: vset /agent/[agent_name]/nav/start [radius]");
	}

	FString AgentName = Args[0];
	float Radius = FCString::Atof(*Args[1]);

	if (Radius <= 0)
	{
		return FExecStatus::Error("Radius must be positive");
	}

	AActor* Agent = FindAgentByName(AgentName);
	if (!Agent)
	{
		return FExecStatus::Error(FString::Printf(TEXT("Agent '%s' not found"), *AgentName));
	}

	// Start autonomous navigation
	ANavAgentController* Controller = UNavigationBPLib::StartAutonomousNavigation(
		this->GetWorld(),
		Agent,
		Radius
	);

	if (Controller)
	{
		return FExecStatus::OK(FString::Printf(TEXT("Started autonomous navigation for '%s' with radius %.1f"), *AgentName, Radius));
	}
	else
	{
		return FExecStatus::Error(FString::Printf(TEXT("Failed to start navigation for '%s'"), *AgentName));
	}
}

FExecStatus FAgentNavHandler::NavigateToPosition(const TArray<FString>& Args)
{
	if (Args.Num() != 4)
	{
		return FExecStatus::Error("Usage: vset /agent/[agent_name]/nav/goto [x] [y] [z]");
	}

	FString AgentName = Args[0];
	float X = FCString::Atof(*Args[1]);
	float Y = FCString::Atof(*Args[2]);
	float Z = FCString::Atof(*Args[3]);
	FVector TargetPos(X, Y, Z);

	AActor* Agent = FindAgentByName(AgentName);
	if (!Agent)
	{
		return FExecStatus::Error(FString::Printf(TEXT("Agent '%s' not found"), *AgentName));
	}

	// Navigate to position
	ANavAgentController* Controller = UNavigationBPLib::NavigateAgentToPosition(
		this->GetWorld(),
		Agent,
		TargetPos
	);

	if (Controller)
	{
		return FExecStatus::OK(FString::Printf(TEXT("Navigating '%s' to position %s"), *AgentName, *TargetPos.ToString()));
	}
	else
	{
		return FExecStatus::Error(FString::Printf(TEXT("Failed to navigate '%s' to target"), *AgentName));
	}
}

FExecStatus FAgentNavHandler::StopNav(const TArray<FString>& Args)
{
	if (Args.Num() != 1)
	{
		return FExecStatus::Error("Usage: vset /agent/[agent_name]/nav/stop");
	}

	FString AgentName = Args[0];

	AActor* Agent = FindAgentByName(AgentName);
	if (!Agent)
	{
		return FExecStatus::Error(FString::Printf(TEXT("Agent '%s' not found"), *AgentName));
	}

	// Stop navigation
	bool bSuccess = UNavigationBPLib::StopNavigation(this->GetWorld(), Agent);

	if (bSuccess)
	{
		return FExecStatus::OK(FString::Printf(TEXT("Stopped navigation for '%s'"), *AgentName));
	}
	else
	{
		return FExecStatus::Error(FString::Printf(TEXT("Agent '%s' was not navigating"), *AgentName));
	}
}

FExecStatus FAgentNavHandler::GetNavStatus(const TArray<FString>& Args)
{
	if (Args.Num() != 1)
	{
		return FExecStatus::Error("Usage: vget /agent/[agent_name]/nav/status");
	}

	FString AgentName = Args[0];

	AActor* Agent = FindAgentByName(AgentName);
	if (!Agent)
	{
		return FExecStatus::Error(FString::Printf(TEXT("Agent '%s' not found"), *AgentName));
	}

	// Get navigation status
	ANavAgentController* Controller = UNavigationBPLib::GetNavController(this->GetWorld(), Agent);

	if (!Controller)
	{
		return FExecStatus::OK("paused");
	}

	bool bIsNavigating = Controller->IsNavigating();
	ENavMode NavMode = Controller->GetNavMode();
	FVector CurrentGoal = Controller->GetCurrentGoal();

	FString ModeStr;
	switch (NavMode)
	{
	case ENavMode::Autonomous:
		ModeStr = "autonomous";
		break;
	case ENavMode::ToTarget:
		ModeStr = "totarget";
		break;
	case ENavMode::Paused:
		ModeStr = "paused";
		break;
	default:
		ModeStr = "unknown";
	}

	FString StatusStr = FString::Printf(
		TEXT("mode:%s,goal:%s"),
		*ModeStr,
		*CurrentGoal.ToString()
	);

	return FExecStatus::OK(StatusStr);
}

AActor* FAgentNavHandler::FindAgentByName(const FString& AgentName)
{
	UWorld* World = this->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// Try to find actor by name
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && Actor->GetName() == AgentName)
		{
			return Actor;
		}
	}

	return nullptr;
}
