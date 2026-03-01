#include "EditorHandler.h"
#include "UnrealcvLog.h"

void FEditorHandler::RegisterCommands()
{
#if WITH_EDITOR
	FDispatcherDelegate Cmd;
	FString Help;

	Cmd = FDispatcherDelegate::CreateRaw(this, &FEditorHandler::StartStandalonePIE);
	Help = "Start Standalone PIE in a separate process";
	CommandDispatcher->BindCommand("vset /editor/start_standalone_pie", Cmd, Help);
#endif
}

#if WITH_EDITOR
FExecStatus FEditorHandler::StartStandalonePIE(const TArray<FString>& Args)
{
	UE_LOG(LogUnrealCV, Log, TEXT("Starting Standalone PIE via TCP command"));

	FRequestPlaySessionParams Params;
	Params.SessionDestination = EPlaySessionDestinationType::NewProcess;
	Params.EditorPlaySettings = NewObject<ULevelEditorPlaySettings>();
	Params.EditorPlaySettings->SetPlayNetMode(EPlayNetMode::PIE_Standalone);

	UEditorEngine* Editor = (UEditorEngine*)GEngine;
	if (Editor)
	{
		Editor->RequestPlaySession(Params);
		return FExecStatus::OK("Standalone PIE started");
	}
	else
	{
		return FExecStatus::Error("Failed to get UEditorEngine");
	}
}
#endif
