#include "AnimationBPLib.h"
#include "Runtime/CoreUObject/Public/UObject/UObjectIterator.h"
#include "Animation/AnimationAsset.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Blueprint.h"

FString UAnimationBPLib::GetAnimationName(USkeletalMeshComponent* SkeletalMeshComponent)
{
	FString AnimationName;
	return AnimationName;
}

// From: https://answers.unrealengine.com/questions/263204/getting-the-animations-list-from-persona.html
TArray<UAnimationAsset *> UAnimationBPLib::GetAllAnimationOfSkeleton(USkeleton *Skeleton)
{
	TArray<UAnimationAsset *> AnimsArray;

	if (Skeleton == NULL)
	{
		return AnimsArray;
	}

	for (TObjectIterator<UAnimationAsset> Itr; Itr; ++Itr)
	{
		if (Skeleton->GetWorld() != (*Itr)->GetWorld())
		{
			continue;
		}

		if (Skeleton == (*Itr)->GetSkeleton())
		{
			AnimsArray.Add(*Itr);
		}
	}
	return AnimsArray;
}

TArray<UAnimSequence *> UAnimationBPLib::GetAllAnimationSequenceOfSkeleton(USkeleton *Skeleton)
{
	TArray<UAnimSequence*> AnimsArray;

	if (Skeleton == NULL)
	{
		return AnimsArray;
	}

	for (TObjectIterator<UAnimSequence> Itr; Itr; ++Itr)
	{
		if (Skeleton->GetWorld() != (*Itr)->GetWorld())
		{
			continue;
		}

		if (Skeleton == (*Itr)->GetSkeleton())
		{
			AnimsArray.Add(*Itr);
		}
	}
	return AnimsArray;
}

UAnimInstance* UAnimationBPLib::GetCurrentAnimInstance(USkeletalMeshComponent* SkeletalMeshComponent)
{
	if (!IsValid(SkeletalMeshComponent))
	{
		return nullptr;
	}
	return SkeletalMeshComponent->GetAnimInstance();
}

bool UAnimationBPLib::SetSkeletalMeshAnimationBlueprint(USkeletalMeshComponent* SkeletalMeshComponent, FString AnimBlueprintPath)
{
	if (!IsValid(SkeletalMeshComponent))
	{
		return false;
	}

	if (AnimBlueprintPath.IsEmpty())
	{
		return false;
	}

	UClass* AnimBPClass = LoadObject<UClass>(nullptr, *AnimBlueprintPath);
	if (!IsValid(AnimBPClass))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load Animation Blueprint class: %s"), *AnimBlueprintPath);
		return false;
	}

	SkeletalMeshComponent->SetAnimInstanceClass(AnimBPClass);
	return true;
}

namespace
{
	USkeletalMeshComponent* FindSkeletalMeshComponentRecursive(UActorComponent* Component)
	{
		if (!Component)
		{
			return nullptr;
		}

		USkeletalMeshComponent* SKM = Cast<USkeletalMeshComponent>(Component);
		if (SKM)
		{
			return SKM;
		}

		USceneComponent* SceneComp = Cast<USceneComponent>(Component);
		if (SceneComp)
		{
			for (UActorComponent* Child : SceneComp->GetAttachChildren())
			{
				USkeletalMeshComponent* FoundSKM = FindSkeletalMeshComponentRecursive(Child);
				if (FoundSKM)
				{
					return FoundSKM;
				}
			}
		}

		return nullptr;
	}

	USkeletalMeshComponent* FindSkeletalMeshComponentInActor(AActor* Actor)
	{
		if (!Actor)
		{
			return nullptr;
		}

		USkeletalMeshComponent* SKM = Actor->FindComponentByClass<USkeletalMeshComponent>();
		if (SKM)
		{
			return SKM;
		}

		TArray<USkeletalMeshComponent*> Components;
		Actor->GetComponents<USkeletalMeshComponent>(Components);
		if (Components.Num() > 0)
		{
			return Components[0];
		}

		for (UActorComponent* Component : Actor->GetComponents())
		{
			USkeletalMeshComponent* FoundSKM = FindSkeletalMeshComponentRecursive(Component);
			if (FoundSKM)
			{
				return FoundSKM;
			}
		}

		return nullptr;
	}
}

bool UAnimationBPLib::SetActorAnimationBlueprint(AActor* Actor, FString AnimBlueprintPath)
{
	if (!IsValid(Actor))
	{
		return false;
	}

	USkeletalMeshComponent* SkeletalMeshComponent = FindSkeletalMeshComponentInActor(Actor);
	if (!IsValid(SkeletalMeshComponent))
	{
		return false;
	}

	return SetSkeletalMeshAnimationBlueprint(SkeletalMeshComponent, AnimBlueprintPath);
}
