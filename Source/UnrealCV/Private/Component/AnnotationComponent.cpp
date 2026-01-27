#include "AnnotationComponent.h"

#include "Runtime/CoreUObject/Public/UObject/ConstructorHelpers.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Runtime/Engine/Classes/Engine/StaticMesh.h"
#include "Runtime/Engine/Classes/Components/SkeletalMeshComponent.h"
#include "Runtime/Engine/Classes/Components/InstancedStaticMeshComponent.h"
#include "Runtime/Engine/Classes/Engine/InstancedStaticMesh.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Runtime/Engine/Public/MaterialShared.h"
#include "Runtime/Engine/Classes/Engine/Engine.h"

#if ENGINE_MAJOR_VERSION >= 5
#include "Runtime/Engine/Public/StaticMeshSceneProxy.h"
#include "Runtime/Engine/Public/SkeletalMeshSceneProxy.h"
#include "InstancedStaticMeshSceneProxyDesc.h"

#endif
#include "Runtime/Engine/Public/Rendering/SkeletalMeshRenderData.h"
#include "SkinnedMeshSceneProxyDesc.h"
#include "UnrealcvLog.h"

#include "GroomComponent.h"
#include "ExtraSceneProxies/HairStrandsSceneProxy.h"





// Note: For UE4 < 19
// Note: check https://github.com/unrealcv/unrealcv/blob/1369a72be8428547318d8a52ae2d63e1eb57a001/Source/UnrealCV/Private/Component/AnnotationComponent.cpp#L11

/** Store mesh data of a parent mesh component */

/*
enum class EParentMeshType
{
	None,
	StaticMesh,
	SkelMesh,
};
*/
/*
class FParentMeshInfo
{
public:

	FParentMeshInfo(USceneComponent* ParentComponent)
	{
		ParentMeshType = EParentMeshType::None;

		if (!IsValid(ParentComponent))
		{ 
			UE_LOG(LogTemp, Warning, TEXT("ParentComponent is invalid."));
			return;
		}

		UStaticMeshComponent* InStaticMeshComponent = Cast<UStaticMeshComponent>(ParentComponent);
		USkeletalMeshComponent* InSkelMeshComponent = Cast<USkeletalMeshComponent>(ParentComponent);
		if (IsValid(InStaticMeshComponent))
		{
			StaticMeshComponent = InStaticMeshComponent;
			ParentMeshType = EParentMeshType::StaticMesh;
		}
		else if (IsValid(InSkelMeshComponent))
		{
			SkelMeshComponent = InSkelMeshComponent;
			CachedMeshObject = SkelMeshComponent->MeshObject;
			CachedSkeletalMesh = SkelMeshComponent->SkeletalMesh;
			ParentMeshType = EParentMeshType::SkelMesh;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("The ParentComponent is of an unrecognized type : %s."), *ParentComponent->GetClass()->GetName());
		}
	}

	bool RequiresUpdate()
	{
		switch (ParentMeshType)
		{
		case EParentMeshType::StaticMesh:
			return false;
		case EParentMeshType::SkelMesh:
			if (CachedMeshObject != SkelMeshComponent->MeshObject
			|| CachedSkeletalMesh != SkelMeshComponent->SkeletalMesh)
			{
				return true;
			}
			else
			{
				return false;
			}
			break;
		case EParentMeshType::None: // The ParentMeshType is invalid
			return true;
		default: // The ParentMeshType is invalid
			return true;
		}
	}

	UMeshComponent* GetParentMeshComponent()
	{
		switch (ParentMeshType)
		{
		case EParentMeshType::None:
			return nullptr;
		case EParentMeshType::StaticMesh:
			return StaticMeshComponent.Get();
		case EParentMeshType::SkelMesh:
			return SkelMeshComponent.Get();
		default:
			return nullptr;
		}

	}

private:
	FSkeletalMeshObject* CachedMeshObject;
	USkeletalMesh* CachedSkeletalMesh;
	TWeakObjectPtr<UStaticMeshComponent> StaticMeshComponent;
	TWeakObjectPtr<USkeletalMeshComponent> SkelMeshComponent;
	EParentMeshType ParentMeshType;
};
*/

/** A proxy class to get mesh data from StaticMesh, should be used together with AnnotationCamSensor.
Inheritance is needed because I need to access protected data
Use `show Material` command to see the effect of this component
Note that some area might be not colored, this is caused by the issue that
both the original mesh and the annotation mesh are rendered, this is not an issue for the AnnotationCamSensor, which will exclude original meshes.
*/
class FStaticAnnotationSceneProxy : public FStaticMeshSceneProxy
{
public:
	FMaterialRenderProxy* MaterialRenderProxy;

	FStaticAnnotationSceneProxy(UStaticMeshComponent* Component, bool bForceLODsShareStaticLighting, UMaterialInterface* AnnotationMID) :
		FStaticMeshSceneProxy(Component, bForceLODsShareStaticLighting)
	{
		MaterialRenderProxy = AnnotationMID->GetRenderProxy();
		// this->MaterialRelevance = AnnotationMID->GetRelevance(GetScene().GetFeatureLevel());
		// Note: This MaterailRelevance makes no difference?

		this->bVerifyUsedMaterials = false;
		// This is required, otherwise the code will fail

		bCastShadow = false;

		// bAffectDynamicIndirectLighting = false;
		// bAffectIndirectLightingWhileHidden = false;
		// bAffectDistanceFieldLighting = false;

		// // // bVisibleInRayTracing = false;
		// bVisibleInLumenScene = false;
	}

	virtual void GetDynamicMeshElements(
		const TArray < const FSceneView * > & Views,
		const FSceneViewFamily & ViewFamily,
		uint32 VisibilityMap,
		FMeshElementCollector & Collector) const override;

	virtual bool GetMeshElement
	(
		int32 LODIndex,
		int32 BatchIndex,
		int32 ElementIndex,
		uint8 InDepthPriorityGroup,
		bool bUseSelectedMaterial,
		bool bAllowPreCulledIndices,
		FMeshBatch & OutMeshBatch
	) const override;

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView * View) const override;
};

FPrimitiveViewRelevance FStaticAnnotationSceneProxy::GetViewRelevance(const FSceneView * View) const
{
	if ( !View->Family->EngineShowFlags.Materials && !View->Family->EngineShowFlags.PostProcessing )
	{
		return FStaticMeshSceneProxy::GetViewRelevance(View);
	}
	else
	{
		FPrimitiveViewRelevance ViewRelevance;
		ViewRelevance.bDrawRelevance = 0;
		// This will make the AnnotationComponent gets ignored if the Materials flag is on
		// Which means it won't affect regulary rendering.
		// ViewRelevance.bDynamicRelevance = 0;
		// ViewRelevance.bStaticRelevance = 0;
		// ViewRelevance.bShadowRelevance = 0;
		// ViewRelevance.bRenderInMainPass = 0;
		// ViewRelevance.bRenderInDepthPass = 0;
		// ViewRelevance.bVelocityRelevance = 0;
		return ViewRelevance;
	}
}


void FStaticAnnotationSceneProxy::GetDynamicMeshElements(
	const TArray < const FSceneView * > & Views,
	const FSceneViewFamily & ViewFamily,
	uint32 VisibilityMap,
	FMeshElementCollector & Collector) const
{
	FStaticMeshSceneProxy::GetDynamicMeshElements(Views, ViewFamily, VisibilityMap, Collector);
}

bool FStaticAnnotationSceneProxy::GetMeshElement(
	int32 LODIndex,
	int32 BatchIndex,
	int32 ElementIndex,
	uint8 InDepthPriorityGroup,
	bool bUseSelectedMaterial,
	bool bAllowPreCulledIndices,
	FMeshBatch & OutMeshBatch) const
{
	bool Ret = FStaticMeshSceneProxy::GetMeshElement(LODIndex, BatchIndex, ElementIndex, InDepthPriorityGroup,
		bUseSelectedMaterial, bAllowPreCulledIndices, OutMeshBatch);
	OutMeshBatch.MaterialRenderProxy = this->MaterialRenderProxy;
	return Ret;
}

class FSkeletalAnnotationSceneProxy : public FSkeletalMeshSceneProxy
{
public:
	FSkeletalAnnotationSceneProxy(const USkinnedMeshComponent* Component, FSkeletalMeshRenderData* InSkeletalMeshRenderData, UMaterialInterface* AnnotationMID)
	: FSkeletalMeshSceneProxy(FSkinnedMeshSceneProxyDesc(Component), InSkeletalMeshRenderData)
	{
		// TODO: Update MaterialRelevance
		this->bVerifyUsedMaterials = false;
		// this->bCastShadow = false;
		this->bCastDynamicShadow = false;

		// bAffectDynamicIndirectLighting = false;
		// bAffectIndirectLightingWhileHidden = false;
		// bAffectDistanceFieldLighting = false;

		// // bVisibleInRayTracing = false;
		// bVisibleInLumenScene = false;

		for(int32 LODIdx=0; LODIdx < LODSections.Num(); LODIdx++)
		{
			FLODSectionElements& LODSection = LODSections[LODIdx];
			for(int32 SectionIndex = 0; SectionIndex < LODSection.SectionElements.Num(); SectionIndex++)
			{
				if (IsValid(AnnotationMID))
				{
					LODSection.SectionElements[SectionIndex].Material = AnnotationMID;
				}
				else
				{
					UE_LOG(LogUnrealCV, Warning, TEXT("AnnotationMaterial is Invalid in FSkeletalSceneProxy"));
				}
			}
		}
	}
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView * View) const override;

	virtual void GetDynamicMeshElements(
		const TArray<const FSceneView*>& Views,
		const FSceneViewFamily& ViewFamily,
		uint32 VisibilityMap,
		FMeshElementCollector& Collector) const;
};


void FSkeletalAnnotationSceneProxy::GetDynamicMeshElements(
	const TArray<const FSceneView*>& Views,
	const FSceneViewFamily& ViewFamily,
	uint32 VisibilityMap,
	FMeshElementCollector& Collector) const
{
	FSkeletalMeshSceneProxy::GetDynamicMeshElements(Views, ViewFamily, VisibilityMap, Collector);
}

FPrimitiveViewRelevance FSkeletalAnnotationSceneProxy::GetViewRelevance(const FSceneView * View) const
{
	if ( !View->Family->EngineShowFlags.Materials && !View->Family->EngineShowFlags.PostProcessing )
	{
		return FSkeletalMeshSceneProxy::GetViewRelevance(View);
	}
	else
	{
		FPrimitiveViewRelevance ViewRelevance;
		ViewRelevance.bDrawRelevance = 0;
		// This will make the AnnotationComponent gets ignored if the Materials flag is on
		// Which means it won't affect regulary rendering.
		// ViewRelevance.bDynamicRelevance = 0;
		// ViewRelevance.bStaticRelevance = 0;
		// ViewRelevance.bShadowRelevance = 0;
		// ViewRelevance.bRenderInMainPass = 0;
		// ViewRelevance.bRenderInDepthPass = 0;
		// ViewRelevance.bVelocityRelevance = 0;
		return ViewRelevance;
	}
}


class FInstancedStaticMeshAnnotationSceneProxy : public FStaticMeshSceneProxy
{
public:
	FMaterialRenderProxy* AnnotationMaterialRenderProxy;
	UStaticMesh* StaticMesh;
	FInstancedStaticMeshRenderData InstancedRenderData;
	TSharedPtr<FInstanceDataSceneProxy, ESPMode::ThreadSafe> InstanceDataSceneProxy;

	FInstancingUserData UserData_AllInstances;
	FInstancingUserData UserData_SelectedInstances;
	FInstancingUserData UserData_DeselectedInstances;

	TMap<int32, FInstancedStaticMeshVFLooseUniformShaderParametersRef> LODLooseUniformBuffers;

	float InstanceLODDistanceScale;
	FBoxSphereBounds StaticMeshBounds;
	bool bAnySegmentUsesWorldPositionOffset;
	bool bUseGpuLodSelection;

#if WITH_EDITOR
	bool bHasSelectedInstances;
#endif

	FInstancedStaticMeshAnnotationSceneProxy(const FInstancedStaticMeshSceneProxyDesc& InProxyDesc, UMaterialInterface* AnnotationMID, ERHIFeatureLevel::Type InFeatureLevel)
		: FStaticMeshSceneProxy(InProxyDesc, true)
		, StaticMesh(InProxyDesc.GetStaticMesh())
		, InstancedRenderData(&InProxyDesc, InFeatureLevel)
		, InstanceLODDistanceScale(InProxyDesc.InstanceLODDistanceScale)
		, StaticMeshBounds(StaticMesh->GetBounds())
	{
		InstanceDataSceneProxy = InProxyDesc.InstanceDataSceneProxy;

		AnnotationMaterialRenderProxy = AnnotationMID->GetRenderProxy();
		this->bVerifyUsedMaterials = false;
		bCastShadow = false;

#if WITH_EDITOR
		bHasSelectedInstances = InProxyDesc.bHasSelectedInstances;
		if (bHasSelectedInstances)
		{
			SetSelection_GameThread(true);
		}
#endif

		SetupInstanceSceneDataBuffers(InstanceDataSceneProxy->GeInstanceSceneDataBuffers());

		bAnySegmentUsesWorldPositionOffset = false;

		for (int32 LODIndex = 0; LODIndex < LODs.Num(); LODIndex++)
		{
			FStaticMeshSceneProxy::FLODInfo& LODInfo = LODs[LODIndex];
			for (int32 SectionIndex = 0; SectionIndex < LODInfo.Sections.Num(); SectionIndex++)
			{
				FStaticMeshSceneProxy::FLODInfo::FSectionInfo& Section = LODInfo.Sections[SectionIndex];
				Section.Material = AnnotationMID;
				bAnySegmentUsesWorldPositionOffset |= Section.Material->IsUsingWorldPositionOffset_Concurrent(GMaxRHIFeatureLevel);
			}
		}
		UE_LOG(LogUnrealCV, Log, TEXT("FInstancedStaticMeshAnnotationSceneProxy: LODs.Num=%d, AnnotationMID=%p, AnnotationMaterialRenderProxy=%p"),
			LODs.Num(), AnnotationMID, AnnotationMaterialRenderProxy);

		UserData_AllInstances.MeshRenderData = StaticMesh->GetRenderData();
		UserData_AllInstances.MinDrawDistance = InProxyDesc.InstanceMinDrawDistance;
		UserData_AllInstances.StartCullDistance = InProxyDesc.InstanceStartCullDistance;
		UserData_AllInstances.EndCullDistance = InProxyDesc.InstanceEndCullDistance;
		UserData_AllInstances.LODDistanceScale = 1.0f;
		UserData_AllInstances.InstancingOffset = StaticMesh->GetBoundingBox().GetCenter();
		UserData_AllInstances.MinLOD = ClampedMinLOD;
		UserData_AllInstances.bRenderSelected = true;
		UserData_AllInstances.bRenderUnselected = true;
		UserData_AllInstances.RenderData = nullptr;
		UserData_AllInstances.AverageInstancesScale = FVector::Zero();

		UserData_SelectedInstances = UserData_AllInstances;
		UserData_SelectedInstances.bRenderUnselected = false;

		UserData_DeselectedInstances = UserData_AllInstances;
		UserData_DeselectedInstances.bRenderSelected = false;

		bUseGpuLodSelection = InProxyDesc.bUseGpuLodSelection;
	}

	virtual void CreateRenderThreadResources(FRHICommandListBase& RHICmdList) override
	{
		FStaticMeshSceneProxy::CreateRenderThreadResources(RHICmdList);

		if (InstanceDataSceneProxy.IsValid())
		{
			InstancedRenderData.BindBuffersToVertexFactories(RHICmdList, InstanceDataSceneProxy->GetLegacyInstanceBuffer());

			for (int32 LODIndex = 0; LODIndex < LODs.Num(); ++LODIndex)
			{
				FInstancedStaticMeshVFLooseUniformShaderParametersRef LooseUniformBuffer = CreateLooseUniformBuffer(nullptr, &UserData_AllInstances, 0, LODIndex);
				LODLooseUniformBuffers.Add(LODIndex, LooseUniformBuffer);
			}
		}
	}

	virtual void DestroyRenderThreadResources() override
	{
		InstancedRenderData.ReleaseResources(&GetScene(), StaticMesh);
		FStaticMeshSceneProxy::DestroyRenderThreadResources();
	}

	FInstancedStaticMeshVFLooseUniformShaderParametersRef CreateLooseUniformBuffer(const FSceneView* View, const FInstancingUserData* InstancingUserData, uint32 InstancedLODRange, uint32 InstancedLODIndex) const
	{
		FInstancedStaticMeshVFLooseUniformShaderParameters LooseParameters;

		FVector4f InstancingViewZCompareZero(MIN_flt, MIN_flt, MAX_flt, 1.0f);
		FVector4f InstancingViewZCompareOne(MIN_flt, MIN_flt, MAX_flt, 0.0f);
		FVector4f InstancingViewZConstant(0.0f, 0.0f, 1.0f, 0.0f);
		FVector4f InstancingTranslatedWorldViewOriginZero(ForceInit);
		FVector4f InstancingTranslatedWorldViewOriginOne(ForceInit);
		InstancingTranslatedWorldViewOriginOne.W = 1.0f;

		LooseParameters.InstancingViewZCompareZero = InstancingViewZCompareZero;
		LooseParameters.InstancingViewZCompareOne = InstancingViewZCompareOne;
		LooseParameters.InstancingViewZConstant = InstancingViewZConstant;
		LooseParameters.InstancingTranslatedWorldViewOriginZero = InstancingTranslatedWorldViewOriginZero;
		LooseParameters.InstancingTranslatedWorldViewOriginOne = InstancingTranslatedWorldViewOriginOne;

		FVector4f InstancingFadeOutParams(MAX_flt, 0.f, 1.f, 1.f);
		if (InstancingUserData)
		{
			const float MaxDrawDistanceScale = GetCachedScalabilityCVars().ViewDistanceScale;
			const float StartDistance = InstancingUserData->StartCullDistance * MaxDrawDistanceScale;
			const float EndDistance = InstancingUserData->EndCullDistance * MaxDrawDistanceScale;

			InstancingFadeOutParams.X = StartDistance;
			if (EndDistance > 0)
			{
				if (EndDistance > StartDistance)
				{
					InstancingFadeOutParams.Y = 1.f / (float)(EndDistance - StartDistance);
				}
				else
				{
					InstancingFadeOutParams.Y = 1.f;
				}
			}
			else
			{
				InstancingFadeOutParams.Y = 0.f;
			}
			InstancingFadeOutParams.Z = InstancingUserData->bRenderSelected ? 1.f : 0.f;
			InstancingFadeOutParams.W = InstancingUserData->bRenderUnselected ? 1.f : 0.f;
		}

		LooseParameters.InstancingFadeOutParams = InstancingFadeOutParams;

		return FInstancedStaticMeshVFLooseUniformShaderParametersRef::CreateUniformBufferImmediate(
			LooseParameters,
			EUniformBufferUsage::UniformBuffer_MultiFrame
		);
	}

	bool CanSetupInstancedMeshBatch(int32 LODIndex) const
	{
		return LODLooseUniformBuffers.Contains(LODIndex) && LODIndex < InstancedRenderData.VertexFactories.Num();
	}

	void SetupInstancedMeshBatch(int32 LODIndex, int32 BatchIndex, FMeshBatch& OutMeshBatch) const
	{
		OutMeshBatch.VertexFactory = &InstancedRenderData.VertexFactories[LODIndex];

		FMeshBatchElement& BatchElement0 = OutMeshBatch.Elements[0];
		BatchElement0.UserData = (void*)&UserData_AllInstances;
		BatchElement0.bUserDataIsColorVertexBuffer = false;
		BatchElement0.InstancedLODIndex = LODIndex;
		BatchElement0.UserIndex = 0;
		BatchElement0.PrimitiveUniformBuffer = GetUniformBuffer();
		BatchElement0.LooseParametersUniformBuffer = LODLooseUniformBuffers[LODIndex];
		BatchElement0.bForceInstanceCulling = true;
		BatchElement0.NumInstances = GetInstanceDataHeader().NumInstances;
	}

	virtual bool GetMeshElement(
		int32 LODIndex,
		int32 BatchIndex,
		int32 ElementIndex,
		uint8 InDepthPriorityGroup,
		bool bUseSelectedMaterial,
		bool bAllowPreCulledIndices,
		FMeshBatch& OutMeshBatch) const override
	{
		if (!CanSetupInstancedMeshBatch(LODIndex))
		{
			return false;
		}

		if (!FStaticMeshSceneProxy::GetMeshElement(LODIndex, BatchIndex, ElementIndex, InDepthPriorityGroup,
			bUseSelectedMaterial, bAllowPreCulledIndices, OutMeshBatch))
		{
			return false;
		}

		SetupInstancedMeshBatch(LODIndex, BatchIndex, OutMeshBatch);
		OutMeshBatch.MaterialRenderProxy = this->AnnotationMaterialRenderProxy;

		static bool bLogOnce = true;
		if (bLogOnce)
		{
			UE_LOG(LogUnrealCV, Log, TEXT("GetMeshElement: LODIndex=%d, MaterialRenderProxy=%p"), LODIndex, OutMeshBatch.MaterialRenderProxy);
			bLogOnce = false;
		}

		return true;
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		if (!View->Family->EngineShowFlags.Materials && !View->Family->EngineShowFlags.PostProcessing)
		{
// 			FPrimitiveViewRelevance Result;
// 			if (View->Family->EngineShowFlags.InstancedStaticMeshes)
// 			{
// 				Result = FStaticMeshSceneProxy::GetViewRelevance(View);
// #if WITH_EDITOR
// 				if (bHasSelectedInstances)
// 				{
// 					Result.bDynamicRelevance = true;
// 					Result.bStaticRelevance = false;
// 				}
// #endif
// 			}
// 			return Result;

			checkSlow(IsInParallelRenderingThread());

			FPrimitiveViewRelevance Result;
			Result.bDrawRelevance = IsShown(View) && View->Family->EngineShowFlags.StaticMeshes;
			Result.bRenderCustomDepth = ShouldRenderCustomDepth();
			Result.bRenderInMainPass = ShouldRenderInMainPass();
			Result.bRenderInDepthPass = ShouldRenderInDepthPass();
			Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
			Result.bTranslucentSelfShadow = bCastVolumetricTranslucentShadow;

#if STATICMESH_ENABLE_DEBUG_RENDERING
			bool bDrawSimpleCollision = false, bDrawComplexCollision = false;
			const bool bInCollisionView = IsCollisionView(View->Family->EngineShowFlags, bDrawSimpleCollision, bDrawComplexCollision);
#else
			bool bInCollisionView = false;
#endif
			const bool bAllowStaticLighting = IsStaticLightingAllowed();

			if(
#if !(UE_BUILD_SHIPPING) || WITH_EDITOR
				IsRichView(*View->Family) || 
				View->Family->EngineShowFlags.Collision ||
				bInCollisionView ||
				View->Family->EngineShowFlags.Bounds ||
				View->Family->EngineShowFlags.VisualizeInstanceUpdates ||
#endif
#if WITH_EDITOR
				(IsSelected() && View->Family->EngineShowFlags.VertexColors) ||
				(IsSelected() && View->Family->EngineShowFlags.PhysicalMaterialMasks) ||
#endif
				// Force down dynamic rendering path if invalid lightmap settings, so we can apply an error material in DrawRichMesh
				(bAllowStaticLighting && HasStaticLighting() && !HasValidSettingsForStaticLighting()) ||
				HasViewDependentDPG()
				)
			{
				Result.bDynamicRelevance = true;
			}
			else
			{
				Result.bStaticRelevance = true;

#if WITH_EDITOR
				//only check these in the editor
				Result.bEditorVisualizeLevelInstanceRelevance = IsEditingLevelInstanceChild();
				Result.bEditorStaticSelectionRelevance = (WantsEditorEffects() || IsSelected() || IsHovered());
#endif
			}

			Result.bShadowRelevance = IsShadowCast(View);

			MaterialRelevance.SetPrimitiveViewRelevance(Result);

			if (!View->Family->EngineShowFlags.Materials 
#if STATICMESH_ENABLE_DEBUG_RENDERING
				|| bInCollisionView
#endif
				)
			{
				Result.bOpaque = true;
			}

			Result.bVelocityRelevance = DrawsVelocity() && Result.bOpaque && Result.bRenderInMainPass;

			return Result;
		}
		else
		{
			FPrimitiveViewRelevance ViewRelevance;
			ViewRelevance.bDrawRelevance = 0;
			return ViewRelevance;
		}
	}
};







// FString MeterialPath = TEXT("MaterialInstanceConstant'/UnrealCV/AnnotationColor_Inst.AnnotationColor_Inst'");
// static ConstructorHelpers::FObjectFinder<UMaterialInstanceDynamic> AnnotationMaterialObject(*MaterialPath);
UAnnotationComponent::UAnnotationComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bRefreshRenderState = false;

	FString MaterialPath = TEXT("Material'/UnrealCV/AnnotationColor.AnnotationColor'");
	static ConstructorHelpers::FObjectFinder<UMaterial> AnnotationMaterialObject(*MaterialPath);
	if (AnnotationMaterialObject.Object == nullptr)
    {
        UE_LOG(LogTemp, Warning, TEXT("Annotation material is not valid."));
    }
    else
    {
        AnnotationMaterial = AnnotationMaterialObject.Object;
	}

	FString GroomMaterialPath = TEXT("Material'/UnrealCV/GroomAnnotationColor.GroomAnnotationColor'");
	static ConstructorHelpers::FObjectFinder<UMaterial> GroomAnnotationMaterialObject(*GroomMaterialPath);
	if (GroomAnnotationMaterialObject.Object == nullptr)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("GroomAnnotationColor material is not valid."));
	}
	else
	{
		GroomAnnotationMaterial = GroomAnnotationMaterialObject.Object;
	}

	this->PrimaryComponentTick.bCanEverTick = true;
}

void UAnnotationComponent::OnRegister()
{
	Super::OnRegister();

	AnnotationMID = UMaterialInstanceDynamic::Create(AnnotationMaterial, this, TEXT("AnnotationMaterialMID"));
	if (!IsValid(AnnotationMID))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("AnnotationMaterial is not correctly initialized"));
		return;
	}

	if (IsValid(GroomAnnotationMaterial))
	{
		GroomAnnotationMID = UMaterialInstanceDynamic::Create(GroomAnnotationMaterial, this, TEXT("GroomAnnotationMaterialMID"));
		if (!IsValid(GroomAnnotationMID))
		{
			UE_LOG(LogUnrealCV, Warning, TEXT("GroomAnnotationMaterial is not correctly initialized"));
		}
	}

	const float OneOver255 = 1.0f / 255.0f;
	FLinearColor LinearAnnotationColor = FLinearColor(
		this->AnnotationColor.R * OneOver255,
		this->AnnotationColor.G * OneOver255,
		this->AnnotationColor.B * OneOver255,
		1.0
	);
	AnnotationMID->SetVectorParameterValue("AnnotationColor", LinearAnnotationColor);

	if (IsValid(GroomAnnotationMID))
	{
		GroomAnnotationMID->SetVectorParameterValue("AnnotationColor", LinearAnnotationColor);
	}
}

/** 
 * Note: The "exposure compensation" in "PostProcessVolume3" in the RR map will destroy the color
 * Saturate the color to 1. This is a mysterious behavior after tedious debug.
 */
void UAnnotationComponent::SetAnnotationColor(FColor NewAnnotationColor)
{
	this->AnnotationColor = NewAnnotationColor;
	const float OneOver255 = 1.0f / 255.0f;
	FLinearColor LinearAnnotationColor = FLinearColor(
		AnnotationColor.R * OneOver255,
		AnnotationColor.G * OneOver255,
		AnnotationColor.B * OneOver255,
		1.0
	);

	if (IsValid(AnnotationMID))
	{
		AnnotationMID->SetVectorParameterValue("AnnotationColor", LinearAnnotationColor);
	}

	if (IsValid(GroomAnnotationMID))
	{
		GroomAnnotationMID->SetVectorParameterValue("AnnotationColor", LinearAnnotationColor);
	}
}

FColor UAnnotationComponent::GetAnnotationColor()
{
	return AnnotationColor;
}

void UAnnotationComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	Super::GetUsedMaterials(OutMaterials, bGetDebugMaterials);

	if (AnnotationMID)
	{
		OutMaterials.AddUnique(AnnotationMID);
	}

	if (GroomAnnotationMID)
	{
		OutMaterials.AddUnique(GroomAnnotationMID);
	}
}

FPrimitiveSceneProxy* UAnnotationComponent::CreateSceneProxy(UStaticMeshComponent* StaticMeshComponent)
{
	// FPrimitiveSceneProxy* PrimitiveSceneProxy = StaticMeshComponent->CreateSceneProxy();
	// FStaticMeshSceneProxy* StaticMeshSceneProxy = (FStaticMeshSceneProxy*)PrimitiveSceneProxy;

	UMaterialInterface* ProxyMaterial = AnnotationMID;
	UStaticMesh* ParentStaticMesh = StaticMeshComponent->GetStaticMesh();

	UE_LOG(LogUnrealCV, Log, TEXT("CreateSceneProxy for StaticMeshComponent: %s, Class: %s, Owner: %s, AnnotationMID=%p, AnnotationColor=%s"),
		*StaticMeshComponent->GetName(),
		*StaticMeshComponent->GetClass()->GetName(),
		StaticMeshComponent->GetOwner() ? *StaticMeshComponent->GetOwner()->GetName() : TEXT("None"),
		AnnotationMID,
		*AnnotationColor.ToString());

	if(ParentStaticMesh == NULL
		|| ParentStaticMesh->GetRenderData() == NULL
		|| ParentStaticMesh->GetRenderData()->LODResources.Num() == 0)
		// || StaticMesh->RenderData->LODResources[0].VertexBuffer.GetNumVertices() == 0)
	{
		// UE_LOG(LogTemp, Warning, TEXT("%s, ParentStaticMesh is invalid."), *StaticMeshComponent->GetName());
		UE_LOG(LogUnrealCV, Warning, TEXT("CreateSceneProxy failed for %s: ParentStaticMesh=%p, RenderData=%p, LODResources=%d"),
			*StaticMeshComponent->GetName(),
			ParentStaticMesh,
			ParentStaticMesh ? ParentStaticMesh->GetRenderData() : nullptr,
			(ParentStaticMesh && ParentStaticMesh->GetRenderData()) ? ParentStaticMesh->GetRenderData()->LODResources.Num() : 0);
		return NULL;
	}

	UInstancedStaticMeshComponent* InstancedComponent = Cast<UInstancedStaticMeshComponent>(StaticMeshComponent);
	if (InstancedComponent)
	{
		UE_LOG(LogUnrealCV, Log, TEXT("Creating FInstancedStaticMeshAnnotationSceneProxy for %s"), *StaticMeshComponent->GetName());
		FInstancedStaticMeshSceneProxyDesc ProxyDesc(InstancedComponent);
		FPrimitiveSceneProxy* Proxy = ::new FInstancedStaticMeshAnnotationSceneProxy(ProxyDesc, ProxyMaterial, GetWorld()->GetFeatureLevel());
		UE_LOG(LogUnrealCV, Log, TEXT("Created FInstancedStaticMeshAnnotationSceneProxy for %s, Proxy=%p"), *StaticMeshComponent->GetName(), Proxy);
		return Proxy;
	}

	// FPrimitiveSceneProxy* Proxy = ::new FStaticMeshSceneProxy(OwnerComponent, false);
	FPrimitiveSceneProxy* Proxy = ::new FStaticAnnotationSceneProxy(StaticMeshComponent, false, ProxyMaterial);
	UE_LOG(LogUnrealCV, Log, TEXT("Created FStaticAnnotationSceneProxy for %s, Proxy=%p"), *StaticMeshComponent->GetName(), Proxy);
	return Proxy;
	// This is not recommended, but I know what I am doing.
}

// See https://github.com/EpicGames/UnrealEngine/blob/release/Engine/Source/Runtime/Engine/Private/Components/SkinnedMeshComponent.cpp:417
FPrimitiveSceneProxy* UAnnotationComponent::CreateSceneProxy(USkeletalMeshComponent* SkeletalMeshComponent)
{
	UMaterialInterface* ProxyMaterial = AnnotationMID; // Material Instance Dynamic
	// ERHIFeatureLevel::Type SceneFeatureLevel = GetWorld()->GetFeatureLevel();

	// Ref: https://github.com/EpicGames/UnrealEngine/blob/4.19/Engine/Source/Runtime/Engine/Private/Components/SkinnedMeshComponent.cpp#L415
	FSkeletalMeshRenderData* SkelMeshRenderData = SkeletalMeshComponent->GetSkeletalMeshRenderData();

	// Only create a scene proxy for rendering if properly initialized
	if (SkelMeshRenderData &&
		SkelMeshRenderData->LODRenderData.IsValidIndex(SkeletalMeshComponent->GetPredictedLODLevel()) &&
		SkeletalMeshComponent->MeshObject) // The risk of using MeshObject
	{
		// Only create a scene proxy if the bone count being used is supported, or if we don't have a skeleton (this is the case with destructibles)
		// int32 MaxBonesPerChunk = SkelMeshResource->GetMaxBonesPerSection();
		// if (MaxBonesPerChunk <= GetFeatureLevelMaxNumberOfBones(SceneFeatureLevel))
		// {
		//	Result = ::new FSkeletalAnnotationSceneProxy(SkeletalMeshComponent, SkelMeshResource, AnnotationMID);
		// }
		// TODO: The SkeletalMeshComponent might need to be recreated

		// Check the MeshObject is valid
		// if (!!!(SkeletalMeshComponent->MeshObject))
		// {
		// 	UE_LOG(LogUnrealCV, Warning, TEXT("SkeletalMeshComponent %s MeshObject is invalid."), *SkeletalMeshComponent->GetName());
		// }
		check(SkeletalMeshComponent->MeshObject)

		FSkinnedMeshSceneProxyDesc Test(SkeletalMeshComponent);
		if (!!!(Test.MeshObject))
		{
			UE_LOG(LogUnrealCV, Warning, TEXT("FSkinnedMeshSceneProxyDesc %s MeshObject is invalid."), *SkeletalMeshComponent->GetName());
			return nullptr;
		}
		check(Test.MeshObject)
		return new FSkeletalAnnotationSceneProxy(SkeletalMeshComponent, SkelMeshRenderData, ProxyMaterial);
	}
	else
	{
		LOG1(FString::Printf(TEXT("The data of SkeletalMeshComponent %s is invalid."), *SkeletalMeshComponent->GetName()));
		return nullptr;
	}
}


FPrimitiveSceneProxy* UAnnotationComponent::CreateSceneProxy(UGroomComponent* GroomComponent)
{
	UMaterialInterface* ProxyMaterial = GroomAnnotationMID;

	UE_LOG(LogUnrealCV, Log, TEXT("CreateSceneProxy for GroomComponent: %s, Owner: %s, GroomAnnotationMID=%p, AnnotationColor=%s"),
		*GroomComponent->GetName(),
		GroomComponent->GetOwner() ? *GroomComponent->GetOwner()->GetName() : TEXT("None"),
		GroomAnnotationMID,
		*AnnotationColor.ToString());

	if (!GroomComponent->GroomAsset || GroomComponent->GroomAsset->GetNumHairGroups() == 0)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("CreateSceneProxy failed for GroomComponent %s: Invalid GroomAsset"), *GroomComponent->GetName());
		return nullptr;
	}

	if (!IsValid(ProxyMaterial))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("CreateSceneProxy failed for GroomComponent %s: GroomAnnotationMID is invalid"), *GroomComponent->GetName());
		return nullptr;
	}

	FPrimitiveSceneProxy* Proxy = ::new FGroomAnnotationSceneProxy(GroomComponent, ProxyMaterial);
	UE_LOG(LogUnrealCV, Log, TEXT("Created FGroomAnnotationSceneProxy for %s, Proxy=%p"), *GroomComponent->GetName(), Proxy);
	return Proxy;
}


// TODO: This needs to be involked when the ParentComponent refresh its render state, otherwise it will crash the engine
FPrimitiveSceneProxy* UAnnotationComponent::CreateSceneProxy()
{
	// UMaterialInstanceDynamic* AnnotationMID = UMaterialInstanceDynamic::Create(AnnotationMaterial, this);
	// FColor AnnotationColor = FColor::MakeRandomColor();
	// AnnotationMID->SetVectorParameterByIndex(0, AnnotationColor);

	USceneComponent* ParentComponent = this->GetAttachParent();
	// USceneComponent* ParentComponent = this->ParentMeshInfo->GetParentMeshComponent();

	if (!IsValid(ParentComponent))
	{
		UE_LOG(LogTemp, Warning, TEXT("Parent component is invalid."));
		return nullptr;
	}

	UE_LOG(LogUnrealCV, Log, TEXT("CreateSceneProxy for AnnotationComponent: %s, ParentComponent: %s, ParentClass: %s"),
		*this->GetName(),
		*ParentComponent->GetName(),
		*ParentComponent->GetClass()->GetName());

	UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(ParentComponent);
	USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(ParentComponent);
	UGroomComponent* GroomComponent = Cast<UGroomComponent>(ParentComponent);
	// UCableComponent* CableComponent = Cast<UCableComponent>(ParentComponent);
	if (IsValid(StaticMeshComponent))
	{
		return CreateSceneProxy(StaticMeshComponent);
	}
	else if (IsValid(SkeletalMeshComponent))
	{
		bRefreshRenderState = true;
		return CreateSceneProxy(SkeletalMeshComponent);
	}
	else if (IsValid(GroomComponent))
	{
		bRefreshRenderState = true;
		return CreateSceneProxy(GroomComponent);
	}
	// else if (IsValid(CableComponent))
	// {
	// 	return CreateSceneProxy(CableComponent);
	// }
	else
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("The type of ParentMeshComponent : %s can not be supported."), *ParentComponent->GetClass()->GetName());
		return nullptr;
	}
	// return nullptr;
}

FBoxSphereBounds UAnnotationComponent::CalcBounds(const FTransform & LocalToWorld) const
{
	// UMeshComponent* ParentMeshComponent = ParentMeshInfo->GetParentMeshComponent();
	// if (IsValid(ParentMeshComponent))
	// {
	// 	return ParentMeshComponent->CalcBounds(LocalToWorld);
	// }
	// else
	// {
	// 	FBoxSphereBounds DefaultBounds;
	// 	return DefaultBounds;
	// }

	USceneComponent* Parent = this->GetAttachParent();
	UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Parent);
	if (IsValid(StaticMeshComponent))
	{
		return StaticMeshComponent->CalcBounds(LocalToWorld);
	}

	USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(Parent);
	if (IsValid(SkeletalMeshComponent))
	{
		return SkeletalMeshComponent->CalcBounds(LocalToWorld);
	}

	UGroomComponent* GroomComponent = Cast<UGroomComponent>(Parent);
	if (IsValid(GroomComponent))
	{
		return GroomComponent->CalcBounds(LocalToWorld);
	}

	UE_LOG(LogTemp, Error, TEXT("The type of ParentMeshComponent : %s can not be supported."), *Parent->GetClass()->GetName());
    FBoxSphereBounds DefaultBounds = FBoxSphereBounds(FVector::ZeroVector, FVector::ZeroVector, 0.0f);
	return DefaultBounds;
}

FMatrix UAnnotationComponent::GetRenderMatrix() const
{
	USceneComponent* Parent = this->GetAttachParent();
	if (IsValid(Parent))
	{
		UGroomComponent* GroomComponent = Cast<UGroomComponent>(Parent);
		if (IsValid(GroomComponent))
		{
			return GroomComponent->GetRenderMatrix();
		}

		UPrimitiveComponent* ParentPrimitive = Cast<UPrimitiveComponent>(Parent);
		if (IsValid(ParentPrimitive))
		{
			return ParentPrimitive->GetRenderMatrix();
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("ParentMeshComponent is not a PrimitiveComponent."));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ParentMeshComponent is invalid."));
	}
	return Super::GetRenderMatrix();
}

// Extra overhead for the game scene
void UAnnotationComponent::TickComponent(
	float DeltaTime,
	enum ELevelTick TickType,
	FActorComponentTickFunction * ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	USceneComponent* Parent = this->GetAttachParent();
	if (IsValid(Parent))
	{
		UGroomComponent* GroomComponent = Cast<UGroomComponent>(Parent);
		if (IsValid(GroomComponent))
		{
			SetWorldTransform(GroomComponent->GetComponentTransform());
		}
	}

	if (bRefreshRenderState)
	{
		MarkRenderStateDirty(); // Without it will break the SkeletalMeshComponent
	}

	// if (ParentMeshInfo->RequiresUpdate())
	// TODO: This sometimes miss a required update, see OWIMap. Not sure why.
	// TODO: Per-frame update is certainly wasted.
	// FIXME: Update the render proxy per frame will cause jittering on the material.
}


void UAnnotationComponent::ForceUpdate()
{
	UE_LOG(LogTemp, Warning, TEXT("Force update the annotation component."));
	this->MarkRenderStateDirty();
}
