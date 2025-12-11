#include "FMetaHumanEditorCommands.h"

#define LOCTEXT_NAMESPACE "FMetaHumanEditorCommands"

FMetaHumanEditorCommands::FMetaHumanEditorCommands()
	: TCommands<FMetaHumanEditorCommands>(
		TEXT("MetaHumanEditor"),
		NSLOCTEXT("Contexts", "MetaHumanEditor", "MetaHuman Editor"),
		NAME_None,
		FAppStyle::GetAppStyleSetName())
{
}

void FMetaHumanEditorCommands::RegisterCommands()
{
	UI_COMMAND(SearchAndSaveCache,
		"Search & Save Cache",
		"Search for all MetaHumans and save to cache file",
		EUserInterfaceActionType::Button,
		FInputChord());

	UI_COMMAND(SearchSaveAndSetAnimation,
		"Search, Save & Set Animation",
		"Search for all MetaHumans, save to cache, and set animation blueprint",
		EUserInterfaceActionType::Button,
		FInputChord());
}

#undef LOCTEXT_NAMESPACE
