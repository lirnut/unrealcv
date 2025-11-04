// Copyright 2025 UnrealCV Team. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Runtime/Engine/Classes/Kismet/BlueprintFunctionLibrary.h"
#include "SceneCompositionBPLib.h"
#include "DatasetAutomationBPLib.generated.h"

UENUM(BlueprintType)
enum class EDatasetGenerationState : uint8
{
	Idle,
	GeneratingScene,
	Recording,
	WaitingForRecordingComplete,
	CleaningUp,
	Completed,
	Error
};

USTRUCT(BlueprintType)
struct FAutomationConfig
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Automation")
	int32 TotalScenes = 100;

	UPROPERTY(BlueprintReadWrite, Category = "Automation")
	FVector2D SpawnAreaMin = FVector2D(-1000, -1000);

	UPROPERTY(BlueprintReadWrite, Category = "Automation")
	FVector2D SpawnAreaMax = FVector2D(1000, 1000);

	UPROPERTY(BlueprintReadWrite, Category = "Automation")
	FString ForegroundCategory = TEXT("Foreground_Human");

	UPROPERTY(BlueprintReadWrite, Category = "Automation")
	FString OccluderCategory = TEXT("Occluder_Tree");

	UPROPERTY(BlueprintReadWrite, Category = "Automation")
	int32 OccluderCount = 3;

	UPROPERTY(BlueprintReadWrite, Category = "Automation")
	int32 CameraID = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Automation")
	FString OutputDirectory = TEXT("C:/Dataset");

	UPROPERTY(BlueprintReadWrite, Category = "Automation")
	float RecordingDuration = 10.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Automation")
	int32 RecordingFPS = 30;

	UPROPERTY(BlueprintReadWrite, Category = "Automation")
	bool bUseBulletTime = false;

	UPROPERTY(BlueprintReadWrite, Category = "Automation")
	bool bUseTrajectory = false;

	UPROPERTY(BlueprintReadWrite, Category = "Automation")
	FString TrajectoryType = TEXT("rotate_360");

	UPROPERTY(BlueprintReadWrite, Category = "Automation")
	int32 TrajectoryFPS = 30;

	UPROPERTY(BlueprintReadWrite, Category = "Automation")
	float TrajectoryDegreesPerSecond = 36.0f;
};

USTRUCT(BlueprintType)
struct FAutomationStatus
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Automation")
	EDatasetGenerationState State = EDatasetGenerationState::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "Automation")
	int32 CurrentSceneIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Automation")
	int32 TotalScenes = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Automation")
	float Progress = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Automation")
	FString CurrentFileName = TEXT("");

	UPROPERTY(BlueprintReadOnly, Category = "Automation")
	FString ErrorMessage = TEXT("");
};

UCLASS()
class UNREALCV_API UDatasetAutomationBPLib : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Automation", meta = (WorldContext = "WorldContextObject"))
	static bool StartBatchGeneration(
		UObject* WorldContextObject,
		const FAutomationConfig& Config
	);

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Automation")
	static void StopBatchGeneration();

	UFUNCTION(BlueprintPure, Category = "UnrealCV|Automation")
	static FAutomationStatus GetAutomationStatus();

	UFUNCTION(BlueprintPure, Category = "UnrealCV|Automation")
	static FString GetAutomationStatusString();

	UFUNCTION(BlueprintPure, Category = "UnrealCV|Automation")
	static bool IsRunning();

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Automation", meta = (WorldContext = "WorldContextObject"))
	static void TickAutomation(UObject* WorldContextObject, float DeltaTime);

private:
	static FAutomationConfig CurrentConfig;
	static FAutomationStatus CurrentStatus;
	static FSceneHandle CurrentScene;
	static UWorld* WorldContext;
	static FTimerHandle AutomationTimerHandle;

	static void TransitionToState(EDatasetGenerationState NewState);
	static void ProcessState(float DeltaTime);
	static FString GenerateFileName(int32 Index);
	static void AutoTick();
};
