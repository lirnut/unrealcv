// Copyright 2025 UnrealCV Team. All Rights Reserved.
#include "SceneCompositionBPLib.h"
#include "AssetPoolManager.h"
#include "SensorBPLib.h"
#include "FusionCamSensor.h"
#include "UnrealcvLog.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "AnimatedSkeletalActor.h"
#include "UnrealcvServer.h"
#include "WorldController.h"
#include "ObjectAnnotator.h"
#include "NavAgentController.h"

TArray<FSceneHandle> USceneCompositionBPLib::ActiveScenes;

// ========== Scene Generation ==========

FString USceneCompositionBPLib::GenerateSceneID()
{
	static int32 SceneCounter = 0;
	SceneCounter++;
	return FString::Printf(TEXT("scene_%04d"), SceneCounter);
}

void USceneCompositionBPLib::LoadStableAssetsPack(UObject* WorldContextObject)
{
	FAssetPoolManager& AssetPool = FAssetPoolManager::Get();
	AssetPool.LoadStableAssetsPack();
}

bool USceneCompositionBPLib::GenerateRandomScene(
	UObject* WorldContextObject,
	FVector2D SpawnAreaMin,
	FVector2D SpawnAreaMax,
	const FString& ForegroundCategory,
	const FString& OccluderCategory,
	int32 OccluderCount,
	int32 CameraID,
	FSceneHandle& OutSceneHandle,
	bool bAutoPositionCamera
)
{
	float CameraHeight = FMath::RandRange(20, 140);
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("USceneCompositionBPLib::GenerateRandomScene: Invalid world context"));
		return false;
	}

	FAssetPoolManager& AssetPool = FAssetPoolManager::Get();

	if (!AssetPool.HasCategory(ForegroundCategory))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("GenerateRandomScene: Invalid foreground category '%s'"), *ForegroundCategory);
		return false;
	}
	if (!AssetPool.HasCategory(OccluderCategory))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("GenerateRandomScene: Invalid occluder category '%s'"), *OccluderCategory);
		return false;
	}

	// Initialize scene handle
	OutSceneHandle = FSceneHandle();
	OutSceneHandle.SceneID = GenerateSceneID();
	OutSceneHandle.CameraID = CameraID;
	OutSceneHandle.ForegroundCategory = ForegroundCategory;

	// 1. Spawn foreground actor - get metadata to check type
	FVector ForegroundPosition;
	ForegroundPosition.X = FMath::RandRange(SpawnAreaMin.X, SpawnAreaMax.X);
	ForegroundPosition.Y = FMath::RandRange(SpawnAreaMin.Y, SpawnAreaMax.Y);
	ForegroundPosition.Z = 0.0f;

	TMap<FString, FString> ForegroundMetadata = AssetPool.GetRandomAssetMetadata(ForegroundCategory);
	if (ForegroundMetadata.Num() == 0)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("GenerateRandomScene: No assets in foreground category '%s'"), *ForegroundCategory);
		return false;
	}

	FRotator ForegroundRotation = FRotator::ZeroRotator;
	ForegroundRotation.Yaw = FMath::RandRange(0.0f, 360.0f);

	OutSceneHandle.ForegroundActor = SpawnActorFromMetadata(World, ForegroundMetadata, ForegroundPosition, ForegroundRotation);
	if (!IsValid(OutSceneHandle.ForegroundActor))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("GenerateRandomScene: Failed to spawn foreground actor"));
		return false;
	}

	OutSceneHandle.ForegroundCategory = ForegroundCategory;
	OutSceneHandle.ForegroundObjectMetadata = ForegroundMetadata;
	OutSceneHandle.SceneCategory = World->GetMapName();
	

	// Check if foreground is Blueprint type and create NavAgent
	if (ForegroundMetadata.Contains(TEXT("Type")) && ForegroundMetadata[TEXT("Type")] == TEXT("Blueprint"))
	{
		OutSceneHandle.NavController = CreateNavAgentController(WorldContextObject, OutSceneHandle.ForegroundActor);
		if (IsValid(OutSceneHandle.NavController))
		{
			OutSceneHandle.bHasNavigation = true;
			UE_LOG(LogUnrealCV, Log, TEXT("GenerateRandomScene: Applied NavAgent to Blueprint actor '%s'"), *OutSceneHandle.ForegroundActor->GetName());
		}
	}

	UFusionCamSensor* Camera = USensorBPLib::GetSensorById(CameraID);
	if (!IsValid(Camera))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("GenerateRandomScene: Invalid camera ID %d"), CameraID);
		ClearScene(OutSceneHandle);
		return false;
	}

	if (bAutoPositionCamera)
	{
		float Distance = FMath::RandRange(300.0f, 600.0f);
		float HorizontalAngle = FMath::RandRange(0.0f, 360.0f);

		FVector CameraPosition;
		CameraPosition.X = ForegroundPosition.X + Distance * FMath::Cos(FMath::DegreesToRadians(HorizontalAngle));
		CameraPosition.Y = ForegroundPosition.Y + Distance * FMath::Sin(FMath::DegreesToRadians(HorizontalAngle));
		CameraPosition.Z = CameraHeight;

		FRotator CameraRotation = (ForegroundPosition - CameraPosition).Rotation();

		Camera->SetSensorLocation(CameraPosition);
		Camera->SetSensorRotation(CameraRotation);
	}

	FVector CameraPosition = Camera->GetSensorLocation();

	// 3. Spawn occluders between camera and foreground
	OutSceneHandle.OccluderActors = SpawnRandomOccluders(
		WorldContextObject,
		OccluderCount,
		CameraPosition,
		ForegroundPosition,
		OccluderCategory,
		OutSceneHandle
	);

	// OutSceneHandle.DirectionalLight = CreateDirectionalLight(WorldContextObject);
	OutSceneHandle.DirectionalLight = nullptr;

	OutSceneHandle.OcclusionRatio = 0.0f;


	if (IsValid(OutSceneHandle.ForegroundActor))
	{
		AUnrealcvWorldController* WorldController = FUnrealcvServer::Get().WorldController.Get();
		if (IsValid(WorldController))
		{
			WorldController->ObjectAnnotator.GetAnnotationColor(OutSceneHandle.ForegroundActor, OutSceneHandle.AnnotationColor);
			OutSceneHandle.AllAnnotationColors = WorldController->ObjectAnnotator.GetAnnotationColors();
		}
	}


	ActiveScenes.Add(OutSceneHandle);

	return true;
}

void USceneCompositionBPLib::ClearScene(const FSceneHandle& SceneHandle)
{
	if (IsValid(SceneHandle.ForegroundActor))
	{
		SceneHandle.ForegroundActor->Destroy();
	}

	for (AActor* Occluder : SceneHandle.OccluderActors)
	{
		if (IsValid(Occluder))
		{
			Occluder->Destroy();
		}
	}

	if (IsValid(SceneHandle.DirectionalLight))
	{
		SceneHandle.DirectionalLight->Destroy();
	}

	if (IsValid(SceneHandle.NavController))
	{
		SceneHandle.NavController->Destroy();
	}

	ActiveScenes.RemoveAll([&SceneHandle](const FSceneHandle& Handle) {
		return Handle.SceneID == SceneHandle.SceneID;
	});
}

void USceneCompositionBPLib::ClearAllScenes(UObject* WorldContextObject)
{
	for (const FSceneHandle& SceneHandle : ActiveScenes)
	{
		if (IsValid(SceneHandle.ForegroundActor))
		{
			SceneHandle.ForegroundActor->Destroy();
		}

		for (AActor* Occluder : SceneHandle.OccluderActors)
		{
			if (IsValid(Occluder))
			{
				Occluder->Destroy();
			}
		}

		if (IsValid(SceneHandle.DirectionalLight))
		{
			SceneHandle.DirectionalLight->Destroy();
		}
	}

	ActiveScenes.Empty();
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
		UE_LOG(LogUnrealCV, Error, TEXT("LoadAndSpawnActor: Invalid World"));
		return nullptr;
	}

	UObject* LoadedAsset = LoadObject<UObject>(nullptr, *AssetPath);
	if (!LoadedAsset)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("LoadAndSpawnActor: Asset not found '%s'"), *AssetPath);
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* SpawnedActor = nullptr;

	if (UBlueprint* Blueprint = Cast<UBlueprint>(LoadedAsset))
	{
		if (Blueprint->GeneratedClass && Blueprint->GeneratedClass->IsChildOf(AActor::StaticClass()))
		{
			SpawnedActor = World->SpawnActor<AActor>(Blueprint->GeneratedClass, Location, Rotation, SpawnParams);
		}
	}
	else if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(LoadedAsset))
	{
		AStaticMeshActor* MeshActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Location, Rotation, SpawnParams);
		if (MeshActor)
		{
			UStaticMeshComponent* MeshComponent = MeshActor->GetStaticMeshComponent();
			if (MeshComponent)
			{
				MeshComponent->SetMobility(EComponentMobility::Movable);
				MeshComponent->SetStaticMesh(StaticMesh);
			}
			SpawnedActor = MeshActor;
		}
	}
	else if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(LoadedAsset))
	{
		AActor* SkeletalActor = World->SpawnActor<AActor>(AActor::StaticClass(), Location, Rotation, SpawnParams);
		if (SkeletalActor)
		{
			USkeletalMeshComponent* SkeletalComponent = NewObject<USkeletalMeshComponent>(SkeletalActor);
			if (SkeletalComponent)
			{
				SkeletalComponent->SetMobility(EComponentMobility::Movable);
				SkeletalComponent->SetSkeletalMesh(SkeletalMesh);
				SkeletalComponent->RegisterComponent();
				SkeletalComponent->AttachToComponent(SkeletalActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
			}
			SpawnedActor = SkeletalActor;
		}
	}
	else if (UAnimSequence* AnimSequence = Cast<UAnimSequence>(LoadedAsset))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("LoadAndSpawnActor: AnimSequence not supported, need SkeletalMesh or Blueprint"));
	}

	return SpawnedActor;
}

AActor* USceneCompositionBPLib::SpawnActorFromMetadata(UWorld* World, const TMap<FString, FString>& Metadata, const FVector& Location, const FRotator& Rotation)
{
	if (!IsValid(World))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("SpawnActorFromMetadata: Invalid World"));
		return nullptr;
	}

	if (!Metadata.Contains(TEXT("Path")))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("SpawnActorFromMetadata: Metadata missing 'Path' key"));
		return nullptr;
	}

	FString ErrorMessage;
	if (!FAssetPoolManager::ValidateMetadata(Metadata, ErrorMessage))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("SpawnActorFromMetadata: Invalid metadata - %s"), *ErrorMessage);
		return nullptr;
	}

	const FString& AssetPath = Metadata[TEXT("Path")];
	const FString& AssetType = Metadata[TEXT("Type")];

	AActor* SpawnedActor = nullptr;

	if (AssetType == TEXT("StaticMesh"))
	{
		UStaticMesh* StaticMesh = LoadObject<UStaticMesh>(nullptr, *AssetPath);
		if (!IsValid(StaticMesh))
		{
			UE_LOG(LogUnrealCV, Error, TEXT("SpawnActorFromMetadata: StaticMesh not found '%s'"), *AssetPath);
			return nullptr;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* MeshActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Location, Rotation, SpawnParams);
		if (MeshActor)
		{
			UStaticMeshComponent* MeshComponent = MeshActor->GetStaticMeshComponent();
			if (MeshComponent)
			{
				MeshComponent->SetMobility(EComponentMobility::Movable);
				MeshComponent->SetStaticMesh(StaticMesh);
			}
		}
		SpawnedActor = MeshActor;
	}
	else if (AssetType == TEXT("Blueprint"))
	{
		UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
		if (!IsValid(Blueprint) || !Blueprint->GeneratedClass || !Blueprint->GeneratedClass->IsChildOf(AActor::StaticClass()))
		{
			UE_LOG(LogUnrealCV, Error, TEXT("SpawnActorFromMetadata: Blueprint not found or invalid '%s'"), *AssetPath);
			return nullptr;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnedActor = World->SpawnActor<AActor>(Blueprint->GeneratedClass, Location, Rotation, SpawnParams);
	}
	else if (AssetType == TEXT("SM+AnimSeq"))
	{
		USkeletalMesh* SkeletalMesh = LoadObject<USkeletalMesh>(nullptr, *AssetPath);
		if (!IsValid(SkeletalMesh))
		{
			UE_LOG(LogUnrealCV, Error, TEXT("SpawnActorFromMetadata: SkeletalMesh not found '%s'"), *AssetPath);
			return nullptr;
		}

		const FString& AnimSequencePath = Metadata[TEXT("AnimSequence")];
		UAnimSequence* AnimSeq = LoadObject<UAnimSequence>(nullptr, *AnimSequencePath);
		if (!IsValid(AnimSeq))
		{
			UE_LOG(LogUnrealCV, Error, TEXT("SpawnActorFromMetadata: AnimSequence not found '%s'"), *AnimSequencePath);
			return nullptr;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AAnimatedSkeletalActor* AnimatedActor = World->SpawnActor<AAnimatedSkeletalActor>(
			AAnimatedSkeletalActor::StaticClass(),
			Location,
			Rotation,
			SpawnParams
		);

		if (AnimatedActor)
		{
			AnimatedActor->InitializeFromAssets(SkeletalMesh, AnimSeq);
		}

		SpawnedActor = AnimatedActor;
	}
	else
	{
		UE_LOG(LogUnrealCV, Error, TEXT("SpawnActorFromMetadata: Unknown asset type '%s'"), *AssetType);
		return nullptr;
	}

	if (IsValid(SpawnedActor))
	{
		AUnrealcvWorldController* WorldController = FUnrealcvServer::Get().WorldController.Get();
		if (IsValid(WorldController))
		{
			int32 ColorIndex = WorldController->ObjectAnnotator.GetAnnotationColors().Num();
			FColor AnnotationColor = FColor::MakeRandomColor();

			if (ColorIndex < 32768)
			{
				FColorGenerator ColorGen;
				AnnotationColor = ColorGen.GetColorFromColorMap(ColorIndex);
			}

			WorldController->ObjectAnnotator.SetAnnotationColor(SpawnedActor, AnnotationColor);
		}
		else
		{
			UE_LOG(LogUnrealCV, Warning, TEXT("SpawnActorFromMetadata: WorldController not available, actor not annotated"));
		}
	}

	return SpawnedActor;
}

float USceneCompositionBPLib::GetBoundsRadiusFromMetadata(const TMap<FString, FString>& Metadata)
{
	FString ErrorMessage;
	if (!FAssetPoolManager::ValidateMetadata(Metadata, ErrorMessage))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("GetBoundsRadiusFromMetadata: Invalid metadata - %s"), *ErrorMessage);
		return 150.0f;
	}

	const FString& AssetPath = Metadata[TEXT("Path")];
	const FString& AssetType = Metadata[TEXT("Type")];

	if (AssetType == TEXT("StaticMesh"))
	{
		UStaticMesh* StaticMesh = LoadObject<UStaticMesh>(nullptr, *AssetPath);
		if (IsValid(StaticMesh))
		{
			return StaticMesh->GetBounds().SphereRadius;
		}
	}
	else if (AssetType == TEXT("SM+AnimSeq"))
	{
		USkeletalMesh* SkeletalMesh = LoadObject<USkeletalMesh>(nullptr, *AssetPath);
		if (IsValid(SkeletalMesh))
		{
			return SkeletalMesh->GetBounds().SphereRadius;
		}
	}

	return 150.0f;
}

void USceneCompositionBPLib::AdjustActorToGroundLevel(AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return;
	}

	FVector ActorLocation = Actor->GetActorLocation();
	FVector OriginalLocation = ActorLocation;

	FBox ActorBounds = Actor->GetComponentsBoundingBox();
	if (ActorBounds.IsValid)
	{
		float BottomZ = ActorBounds.Min.Z;
		float HeightOffset = -BottomZ;

		ActorLocation.Z += HeightOffset;
		Actor->SetActorLocation(ActorLocation, false, nullptr, ETeleportType::TeleportPhysics);

		UE_LOG(LogUnrealCV, Log, TEXT("AdjustActorToGroundLevel: Adjusted '%s' from Z=%.2f to Z=%.2f (bottom offset=%.2f)"),
			*Actor->GetName(), OriginalLocation.Z, ActorLocation.Z, BottomZ);
	}
	else
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("AdjustActorToGroundLevel: Failed to get valid bounds for actor '%s'"), *Actor->GetName());
	}
}

TArray<AActor*> USceneCompositionBPLib::SpawnRandomOccluders(
	UObject* WorldContextObject,
	int32 Count,
	FVector CameraPosition,
	FVector ForegroundPosition,
	const FString& OccluderCategory,
	FSceneHandle& OutSceneHandle)
{
	TArray<AActor*> SpawnedOccluders;

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("USceneCompositionBPLib::SpawnRandomOccluders: Invalid world context"));
		return SpawnedOccluders;
	}

	FAssetPoolManager& AssetPool = FAssetPoolManager::Get();

	struct FOccupiedSpace
	{
		FVector Position;
		float Radius;
	};
	TArray<FOccupiedSpace> OccupiedSpaces;

	const float SafetyMargin = 50.0f;
	const int32 MaxAttempts = 10;

	for (int32 i = 0; i < Count; i++)
	{
		TMap<FString, FString> Metadata = AssetPool.GetRandomAssetMetadata(OccluderCategory);
		if (Metadata.Num() == 0)
		{
			UE_LOG(LogUnrealCV, Warning, TEXT("USceneCompositionBPLib::SpawnRandomOccluders: Failed to get asset from category '%s'"), *OccluderCategory);
			continue;
		}

		float CurrentRadius = GetBoundsRadiusFromMetadata(Metadata);

		bool bSpawned = false;
		for (int32 Attempt = 0; Attempt < MaxAttempts; Attempt++)
		{
			float Alpha = FMath::RandRange(0.3f, 0.7f);
			FVector CandidatePosition = FMath::Lerp(CameraPosition, ForegroundPosition, Alpha);

			FVector Direction = (ForegroundPosition - CameraPosition).GetSafeNormal();
			FVector Perpendicular = FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal();
			float LateralOffset = FMath::RandRange(-200.0f, 200.0f);
			CandidatePosition += Perpendicular * LateralOffset;

			CandidatePosition.Z = 0.0f;

			bool bHasOverlap = false;
			for (const FOccupiedSpace& Occupied : OccupiedSpaces)
			{
				float MinDistance = Occupied.Radius + CurrentRadius + SafetyMargin;
				float Distance = FVector::Dist2D(CandidatePosition, Occupied.Position);
				if (Distance < MinDistance)
				{
					bHasOverlap = true;
					break;
				}
			}

			if (!bHasOverlap)
			{
				FRotator Rotation = FRotator::ZeroRotator;
				Rotation.Yaw = FMath::RandRange(0.0f, 360.0f);

				AActor* Occluder = SpawnActorFromMetadata(World, Metadata, CandidatePosition, Rotation);
				if (IsValid(Occluder))
				{
					AdjustActorToGroundLevel(Occluder);
					SpawnedOccluders.Add(Occluder);
					OccupiedSpaces.Add({CandidatePosition, CurrentRadius});
					OutSceneHandle.OccluderMetadataList.Add({Metadata});
					OutSceneHandle.OccluderCategory = OccluderCategory;
					bSpawned = true;
					break;
				}
			}
		}

		if (!bSpawned)
		{
			UE_LOG(LogUnrealCV, Warning, TEXT("USceneCompositionBPLib::SpawnRandomOccluders: Failed to find non-overlapping position after %d attempts"), MaxAttempts);
		}
	}

	return SpawnedOccluders;
}

// ========== Asset Pool Queries ==========

void USceneCompositionBPLib::RegisterAsset(const FString& Category, const FString& AssetPath)
{
	FAssetPoolManager::Get().RegisterAsset(Category, AssetPath);
}

void USceneCompositionBPLib::RegisterAssetWithMetadata(const FString& Category, const TMap<FString, FString>& Metadata)
{
	FAssetPoolManager::Get().RegisterAssetWithMetadata(Category, Metadata);
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

// // ========== Lighting ==========

// AActor* USceneCompositionBPLib::CreateDirectionalLight(
// 	UObject* WorldContextObject,
// 	float Intensity,
// 	FLinearColor Color)
// {
// 	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
// 	if (!World)
// 	{
// 		UE_LOG(LogUnrealCV, Error, TEXT("CreateDirectionalLight: Invalid world context"));
// 		return nullptr;
// 	}

// 	FActorSpawnParameters SpawnParams;
// 	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

// 	float RandomYaw = FMath::RandRange(0.0f, 360.0f);
// 	float RandomPitch = FMath::RandRange(-60.0f, -30.0f);
// 	FRotator LightRotation = FRotator(RandomPitch, RandomYaw, 0.0f);

// 	ADirectionalLight* DirectionalLight = World->SpawnActor<ADirectionalLight>(
// 		ADirectionalLight::StaticClass(),
// 		FVector::ZeroVector,
// 		LightRotation,
// 		SpawnParams
// 	);

// 	if (DirectionalLight)
// 	{
// 		UDirectionalLightComponent* LightComponent = DirectionalLight->GetComponent();
// 		if (LightComponent)
// 		{
// 			LightComponent->SetIntensity(Intensity);
// 			LightComponent->SetLightColor(Color);
// 			LightComponent->SetMobility(EComponentMobility::Movable);
// 		}
// 	}

// 	return DirectionalLight;
// }

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

	return true;
}

ANavAgentController* USceneCompositionBPLib::CreateNavAgentController(UObject* WorldContextObject, AActor* ControlledAgent)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World || !IsValid(ControlledAgent))
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ANavAgentController* NavController = World->SpawnActor<ANavAgentController>(
		ANavAgentController::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParams
	);

	if (NavController)
	{
		NavController->ControlledAgent = ControlledAgent;
		NavController->DefaultNavRadius = 1000.0f;
		NavController->MinNavRadius = 200.0f;
		NavController->ReachThreshold = 50.0f;
		NavController->MaxSteps = 200;
		NavController->bDebugDraw = false;
	}

	return NavController;
}
