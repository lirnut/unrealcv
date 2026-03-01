#include "FUnrealCVEditorModule.h"
#include "FMetaHumanEditorCommands.h"
#include "Framework/Application/SlateApplication.h"
#include "LevelEditor.h"
#include "ToolMenus.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "BPFunctionLib/MetaHumanBPLib.h"
#include "BPFunctionLib/AnnotationBPLib.h"
#include "BPFunctionLib/LightBPLib.h"
#include "Editor/UnrealEd/Public/PlayInEditorDataTypes.h"
#include "Editor/UnrealEd/Classes/Settings/LevelEditorPlaySettings.h"

#define LOCTEXT_NAMESPACE "FUnrealCVEditorModule"

IMPLEMENT_MODULE(FUnrealCVEditorModule, UnrealCVEditor)

bool FUnrealCVEditorModule::bAutoStandalonePIEEnabled = false;
double FUnrealCVEditorModule::LastAutoPIEStartTime = 0.0;
FTSTicker::FDelegateHandle FUnrealCVEditorModule::TickerHandle;

void FUnrealCVEditorModule::StartupModule()
{
	// FMetaHumanEditorCommands::Register();

	PluginCommands = MakeShareable(new FUICommandList);

	RegisterMenus();

	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([this](float DeltaTime)
		{
			TickAutoStandalonePIE(DeltaTime);
			return true;
		}),
		1.0f
	);
	UE_LOG(LogTemp, Log, TEXT("Auto PIE ticker registered, handle valid: %d"), TickerHandle.IsValid());
}

void FUnrealCVEditorModule::ShutdownModule()
{
	// FMetaHumanEditorCommands::Unregister();
	UToolMenus::UnregisterOwner(this);
	FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
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
					"AnnotateWorld",
					LOCTEXT("AnnotateWorldLabel", "AnnotateWorld"),
					LOCTEXT("AnnotateWorldTooltip", "AnnotateWorld"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateStatic(&FUnrealCVEditorModule::OnAnnotateWorld))
				);

				CacheSection.AddMenuEntry(
					"DeannotateWorld",
					LOCTEXT("DeannotateWorldLabel", "DeannotateWorld"),
					LOCTEXT("DeannotateWorldTooltip", "DeannotateWorld"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateStatic(&FUnrealCVEditorModule::OnDeannotateWorld))
				);

				CacheSection.AddMenuEntry(
					"EnableDeepShadow",
					LOCTEXT("EnableDeepShadowLabel", "Enable Deep Shadow"),
					LOCTEXT("EnableDeepShadowTooltip", "Enable cast deep shadow for directional light"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateStatic(&FUnrealCVEditorModule::OnEnableDeepShadow))
				);

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

				CacheSection.AddMenuEntry(
					"StartStandalonePIE",
					LOCTEXT("StartStandalonePIELabel", "Start Standalone PIE"),
					LOCTEXT("StartStandalonePIETooltip", "Launch Standalone PIE in a separate process"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateStatic(&FUnrealCVEditorModule::OnStartStandalonePIE))
				);

				CacheSection.AddMenuEntry(
					"EnableAutoStandalonePIE",
					LOCTEXT("EnableAutoStandalonePIELabel", "Enable Auto Standalone PIE"),
					LOCTEXT("EnableAutoStandalonePIETooltip", "Automatically start Standalone PIE every x minutes"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateStatic(&FUnrealCVEditorModule::OnEnableAutoStandalonePIE))
				);

				CacheSection.AddMenuEntry(
					"DisableAutoStandalonePIE",
					LOCTEXT("DisableAutoStandalonePIELabel", "Disable Auto Standalone PIE"),
					LOCTEXT("DisableAutoStandalonePIETooltip", "Stop automatically starting Standalone PIE"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateStatic(&FUnrealCVEditorModule::OnDisableAutoStandalonePIE))
				);
			}),
			false,
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Settings")
		);
	}
}

void FUnrealCVEditorModule::OnAnnotateWorld()
{
	UAnnotationBPLib::AnnotateWorld();
}

void FUnrealCVEditorModule::OnDeannotateWorld()
{
	UAnnotationBPLib::DeannotateWorld();
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

void FUnrealCVEditorModule::OnEnableDeepShadow()
{
	UE_LOG(LogTemp, Log, TEXT("=== Enable Cast Deep Shadow ==="));

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		FString Message = TEXT("Failed to get world context");
		UE_LOG(LogTemp, Error, TEXT("%s"), *Message);

		FNotificationInfo ErrorInfo(FText::FromString(Message));
		ErrorInfo.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(ErrorInfo);
		return;
	}

	bool bSuccess = ULightBPLib::SetDirectionalLightCastDeepShadow(World, true);

	if (bSuccess)
	{
		FString Message = TEXT("Enabled cast deep shadow for directional light");
		UE_LOG(LogTemp, Log, TEXT("%s"), *Message);

		FNotificationInfo SuccessInfo(FText::FromString(Message));
		SuccessInfo.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(SuccessInfo);
	}
	else
	{
		FString Message = TEXT("Failed to enable cast deep shadow (no directional light found)");
		UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);

		FNotificationInfo WarningInfo(FText::FromString(Message));
		WarningInfo.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(WarningInfo);
	}
}

void FUnrealCVEditorModule::OnStartStandalonePIE()
{
	UE_LOG(LogTemp, Log, TEXT("=== Start Standalone PIE ==="));

	FRequestPlaySessionParams Params;
	Params.SessionDestination = EPlaySessionDestinationType::NewProcess;
	Params.EditorPlaySettings = NewObject<ULevelEditorPlaySettings>();
	Params.EditorPlaySettings->SetPlayNetMode(EPlayNetMode::PIE_Standalone);

	UEditorEngine* Editor = (UEditorEngine*)GEngine;
	if (Editor)
	{
		Editor->RequestPlaySession(Params);
		UE_LOG(LogTemp, Log, TEXT("Standalone PIE requested"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to get UEditorEngine"));
	}
}

void FUnrealCVEditorModule::OnEnableAutoStandalonePIE()
{
	UE_LOG(LogTemp, Log, TEXT("=== Enable Auto Standalone PIE (8min) ==="));

	bAutoStandalonePIEEnabled = true;
	LastAutoPIEStartTime = FPlatformTime::Seconds();

	FNotificationInfo Info(LOCTEXT("AutoStandalonePIEEnabled", "Auto Standalone PIE enabled (every 8 minutes)"));
	Info.ExpireDuration = 3.0f;
	FSlateNotificationManager::Get().AddNotification(Info);
}

void FUnrealCVEditorModule::OnDisableAutoStandalonePIE()
{
	UE_LOG(LogTemp, Log, TEXT("=== Disable Auto Standalone PIE ==="));

	bAutoStandalonePIEEnabled = false;

	FNotificationInfo Info(LOCTEXT("AutoStandalonePIEDisabled", "Auto Standalone PIE disabled"));
	Info.ExpireDuration = 3.0f;
	FSlateNotificationManager::Get().AddNotification(Info);
}

bool FUnrealCVEditorModule::TickAutoStandalonePIE(float DeltaTime)
{
	UE_LOG(LogTemp, Log, TEXT("TickAutoStandalonePIE"));
	if (!bAutoStandalonePIEEnabled)
	{
		return true;
	}

	double CurrentTime = FPlatformTime::Seconds();
	if (CurrentTime - LastAutoPIEStartTime >= AutoPIEIntervalSeconds)
	{
		UE_LOG(LogTemp, Log, TEXT("=== Auto Starting Standalone PIE ==="));
		OnStartStandalonePIE();
		LastAutoPIEStartTime = CurrentTime;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE

