#include "RuntimeActorSetterBPLib.h"
#include "Components/PrimitiveComponent.h"

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
