#pragma once

#include "CoreMinimal.h"
#include "PrimitiveSceneProxy.h"
#include "GroomComponent.h"

class FGroomAnnotationSceneProxy : public FPrimitiveSceneProxy
{
public:
	FGroomAnnotationSceneProxy(UGroomComponent* Component, UMaterialInterface* AnnotationMID);
	virtual ~FGroomAnnotationSceneProxy();

	virtual void GetDynamicMeshElements(
		const TArray<const FSceneView*>& Views,
		const FSceneViewFamily& ViewFamily,
		uint32 VisibilityMap,
		FMeshElementCollector& Collector) const override;

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	virtual uint32 GetMemoryFootprint() const override { return sizeof(*this) + GetAllocatedSize(); }
	virtual SIZE_T GetTypeHash() const override { return GetTypeHashHelper(this); }
	uint32 GetAllocatedSize() const { return FPrimitiveSceneProxy::GetAllocatedSize(); }

#if RHI_RAYTRACING
	virtual bool HasRayTracingRepresentation() const override { return false; }
	virtual bool IsRayTracingRelevant() const override { return false; }
	virtual bool IsRayTracingStaticRelevant() const override { return false; }
#endif // RHI_RAYTRACING

private:
	TArray<TRefCountPtr<FHairGroupInstance>> HairGroupInstances;
	const FMaterialRenderProxy* MaterialRenderProxy;
	uint32 ComponentId;
	TWeakObjectPtr<UGroomComponent> GroomComponentWeak;
};
