// UnrealCV Recording Control HUD
// Provides in-game UI for controlling recording without TCP server
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RecordingHUD.generated.h"

/**
 * HUD class that spawns and manages the in-game recording control widget.
 * This allows users to control recording directly from the game without using the TCP server.
 *
 * Usage:
 * 1. Set this HUD class in your GameMode Blueprint
 * 2. Press F1 in-game to toggle the recording control UI
 * 3. Or call ToggleRecordingWidget() from Blueprint/C++
 */
UCLASS()
class UNREALCV_API ARecordingHUD : public AHUD
{
	GENERATED_BODY()

public:
	ARecordingHUD();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/**
	 * Toggle the recording widget visibility (on/off).
	 * Can be called from Blueprint or bound to a key.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealCV|UI")
	void ToggleRecordingWidget();

	/**
	 * Show the recording widget.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealCV|UI")
	void ShowRecordingWidget();

	/**
	 * Hide the recording widget.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealCV|UI")
	void HideRecordingWidget();

	/**
	 * Check if the recording widget is currently visible.
	 */
	UFUNCTION(BlueprintPure, Category = "UnrealCV|UI")
	bool IsRecordingWidgetVisible() const;

protected:
	/**
	 * The widget class to spawn for recording control.
	 * This should be a UMG Widget Blueprint (created in the Editor).
	 * Default path: /UnrealCV/UI/WBP_RecordingControl
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "UnrealCV|UI")
	TSubclassOf<class UUserWidget> RecordingWidgetClass;

	/**
	 * The spawned recording widget instance.
	 */
	UPROPERTY()
	class UUserWidget* RecordingWidgetInstance;

	/**
	 * Whether to automatically spawn the widget on BeginPlay.
	 * Default: true
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "UnrealCV|UI")
	bool bAutoSpawnWidget;

	/**
	 * Whether to show the widget by default when spawned.
	 * If false, widget is hidden until user presses toggle key.
	 * Default: false (hidden until F1 pressed)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "UnrealCV|UI")
	bool bShowWidgetOnSpawn;

	/**
	 * Whether to enable input while the widget is visible.
	 * Default: true
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "UnrealCV|UI")
	bool bEnableInputWhileVisible;

private:
	/**
	 * Spawn the recording widget instance.
	 */
	void SpawnRecordingWidget();

	/**
	 * Handle input for toggling the widget (F1 key by default).
	 */
	void SetupInputBindings();
};
