#include "GroomBPLib.h"
#include "GroomComponent.h"
#include "GroomAssetPhysics.h"

namespace
{
    void CollectGroomComponentsRecursive(USceneComponent* SceneComponent, TArray<UGroomComponent*>& OutComponents)
    {
        if (!SceneComponent)
        {
            return;
        }

        UGroomComponent* GroomComp = Cast<UGroomComponent>(SceneComponent);
        if (IsValid(GroomComp))
        {
            OutComponents.Add(GroomComp);
        }

        for (USceneComponent* Child : SceneComponent->GetAttachChildren())
        {
            CollectGroomComponentsRecursive(Child, OutComponents);
        }
    }

    void FindAllGroomComponentsInActor(AActor* Actor, TArray<UGroomComponent*>& OutComponents)
    {
        if (!IsValid(Actor))
        {
            return;
        }

        TArray<UGroomComponent*> DirectComponents;
        Actor->GetComponents<UGroomComponent>(DirectComponents);
        for (UGroomComponent* Comp : DirectComponents)
        {
            if (IsValid(Comp))
            {
                OutComponents.Add(Comp);
            }
        }

        for (USceneComponent* Child : Actor->GetRootComponent()->GetAttachChildren())
        {
            CollectGroomComponentsRecursive(Child, OutComponents);
        }
    }
}

bool UGroomBPLib::SetHairGravity(AActor* Actor, FVector GravityVector)
{
    TArray<UGroomComponent*> GroomComponents;
    FindAllGroomComponentsInActor(Actor, GroomComponents);

    if (GroomComponents.Num() == 0)
    {
        return false;
    }

    for (UGroomComponent* GroomComp : GroomComponents)
    {
        if (IsValid(GroomComp))
        {
            GroomComp->SimulationSettings.bOverrideSettings = true;
            GroomComp->SimulationSettings.SolverSettings.bEnableSimulation = true;
            GroomComp->SimulationSettings.ExternalForces.GravityVector = GravityVector;
        }
    }
    return true;
}

bool UGroomBPLib::SetHairAirDrag(AActor* Actor, float AirDrag)
{
    TArray<UGroomComponent*> GroomComponents;
    FindAllGroomComponentsInActor(Actor, GroomComponents);

    if (GroomComponents.Num() == 0)
    {
        return false;
    }

    for (UGroomComponent* GroomComp : GroomComponents)
    {
        if (IsValid(GroomComp))
        {
            GroomComp->SimulationSettings.bOverrideSettings = true;
            GroomComp->SimulationSettings.SolverSettings.bEnableSimulation = true;
            GroomComp->SimulationSettings.ExternalForces.AirDrag = AirDrag;
        }
    }
    return true;
}

bool UGroomBPLib::SetHairAirVelocity(AActor* Actor, FVector AirVelocity)
{
    TArray<UGroomComponent*> GroomComponents;
    FindAllGroomComponentsInActor(Actor, GroomComponents);

    if (GroomComponents.Num() == 0)
    {
        return false;
    }

    for (UGroomComponent* GroomComp : GroomComponents)
    {
        if (IsValid(GroomComp))
        {
            GroomComp->SimulationSettings.bOverrideSettings = true;
            GroomComp->SimulationSettings.SolverSettings.bEnableSimulation = true;
            GroomComp->SimulationSettings.ExternalForces.AirVelocity = AirVelocity;
        }
    }
    return true;
}

bool UGroomBPLib::SetHairBendDamping(AActor* Actor, float BendDamping)
{
    TArray<UGroomComponent*> GroomComponents;
    FindAllGroomComponentsInActor(Actor, GroomComponents);

    if (GroomComponents.Num() == 0)
    {
        return false;
    }

    for (UGroomComponent* GroomComp : GroomComponents)
    {
        if (IsValid(GroomComp))
        {
            GroomComp->SimulationSettings.bOverrideSettings = true;
            GroomComp->SimulationSettings.SolverSettings.bEnableSimulation = true;
            GroomComp->SimulationSettings.MaterialConstraints.BendDamping = BendDamping;
        }
    }
    return true;
}

bool UGroomBPLib::SetHairBendStiffness(AActor* Actor, float BendStiffness)
{
    TArray<UGroomComponent*> GroomComponents;
    FindAllGroomComponentsInActor(Actor, GroomComponents);

    if (GroomComponents.Num() == 0)
    {
        return false;
    }

    for (UGroomComponent* GroomComp : GroomComponents)
    {
        if (IsValid(GroomComp))
        {
            GroomComp->SimulationSettings.bOverrideSettings = true;
            GroomComp->SimulationSettings.SolverSettings.bEnableSimulation = true;
            GroomComp->SimulationSettings.MaterialConstraints.BendStiffness = BendStiffness;
        }
    }
    return true;
}

bool UGroomBPLib::SetHairStrandsViscosity(AActor* Actor, float StrandsViscosity)
{
    TArray<UGroomComponent*> GroomComponents;
    FindAllGroomComponentsInActor(Actor, GroomComponents);

    if (GroomComponents.Num() == 0)
    {
        return false;
    }

    for (UGroomComponent* GroomComp : GroomComponents)
    {
        if (IsValid(GroomComp))
        {
            GroomComp->SimulationSettings.bOverrideSettings = true;
            GroomComp->SimulationSettings.SolverSettings.bEnableSimulation = true;
            GroomComp->SimulationSettings.MaterialConstraints.StrandsViscosity = StrandsViscosity;
        }
    }
    return true;
}

bool UGroomBPLib::DisableHairGravity(AActor* Actor)
{
    return SetHairGravity(Actor, FVector::ZeroVector);
}

bool UGroomBPLib::SetHairSimulationEnabled(AActor* Actor, bool bEnabled)
{
    TArray<UGroomComponent*> GroomComponents;
    FindAllGroomComponentsInActor(Actor, GroomComponents);

    if (GroomComponents.Num() == 0)
    {
        return false;
    }

    for (UGroomComponent* GroomComp : GroomComponents)
    {
        if (IsValid(GroomComp))
        {
            GroomComp->SetEnableSimulation(bEnabled);
        }
    }
    return true;
}

bool UGroomBPLib::ResetHairSimulation(AActor* Actor)
{
    TArray<UGroomComponent*> GroomComponents;
    FindAllGroomComponentsInActor(Actor, GroomComponents);

    if (GroomComponents.Num() == 0)
    {
        return false;
    }

    for (UGroomComponent* GroomComp : GroomComponents)
    {
        if (IsValid(GroomComp))
        {
            GroomComp->ResetSimulation();
        }
    }
    return true;
}

