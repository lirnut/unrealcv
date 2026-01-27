// from UE_5.6\Engine\Plugins\Runtime\HairStrands\Source\HairStrandsCore\Private\GroomComponent.cpp

#include "HairStrandsSceneProxy.h"
#include "GroomComponent.h"
#include "HairCardsVertexFactory.h"
#include "PrimitiveUniformShaderParametersBuilder.h"
#include "UnrealCVLog.h"


inline int32 GetMaterialIndexWithFallback(int32 SlotIndex)
{
	// Default policy: if no slot index has been bound, fallback on slot 0. If there is no
	// slot, the material will fallback on the default material.
	return SlotIndex != INDEX_NONE ? SlotIndex : 0;
}
static uint32 GetPointToVertexCount()
{
	return GetHairStrandsUsesTriangleStrips() ? HAIR_POINT_TO_VERTEX_FOR_TRISTRP : HAIR_POINT_TO_VERTEX_FOR_TRILIST;
}
static EPrimitiveType GetPrimitiveType(EHairGeometryType In)
{
	return (In == EHairGeometryType::Strands && GetHairStrandsUsesTriangleStrips()) ? PT_TriangleStrip : PT_TriangleList;
}


SIZE_T FHairStrandsSceneProxy::GetTypeHash() const
{
    static size_t UniquePointer;
    return reinterpret_cast<size_t>(&UniquePointer);
}

 FRenderCurveResourceData* FHairStrandsSceneProxy::GetRenderCurveResourceData()
{
    return CurveResourceData;
}

FHairStrandsSceneProxy::FHairStrandsSceneProxy(UGroomComponent* Component)
    : FPrimitiveSceneProxy(Component)
    , MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
{
    // Forcing primitive uniform as we don't support robustly GPU scene data
    bVFRequiresPrimitiveUniformBuffer = true;
    bCastDeepShadow = true;

    HairGroupMaterialProxies.SetNum(Component->HairGroupInstances.Num());
    HairGroupInstances = Component->HairGroupInstances;
    check(Component);
    check(Component->GroomAsset);
    check(Component->GroomAsset->GetNumHairGroups() > 0);
    ComponentId = Component->GetPrimitiveSceneId().PrimIDValue;
    Strands_DebugMaterial = Component->Strands_DebugMaterial;
    bAlwaysHasVelocity = false;
    if ((IsHairStrandsBindingEnable() && Component->RegisteredMeshComponent))
    {
        bAlwaysHasVelocity = true;
    }

    check(HairGroupInstances.Num());

    const ERHIFeatureLevel::Type FeatureLevel = GetScene().GetFeatureLevel();
    const EShaderPlatform ShaderPlatform = GetScene().GetShaderPlatform();

    const int32 GroupCount = Component->GroomAsset->GetNumHairGroups();
    check(Component->GroomAsset->GetHairGroupsPlatformData().Num() == HairGroupInstances.Num());
    for (int32 GroupIt=0; GroupIt<GroupCount; GroupIt++)
    {
        const bool bIsVisible = Component->GroomAsset->GetHairGroupsInfo()[GroupIt].bIsVisible;

        FHairGroupPlatformData& InGroupDataNonConst = Component->GroomAsset->GetHairGroupsPlatformData()[GroupIt];
        const FHairGroupPlatformData& InGroupData = Component->GroomAsset->GetHairGroupsPlatformData()[GroupIt];
        FHairGroupInstance* HairInstance = HairGroupInstances[GroupIt];
        check(HairInstance->HairGroupPublicData);

        // DO NOT modify HairInstance state - it's shared with the original GroomComponent SceneProxy
        // This causes RDG resource access conflicts when both proxies try to update the same resources
        // HairInstance->bForceCards = Component->bUseCards;
        // HairInstance->bUpdatePositionOffset = Component->RegisteredMeshComponent != nullptr;

        // VertexFactory should already be created by the original GroomComponent
        // if (HairInstance->Strands.IsValid() && HairInstance->Strands.VertexFactory == nullptr)
        // {
        //     HairInstance->Strands.VertexFactory = new FHairStrandsVertexFactory(HairInstance, FeatureLevel, "FStrandsHairSceneProxy");
        // }

        // (Experimental) For now only use the resource of the first group
        CurveResourceData = nullptr;
        if (IsRenderCurveEnabled() && HairInstance->Strands.IsValid() && GroupIt == 0)
        {
            CurveResourceData = (&InGroupDataNonConst.Strands.CurveResourceData);
        }

        if (HairInstance->Cards.IsValid())
        {
            const uint32 LODCount = HairInstance->Cards.LODs.Num();
            for (uint32 LODIt = 0; LODIt < LODCount; ++LODIt)
            {
                if (HairInstance->Cards.IsValid(LODIt))
                {
                    // VertexFactory should already be created by the original GroomComponent
                    // if (HairInstance->Cards.LODs[LODIt].VertexFactory == nullptr) HairInstance->Cards.LODs[LODIt].VertexFactory = new FHairCardsVertexFactory(HairInstance, LODIt, EHairGeometryType::Cards, ShaderPlatform, FeatureLevel, "HairCardsVertexFactory");
                }
            }
        }

        if (HairInstance->Meshes.IsValid())
        {
            const uint32 LODCount = HairInstance->Meshes.LODs.Num();
            for (uint32 LODIt = 0; LODIt < LODCount; ++LODIt)
            {
                if (HairInstance->Meshes.IsValid(LODIt))
                {
                    // VertexFactory should already be created by the original GroomComponent
                    // if (HairInstance->Meshes.LODs[LODIt].VertexFactory == nullptr) HairInstance->Meshes.LODs[LODIt].VertexFactory = new FHairCardsVertexFactory(HairInstance, LODIt, EHairGeometryType::Meshes, ShaderPlatform, FeatureLevel, "HairMeshesVertexFactory");
                }
            }
        }

        {
            // If one of the group has simulation enable, then we enable velocity rendering for meshes/cards
            if (IsHairStrandsSimulationEnable() && HairInstance->Guides.IsValid() && (HairInstance->Guides.bIsSimulationEnable || HairInstance->Guides.bIsDeformationEnable || HairInstance->Guides.bIsSimulationCacheEnable))
            {
                bAlwaysHasVelocity = true;
            }
        }

        // Material - Strands
        if (IsHairStrandsEnabled(EHairStrandsShaderType::Strands, ShaderPlatform))
        {
            const int32 SlotIndex = Component->GroomAsset->GetMaterialIndex(Component->GroomAsset->GetHairGroupsRendering()[GroupIt].MaterialSlotName);
            // Component->CheckHairStrandsUsage(GetMaterialIndexWithFallback(SlotIndex));
            const UMaterialInterface* Material = Component->GetMaterial(GetMaterialIndexWithFallback(SlotIndex), EHairGeometryType::Strands, true);
            HairGroupMaterialProxies[GroupIt].Strands = Material ? Material->GetRenderProxy() : nullptr;
        }

        // Material - Cards
        HairGroupMaterialProxies[GroupIt].Cards.Init(nullptr, InGroupData.Cards.LODs.Num());
        if (IsHairStrandsEnabled(EHairStrandsShaderType::Cards, ShaderPlatform))
        {
            uint32 CardsLODIndex = 0;
            for (const FHairGroupPlatformData::FCards::FLOD& LOD : InGroupData.Cards.LODs)
            {
                if (LOD.IsValid())
                {
                    // Material
                    int32 SlotIndex = INDEX_NONE;
                    for (const FHairGroupsCardsSourceDescription& Desc : Component->GroomAsset->GetHairGroupsCards())
                    {
                        if (Desc.GroupIndex == GroupIt && Desc.LODIndex == CardsLODIndex)
                        {
                            SlotIndex = Component->GroomAsset->GetMaterialIndex(Desc.MaterialSlotName);
                            break;
                        }
                    }
                    // Component->CheckHairStrandsUsage(GetMaterialIndexWithFallback(SlotIndex));
                    const UMaterialInterface* Material = Component->GetMaterial(GetMaterialIndexWithFallback(SlotIndex), EHairGeometryType::Cards, true);
                    HairGroupMaterialProxies[GroupIt].Cards[CardsLODIndex] = Material ? Material->GetRenderProxy() : nullptr;
                }
                ++CardsLODIndex;
            }
        }

        // Material - Meshes
        HairGroupMaterialProxies[GroupIt].Meshes.Init(nullptr, InGroupData.Meshes.LODs.Num());
        if (IsHairStrandsEnabled(EHairStrandsShaderType::Meshes, ShaderPlatform))
        {
            uint32 MeshesLODIndex = 0;
            for (const FHairGroupPlatformData::FMeshes::FLOD& LOD : InGroupData.Meshes.LODs)
            {
                if (LOD.IsValid())
                {
                    // Material
                    int32 SlotIndex = INDEX_NONE;
                    for (const FHairGroupsMeshesSourceDescription& Desc : Component->GroomAsset->GetHairGroupsMeshes())
                    {
                        if (Desc.GroupIndex == GroupIt && Desc.LODIndex == MeshesLODIndex)
                        {
                            SlotIndex = Component->GroomAsset->GetMaterialIndex(Desc.MaterialSlotName);
                            break;
                        }
                    }
                    // Component->CheckHairStrandsUsage(GetMaterialIndexWithFallback(SlotIndex));
                    const UMaterialInterface* Material = Component->GetMaterial(GetMaterialIndexWithFallback(SlotIndex), EHairGeometryType::Meshes, true);
                    HairGroupMaterialProxies[GroupIt].Meshes[MeshesLODIndex] = Material ? Material->GetRenderProxy() : nullptr;
                }
                ++MeshesLODIndex;
            }
        }
    }
}

 FHairStrandsSceneProxy::~FHairStrandsSceneProxy()
{
}

/**
 *	Called when the rendering thread adds the proxy to the scene.
    *	This function allows for generating renderer-side resources.
    *	Called in the rendering thread.
    */
 void FHairStrandsSceneProxy::CreateRenderThreadResources(FRHICommandListBase& RHICmdList) 
{
    FPrimitiveSceneProxy::CreateRenderThreadResources(RHICmdList);

    // Register the data to the scene
    FSceneInterface& LocalScene = GetScene();
    for (TRefCountPtr<FHairGroupInstance>& Instance : HairGroupInstances)
    {
        if (Instance->IsValid() || Instance->Strands.ClusterResource)
        {
            check(Instance->HairGroupPublicData != nullptr);
            // auto RegisteredIndex = Instance->RegisteredIndex;
            Instance->AddRef();
            // Instance->Debug.Proxy = this;
            LocalScene.RemoveHairStrands(Instance);
            LocalScene.AddHairStrands(Instance);
            // Instance->RegisteredIndex = RegisteredIndex;
        }
    }
}

/**
 *	Called when the rendering thread removes the proxy from the scene.
    *	This function allows for removing renderer-side resources.
    *	Called in the rendering thread.
    */
void FHairStrandsSceneProxy::DestroyRenderThreadResources()
{
    FPrimitiveSceneProxy::DestroyRenderThreadResources();

    // Unregister the data to the scene
    FSceneInterface& LocalScene = GetScene();
    for (TRefCountPtr<FHairGroupInstance>& Instance : HairGroupInstances)
    {
        if (Instance->IsValid() || Instance->Strands.ClusterResource)
        {
            check(Instance->GetRefCount() > 0);
            // LocalScene.RemoveHairStrands(Instance);
            // Instance->Debug.Proxy = nullptr;
            Instance->Release();
        }
    }
}

void FHairStrandsSceneProxy::OnTransformChanged(FRHICommandListBase& RHICmdList)
{
    // const FTransform RigidLocalToWorld = FTransform(GetLocalToWorld());
    // for (TRefCountPtr<FHairGroupInstance>& Instance : HairGroupInstances)
    // {
    //     Instance->Debug.RigidPreviousLocalToWorld = Instance->Debug.RigidCurrentLocalToWorld;
    //     Instance->Debug.RigidCurrentLocalToWorld = RigidLocalToWorld;
    // }
}

FORCEINLINE bool FHairStrandsSceneProxy::UseProxyLocalToWorld(const FHairGroupInstance* Instance) const
{
    return (Instance->BindingType != EHairBindingType::Skinning);
}

#if RHI_RAYTRACING
void FHairStrandsSceneProxy::GetDynamicRayTracingInstances(FRayTracingInstanceCollector& Collector)
{
    if (!IsHairRayTracingEnabled() || HairGroupInstances.Num() == 0)
        return;

    const EShaderPlatform Platform = Collector.GetReferenceView()->GetShaderPlatform();
    if (!IsHairStrandsEnabled(EHairStrandsShaderType::Strands, Platform) &&
        !IsHairStrandsEnabled(EHairStrandsShaderType::Cards, Platform) &&
        !IsHairStrandsEnabled(EHairStrandsShaderType::Meshes, Platform))
    {
        return;
    }

    const bool bWireframe = false && Collector.GetReferenceView()->Family->EngineShowFlags.Wireframe;
    const EHairViewRayTracingMask ViewRayTracingMask = Collector.GetReferenceView()->Family->EngineShowFlags.PathTracing ? EHairViewRayTracingMask::PathTracing : EHairViewRayTracingMask::RayTracing;
    if (bWireframe)
        return;

    for (uint32 GroupIt = 0, GroupCount = HairGroupInstances.Num(); GroupIt < GroupCount; ++GroupIt)
    {
        FHairGroupInstance* Instance = HairGroupInstances[GroupIt];
        check(Instance->GetRefCount() > 0);

        const EHairGeometryType GeometryType = Instance->HairGroupPublicData->VFInput.GeometryType;
        if (GeometryType == EHairGeometryType::NoneGeometry)
        {
            UE_LOG(LogUnrealCV, Verbose, TEXT("GetDynamicRayTracingInstances: Skipping HairGroup %d with NoneGeometry"), GroupIt);
            continue;
        }

        FMatrix OverrideLocalToWorld = UseProxyLocalToWorld(Instance) ? GetLocalToWorld() : Instance->GetCurrentLocalToWorld().ToMatrixWithScale();

        const uint32 LODIndex = Instance->HairGroupPublicData->GetIntLODIndex();

        FHairStrandsRaytracingResource* RTGeometry = nullptr;
        bool bIsHairStrands = false;
        EHairViewRayTracingMask InstanceViewRayTracingMask = EHairViewRayTracingMask::PathTracing | EHairViewRayTracingMask::RayTracing;
        switch (GeometryType)
        {
            case EHairGeometryType::Strands:
            {
                RTGeometry = Instance->Strands.RenRaytracingResource;
                InstanceViewRayTracingMask = Instance->Strands.ViewRayTracingMask;
                bIsHairStrands = true;
                break;
            }
            case EHairGeometryType::Cards:
            {
                RTGeometry = Instance->Cards.LODs[LODIndex].RaytracingResource;
                break;
            }
            case EHairGeometryType::Meshes:
            {
                RTGeometry = Instance->Meshes.LODs[LODIndex].RaytracingResource;
                break;
            }
        }

        // If the view and the instance raytracing mask don't match skip this instance.
        if (!EnumHasAnyFlags(InstanceViewRayTracingMask, ViewRayTracingMask))
        {
            continue;
        }

        if (RTGeometry && RTGeometry->RayTracingGeometry.IsValid())
        {
            for (const FRayTracingGeometrySegment& Segment : RTGeometry->RayTracingGeometry.Initializer.Segments)
            {
                check(Segment.VertexBuffer.IsValid());
            }
            if (FMeshBatch* MeshBatch = CreateMeshBatch(Collector.GetReferenceView(), *Collector.GetReferenceView()->Family, Collector, EHairMeshBatchType::Raytracing, Instance, GroupIt, nullptr))
            {
                FRayTracingInstance RayTracingInstance;
                RayTracingInstance.Geometry = &RTGeometry->RayTracingGeometry;
                RayTracingInstance.Materials.Add(*MeshBatch);
                RayTracingInstance.InstanceTransforms.Add(OverrideLocalToWorld);
                RayTracingInstance.bThinGeometry = bIsHairStrands;

                Collector.AddRayTracingInstance(MoveTemp(RayTracingInstance));
                if (RTGeometry->PositionBuffer.Buffer)
                {
                    Collector.AddRDGPooledBuffer(RTGeometry->PositionBuffer.Buffer);
                }
                if (RTGeometry->IndexBuffer.Buffer)
                {
                    Collector.AddRDGPooledBuffer(RTGeometry->IndexBuffer.Buffer);
                }
                UE_LOG(LogUnrealCV, VeryVerbose, TEXT("GetDynamicRayTracingInstances: Added RayTracingInstance for HairGroup %d, GeometryType=%d"),
                    GroupIt, (int32)GeometryType);
            }
            else
            {
                UE_LOG(LogUnrealCV, Warning, TEXT("GetDynamicRayTracingInstances: CreateMeshBatch failed for HairGroup %d, GeometryType=%d"),
                    GroupIt, (int32)GeometryType);
            }
        }
        else
        {
            UE_LOG(LogUnrealCV, Verbose, TEXT("GetDynamicRayTracingInstances: Invalid RTGeometry for HairGroup %d, GeometryType=%d"),
                GroupIt, (int32)GeometryType);
        }
    }
}
#endif

void FHairStrandsSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
    const EShaderPlatform Platform = ViewFamily.GetShaderPlatform();
    if (!IsHairStrandsEnabled(EHairStrandsShaderType::Strands, Platform) &&
        !IsHairStrandsEnabled(EHairStrandsShaderType::Cards, Platform) &&
        !IsHairStrandsEnabled(EHairStrandsShaderType::Meshes, Platform))
    {
        return;
    }

    if (HairGroupInstances.Num() == 0)
    {
        return;
    }

    const uint32 GroupCount = HairGroupInstances.Num();

    QUICK_SCOPE_CYCLE_COUNTER(STAT_HairStrandsSceneProxy_GetDynamicMeshElements);

    // Need information back from the rendering thread to knwo which representation to use (strands/cards/mesh)
    for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
    {
        const FSceneView* View = Views[ViewIndex];
        if (View->bIsReflectionCapture)
        {
            continue;
        }

        if ((IsShadowCast(View) || IsShown(View)) && (VisibilityMap & (1 << ViewIndex)))
        {
            for (uint32 GroupIt = 0; GroupIt < GroupCount; ++GroupIt)
            {
                check(HairGroupInstances[GroupIt]->GetRefCount() > 0);

                if (HairGroupInstances[GroupIt]->GeometryType == EHairGeometryType::NoneGeometry)
                {
                    UE_LOG(LogUnrealCV, Verbose, TEXT("GetDynamicMeshElements: Skipping HairGroup %d with NoneGeometry"), GroupIt);
                    continue;
                }

                FMaterialRenderProxy* Debug_MaterialProxy = nullptr;
                const EGroomViewMode ViewMode = GetGroomViewMode(*View);
                bool bNeedDebugMaterial = false;
                switch(ViewMode)
                {
                case EGroomViewMode::SimHairStrands	:
                case EGroomViewMode::Cluster		:
                case EGroomViewMode::ClusterAABB	:
                case EGroomViewMode::RenderHairStrands:
                    bNeedDebugMaterial = HairGroupInstances[GroupIt]->GeometryType == EHairGeometryType::Strands; break;
                case EGroomViewMode::RootUV			:
                case EGroomViewMode::UV				:
                case EGroomViewMode::Seed			:
                case EGroomViewMode::ClumpID		:
                case EGroomViewMode::Dimension		:
                case EGroomViewMode::RadiusVariation:
                case EGroomViewMode::RootUDIM		:
                case EGroomViewMode::Color			:
                case EGroomViewMode::Roughness		:
                case EGroomViewMode::AO				:
                case EGroomViewMode::Group			:
                case EGroomViewMode::LODColoration	:
                    // bNeedDebugMaterial = true;
                    break;
                };

                // if (bNeedDebugMaterial)
                // {
                // 	float DebugModeScalar = 0;
                // 	switch(ViewMode)
                // 	{
                // 	case EGroomViewMode::None				: DebugModeScalar =99.f; break;
                // 	case EGroomViewMode::SimHairStrands		: DebugModeScalar = 0.f; break;
                // 	case EGroomViewMode::RenderHairStrands	: DebugModeScalar = 0.f; break;
                // 	case EGroomViewMode::RootUV				: DebugModeScalar = 1.f; break;
                // 	case EGroomViewMode::UV					: DebugModeScalar = 2.f; break;
                // 	case EGroomViewMode::Seed				: DebugModeScalar = 3.f; break;
                // 	case EGroomViewMode::Dimension			: DebugModeScalar = 4.f; break;
                // 	case EGroomViewMode::RadiusVariation	: DebugModeScalar = 5.f; break;
                // 	case EGroomViewMode::RootUDIM			: DebugModeScalar = 6.f; break;
                // 	case EGroomViewMode::Color				: DebugModeScalar = 7.f; break;
                // 	case EGroomViewMode::Roughness			: DebugModeScalar = 8.f; break;
                // 	case EGroomViewMode::Cluster			: DebugModeScalar = 0.f; break;
                // 	case EGroomViewMode::ClusterAABB		: DebugModeScalar = 0.f; break;
                // 	case EGroomViewMode::Group				: DebugModeScalar = 9.f; break;
                // 	case EGroomViewMode::LODColoration		: DebugModeScalar = 10.f; break;
                // 	case EGroomViewMode::ClumpID			: DebugModeScalar = 11.f; break;
                // 	case EGroomViewMode::AO					: DebugModeScalar = 12.f; break;
                // 	};

                // 	// TODO: fix this as the radius is incorrect. This code run before the interpolation code, which is where HairRadius is updated.
                // 	float HairMaxRadius = 0;
                // 	for (FHairGroupInstance* Instance : HairGroupInstances)
                // 	{
                // 		HairMaxRadius = FMath::Max(HairMaxRadius, Instance->Strands.Modifier.HairWidth * 0.5f);
                // 	}
                    
                // 	// Reuse the HairMaxRadius field to send the LOD index instead of adding yet another variable
                // 	if (ViewMode == EGroomViewMode::LODColoration)
                // 	{
                // 		HairMaxRadius = HairGroupInstances[GroupIt]->HairGroupPublicData ? HairGroupInstances[GroupIt]->HairGroupPublicData->LODIndex : 0;
                // 	}

                // 	// Reuse the HairMaxRadius field to send the clumpID index selection
                // 	if (ViewMode == EGroomViewMode::ClumpID)
                // 	{
                // 		HairMaxRadius = 0;
                // 	}

                // 	FVector HairColor = FVector::ZeroVector;
                // 	if (ViewMode == EGroomViewMode::Group)
                // 	{
                // 		HairColor = FVector(GetHairGroupDebugColor(GroupIt));
                // 	}
                // 	else if (ViewMode == EGroomViewMode::LODColoration)
                // 	{
                // 		int32 LODIndex = HairGroupInstances[GroupIt]->HairGroupPublicData ? HairGroupInstances[GroupIt]->HairGroupPublicData->LODIndex : 0;
                // 		LODIndex = FMath::Clamp(LODIndex, 0, GEngine->LODColorationColors.Num() - 1);
                // 		const FLinearColor LODColor = GEngine->LODColorationColors[LODIndex];
                // 		HairColor = FVector(LODColor.R, LODColor.G, LODColor.B);
                // 	}
                // 	auto DebugMaterial = new FHairDebugModeMaterialRenderProxy(Strands_DebugMaterial ? Strands_DebugMaterial->GetRenderProxy() : nullptr, DebugModeScalar, 0, HairMaxRadius, HairColor);
                // 	Collector.RegisterOneFrameMaterialProxy(DebugMaterial);
                // 	Debug_MaterialProxy = DebugMaterial;
                // }

                if (FMeshBatch* MeshBatch = CreateMeshBatch(View, ViewFamily, Collector, EHairMeshBatchType::Raster, HairGroupInstances[GroupIt], GroupIt, Debug_MaterialProxy))
                {
                    Collector.AddMesh(ViewIndex, *MeshBatch);
                    UE_LOG(LogUnrealCV, VeryVerbose, TEXT("GetDynamicMeshElements: Added MeshBatch for HairGroup %d, GeometryType=%d"),
                        GroupIt, (int32)HairGroupInstances[GroupIt]->GeometryType);
                }
                else
                {
                    UE_LOG(LogUnrealCV, Warning, TEXT("GetDynamicMeshElements: CreateMeshBatch failed for HairGroup %d, GeometryType=%d"),
                        GroupIt, (int32)HairGroupInstances[GroupIt]->GeometryType);
                    continue;
                }

            #if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
                // Render bounds
                RenderBounds(Collector.GetPDI(ViewIndex), ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
            #endif
            }
        }
    }
}

FMeshBatch* FHairStrandsSceneProxy::CreateMeshBatch(
    const FSceneView* View,
    const FSceneViewFamily& ViewFamily,
    FMeshElementCollector& Collector,
    const EHairMeshBatchType MeshBatchType,
    const FHairGroupInstance* Instance,
    uint32 GroupIndex,
    FMaterialRenderProxy* Debug_MaterialProxy) const
{
    const EHairGeometryType GeometryType = Instance->GeometryType;
    if (GeometryType == EHairGeometryType::NoneGeometry)
    {
        UE_LOG(LogUnrealCV, VeryVerbose, TEXT("CreateMeshBatch: GeometryType is NoneGeometry for HairGroup %d"), GroupIndex);
        return nullptr;
    }

    check(Instance->GetRefCount());

    const int32 IntLODIndex = Instance->HairGroupPublicData->GetIntLODIndex();
    const bool bIsVisible = true;

    const FVertexFactory* VertexFactory = nullptr;
    FIndexBuffer* IndexBuffer = nullptr;
    const FMaterialRenderProxy* MaterialRenderProxy = nullptr;
    const ERHIFeatureLevel::Type FeatureLevel = View->GetFeatureLevel();

    uint32 NumPrimitive = 0;
    uint32 HairVertexCount = 0;
    uint32 MaxVertexIndex = 0;
    bool bUseCulling = false;
    bool bWireframe = false;
    EPrimitiveIdMode PrimitiveIdMode = PrimID_Num;
    if (GeometryType == EHairGeometryType::Meshes)
    {
        if (!Instance->Meshes.IsValid(IntLODIndex))
        {
            UE_LOG(LogUnrealCV, Warning, TEXT("CreateMeshBatch: Meshes LOD %d is invalid for HairGroup %d"), IntLODIndex, GroupIndex);
            return nullptr;
        }
        VertexFactory = (FVertexFactory*)Instance->Meshes.LODs[IntLODIndex].GetVertexFactory();
        check(VertexFactory);
        PrimitiveIdMode = Instance->Meshes.LODs[IntLODIndex].GetVertexFactory()->GetPrimitiveIdMode(FeatureLevel);
        HairVertexCount = Instance->Meshes.LODs[IntLODIndex].RestResource->GetPrimitiveCount() * 3;
        MaxVertexIndex = HairVertexCount;
        NumPrimitive = HairVertexCount / 3;
        IndexBuffer = &Instance->Meshes.LODs[IntLODIndex].RestResource->IndexBuffer;
        bUseCulling = false;
        if (MaterialRenderProxy == nullptr)
        {
            MaterialRenderProxy = HairGroupMaterialProxies[GroupIndex].Meshes[IntLODIndex];
        }
        bWireframe = false && ViewFamily.EngineShowFlags.Wireframe;
    }
    else if (GeometryType == EHairGeometryType::Cards)
    {
        if (!Instance->Cards.IsValid(IntLODIndex))
        {
            UE_LOG(LogUnrealCV, Warning, TEXT("CreateMeshBatch: Cards LOD %d is invalid for HairGroup %d"), IntLODIndex, GroupIndex);
            return nullptr;
        }

        VertexFactory = (FVertexFactory*)Instance->Cards.LODs[IntLODIndex].GetVertexFactory();
        check(VertexFactory);
        PrimitiveIdMode = Instance->Meshes.LODs[IntLODIndex].GetVertexFactory()->GetPrimitiveIdMode(FeatureLevel);
        HairVertexCount = Instance->Cards.LODs[IntLODIndex].RestResource->GetPrimitiveCount() * 3;
        MaxVertexIndex = HairVertexCount;
        NumPrimitive = HairVertexCount / 3;
        IndexBuffer = &Instance->Cards.LODs[IntLODIndex].RestResource->RestIndexBuffer;
        bUseCulling = false;
        if (MaterialRenderProxy == nullptr)
        {
            MaterialRenderProxy = HairGroupMaterialProxies[GroupIndex].Cards[IntLODIndex];
        }
        bWireframe = false && ViewFamily.EngineShowFlags.Wireframe;
    }
    else // if (GeometryType == EHairGeometryType::Strands)
    {
        VertexFactory = (FVertexFactory*)Instance->Strands.VertexFactory;
        PrimitiveIdMode = Instance->Strands.VertexFactory->GetPrimitiveIdMode(FeatureLevel);
        HairVertexCount = Instance->HairGroupPublicData->GetActiveStrandsPointCount();
        MaxVertexIndex = HairVertexCount * GetPointToVertexCount();
        bUseCulling = Instance->Strands.bCullingEnable;
        NumPrimitive = bUseCulling ? 0 : HairVertexCount * HAIR_POINT_TO_TRIANGLE;
        if (MaterialRenderProxy == nullptr)
        {
            MaterialRenderProxy = HairGroupMaterialProxies[GroupIndex].Strands;
        }
        bWireframe = false && ViewFamily.EngineShowFlags.Wireframe;
    }

    if (MaterialRenderProxy == nullptr || !bIsVisible)
    {
        UE_LOG(LogUnrealCV, Warning, TEXT("CreateMeshBatch: MaterialRenderProxy=%p, bIsVisible=%d for HairGroup %d, GeometryType=%d"),
            MaterialRenderProxy, bIsVisible, GroupIndex, (int32)GeometryType);
        return nullptr;
    }

    // Invalid primitive setup. This can happens when the (procedural) resources are not ready.
    if (NumPrimitive == 0 && !bUseCulling)
    {
        UE_LOG(LogTemp, Error, TEXT("NumPrimitive is 0 and bUseCulling is false"));
        return nullptr;
    }

    // if (bWireframe)
    // {
    // 	FMaterialRenderProxy* ColoredMaterialRenderProxy = new FColoredMaterialRenderProxy( GEngine->WireframeMaterial ? GEngine->WireframeMaterial->GetRenderProxy() : NULL, FLinearColor(1.f, 0.5f, 0.f));
    // 	Collector.RegisterOneFrameMaterialProxy(ColoredMaterialRenderProxy);
    // 	MaterialRenderProxy = ColoredMaterialRenderProxy;
    // }

    // Draw the mesh.
    FMeshBatch& Mesh = Collector.AllocateMesh();

    const bool bUseCardsOrMeshes = GeometryType == EHairGeometryType::Cards || GeometryType == EHairGeometryType::Meshes;
    Mesh.CastShadow = bUseCardsOrMeshes;
#if RHI_RAYTRACING
    Mesh.CastRayTracedShadow = (MeshBatchType == EHairMeshBatchType::Raytracing || bUseCardsOrMeshes) && bCastDynamicShadow;
#endif
    Mesh.bUseForMaterial = MeshBatchType == EHairMeshBatchType::Raytracing || bUseCardsOrMeshes;
    Mesh.bUseForDepthPass = bUseCardsOrMeshes;
    Mesh.SegmentIndex = 0;
    #if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
    Mesh.VisualizeLODIndex = IntLODIndex;
    #endif

    FMeshBatchElement& BatchElement = Mesh.Elements[0];
    BatchElement.IndexBuffer = IndexBuffer;
    Mesh.bWireframe = bWireframe;
    Mesh.VertexFactory = VertexFactory;
    Mesh.MaterialRenderProxy = MaterialRenderProxy;
    bool bHasPrecomputedVolumetricLightmap;
    FMatrix PreviousLocalToWorld;
    int32 SingleCaptureIndex;
    bool bOutputVelocity = GeometryType == EHairGeometryType::Cards || GeometryType == EHairGeometryType::Meshes;

    FPrimitiveSceneInfo* PrimSceneInfo = GetPrimitiveSceneInfo();
    GetScene().GetPrimitiveUniformShaderParameters_RenderThread(PrimSceneInfo, bHasPrecomputedVolumetricLightmap, PreviousLocalToWorld, SingleCaptureIndex, bOutputVelocity);

    // if (GHairStrands_ForceVelocityOutput > 0)
    // {
    // 	bOutputVelocity = true;
    // }

    const bool bUseProxy = UseProxyLocalToWorld(Instance);

    FMatrix CurrentLocalToWorld = bUseProxy ? GetLocalToWorld() : Instance->GetCurrentLocalToWorld().ToMatrixWithScale();
    PreviousLocalToWorld = bUseProxy ? PreviousLocalToWorld : Instance->GetPreviousLocalToWorld().ToMatrixWithScale();

    // Band-aid to avoid invalid velociy vector when switching LOD skinned <-> rigid
    if (Instance->HairGroupPublicData->VFInput.bHasLODSwitch && Instance->HairGroupPublicData->VFInput.bHasLODSwitchBindingType)
    {
        PreviousLocalToWorld = CurrentLocalToWorld;
    }

    // Update primitive uniform buffer
    {
        // Use default SceneProxy builder values
        FPrimitiveUniformShaderParametersBuilder Builder;
        BuildUniformShaderParameters(Builder);

        // Override transforms and the local bound.
        // The original local bound relative to the component local to world transform. If we override the local to world transform, 
        // we need to recompute the local bound relative to this new transform. It is important that the new local bound is correct 
        // as otherwise the GPUScene (which use bot the local to world transform and the local bound for culling purpose) will issue 
        // incorrect visibility test.
        FBoxSphereBounds NewLocalBound = GetLocalBounds();
        if (!bUseProxy)
        {
            const FTransform InvLocalToWorld = Instance->GetCurrentLocalToWorld().Inverse();
            const FBoxSphereBounds OriginalWorldBound = GetBounds();
            NewLocalBound = OriginalWorldBound.TransformBy(InvLocalToWorld);
        }
        Builder
            .LocalToWorld(CurrentLocalToWorld)
            .PreviousLocalToWorld(PreviousLocalToWorld)
            .LocalBounds(NewLocalBound)
            .OutputVelocity(bOutputVelocity)
            .UseVolumetricLightmap(false);

        // Create primitive uniform buffer
        FRHICommandListBase& RHICmdList = Collector.GetRHICommandList();
        FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
        DynamicPrimitiveUniformBuffer.UniformBuffer.BufferUsage = UniformBuffer_SingleFrame;
        DynamicPrimitiveUniformBuffer.UniformBuffer.SetContents(RHICmdList, Builder.Build());
        DynamicPrimitiveUniformBuffer.UniformBuffer.InitResource(RHICmdList);
        BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer; // automatic copy to the gpu scene buffer
    }

    //primtiveid is set to 0
    BatchElement.FirstIndex = 0;
    BatchElement.NumInstances = 1;
    BatchElement.PrimitiveIdMode = PrimitiveIdMode;
    if (bUseCulling)
    {
        BatchElement.NumPrimitives = 0;
        BatchElement.IndirectArgsBuffer = bUseCulling ? Instance->HairGroupPublicData->GetDrawIndirectBuffer().Buffer->GetRHI() : nullptr;
        BatchElement.IndirectArgsOffset = 0;
    }
    else
    {
        BatchElement.NumPrimitives = NumPrimitive;
        BatchElement.IndirectArgsBuffer = nullptr;
        BatchElement.IndirectArgsOffset = 0;
    }

    // Setup our vertex factor custom data
    BatchElement.VertexFactoryUserData = const_cast<void*>(reinterpret_cast<const void*>(Instance->HairGroupPublicData));

    BatchElement.MinVertexIndex = 0;
    BatchElement.MaxVertexIndex = MaxVertexIndex;
    BatchElement.UserData = reinterpret_cast<void*>(uint64(ComponentId));
    Mesh.ReverseCulling = bUseCardsOrMeshes ? IsLocalToWorldDeterminantNegative() : false;
    Mesh.bDisableBackfaceCulling = GeometryType == EHairGeometryType::Strands;
    Mesh.Type = GetPrimitiveType(GeometryType);
    Mesh.DepthPriorityGroup = SDPG_World;
    Mesh.bCanApplyViewModeOverrides = false;
    Mesh.BatchHitProxyId = PrimSceneInfo->DefaultDynamicHitProxyId;

    return &Mesh;
}

FPrimitiveViewRelevance FHairStrandsSceneProxy::GetViewRelevance(const FSceneView* View) const
{
    // When path tracing is enabled force DrawRelevance if not visible in main view ('hidden in game'), 
    // but visible in shadow 'hidden shadow') so that raytracing geometry is created/updated correctly
    const bool bPathtracing = View->Family->EngineShowFlags.PathTracing;
    // const bool bIsShown = IsShown(View);
    const bool bIsShown = true;
    const bool bForceDrawRelevance = bPathtracing && (!bIsShown && (IsShadowCast(View) || bAffectIndirectLightingWhileHidden));
    // const bool bVisible = View->Family->EngineShowFlags.Hair;
    const bool bVisible = true;

    bool bUseCardsOrMesh = false;
    for (const TRefCountPtr<FHairGroupInstance>& Instance : HairGroupInstances)
    {
        check(Instance->GetRefCount());
        const EHairGeometryType GeometryType = Instance->GeometryType;
        bUseCardsOrMesh = bUseCardsOrMesh || GeometryType == EHairGeometryType::Cards || GeometryType == EHairGeometryType::Meshes;
    }

    FPrimitiveViewRelevance Result;

    // Special pass for hair strands geometry (not part of the base pass, and shadowing is handlded in a custom fashion). When cards rendering is enabled we reusethe base pass
    Result.bDrawRelevance		= bVisible && (bIsShown || bForceDrawRelevance);
    Result.bRenderInMainPass	= bUseCardsOrMesh && ShouldRenderInMainPass();
    Result.bShadowRelevance		= IsShadowCast(View);
    Result.bDynamicRelevance	= bUseCardsOrMesh;
    Result.bRenderCustomDepth	= ShouldRenderCustomDepth();
    Result.bVelocityRelevance	= Result.bRenderInMainPass && bUseCardsOrMesh;
    Result.bUsesLightingChannels= GetLightingChannelMask() != GetDefaultLightingChannelMask();

    // Selection only
    #if WITH_EDITOR
    {
        Result.bEditorStaticSelectionRelevance = true;
    }
    #endif
    MaterialRelevance.SetPrimitiveViewRelevance(Result);

    // Override the MaterialRelevance output
    Result.bHairStrands = bIsShown || bForceDrawRelevance;
    return Result;
}

uint32 FHairStrandsSceneProxy::GetMemoryFootprint(void) const  { return(sizeof(*this) + GetAllocatedSize()); }

uint32 FHairStrandsSceneProxy::GetAllocatedSize(void) const { return(FPrimitiveSceneProxy::GetAllocatedSize()); }

