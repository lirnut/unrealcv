#include "SpawnBPLib.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Blueprint.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "UnrealcvLog.h"

static AActor* TrySpawnFromClass(UWorld* World, const FString& ClassPath, const FVector& Location, const FRotator& Rotation)
{
	UClass* LoadedClass = StaticLoadClass(AActor::StaticClass(), nullptr, *ClassPath);
	if (!IsValid(LoadedClass))
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* SpawnedActor = World->SpawnActor<AActor>(LoadedClass, Location, Rotation, SpawnParams);
	if (IsValid(SpawnedActor))
	{
		UE_LOG(LogUnrealCV, Log, TEXT("SpawnActorFromPath: Spawned from StaticLoadClass '%s'"), *ClassPath);
		return SpawnedActor;
	}

	return nullptr;
}

static AActor* TrySpawnFromLoadObject_Blueprint(UWorld* World, const FString& AssetPath, const FVector& Location, const FRotator& Rotation)
{
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
	if (!IsValid(Blueprint) || !Blueprint->GeneratedClass || !Blueprint->GeneratedClass->IsChildOf(AActor::StaticClass()))
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* SpawnedActor = World->SpawnActor<AActor>(Blueprint->GeneratedClass, Location, Rotation, SpawnParams);
	if (IsValid(SpawnedActor))
	{
		UE_LOG(LogUnrealCV, Log, TEXT("SpawnActorFromPath: Spawned from LoadObject<UBlueprint> '%s'"), *AssetPath);
		return SpawnedActor;
	}

	return nullptr;
}

static AActor* TrySpawnFromLoadObject_StaticMesh(UWorld* World, const FString& AssetPath, const FVector& Location, const FRotator& Rotation)
{
	UStaticMesh* StaticMesh = LoadObject<UStaticMesh>(nullptr, *AssetPath);
	if (!IsValid(StaticMesh))
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AStaticMeshActor* MeshActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Location, Rotation, SpawnParams);
	if (!IsValid(MeshActor))
	{
		return nullptr;
	}

	UStaticMeshComponent* MeshComponent = MeshActor->GetStaticMeshComponent();
	if (MeshComponent)
	{
		MeshComponent->SetMobility(EComponentMobility::Movable);
		MeshComponent->SetStaticMesh(StaticMesh);
	}

	UE_LOG(LogUnrealCV, Log, TEXT("SpawnActorFromPath: Spawned from LoadObject<UStaticMesh> '%s'"), *AssetPath);
	return MeshActor;
}

static AActor* TrySpawnFromLoadObject_SkeletalMesh(UWorld* World, const FString& AssetPath, const FVector& Location, const FRotator& Rotation)
{
	USkeletalMesh* SkeletalMesh = LoadObject<USkeletalMesh>(nullptr, *AssetPath);
	if (!IsValid(SkeletalMesh))
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* SkeletalActor = World->SpawnActor<AActor>(AActor::StaticClass(), Location, Rotation, SpawnParams);
	if (!IsValid(SkeletalActor))
	{
		return nullptr;
	}

	USkeletalMeshComponent* SkeletalComponent = NewObject<USkeletalMeshComponent>(SkeletalActor);
	if (SkeletalComponent)
	{
		SkeletalComponent->SetMobility(EComponentMobility::Movable);
		SkeletalComponent->SetSkeletalMesh(SkeletalMesh);
		SkeletalComponent->RegisterComponent();
		SkeletalComponent->AttachToComponent(SkeletalActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	}

	UE_LOG(LogUnrealCV, Log, TEXT("SpawnActorFromPath: Spawned from LoadObject<USkeletalMesh> '%s'"), *AssetPath);
	return SkeletalActor;
}

static AActor* TrySpawnFromStaticLoadObject_StaticMesh(UWorld* World, const FString& AssetPath, const FVector& Location, const FRotator& Rotation)
{
	UStaticMesh* StaticMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *AssetPath));
	if (!IsValid(StaticMesh))
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AStaticMeshActor* MeshActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Location, Rotation, SpawnParams);
	if (!IsValid(MeshActor))
	{
		return nullptr;
	}

	UStaticMeshComponent* MeshComponent = MeshActor->GetStaticMeshComponent();
	if (MeshComponent)
	{
		MeshComponent->SetMobility(EComponentMobility::Movable);
		MeshComponent->SetStaticMesh(StaticMesh);
	}

	UE_LOG(LogUnrealCV, Log, TEXT("SpawnActorFromPath: Spawned from StaticLoadObject<UStaticMesh> '%s'"), *AssetPath);
	return MeshActor;
}

static AActor* TrySpawnFromStaticLoadObject_SkeletalMesh(UWorld* World, const FString& AssetPath, const FVector& Location, const FRotator& Rotation)
{
	USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(StaticLoadObject(USkeletalMesh::StaticClass(), nullptr, *AssetPath));
	if (!IsValid(SkeletalMesh))
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* SkeletalActor = World->SpawnActor<AActor>(AActor::StaticClass(), Location, Rotation, SpawnParams);
	if (!IsValid(SkeletalActor))
	{
		return nullptr;
	}

	USkeletalMeshComponent* SkeletalComponent = NewObject<USkeletalMeshComponent>(SkeletalActor);
	if (SkeletalComponent)
	{
		SkeletalComponent->SetMobility(EComponentMobility::Movable);
		SkeletalComponent->SetSkeletalMesh(SkeletalMesh);
		SkeletalComponent->RegisterComponent();
		SkeletalComponent->AttachToComponent(SkeletalActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	}

	UE_LOG(LogUnrealCV, Log, TEXT("SpawnActorFromPath: Spawned from StaticLoadObject<USkeletalMesh> '%s'"), *AssetPath);
	return SkeletalActor;
}

AActor* USpawnBPLib::SpawnActorFromPath(
	UObject* WorldContextObject,
	const FString& AssetPath,
	FVector Location,
	FRotator Rotation)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("SpawnActorFromPath: Invalid world context"));
		return nullptr;
	}

	AActor* SpawnedActor = nullptr;

	SpawnedActor = TrySpawnFromClass(World, AssetPath, Location, Rotation);
	if (IsValid(SpawnedActor))
	{
		return SpawnedActor;
	}

	if (!AssetPath.EndsWith(TEXT("_C")))
	{
		FString ClassPathWithSuffix = AssetPath + TEXT("_C");
		SpawnedActor = TrySpawnFromClass(World, ClassPathWithSuffix, Location, Rotation);
		if (IsValid(SpawnedActor))
		{
			return SpawnedActor;
		}
	}

	SpawnedActor = TrySpawnFromLoadObject_Blueprint(World, AssetPath, Location, Rotation);
	if (IsValid(SpawnedActor))
	{
		return SpawnedActor;
	}

	SpawnedActor = TrySpawnFromLoadObject_StaticMesh(World, AssetPath, Location, Rotation);
	if (IsValid(SpawnedActor))
	{
		return SpawnedActor;
	}

	SpawnedActor = TrySpawnFromLoadObject_SkeletalMesh(World, AssetPath, Location, Rotation);
	if (IsValid(SpawnedActor))
	{
		return SpawnedActor;
	}

	SpawnedActor = TrySpawnFromStaticLoadObject_StaticMesh(World, AssetPath, Location, Rotation);
	if (IsValid(SpawnedActor))
	{
		return SpawnedActor;
	}

	SpawnedActor = TrySpawnFromStaticLoadObject_SkeletalMesh(World, AssetPath, Location, Rotation);
	if (IsValid(SpawnedActor))
	{
		return SpawnedActor;
	}

	UE_LOG(LogUnrealCV, Error, TEXT("SpawnActorFromPath: Failed to load and spawn asset from '%s'"), *AssetPath);
	return nullptr;
}
