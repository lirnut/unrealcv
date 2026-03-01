#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "HAL/PlatformTime.h"
#include "Containers/Ticker.h"

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
	static void OnStartStandalonePIE();
	static void OnEnableAutoStandalonePIE();
	static void OnDisableAutoStandalonePIE();
	bool TickAutoStandalonePIE(float DeltaTime);

	static bool bAutoStandalonePIEEnabled;
	static double LastAutoPIEStartTime;
	static constexpr double AutoPIEIntervalSeconds = 10.0;
	static FTSTicker::FDelegateHandle TickerHandle;

	TSharedPtr<class FMetaHumanEditorCommands> Commands;
	TSharedPtr<FUICommandList> PluginCommands;
};
