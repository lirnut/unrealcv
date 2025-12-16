// Weichao Qiu @ 2018
#include "UObjectUtils.h"
#include "Runtime/Engine/Public/EngineUtils.h"
#include "Runtime/CoreUObject/Public/UObject/UObjectIterator.h"


AActor* GetActorById(UWorld* World, FString ActorId)
{
    using FCacheKey = TPair<UWorld*, FName>;

    // 静态缓存表：Key = (World, ActorName)，Value = WeakObjectPtr<AActor>
    static TMap<FCacheKey, TWeakObjectPtr<AActor>> ActorCache;

    FName TargetName(*ActorId);
    FCacheKey Key(World, TargetName);

    if (TWeakObjectPtr<AActor>* CachedPtr = ActorCache.Find(Key))
    {
        AActor* CachedActor = CachedPtr->Get();

        if (CachedActor && CachedActor->IsValidLowLevelFast() && CachedActor->GetFName() == TargetName)
        {
            return CachedActor;
        }

        ActorCache.Remove(Key);
    }

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;

        if (Actor->GetFName() == TargetName)
        {
            ActorCache.Add(Key, Actor);
            return Actor;
        }
    }

    return nullptr;
}


// AActor* GetActorById(UWorld* World, FString ActorId)
// {
// 	for (TActorIterator<AActor> ActorItr(World); ActorItr; ++ActorItr)
// 	{
// 		AActor* Actor = *ActorItr;
// 		if (Actor->GetWorld() == World && Actor->GetName() == ActorId)
// 		{
// 			return Actor;
// 		}
// 	}
// 	return nullptr;
// }

UObject* GetObjectByIdNaive(UWorld* World, FString ObjectId)
{
	for (TObjectIterator<UObject> ObjItr; ObjItr; ++ObjItr)
	{
		UObject* Obj = *ObjItr;
		if (Obj->GetWorld() == World && Obj->GetName() == ObjectId)
		{
			return Obj;
		}
	}
	return nullptr;
}

UObject* GetObjectById(UWorld* World, FString ObjectId)
{
    double time = FPlatformTime::Seconds();

    FName TargetName(*ObjectId);

    // 1. Try actor first
    if (AActor* A = GetActorById(World, ObjectId))
	{

    UE_LOG(LogTemp, Warning, TEXT("GetObjectById %s cost %f"),
           *ObjectId, FPlatformTime::Seconds() - time);
        return A;
	}

    UE_LOG(LogTemp, Warning, TEXT("GetObjectById Failx"));

    auto tmp = GetObjectByIdNaive(World, ObjectId);

    UE_LOG(LogTemp, Warning, TEXT("GetObjectById %s cost %f"),
           *ObjectId, FPlatformTime::Seconds() - time);
    return tmp;
}


// UObject* SearchWorldObjects(UWorld* World, FName TargetName)
// {
//     for (ULevel* Level : World->GetLevels())
//     {
//         for (AActor* Actor : Level->Actors)
//         {
//             if (!Actor) continue;

//             if (Actor->GetFName() == TargetName)
//                 return Actor;

//             TInlineComponentArray<UActorComponent*> Components(Actor);
//             for (UActorComponent* Comp : Components)
//             {
//                 if (Comp && Comp->GetFName() == TargetName)
//                     return Comp;
//             }
//         }
//     }
//     return nullptr;
// }

