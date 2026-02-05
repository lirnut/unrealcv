#include "HairStrandsSceneProxy.h"
#include "GroomComponent.h"
#include "GroomInstance.h"
#include "HairStrandsVertexFactory.h"
#include "MeshBatch.h"
#include "SceneManagement.h"
#include "PrimitiveUniformShaderParametersBuilder.h"
#include "Materials/MaterialRenderProxy.h"
#include "HairStrandsInterface.h"
#include "UnrealcvLog.h"

static uint32 GetPointToVertexCount_UnrealCV()
{
	return GetHairStrandsUsesTriangleStrips() ? 4 : 6;
}

static constexpr uint32 HAIR_POINT_TO_TRIANGLE_UnrealCV = 2;

FGroomAnnotationSceneProxy::FGroomAnnotationSceneProxy(UGroomComponent* Component, UMaterialInterface* AnnotationMID)
	: FPrimitiveSceneProxy(Component)
	, GroomComponentWeak(Component)
{
	bVFRequiresPrimitiveUniformBuffer = true;
	ComponentId = Component->GetPrimitiveSceneId().PrimIDValue;

	if (Component && Component->GroomAsset)
	{
		const uint32 GroupCount = Component->GetGroupCount();
		HairGroupInstances.Reserve(GroupCount);
		for (uint32 GroupIt = 0; GroupIt < GroupCount; ++GroupIt)
		{
			if (FHairGroupInstance* Instance = Component->GetGroupInstance(GroupIt))
			{
				HairGroupInstances.Add(Instance);
				// UE_LOG(LogUnrealCV, Warning, TEXT("FGroomAnnotationSceneProxy: Added Instance %d, Ptr=%p, RefCount=%d, BindingType=%d"),
				// 	GroupIt, Instance, Instance->GetRefCount(), (int32)Instance->BindingType);
			}
		}
		MaterialRenderProxy = AnnotationMID->GetRenderProxy();
	}

#if WITH_EDITOR
	TArray<UMaterialInterface*> UsedMaterials;
	UsedMaterials.Add(AnnotationMID);
	SetUsedMaterialForVerification(UsedMaterials);
#endif
}

FGroomAnnotationSceneProxy::~FGroomAnnotationSceneProxy()
{
}

void FGroomAnnotationSceneProxy::GetDynamicMeshElements(
	const TArray<const FSceneView*>& Views,
	const FSceneViewFamily& ViewFamily,
	uint32 VisibilityMap,
	FMeshElementCollector& Collector) const
{
	if (HairGroupInstances.Num() == 0 || !MaterialRenderProxy)
	{
		return;
	}

	const EShaderPlatform Platform = ViewFamily.GetShaderPlatform();
	if (!IsHairStrandsEnabled(EHairStrandsShaderType::Strands, Platform))
	{
		return;
	}

	UGroomComponent* GroomComp = GroomComponentWeak.Get();
	if (!GroomComp)
	{
		return;
	}

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FSceneView* View = Views[ViewIndex];

		if ((IsShown(View) || IsShadowCast(View)) && (VisibilityMap & (1 << ViewIndex)))
		{
			for (int32 GroupIt = 0; GroupIt < HairGroupInstances.Num(); ++GroupIt)
			{
				FHairGroupInstance* Instance = GroomComp->GetGroupInstance(GroupIt);
				if (!Instance || !Instance->Strands.IsValid() || !Instance->Strands.VertexFactory)
				{
					continue;
				}

				const int32 IntLODIndex = Instance->HairGroupPublicData->GetIntLODIndex();
				const bool bIsVisible = Instance->HairGroupPublicData->GetLODVisibility();
				if (!bIsVisible)
				{
					continue;
				}

				FMeshBatch& Mesh = Collector.AllocateMesh();

				Mesh.VertexFactory = Instance->Strands.VertexFactory;
				Mesh.MaterialRenderProxy = MaterialRenderProxy;

				Mesh.CastShadow = false;
				Mesh.bUseForMaterial = true;
				Mesh.bUseForDepthPass = false;
				Mesh.bDisableBackfaceCulling = true;
				Mesh.Type = PT_TriangleList;
				Mesh.DepthPriorityGroup = SDPG_World;
				Mesh.bCanApplyViewModeOverrides = false;
				Mesh.ReverseCulling = false;

				FMeshBatchElement& BatchElement = Mesh.Elements[0];

				const uint32 HairVertexCount = Instance->HairGroupPublicData->GetActiveStrandsPointCount();
				const bool bUseCulling = Instance->Strands.bCullingEnable;

				if (bUseCulling)
				{
					BatchElement.NumPrimitives = 0;
					BatchElement.IndirectArgsBuffer = Instance->HairGroupPublicData->GetDrawIndirectBuffer().Buffer->GetRHI();
					BatchElement.IndirectArgsOffset = 0;
				}
				else
				{
					BatchElement.NumPrimitives = HairVertexCount * HAIR_POINT_TO_TRIANGLE_UnrealCV;
					BatchElement.IndirectArgsBuffer = nullptr;
					BatchElement.IndirectArgsOffset = 0;
				}

				BatchElement.FirstIndex = 0;
				BatchElement.NumInstances = 1;
				BatchElement.MinVertexIndex = 0;
				BatchElement.MaxVertexIndex = HairVertexCount * GetPointToVertexCount_UnrealCV();
				BatchElement.PrimitiveIdMode = PrimID_ForceZero;

				BatchElement.VertexFactoryUserData = const_cast<void*>(
					reinterpret_cast<const void*>(Instance->HairGroupPublicData)
				);
				BatchElement.UserData = reinterpret_cast<void*>(uint64(ComponentId));

				{
					FPrimitiveUniformShaderParametersBuilder Builder;
					BuildUniformShaderParameters(Builder);

					const bool bUseSkinning = (Instance->BindingType == EHairBindingType::Skinning);
					FMatrix CurrentLocalToWorld = Instance->GetCurrentLocalToWorld().ToMatrixWithScale();
					FMatrix PreviousLocalToWorld = Instance->GetPreviousLocalToWorld().ToMatrixWithScale();

					// FixMe: the Transform of Groom Annotation wil not be updated when the original GroomComponent can not be seen

					// static int32 FrameCounter = 0;
					// if (FrameCounter++ % 60 == 0)
					// {
					// 	UE_LOG(LogUnrealCV, Warning, TEXT("GroomAnnotation Frame %d: BindingType=%d, Instance=%p, CurrentLocalToWorld Origin=(%f,%f,%f)"),
					// 		FrameCounter,
					// 		(int32)Instance->BindingType,
					// 		Instance,
					// 		CurrentLocalToWorld.GetOrigin().X,
					// 		CurrentLocalToWorld.GetOrigin().Y,
					// 		CurrentLocalToWorld.GetOrigin().Z);
					// }

					FBoxSphereBounds NewLocalBound = GetLocalBounds();
					if (bUseSkinning)
					{
						const FTransform InvLocalToWorld = Instance->GetCurrentLocalToWorld().Inverse();
						const FBoxSphereBounds OriginalWorldBound = GetBounds();
						NewLocalBound = OriginalWorldBound.TransformBy(InvLocalToWorld);
					}

					Builder
						.LocalToWorld(CurrentLocalToWorld)
						.PreviousLocalToWorld(PreviousLocalToWorld)
						.LocalBounds(NewLocalBound)
						.OutputVelocity(false)
						.UseVolumetricLightmap(false);

					FRHICommandListBase& RHICmdList = Collector.GetRHICommandList();
					FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer =
						Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
					DynamicPrimitiveUniformBuffer.UniformBuffer.BufferUsage = UniformBuffer_SingleFrame;
					DynamicPrimitiveUniformBuffer.UniformBuffer.SetContents(RHICmdList, Builder.Build());
					DynamicPrimitiveUniformBuffer.UniformBuffer.InitResource(RHICmdList);
					BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;
				}

				Collector.AddMesh(ViewIndex, Mesh);
			}
		}
	}
}

FPrimitiveViewRelevance FGroomAnnotationSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	// if (!View->Family->EngineShowFlags.Materials && !View->Family->EngineShowFlags.PostProcessing)
	if ( !View->Family->EngineShowFlags.Materials )
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bRenderInMainPass = ShouldRenderInMainPass();
		Result.bShadowRelevance = false;
		Result.bDynamicRelevance = true;
		Result.bRenderCustomDepth = false;
		Result.bUsesLightingChannels = false;
		return Result;
	}
	else
	{
		FPrimitiveViewRelevance ViewRelevance;
		ViewRelevance.bDrawRelevance = 0;
		return ViewRelevance;
	}
}
