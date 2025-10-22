// UnrealCV Recording Control HUD
// Provides in-game UI for controlling recording without TCP server
#include "UI/RecordingHUD.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UnrealcvLog.h"

ARecordingHUD::ARecordingHUD()
{
	PrimaryActorTick.bCanEverTick = true;

	// Default settings
	bAutoSpawnWidget = true;
	bShowWidgetOnSpawn = true;
	bEnableInputWhileVisible = true;

	// Try to load the default widget class
	// Note: This path assumes the widget blueprint will be created at this location
	// Users can override this in Blueprint or by setting it in the Editor
	static ConstructorHelpers::FClassFinder<UUserWidget> WidgetClassFinder(
		TEXT("/UnrealCV/UI/WBP_RecordingControl")
	);
	if (WidgetClassFinder.Succeeded())
	{
		RecordingWidgetClass = WidgetClassFinder.Class;
	}
	else
	{
		UE_LOG(LogUnrealCV, Error, TEXT("ARecordingHUD::ARecordingHUD: Failed to initialize RecordingWidgetClass"));
	}
}

void ARecordingHUD::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoSpawnWidget)
	{
		SpawnRecordingWidget();
	}

	SetupInputBindings();
}

void ARecordingHUD::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// You can add periodic updates here if needed
	// For example, updating recording status display
}

void ARecordingHUD::SpawnRecordingWidget()
{
	// Check if widget class is set
	if (!RecordingWidgetClass)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("ARecordingHUD::SpawnRecordingWidget: RecordingWidgetClass is not set. Please create the UMG widget blueprint and assign it."));
		return;
	}

	// Check if we already have an instance
	if (IsValid(RecordingWidgetInstance))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("ARecordingHUD::SpawnRecordingWidget: RecordingWidgetInstance already exists"));
		return;
	}

	// Get player controller
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!IsValid(PC))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("ARecordingHUD::SpawnRecordingWidget: Cannot get PlayerController"));
		return;
	}

	// Create widget instance
	RecordingWidgetInstance = CreateWidget<UUserWidget>(PC, RecordingWidgetClass);
	if (!IsValid(RecordingWidgetInstance))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("ARecordingHUD::SpawnRecordingWidget: Failed to create widget instance"));
		return;
	}

	// Add to viewport
	RecordingWidgetInstance->AddToViewport();

	// Set initial visibility
	if (bShowWidgetOnSpawn)
	{
		RecordingWidgetInstance->SetVisibility(ESlateVisibility::Visible);

		if (bEnableInputWhileVisible)
		{
			PC->bShowMouseCursor = true;
			PC->SetInputMode(FInputModeGameAndUI());
		}
	}
	else
	{
		RecordingWidgetInstance->SetVisibility(ESlateVisibility::Hidden);
	}

	UE_LOG(LogUnrealCV, Log, TEXT("ARecordingHUD::SpawnRecordingWidget: Widget spawned successfully"));
}

void ARecordingHUD::SetupInputBindings()
{
	// Get player controller
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!IsValid(PC))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("ARecordingHUD::SetupInputBindings: Cannot get PlayerController"));
		return;
	}

	// Enable input processing
	EnableInput(PC);

	// Get input component
	UInputComponent* PCInputComponent = PC->InputComponent;
	if (!IsValid(PCInputComponent))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("ARecordingHUD::SetupInputBindings: Cannot get InputComponent"));
		return;
	}

	FInputKeyBinding KeyBinding(FInputChord(EKeys::T, false, false, false, false), EInputEvent::IE_Pressed);
	KeyBinding.bConsumeInput = false; // Allow other systems to also receive key
	KeyBinding.KeyDelegate.GetDelegateForManualSet().BindUObject(this, &ARecordingHUD::ToggleRecordingWidget);
	PCInputComponent->KeyBindings.Add(KeyBinding);

	UE_LOG(LogUnrealCV, Log, TEXT("ARecordingHUD::SetupInputBindings: key bound to toggle recording UI"));
}

void ARecordingHUD::ToggleRecordingWidget()
{
	if (!IsValid(RecordingWidgetInstance))
	{
		// Try to spawn if not exists
		SpawnRecordingWidget();
		return;
	}

	if (IsRecordingWidgetVisible())
	{
		HideRecordingWidget();
	}
	else
	{
		ShowRecordingWidget();
	}
}

void ARecordingHUD::ShowRecordingWidget()
{
	if (!IsValid(RecordingWidgetInstance))
	{
		SpawnRecordingWidget();
		return;
	}

	RecordingWidgetInstance->SetVisibility(ESlateVisibility::Visible);

	if (bEnableInputWhileVisible)
	{
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (IsValid(PC))
		{
			PC->bShowMouseCursor = true;
			PC->SetInputMode(FInputModeGameAndUI());
		}
	}

	UE_LOG(LogUnrealCV, Log, TEXT("ARecordingHUD::ShowRecordingWidget: Widget visible"));
}

void ARecordingHUD::HideRecordingWidget()
{
	if (!IsValid(RecordingWidgetInstance))
	{
		return;
	}

	RecordingWidgetInstance->SetVisibility(ESlateVisibility::Hidden);

	if (bEnableInputWhileVisible)
	{
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (IsValid(PC))
		{
			PC->bShowMouseCursor = false;
			PC->SetInputMode(FInputModeGameOnly());
		}
	}

	UE_LOG(LogUnrealCV, Log, TEXT("ARecordingHUD::HideRecordingWidget: Widget hidden"));
}

bool ARecordingHUD::IsRecordingWidgetVisible() const
{
	if (!IsValid(RecordingWidgetInstance))
	{
		return false;
	}

	return RecordingWidgetInstance->GetVisibility() == ESlateVisibility::Visible;
}
