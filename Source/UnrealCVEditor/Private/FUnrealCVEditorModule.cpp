#include "FUnrealCVEditorModule.h"
#include "FMetaHumanEditorCommands.h"
#include "Framework/Application/SlateApplication.h"
#include "LevelEditor.h"
#include "ToolMenus.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "BPFunctionLib/MetaHumanBPLib.h"

#define LOCTEXT_NAMESPACE "FUnrealCVEditorModule"

IMPLEMENT_MODULE(FUnrealCVEditorModule, UnrealCVEditor)

void FUnrealCVEditorModule::StartupModule()
{
	FMetaHumanEditorCommands::Register();

	PluginCommands = MakeShareable(new FUICommandList);

	RegisterMenus();
}

void FUnrealCVEditorModule::ShutdownModule()
{
	FMetaHumanEditorCommands::Unregister();
	UToolMenus::UnregisterOwner(this);
}

void FUnrealCVEditorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);
	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.User");
		if (!Menu)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to extend LevelEditor.LevelEditorToolBar.User menu"));
			return;
		}

		FToolMenuSection& Section = Menu->FindOrAddSection("MetaHumanTools");
		Section.Label = LOCTEXT("MetaHumanTools", "MetaHuman Tools");

		Section.AddSubMenu(
			"MetaHumanCacheManager",
			LOCTEXT("MetaHumanCacheManagerLabel", "MetaHuman Cache Manager"),
			LOCTEXT("MetaHumanCacheManagerTooltip", "Search and manage MetaHuman blueprints and cache"),
			FNewToolMenuDelegate::CreateLambda([](UToolMenu* SubMenu)
			{
				FToolMenuSection& CacheSection = SubMenu->AddSection("CacheManagement", LOCTEXT("CacheManagement", "Cache Management"));

				CacheSection.AddMenuEntry(
					"SearchAndSaveCache",
					LOCTEXT("SearchAndSaveCacheLabel", "Search & Save Cache"),
					LOCTEXT("SearchAndSaveCacheTooltip", "Search for all MetaHumans and save to cache file"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateStatic(&FUnrealCVEditorModule::OnSearchAndSaveCache))
				);

				CacheSection.AddMenuEntry(
					"SearchSaveAndSetAnimation",
					LOCTEXT("SearchSaveAndSetAnimationLabel", "Search, Save & Set Animation"),
					LOCTEXT("SearchSaveAndSetAnimationTooltip", "Search for all MetaHumans, save to cache, and set animation blueprint"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateStatic(&FUnrealCVEditorModule::OnSearchSaveAndSetAnimation))
				);

				CacheSection.AddMenuEntry(
					"SearchSaveSetAnimationAndSpawn",
					LOCTEXT("SearchSaveSetAnimationAndSpawnLabel", "Search, Save, Set Animation & Spawn to Map"),
					LOCTEXT("SearchSaveSetAnimationAndSpawnTooltip", "Search for all MetaHumans, save to cache, set animation, and spawn to map"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateStatic(&FUnrealCVEditorModule::OnSearchSaveSetAnimationAndSpawn))
				);

				CacheSection.AddMenuEntry(
					"Cancel",
					LOCTEXT("CancelLabel", "Cancel"),
					LOCTEXT("CancelTooltip", "Cancel any async procedure"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateStatic(&FUnrealCVEditorModule::OnCancel))
				);
			}),
			false,
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Settings")
		);
	}
}

void FUnrealCVEditorModule::OnSearchAndSaveCache()
{
	UE_LOG(LogTemp, Log, TEXT("=== MetaHuman Cache Manager: Search & Save Cache ==="));

	TArray<FString> MetaHumanPaths = UMetaHumanBPLib::GetAllMetaHumanBlueprintPaths();

	if (MetaHumanPaths.Num() > 0)
	{
		FString Message = FString::Printf(TEXT("Found and saved %d MetaHumans to cache"), MetaHumanPaths.Num());
		UE_LOG(LogTemp, Log, TEXT("%s"), *Message);

		FNotificationInfo SuccessInfo(FText::FromString(Message));
		SuccessInfo.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification(SuccessInfo);
	}
	else
	{
		FString Message = TEXT("No MetaHumans found or loaded from cache");
		UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);

		FNotificationInfo WarningInfo(FText::FromString(Message));
		WarningInfo.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification(WarningInfo);
	}
}

void FUnrealCVEditorModule::OnSearchSaveAndSetAnimation()
{
	UE_LOG(LogTemp, Log, TEXT("=== MetaHuman Cache Manager: Search, Save & Set Animation ==="));

	TArray<FString> AllMetaHumans = UMetaHumanBPLib::GetAllMetaHumanBlueprintPaths();

	if (AllMetaHumans.IsEmpty())
	{
		FString Message = TEXT("No MetaHumans found");
		UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);

		FNotificationInfo WarningInfo(FText::FromString(Message));
		WarningInfo.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(WarningInfo);
		return;
	}

	FString AnimBlueprintPath = TEXT("/Game/MetaHumans/ABP_RandomIdle.ABP_RandomIdle_C");
	UMetaHumanBPLib::SetupAllMetaHumansWithAnimation(AnimBlueprintPath);

	FString Message = TEXT("Set Anim started ...");
	UE_LOG(LogTemp, Log, TEXT("%s"), *Message);

	FNotificationInfo SuccessInfo(FText::FromString(Message));
	SuccessInfo.ExpireDuration = 5.0f;
	FSlateNotificationManager::Get().AddNotification(SuccessInfo);
}

void FUnrealCVEditorModule::OnSearchSaveSetAnimationAndSpawn()
{
	UE_LOG(LogTemp, Log, TEXT("=== MetaHuman Cache Manager: Search, Save, Set Animation & Spawn to Map ==="));

	FString AnimBlueprintPath = TEXT("/Game/MetaHumans/ABP_RandomIdle.ABP_RandomIdle_C");
	UMetaHumanBPLib::SpawnAllMetaHumansToMap(AnimBlueprintPath);

	FString Message = TEXT("Spawned started ...");
	UE_LOG(LogTemp, Log, TEXT("%s"), *Message);

	FNotificationInfo SuccessInfo(FText::FromString(Message));
	SuccessInfo.ExpireDuration = 5.0f;
	FSlateNotificationManager::Get().AddNotification(SuccessInfo);
}

void FUnrealCVEditorModule::OnCancel()
{
	UE_LOG(LogTemp, Log, TEXT("Canceling async operation"));
	UMetaHumanBPLib::CancelAsyncOperation();
}

#undef LOCTEXT_NAMESPACE

