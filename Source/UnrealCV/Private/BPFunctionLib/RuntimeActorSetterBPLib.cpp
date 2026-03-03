#include "RuntimeActorSetterBPLib.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "UnrealcvLog.h"

TArray<TWeakObjectPtr<AActor>> URuntimeActorSetterBPLib::PausedActors;
TArray<TWeakObjectPtr<UActorComponent>> URuntimeActorSetterBPLib::PausedComponents;

void URuntimeActorSetterBPLib::CollectPrimitiveComponentsRecursive(USceneComponent* SceneComponent, TArray<UPrimitiveComponent*>& OutComponents)
{
    if (!SceneComponent)
    {
        return;
    }

    UPrimitiveComponent* PrimitiveComp = Cast<UPrimitiveComponent>(SceneComponent);
    if (IsValid(PrimitiveComp))
    {
        OutComponents.Add(PrimitiveComp);
    }

    for (USceneComponent* Child : SceneComponent->GetAttachChildren())
    {
        CollectPrimitiveComponentsRecursive(Child, OutComponents);
    }
}

void URuntimeActorSetterBPLib::FindAllPrimitiveComponentsInActor(AActor* Actor, TArray<UPrimitiveComponent*>& OutComponents)
{
    if (!IsValid(Actor))
    {
        return;
    }

    TArray<UPrimitiveComponent*> DirectComponents;
    Actor->GetComponents<UPrimitiveComponent>(DirectComponents);
    for (UPrimitiveComponent* Comp : DirectComponents)
    {
        if (IsValid(Comp))
        {
            OutComponents.Add(Comp);
        }
    }

    if (Actor->GetRootComponent())
    {
        for (USceneComponent* Child : Actor->GetRootComponent()->GetAttachChildren())
        {
            CollectPrimitiveComponentsRecursive(Child, OutComponents);
        }
    }
}

bool URuntimeActorSetterBPLib::SetAffectDistanceFieldLighting(AActor* Actor, bool bAffect)
{
    TArray<UPrimitiveComponent*> Components;
    FindAllPrimitiveComponentsInActor(Actor, Components);

    if (Components.Num() == 0)
    {
        return false;
    }

    for (UPrimitiveComponent* Comp : Components)
    {
        if (IsValid(Comp))
        {
            Comp->SetAffectDistanceFieldLighting(bAffect);
        }
    }
    return true;
}

bool URuntimeActorSetterBPLib::SetCastShadow(AActor* Actor, bool bCastShadow)
{
    TArray<UPrimitiveComponent*> Components;
    FindAllPrimitiveComponentsInActor(Actor, Components);

    if (Components.Num() == 0)
    {
        return false;
    }

    for (UPrimitiveComponent* Comp : Components)
    {
        if (IsValid(Comp))
        {
            Comp->SetCastShadow(bCastShadow);
        }
    }
    return true;
}

bool URuntimeActorSetterBPLib::SetMovable(AActor* Actor, bool bMovable)
{
    if (!IsValid(Actor))
    {
        return false;
    }

    Actor->SetActorEnableCollision(bMovable);
    return true;
}

bool URuntimeActorSetterBPLib::SetTickableWhenPaused(AActor* Actor, bool bTickable)
{
    if (!IsValid(Actor))
    {
        return false;
    }

    Actor->SetTickableWhenPaused(bTickable);
    return true;
}

bool URuntimeActorSetterBPLib::GetActorComponents(AActor* Actor, TArray<UPrimitiveComponent*>& OutComponents)
{
    FindAllPrimitiveComponentsInActor(Actor, OutComponents);
    return OutComponents.Num() > 0;
}

void URuntimeActorSetterBPLib::PauseAllActorsExceptPawn(UObject* WorldContextObject)
{
    UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
    if (!World)
    {
        UE_LOG(LogUnrealCV, Error, TEXT("PauseAllActorsExceptPawn: Invalid world context"));
        return;
    }

    APlayerController* PlayerController = World->GetFirstPlayerController();
    APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;

    PausedActors.Empty();
    PausedComponents.Empty();

    for (FActorIterator It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!IsValid(Actor))
        {
            continue;
        }

        if (Actor == PlayerPawn)
        {
            continue;
        }

        if (Actor->IsActorTickEnabled())
        {
            PausedActors.Add(Actor);
            Actor->SetActorTickEnabled(false);
        }

        TArray<UActorComponent*> AllComponents;
        Actor->GetComponents(AllComponents);

        for (UActorComponent* Comp : AllComponents)
        {
            if (!IsValid(Comp))
            {
                continue;
            }

            if (Comp->IsComponentTickEnabled())
            {
                PausedComponents.Add(Comp);
                Comp->SetComponentTickEnabled(false);
            }
        }
    }

    UE_LOG(LogUnrealCV, Log, TEXT("PauseAllActorsExceptPawn: Paused %d actors, %d components"),
        PausedActors.Num(), PausedComponents.Num());
}

void URuntimeActorSetterBPLib::ResumeAllActors()
{
    for (const TWeakObjectPtr<AActor>& WeakActor : PausedActors)
    {
        if (AActor* Actor = WeakActor.Get())
        {
            Actor->SetActorTickEnabled(true);
        }
    }

    for (const TWeakObjectPtr<UActorComponent>& WeakComp : PausedComponents)
    {
        if (UActorComponent* Comp = WeakComp.Get())
        {
            Comp->SetComponentTickEnabled(true);
        }
    }

    UE_LOG(LogUnrealCV, Log, TEXT("ResumeAllActors: Resumed %d actors, %d components"),
        PausedActors.Num(), PausedComponents.Num());

    PausedActors.Empty();
    PausedComponents.Empty();
}

void URuntimeActorSetterBPLib::SetCustomTimeDilationAllActorsExceptPawn(UObject* WorldContextObject, float TimeDilation)
{
    UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
    if (!World)
    {
        UE_LOG(LogUnrealCV, Error, TEXT("SetCustomTimeDilationAllActorsExceptPawn: Invalid world context"));
        return;
    }

    APlayerController* PlayerController = World->GetFirstPlayerController();
    APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;

    int32 Count = 0;
    for (FActorIterator It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!IsValid(Actor))
        {
            continue;
        }

        if (Actor == PlayerPawn)
        {
            continue;
        }

        Actor->CustomTimeDilation = TimeDilation;
        Count++;
    }

    UE_LOG(LogUnrealCV, Log, TEXT("SetCustomTimeDilationAllActorsExceptPawn: Set %.2f for %d actors"),
        TimeDilation, Count);
}
