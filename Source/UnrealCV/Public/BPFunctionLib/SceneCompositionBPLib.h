// Copyright 2025 UnrealCV Team. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "FusionCamSensor.h"
#include "Runtime/Engine/Classes/Kismet/BlueprintFunctionLibrary.h"
#include "UnrealcvLog.h"
#include "SceneCompositionBPLib.generated.h"

/**
 * Metadata wrapper for occluder actors
 */
USTRUCT(BlueprintType)
struct FOccluderMetadata
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|Recording")
	TMap<FString, FString> Metadata;

	FOccluderMetadata() {}
	FOccluderMetadata(TMap<FString, FString> InMetadata) : Metadata(InMetadata) {}
};

/**
 * Scene generation parameters encapsulating all configuration for GenerateRandomScene.
 * Supports JSON-based construction with intelligent null/empty parameter handling.
 */
USTRUCT(BlueprintType)
struct FSceneGenerationParams
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|SceneComposition")
	FVector2D SpawnAreaMin;

	UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|SceneComposition")
	FVector2D SpawnAreaMax;

	UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|SceneComposition")
	float GroundHeight;

	UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|SceneComposition")
	FString ForegroundPathSpec;

	UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|SceneComposition")
	FString ForegroundCategory;

	UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|SceneComposition")
	FString OccluderPathSpec;

	UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|SceneComposition")
	FString OccluderCategory;

	UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|SceneComposition")
	int32 OccluderCount;

	UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|SceneComposition")
	int32 CameraID;

	UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|SceneComposition")
	bool bAutoPositionCamera;

	UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|SceneComposition")
	float ForegroundYaw;

	UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|SceneComposition")
	TArray<FVector> SafePoints;

	UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|SceneComposition")
	float AutoPositionCameraHeight;

	UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|SceneComposition")
	float AutoPositionCameraDistance;

	UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|SceneComposition")
	float AutoPositionCameraAngleOffset;

	FSceneGenerationParams()
		: SpawnAreaMin(0.0f, 0.0f)
		, SpawnAreaMax(1000.0f, 1000.0f)
		, GroundHeight(100.0f)
		, ForegroundPathSpec(TEXT(""))
		, ForegroundCategory(TEXT("Foreground_Human"))
		, OccluderPathSpec(TEXT(""))
		, OccluderCategory(TEXT("Occluder_All"))
		, OccluderCount(3)
		, CameraID(1)
		, bAutoPositionCamera(true)
		, ForegroundYaw(-1.0f)
		, SafePoints({})
		, AutoPositionCameraHeight(167.5f)
		, AutoPositionCameraDistance(325.0f)
		, AutoPositionCameraAngleOffset(0.0f)
	{
	}

	void DebugPrint() const
	{
		UE_LOG(LogUnrealCV, Log, TEXT("SceneGenerationParams: SpawnAreaMin: %s"), *SpawnAreaMin.ToString());
		UE_LOG(LogUnrealCV, Log, TEXT("SceneGenerationParams: SpawnAreaMax: %s"), *SpawnAreaMax.ToString());
		UE_LOG(LogUnrealCV, Log, TEXT("SceneGenerationParams: GroundHeight: %f"), GroundHeight);
		UE_LOG(LogUnrealCV, Log, TEXT("SceneGenerationParams: ForegroundPathSpec: %s"), *ForegroundPathSpec);
		UE_LOG(LogUnrealCV, Log, TEXT("SceneGenerationParams: ForegroundCategory: %s"), *ForegroundCategory);
		UE_LOG(LogUnrealCV, Log, TEXT("SceneGenerationParams: OccluderPathSpec: %s"), *OccluderPathSpec);
		UE_LOG(LogUnrealCV, Log, TEXT("SceneGenerationParams: OccluderCategory: %s"), *OccluderCategory);
		UE_LOG(LogUnrealCV, Log, TEXT("SceneGenerationParams: OccluderCount: %d"), OccluderCount);
		UE_LOG(LogUnrealCV, Log, TEXT("SceneGenerationParams: CameraID: %d"), CameraID);
		UE_LOG(LogUnrealCV, Log, TEXT("SceneGenerationParams: bAutoPositionCamera: %s"), bAutoPositionCamera ? TEXT("true") : TEXT("false"));
		UE_LOG(LogUnrealCV, Log, TEXT("SceneGenerationParams: ForegroundYaw: %f"), ForegroundYaw);
		UE_LOG(LogUnrealCV, Log, TEXT("SceneGenerationParams: SafePoints count: %d"), SafePoints.Num());
		UE_LOG(LogUnrealCV, Log, TEXT("SceneGenerationParams: AutoPositionCameraHeight: %f"), AutoPositionCameraHeight);
		UE_LOG(LogUnrealCV, Log, TEXT("SceneGenerationParams: AutoPositionCameraDistance: %f"), AutoPositionCameraDistance);
		UE_LOG(LogUnrealCV, Log, TEXT("SceneGenerationParams: AutoPositionCameraAngleOffset: %f"), AutoPositionCameraAngleOffset);
	}
};


/**
 * Scene Handle - Reference to a generated scene for later manipulation/cleanup.
 */
USTRUCT(BlueprintType)
struct FSceneHandle
{
	GENERATED_BODY()

	/** Unique identifier for this scene (e.g., "scene_0001") */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealCV|SceneComposition")
	FString SceneID;

	/** Foreground actor spawned in this scene */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealCV|SceneComposition")
	AActor* ForegroundActor;

	/** Occluder actors spawned in this scene */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealCV|SceneComposition")
	TArray<AActor*> OccluderActors;

	/** Directional light spawned for this scene */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealCV|SceneComposition")
	AActor* DirectionalLight;

	/** Camera ID used for this scene */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealCV|SceneComposition")
	int32 CameraID;

	/** Foreground category used (e.g., "Foreground_Human") */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealCV|SceneComposition")
	FString ForegroundCategory;

	/** Navigation controller for Blueprint actors */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealCV|SceneComposition")
	class ANavAgentController* NavController;

	/** Whether this scene has navigation enabled */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealCV|SceneComposition")
	bool bHasNavigation;

	////////////////////////////////////////////////////////////////////////////////////////////////
	// Scene Recording Metadata
	////////////////////////////////////////////////////////////////////////////////////////////////
	UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|Recording")
	FString SceneCategory;

	UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|Recording")
	FString ForegroundSubcategory;

	UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|Recording")
	FString OccluderCategory;

	UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|Recording")
	TArray<FOccluderMetadata> OccluderMetadataList;

	UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|Recording")
	TMap<FString, FString> ForegroundObjectMetadata;

	// UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|Recording")
	// FColor AnnotationColor;

	// UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|Recording")
	// TMap<FString, FColor> AllAnnotationColors;

	FSceneHandle()
		: SceneID(TEXT(""))
		, ForegroundActor(nullptr)
		, DirectionalLight(nullptr)
		, CameraID(0)
		, ForegroundCategory(TEXT(""))
		, NavController(nullptr)
		, bHasNavigation(false)

		, SceneCategory(TEXT(""))
		, ForegroundSubcategory(TEXT(""))
		, OccluderCategory(TEXT(""))
		, OccluderMetadataList({})
		, ForegroundObjectMetadata({})
		// , AnnotationColor(FColor::White)
		// , AllAnnotationColors({})
	{
	}

	void DebugPrint() const
	{
		UE_LOG(LogUnrealCV, Log, TEXT("SceneHandle: SceneID: %s"), *SceneID);
		UE_LOG(LogUnrealCV, Log, TEXT("SceneHandle: ForegroundActor: %p"), ForegroundActor);
		UE_LOG(LogUnrealCV, Log, TEXT("SceneHandle: DirectionalLight: %p"), DirectionalLight);
		UE_LOG(LogUnrealCV, Log, TEXT("SceneHandle: CameraID: %d"), CameraID);
		UE_LOG(LogUnrealCV, Log, TEXT("SceneHandle: ForegroundCategory: %s"), *ForegroundCategory);
		UE_LOG(LogUnrealCV, Log, TEXT("SceneHandle: NavController: %p"), NavController);
		UE_LOG(LogUnrealCV, Log, TEXT("SceneHandle: bHasNavigation: %s"), bHasNavigation ? TEXT("true") : TEXT("false"));
		UE_LOG(LogUnrealCV, Log, TEXT("SceneHandle: SceneCategory: %s"), *SceneCategory);
		UE_LOG(LogUnrealCV, Log, TEXT("SceneHandle: ForegroundSubcategory: %s"), *ForegroundSubcategory);
		UE_LOG(LogUnrealCV, Log, TEXT("SceneHandle: OccluderCategory: %s"), *OccluderCategory);
	}
};

/**
 * Scene Composition Blueprint Function Library
 *
 * Provides functions for automated scene generation with randomized foreground,
 * occluders, and camera placement. Used for SOW dataset production.
 */
UCLASS()
class UNREALCV_API USceneCompositionBPLib : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ========== Core Scene Generation ==========

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|SceneComposition", meta = (WorldContext = "WorldContextObject"))
	static bool GenerateRandomScene(
		UObject* WorldContextObject,
		const FSceneGenerationParams& Params,
		FSceneHandle& OutSceneHandle
	);

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|SceneComposition", meta = (WorldContext = "WorldContextObject"))
	static bool CreateSceneParamsFromJson(
		UObject* WorldContextObject,
		const FString& JsonFilePath,
		FSceneGenerationParams& OutParams
	);

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|SceneComposition", meta = (WorldContext = "WorldContextObject"))
	static void LoadStableAssetsPack(UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|SceneComposition")
	static void ClearScene(const FSceneHandle& SceneHandle);

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|SceneComposition", meta = (WorldContext = "WorldContextObject"))
	static void ClearAllScenes(UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|SceneComposition")
	static bool AddSafePointToScene(const FString& SceneName, FVector Location);

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|SceneComposition", meta = (WorldContext = "WorldContextObject"))
	static bool AddSafePointToCurrentScene(UObject* WorldContextObject, FVector Location);

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|SceneComposition")
	static TArray<FVector> GetSafePointsForScene(const FString& SceneName);

	// ========== Actor Spawning ==========

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|SceneComposition", meta = (WorldContext = "WorldContextObject"))
	static TArray<AActor*> SpawnRandomOccluders(
		UObject* WorldContextObject,
		int32 Count,
		FVector CameraPosition,
		FVector ForegroundPosition,
		const FString& OccluderPathSpec,
		const FString& OccluderCategory,
		FSceneHandle& OutSceneHandle
	);

	// ========== Asset Pool Management ==========

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|SceneComposition")
	static void RegisterAsset(const FString& Category, const FString& AssetPath);

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|SceneComposition")
	static void RegisterAssetWithMetadata(const FString& Category, const TMap<FString, FString>& Metadata);

	UFUNCTION(BlueprintPure, Category = "UnrealCV|SceneComposition")
	static TArray<FString> GetForegroundCategories();

	UFUNCTION(BlueprintPure, Category = "UnrealCV|SceneComposition")
	static TArray<FString> GetOccluderCategories();

	UFUNCTION(BlueprintPure, Category = "UnrealCV|SceneComposition")
	static int32 GetAssetCount(const FString& Category);

	UFUNCTION(BlueprintPure, Category = "UnrealCV|SceneComposition")
	static bool HasCategory(const FString& Category);

	// // ========== Lighting ==========

	// UFUNCTION(BlueprintCallable, Category = "UnrealCV|SceneComposition", meta = (WorldContext = "WorldContextObject"))
	// static AActor* CreateDirectionalLight(
	// 	UObject* WorldContextObject,
	// 	float Intensity = 5.0f,
	// 	FLinearColor Color = FLinearColor::White
	// );

	// ========== Camera Positioning ==========

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|SceneComposition")
	static bool PositionCameraToViewTarget(
		int32 CameraID,
		AActor* TargetActor,
		float MinDistance = 200.0f,
		float MaxDistance = 700.0f,
		float MinAngle = -20.0f,
		float MaxAngle = 20.0f
	);

private:
	static FString GenerateSceneID();
	// static AActor* LoadAndSpawnActor(UWorld* World, const FString& AssetPath, const FVector& Location, const FRotator& Rotation);
	static AActor* SpawnActorFromMetadata(UWorld* World, const TMap<FString, FString>& Metadata, const FVector& Location, const FRotator& Rotation);
	static float GetBoundsRadiusFromMetadata(const TMap<FString, FString>& Metadata);
	static class ANavAgentController* CreateNavAgentController(UObject* WorldContextObject, AActor* ControlledAgent);
	static void AdjustActorToGroundLevel(AActor* Actor);

	static float GetLandHeight(UWorld* World, float X, float Y, float InitialHeight);
	static void EnablePhysicsSettling(AActor* Actor, float InitialHeight = 5000.0f);
	static void SettleActorToGround(AActor* Actor, UWorld* World, float InitialHeight = 5000.0f, float HeightOffset = 0.0f);
	static void EnableCollisionOnly(AActor* Actor);

	static bool CheckCollisionAtLocation(UWorld* World, const FVector& Location, float Radius, const TArray<AActor*>& IgnoreActors);
	static bool FindCollisionFreeLocation(UWorld* World, FVector& OutLocation, float Radius, const TArray<AActor*>& IgnoreActors, int32 MaxAttempts = 10, float SearchRadius = 300.0f);

	static class AUnrealcvPawn* GetUnrealcvPawn(UWorld* World);

	static TArray<FSceneHandle> ActiveScenes;
};
