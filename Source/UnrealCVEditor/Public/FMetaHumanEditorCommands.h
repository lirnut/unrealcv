#pragma once

#include "Framework/Commands/Commands.h"
#include "Framework/Commands/UICommandInfo.h"

class FMetaHumanEditorCommands : public TCommands<FMetaHumanEditorCommands>
{
public:
	FMetaHumanEditorCommands();

	virtual void RegisterCommands() override;

public:
	TSharedPtr<FUICommandInfo> SearchAndSaveCache;
	TSharedPtr<FUICommandInfo> SearchSaveAndSetAnimation;
	TSharedPtr<FUICommandInfo> SearchSaveSetAnimationAndSpawn;
};
