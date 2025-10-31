// Copyright 2025 UnrealCV Team. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Runtime/Engine/Classes/Kismet/BlueprintFunctionLibrary.h"
#include "SceneCompositionBPLib.generated.h"

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

	/** Camera ID used for this scene */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealCV|SceneComposition")
	int32 CameraID;

	/** Foreground category used (e.g., "Foreground_Human") */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealCV|SceneComposition")
	FString ForegroundCategory;

	/** Calculated occlusion ratio (0.0 - 1.0) */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealCV|SceneComposition")
	float OcclusionRatio;

	FSceneHandle()
		: SceneID(TEXT(""))
		, ForegroundActor(nullptr)
		, CameraID(0)
		, ForegroundCategory(TEXT(""))
		, OcclusionRatio(0.0f)
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
		FSceneHandle& OutSceneHandle
	);

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

	// ========== Actor Spawning ==========

	/**
	 * Spawn a random foreground actor from asset pool.
	 * @param WorldContextObject World context
	 * @param Position World position to spawn at
	 * @param ForegroundCategory Category (e.g., "Foreground_Human", "Foreground_Pet_Dog")
	 * @return Spawned actor, or nullptr if failed
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealCV|SceneComposition", meta = (WorldContext = "WorldContextObject"))
	static AActor* SpawnRandomForeground(
		UObject* WorldContextObject,
		FVector Position,
		const FString& ForegroundCategory
	);

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
		const FString& OccluderCategory
	);

	// ========== Asset Pool Management ==========

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|SceneComposition")
	static void RegisterAsset(const FString& Category, const FString& AssetPath);

	UFUNCTION(BlueprintPure, Category = "UnrealCV|SceneComposition")
	static TArray<FString> GetForegroundCategories();

	UFUNCTION(BlueprintPure, Category = "UnrealCV|SceneComposition")
	static TArray<FString> GetOccluderCategories();

	UFUNCTION(BlueprintPure, Category = "UnrealCV|SceneComposition")
	static int32 GetAssetCount(const FString& Category);

	UFUNCTION(BlueprintPure, Category = "UnrealCV|SceneComposition")
	static bool HasCategory(const FString& Category);

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
	// Helper: Generate unique scene ID
	static FString GenerateSceneID();

	// Helper: Load actor from asset path
	static AActor* LoadAndSpawnActor(UWorld* World, const FString& AssetPath, const FVector& Location, const FRotator& Rotation);

	// // Helper: Count non-black pixels in image data
	// static int32 CountVisiblePixels(const TArray<FColor>& PixelData);
};
