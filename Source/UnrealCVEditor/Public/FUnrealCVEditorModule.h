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

	static void OnAnnotateWorld();
	static void OnDeannotateWorld();
	static void OnSearchAndSaveCache();
	static void OnSearchSaveAndSetAnimation();
	static void OnSearchSaveSetAnimationAndSpawn();
	static void OnCancel();
	static void OnEnableDeepShadow();

	TSharedPtr<class FMetaHumanEditorCommands> Commands;
	TSharedPtr<FUICommandList> PluginCommands;
};
