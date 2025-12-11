#pragma once

#include "Widgets/SCompoundWidget.h"

class SMetaHumanEditorWindow : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SMetaHumanEditorWindow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FReply OnSearchAndSaveCache();
	FReply OnSearchSaveAndSetAnimation();

	void DisplayNotification(const FString& Message, bool bSuccess = true);
};
