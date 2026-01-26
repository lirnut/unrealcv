// #pragma once

// #include "CoreMinimal.h"
// #include "PrimitiveSceneProxy.h"
// #include "GroomComponent.h"
// #include "HairStrandsInterface.h"
// #include "Materials/MaterialRenderProxy.h"

// class FGroomAnnotationSceneProxy : public FPrimitiveSceneProxy
// {
// public:
// 	FGroomAnnotationSceneProxy(UGroomComponent* Component, UMaterialInterface* AnnotationMID);
// 	virtual ~FGroomAnnotationSceneProxy();

// 	virtual SIZE_T GetTypeHash() const override;
// 	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;
// 	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
// 	virtual uint32 GetMemoryFootprint() const override;

// #if RHI_RAYTRACING
// 	virtual bool IsRayTracingRelevant() const override { return true; }
// 	virtual bool HasRayTracingRepresentation() const override { return true; }
// 	virtual void GetDynamicRayTracingInstances(FRayTracingInstanceCollector& Collector) override;
// #endif

// private:
// 	FMaterialRenderProxy* AnnotationMaterialRenderProxy;
// 	FMaterialRelevance MaterialRelevance;

// 	TArray<TRefCountPtr<FHairGroupInstance>> HairGroupInstances;

// 	struct FHairGroupMaterialProxy
// 	{
// 		const FMaterialRenderProxy* Strands = nullptr;
// 		TArray<const FMaterialRenderProxy*> Cards;
// 		TArray<const FMaterialRenderProxy*> Meshes;
// 	};
// 	TArray<FHairGroupMaterialProxy> HairGroupMaterialProxies;

// 	uint32 ComponentId;

// 	FMeshBatch* CreateMeshBatch(
// 		const FSceneView* View,
// 		const FSceneViewFamily& ViewFamily,
// 		FMeshElementCollector& Collector,
// 		const FHairGroupInstance* Instance,
// 		uint32 GroupIndex) const;

// 	bool UseProxyLocalToWorld(const FHairGroupInstance* Instance) const;
// };
