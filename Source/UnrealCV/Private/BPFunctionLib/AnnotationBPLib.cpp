#include "AnnotationBPLib.h"
#include "UnrealcvServer.h"
#include "UnrealcvLog.h"
#include "Component/AnnotationComponent.h"
#include "Runtime/CoreUObject/Public/UObject/UObjectHash.h"
#include "Runtime/Core/Public/Async/ParallelFor.h"

TMap<UWorld*, TArray<TWeakObjectPtr<UPrimitiveComponent>>> UAnnotationBPLib::CachedAnnotationComponents;
TMap<UWorld*, int32> UAnnotationBPLib::CachedWorldFrameNumbers;
bool UAnnotationBPLib::bCacheEnabled = false;

void UAnnotationBPLib::AnnotateActor(AActor* Actor)
{
	if (!IsValid(Actor))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("AnnotateActor: Actor is invalid"));
		return;
	}

	TWeakObjectPtr<AUnrealcvWorldController> WorldController = FUnrealcvServer::Get().WorldController;

	if (!WorldController.IsValid())
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("AnnotateActor: WorldController is not available, this func will not work withoud game world"));
		return;
	}

	FColor AnnotationColor;
	FObjectAnnotator::GetAnnotationColor(Actor, AnnotationColor);
	FObjectAnnotator::SetAnnotationColor(Actor, AnnotationColor);
	FlushRenderingCommands();

	UE_LOG(LogUnrealCV, Log, TEXT("AnnotateActor: Annotated %s with color %s"),
		*Actor->GetName(), *AnnotationColor.ToString());
}

void UAnnotationBPLib::AnnotateWorld()
{
	TWeakObjectPtr<AUnrealcvWorldController> WorldController = FUnrealcvServer::Get().WorldController;

	if (!WorldController.IsValid() || IsValid(FUnrealcvServer::Get().GetWorld()))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("AnnotateActor: WorldController is not available"));
		if (!GEditor)
		{
			UE_LOG(LogUnrealCV, Error, TEXT("AnnotateActor: !GEditor"));
			return;
		}

		const FWorldContext& WorldContext = GEditor->GetEditorWorldContext();
		FObjectAnnotator::AnnotateWorld(WorldContext.World());
	}
	else
	{
		FObjectAnnotator::AnnotateWorld(FUnrealcvServer::Get().GetWorld());
	}
	UE_LOG(LogUnrealCV, Log, TEXT("AnnotateWorld: World annotation completed"));
}

void UAnnotationBPLib::GetAnnotationComponents(UWorld* World, TArray<TWeakObjectPtr<UPrimitiveComponent>>& OutComponentList)
{
	if (!IsValid(World))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("Can not get AnnotationComponents, World is invalid"));
		return;
	}

	if (bCacheEnabled && CachedAnnotationComponents.Contains(World))
	{
		int32 CachedFrameNumber = CachedWorldFrameNumbers.FindRef(World);
		int32 CurrentFrameNumber = World->GetTimeSeconds() * 60;

		if (CurrentFrameNumber - CachedFrameNumber < 120)
		{
			OutComponentList = CachedAnnotationComponents.FindRef(World);

			TArray<TWeakObjectPtr<UPrimitiveComponent>> ValidComponents;
			for (const TWeakObjectPtr<UPrimitiveComponent>& WeakComponent : OutComponentList)
			{
				if (WeakComponent.IsValid())
				{
					ValidComponents.Add(WeakComponent);
				}
			}

			if (ValidComponents.Num() != OutComponentList.Num())
			{
				CachedAnnotationComponents.Add(World, ValidComponents);
				OutComponentList = ValidComponents;
			}

			return;
		}
	}

	TArray<UObject*> UObjectList;
	bool bIncludeDerivedClasses = false;
	EObjectFlags ExclusionFlags = EObjectFlags::RF_ClassDefaultObject;

	GetObjectsOfClass(UAnnotationComponent::StaticClass(), UObjectList, bIncludeDerivedClasses, ExclusionFlags);

	TArray<TArray<UPrimitiveComponent*>> TempComponentLists;
	TempComponentLists.SetNum(UObjectList.Num());

	for (int32 i = 0; i < UObjectList.Num(); ++i)
	{
		TempComponentLists[i].Empty();
	}

	ParallelFor(UObjectList.Num(), [&](int32 Index)
	{
		UObject* Object = UObjectList[Index];
		UPrimitiveComponent* Component = Cast<UPrimitiveComponent>(Object);

		if (Component && Component->GetWorld() == World)
		{
			TempComponentLists[Index].Add(Component);
		}
	});

	OutComponentList.Empty();

	for (const TArray<UPrimitiveComponent*>& TempComponentList : TempComponentLists)
	{
		for (UPrimitiveComponent* Component : TempComponentList)
		{
			TWeakObjectPtr<UPrimitiveComponent> WeakComponent = Component;
			OutComponentList.Add(WeakComponent);
		}
	}

	if (bCacheEnabled)
	{
		int32 CurrentFrameNumber = World->GetTimeSeconds() * 60;
		CachedAnnotationComponents.Add(World, OutComponentList);
		CachedWorldFrameNumbers.Add(World, CurrentFrameNumber);
	}

	if (OutComponentList.Num() == 0)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("No annotation in the scene to show, fall back to lit mode"));
	}
}

void UAnnotationBPLib::SetAnnotationCacheEnabled(bool bEnabled)
{
	bCacheEnabled = bEnabled;
	if (!bEnabled)
	{
		ClearAnnotationCache();
	}
}

void UAnnotationBPLib::ClearAnnotationCache()
{
	CachedAnnotationComponents.Empty();
	CachedWorldFrameNumbers.Empty();
}
