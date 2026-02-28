// Copyright 2025 UnrealCV Team. All Rights Reserved.
#include "SceneCompositionBPLib.h"
#include "AssetPoolManager.h"
#include "SensorBPLib.h"
#include "FusionCamSensor.h"
#include "Sensor/CameraSensor/PawnCamSensor.h"
#include "UnrealcvLog.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PrimitiveComponent.h"
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
#include "MetaHumanBPLib.h"
#include "AnnotationBPLib.h"
#include "RuntimeActorSetterBPLib.h"
#include "DrawDebugHelpers.h"
#include "UnrealcvGameMode.h"
#include "EngineUtils.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "Misc/FileHelper.h"
#include "JsonConfigHelper.h"
#include "SpawnBPLib.h"

TArray<FSceneHandle> USceneCompositionBPLib::ActiveScenes;

FString USceneCompositionBPLib::GetSceneConfigFilePath()
{
	FString UserConfigPath = FPaths::Combine(FPlatformProcess::UserHomeDir(), TEXT("UnrealCV"), TEXT("SceneComposition.json"));

	UE_LOG(LogUnrealCV, Log, TEXT("GetSceneConfigFilePath: User home dir = '%s'"), FPlatformProcess::UserHomeDir());
	UE_LOG(LogUnrealCV, Log, TEXT("GetSceneConfigFilePath: Full config path = '%s'"), *UserConfigPath);

	if (FPaths::FileExists(UserConfigPath))
	{
		return UserConfigPath;
	}

	FString ProjectConfigPath = FPaths::ProjectSavedDir() / TEXT("SceneComposition.json");
	if (FPaths::FileExists(ProjectConfigPath))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("GetSceneConfigFilePath: Using legacy project config at '%s', consider migrating to '%s'"),
			*ProjectConfigPath, *UserConfigPath);
		return ProjectConfigPath;
	}

	FString UserConfigDir = FPaths::GetPath(UserConfigPath);
	if (!FPaths::DirectoryExists(UserConfigDir))
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		PlatformFile.CreateDirectoryTree(*UserConfigDir);
	}

	return UserConfigPath;
}

static APawn* GetFirstPersonPawn()
{
	UFusionCamSensor* FirstPersonCamera = USensorBPLib::GetSensorById(0);
	if (IsValid(FirstPersonCamera))
	{
		UPawnCamSensor* PawnSensor = Cast<UPawnCamSensor>(FirstPersonCamera->GetOuter());
		if (IsValid(PawnSensor))
		{
			return Cast<APawn>(PawnSensor->GetOwner());
		}
	}

	return nullptr;
}

AUnrealcvPawn* USceneCompositionBPLib::GetUnrealcvPawn(UWorld* World)
{
	if (!IsValid(World))
	{
		return nullptr;
	}

	for (TActorIterator<AUnrealcvPawn> ActorItr(World); ActorItr; ++ActorItr)
	{
		AUnrealcvPawn* Pawn = *ActorItr;
		if (IsValid(Pawn))
		{
			return Pawn;
		}
	}

	return nullptr;
}



// ========== Terrain Detection ==========

static bool PerformHeightTrace(
	UWorld* World,
	float X,
	float Y,
	float InitialHeight,
	float TraceLength,
	FCollisionQueryParams& QueryParams,
	FHitResult& OutHitResult)
{
	if (!IsValid(World))
	{
		return false;
	}

	QueryParams.TraceTag = TEXT("LandHeightTrace");

	const int32 NumRays = 9;
	const float SearchRadius = 20.0f;
	TArray<float> HitHeights;

	for (int32 i = 0; i < NumRays; ++i)
	{
		float OffsetX = 0.0f;
		float OffsetY = 0.0f;

		if (i == 0)
		{
			OffsetX = 0.0f;
			OffsetY = 0.0f;
		}
		else if (i == 1)
		{
			OffsetX = SearchRadius;
			OffsetY = 0.0f;
		}
		else if (i == 2)
		{
			OffsetX = -SearchRadius;
			OffsetY = 0.0f;
		}
		else if (i == 3)
		{
			OffsetX = 0.0f;
			OffsetY = SearchRadius;
		}
		else if (i == 4)
		{
			OffsetX = 0.0f;
			OffsetY = -SearchRadius;
		}
		else if (i == 5)
		{
			OffsetX = SearchRadius;
			OffsetY = SearchRadius;
		}
		else if (i == 6)
		{
			OffsetX = SearchRadius;
			OffsetY = -SearchRadius;
		}
		else if (i == 7)
		{
			OffsetX = -SearchRadius;
			OffsetY = SearchRadius;
		}
		else if (i == 8)
		{
			OffsetX = -SearchRadius;
			OffsetY = -SearchRadius;
		}

		FVector TraceStart = FVector(X + OffsetX, Y + OffsetY, InitialHeight);
		FVector TraceEnd = FVector(X + OffsetX, Y + OffsetY, InitialHeight - TraceLength);

		FHitResult HitResult;
		if (World->LineTraceSingleByChannel(
			HitResult,
			TraceStart,
			TraceEnd,
			ECC_WorldStatic,
			QueryParams))
		{
			HitHeights.Add(HitResult.ImpactPoint.Z);
		}
	}

	if (HitHeights.Num() == 0)
	{
		return false;
	}

	HitHeights.Sort();
	float MedianHeight = HitHeights[HitHeights.Num() / 2];

	OutHitResult.ImpactPoint.Z = MedianHeight;
	OutHitResult.bBlockingHit = true;

	return true;
}

float USceneCompositionBPLib::GetLandHeight(UWorld* World, float X, float Y, float InitialHeight)
{
	if (!IsValid(World))
	{
		return InitialHeight;
	}

	FCollisionQueryParams QueryParams;
	auto DefaultPawn = GetFirstPersonPawn();
	if (IsValid(DefaultPawn))
	{
		QueryParams.AddIgnoredActor(DefaultPawn);
	}
	auto UnrealcvPawn = GetUnrealcvPawn(World);
	if (IsValid(UnrealcvPawn))
	{
		QueryParams.AddIgnoredActor(UnrealcvPawn);
	}

	FHitResult HitResult;
	bool bHit = PerformHeightTrace(World, X, Y, InitialHeight, 10000.0f, QueryParams, HitResult);

	if (bHit && HitResult.bBlockingHit)
	{
		return HitResult.ImpactPoint.Z;
	}

	return InitialHeight;
}

void USceneCompositionBPLib::EnablePhysicsSettling(AActor* Actor, float InitialHeight)
{
	if (!IsValid(Actor))
	{
		return;
	}

	FVector ActorLocation = Actor->GetActorLocation();
	ActorLocation.Z = InitialHeight;
	Actor->SetActorLocation(ActorLocation, false, nullptr, ETeleportType::TeleportPhysics);

	for (UActorComponent* Component : Actor->GetComponents())
	{
		UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Component);
		if (PrimComp)
		{
			PrimComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			PrimComp->SetSimulatePhysics(true);
			PrimComp->SetEnableGravity(true);

			FVector LinearVelocity = FVector::ZeroVector;
			PrimComp->SetPhysicsLinearVelocity(LinearVelocity);
		}
	}

	UE_LOG(LogUnrealCV, Log, TEXT("EnablePhysicsSettling: Actor '%s' physics enabled, will settle to ground from height %.2f"),
		*Actor->GetName(), InitialHeight);
}

void USceneCompositionBPLib::SettleActorToGround(AActor* Actor, UWorld* World, float InitialHeight, float HeightOffset)
{
	if (!IsValid(Actor) || !IsValid(World))
	{
		return;
	}

	FVector ActorLocation = Actor->GetActorLocation();

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Actor);
	auto DefaultPawn = GetFirstPersonPawn();
	if (IsValid(DefaultPawn))
	{
		QueryParams.AddIgnoredActor(DefaultPawn);
	}
	auto UnrealcvPawn = GetUnrealcvPawn(World);
	if (IsValid(UnrealcvPawn))
	{
		QueryParams.AddIgnoredActor(UnrealcvPawn);
	}

	FHitResult HitResult;
	bool bHit = PerformHeightTrace(World, ActorLocation.X, ActorLocation.Y, InitialHeight, 10000.0f, QueryParams, HitResult);

	if (bHit && HitResult.bBlockingHit)
	{
		ActorLocation.Z = HitResult.ImpactPoint.Z + HeightOffset;
	}
	else
	{
		ActorLocation.Z = InitialHeight;
	}

	Actor->SetActorLocation(ActorLocation, false, nullptr, ETeleportType::TeleportPhysics);

	UE_LOG(LogUnrealCV, Log, TEXT("SettleActorToGround: Actor '%s' settled to height %.2f"),
		*Actor->GetName(), ActorLocation.Z);
}


void USceneCompositionBPLib::EnableCollisionOnly(AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return;
	}

	for (UActorComponent* Component : Actor->GetComponents())
	{
		UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Component);
		if (PrimComp)
		{
			PrimComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			PrimComp->SetGenerateOverlapEvents(false);
		}
	}

	UE_LOG(LogUnrealCV, Log, TEXT("EnableCollisionOnly: Actor '%s' collision enabled (query only, no physics)"),
		*Actor->GetName());
}

bool USceneCompositionBPLib::CheckCollisionAtLocation(UWorld* World, const FVector& Location, float Radius, const TArray<AActor*>& IgnoreActors)
{
	if (!IsValid(World))
	{
		return false;
	}

	FCollisionQueryParams QueryParams;
	QueryParams.TraceTag = TEXT("ActorCollisionCheck");
	for (AActor* IgnoreActor : IgnoreActors)
	{
		if (IsValid(IgnoreActor))
		{
			QueryParams.AddIgnoredActor(IgnoreActor);
		}
	}

	FCollisionShape SphereShape = FCollisionShape::MakeSphere(Radius);

	bool bHasCollision = World->OverlapBlockingTestByChannel(
		Location,
		FQuat::Identity,
		ECC_Pawn,
		SphereShape,
		QueryParams
	);

	return bHasCollision;
}

bool USceneCompositionBPLib::FindCollisionFreeLocation(UWorld* World, FVector& OutLocation, float Radius, const TArray<AActor*>& IgnoreActors, int32 MaxAttempts, float SearchRadius)
{
	if (!IsValid(World))
	{
		return false;
	}

	FVector OriginalLocation = OutLocation;

	for (int32 Attempt = 0; Attempt < MaxAttempts; Attempt++)
	{
		float RandomDistance = FMath::RandRange(0.0f, SearchRadius);
		float RandomAngle = FMath::RandRange(0.0f, 360.0f);

		FVector Offset = FVector(
			RandomDistance * FMath::Cos(FMath::DegreesToRadians(RandomAngle)),
			RandomDistance * FMath::Sin(FMath::DegreesToRadians(RandomAngle)),
			0.0f
		);

		FVector CandidateLocation = OriginalLocation + Offset;

		if (!CheckCollisionAtLocation(World, CandidateLocation, Radius, IgnoreActors))
		{
			OutLocation = CandidateLocation;
			return true;
		}
	}

	OutLocation = OriginalLocation;
	return false;
}

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

bool USceneCompositionBPLib::CreateSceneParamsFromJson(
	UObject* WorldContextObject,
	const FString& JsonFilePath,
	FSceneGenerationParams& OutParams)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("CreateSceneParamsFromJson: Invalid world context"));
		return false;
	}

	FString ConfigFilePath = JsonFilePath.IsEmpty() ? GetSceneConfigFilePath() : JsonFilePath;

	FString JsonFileContent;
	if (!FFileHelper::LoadFileToString(JsonFileContent, *ConfigFilePath))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("CreateSceneParamsFromJson: Failed to read JSON file '%s'"), *ConfigFilePath);
		return false;
	}

	// Parse JSON
	TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonFileContent);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogUnrealCV, Error, TEXT("CreateSceneParamsFromJson: Failed to parse JSON from '%s'"), *JsonFilePath);
		return false;
	}

	FString CurrentMapPath = World->GetMapName();
	FString CurrentMapName = FJsonConfigHelper::ExtractMapNameFromPath(CurrentMapPath);

	UE_LOG(LogUnrealCV, Log, TEXT("CreateSceneParamsFromJson: Current map name: %s"), *CurrentMapName);

	// Find matching scene config by prefix matching
	TSharedPtr<FJsonObject> MatchingConfig = nullptr;
	FString MatchedKey;

	for (const auto& Pair : JsonObject->Values)
	{
		const FString& Key = Pair.Key;
		// if (CurrentMapName.StartsWith(Key) || Key.StartsWith(CurrentMapName))
		if (CurrentMapName.Find(Key) != INDEX_NONE)
		{
			if (Pair.Value->Type == EJson::Object)
			{
				MatchingConfig = Pair.Value->AsObject();
				MatchedKey = Key;
				UE_LOG(LogUnrealCV, Log, TEXT("CreateSceneParamsFromJson: Found matching config key: %s"), *Key);
				break;
			}
		}
	}

	if (!MatchingConfig.IsValid())
	{
		UE_LOG(LogUnrealCV, Error, TEXT("CreateSceneParamsFromJson: No matching scene config found in JSON for map '%s'"), *CurrentMapName);
		return false;
	}
	else
	{
		UE_LOG(LogUnrealCV, Log, TEXT("CreateSceneParamsFromJson: Matched scene config %s in JSON for map '%s'"), *MatchedKey, *CurrentMapName);
	}

	// Initialize with defaults
	// OutParams = FSceneGenerationParams();

	// Parse spawn area (required if present)
	bool bHasSpawnArea = false;
	if (MatchingConfig->HasField(TEXT("XMin")) && MatchingConfig->HasField(TEXT("XMax")) &&
		MatchingConfig->HasField(TEXT("YMin")) && MatchingConfig->HasField(TEXT("YMax")))
	{
		double XMin = 0.0, XMax = 1000.0, YMin = 0.0, YMax = 1000.0;

		TSharedPtr<FJsonValue> XMinVal = MatchingConfig->TryGetField(TEXT("XMin"));
		TSharedPtr<FJsonValue> XMaxVal = MatchingConfig->TryGetField(TEXT("XMax"));
		TSharedPtr<FJsonValue> YMinVal = MatchingConfig->TryGetField(TEXT("YMin"));
		TSharedPtr<FJsonValue> YMaxVal = MatchingConfig->TryGetField(TEXT("YMax"));

		if (FJsonConfigHelper::ParseJsonNumber(XMinVal, XMin) &&
			FJsonConfigHelper::ParseJsonNumber(XMaxVal, XMax) &&
			FJsonConfigHelper::ParseJsonNumber(YMinVal, YMin) &&
			FJsonConfigHelper::ParseJsonNumber(YMaxVal, YMax))
		{
			OutParams.SpawnAreaMin = FVector2D(XMin, YMin);
			OutParams.SpawnAreaMax = FVector2D(XMax, YMax);
			bHasSpawnArea = true;
			UE_LOG(LogUnrealCV, Log, TEXT("CreateSceneParamsFromJson: Spawn area (XMin/XMax/YMin/YMax) parsed: %f"), XMin);
			UE_LOG(LogUnrealCV, Log, TEXT("CreateSceneParamsFromJson: Spawn area (XMin/XMax/YMin/YMax) parsed: %f"), XMax);
			UE_LOG(LogUnrealCV, Log, TEXT("CreateSceneParamsFromJson: Spawn area (XMin/XMax/YMin/YMax) parsed: %f"), YMin);
			UE_LOG(LogUnrealCV, Log, TEXT("CreateSceneParamsFromJson: Spawn area (XMin/XMax/YMin/YMax) parsed: %f"), YMax);
		}
		else
		{
			UE_LOG(LogUnrealCV, Error, TEXT("CreateSceneParamsFromJson: Failed to parse spawn area (XMin/XMax/YMin/YMax) from config '%s'"), *MatchedKey);
		}
	}

	if (!bHasSpawnArea)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("CreateSceneParamsFromJson: Spawn area (XMin/XMax/YMin/YMax) not found in config '%s', using defaults"), *MatchedKey);
	}

	// Parse GroundHeight
	if (MatchingConfig->HasField(TEXT("GroundHeight")))
	{
		double GroundHeight = 0.0;
		TSharedPtr<FJsonValue> Val = MatchingConfig->TryGetField(TEXT("GroundHeight"));
		if (FJsonConfigHelper::ParseJsonNumber(Val, GroundHeight))
		{
			OutParams.GroundHeight = GroundHeight;
			UE_LOG(LogUnrealCV, Log, TEXT("CreateSceneParamsFromJson: GroundHeight parsed: %f"), GroundHeight);
		}
		else
		{
			UE_LOG(LogUnrealCV, Warning, TEXT("CreateSceneParamsFromJson: Failed to parse GroundHeight, using default"));
		}
	}
	else
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("CreateSceneParamsFromJson: GroundHeight not found in config '%s', using default"), *MatchedKey);
	}

	// Parse ForegroundPathSpec (non-empty means use it)
	if (MatchingConfig->HasField(TEXT("ForegroundPathSpec")))
	{
		FString Value;
		TSharedPtr<FJsonValue> Val = MatchingConfig->TryGetField(TEXT("ForegroundPathSpec"));
		if (FJsonConfigHelper::ParseJsonValue(Val, Value))
		{
			OutParams.ForegroundPathSpec = Value;
			UE_LOG(LogUnrealCV, Log, TEXT("CreateSceneParamsFromJson: ForegroundPathSpec parsed: %s"), *Value);
		}
		else
		{
			UE_LOG(LogUnrealCV, Warning, TEXT("CreateSceneParamsFromJson: Failed to parse ForegroundPathSpec"));
		}
	}

	// Parse ForegroundCategory (use if non-empty)
	if (MatchingConfig->HasField(TEXT("ForegroundCategory")))
	{
		FString Value;
		TSharedPtr<FJsonValue> Val = MatchingConfig->TryGetField(TEXT("ForegroundCategory"));
		if (FJsonConfigHelper::ParseJsonValue(Val, Value))
		{
			OutParams.ForegroundCategory = Value;
			UE_LOG(LogUnrealCV, Log, TEXT("CreateSceneParamsFromJson: ForegroundCategory parsed: %s"), *Value);
		}
		else
		{
			UE_LOG(LogUnrealCV, Warning, TEXT("CreateSceneParamsFromJson: Failed to parse ForegroundCategory"));
		}
	}
	else
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("CreateSceneParamsFromJson: ForegroundCategory not found in config '%s', using default"), *MatchedKey);
	}

	// Parse OccluderPathSpec (non-empty means use it)
	if (MatchingConfig->HasField(TEXT("OccluderPathSpec")))
	{
		FString Value;
		TSharedPtr<FJsonValue> Val = MatchingConfig->TryGetField(TEXT("OccluderPathSpec"));
		if (FJsonConfigHelper::ParseJsonValue(Val, Value))
		{
			OutParams.OccluderPathSpec = Value;
			UE_LOG(LogUnrealCV, Log, TEXT("CreateSceneParamsFromJson: OccluderPathSpec parsed: %s"), *Value);
		}
		else
		{
			UE_LOG(LogUnrealCV, Warning, TEXT("CreateSceneParamsFromJson: Failed to parse OccluderPathSpec"));
		}
	}


	// Parse OccluderCategory (use if non-empty)
	if (MatchingConfig->HasField(TEXT("OccluderCategory")))
	{
		FString Value;
		TSharedPtr<FJsonValue> Val = MatchingConfig->TryGetField(TEXT("OccluderCategory"));
		if (FJsonConfigHelper::ParseJsonValue(Val, Value))
		{
			UE_LOG(LogUnrealCV, Log, TEXT("CreateSceneParamsFromJson: OccluderCategory parsed: %s"), *Value);
		}
		else
		{
			UE_LOG(LogUnrealCV, Warning, TEXT("CreateSceneParamsFromJson: Failed to parse OccluderCategory"));
		}
	}
	else
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("CreateSceneParamsFromJson: OccluderCategory not found in config '%s', using default"), *MatchedKey);
	}

	// Parse OccluderCount (0 and non-zero both valid)
	if (MatchingConfig->HasField(TEXT("OccluderCount")))
	{
		double Value = -1.0;
		TSharedPtr<FJsonValue> Val = MatchingConfig->TryGetField(TEXT("OccluderCount"));
		if (FJsonConfigHelper::ParseJsonNumber(Val, Value))
		{
			OutParams.OccluderCount = (int32)Value;
		}
		else
		{
			UE_LOG(LogUnrealCV, Warning, TEXT("CreateSceneParamsFromJson: Failed to parse OccluderCount, using default"));
		}
	}
	else
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("CreateSceneParamsFromJson: OccluderCount not found in config '%s', using default"), *MatchedKey);
	}

	// Parse CameraID
	if (MatchingConfig->HasField(TEXT("CameraID")))
	{
		FString Value;
		TSharedPtr<FJsonValue> Val = MatchingConfig->TryGetField(TEXT("CameraID"));
		if (FJsonConfigHelper::ParseJsonValue(Val, Value))
		{
			int32 CameraID = USensorBPLib::GetIndexByAnyID(Value);
			if (CameraID < 0)
			{
				UE_LOG(LogUnrealCV, Error, TEXT("CreateSceneParamsFromJson: CameraID is not valid, using 1"));
				OutParams.CameraID = 1;
			}
			else
			{
				OutParams.CameraID = CameraID;
				UE_LOG(LogUnrealCV, Log, TEXT("CreateSceneParamsFromJson: CameraID parsed: %d"), CameraID);
			}
		}
		else
		{
			UE_LOG(LogUnrealCV, Warning, TEXT("CreateSceneParamsFromJson: Failed to parse CameraID, using default"));
		}
	}
	else
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("CreateSceneParamsFromJson: CameraID not found in config '%s', using default"), *MatchedKey);
	}

	// Parse bAutoPositionCamera
	if (MatchingConfig->HasField(TEXT("bAutoPositionCamera")))
	{
		TSharedPtr<FJsonValue> JsonVal = MatchingConfig->TryGetField(TEXT("bAutoPositionCamera"));
		if (JsonVal.IsValid() && JsonVal->Type == EJson::Boolean)
		{
			OutParams.bAutoPositionCamera = JsonVal->AsBool();
		}
		else
		{
			UE_LOG(LogUnrealCV, Warning, TEXT("CreateSceneParamsFromJson: Failed to parse bAutoPositionCamera, using default"));
		}
	}
	else
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("CreateSceneParamsFromJson: bAutoPositionCamera not found in config '%s', using default"), *MatchedKey);
	}

	// Parse ForegroundYaw (use as-is, -1 means random in the generation function)
	if (MatchingConfig->HasField(TEXT("ForegroundYaw")))
	{
		double Value = 0.0;
		TSharedPtr<FJsonValue> Val = MatchingConfig->TryGetField(TEXT("ForegroundYaw"));
		if (FJsonConfigHelper::ParseJsonNumber(Val, Value))
		{
			OutParams.ForegroundYaw = Value;
			UE_LOG(LogUnrealCV, Log, TEXT("CreateSceneParamsFromJson: ForegroundYaw parsed: %f"), Value);
		}
		else
		{
			UE_LOG(LogUnrealCV, Warning, TEXT("CreateSceneParamsFromJson: Failed to parse ForegroundYaw, using default"));
		}
	}
	else
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("CreateSceneParamsFromJson: ForegroundYaw not found in config '%s', using default"), *MatchedKey);
	}

	// Parse AutoPositionCameraHeight
	if (MatchingConfig->HasField(TEXT("AutoPositionCameraHeight")))
	{
		double Value = 167.5;
		TSharedPtr<FJsonValue> Val = MatchingConfig->TryGetField(TEXT("AutoPositionCameraHeight"));
		if (FJsonConfigHelper::ParseJsonNumber(Val, Value))
		{
			OutParams.AutoPositionCameraHeight = Value;
			UE_LOG(LogUnrealCV, Log, TEXT("CreateSceneParamsFromJson: AutoPositionCameraHeight parsed: %f"), Value);
		}
		else
		{
			UE_LOG(LogUnrealCV, Warning, TEXT("CreateSceneParamsFromJson: Failed to parse AutoPositionCameraHeight, using default"));
		}
	}

	// Parse AutoPositionCameraDistance
	if (MatchingConfig->HasField(TEXT("AutoPositionCameraDistance")))
	{
		double Value = 325.0;
		TSharedPtr<FJsonValue> Val = MatchingConfig->TryGetField(TEXT("AutoPositionCameraDistance"));
		if (FJsonConfigHelper::ParseJsonNumber(Val, Value))
		{
			OutParams.AutoPositionCameraDistance = Value;
			UE_LOG(LogUnrealCV, Log, TEXT("CreateSceneParamsFromJson: AutoPositionCameraDistance parsed: %f"), Value);
		}
		else
		{
			UE_LOG(LogUnrealCV, Warning, TEXT("CreateSceneParamsFromJson: Failed to parse AutoPositionCameraDistance, using default"));
		}
	}

	// Parse AutoPositionCameraAngleOffset
	if (MatchingConfig->HasField(TEXT("AutoPositionCameraAngleOffset")))
	{
		double Value = 0.0;
		TSharedPtr<FJsonValue> Val = MatchingConfig->TryGetField(TEXT("AutoPositionCameraAngleOffset"));
		if (FJsonConfigHelper::ParseJsonNumber(Val, Value))
		{
			OutParams.AutoPositionCameraAngleOffset = Value;
			UE_LOG(LogUnrealCV, Log, TEXT("CreateSceneParamsFromJson: AutoPositionCameraAngleOffset parsed: %f"), Value);
		}
		else
		{
			UE_LOG(LogUnrealCV, Warning, TEXT("CreateSceneParamsFromJson: Failed to parse AutoPositionCameraAngleOffset, using default"));
		}
	}

	if (MatchingConfig->HasField(TEXT("SafePoints")))
	{
		const TArray<TSharedPtr<FJsonValue>>* SafePointsArray = nullptr;
		if (MatchingConfig->TryGetArrayField(TEXT("SafePoints"), SafePointsArray))
		{
			OutParams.SafePoints.Empty();
			for (const TSharedPtr<FJsonValue>& PointValue : *SafePointsArray)
			{
				if (PointValue->Type == EJson::Object)
				{
					TSharedPtr<FJsonObject> PointObj = PointValue->AsObject();

					if (PointObj->HasField(TEXT("Position")))
					{
						TSharedPtr<FJsonObject> PosObj = PointObj->GetObjectField(TEXT("Position"));
						double X = 0.0, Y = 0.0, Z = 0.0;

						TSharedPtr<FJsonValue> XVal = PosObj->TryGetField(TEXT("X"));
						TSharedPtr<FJsonValue> YVal = PosObj->TryGetField(TEXT("Y"));
						TSharedPtr<FJsonValue> ZVal = PosObj->TryGetField(TEXT("Z"));

						if (FJsonConfigHelper::ParseJsonNumber(XVal, X) &&
							FJsonConfigHelper::ParseJsonNumber(YVal, Y) &&
							FJsonConfigHelper::ParseJsonNumber(ZVal, Z))
						{
							OutParams.SafePoints.Add(FVector(X, Y, Z));
						}
					}
					else
					{
						UE_LOG(LogUnrealCV, Error, TEXT("CreateSceneParamsFromJson: Safe point missing 'Position' field"));
					}
				}
			}
			UE_LOG(LogUnrealCV, Log, TEXT("CreateSceneParamsFromJson: Parsed %d safe points"), OutParams.SafePoints.Num());
		}
	}

	UE_LOG(LogUnrealCV, Log, TEXT("CreateSceneParamsFromJson: Successfully loaded params from config '%s'"), *MatchedKey);
	return true;
}

bool USceneCompositionBPLib::GenerateRandomScene(
	UObject* WorldContextObject,
	const FSceneGenerationParams& Params,
	FSceneHandle& OutSceneHandle)
{
	UE_LOG(LogUnrealCV, Log, TEXT("USceneCompositionBPLib::GenerateRandomScene: Input params debug print ->"));
	Params.DebugPrint();

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("USceneCompositionBPLib::GenerateRandomScene: Invalid world context"));
		return false;
	}

	FAssetPoolManager& AssetPool = FAssetPoolManager::Get();

	FString ResolvedForegroundCategory = Params.ForegroundCategory;

	if (!Params.ForegroundPathSpec.IsEmpty())
	{
		FString ResolvedCategory = AssetPool.GetCategoryByAssetPath(Params.ForegroundPathSpec);
		if (ResolvedCategory.IsEmpty())
		{
			UE_LOG(LogUnrealCV, Error, TEXT("GenerateRandomScene: Asset path '%s' not found in asset pool"), *Params.ForegroundPathSpec);
			return false;
		}
		ResolvedForegroundCategory = ResolvedCategory;
		UE_LOG(LogUnrealCV, Log, TEXT("GenerateRandomScene: Resolved foreground path '%s' to category '%s'"), *Params.ForegroundPathSpec, *ResolvedForegroundCategory);
	}

	FString ResolvedOccluderCategory = Params.OccluderCategory;

	if (!Params.OccluderPathSpec.IsEmpty())
	{
		FString ResolvedCategory = AssetPool.GetCategoryByAssetPath(Params.OccluderPathSpec);
		if (ResolvedCategory.IsEmpty())
		{
			UE_LOG(LogUnrealCV, Error, TEXT("GenerateRandomScene: Occluder asset path '%s' not found in asset pool"), *Params.OccluderPathSpec);
			return false;
		}
		ResolvedOccluderCategory = ResolvedCategory;
		UE_LOG(LogUnrealCV, Log, TEXT("GenerateRandomScene: Resolved occluder path '%s' to category '%s'"), *Params.OccluderPathSpec, *ResolvedOccluderCategory);
	}

	if (!AssetPool.HasCategory(ResolvedForegroundCategory))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("GenerateRandomScene: Invalid foreground category '%s'"), *ResolvedForegroundCategory);
		return false;
	}
	if (!AssetPool.HasCategory(ResolvedOccluderCategory))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("GenerateRandomScene: Invalid occluder category '%s'"), *ResolvedOccluderCategory);
		return false;
	}

	OutSceneHandle = FSceneHandle();
	OutSceneHandle.SceneID = GenerateSceneID();
	OutSceneHandle.CameraID = Params.CameraID;
	OutSceneHandle.ForegroundCategory = ResolvedForegroundCategory;

	FVector ForegroundPosition;
	if (Params.SafePoints.Num() > 0)
	{
		int32 RandomIndex = FMath::RandRange(0, Params.SafePoints.Num() - 1);
		ForegroundPosition = Params.SafePoints[RandomIndex];
		UE_LOG(LogUnrealCV, Log, TEXT("GenerateRandomScene: Using safe point %d: (%.2f, %.2f, %.2f)"),
			RandomIndex, ForegroundPosition.X, ForegroundPosition.Y, ForegroundPosition.Z);
	}
	else
	{
		ForegroundPosition.X = FMath::RandRange(Params.SpawnAreaMin.X, Params.SpawnAreaMax.X);
		ForegroundPosition.Y = FMath::RandRange(Params.SpawnAreaMin.Y, Params.SpawnAreaMax.Y);
		ForegroundPosition.Z = Params.GroundHeight;
		UE_LOG(LogUnrealCV, Log, TEXT("GenerateRandomScene: Using rectangle randomization: (%.2f, %.2f, %.2f)"),
			ForegroundPosition.X, ForegroundPosition.Y, ForegroundPosition.Z);
	}

	TMap<FString, FString> ForegroundMetadata;

	if (!Params.ForegroundPathSpec.IsEmpty())
	{
		ForegroundMetadata = AssetPool.GetAssetMetadataByPath(Params.ForegroundPathSpec);
	}
	else
	{
		ForegroundMetadata = AssetPool.GetRandomAssetMetadata(ResolvedForegroundCategory);
	}

	if (ForegroundMetadata.Num() == 0)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("GenerateRandomScene: No assets in foreground category '%s'"), *ResolvedForegroundCategory);
		return false;
	}

	FRotator ForegroundRotation = FRotator::ZeroRotator;
	float Yaw = (Params.ForegroundYaw == -1.0f) ? FMath::RandRange(0.0f, 360.0f) : Params.ForegroundYaw;
	ForegroundRotation.Yaw = Yaw - 90.0f;

	OutSceneHandle.ForegroundActor = SpawnActorFromMetadata(World, ForegroundMetadata, ForegroundPosition, ForegroundRotation);
	if (!IsValid(OutSceneHandle.ForegroundActor))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("GenerateRandomScene: Failed to spawn foreground actor"));
		return false;
	}

	OutSceneHandle.ForegroundCategory = ResolvedForegroundCategory;
	OutSceneHandle.ForegroundObjectMetadata = ForegroundMetadata;
	OutSceneHandle.SceneCategory = World->GetMapName();

	if (ForegroundMetadata.Contains(TEXT("Type")) && ForegroundMetadata[TEXT("Type")] == TEXT("Blueprint"))
	{
		OutSceneHandle.NavController = CreateNavAgentController(WorldContextObject, OutSceneHandle.ForegroundActor);
		if (IsValid(OutSceneHandle.NavController))
		{
			OutSceneHandle.bHasNavigation = true;
			UE_LOG(LogUnrealCV, Log, TEXT("GenerateRandomScene: Applied NavAgent to Blueprint actor '%s'"), *OutSceneHandle.ForegroundActor->GetName());
		}
	}

	UFusionCamSensor* Camera = USensorBPLib::GetSensorById(Params.CameraID);
	if (!IsValid(Camera))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("GenerateRandomScene: Invalid camera ID %d"), Params.CameraID);
		ClearScene(OutSceneHandle);
		return false;
	}

	if (Params.bAutoPositionCamera)
	{
		auto NewPosition = OutSceneHandle.ForegroundActor->GetActorLocation();
		float CameraHeight = Params.AutoPositionCameraHeight;
		float Distance = Params.AutoPositionCameraDistance;

		float CameraAngleOffset = Params.AutoPositionCameraAngleOffset;
		float HorizontalAngle = Yaw + CameraAngleOffset;

		FVector CameraPosition;
		CameraPosition.X = NewPosition.X + Distance * FMath::Cos(FMath::DegreesToRadians(HorizontalAngle));
		CameraPosition.Y = NewPosition.Y + Distance * FMath::Sin(FMath::DegreesToRadians(HorizontalAngle));
		CameraPosition.Z = CameraHeight + NewPosition.Z;

		// float LandHeight = GetLandHeight(World, CameraPosition.X, CameraPosition.Y, CameraPosition.Z);
		// CameraPosition.Z = LandHeight + CameraHeight;

		FRotator CameraRotation = (NewPosition - CameraPosition).Rotation();
		CameraRotation.Roll = 0.0f;
		Camera->SetSensorLocation(CameraPosition);
		Camera->SetSensorRotation(CameraRotation);
	}

	FVector CameraPosition = Camera->GetSensorLocation();

	OutSceneHandle.OccluderActors = SpawnRandomOccluders(
		WorldContextObject,
		Params.OccluderCount,
		CameraPosition,
		ForegroundPosition,
		Params.OccluderPathSpec,
		ResolvedOccluderCategory,
		OutSceneHandle
	);

	OutSceneHandle.DirectionalLight = nullptr;

	ActiveScenes.Add(OutSceneHandle);

	// UAnnotationBPLib::AnnotateWorld();
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

// AActor* USceneCompositionBPLib::LoadAndSpawnActor(UWorld* World, const FString& AssetPath, const FVector& Location, const FRotator& Rotation)
// {
// 	if (!IsValid(World))
// 	{
// 		UE_LOG(LogUnrealCV, Error, TEXT("LoadAndSpawnActor: Invalid World"));
// 		return nullptr;
// 	}

// 	UObject* LoadedAsset = LoadObject<UObject>(nullptr, *AssetPath);
// 	if (!LoadedAsset)
// 	{
// 		UE_LOG(LogUnrealCV, Error, TEXT("LoadAndSpawnActor: Asset not found '%s'"), *AssetPath);
// 		return nullptr;
// 	}

// 	FActorSpawnParameters SpawnParams;
// 	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
// 	AActor* SpawnedActor = nullptr;

// 	if (UBlueprint* Blueprint = Cast<UBlueprint>(LoadedAsset))
// 	{
// 		if (Blueprint->GeneratedClass && Blueprint->GeneratedClass->IsChildOf(AActor::StaticClass()))
// 		{
// 			SpawnedActor = World->SpawnActor<AActor>(Blueprint->GeneratedClass, Location, Rotation, SpawnParams);
// 		}
// 	}
// 	else if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(LoadedAsset))
// 	{
// 		AStaticMeshActor* MeshActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Location, Rotation, SpawnParams);
// 		if (MeshActor)
// 		{
// 			UStaticMeshComponent* MeshComponent = MeshActor->GetStaticMeshComponent();
// 			if (MeshComponent)
// 			{
// 				MeshComponent->SetMobility(EComponentMobility::Movable);
// 				MeshComponent->SetStaticMesh(StaticMesh);
// 			}
// 			SpawnedActor = MeshActor;
// 		}
// 	}
// 	else if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(LoadedAsset))
// 	{
// 		AActor* SkeletalActor = World->SpawnActor<AActor>(AActor::StaticClass(), Location, Rotation, SpawnParams);
// 		if (SkeletalActor)
// 		{
// 			USkeletalMeshComponent* SkeletalComponent = NewObject<USkeletalMeshComponent>(SkeletalActor);
// 			if (SkeletalComponent)
// 			{
// 				SkeletalComponent->SetMobility(EComponentMobility::Movable);
// 				SkeletalComponent->SetSkeletalMesh(SkeletalMesh);
// 				SkeletalComponent->RegisterComponent();
// 				SkeletalComponent->AttachToComponent(SkeletalActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
// 			}
// 			SpawnedActor = SkeletalActor;
// 		}
// 	}
// 	else if (UAnimSequence* AnimSequence = Cast<UAnimSequence>(LoadedAsset))
// 	{
// 		UE_LOG(LogUnrealCV, Warning, TEXT("LoadAndSpawnActor: AnimSequence not supported, need SkeletalMesh or Blueprint"));
// 	}

// 	return SpawnedActor;
// }

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

	if (AssetType == TEXT("SM+AnimSeq"))
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
		SpawnedActor = USpawnBPLib::SpawnActorFromPath(World, AssetPath, Location, Rotation);
	}

	if (!IsValid(SpawnedActor))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("SpawnActorFromMetadata: Failed to spawn actor '%s'"), *AssetPath);
		return nullptr;
	}

	{
		AUnrealcvWorldController* WorldController = FUnrealcvServer::Get().WorldController.Get();
		if (IsValid(WorldController))
		{
			SettleActorToGround(SpawnedActor, World, Location.Z + 200.0f);
			EnableCollisionOnly(SpawnedActor);
		}
		else
		{
			UE_LOG(LogUnrealCV, Warning, TEXT("SpawnActorFromMetadata: WorldController not available, actor not annotated"));
		}
	}

	FVector FinalLocation = Location;
	{
		float ActorRadius = GetBoundsRadiusFromMetadata(Metadata) + 50.0f;
		TArray<AActor*> IgnoreList;
		IgnoreList.Add(SpawnedActor);

		if (CheckCollisionAtLocation(World, FinalLocation, ActorRadius, IgnoreList))
		{
			if (FindCollisionFreeLocation(World, FinalLocation, ActorRadius, IgnoreList, 15, 500.0f))
			{
				SpawnedActor->SetActorLocation(FinalLocation, false, nullptr, ETeleportType::TeleportPhysics);
				UE_LOG(LogUnrealCV, Warning, TEXT("SpawnActorFromMetadata: Actor '%s' collision detected at original position, relocated to %.2f, %.2f, %.2f"),
					*SpawnedActor->GetName(), FinalLocation.X, FinalLocation.Y, FinalLocation.Z);
			}
			else
			{
				UE_LOG(LogUnrealCV, Warning, TEXT("SpawnActorFromMetadata: Actor '%s' collision detected, could not find collision-free location after 15 attempts"),
					*SpawnedActor->GetName());
			}
		}
	}

	UE_LOG(LogUnrealCV, Warning, TEXT("SpawnActorFromMetadata: Spawned actor '%s' at location %.2f, %.2f, %.2f"),
		*SpawnedActor->GetName(), FinalLocation.X, FinalLocation.Y, FinalLocation.Z);

	SpawnedActor->RegisterAllComponents();

	World->GetTimerManager().SetTimerForNextTick([SpawnedActor]() {
		UAnnotationBPLib::AnnotateActor(SpawnedActor);
	});
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
	const FString& OccluderPathSpec,
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

	const float SafetyMargin = 50.0f;
	const int32 MaxLocationAttempts = 15;
	const int32 MaxAssetAttempts = 10;

	for (int32 i = 0; i < Count; i++)
	{
		TMap<FString, FString> Metadata;

		if (!OccluderPathSpec.IsEmpty())
		{
			Metadata = AssetPool.GetAssetMetadataByPath(OccluderPathSpec);
			if (Metadata.Num() == 0)
			{
				UE_LOG(LogUnrealCV, Warning, TEXT("USceneCompositionBPLib::SpawnRandomOccluders: Specified occluder path '%s' not found in asset pool"), *OccluderPathSpec);
				continue;
			}
		}
		else
		{
			Metadata = AssetPool.GetRandomAssetMetadata(OccluderCategory);
			if (Metadata.Num() == 0)
			{
				UE_LOG(LogUnrealCV, Warning, TEXT("USceneCompositionBPLib::SpawnRandomOccluders: Failed to get asset from category '%s'"), *OccluderCategory);
				continue;
			}
		}

		float CurrentRadius = GetBoundsRadiusFromMetadata(Metadata) + SafetyMargin;

		bool bSpawned = false;
		for (int32 Attempt = 0; Attempt < MaxLocationAttempts; Attempt++)
		{
			float Alpha = FMath::RandRange(0.3f, 0.7f);
			FVector CandidatePosition = FMath::Lerp(CameraPosition, ForegroundPosition, Alpha);

			FVector Direction = (ForegroundPosition - CameraPosition).GetSafeNormal();
			FVector Perpendicular = FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal();
			float LateralOffset = FMath::RandRange(-200.0f, 200.0f);
			CandidatePosition += Perpendicular * LateralOffset;

			// CandidatePosition.Z = ForegroundPosition.Z + FMath::RandRange(0.0f, 120.0f);
			CandidatePosition.Z = GetLandHeight(World, CandidatePosition.X, CandidatePosition.Y, CandidatePosition.Z + 200.0f) + FMath::RandRange(0.0f, 50.0f);

			TArray<AActor*> IgnoreList;
			IgnoreList.Add(OutSceneHandle.ForegroundActor);
			for (AActor* ExistingOccluder : SpawnedOccluders)
			{
				IgnoreList.Add(ExistingOccluder);
			}
			auto DefaultPawn = GetFirstPersonPawn();
			if (IsValid(DefaultPawn))
			{
				IgnoreList.Add(DefaultPawn);
			}
			auto UnrealcvPawn = GetUnrealcvPawn(World);
			if (IsValid(UnrealcvPawn))
			{
				IgnoreList.Add(UnrealcvPawn);
			}

			if (!CheckCollisionAtLocation(World, CandidatePosition, CurrentRadius, IgnoreList) || Attempt == MaxLocationAttempts - 1)
			{
				FVector DirectionToForeground = (ForegroundPosition - CandidatePosition).GetSafeNormal();
				FRotator OccluderRotation = DirectionToForeground.Rotation();
				OccluderRotation.Roll = 0.0f;
				OccluderRotation.Pitch = 0.0f;
				// OccluderRotation.Yaw += -90.0f;

				// random +-5 degrees
				float CameraAngleOffset = FMath::RandRange(-5.0f, 5.0f);
				OccluderRotation.Yaw += CameraAngleOffset;

				AActor* Occluder = SpawnActorFromMetadata(World, Metadata, CandidatePosition, OccluderRotation);
				if (IsValid(Occluder))
				{
					// EnablePhysicsSettling(Occluder);
					SpawnedOccluders.Add(Occluder);
					OutSceneHandle.OccluderMetadataList.Add({Metadata});
					OutSceneHandle.OccluderCategory = OccluderCategory;
					bSpawned = true;
					break;
				}
			}
		}

		if (!bSpawned)
		{
			UE_LOG(LogUnrealCV, Warning, TEXT("USceneCompositionBPLib::SpawnRandomOccluders: Failed to find collision-free position after %d attempts"), MaxLocationAttempts);
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
	CameraRotation.Roll = 0.0f;

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

bool USceneCompositionBPLib::AddSafePointToScene(const FString& SceneName, FVector Location)
{
	FString JsonFilePath = GetSceneConfigFilePath();

	FString JsonFileContent;
	if (!FFileHelper::LoadFileToString(JsonFileContent, *JsonFilePath))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("AddSafePointToScene: Failed to read JSON file '%s'"), *JsonFilePath);
		return false;
	}

	TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonFileContent);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogUnrealCV, Error, TEXT("AddSafePointToScene: Failed to parse JSON from '%s'"), *JsonFilePath);
		return false;
	}

	TSharedPtr<FJsonObject> SceneConfig = nullptr;
	FString MatchedKey;
	for (const auto& Pair : JsonObject->Values)
	{
		if (Pair.Value->Type == EJson::Object)
		{
			if (Pair.Key.Find(SceneName) != INDEX_NONE || SceneName.Find(Pair.Key) != INDEX_NONE)
			{
				SceneConfig = Pair.Value->AsObject();
				MatchedKey = Pair.Key;
				break;
			}
		}
	}

	if (!SceneConfig.IsValid())
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("AddSafePointToScene: Scene '%s' not found in JSON, creating default config"), *SceneName);

		SceneConfig = MakeShared<FJsonObject>();
		SceneConfig->SetBoolField(TEXT("Enabled"), true);
		SceneConfig->SetNumberField(TEXT("XMax"), 3790.0);
		SceneConfig->SetNumberField(TEXT("YMax"), 1400.0);
		SceneConfig->SetNumberField(TEXT("XMin"), -3657.0);
		SceneConfig->SetNumberField(TEXT("YMin"), -2299.0);
		SceneConfig->SetNumberField(TEXT("GroundHeight"), 100.0);
		SceneConfig->SetStringField(TEXT("ForegroundPathSpec"), TEXT(""));
		SceneConfig->SetStringField(TEXT("ForegroundCategory"), TEXT("Foreground_Human"));
		SceneConfig->SetStringField(TEXT("ForegroundCategory1"), TEXT("Foreground_Human_SKM"));
		SceneConfig->SetStringField(TEXT("OccluderPathSpec"), TEXT(""));
		SceneConfig->SetStringField(TEXT("OccluderCategory"), TEXT("Occluder_All"));
		SceneConfig->SetNumberField(TEXT("OccluderCount"), 0.0);
		SceneConfig->SetNumberField(TEXT("CameraID"), 1);
		SceneConfig->SetField(TEXT("OutSceneHandle"), MakeShared<FJsonValueNull>());
		SceneConfig->SetBoolField(TEXT("bAutoPositionCamera"), true);
		SceneConfig->SetNumberField(TEXT("ForegroundYaw"), -1.0);

		JsonObject->SetObjectField(SceneName, SceneConfig);
		MatchedKey = SceneName;

		UE_LOG(LogUnrealCV, Log, TEXT("AddSafePointToScene: Created default config for scene '%s'"), *SceneName);
	}
	else
	{
		UE_LOG(LogUnrealCV, Log, TEXT("AddSafePointToScene: Matched scene config '%s' for input '%s'"), *MatchedKey, *SceneName);
	}

	TArray<TSharedPtr<FJsonValue>> SafePointsArray;
	if (SceneConfig->HasField(TEXT("SafePoints")))
	{
		const TArray<TSharedPtr<FJsonValue>>* ExistingArray = nullptr;
		if (SceneConfig->TryGetArrayField(TEXT("SafePoints"), ExistingArray))
		{
			SafePointsArray = *ExistingArray;
		}
	}

	TSharedPtr<FJsonObject> PositionObj = MakeShared<FJsonObject>();
	PositionObj->SetNumberField(TEXT("X"), Location.X);
	PositionObj->SetNumberField(TEXT("Y"), Location.Y);
	PositionObj->SetNumberField(TEXT("Z"), Location.Z);

	TSharedPtr<FJsonObject> NewPoint = MakeShared<FJsonObject>();
	NewPoint->SetObjectField(TEXT("Position"), PositionObj);

	SafePointsArray.Add(MakeShared<FJsonValueObject>(NewPoint));

	SceneConfig->SetArrayField(TEXT("SafePoints"), SafePointsArray);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	if (!FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("AddSafePointToScene: Failed to serialize JSON"));
		return false;
	}

	if (!FFileHelper::SaveStringToFile(OutputString, *JsonFilePath))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("AddSafePointToScene: Failed to write JSON file '%s'"), *JsonFilePath);
		return false;
	}

	UE_LOG(LogUnrealCV, Log, TEXT("AddSafePointToScene: Added safe point (%.2f, %.2f, %.2f) to scene '%s'"),
		Location.X, Location.Y, Location.Z, *SceneName);
	return true;
}

bool USceneCompositionBPLib::AddSafePointToCurrentScene(UObject* WorldContextObject, FVector Location)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("AddSafePointToCurrentScene: Invalid world context"));
		return false;
	}

	FString CurrentMapPath = World->GetMapName();
	FString CurrentMapName = FJsonConfigHelper::ExtractMapNameFromPath(CurrentMapPath);

	UE_LOG(LogUnrealCV, Log, TEXT("AddSafePointToCurrentScene: Current map name: %s"), *CurrentMapName);

	return AddSafePointToScene(CurrentMapName, Location);
}

TArray<FVector> USceneCompositionBPLib::GetSafePointsForScene(const FString& SceneName)
{
	TArray<FVector> SafePoints;
	FString JsonFilePath = GetSceneConfigFilePath();

	FString JsonFileContent;
	if (!FFileHelper::LoadFileToString(JsonFileContent, *JsonFilePath))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("GetSafePointsForScene: Failed to read JSON file '%s'"), *JsonFilePath);
		return SafePoints;
	}

	TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonFileContent);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogUnrealCV, Error, TEXT("GetSafePointsForScene: Failed to parse JSON from '%s'"), *JsonFilePath);
		return SafePoints;
	}

	TSharedPtr<FJsonObject> SceneConfig = nullptr;
	FString MatchedKey;
	for (const auto& Pair : JsonObject->Values)
	{
		if (Pair.Value->Type == EJson::Object)
		{
			if (Pair.Key.Find(SceneName) != INDEX_NONE || SceneName.Find(Pair.Key) != INDEX_NONE)
			{
				SceneConfig = Pair.Value->AsObject();
				MatchedKey = Pair.Key;
				break;
			}
		}
	}

	if (!SceneConfig.IsValid())
	{
		UE_LOG(LogUnrealCV, Error, TEXT("GetSafePointsForScene: Scene '%s' not found in JSON"), *SceneName);
		return SafePoints;
	}

	UE_LOG(LogUnrealCV, Log, TEXT("GetSafePointsForScene: Matched scene config '%s' for input '%s'"), *MatchedKey, *SceneName);

	if (SceneConfig->HasField(TEXT("SafePoints")))
	{
		const TArray<TSharedPtr<FJsonValue>>* SafePointsArray = nullptr;
		if (SceneConfig->TryGetArrayField(TEXT("SafePoints"), SafePointsArray))
		{
			for (const TSharedPtr<FJsonValue>& PointValue : *SafePointsArray)
			{
				if (PointValue->Type == EJson::Object)
				{
					TSharedPtr<FJsonObject> PointObj = PointValue->AsObject();

					if (PointObj->HasField(TEXT("Position")))
					{
						TSharedPtr<FJsonObject> PosObj = PointObj->GetObjectField(TEXT("Position"));
						double X = 0.0, Y = 0.0, Z = 0.0;

						TSharedPtr<FJsonValue> XVal = PosObj->TryGetField(TEXT("X"));
						TSharedPtr<FJsonValue> YVal = PosObj->TryGetField(TEXT("Y"));
						TSharedPtr<FJsonValue> ZVal = PosObj->TryGetField(TEXT("Z"));

						if (FJsonConfigHelper::ParseJsonNumber(XVal, X) &&
							FJsonConfigHelper::ParseJsonNumber(YVal, Y) &&
							FJsonConfigHelper::ParseJsonNumber(ZVal, Z))
						{
							SafePoints.Add(FVector(X, Y, Z));
						}
					}
					else
					{
						UE_LOG(LogUnrealCV, Error, TEXT("GetSafePointsForScene: Safe point missing 'Position' field"));
					}
				}
			}
		}
	}

	return SafePoints;
}

