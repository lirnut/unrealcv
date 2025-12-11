#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FMetaHumanEditorCommands;
typedef TSharedPtr<class FUICommandList> FUICommandListPtr;

class FUnrealCVEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenus();
	static void OnSearchAndSaveCache();
	static void OnSearchSaveAndSetAnimation();

	TSharedPtr<class FMetaHumanEditorCommands> Commands;
	TSharedPtr<FUICommandList> PluginCommands;
};
