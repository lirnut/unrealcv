#include "BPFunctionLib/MetaDataBPLib.h"
#include "UnrealcvLog.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "MaterialCachedData.h"
#include "Engine/Texture.h"
#include "TextureResource.h"
#include "Controller/ObjectAnnotator.h"
#include "BPFunctionLib/SerializeBPLib.h"

FActorSemanticMetadata UMetaDataBPLib::GetActorSemanticMetadata(AActor* Actor)
{
    FActorSemanticMetadata Metadata;
    if (!IsValid(Actor))
    {
        return Metadata;
    }

    Metadata.ObjectName = Actor->GetName();
    Metadata.ClassName = Actor->GetClass()->GetName();
    Metadata.ClassPath = Actor->GetClass()->GetPathName();
    Metadata.Position = Actor->GetActorLocation();
    Metadata.Rotation = Actor->GetActorRotation();
    Metadata.Scale = Actor->GetActorScale3D();

    FBox ActorBounds = Actor->GetComponentsBoundingBox();
    Metadata.BoundingBoxMin = ActorBounds.Min;
    Metadata.BoundingBoxMax = ActorBounds.Max;

    FObjectAnnotator::GetAnnotationColor(Actor, Metadata.AnnotationColor);

    TArray<UActorComponent*> Components;
    Actor->GetComponents(Components);
    for (UActorComponent* Component : Components)
    {
        UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Component);
        if (!IsValid(PrimComp))
        {
            continue;
        }

        int32 NumMaterials = PrimComp->GetNumMaterials();
        for (int32 i = 0; i < NumMaterials; ++i)
        {
            UMaterialInterface* MatInterface = PrimComp->GetMaterial(i);
            if (IsValid(MatInterface))
            {
                FString MatName = MatInterface->GetName();
                if (!Metadata.MaterialNames.Contains(MatName))
                {
                    Metadata.MaterialNames.Add(MatName);
                }
            }
        }
    }

    return Metadata;
}

FClassSemanticMetadata UMetaDataBPLib::GetClassSemanticMetadata(AActor* Actor)
{
    FClassSemanticMetadata Metadata;
    if (!IsValid(Actor))
    {
        return Metadata;
    }

    UClass* ActorClass = Actor->GetClass();
    Metadata.ClassName = ActorClass->GetName();
    Metadata.ClassPath = ActorClass->GetPathName();
    Metadata.ClassType = ActorClass->IsNative() ? TEXT("Native") : TEXT("Blueprint");
    Metadata.InstanceNames.Add(Actor->GetName());

    TArray<UActorComponent*> Components;
    Actor->GetComponents(Components);
    for (UActorComponent* Component : Components)
    {
        UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Component);
        if (!IsValid(PrimComp))
        {
            continue;
        }

        int32 NumMaterials = PrimComp->GetNumMaterials();
        for (int32 i = 0; i < NumMaterials; ++i)
        {
            UMaterialInterface* MatInterface = PrimComp->GetMaterial(i);
            if (IsValid(MatInterface))
            {
                FString MatName = MatInterface->GetName();
                if (!Metadata.MaterialNames.Contains(MatName))
                {
                    Metadata.MaterialNames.Add(MatName);
                }
            }
        }
    }

    return Metadata;
}

TArray<FMaterialSemanticMetadata> UMetaDataBPLib::GetActorMaterialsMetadata(AActor* Actor)
{
    TArray<FMaterialSemanticMetadata> MaterialsMetadata;
    if (!IsValid(Actor))
    {
        return MaterialsMetadata;
    }

    TSet<UMaterialInterface*> ProcessedMaterials;

    TArray<UActorComponent*> Components;
    Actor->GetComponents(Components);
    for (UActorComponent* Component : Components)
    {
        UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Component);
        if (!IsValid(PrimComp))
        {
            continue;
        }

        int32 NumMaterials = PrimComp->GetNumMaterials();
        for (int32 i = 0; i < NumMaterials; ++i)
        {
            UMaterialInterface* MatInterface = PrimComp->GetMaterial(i);
            if (!IsValid(MatInterface) || ProcessedMaterials.Contains(MatInterface))
            {
                continue;
            }
            ProcessedMaterials.Add(MatInterface);

            FMaterialSemanticMetadata MatMetadata;
            MatMetadata.MaterialName = MatInterface->GetName();
            MatMetadata.MaterialPath = MatInterface->GetPathName();
            MatMetadata.BlendMode = GetBlendModeString(MatInterface->GetBlendMode());
            MatMetadata.ShadingModel = GetShadingModelString((EMaterialShadingModel)MatInterface->GetShadingModels().GetFirstShadingModel());
            MatMetadata.bTwoSided = MatInterface->IsTwoSided();
            MatMetadata.bIsThinSurface = MatInterface->IsThinSurface();
            MatMetadata.bIsMasked = MatInterface->IsMasked();
            MatMetadata.OpacityMaskClipValue = MatInterface->GetOpacityMaskClipValue();
            MatMetadata.bCastDynamicShadowAsMasked = MatInterface->GetCastDynamicShadowAsMasked();
            MatMetadata.bOutputTranslucentVelocity = MatInterface->IsTranslucencyWritingVelocity();
            MatMetadata.bHasPixelAnimation = MatInterface->HasPixelAnimation();
            MatMetadata.bEnableTessellation = MatInterface->IsTessellationEnabled();
            MatMetadata.bCastsRayTracedShadows = MatInterface->CastsRayTracedShadows();
            MatMetadata.bIsDisplacementFadeEnabled = MatInterface->IsDisplacementFadeEnabled();
            MatMetadata.MaxWorldPositionOffsetDisplacement = MatInterface->GetMaxWorldPositionOffsetDisplacement();

            // Get material domain from base material
            UMaterial* BaseMaterial = MatInterface->GetBaseMaterial();
            if (IsValid(BaseMaterial))
            {
                MatMetadata.ParentMaterial = BaseMaterial->GetPathName();
                MatMetadata.MaterialDomain = GetMaterialDomainString(BaseMaterial->MaterialDomain);
            }

            // Get cached expression data (available in packaged builds)
            const FMaterialCachedExpressionData& CachedData = MatInterface->GetCachedExpressionData();
            MatMetadata.bHasMaterialLayers = CachedData.bHasMaterialLayers;
            MatMetadata.bHasRuntimeVirtualTextureOutput = CachedData.bHasRuntimeVirtualTextureOutput;
            MatMetadata.bHasSceneColor = CachedData.bHasSceneColor;
            MatMetadata.bHasPerInstanceCustomData = CachedData.bHasPerInstanceCustomData;
            MatMetadata.bHasWorldPositionOffset = CachedData.bHasWorldPosition;
            MatMetadata.bHasVertexInterpolator = CachedData.bHasVertexInterpolator;
            MatMetadata.bHasCustomizedUVs = CachedData.bHasCustomizedUVs;
            MatMetadata.bHasMeshPaintTexture = CachedData.bHasMeshPaintTexture;

            // Get referenced textures from cached data
            for (const TObjectPtr<UObject>& TextureObj : CachedData.ReferencedTextures)
            {
                if (IsValid(TextureObj))
                {
                    UTexture* Texture = Cast<UTexture>(TextureObj);
                    if (IsValid(Texture))
                    {
                        MatMetadata.ReferencedTextures.Add(GetTextureSemanticMetadata(Texture));
                    }
                }
            }

            // Get parameter values from MaterialInstance if available
            UMaterialInstance* MatInstance = Cast<UMaterialInstance>(MatInterface);
            if (IsValid(MatInstance))
            {
                // Get scalar parameter values
                for (const FScalarParameterValue& ScalarParam : MatInstance->ScalarParameterValues)
                {
                    FString ParamName = ScalarParam.ParameterInfo.Name.ToString();
                    MatMetadata.ScalarParameters.Add(ParamName, ScalarParam.ParameterValue);

                    // Extract physical material properties from common parameter names
                    if (ParamName == TEXT("Roughness") || ParamName == TEXT("Roughness_Constant"))
                    {
                        MatMetadata.Roughness = ScalarParam.ParameterValue;
                    }
                    else if (ParamName == TEXT("Metallic") || ParamName == TEXT("Metallic_Constant"))
                    {
                        MatMetadata.Metallic = ScalarParam.ParameterValue;
                    }
                    else if (ParamName == TEXT("Specular") || ParamName == TEXT("Specular_Constant"))
                    {
                        MatMetadata.Specular = ScalarParam.ParameterValue;
                    }
                    else if (ParamName == TEXT("Anisotropy") || ParamName == TEXT("Anisotropy_Constant"))
                    {
                        MatMetadata.Anisotropy = ScalarParam.ParameterValue;
                    }
                    else if (ParamName == TEXT("Opacity") || ParamName == TEXT("Opacity_Constant"))
                    {
                        MatMetadata.Opacity = ScalarParam.ParameterValue;
                    }
                    else if (ParamName == TEXT("ClearCoat") || ParamName == TEXT("ClearCoat_Constant"))
                    {
                        MatMetadata.ClearCoat = ScalarParam.ParameterValue;
                    }
                    else if (ParamName == TEXT("ClearCoatRoughness") || ParamName == TEXT("ClearCoatRoughness_Constant"))
                    {
                        MatMetadata.ClearCoatRoughness = ScalarParam.ParameterValue;
                    }
                    else if (ParamName == TEXT("AmbientOcclusion") || ParamName == TEXT("AmbientOcclusion_Constant") || ParamName == TEXT("AO"))
                    {
                        MatMetadata.AmbientOcclusion = ScalarParam.ParameterValue;
                    }
                    else if (ParamName == TEXT("Refraction") || ParamName == TEXT("Refraction_Constant") || ParamName == TEXT("IOR"))
                    {
                        MatMetadata.Refraction = ScalarParam.ParameterValue;
                    }
                }

                // Get vector parameter values
                for (const FVectorParameterValue& VectorParam : MatInstance->VectorParameterValues)
                {
                    FString ParamName = VectorParam.ParameterInfo.Name.ToString();
                    MatMetadata.VectorParameters.Add(ParamName, VectorParam.ParameterValue);

                    // Extract color properties from common parameter names
                    if (ParamName == TEXT("BaseColor") || ParamName == TEXT("Base_Color") || ParamName == TEXT("Color") || ParamName == TEXT("Albedo"))
                    {
                        MatMetadata.BaseColor = VectorParam.ParameterValue;
                    }
                    else if (ParamName == TEXT("SubsurfaceColor") || ParamName == TEXT("Subsurface_Color"))
                    {
                        MatMetadata.SubsurfaceColor = VectorParam.ParameterValue;
                    }
                    else if (ParamName == TEXT("EmissiveColor") || ParamName == TEXT("Emissive_Color") || ParamName == TEXT("Emissive"))
                    {
                        MatMetadata.EmissiveColor = VectorParam.ParameterValue;
                    }
                }

                // Get texture parameter values
                for (const FTextureParameterValue& TextureParam : MatInstance->TextureParameterValues)
                {
                    if (IsValid(TextureParam.ParameterValue))
                    {
                        MatMetadata.TextureMaps.Add(TextureParam.ParameterInfo.Name.ToString(), TextureParam.ParameterValue->GetPathName());
                    }
                }

                // Get static switch parameter values from MaterialInterface (UE5.5+ compatible)
                TMap<FMaterialParameterInfo, FMaterialParameterMetadata> StaticSwitchParams;
                MatInterface->GetAllParametersOfType(EMaterialParameterType::StaticSwitch, StaticSwitchParams);
                for (const auto& Pair : StaticSwitchParams)
                {
                    bool Value = Pair.Value.Value.AsStaticSwitch();
                    MatMetadata.StaticSwitchParameters.Add(Pair.Key.Name.ToString(), Value);
                }

                // Get double vector parameter values
                for (const FDoubleVectorParameterValue& DoubleVectorParam : MatInstance->DoubleVectorParameterValues)
                {
                    FVector4d Value = DoubleVectorParam.ParameterValue;
                    MatMetadata.VectorParameters.Add(DoubleVectorParam.ParameterInfo.Name.ToString(), FLinearColor((float)Value.X, (float)Value.Y, (float)Value.Z, (float)Value.W));
                }

                // Add base property overrides info
                const FMaterialInstanceBasePropertyOverrides& BaseOverrides = MatInstance->BasePropertyOverrides;
                if (BaseOverrides.bOverride_OpacityMaskClipValue)
                {
                    MatMetadata.OpacityMaskClipValue = BaseOverrides.OpacityMaskClipValue;
                }
                if (BaseOverrides.bOverride_BlendMode)
                {
                    MatMetadata.BlendMode = GetBlendModeString(BaseOverrides.BlendMode);
                }
                if (BaseOverrides.bOverride_ShadingModel)
                {
                    MatMetadata.ShadingModel = GetShadingModelString((EMaterialShadingModel)BaseOverrides.ShadingModel);
                }
                if (BaseOverrides.bOverride_TwoSided)
                {
                    MatMetadata.bTwoSided = BaseOverrides.TwoSided;
                }
                if (BaseOverrides.bOverride_bIsThinSurface)
                {
                    MatMetadata.bIsThinSurface = BaseOverrides.bIsThinSurface;
                }
                if (BaseOverrides.bOverride_bHasPixelAnimation)
                {
                    MatMetadata.bHasPixelAnimation = BaseOverrides.bHasPixelAnimation;
                }
                if (BaseOverrides.bOverride_bEnableTessellation)
                {
                    MatMetadata.bEnableTessellation = BaseOverrides.bEnableTessellation;
                }
                if (BaseOverrides.bOverride_MaxWorldPositionOffsetDisplacement)
                {
                    MatMetadata.MaxWorldPositionOffsetDisplacement = BaseOverrides.MaxWorldPositionOffsetDisplacement;
                }
            }

            MaterialsMetadata.Add(MatMetadata);
        }
    }

    return MaterialsMetadata;
}

FJsonObjectBP UMetaDataBPLib::ActorMetadataToJson(const FActorSemanticMetadata& Metadata)
{
    TArray<FString> Keys;
    TArray<FJsonObjectBP> Values;

    Keys.Add(TEXT("ObjectName"));
    Values.Add(FJsonObjectBP(Metadata.ObjectName));

    Keys.Add(TEXT("ClassName"));
    Values.Add(FJsonObjectBP(Metadata.ClassName));

    Keys.Add(TEXT("ClassPath"));
    Values.Add(FJsonObjectBP(Metadata.ClassPath));

    TMap<FString, FJsonObjectBP> BoundingBoxMap;
    BoundingBoxMap.Add(TEXT("MinX"), FJsonObjectBP(static_cast<float>(Metadata.BoundingBoxMin.X)));
    BoundingBoxMap.Add(TEXT("MinY"), FJsonObjectBP(static_cast<float>(Metadata.BoundingBoxMin.Y)));
    BoundingBoxMap.Add(TEXT("MinZ"), FJsonObjectBP(static_cast<float>(Metadata.BoundingBoxMin.Z)));
    BoundingBoxMap.Add(TEXT("MaxX"), FJsonObjectBP(static_cast<float>(Metadata.BoundingBoxMax.X)));
    BoundingBoxMap.Add(TEXT("MaxY"), FJsonObjectBP(static_cast<float>(Metadata.BoundingBoxMax.Y)));
    BoundingBoxMap.Add(TEXT("MaxZ"), FJsonObjectBP(static_cast<float>(Metadata.BoundingBoxMax.Z)));

    TMap<FString, FJsonObjectBP> ScaleInfo;
    ScaleInfo.Add(TEXT("BoundingBox"), FJsonObjectBP(BoundingBoxMap));
    ScaleInfo.Add(TEXT("Position"), FJsonObjectBP(Metadata.Position));
    ScaleInfo.Add(TEXT("Rotation"), FJsonObjectBP(Metadata.Rotation));
    ScaleInfo.Add(TEXT("Scale"), FJsonObjectBP(Metadata.Scale));

    Keys.Add(TEXT("ScaleInfo"));
    Values.Add(FJsonObjectBP(ScaleInfo));

    TArray<FJsonObjectBP> MaterialsArray;
    for (const FString& MatName : Metadata.MaterialNames)
    {
        MaterialsArray.Add(FJsonObjectBP(MatName));
    }
    Keys.Add(TEXT("MaterialNames"));
    Values.Add(FJsonObjectBP(MaterialsArray));

    FString ColorStr = FString::Printf(TEXT("%d,%d,%d"), Metadata.AnnotationColor.R, Metadata.AnnotationColor.G, Metadata.AnnotationColor.B);
    Keys.Add(TEXT("AnnotationColor"));
    Values.Add(FJsonObjectBP(ColorStr));

    return USerializeBPLib::TMapToJson(Keys, Values);
}

FJsonObjectBP UMetaDataBPLib::ClassMetadataToJson(const FClassSemanticMetadata& Metadata)
{
    TArray<FString> Keys;
    TArray<FJsonObjectBP> Values;

    Keys.Add(TEXT("ClassName"));
    Values.Add(FJsonObjectBP(Metadata.ClassName));

    Keys.Add(TEXT("ClassPath"));
    Values.Add(FJsonObjectBP(Metadata.ClassPath));

    Keys.Add(TEXT("ClassType"));
    Values.Add(FJsonObjectBP(Metadata.ClassType));

    TArray<FJsonObjectBP> InstancesArray;
    for (const FString& InstanceName : Metadata.InstanceNames)
    {
        InstancesArray.Add(FJsonObjectBP(InstanceName));
    }
    Keys.Add(TEXT("ClassInstances"));
    Values.Add(FJsonObjectBP(InstancesArray));

    TArray<FJsonObjectBP> MaterialsArray;
    for (const FString& MatName : Metadata.MaterialNames)
    {
        MaterialsArray.Add(FJsonObjectBP(MatName));
    }
    Keys.Add(TEXT("MaterialNames"));
    Values.Add(FJsonObjectBP(MaterialsArray));

    return USerializeBPLib::TMapToJson(Keys, Values);
}

FJsonObjectBP UMetaDataBPLib::MaterialMetadataToJson(const FMaterialSemanticMetadata& Metadata)
{
    TArray<FString> Keys;
    TArray<FJsonObjectBP> Values;

    Keys.Add(TEXT("MaterialName"));
    Values.Add(FJsonObjectBP(Metadata.MaterialName));

    Keys.Add(TEXT("MaterialPath"));
    Values.Add(FJsonObjectBP(Metadata.MaterialPath));

    Keys.Add(TEXT("ParentMaterial"));
    Values.Add(FJsonObjectBP(Metadata.ParentMaterial));

    Keys.Add(TEXT("MaterialDomain"));
    Values.Add(FJsonObjectBP(Metadata.MaterialDomain));

    Keys.Add(TEXT("BlendMode"));
    Values.Add(FJsonObjectBP(Metadata.BlendMode));

    Keys.Add(TEXT("ShadingModel"));
    Values.Add(FJsonObjectBP(Metadata.ShadingModel));

    Keys.Add(TEXT("bTwoSided"));
    Values.Add(FJsonObjectBP(Metadata.bTwoSided ? 1 : 0));

    Keys.Add(TEXT("bIsThinSurface"));
    Values.Add(FJsonObjectBP(Metadata.bIsThinSurface ? 1 : 0));

    Keys.Add(TEXT("bIsMasked"));
    Values.Add(FJsonObjectBP(Metadata.bIsMasked ? 1 : 0));

    Keys.Add(TEXT("OpacityMaskClipValue"));
    Values.Add(FJsonObjectBP(Metadata.OpacityMaskClipValue));

    // Physical material properties
    Keys.Add(TEXT("Roughness"));
    Values.Add(FJsonObjectBP(Metadata.Roughness));

    Keys.Add(TEXT("Metallic"));
    Values.Add(FJsonObjectBP(Metadata.Metallic));

    Keys.Add(TEXT("Specular"));
    Values.Add(FJsonObjectBP(Metadata.Specular));

    Keys.Add(TEXT("Anisotropy"));
    Values.Add(FJsonObjectBP(Metadata.Anisotropy));

    Keys.Add(TEXT("Opacity"));
    Values.Add(FJsonObjectBP(Metadata.Opacity));

    Keys.Add(TEXT("ClearCoat"));
    Values.Add(FJsonObjectBP(Metadata.ClearCoat));

    Keys.Add(TEXT("ClearCoatRoughness"));
    Values.Add(FJsonObjectBP(Metadata.ClearCoatRoughness));

    Keys.Add(TEXT("AmbientOcclusion"));
    Values.Add(FJsonObjectBP(Metadata.AmbientOcclusion));

    Keys.Add(TEXT("Refraction"));
    Values.Add(FJsonObjectBP(Metadata.Refraction));

    TMap<FString, FJsonObjectBP> BaseColorMap;
    BaseColorMap.Add(TEXT("R"), FJsonObjectBP(Metadata.BaseColor.R));
    BaseColorMap.Add(TEXT("G"), FJsonObjectBP(Metadata.BaseColor.G));
    BaseColorMap.Add(TEXT("B"), FJsonObjectBP(Metadata.BaseColor.B));
    BaseColorMap.Add(TEXT("A"), FJsonObjectBP(Metadata.BaseColor.A));
    Keys.Add(TEXT("BaseColor"));
    Values.Add(FJsonObjectBP(BaseColorMap));

    TMap<FString, FJsonObjectBP> SubsurfaceColorMap;
    SubsurfaceColorMap.Add(TEXT("R"), FJsonObjectBP(Metadata.SubsurfaceColor.R));
    SubsurfaceColorMap.Add(TEXT("G"), FJsonObjectBP(Metadata.SubsurfaceColor.G));
    SubsurfaceColorMap.Add(TEXT("B"), FJsonObjectBP(Metadata.SubsurfaceColor.B));
    SubsurfaceColorMap.Add(TEXT("A"), FJsonObjectBP(Metadata.SubsurfaceColor.A));
    Keys.Add(TEXT("SubsurfaceColor"));
    Values.Add(FJsonObjectBP(SubsurfaceColorMap));

    TMap<FString, FJsonObjectBP> EmissiveColorMap;
    EmissiveColorMap.Add(TEXT("R"), FJsonObjectBP(Metadata.EmissiveColor.R));
    EmissiveColorMap.Add(TEXT("G"), FJsonObjectBP(Metadata.EmissiveColor.G));
    EmissiveColorMap.Add(TEXT("B"), FJsonObjectBP(Metadata.EmissiveColor.B));
    EmissiveColorMap.Add(TEXT("A"), FJsonObjectBP(Metadata.EmissiveColor.A));
    Keys.Add(TEXT("EmissiveColor"));
    Values.Add(FJsonObjectBP(EmissiveColorMap));

    Keys.Add(TEXT("bCastDynamicShadowAsMasked"));
    Values.Add(FJsonObjectBP(Metadata.bCastDynamicShadowAsMasked ? 1 : 0));

    Keys.Add(TEXT("bOutputTranslucentVelocity"));
    Values.Add(FJsonObjectBP(Metadata.bOutputTranslucentVelocity ? 1 : 0));

    Keys.Add(TEXT("bHasPixelAnimation"));
    Values.Add(FJsonObjectBP(Metadata.bHasPixelAnimation ? 1 : 0));

    Keys.Add(TEXT("bEnableTessellation"));
    Values.Add(FJsonObjectBP(Metadata.bEnableTessellation ? 1 : 0));

    Keys.Add(TEXT("bHasMaterialLayers"));
    Values.Add(FJsonObjectBP(Metadata.bHasMaterialLayers ? 1 : 0));

    Keys.Add(TEXT("bHasRuntimeVirtualTextureOutput"));
    Values.Add(FJsonObjectBP(Metadata.bHasRuntimeVirtualTextureOutput ? 1 : 0));

    Keys.Add(TEXT("bHasSceneColor"));
    Values.Add(FJsonObjectBP(Metadata.bHasSceneColor ? 1 : 0));

    Keys.Add(TEXT("bHasPerInstanceCustomData"));
    Values.Add(FJsonObjectBP(Metadata.bHasPerInstanceCustomData ? 1 : 0));

    Keys.Add(TEXT("bHasWorldPositionOffset"));
    Values.Add(FJsonObjectBP(Metadata.bHasWorldPositionOffset ? 1 : 0));

    Keys.Add(TEXT("bHasVertexInterpolator"));
    Values.Add(FJsonObjectBP(Metadata.bHasVertexInterpolator ? 1 : 0));

    Keys.Add(TEXT("bHasCustomizedUVs"));
    Values.Add(FJsonObjectBP(Metadata.bHasCustomizedUVs ? 1 : 0));

    Keys.Add(TEXT("bHasMeshPaintTexture"));
    Values.Add(FJsonObjectBP(Metadata.bHasMeshPaintTexture ? 1 : 0));

    Keys.Add(TEXT("bCastsRayTracedShadows"));
    Values.Add(FJsonObjectBP(Metadata.bCastsRayTracedShadows ? 1 : 0));

    Keys.Add(TEXT("bIsDisplacementFadeEnabled"));
    Values.Add(FJsonObjectBP(Metadata.bIsDisplacementFadeEnabled ? 1 : 0));

    Keys.Add(TEXT("MaxWorldPositionOffsetDisplacement"));
    Values.Add(FJsonObjectBP(Metadata.MaxWorldPositionOffsetDisplacement));

    TArray<FJsonObjectBP> ReferencedTexturesArray;
    for (const FTextureSemanticMetadata& TextureMetadata : Metadata.ReferencedTextures)
    {
        ReferencedTexturesArray.Add(TextureMetadataToJson(TextureMetadata));
    }
    Keys.Add(TEXT("ReferencedTextures"));
    Values.Add(FJsonObjectBP(ReferencedTexturesArray));

    TArray<FString> TextureKeys;
    TArray<FJsonObjectBP> TextureValues;
    for (const auto& Pair : Metadata.TextureMaps)
    {
        TextureKeys.Add(Pair.Key);
        TextureValues.Add(FJsonObjectBP(Pair.Value));
    }
    Keys.Add(TEXT("TextureMaps"));
    Values.Add(USerializeBPLib::TMapToJson(TextureKeys, TextureValues));

    TArray<FString> ScalarKeys;
    TArray<FJsonObjectBP> ScalarValues;
    for (const auto& Pair : Metadata.ScalarParameters)
    {
        ScalarKeys.Add(Pair.Key);
        ScalarValues.Add(FJsonObjectBP(Pair.Value));
    }
    Keys.Add(TEXT("ScalarParameters"));
    Values.Add(USerializeBPLib::TMapToJson(ScalarKeys, ScalarValues));

    TArray<FString> VectorKeys;
    TArray<FJsonObjectBP> VectorValues;
    for (const auto& Pair : Metadata.VectorParameters)
    {
        TMap<FString, FJsonObjectBP> ColorMap;
        ColorMap.Add(TEXT("R"), FJsonObjectBP(Pair.Value.R));
        ColorMap.Add(TEXT("G"), FJsonObjectBP(Pair.Value.G));
        ColorMap.Add(TEXT("B"), FJsonObjectBP(Pair.Value.B));
        ColorMap.Add(TEXT("A"), FJsonObjectBP(Pair.Value.A));
        VectorKeys.Add(Pair.Key);
        VectorValues.Add(FJsonObjectBP(ColorMap));
    }
    Keys.Add(TEXT("VectorParameters"));
    Values.Add(USerializeBPLib::TMapToJson(VectorKeys, VectorValues));

    // Static switch parameters
    TArray<FString> StaticSwitchKeys;
    TArray<FJsonObjectBP> StaticSwitchValues;
    for (const auto& Pair : Metadata.StaticSwitchParameters)
    {
        StaticSwitchKeys.Add(Pair.Key);
        StaticSwitchValues.Add(FJsonObjectBP(Pair.Value ? 1 : 0));
    }
    Keys.Add(TEXT("StaticSwitchParameters"));
    Values.Add(USerializeBPLib::TMapToJson(StaticSwitchKeys, StaticSwitchValues));

    // Add comprehensive AllParameters section with type info and override status
    TMap<FString, FJsonObjectBP> AllParamsMap;

    // Scalar parameters with metadata
    TArray<FJsonObjectBP> ScalarParamsDetailed;
    for (const auto& Pair : Metadata.ScalarParameters)
    {
        TMap<FString, FJsonObjectBP> ParamDetail;
        ParamDetail.Add(TEXT("Name"), FJsonObjectBP(Pair.Key));
        ParamDetail.Add(TEXT("Value"), FJsonObjectBP(Pair.Value));
        ParamDetail.Add(TEXT("Type"), FJsonObjectBP(TEXT("Scalar")));
        ParamDetail.Add(TEXT("bOverride"), FJsonObjectBP(1));  // If in this list, it's overridden
        ScalarParamsDetailed.Add(FJsonObjectBP(ParamDetail));
    }
    AllParamsMap.Add(TEXT("Scalars"), FJsonObjectBP(ScalarParamsDetailed));

    // Vector parameters with metadata
    TArray<FJsonObjectBP> VectorParamsDetailed;
    for (const auto& Pair : Metadata.VectorParameters)
    {
        TMap<FString, FJsonObjectBP> ParamDetail;
        ParamDetail.Add(TEXT("Name"), FJsonObjectBP(Pair.Key));
        TMap<FString, FJsonObjectBP> ColorMap;
        ColorMap.Add(TEXT("R"), FJsonObjectBP(Pair.Value.R));
        ColorMap.Add(TEXT("G"), FJsonObjectBP(Pair.Value.G));
        ColorMap.Add(TEXT("B"), FJsonObjectBP(Pair.Value.B));
        ColorMap.Add(TEXT("A"), FJsonObjectBP(Pair.Value.A));
        ParamDetail.Add(TEXT("Value"), FJsonObjectBP(ColorMap));
        ParamDetail.Add(TEXT("Type"), FJsonObjectBP(TEXT("Vector")));
        ParamDetail.Add(TEXT("bOverride"), FJsonObjectBP(1));
        VectorParamsDetailed.Add(FJsonObjectBP(ParamDetail));
    }
    AllParamsMap.Add(TEXT("Vectors"), FJsonObjectBP(VectorParamsDetailed));

    // Texture parameters with metadata
    TArray<FJsonObjectBP> TextureParamsDetailed;
    for (const auto& Pair : Metadata.TextureMaps)
    {
        TMap<FString, FJsonObjectBP> ParamDetail;
        ParamDetail.Add(TEXT("Name"), FJsonObjectBP(Pair.Key));
        ParamDetail.Add(TEXT("Value"), FJsonObjectBP(Pair.Value));
        ParamDetail.Add(TEXT("Type"), FJsonObjectBP(TEXT("Texture")));
        ParamDetail.Add(TEXT("bOverride"), FJsonObjectBP(1));
        TextureParamsDetailed.Add(FJsonObjectBP(ParamDetail));
    }
    AllParamsMap.Add(TEXT("Textures"), FJsonObjectBP(TextureParamsDetailed));

    // Static switch parameters with metadata
    TArray<FJsonObjectBP> SwitchParamsDetailed;
    for (const auto& Pair : Metadata.StaticSwitchParameters)
    {
        TMap<FString, FJsonObjectBP> ParamDetail;
        ParamDetail.Add(TEXT("Name"), FJsonObjectBP(Pair.Key));
        ParamDetail.Add(TEXT("Value"), FJsonObjectBP(Pair.Value ? 1 : 0));
        ParamDetail.Add(TEXT("Type"), FJsonObjectBP(TEXT("StaticSwitch")));
        ParamDetail.Add(TEXT("bOverride"), FJsonObjectBP(1));
        SwitchParamsDetailed.Add(FJsonObjectBP(ParamDetail));
    }
    AllParamsMap.Add(TEXT("StaticSwitches"), FJsonObjectBP(SwitchParamsDetailed));

    Keys.Add(TEXT("AllParameters"));
    Values.Add(FJsonObjectBP(AllParamsMap));

    return USerializeBPLib::TMapToJson(Keys, Values);
}

TMap<FString, FJsonObjectBP> UMetaDataBPLib::GetSceneSemanticAnnotations(UWorld* World)
{
    TMap<FString, FJsonObjectBP> Result;
    if (!IsValid(World))
    {
        return Result;
    }

    TMap<FString, FActorSemanticMetadata> ObjectsMap;
    TMap<FString, FClassSemanticMetadata> ClassesMap;
    TMap<FString, FMaterialSemanticMetadata> MaterialsMap;

    for (TActorIterator<AActor> ActorItr(World); ActorItr; ++ActorItr)
    {
        AActor* Actor = *ActorItr;
        if (!IsValid(Actor))
        {
            continue;
        }

        FActorSemanticMetadata ActorMetadata = GetActorSemanticMetadata(Actor);
        if (!ActorMetadata.ObjectName.IsEmpty())
        {
            ObjectsMap.Add(ActorMetadata.ObjectName, ActorMetadata);
        }

        FString ClassName = Actor->GetClass()->GetName();
        if (ClassesMap.Contains(ClassName))
        {
            ClassesMap[ClassName].InstanceNames.Add(Actor->GetName());
        }
        else
        {
            FClassSemanticMetadata ClassMetadata = GetClassSemanticMetadata(Actor);
            ClassesMap.Add(ClassName, ClassMetadata);
        }

        TArray<FMaterialSemanticMetadata> ActorMaterials = GetActorMaterialsMetadata(Actor);
        for (const FMaterialSemanticMetadata& MatMetadata : ActorMaterials)
        {
            if (!MaterialsMap.Contains(MatMetadata.MaterialName))
            {
                MaterialsMap.Add(MatMetadata.MaterialName, MatMetadata);
            }
        }
    }

    TMap<FString, FJsonObjectBP> ObjectsJson;
    for (const auto& Pair : ObjectsMap)
    {
        ObjectsJson.Add(Pair.Key, ActorMetadataToJson(Pair.Value));
    }
    Result.Add(TEXT("Objects"), FJsonObjectBP(ObjectsJson));

    TMap<FString, FJsonObjectBP> ClassesJson;
    for (const auto& Pair : ClassesMap)
    {
        ClassesJson.Add(Pair.Key, ClassMetadataToJson(Pair.Value));
    }
    Result.Add(TEXT("Classes"), FJsonObjectBP(ClassesJson));

    TMap<FString, FJsonObjectBP> MaterialsJson;
    for (const auto& Pair : MaterialsMap)
    {
        MaterialsJson.Add(Pair.Key, MaterialMetadataToJson(Pair.Value));
    }
    Result.Add(TEXT("Materials"), FJsonObjectBP(MaterialsJson));

    return Result;
}

FJsonObjectBP UMetaDataBPLib::GetSemanticAnnotationsJson(UWorld* World)
{
    TMap<FString, FJsonObjectBP> Annotations = GetSceneSemanticAnnotations(World);

    TArray<FString> Keys;
    TArray<FJsonObjectBP> Values;

    for (const auto& Pair : Annotations)
    {
        Keys.Add(Pair.Key);
        Values.Add(Pair.Value);
    }

    return USerializeBPLib::TMapToJson(Keys, Values);
}

FString UMetaDataBPLib::GetBlendModeString(EBlendMode BlendMode)
{
    switch (BlendMode)
    {
    case BLEND_Opaque: return TEXT("Opaque");
    case BLEND_Masked: return TEXT("Masked");
    case BLEND_Translucent: return TEXT("Translucent");
    case BLEND_Additive: return TEXT("Additive");
    case BLEND_Modulate: return TEXT("Modulate");
    case BLEND_AlphaComposite: return TEXT("AlphaComposite");
    case BLEND_AlphaHoldout: return TEXT("AlphaHoldout");
    default: return TEXT("Unknown");
    }
}

FString UMetaDataBPLib::GetShadingModelString(EMaterialShadingModel ShadingModel)
{
    switch (ShadingModel)
    {
    case MSM_Unlit: return TEXT("Unlit");
    case MSM_DefaultLit: return TEXT("DefaultLit");
    case MSM_Subsurface: return TEXT("Subsurface");
    case MSM_PreintegratedSkin: return TEXT("PreintegratedSkin");
    case MSM_ClearCoat: return TEXT("ClearCoat");
    case MSM_SubsurfaceProfile: return TEXT("SubsurfaceProfile");
    case MSM_TwoSidedFoliage: return TEXT("TwoSidedFoliage");
    case MSM_Hair: return TEXT("Hair");
    case MSM_Cloth: return TEXT("Cloth");
    case MSM_Eye: return TEXT("Eye");
    case MSM_SingleLayerWater: return TEXT("SingleLayerWater");
    case MSM_ThinTranslucent: return TEXT("ThinTranslucent");
    case MSM_Strata: return TEXT("Strata");
    default: return TEXT("Unknown");
    }
}

FString UMetaDataBPLib::GetMaterialDomainString(EMaterialDomain Domain)
{
    switch (Domain)
    {
    case MD_Surface: return TEXT("Surface");
    case MD_DeferredDecal: return TEXT("DeferredDecal");
    case MD_LightFunction: return TEXT("LightFunction");
    case MD_Volume: return TEXT("Volume");
    case MD_PostProcess: return TEXT("PostProcess");
    case MD_UI: return TEXT("UI");
    case MD_RuntimeVirtualTexture: return TEXT("RuntimeVirtualTexture");
    default: return TEXT("Unknown");
    }
}

FTextureSemanticMetadata UMetaDataBPLib::GetTextureSemanticMetadata(UTexture* Texture)
{
    FTextureSemanticMetadata Metadata;
    if (!IsValid(Texture))
    {
        return Metadata;
    }

    Metadata.TextureName = Texture->GetName();
    Metadata.TexturePath = Texture->GetPathName();
    Metadata.TextureClass = GetTextureClassString(Texture->GetTextureClass());
    Metadata.bIsValid = true;

    // Get texture resource size info
    FTextureResource* Resource = Texture->GetResource();
    if (Resource)
    {
        Metadata.SizeX = Resource->GetSizeX();
        Metadata.SizeY = Resource->GetSizeY();
        Metadata.NumMips = Resource->GetCurrentMipCount();
    }

    // Get compression settings
    Metadata.CompressionSettings = GetTextureCompressionSettingsString(Texture->CompressionSettings);
    Metadata.bSRGB = Texture->SRGB;

    return Metadata;
}

FJsonObjectBP UMetaDataBPLib::TextureMetadataToJson(const FTextureSemanticMetadata& Metadata)
{
    TArray<FString> Keys;
    TArray<FJsonObjectBP> Values;

    Keys.Add(TEXT("TextureName"));
    Values.Add(FJsonObjectBP(Metadata.TextureName));

    Keys.Add(TEXT("TexturePath"));
    Values.Add(FJsonObjectBP(Metadata.TexturePath));

    Keys.Add(TEXT("TextureClass"));
    Values.Add(FJsonObjectBP(Metadata.TextureClass));

    Keys.Add(TEXT("SizeX"));
    Values.Add(FJsonObjectBP(Metadata.SizeX));

    Keys.Add(TEXT("SizeY"));
    Values.Add(FJsonObjectBP(Metadata.SizeY));

    Keys.Add(TEXT("NumMips"));
    Values.Add(FJsonObjectBP(Metadata.NumMips));

    Keys.Add(TEXT("CompressionSettings"));
    Values.Add(FJsonObjectBP(Metadata.CompressionSettings));

    Keys.Add(TEXT("bSRGB"));
    Values.Add(FJsonObjectBP(Metadata.bSRGB ? 1 : 0));

    return USerializeBPLib::TMapToJson(Keys, Values);
}

FString UMetaDataBPLib::GetTextureCompressionSettingsString(TextureCompressionSettings CompressionSettings)
{
    switch (CompressionSettings)
    {
    case TC_Default: return TEXT("Default");
    case TC_Normalmap: return TEXT("Normalmap");
    case TC_Masks: return TEXT("Masks");
    case TC_Grayscale: return TEXT("Grayscale");
    case TC_Displacementmap: return TEXT("Displacementmap");
    case TC_VectorDisplacementmap: return TEXT("VectorDisplacementmap");
    case TC_HDR: return TEXT("HDR");
    case TC_EditorIcon: return TEXT("EditorIcon");
    case TC_Alpha: return TEXT("Alpha");
    case TC_DistanceFieldFont: return TEXT("DistanceFieldFont");
    case TC_BC7: return TEXT("BC7");
    case TC_HalfFloat: return TEXT("HalfFloat");
    case TC_LQ: return TEXT("LQ");
    case TC_SingleFloat: return TEXT("SingleFloat");
    default: return TEXT("Unknown");
    }
}

FString UMetaDataBPLib::GetTextureClassString(ETextureClass TextureClass)
{
    switch (TextureClass)
    {
    case ETextureClass::Invalid: return TEXT("Invalid");
    case ETextureClass::TwoD: return TEXT("2D");
    case ETextureClass::Cube: return TEXT("Cube");
    case ETextureClass::Array: return TEXT("Array");
    case ETextureClass::CubeArray: return TEXT("CubeArray");
    case ETextureClass::Volume: return TEXT("Volume");
    case ETextureClass::TwoDDynamic: return TEXT("2DDynamic");
    case ETextureClass::RenderTarget: return TEXT("RenderTarget");
    default: return TEXT("Unknown");
    }
}
