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

private:
	TArray<TRefCountPtr<FHairGroupInstance>> HairGroupInstances;
	const FMaterialRenderProxy* MaterialRenderProxy;
	uint32 ComponentId;
	TWeakObjectPtr<UGroomComponent> GroomComponentWeak;
};
