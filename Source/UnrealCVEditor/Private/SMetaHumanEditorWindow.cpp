#include "SMetaHumanEditorWindow.h"
#include "BPFunctionLib/MetaHumanBPLib.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "SMetaHumanEditorWindow"

void SMetaHumanEditorWindow::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(10.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 10.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Title", "MetaHuman Manager"))
				.TextStyle(FAppStyle::Get(), "Heading1")
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 20.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Description", "Manage MetaHuman cache and animation settings"))
				.TextStyle(FAppStyle::Get(), "SmallText")
				.ColorAndOpacity(FLinearColor::Gray)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 10.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(5.0f)
				[
					SNew(SButton)
					.OnClicked(this, &SMetaHumanEditorWindow::OnSearchAndSaveCache)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.ContentPadding(FMargin(20.0f, 10.0f))
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SearchAndSave", "Search & Save Cache"))
						.Justification(ETextJustify::Center)
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(5.0f)
				[
					SNew(SButton)
					.OnClicked(this, &SMetaHumanEditorWindow::OnSearchSaveAndSetAnimation)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.ContentPadding(FMargin(20.0f, 10.0f))
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SearchSaveSet", "Search, Save & Set Animation"))
						.Justification(ETextJustify::Center)
					]
				]
			]
		]
	];
}

FReply SMetaHumanEditorWindow::OnSearchAndSaveCache()
{
	TArray<FString> MetaHumanPaths = UMetaHumanBPLib::GetAllMetaHumanBlueprintPaths();

	if (MetaHumanPaths.Num() > 0)
	{
		FString Message = FString::Printf(TEXT("Found and saved %d MetaHumans to cache"), MetaHumanPaths.Num());
		DisplayNotification(Message, true);
		UE_LOG(LogTemp, Log, TEXT("MetaHuman Editor: %s"), *Message);
	}
	else
	{
		FString Message = TEXT("No MetaHumans found or loaded from cache");
		DisplayNotification(Message, false);
		UE_LOG(LogTemp, Warning, TEXT("MetaHuman Editor: %s"), *Message);
	}

	return FReply::Handled();
}

FReply SMetaHumanEditorWindow::OnSearchSaveAndSetAnimation()
{
	TArray<FString> AllMetaHumans = UMetaHumanBPLib::GetAllMetaHumanBlueprintPaths();

	if (AllMetaHumans.IsEmpty())
	{
		DisplayNotification(TEXT("No MetaHumans found"), false);
		return FReply::Handled();
	}

	FString AnimBlueprintPath = TEXT("/Game/MetaHumans/ABP_RandomIdle");
	TArray<FString> SuccessfulPaths = UMetaHumanBPLib::SetupAllMetaHumansWithAnimation(AnimBlueprintPath);

	FString Message = FString::Printf(
		TEXT("Successfully configured %d/%d MetaHumans with animation"),
		SuccessfulPaths.Num(),
		AllMetaHumans.Num());
	DisplayNotification(Message, SuccessfulPaths.Num() > 0);
	UE_LOG(LogTemp, Log, TEXT("MetaHuman Editor: %s"), *Message);

	return FReply::Handled();
}

void SMetaHumanEditorWindow::DisplayNotification(const FString& Message, bool bSuccess)
{
	FNotificationInfo NotificationInfo(FText::FromString(Message));
	NotificationInfo.FadeOutDuration = 3.0f;
	NotificationInfo.ExpireDuration = 5.0f;

	if (bSuccess)
	{
		NotificationInfo.Image = FAppStyle::GetBrush("Icons.Success");
	}
	else
	{
		NotificationInfo.Image = FAppStyle::GetBrush("Icons.Warning");
	}

	FSlateNotificationManager::Get().AddNotification(NotificationInfo);
}

#undef LOCTEXT_NAMESPACE
