#pragma once

#include "CommandHandler.h"

#if WITH_EDITOR
#include "Editor/UnrealEd/Public/PlayInEditorDataTypes.h"
#include "Editor/UnrealEd/Classes/Settings/LevelEditorPlaySettings.h"
#endif

class FEditorHandler : public FCommandHandler
{
public:
	void RegisterCommands();

private:
	FExecStatus StartStandalonePIE(const TArray<FString>& Args);
};
