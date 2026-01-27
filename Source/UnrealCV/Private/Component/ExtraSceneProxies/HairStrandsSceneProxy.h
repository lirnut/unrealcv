# pragma once
// from UE_5.6\Engine\Plugins\Runtime\HairStrands\Source\HairStrandsCore\Private\GroomComponent.cpp

#include "GroomComponent.h"
#include "HairStrandsInterface.h"
#include "RayTracingInstance.h"

enum class EHairMeshBatchType
{
	Raster,
	Raytracing
};
class FHairStrandsSceneProxy : public FPrimitiveSceneProxy
{
public:
	SIZE_T GetTypeHash() const override;

	FRenderCurveResourceData* CurveResourceData = nullptr;

	virtual FRenderCurveResourceData* GetRenderCurveResourceData() override;

	FHairStrandsSceneProxy(UGroomComponent* Component);

	virtual ~FHairStrandsSceneProxy();
	
	/**
	 *	Called when the rendering thread adds the proxy to the scene.
	 *	This function allows for generating renderer-side resources.
	 *	Called in the rendering thread.
	 */
	virtual void CreateRenderThreadResources(FRHICommandListBase& RHICmdList);

	/**
	 *	Called when the rendering thread removes the proxy from the scene.
	 *	This function allows for removing renderer-side resources.
	 *	Called in the rendering thread.
	 */
	virtual void DestroyRenderThreadResources() override;

	virtual void OnTransformChanged(FRHICommandListBase& RHICmdList) override;

	FORCEINLINE bool UseProxyLocalToWorld(const FHairGroupInstance* Instance) const;

#if RHI_RAYTRACING
	virtual bool IsRayTracingRelevant() const override { return true; }
	virtual bool IsRayTracingStaticRelevant() const override { return false; }
	virtual bool HasRayTracingRepresentation() const override { return true; }

	virtual void GetDynamicRayTracingInstances(FRayTracingInstanceCollector& Collector) override;
#endif

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;
	virtual FMeshBatch* CreateMeshBatch(
		const FSceneView* View,
		const FSceneViewFamily& ViewFamily,
		FMeshElementCollector& Collector,
		const EHairMeshBatchType MeshBatchType,
		const FHairGroupInstance* Instance,
		uint32 GroupIndex,
		FMaterialRenderProxy* Debug_MaterialProxy) const;

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;

	virtual uint32 GetMemoryFootprint(void) const override;

	uint32 GetAllocatedSize(void) const;

	TArray<TRefCountPtr<FHairGroupInstance>> HairGroupInstances;

	// Cache the material proxy to avoid race condition, when groom component's proxy is recreated, 
	// while another one is currently in flight for drawing.
	struct FHairGroupMaterialProxy
	{
		const FMaterialRenderProxy* Strands = nullptr;
		TArray<const FMaterialRenderProxy*> Cards;
		TArray<const FMaterialRenderProxy*> Meshes;
	};
	TArray<FHairGroupMaterialProxy> HairGroupMaterialProxies;
	
private:
	uint32 ComponentId = 0;
	FMaterialRelevance MaterialRelevance;
	UMaterialInterface* Strands_DebugMaterial = nullptr;
};