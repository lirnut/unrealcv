// Copyright 2025 UnrealCV Team. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Runtime/Engine/Classes/Kismet/BlueprintFunctionLibrary.h"
#include "FusionCamCaptureActor.h"
#include "SceneCompositionBPLib.h"
#include "DatasetAutomationBPLib.generated.h"

class FGenericTickableObject;

UENUM(BlueprintType)
enum class EDatasetGenerationState : uint8
{
	Idle,
	ExecutingCommand,
	WaitingAsync,
	Completed,
	Error
};

USTRUCT()
struct FAutomationStep
{
	GENERATED_BODY()

	FString Command;
	FString StringParam;

	FAutomationStep()
		: Command(TEXT("")), StringParam(TEXT(""))
	{}

	FAutomationStep(const FString& InCommand, const FString& InStringParam = TEXT(""))
		: Command(InCommand), StringParam(InStringParam)
	{}
};

USTRUCT(BlueprintType)
struct FAutomationConfig
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Automation")
	int32 TotalScenes = 100;

	UPROPERTY(BlueprintReadWrite, Category = "Automation")
	FSceneGenerationParams SceneParams;

	UPROPERTY(BlueprintReadWrite, Category = "Automation")
	bool bLoadSceneParamsFromJson = true;

	UPROPERTY(BlueprintReadWrite, Category = "Automation")
	FString OutputDirectory = TEXT("C:/Dataset");

	UPROPERTY(BlueprintReadWrite, Category = "Automation")
	int32 TrajectoryFPS = 30;

	UPROPERTY(BlueprintReadWrite, Category = "Automation")
	float TrajectoryDegreesPerSecond = 36.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Automation")
	int32 NumFrames = 121;
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

	UPROPERTY(BlueprintReadOnly, Category = "Automation")
	FIntPoint ChosenRes = FIntPoint(0, 0);

	UPROPERTY(BlueprintReadOnly, Category = "Automation")
	float ChosenFOV = 90.0f;

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
	static bool SetMap(UObject* WorldContextObject, const FString& MapName);

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Automation")
	static bool SetTaskName(const FString& InTaskName);

	UFUNCTION(BlueprintPure, Category = "UnrealCV|Automation")
	static FString GetTaskName();

	static bool SetExternalCommandSequence(const TArray<FAutomationStep>& Sequence);

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Automation")
	static bool ParseCommandSequenceJson(const FString& JsonContent, FString& OutErrorMessage);

	UFUNCTION(BlueprintPure, Category = "UnrealCV|Automation")
	static FString GetCommandQueueSummary();

public:
	static FAutomationConfig CurrentConfig;
	static FAutomationStatus CurrentStatus;
	static FSceneHandle CurrentScene;

private:
	static UWorld* WorldContext;

	// static FString PrimaryCameraID;
	static TArray<FString> ActiveCameraPool;
	// static TMap<FString, bool> CameraRecordingState;

	static TArray<FAutomationStep> CommandQueue;
	static int32 CurrentCommandIndex;
	static int32 CurrentSceneCounter;
	static FString CurrentSceneID;
	static FString TaskName;
	static double DelayStartTime;
	static double DelayDuration;

	static TArray<FAutomationStep> ExternalCommandQueue;

	static FGenericTickableObject* TickableObject;

	static void OnTick(double RealDeltaTime);
	static void TransitionToState(EDatasetGenerationState NewState);
	static void ProcessState(double RealDeltaTime);

	static void BuildCommandSequenceForScene();
	static void ExecuteNextCommand();
	static void ExecuteCommand(const FAutomationStep& Step);
	static FString GenerateSceneID(int32 SceneIndex);
	static FString GenerateOutputPath(const FString& SceneID, const FString& TrajectoryType);
	static class AFusionCameraActor* GetFusionCameraActor(int32 CameraID);

	static FString GetIdleCamera();
	static bool AreAllCamerasIdle();
	static bool StartTrajectoryRecording(const FString& FileName, const FString& TrajectoryType);
	static bool ParseVector3D(const FString& Str, FVector& OutVector);
};

