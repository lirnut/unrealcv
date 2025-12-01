// Copyright 2025 UnrealCV Team. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Runtime/Engine/Classes/Kismet/BlueprintFunctionLibrary.h"
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

	/** Calculated occlusion ratio (0.0 - 1.0) */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealCV|SceneComposition")
	float OcclusionRatio;

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

	UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|Recording")
	FColor AnnotationColor;

	UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|Recording")
	TMap<FString, FColor> AllAnnotationColors;

	FSceneHandle()
		: SceneID(TEXT(""))
		, ForegroundActor(nullptr)
		, DirectionalLight(nullptr)
		, CameraID(0)
		, ForegroundCategory(TEXT(""))
		, OcclusionRatio(0.0f)
		, NavController(nullptr)
		, bHasNavigation(false)

		, SceneCategory(TEXT(""))
		, ForegroundSubcategory(TEXT(""))
		, OccluderCategory(TEXT(""))
		, OccluderMetadataList({})
		, ForegroundObjectMetadata({})
		, AnnotationColor(FColor::White)
		, AllAnnotationColors({})
	{
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

	/**
	 * Generate a complete random scene with foreground, occluders, and camera.
	 * @param bAutoPositionCamera If true, automatically position camera at eye level facing foreground
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealCV|SceneComposition", meta = (WorldContext = "WorldContextObject"))
	static bool GenerateRandomScene(
		UObject* WorldContextObject,
		FVector2D SpawnAreaMin,
		FVector2D SpawnAreaMax,
		const FString& ForegroundCategory,
		const FString& OccluderCategory,
		int32 OccluderCount,
		int32 CameraID,
		FSceneHandle& OutSceneHandle,
		bool bAutoPositionCamera = true
	);

	/**
	 * Load stable assets pack for scene composition.
	 * @param WorldContextObject World context
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealCV|SceneComposition", meta = (WorldContext = "WorldContextObject"))
	static void LoadStableAssetsPack(UObject* WorldContextObject);

	// /**
	//  * Calculate occlusion ratio for a specific foreground actor from camera view.
	//  *
	//  * Method: Render object mask twice (with/without occluders), compare pixel counts.
	//  * Occlusion Ratio = (Pixels Hidden) / (Total Foreground Pixels)
	//  *
	//  * @param CameraID Camera to calculate from
	//  * @param ForegroundActor Actor to check occlusion for
	//  * @param OccluderActors Actors that may occlude the foreground
	//  * @return Occlusion ratio (0.0 = not occluded, 1.0 = fully occluded)
	//  */
	// UFUNCTION(BlueprintPure, Category = "UnrealCV|SceneComposition")
	// static float CalculateOcclusionRatio(
	// 	int32 CameraID,
	// 	AActor* ForegroundActor,
	// 	const TArray<AActor*>& OccluderActors
	// );

	/**
	 * Clear all actors in a generated scene.
	 * @param SceneHandle Handle returned by GenerateRandomScene
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealCV|SceneComposition")
	static void ClearScene(const FSceneHandle& SceneHandle);

	/**
	 * Clear all scenes tracked by the scene composition system.
	 * @param WorldContextObject World context
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealCV|SceneComposition", meta = (WorldContext = "WorldContextObject"))
	static void ClearAllScenes(UObject* WorldContextObject);

	// ========== Actor Spawning ==========

	/**
	 * Spawn random occluder actors between camera and foreground.
	 *
	 * Occluders are placed randomly in a box volume between camera and foreground,
	 * with some randomness in height and lateral position.
	 *
	 * @param WorldContextObject World context
	 * @param Count Number of occluders to spawn
	 * @param CameraPosition Camera world position
	 * @param ForegroundPosition Foreground actor world position
	 * @param OccluderCategory Category (e.g., "Occluder_Tree", "Occluder_Pillar")
	 * @return Array of spawned occluder actors
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealCV|SceneComposition", meta = (WorldContext = "WorldContextObject"))
	static TArray<AActor*> SpawnRandomOccluders(
		UObject* WorldContextObject,
		int32 Count,
		FVector CameraPosition,
		FVector ForegroundPosition,
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

	// /**
	//  * Create directional light for scene with random rotation.
	//  * @param WorldContextObject World context
	//  * @param Intensity Light intensity (default 5.0)
	//  * @param Color Light color (default white)
	//  * @return Spawned directional light actor
	//  */
	// UFUNCTION(BlueprintCallable, Category = "UnrealCV|SceneComposition", meta = (WorldContext = "WorldContextObject"))
	// static AActor* CreateDirectionalLight(
	// 	UObject* WorldContextObject,
	// 	float Intensity = 5.0f,
	// 	FLinearColor Color = FLinearColor::White
	// );

	// ========== Camera Positioning ==========

	/**
	 * Position camera to view a target actor.
	 *
	 * Camera is placed at a random distance and angle from the target,
	 * ensuring the target is visible in the frame.
	 *
	 * @param CameraID Camera to position
	 * @param TargetActor Actor to view
	 * @param MinDistance Minimum distance from target (cm)
	 * @param MaxDistance Maximum distance from target (cm)
	 * @param MinAngle Minimum vertical angle (degrees, 0 = horizontal)
	 * @param MaxAngle Maximum vertical angle (degrees)
	 * @return True if camera positioned successfully
	 */
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
	static AActor* LoadAndSpawnActor(UWorld* World, const FString& AssetPath, const FVector& Location, const FRotator& Rotation);
	static AActor* SpawnActorFromMetadata(UWorld* World, const TMap<FString, FString>& Metadata, const FVector& Location, const FRotator& Rotation);
	static float GetBoundsRadiusFromMetadata(const TMap<FString, FString>& Metadata);
	static class ANavAgentController* CreateNavAgentController(UObject* WorldContextObject, AActor* ControlledAgent);
	static void AdjustActorToGroundLevel(AActor* Actor);

	static TArray<FSceneHandle> ActiveScenes;
};
