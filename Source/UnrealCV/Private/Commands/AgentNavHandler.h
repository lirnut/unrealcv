// Copyright (c) 2025 UnrealCV
// Agent navigation command handler
#pragma once
#include "CommandHandler.h"

/**
 * Command handler for agent navigation control.
 * Handles TCP API commands for NavAgentController.
 *
 * Supported commands:
 *   vset /agent/[str]/nav/start [float] - Start autonomous navigation with radius
 */
class FAgentNavHandler : public FCommandHandler
{
public:
	void RegisterCommands();

private:
	/** vset /agent/[str]/nav/start [float] - Start autonomous navigation */
	FExecStatus StartAutonomousNav(const TArray<FString>& Args);

	/** vset /agent/[str]/nav/goto [float] [float] [float] - Navigate to position */
	FExecStatus NavigateToPosition(const TArray<FString>& Args);

	/** vset /agent/[str]/nav/stop - Stop navigation */
	FExecStatus StopNav(const TArray<FString>& Args);

	/** vget /agent/[str]/nav/status - Get navigation status */
	FExecStatus GetNavStatus(const TArray<FString>& Args);

	// Helper functions
	AActor* FindAgentByName(const FString& AgentName);
};
