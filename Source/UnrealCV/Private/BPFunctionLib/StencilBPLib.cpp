#include "StencilBPLib.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GroomComponent.h"
#include "UnrealcvLog.h"
#include "GameFramework/DefaultPawn.h"
#include "EngineUtils.h"

// TArray<UStencilBPLib::FStencilBackup> UStencilBPLib::StencilBackups;

void UStencilBPLib::EnableCustomDepthForActor(AActor* TargetActor, int32 StencilValue)
{
	if (!IsValid(TargetActor))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("EnableCustomDepthForActor: Invalid actor"));
		return;
	}

	TArray<UActorComponent*> Components;
	TargetActor->GetComponents(Components);

	for (UActorComponent* Component : Components)
	{
		UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Component);
		if (!IsValid(PrimComp))
		{
			continue;
		}

		// FStencilBackup Backup;
		// Backup.Component = PrimComp;
		// Backup.bOriginalRenderCustomDepth = PrimComp->bRenderCustomDepth;
		// Backup.OriginalStencilValue = PrimComp->CustomDepthStencilValue;

		PrimComp->SetRenderCustomDepth(true);
		PrimComp->SetCustomDepthStencilValue(StencilValue);

		// StencilBackups.Add(Backup);

		UE_LOG(LogUnrealCV, Log, TEXT("EnableCustomDepth: %s (Stencil=%d)"),
			*PrimComp->GetName(), StencilValue);
	}

	UE_LOG(LogUnrealCV, Log, TEXT("EnableCustomDepthForActor: Modified %d components for %s"),
		Components.Num(), *TargetActor->GetName());
}

void UStencilBPLib::DisableCustomDepthForActor(AActor* TargetActor)
{
	if (!IsValid(TargetActor))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("DisableCustomDepthForActor: Invalid actor"));
		return;
	}

	TArray<UActorComponent*> Components;
	TargetActor->GetComponents(Components);

	for (UActorComponent* Component : Components)
	{
		UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Component);
		if (!IsValid(PrimComp))
		{
			continue;
		}

		PrimComp->SetRenderCustomDepth(false);
		// for (int32 i = StencilBackups.Num() - 1; i >= 0; --i)
		// {
		// 	if (StencilBackups[i].Component == PrimComp)
		// 	{
		// 		PrimComp->SetRenderCustomDepth(StencilBackups[i].bOriginalRenderCustomDepth);
		// 		PrimComp->SetCustomDepthStencilValue(StencilBackups[i].OriginalStencilValue);
		// 		StencilBackups.RemoveAt(i);
		// 		break;
		// 	}
		// }
	}

	UE_LOG(LogUnrealCV, Log, TEXT("DisableCustomDepthForActor: Restored components for %s"),
		*TargetActor->GetName());
}

void UStencilBPLib::EnableCustomDepthForActors(const TArray<AActor*>& TargetActors, int32 StencilValue)
{
	for (AActor* Actor : TargetActors)
	{
		EnableCustomDepthForActor(Actor, StencilValue);
	}
}

void UStencilBPLib::DisableCustomDepthForActors(const TArray<AActor*>& TargetActors)
{
	for (AActor* Actor : TargetActors)
	{
		DisableCustomDepthForActor(Actor);
	}
}

void UStencilBPLib::SetCustomDepthStencilValue(AActor* TargetActor, int32 StencilValue)
{
	if (!IsValid(TargetActor))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("SetCustomDepthStencilValue: Invalid actor"));
		return;
	}

	TArray<UActorComponent*> Components;
	TargetActor->GetComponents(Components);

	for (UActorComponent* Component : Components)
	{
		UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Component);
		if (IsValid(PrimComp))
		{
			PrimComp->SetCustomDepthStencilValue(StencilValue);
		}
	}

	UE_LOG(LogUnrealCV, Log, TEXT("SetCustomDepthStencilValue: Set stencil=%d for %s"),
		StencilValue, *TargetActor->GetName());
}

int32 UStencilBPLib::GetCustomDepthStencilValue(AActor* TargetActor)
{
	if (!IsValid(TargetActor))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("GetCustomDepthStencilValue: Invalid actor"));
		return 0;
	}

	TArray<UActorComponent*> Components;
	TargetActor->GetComponents(Components);

	for (UActorComponent* Component : Components)
	{
		UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Component);
		if (IsValid(PrimComp))
		{
			return PrimComp->CustomDepthStencilValue;
		}
	}

	return 0;
}

void UStencilBPLib::EnableCustomDepthForAllActors(int32 StencilValue)
{
	UWorld* World = GEngine->GetCurrentPlayWorld();
	if (!IsValid(World))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("EnableCustomDepthForAllActors: Invalid world"));
		return;
	}

	int32 ModifiedCount = 0;
	int32 SkippedCount = 0;

	for (TActorIterator<AActor> ActorItr(World); ActorItr; ++ActorItr)
	{
		AActor* Actor = *ActorItr;
		if (!IsValid(Actor))
		{
			continue;
		}

		if (Actor->IsA<ADefaultPawn>())
		{
			SkippedCount++;
			continue;
		}

		EnableCustomDepthForActor(Actor, StencilValue);
		ModifiedCount++;
	}

	UE_LOG(LogUnrealCV, Log, TEXT("EnableCustomDepthForAllActors: Modified %d actors, skipped %d DefaultPawn actors"),
		ModifiedCount, SkippedCount);
}

