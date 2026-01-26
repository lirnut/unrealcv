// #include "GroomAnnotationSceneProxy.h"
// #include "GroomComponent.h"
// #include "HairStrandsInterface.h"
// #include "HairCardsVertexFactory.h"
// #include "Materials/Material.h"
// #include "Materials/MaterialRenderProxy.h"
// #include "PrimitiveUniformShaderParametersBuilder.h"
// #include "SceneManagement.h"
// #include "Engine/Engine.h"

// #if RHI_RAYTRACING
// #include "RayTracingInstance.h"
// #endif

// static inline int32 GetMaterialIndexWithFallback(int32 SlotIndex)
// {
// 	return SlotIndex != INDEX_NONE ? SlotIndex : 0;
// }

// static uint32 GetPointToVertexCount()
// {
// 	return GetHairStrandsUsesTriangleStrips() ? HAIR_POINT_TO_VERTEX_FOR_TRISTRP : HAIR_POINT_TO_VERTEX_FOR_TRILIST;
// }

// static EPrimitiveType GetPrimitiveType(EHairGeometryType In)
// {
// 	return (In == EHairGeometryType::Strands && GetHairStrandsUsesTriangleStrips()) ? PT_TriangleStrip : PT_TriangleList;
// }

// FGroomAnnotationSceneProxy::FGroomAnnotationSceneProxy(UGroomComponent* Component, UMaterialInterface* AnnotationMID)
// 	: FPrimitiveSceneProxy(Component)
// 	, AnnotationMaterialRenderProxy(AnnotationMID ? AnnotationMID->GetRenderProxy() : nullptr)
// 	, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
// {
// 	bVFRequiresPrimitiveUniformBuffer = true;
// 	bCastDeepShadow = false;
// 	bCastDynamicShadow = false;

// 	if (!Component || !Component->GroomAsset || Component->GroomAsset->GetNumHairGroups() == 0)
// 	{
// 		return;
// 	}

// 	ComponentId = Component->GetPrimitiveSceneId().PrimIDValue;

// 	const int32 GroupCount = Component->GroomAsset->GetNumHairGroups();
// 	HairGroupInstances.SetNum(GroupCount);
// 	HairGroupMaterialProxies.SetNum(GroupCount);

// 	for (int32 GroupIt = 0; GroupIt < GroupCount; ++GroupIt)
// 	{
// 		FHairGroupInstance* HairInstance = Component->GetGroupInstance(GroupIt);
// 		if (!HairInstance || !HairInstance->HairGroupPublicData)
// 			continue;

// 		HairGroupInstances[GroupIt] = HairInstance;

// 		const FHairGroupPlatformData& InGroupData = Component->GroomAsset->GetHairGroupsPlatformData()[GroupIt];

// 		HairGroupMaterialProxies[GroupIt].Strands = AnnotationMaterialRenderProxy;

// 		HairGroupMaterialProxies[GroupIt].Cards.Init(AnnotationMaterialRenderProxy, InGroupData.Cards.LODs.Num());
// 		HairGroupMaterialProxies[GroupIt].Meshes.Init(AnnotationMaterialRenderProxy, InGroupData.Meshes.LODs.Num());
// 	}
// }

// FGroomAnnotationSceneProxy::~FGroomAnnotationSceneProxy()
// {
// }

// SIZE_T FGroomAnnotationSceneProxy::GetTypeHash() const
// {
// 	static size_t UniquePointer;
// 	return reinterpret_cast<size_t>(&UniquePointer);
// }

// bool FGroomAnnotationSceneProxy::UseProxyLocalToWorld(const FHairGroupInstance* Instance) const
// {
// 	return (Instance->BindingType != EHairBindingType::Skinning);
// }

// void FGroomAnnotationSceneProxy::GetDynamicMeshElements(
// 	const TArray<const FSceneView*>& Views,
// 	const FSceneViewFamily& ViewFamily,
// 	uint32 VisibilityMap,
// 	FMeshElementCollector& Collector) const
// {
// 	const EShaderPlatform Platform = ViewFamily.GetShaderPlatform();
// 	if (!IsHairStrandsEnabled(EHairStrandsShaderType::Strands, Platform) &&
// 		!IsHairStrandsEnabled(EHairStrandsShaderType::Cards, Platform) &&
// 		!IsHairStrandsEnabled(EHairStrandsShaderType::Meshes, Platform))
// 	{
// 		return;
// 	}

// 	if (HairGroupInstances.Num() == 0)
// 	{
// 		return;
// 	}

// 	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
// 	{
// 		const FSceneView* View = Views[ViewIndex];
// 		if (View->bIsReflectionCapture)
// 		{
// 			continue;
// 		}

// 		if ((IsShadowCast(View) || IsShown(View)) && (VisibilityMap & (1 << ViewIndex)))
// 		{
// 			for (uint32 GroupIt = 0; GroupIt < (uint32)HairGroupInstances.Num(); ++GroupIt)
// 			{
// 				FHairGroupInstance* Instance = HairGroupInstances[GroupIt];
// 				if (!Instance || Instance->GetRefCount() <= 0)
// 					continue;

// 				if (FMeshBatch* MeshBatch = CreateMeshBatch(View, ViewFamily, Collector, Instance, GroupIt))
// 				{
// 					Collector.AddMesh(ViewIndex, *MeshBatch);
// 				}
// 			}
// 		}
// 	}
// }

// FMeshBatch* FGroomAnnotationSceneProxy::CreateMeshBatch(
// 	const FSceneView* View,
// 	const FSceneViewFamily& ViewFamily,
// 	FMeshElementCollector& Collector,
// 	const FHairGroupInstance* Instance,
// 	uint32 GroupIndex) const
// {
// 	const EHairGeometryType GeometryType = Instance->GeometryType;
// 	if (GeometryType == EHairGeometryType::NoneGeometry)
// 	{
// 		return nullptr;
// 	}

// 	if (Instance->GetRefCount() <= 0)
// 	{
// 		return nullptr;
// 	}

// 	const int32 IntLODIndex = Instance->HairGroupPublicData->GetIntLODIndex();
// 	const bool bIsVisible = Instance->HairGroupPublicData->GetLODVisibility();

// 	if (!bIsVisible)
// 	{
// 		return nullptr;
// 	}

// 	const FVertexFactory* VertexFactory = nullptr;
// 	FIndexBuffer* IndexBuffer = nullptr;
// 	const FMaterialRenderProxy* MaterialRenderProxy = AnnotationMaterialRenderProxy;
// 	const ERHIFeatureLevel::Type FeatureLevel = View->GetFeatureLevel();

// 	uint32 NumPrimitive = 0;
// 	uint32 HairVertexCount = 0;
// 	uint32 MaxVertexIndex = 0;
// 	bool bUseCulling = false;
// 	EPrimitiveIdMode PrimitiveIdMode = PrimID_Num;

// 	if (GeometryType == EHairGeometryType::Meshes)
// 	{
// 		if (!Instance->Meshes.IsValid(IntLODIndex))
// 		{
// 			return nullptr;
// 		}

// 		auto* MeshVertexFactory = Instance->Meshes.LODs[IntLODIndex].GetVertexFactory();
// 		if (!MeshVertexFactory)
// 			return nullptr;

// 		VertexFactory = (FVertexFactory*)MeshVertexFactory;
// 		PrimitiveIdMode = MeshVertexFactory->GetPrimitiveIdMode(FeatureLevel);
// 		HairVertexCount = Instance->Meshes.LODs[IntLODIndex].RestResource->GetPrimitiveCount() * 3;
// 		MaxVertexIndex = HairVertexCount;
// 		NumPrimitive = HairVertexCount / 3;
// 		IndexBuffer = &Instance->Meshes.LODs[IntLODIndex].RestResource->IndexBuffer;
// 		bUseCulling = false;

// 		if (GroupIndex < (uint32)HairGroupMaterialProxies.Num() && IntLODIndex < HairGroupMaterialProxies[GroupIndex].Meshes.Num())
// 		{
// 			MaterialRenderProxy = HairGroupMaterialProxies[GroupIndex].Meshes[IntLODIndex];
// 		}
// 	}
// 	else if (GeometryType == EHairGeometryType::Cards)
// 	{
// 		if (!Instance->Cards.IsValid(IntLODIndex))
// 		{
// 			return nullptr;
// 		}

// 		auto* CardsVertexFactory = Instance->Cards.LODs[IntLODIndex].GetVertexFactory();
// 		if (!CardsVertexFactory)
// 			return nullptr;

// 		VertexFactory = (FVertexFactory*)CardsVertexFactory;
// 		PrimitiveIdMode = CardsVertexFactory->GetPrimitiveIdMode(FeatureLevel);
// 		HairVertexCount = Instance->Cards.LODs[IntLODIndex].RestResource->GetPrimitiveCount() * 3;
// 		MaxVertexIndex = HairVertexCount;
// 		NumPrimitive = HairVertexCount / 3;
// 		IndexBuffer = &Instance->Cards.LODs[IntLODIndex].RestResource->RestIndexBuffer;
// 		bUseCulling = false;

// 		if (GroupIndex < (uint32)HairGroupMaterialProxies.Num() && IntLODIndex < HairGroupMaterialProxies[GroupIndex].Cards.Num())
// 		{
// 			MaterialRenderProxy = HairGroupMaterialProxies[GroupIndex].Cards[IntLODIndex];
// 		}
// 	}
// 	else
// 	{
// 		if (!Instance->Strands.VertexFactory)
// 			return nullptr;

// 		VertexFactory = (FVertexFactory*)Instance->Strands.VertexFactory;
// 		PrimitiveIdMode = Instance->Strands.VertexFactory->GetPrimitiveIdMode(FeatureLevel);
// 		HairVertexCount = Instance->HairGroupPublicData->GetActiveStrandsPointCount();
// 		MaxVertexIndex = HairVertexCount * GetPointToVertexCount();
// 		bUseCulling = Instance->Strands.bCullingEnable;
// 		NumPrimitive = bUseCulling ? 0 : HairVertexCount * HAIR_POINT_TO_TRIANGLE;

// 		if (GroupIndex < (uint32)HairGroupMaterialProxies.Num())
// 		{
// 			MaterialRenderProxy = HairGroupMaterialProxies[GroupIndex].Strands;
// 		}
// 	}

// 	if (!MaterialRenderProxy)
// 	{
// 		return nullptr;
// 	}

// 	if (NumPrimitive == 0 && !bUseCulling)
// 	{
// 		return nullptr;
// 	}

// 	FMeshBatch& Mesh = Collector.AllocateMesh();

// 	const bool bUseCardsOrMeshes = GeometryType == EHairGeometryType::Cards || GeometryType == EHairGeometryType::Meshes;
// 	Mesh.CastShadow = false;
// #if RHI_RAYTRACING
// 	Mesh.CastRayTracedShadow = false;
// #endif
// 	Mesh.bUseForMaterial = bUseCardsOrMeshes;
// 	Mesh.bUseForDepthPass = bUseCardsOrMeshes;
// 	Mesh.SegmentIndex = 0;

// 	FMeshBatchElement& BatchElement = Mesh.Elements[0];
// 	BatchElement.IndexBuffer = IndexBuffer;
// 	Mesh.bWireframe = false;
// 	Mesh.VertexFactory = VertexFactory;
// 	Mesh.MaterialRenderProxy = MaterialRenderProxy;

// 	bool bHasPrecomputedVolumetricLightmap;
// 	FMatrix PreviousLocalToWorld;
// 	int32 SingleCaptureIndex;
// 	bool bOutputVelocity = bUseCardsOrMeshes;

// 	FPrimitiveSceneInfo* PrimSceneInfo = GetPrimitiveSceneInfo();
// 	GetScene().GetPrimitiveUniformShaderParameters_RenderThread(PrimSceneInfo, bHasPrecomputedVolumetricLightmap, PreviousLocalToWorld, SingleCaptureIndex, bOutputVelocity);

// 	const bool bUseProxy = UseProxyLocalToWorld(Instance);
// 	FMatrix CurrentLocalToWorld = bUseProxy ? GetLocalToWorld() : Instance->GetCurrentLocalToWorld().ToMatrixWithScale();
// 	PreviousLocalToWorld = bUseProxy ? PreviousLocalToWorld : Instance->GetPreviousLocalToWorld().ToMatrixWithScale();

// 	if (Instance->HairGroupPublicData->VFInput.bHasLODSwitch && Instance->HairGroupPublicData->VFInput.bHasLODSwitchBindingType)
// 	{
// 		PreviousLocalToWorld = CurrentLocalToWorld;
// 	}

// 	{
// 		FPrimitiveUniformShaderParametersBuilder Builder;
// 		BuildUniformShaderParameters(Builder);

// 		FBoxSphereBounds NewLocalBound = GetLocalBounds();
// 		if (!bUseProxy)
// 		{
// 			const FTransform InvLocalToWorld = Instance->GetCurrentLocalToWorld().Inverse();
// 			const FBoxSphereBounds OriginalWorldBound = GetBounds();
// 			NewLocalBound = OriginalWorldBound.TransformBy(InvLocalToWorld);
// 		}

// 		Builder
// 			.LocalToWorld(CurrentLocalToWorld)
// 			.PreviousLocalToWorld(PreviousLocalToWorld)
// 			.LocalBounds(NewLocalBound)
// 			.OutputVelocity(bOutputVelocity)
// 			.UseVolumetricLightmap(false);

// 		FRHICommandListBase& RHICmdList = Collector.GetRHICommandList();
// 		FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
// 		DynamicPrimitiveUniformBuffer.UniformBuffer.BufferUsage = UniformBuffer_SingleFrame;
// 		DynamicPrimitiveUniformBuffer.UniformBuffer.SetContents(RHICmdList, Builder.Build());
// 		DynamicPrimitiveUniformBuffer.UniformBuffer.InitResource(RHICmdList);
// 		BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;
// 	}

// 	BatchElement.FirstIndex = 0;
// 	BatchElement.NumInstances = 1;
// 	BatchElement.PrimitiveIdMode = PrimitiveIdMode;

// 	if (bUseCulling)
// 	{
// 		BatchElement.NumPrimitives = 0;
// 		BatchElement.IndirectArgsBuffer = Instance->HairGroupPublicData->GetDrawIndirectBuffer().Buffer->GetRHI();
// 		BatchElement.IndirectArgsOffset = 0;
// 	}
// 	else
// 	{
// 		BatchElement.NumPrimitives = NumPrimitive;
// 		BatchElement.IndirectArgsBuffer = nullptr;
// 		BatchElement.IndirectArgsOffset = 0;
// 	}

// 	BatchElement.VertexFactoryUserData = const_cast<void*>(reinterpret_cast<const void*>(Instance->HairGroupPublicData));
// 	BatchElement.MinVertexIndex = 0;
// 	BatchElement.MaxVertexIndex = MaxVertexIndex;
// 	BatchElement.UserData = reinterpret_cast<void*>(uint64(ComponentId));

// 	Mesh.ReverseCulling = bUseCardsOrMeshes ? IsLocalToWorldDeterminantNegative() : false;
// 	Mesh.bDisableBackfaceCulling = GeometryType == EHairGeometryType::Strands;
// 	Mesh.Type = GetPrimitiveType(GeometryType);
// 	Mesh.DepthPriorityGroup = SDPG_World;
// 	Mesh.bCanApplyViewModeOverrides = false;
// 	Mesh.BatchHitProxyId = PrimSceneInfo->DefaultDynamicHitProxyId;

// 	return &Mesh;
// }

// FPrimitiveViewRelevance FGroomAnnotationSceneProxy::GetViewRelevance(const FSceneView* View) const
// {
// 	if (!View->Family->EngineShowFlags.Materials && !View->Family->EngineShowFlags.PostProcessing)
// 	{
// 		bool bUseCardsOrMesh = false;
// 		for (const TRefCountPtr<FHairGroupInstance>& Instance : HairGroupInstances)
// 		{
// 			if (Instance && Instance->GetRefCount() > 0)
// 			{
// 				const EHairGeometryType GeometryType = Instance->GeometryType;
// 				bUseCardsOrMesh = bUseCardsOrMesh || GeometryType == EHairGeometryType::Cards || GeometryType == EHairGeometryType::Meshes;
// 			}
// 		}

// 		const bool bVisible = View->Family->EngineShowFlags.Hair;
// 		const bool bIsShown = IsShown(View);

// 		FPrimitiveViewRelevance Result;
// 		Result.bDrawRelevance = bVisible && bIsShown;
// 		Result.bRenderInMainPass = bUseCardsOrMesh && ShouldRenderInMainPass();
// 		Result.bShadowRelevance = false;
// 		Result.bDynamicRelevance = bUseCardsOrMesh;
// 		Result.bRenderCustomDepth = ShouldRenderCustomDepth();
// 		Result.bVelocityRelevance = Result.bRenderInMainPass && bUseCardsOrMesh;
// 		Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();

// 		MaterialRelevance.SetPrimitiveViewRelevance(Result);
// 		Result.bHairStrands = bIsShown;

// 		return Result;
// 	}
// 	else
// 	{
// 		FPrimitiveViewRelevance ViewRelevance;
// 		ViewRelevance.bDrawRelevance = 0;
// 		return ViewRelevance;
// 	}
// }

// uint32 FGroomAnnotationSceneProxy::GetMemoryFootprint() const
// {
// 	return sizeof(*this) + GetAllocatedSize();
// }

// #if RHI_RAYTRACING
// void FGroomAnnotationSceneProxy::GetDynamicRayTracingInstances(FRayTracingInstanceCollector& Collector)
// {
// 	if (!IsHairRayTracingEnabled() || HairGroupInstances.Num() == 0)
// 		return;

// 	const EShaderPlatform Platform = Collector.GetReferenceView()->GetShaderPlatform();
// 	if (!IsHairStrandsEnabled(EHairStrandsShaderType::Strands, Platform) &&
// 		!IsHairStrandsEnabled(EHairStrandsShaderType::Cards, Platform) &&
// 		!IsHairStrandsEnabled(EHairStrandsShaderType::Meshes, Platform))
// 	{
// 		return;
// 	}

// 	const bool bWireframe = AllowDebugViewmodes() && Collector.GetReferenceView()->Family->EngineShowFlags.Wireframe;
// 	const EHairViewRayTracingMask ViewRayTracingMask = Collector.GetReferenceView()->Family->EngineShowFlags.PathTracing ? EHairViewRayTracingMask::PathTracing : EHairViewRayTracingMask::RayTracing;
// 	if (bWireframe)
// 		return;

// 	for (uint32 GroupIt = 0; GroupIt < (uint32)HairGroupInstances.Num(); ++GroupIt)
// 	{
// 		FHairGroupInstance* Instance = HairGroupInstances[GroupIt];
// 		if (!Instance || Instance->GetRefCount() <= 0)
// 			continue;

// 		FMatrix OverrideLocalToWorld = UseProxyLocalToWorld(Instance) ? GetLocalToWorld() : Instance->GetCurrentLocalToWorld().ToMatrixWithScale();

// 		const EHairGeometryType GeometryType = Instance->HairGroupPublicData->VFInput.GeometryType;
// 		const uint32 LODIndex = Instance->HairGroupPublicData->GetIntLODIndex();

// 		FHairStrandsRaytracingResource* RTGeometry = nullptr;
// 		bool bIsHairStrands = false;
// 		EHairViewRayTracingMask InstanceViewRayTracingMask = EHairViewRayTracingMask::PathTracing | EHairViewRayTracingMask::RayTracing;

// 		switch (GeometryType)
// 		{
// 		case EHairGeometryType::Strands:
// 			RTGeometry = Instance->Strands.RenRaytracingResource;
// 			InstanceViewRayTracingMask = Instance->Strands.ViewRayTracingMask;
// 			bIsHairStrands = true;
// 			break;
// 		case EHairGeometryType::Cards:
// 			if (LODIndex < (uint32)Instance->Cards.LODs.Num())
// 				RTGeometry = Instance->Cards.LODs[LODIndex].RaytracingResource;
// 			break;
// 		case EHairGeometryType::Meshes:
// 			if (LODIndex < (uint32)Instance->Meshes.LODs.Num())
// 				RTGeometry = Instance->Meshes.LODs[LODIndex].RaytracingResource;
// 			break;
// 		}

// 		if (!EnumHasAnyFlags(InstanceViewRayTracingMask, ViewRayTracingMask))
// 		{
// 			continue;
// 		}

// 		if (RTGeometry && RTGeometry->RayTracingGeometry.IsValid())
// 		{
// 			if (FMeshBatch* MeshBatch = CreateMeshBatch(Collector.GetReferenceView(), *Collector.GetReferenceView()->Family, Collector, Instance, GroupIt))
// 			{
// 				FRayTracingInstance RayTracingInstance;
// 				RayTracingInstance.Geometry = &RTGeometry->RayTracingGeometry;
// 				RayTracingInstance.Materials.Add(*MeshBatch);
// 				RayTracingInstance.InstanceTransforms.Add(OverrideLocalToWorld);
// 				RayTracingInstance.bThinGeometry = bIsHairStrands;

// 				Collector.AddRayTracingInstance(MoveTemp(RayTracingInstance));

// 				if (RTGeometry->PositionBuffer.Buffer)
// 				{
// 					Collector.AddRDGPooledBuffer(RTGeometry->PositionBuffer.Buffer);
// 				}
// 				if (RTGeometry->IndexBuffer.Buffer)
// 				{
// 					Collector.AddRDGPooledBuffer(RTGeometry->IndexBuffer.Buffer);
// 				}
// 			}
// 		}
// 	}
// }
// #endif
