#include "Controller/ProxyAnnotator.h"
#include "Runtime/Engine/Public/EngineUtils.h"
#include "Component/AnnotationComponent.h"
#include "UnrealcvServer.h"
#include "UnrealcvLog.h"
#include "Controller/ObjectAnnotator.h"

FProxyAnnotator::FProxyAnnotator()
{
	ColorGenerator = MakeShared<FColorGenerator>();
}

FProxyAnnotator::~FProxyAnnotator()
{
}

void FProxyAnnotator::AnnotateWorld(UWorld* World)
{
	if (!IsValid(World))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("[ProxyAnnotator] Cannot annotate world, the world is not valid"));
		return;
	}

	TArray<AActor*> ActorArray;
	GetAnnotableActors(World, ActorArray);

	UE_LOG(LogUnrealCV, Log, TEXT("[ProxyAnnotator] Annotating %d actors using AnnotationComponent"), ActorArray.Num());

	const int32 BatchSize = 1;
	int32 ProcessedCount = 0;

	for (int32 i = 0; i < ActorArray.Num(); ++i)
	{
		AActor* Actor = ActorArray[i];
		if (!IsValid(Actor))
		{
			UE_LOG(LogUnrealCV, Warning, TEXT("[ProxyAnnotator] Found invalid actor"));
			continue;
		}

		FColor AnnotationColor = GetDefaultColor(Actor);
		SetAnnotationColor(Actor, AnnotationColor);
		++ProcessedCount;

		if (ProcessedCount >= BatchSize && i < ActorArray.Num() - 1)
		{
			FlushRenderingCommands();
			ProcessedCount = 0;
		}
	}

	FlushRenderingCommands();

	UE_LOG(LogUnrealCV, Log, TEXT("[ProxyAnnotator] Annotated %d actors, %d colors generated"),
		ActorArray.Num(), AnnotationColors.Num());
}

void FProxyAnnotator::DeannotateWorld(UWorld* World)
{
	if (!IsValid(World))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("[ProxyAnnotator] Cannot deannotate world, the world is not valid"));
		return;
	}

	int32 DestroyedCount = 0;
	int32 ComponentCount = 0;

	for (TActorIterator<AActor> ActorItr(World); ActorItr; ++ActorItr)
	{
		AActor* Actor = *ActorItr;
		if (!IsValid(Actor))
		{
			continue;
		}

		TArray<UActorComponent*> AnnotationComponents = Actor->K2_GetComponentsByClass(UAnnotationComponent::StaticClass());
		ComponentCount += AnnotationComponents.Num();

		for (UActorComponent* Component : AnnotationComponents)
		{
			if (IsValid(Component))
			{
				UAnnotationComponent* AnnotationComponent = Cast<UAnnotationComponent>(Component);
				if (AnnotationComponent)
				{
					AnnotationComponent->DestroyComponent();
					++DestroyedCount;
				}
			}
		}
	}

	AnnotationColors.Empty();

	FlushRenderingCommands();

	UE_LOG(LogUnrealCV, Log, TEXT("[ProxyAnnotator] Destroyed %d annotation components out of %d found"),
		DestroyedCount, ComponentCount);
}

int32 FProxyAnnotator::SetAnnotationColor(AActor* Actor, const FColor& AnnotationColor)
{
	if (!IsValid(Actor))
	{
		return 0;
	}

	TArray<UActorComponent*> AnnotationComponents = Actor->K2_GetComponentsByClass(UAnnotationComponent::StaticClass());
	if (AnnotationComponents.Num() == 0)
	{
		CreateAnnotationComponent(Actor, AnnotationColor);
	}
	else
	{
		UpdateAnnotationComponent(Actor, AnnotationColor);
	}
	AnnotationColors.Emplace(Actor->GetName(), AnnotationColor);
	return AnnotationComponents.Num();
}

void FProxyAnnotator::GetAnnotationColor(AActor* Actor, FColor& AnnotationColor)
{
	if (!IsValid(Actor))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("[ProxyAnnotator] Actor is invalid in GetAnnotationColor"));
		return;
	}

	TArray<UActorComponent*> AnnotationComponents = Actor->K2_GetComponentsByClass(UAnnotationComponent::StaticClass());
	TArray<UActorComponent*> MeshComponents = Actor->K2_GetComponentsByClass(UMeshComponent::StaticClass());

	if (AnnotationComponents.Num() == 0) return;

	if (AnnotationComponents.Num() != MeshComponents.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ProxyAnnotator] In actor %s, MeshComponent count (%d) != AnnotationComponent count (%d)"),
			*Actor->GetName(), MeshComponents.Num(), AnnotationComponents.Num());
	}

	UAnnotationComponent* AnnotationComponent = Cast<UAnnotationComponent>(AnnotationComponents[0]);
	AnnotationColor = AnnotationComponent->GetAnnotationColor();
}

void FProxyAnnotator::GetAnnotableActors(UWorld* World, TArray<AActor*>& ActorArray)
{
	if (!IsValid(World))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("[ProxyAnnotator] World is invalid in GetAnnotableActors"));
		return;
	}

	for (TActorIterator<AActor> ActorItr(World); ActorItr; ++ActorItr)
	{
		AActor* Actor = *ActorItr;
		ActorArray.Add(Actor);
	}
}

void FProxyAnnotator::CreateAnnotationComponent(AActor* Actor, const FColor& AnnotationColor)
{
	if (!IsValid(Actor))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("[ProxyAnnotator] Invalid actor in CreateAnnotationComponent"));
		return;
	}

	TArray<UActorComponent*> AnnotationComponents = Actor->K2_GetComponentsByClass(UAnnotationComponent::StaticClass());
	if (AnnotationComponents.Num() != 0)
	{
		UE_LOG(LogUnrealCV, Log, TEXT("[ProxyAnnotator] Skip already annotated actor %s"), *Actor->GetName());
		return;
	}

	TArray<UActorComponent*> MeshComponents = Actor->K2_GetComponentsByClass(UMeshComponent::StaticClass());
	if (MeshComponents.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[ProxyAnnotator] Annotate actor %s with color %s, MeshComponents: %d"),
			*Actor->GetActorNameOrLabel(), *AnnotationColor.ToString(), MeshComponents.Num());

		for (UActorComponent* Component : MeshComponents)
		{
			bool DisableSKMAnnotation = FUnrealcvServer::Get().Config.DisableSKMAnnotation;
			if (DisableSKMAnnotation)
			{
				if (Component->IsA<USkeletalMeshComponent>())
				{
					UE_LOG(LogUnrealCV, Log, TEXT("[ProxyAnnotator] Skipping SkeletalMeshComponent for %s"),
						*Actor->GetName());
					continue;
				}
			}

			UMeshComponent* MeshComponent = Cast<UMeshComponent>(Component);
			check(MeshComponent)

			UE_LOG(LogUnrealCV, Log, TEXT("[ProxyAnnotator]   MeshComponent: %s, Class: %s"),
				*MeshComponent->GetName(), *MeshComponent->GetClass()->GetName());

			UAnnotationComponent* AnnotationComponent = NewObject<UAnnotationComponent>(MeshComponent);
			AnnotationComponent->SetupAttachment(MeshComponent);
			AnnotationComponent->SetAnnotationColor(AnnotationColor);
			AnnotationComponent->RegisterComponent();
			AnnotationComponent->MarkRenderStateDirty();
		}
	}
}

void FProxyAnnotator::UpdateAnnotationComponent(AActor* Actor, const FColor& AnnotationColor)
{
	if (!IsValid(Actor))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("[ProxyAnnotator] Invalid actor in UpdateAnnotationComponent"));
		return;
	}

	TArray<UActorComponent*> AnnotationComponents = Actor->K2_GetComponentsByClass(UAnnotationComponent::StaticClass());
	for (UActorComponent* Component : AnnotationComponents)
	{
		UAnnotationComponent* AnnotationComponent = Cast<UAnnotationComponent>(Component);
		AnnotationComponent->SetAnnotationColor(AnnotationColor);
		AnnotationComponent->MarkRenderStateDirty();
	}
}

FColor FProxyAnnotator::GetDefaultColor(AActor* Actor)
{
	FString ActorName = Actor->GetName();
	if (AnnotationColors.Contains(ActorName))
	{
		return AnnotationColors[ActorName];
	}

	int ColorIndex = AnnotationColors.Num();
	FColor AnnotationColor = ColorGenerator->GetColorFromColorMap(ColorIndex);

	return AnnotationColor;
}
