#include "MaterialBPLib.h"
#include "EngineUtils.h"
#include "Materials/Material.h"
#include "UnrealcvLog.h"
#include "UnrealcvServer.h"

TArray<UMaterialBPLib::FMaterialBackup> UMaterialBPLib::MaterialBackups;
TArray<TWeakObjectPtr<AActor>> UMaterialBPLib::CachedActors;
UMaterial* UMaterialBPLib::OpaqueMaterial = nullptr;

void UMaterialBPLib::EnsureOpaqueMaterialLoaded()
{
	if (!IsValid(OpaqueMaterial))
	{
		OpaqueMaterial = Cast<UMaterial>(StaticLoadObject(
			UMaterial::StaticClass(),
			nullptr,
			TEXT("Material'/UnrealCV/OpaqueWhiteMaterial.OpaqueWhiteMaterial'")
		));

		if (!IsValid(OpaqueMaterial))
		{
			UE_LOG(LogUnrealCV, Error, TEXT("Failed to load OpaqueMaterial"));
		}
	}
}

void UMaterialBPLib::CacheSceneActors(UWorld* World)
{
	if (!IsValid(World))
	{
		World = FUnrealcvServer::Get().GetWorld();
		if (!IsValid(World))
		{
			UE_LOG(LogUnrealCV, Error, TEXT("CacheSceneActors: Invalid world"));
			return;
		}
	}

	CachedActors.Empty();
	for (TActorIterator<AActor> ActorItr(World); ActorItr; ++ActorItr)
	{
		AActor* Actor = *ActorItr;
		if (IsValid(Actor))
		{
			CachedActors.Add(Actor);
		}
	}

	UE_LOG(LogUnrealCV, Log, TEXT("CacheSceneActors: Cached %d actors"), CachedActors.Num());
}

void UMaterialBPLib::ClearCache()
{
	CachedActors.Empty();
	UE_LOG(LogUnrealCV, Log, TEXT("ClearCache: Cleared actor cache"));
}

void UMaterialBPLib::ReplaceActorMaterials(AActor* Actor)
{
	if (!IsValid(Actor) || !IsValid(OpaqueMaterial))
	{
		return;
	}

	TArray<UActorComponent*> Components;
	Actor->GetComponents(Components);

	for (UActorComponent* Component : Components)
	{
		UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Component);
		if (!IsValid(PrimComp))
		{
			continue;
		}

		int32 NumMaterials = PrimComp->GetNumMaterials();
		if (NumMaterials == 0)
		{
			continue;
		}

		FMaterialBackup Backup;
		Backup.Component = PrimComp;
		Backup.OriginalMaterials.Reserve(NumMaterials);

		for (int32 i = 0; i < NumMaterials; ++i)
		{
			UMaterialInterface* OriginalMat = PrimComp->GetMaterial(i);
			Backup.OriginalMaterials.Add(OriginalMat);
			PrimComp->SetMaterial(i, OpaqueMaterial);
		}

		MaterialBackups.Add(Backup);
	}
}

void UMaterialBPLib::ShowOnlyActorMaterial(AActor* TargetActor, UWorld* World)
{
	UE_LOG(LogUnrealCV, Log, TEXT("ShowOnlyActorMaterial: TargetActor=%s"), *GetNameSafe(TargetActor));
	if (!IsValid(TargetActor))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("ShowOnlyActorMaterial: Invalid target actor"));
		return;
	}

	if (!IsValid(World))
	{
		World = FUnrealcvServer::Get().GetWorld();
		if (!IsValid(World))
		{
			UE_LOG(LogUnrealCV, Error, TEXT("ShowOnlyActorMaterial: Invalid world"));
			return;
		}
	}

	RestoreAllActorMaterials();
	EnsureOpaqueMaterialLoaded();

	if (!IsValid(OpaqueMaterial))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("ShowOnlyActorMaterial: OpaqueMaterial is not valid"));
		return;
	}

	if (CachedActors.Num() > 0)
	{
		for (TWeakObjectPtr<AActor>& ActorPtr : CachedActors)
		{
			if (!ActorPtr.IsValid() || ActorPtr.Get() == TargetActor)
			{
				continue;
			}
			ReplaceActorMaterials(ActorPtr.Get());
		}
	}
	else
	{
		for (TActorIterator<AActor> ActorItr(World); ActorItr; ++ActorItr)
		{
			AActor* Actor = *ActorItr;
			if (!IsValid(Actor) || Actor == TargetActor)
			{
				continue;
			}
			ReplaceActorMaterials(Actor);
		}
	}

	UE_LOG(LogUnrealCV, Log, TEXT("ShowOnlyActorMaterial: Replaced materials for %d components"), MaterialBackups.Num());
}

void UMaterialBPLib::ShowOnlyActorsMaterial(const TArray<AActor*>& TargetActors, UWorld* World)
{
	if (TargetActors.Num() == 0)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("ShowOnlyActorsMaterial: Empty target actors array"));
		return;
	}

	if (!IsValid(World))
	{
		World = FUnrealcvServer::Get().GetWorld();
		if (!IsValid(World))
		{
			UE_LOG(LogUnrealCV, Error, TEXT("ShowOnlyActorsMaterial: Invalid world"));
			return;
		}
	}

	RestoreAllActorMaterials();
	EnsureOpaqueMaterialLoaded();

	if (!IsValid(OpaqueMaterial))
	{
		return;
	}

	TSet<AActor*> TargetSet(TargetActors);

	if (CachedActors.Num() > 0)
	{
		for (TWeakObjectPtr<AActor>& ActorPtr : CachedActors)
		{
			if (!ActorPtr.IsValid() || TargetSet.Contains(ActorPtr.Get()))
			{
				continue;
			}
			ReplaceActorMaterials(ActorPtr.Get());
		}
	}
	else
	{
		for (TActorIterator<AActor> ActorItr(World); ActorItr; ++ActorItr)
		{
			AActor* Actor = *ActorItr;
			if (!IsValid(Actor) || TargetSet.Contains(Actor))
			{
				continue;
			}
			ReplaceActorMaterials(Actor);
		}
	}

	UE_LOG(LogUnrealCV, Log, TEXT("ShowOnlyActorsMaterial: Replaced materials for %d components"), MaterialBackups.Num());
}

void UMaterialBPLib::RestoreAllActorMaterials()
{
	for (const FMaterialBackup& Backup : MaterialBackups)
	{
		if (!Backup.Component.IsValid())
		{
			continue;
		}

		UPrimitiveComponent* PrimComp = Backup.Component.Get();
		for (int32 i = 0; i < Backup.OriginalMaterials.Num(); ++i)
		{
			PrimComp->SetMaterial(i, Backup.OriginalMaterials[i]);
		}
	}

	MaterialBackups.Empty();
	UE_LOG(LogUnrealCV, Log, TEXT("RestoreAllActorMaterials: Restored all materials"));
}
