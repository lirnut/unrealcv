#include "GroomBPLib.h"
#include "GroomComponent.h"
#include "GroomAssetPhysics.h"
#include "NiagaraComponent.h"
#include "EngineUtils.h"
#include "UnrealcvLog.h"

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

bool UGroomBPLib::PauseHairSimulation(AActor* Actor)
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
            int32 GroupCount = GroomComp->GetGroupCount();
            for (int32 GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex)
            {
                UNiagaraComponent* NiagaraComp = GroomComp->GetNiagaraComponent(GroupIndex);
                if (IsValid(NiagaraComp))
                {
                    NiagaraComp->SetPaused(true);
                }
            }
            GroomComp->SetComponentTickEnabled(false);
        }
    }
    return true;
}

bool UGroomBPLib::ResumeHairSimulation(AActor* Actor)
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
            int32 GroupCount = GroomComp->GetGroupCount();
            for (int32 GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex)
            {
                UNiagaraComponent* NiagaraComp = GroomComp->GetNiagaraComponent(GroupIndex);
                if (IsValid(NiagaraComp))
                {
                    NiagaraComp->SetPaused(false);
                }
            }
            GroomComp->SetComponentTickEnabled(true);
        }
    }
    return true;
}

bool UGroomBPLib::SetHairSimulationTimeDilation(AActor* Actor, float TimeDilation)
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
            int32 GroupCount = GroomComp->GetGroupCount();
            for (int32 GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex)
            {
                UNiagaraComponent* NiagaraComp = GroomComp->GetNiagaraComponent(GroupIndex);
                if (IsValid(NiagaraComp))
                {
                    NiagaraComp->SetCustomTimeDilation(TimeDilation);
                }
            }
        }
    }
    return true;
}

bool UGroomBPLib::UpdateGroomRenderState(
    AActor* Actor,
    bool bUpdateTransform,
    bool bUpdateDynamicData,
    bool bInvalidateRenderState,
    bool bUpdateHairGroupsDesc)
{
    if (!IsValid(Actor))
    {
        return false;
    }

    TArray<UGroomComponent*> GroomComponents;
    FindAllGroomComponentsInActor(Actor, GroomComponents);

    if (GroomComponents.Num() == 0)
    {
        UE_LOG(LogUnrealCV, Error, TEXT("UpdateGroomRenderState: Actor %s has no GroomComponent"), *Actor->GetName());
        return false;
    }

    for (UGroomComponent* GroomComp : GroomComponents)
    {
        if (!IsValid(GroomComp) || !GroomComp->IsRegistered())
        {
            UE_LOG(LogUnrealCV, Error, TEXT("UpdateGroomRenderState: GroomComponent %s is not registered"), *GroomComp->GetName());
            continue;
        }

        // 1. Update hair groups description (shadow density, raytracing settings, etc.)
        // This updates FHairGroupDesc modifiers from GroomAsset settings
        if (bUpdateHairGroupsDesc)
        {
            UE_LOG(LogUnrealCV, Log, TEXT("UpdateGroomRenderState: Update HairGroupsDesc for GroomComponent %s"), *GroomComp->GetName());
            GroomComp->UpdateHairGroupsDescAndInvalidateRenderState(true);
        }

        // 2. Mark render transform dirty - triggers bounds recalculation
        // Essential for voxelization and shadow frustum culling
        if (bUpdateTransform)
        {
            UE_LOG(LogUnrealCV, Log, TEXT("UpdateGroomRenderState: Mark RenderTransformDirty for GroomComponent %s"), *GroomComp->GetName());
            GroomComp->MarkRenderTransformDirty();
        }

        // 3. Mark dynamic data dirty - triggers SendRenderDynamicData_Concurrent
        // This handles buffer swap for skinning/deformation data
        if (bUpdateDynamicData)
        {
            UE_LOG(LogUnrealCV, Log, TEXT("UpdateGroomRenderState: Mark RenderDynamicDataDirty for GroomComponent %s"), *GroomComp->GetName());
            GroomComp->MarkRenderDynamicDataDirty();
        }

        // 4. Full render state invalidation - most aggressive option
        // Forces complete rebuild of SceneProxy
        if (bInvalidateRenderState)
        {
            UE_LOG(LogUnrealCV, Log, TEXT("UpdateGroomRenderState: Mark RenderStateDirty for GroomComponent %s"), *GroomComp->GetName());
            GroomComp->MarkRenderStateDirty();
        }
    }

    return true;
}

bool UGroomBPLib::RefreshAllGroomShadows(UObject* WorldContextObject)
{
    UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
    if (!World)
    {
        return false;
    }

    int32 UpdatedCount = 0;

    // Iterate all actors with GroomComponents
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!IsValid(Actor))
        {
            continue;
        }

        TArray<UGroomComponent*> GroomComponents;
        Actor->GetComponents<UGroomComponent>(GroomComponents);

        for (UGroomComponent* GroomComp : GroomComponents)
        {
            if (IsValid(GroomComp) && GroomComp->IsRegistered())
                {
                    UE_LOG(LogUnrealCV, Log, TEXT("RefreshAllGroomShadows: Refresh GroomComponent %s"), *GroomComp->GetName());
                    // Apply all refresh methods for maximum effect
                    GroomComp->MarkRenderTransformDirty();
                    GroomComp->MarkRenderDynamicDataDirty();
                    UpdatedCount++;
                }
        }
    }

    return UpdatedCount > 0;
}

