// Copyright 2025 UnrealCV Team. All Rights Reserved.
#include "SceneCompositionBPLib.h"
#include "AssetPoolManager.h"
#include "SensorBPLib.h"
#include "FusionCamSensor.h"
#include "UnrealcvLog.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"

// ========== Scene Generation ==========

FString USceneCompositionBPLib::GenerateSceneID()
{
	static int32 SceneCounter = 0;
	SceneCounter++;
	return FString::Printf(TEXT("scene_%04d"), SceneCounter);
}

bool USceneCompositionBPLib::GenerateRandomScene(
	UObject* WorldContextObject,
	FVector2D SpawnAreaMin,
	FVector2D SpawnAreaMax,
	const FString& ForegroundCategory,
	const FString& OccluderCategory,
	int32 OccluderCount,
	int32 CameraID,
	FSceneHandle& OutSceneHandle)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("USceneCompositionBPLib::GenerateRandomScene: Invalid world context"));
		return false;
	}

	// Validate asset pool
	FAssetPoolManager& AssetPool = FAssetPoolManager::Get();
	if (!AssetPool.HasCategory(ForegroundCategory))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("USceneCompositionBPLib::GenerateRandomScene: Invalid foreground category '%s'"), *ForegroundCategory);
		return false;
	}
	if (!AssetPool.HasCategory(OccluderCategory))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("USceneCompositionBPLib::GenerateRandomScene: Invalid occluder category '%s'"), *OccluderCategory);
		return false;
	}

	// Initialize scene handle
	OutSceneHandle = FSceneHandle();
	OutSceneHandle.SceneID = GenerateSceneID();
	OutSceneHandle.CameraID = CameraID;
	OutSceneHandle.ForegroundCategory = ForegroundCategory;

	// 1. Spawn foreground actor at random position in XY area
	FVector ForegroundPosition;
	ForegroundPosition.X = FMath::RandRange(SpawnAreaMin.X, SpawnAreaMax.X);
	ForegroundPosition.Y = FMath::RandRange(SpawnAreaMin.Y, SpawnAreaMax.Y);
	ForegroundPosition.Z = 0.0f; // Ground level (adjust if needed)

	OutSceneHandle.ForegroundActor = SpawnRandomForeground(WorldContextObject, ForegroundPosition, ForegroundCategory);
	if (!IsValid(OutSceneHandle.ForegroundActor))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("USceneCompositionBPLib::GenerateRandomScene: Failed to spawn foreground actor"));
		return false;
	}

	UE_LOG(LogUnrealCV, Log, TEXT("USceneCompositionBPLib::GenerateRandomScene: Spawned foreground at (%.1f, %.1f, %.1f)"),
		ForegroundPosition.X, ForegroundPosition.Y, ForegroundPosition.Z);

	// 2. Position camera to view foreground
	if (!PositionCameraToViewTarget(CameraID, OutSceneHandle.ForegroundActor))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("USceneCompositionBPLib::GenerateRandomScene: Failed to position camera"));
		ClearScene(OutSceneHandle);
		return false;
	}

	// Get camera position for occluder placement
	UFusionCamSensor* Camera = USensorBPLib::GetSensorById(CameraID);
	if (!IsValid(Camera))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("USceneCompositionBPLib::GenerateRandomScene: Invalid camera ID %d"), CameraID);
		ClearScene(OutSceneHandle);
		return false;
	}
	FVector CameraPosition = Camera->GetComponentLocation();

	// 3. Spawn occluders between camera and foreground
	OutSceneHandle.OccluderActors = SpawnRandomOccluders(
		WorldContextObject,
		OccluderCount,
		CameraPosition,
		ForegroundPosition,
		OccluderCategory
	);

	UE_LOG(LogUnrealCV, Log, TEXT("USceneCompositionBPLib::GenerateRandomScene: Spawned %d occluders"), OutSceneHandle.OccluderActors.Num());

	// // 4. Calculate occlusion ratio
	OutSceneHandle.OcclusionRatio = 0.0f;
	// OutSceneHandle.OcclusionRatio = CalculateOcclusionRatio(
	// 	CameraID,
	// 	OutSceneHandle.ForegroundActor,
	// 	OutSceneHandle.OccluderActors
	// );

	UE_LOG(LogUnrealCV, Log, TEXT("USceneCompositionBPLib::GenerateRandomScene: Scene '%s' generated successfully (Occlusion: %.2f%%)"),
		*OutSceneHandle.SceneID, OutSceneHandle.OcclusionRatio * 100.0f);

	return true;
}

void USceneCompositionBPLib::ClearScene(const FSceneHandle& SceneHandle)
{
	// Destroy foreground actor
	if (IsValid(SceneHandle.ForegroundActor))
	{
		SceneHandle.ForegroundActor->Destroy();
	}

	// Destroy all occluder actors
	for (AActor* Occluder : SceneHandle.OccluderActors)
	{
		if (IsValid(Occluder))
		{
			Occluder->Destroy();
		}
	}

	UE_LOG(LogUnrealCV, Log, TEXT("USceneCompositionBPLib::ClearScene: Cleared scene '%s'"), *SceneHandle.SceneID);
}

// // ========== Occlusion Calculation ==========

// float USceneCompositionBPLib::CalculateOcclusionRatio(
// 	int32 CameraID,
// 	AActor* ForegroundActor,
// 	const TArray<AActor*>& OccluderActors)
// {
// 	if (!IsValid(ForegroundActor))
// 	{
// 		UE_LOG(LogUnrealCV, Warning, TEXT("USceneCompositionBPLib::CalculateOcclusionRatio: Invalid foreground actor"));
// 		return 0.0f;
// 	}

// 	UFusionCamSensor* Camera = USensorBPLib::GetSensorById(CameraID);
// 	if (!IsValid(Camera))
// 	{
// 		UE_LOG(LogUnrealCV, Warning, TEXT("USceneCompositionBPLib::CalculateOcclusionRatio: Invalid camera ID %d"), CameraID);
// 		return 0.0f;
// 	}

// 	// Step 1: Hide all occluders, render foreground only
// 	for (AActor* Occluder : OccluderActors)
// 	{
// 		if (IsValid(Occluder))
// 		{
// 			Occluder->SetActorHiddenInGame(true);
// 		}
// 	}

// 	// Capture object mask (foreground only) using existing GetSeg API
// 	TArray<FColor> PixelData_ForegroundOnly;
// 	int32 Width = 0, Height = 0;
// 	Camera->GetSeg(PixelData_ForegroundOnly, Width, Height);
// 	int32 VisiblePixels_ForegroundOnly = CountVisiblePixels(PixelData_ForegroundOnly);

// 	// Step 2: Show all occluders, render foreground + occluders
// 	for (AActor* Occluder : OccluderActors)
// 	{
// 		if (IsValid(Occluder))
// 		{
// 			Occluder->SetActorHiddenInGame(false);
// 		}
// 	}

// 	// Capture object mask (foreground + occluders)
// 	TArray<FColor> PixelData_WithOccluders;
// 	Camera->GetSeg(PixelData_WithOccluders, Width, Height);
// 	int32 VisiblePixels_WithOccluders = CountVisiblePixels(PixelData_WithOccluders);

// 	// Step 3: Calculate occlusion ratio
// 	if (VisiblePixels_ForegroundOnly == 0)
// 	{
// 		UE_LOG(LogUnrealCV, Warning, TEXT("USceneCompositionBPLib::CalculateOcclusionRatio: Foreground not visible in camera"));
// 		return 0.0f;
// 	}

// 	int32 OccludedPixels = VisiblePixels_ForegroundOnly - VisiblePixels_WithOccluders;
// 	float OcclusionRatio = (float)OccludedPixels / (float)VisiblePixels_ForegroundOnly;

// 	return FMath::Clamp(OcclusionRatio, 0.0f, 1.0f);
// }

// int32 USceneCompositionBPLib::CountVisiblePixels(const TArray<FColor>& PixelData)
// {
// 	int32 Count = 0;
// 	for (const FColor& Pixel : PixelData	)
// 	{
// 		// Count non-black pixels (assuming black = background)
// 		if (Pixel.R > 0 || Pixel.G > 0 || Pixel.B > 0)
// 		{
// 			Count++;
// 		}
// 	}
// 	return Count;
// }

// ========== Actor Spawning ==========

AActor* USceneCompositionBPLib::LoadAndSpawnActor(UWorld* World, const FString& AssetPath, const FVector& Location, const FRotator& Rotation)
{
	if (!IsValid(World))
	{
		return nullptr;
	}

	// Load static mesh from asset path
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *AssetPath);
	if (!Mesh)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("USceneCompositionBPLib::LoadAndSpawnActor: Failed to load asset '%s'"), *AssetPath);
		return nullptr;
	}

	// Spawn static mesh actor
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Location, Rotation, SpawnParams);
	if (!IsValid(Actor))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("USceneCompositionBPLib::LoadAndSpawnActor: Failed to spawn actor"));
		return nullptr;
	}

	// Set static mesh
	UStaticMeshComponent* MeshComponent = Actor->GetStaticMeshComponent();
	if (IsValid(MeshComponent))
	{
		MeshComponent->SetStaticMesh(Mesh);
	}

	return Actor;
}

AActor* USceneCompositionBPLib::SpawnRandomForeground(
	UObject* WorldContextObject,
	FVector Position,
	const FString& ForegroundCategory)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("USceneCompositionBPLib::SpawnRandomForeground: Invalid world context"));
		return nullptr;
	}

	// Get random asset from pool
	FAssetPoolManager& AssetPool = FAssetPoolManager::Get();
	FString AssetPath = AssetPool.GetRandomAsset(ForegroundCategory);
	if (AssetPath.IsEmpty())
	{
		UE_LOG(LogUnrealCV, Error, TEXT("USceneCompositionBPLib::SpawnRandomForeground: Failed to get asset from category '%s'"), *ForegroundCategory);
		return nullptr;
	}

	// Spawn actor
	FRotator Rotation = FRotator::ZeroRotator;
	Rotation.Yaw = FMath::RandRange(0.0f, 360.0f); // Random rotation around Z axis

	AActor* Actor = LoadAndSpawnActor(World, AssetPath, Position, Rotation);
	if (IsValid(Actor))
	{
		UE_LOG(LogUnrealCV, Log, TEXT("USceneCompositionBPLib::SpawnRandomForeground: Spawned '%s' at (%.1f, %.1f, %.1f)"),
			*AssetPath, Position.X, Position.Y, Position.Z);
	}

	return Actor;
}

TArray<AActor*> USceneCompositionBPLib::SpawnRandomOccluders(
	UObject* WorldContextObject,
	int32 Count,
	FVector CameraPosition,
	FVector ForegroundPosition,
	const FString& OccluderCategory)
{
	TArray<AActor*> SpawnedOccluders;

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("USceneCompositionBPLib::SpawnRandomOccluders: Invalid world context"));
		return SpawnedOccluders;
	}

	FAssetPoolManager& AssetPool = FAssetPoolManager::Get();

	for (int32 i = 0; i < Count; i++)
	{
		// Get random asset
		FString AssetPath = AssetPool.GetRandomAsset(OccluderCategory);
		if (AssetPath.IsEmpty())
		{
			UE_LOG(LogUnrealCV, Warning, TEXT("USceneCompositionBPLib::SpawnRandomOccluders: Failed to get asset from category '%s'"), *OccluderCategory);
			continue;
		}

		// Calculate random position between camera and foreground
		// Interpolate along camera-to-foreground line with some lateral randomness
		float Alpha = FMath::RandRange(0.3f, 0.7f); // Position along line (30%-70%)
		FVector BasePosition = FMath::Lerp(CameraPosition, ForegroundPosition, Alpha);

		// Add lateral randomness (perpendicular to camera-foreground line)
		FVector Direction = (ForegroundPosition - CameraPosition).GetSafeNormal();
		FVector Perpendicular = FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal();
		float LateralOffset = FMath::RandRange(-200.0f, 200.0f); // ±2 meters
		BasePosition += Perpendicular * LateralOffset;

		// Add height randomness
		BasePosition.Z += FMath::RandRange(-50.0f, 100.0f);

		// Random rotation
		FRotator Rotation = FRotator::ZeroRotator;
		Rotation.Yaw = FMath::RandRange(0.0f, 360.0f);

		// Spawn occluder
		AActor* Occluder = LoadAndSpawnActor(World, AssetPath, BasePosition, Rotation);
		if (IsValid(Occluder))
		{
			SpawnedOccluders.Add(Occluder);
			UE_LOG(LogUnrealCV, Log, TEXT("USceneCompositionBPLib::SpawnRandomOccluders: Spawned occluder %d at (%.1f, %.1f, %.1f)"),
				i + 1, BasePosition.X, BasePosition.Y, BasePosition.Z);
		}
	}

	return SpawnedOccluders;
}

// ========== Asset Pool Queries ==========

void USceneCompositionBPLib::RegisterAsset(const FString& Category, const FString& AssetPath)
{
	FAssetPoolManager::Get().RegisterAsset(Category, AssetPath);
}

TArray<FString> USceneCompositionBPLib::GetForegroundCategories()
{
	FAssetPoolManager& AssetPool = FAssetPoolManager::Get();
	TArray<FString> AllCategories = AssetPool.GetAllCategories();

	TArray<FString> ForegroundCategories;
	for (const FString& Category : AllCategories)
	{
		if (Category.StartsWith(TEXT("Foreground_")))
		{
			ForegroundCategories.Add(Category);
		}
	}

	return ForegroundCategories;
}

TArray<FString> USceneCompositionBPLib::GetOccluderCategories()
{
	FAssetPoolManager& AssetPool = FAssetPoolManager::Get();
	TArray<FString> AllCategories = AssetPool.GetAllCategories();

	TArray<FString> OccluderCategories;
	for (const FString& Category : AllCategories)
	{
		if (Category.StartsWith(TEXT("Occluder_")))
		{
			OccluderCategories.Add(Category);
		}
	}

	return OccluderCategories;
}

int32 USceneCompositionBPLib::GetAssetCount(const FString& Category)
{
	FAssetPoolManager& AssetPool = FAssetPoolManager::Get();
	return AssetPool.GetAssetCount(Category);
}

bool USceneCompositionBPLib::HasCategory(const FString& Category)
{
	FAssetPoolManager& AssetPool = FAssetPoolManager::Get();
	return AssetPool.HasCategory(Category);
}

// ========== Camera Positioning ==========

bool USceneCompositionBPLib::PositionCameraToViewTarget(
	int32 CameraID,
	AActor* TargetActor,
	float MinDistance,
	float MaxDistance,
	float MinAngle,
	float MaxAngle)
{
	if (!IsValid(TargetActor))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("USceneCompositionBPLib::PositionCameraToViewTarget: Invalid target actor"));
		return false;
	}

	UFusionCamSensor* Camera = USensorBPLib::GetSensorById(CameraID);
	if (!IsValid(Camera))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("USceneCompositionBPLib::PositionCameraToViewTarget: Invalid camera ID %d"), CameraID);
		return false;
	}

	// Get target position
	FVector TargetPosition = TargetActor->GetActorLocation();

	// Random distance and angles
	float Distance = FMath::RandRange(MinDistance, MaxDistance);
	float HorizontalAngle = FMath::RandRange(0.0f, 360.0f); // Azimuth
	float VerticalAngle = FMath::RandRange(MinAngle, MaxAngle); // Elevation

	// Calculate camera position using spherical coordinates
	float HorizontalAngleRad = FMath::DegreesToRadians(HorizontalAngle);
	float VerticalAngleRad = FMath::DegreesToRadians(VerticalAngle);

	FVector Offset;
	Offset.X = Distance * FMath::Cos(VerticalAngleRad) * FMath::Cos(HorizontalAngleRad);
	Offset.Y = Distance * FMath::Cos(VerticalAngleRad) * FMath::Sin(HorizontalAngleRad);
	Offset.Z = Distance * FMath::Sin(VerticalAngleRad);

	FVector CameraPosition = TargetPosition + Offset;

	// Calculate rotation to look at target
	FRotator CameraRotation = (TargetPosition - CameraPosition).Rotation();

	// Set camera transform
	Camera->SetWorldLocation(CameraPosition);
	Camera->SetWorldRotation(CameraRotation);

	UE_LOG(LogUnrealCV, Log, TEXT("USceneCompositionBPLib::PositionCameraToViewTarget: Camera %d positioned at (%.1f, %.1f, %.1f), looking at (%.1f, %.1f, %.1f)"),
		CameraID, CameraPosition.X, CameraPosition.Y, CameraPosition.Z, TargetPosition.X, TargetPosition.Y, TargetPosition.Z);

	return true;
}
