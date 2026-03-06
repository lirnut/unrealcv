#pragma once

#include "CoreMinimal.h"
#include "Runtime/Engine/Classes/Kismet/BlueprintFunctionLibrary.h"
#include "Materials/MaterialInterface.h"
#include "BPFunctionLib/JsonObjectBP.h"
#include "MetaDataBPLib.generated.h"

USTRUCT(BlueprintType)
struct FActorSemanticMetadata
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    FString ObjectName;

    UPROPERTY(BlueprintReadWrite)
    FString ClassName;

    UPROPERTY(BlueprintReadWrite)
    FString ClassPath;

    UPROPERTY(BlueprintReadWrite)
    FVector Position = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite)
    FRotator Rotation = FRotator::ZeroRotator;

    UPROPERTY(BlueprintReadWrite)
    FVector Scale = FVector::OneVector;

    UPROPERTY(BlueprintReadWrite)
    FVector BoundingBoxMin = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite)
    FVector BoundingBoxMax = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite)
    TArray<FString> MaterialNames;

    UPROPERTY(BlueprintReadWrite)
    FColor AnnotationColor = FColor::Black;
};

USTRUCT(BlueprintType)
struct FClassSemanticMetadata
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    FString ClassName;

    UPROPERTY(BlueprintReadWrite)
    FString ClassPath;

    UPROPERTY(BlueprintReadWrite)
    FString ClassType;

    UPROPERTY(BlueprintReadWrite)
    TArray<FString> InstanceNames;

    UPROPERTY(BlueprintReadWrite)
    TArray<FString> MaterialNames;
};

USTRUCT(BlueprintType)
struct FTextureSemanticMetadata
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    FString TextureName;

    UPROPERTY(BlueprintReadWrite)
    FString TexturePath;

    UPROPERTY(BlueprintReadWrite)
    FString TextureClass;

    UPROPERTY(BlueprintReadWrite)
    int32 SizeX = 0;

    UPROPERTY(BlueprintReadWrite)
    int32 SizeY = 0;

    UPROPERTY(BlueprintReadWrite)
    int32 NumMips = 0;

    UPROPERTY(BlueprintReadWrite)
    FString CompressionSettings;

    UPROPERTY(BlueprintReadWrite)
    bool bSRGB = false;

    UPROPERTY(BlueprintReadWrite)
    bool bIsValid = false;
};

USTRUCT(BlueprintType)
struct FMaterialSemanticMetadata
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    FString MaterialName;

    UPROPERTY(BlueprintReadWrite)
    FString MaterialPath;

    UPROPERTY(BlueprintReadWrite)
    FString ParentMaterial;

    UPROPERTY(BlueprintReadWrite)
    FString MaterialDomain;

    UPROPERTY(BlueprintReadWrite)
    FString BlendMode;

    UPROPERTY(BlueprintReadWrite)
    FString ShadingModel;

    UPROPERTY(BlueprintReadWrite)
    bool bTwoSided = false;

    UPROPERTY(BlueprintReadWrite)
    bool bIsThinSurface = false;

    UPROPERTY(BlueprintReadWrite)
    bool bIsMasked = false;

    UPROPERTY(BlueprintReadWrite)
    float OpacityMaskClipValue = 0.333f;

    // Direct material physical properties (extracted from parameters or defaults)
    UPROPERTY(BlueprintReadWrite)
    float Roughness = 0.5f;

    UPROPERTY(BlueprintReadWrite)
    float Metallic = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    float Specular = 0.5f;

    UPROPERTY(BlueprintReadWrite)
    float Anisotropy = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    float Opacity = 1.0f;

    UPROPERTY(BlueprintReadWrite)
    float ClearCoat = 1.0f;

    UPROPERTY(BlueprintReadWrite)
    float ClearCoatRoughness = 0.1f;

    UPROPERTY(BlueprintReadWrite)
    float AmbientOcclusion = 1.0f;

    UPROPERTY(BlueprintReadWrite)
    float Refraction = 1.0f;

    UPROPERTY(BlueprintReadWrite)
    FLinearColor BaseColor = FLinearColor::White;

    UPROPERTY(BlueprintReadWrite)
    FLinearColor SubsurfaceColor = FLinearColor::White;

    UPROPERTY(BlueprintReadWrite)
    FLinearColor EmissiveColor = FLinearColor::Black;

    UPROPERTY(BlueprintReadWrite)
    bool bCastDynamicShadowAsMasked = false;

    UPROPERTY(BlueprintReadWrite)
    bool bOutputTranslucentVelocity = false;

    UPROPERTY(BlueprintReadWrite)
    bool bHasPixelAnimation = false;

    UPROPERTY(BlueprintReadWrite)
    bool bEnableTessellation = false;

    UPROPERTY(BlueprintReadWrite)
    bool bHasMaterialLayers = false;

    UPROPERTY(BlueprintReadWrite)
    bool bHasRuntimeVirtualTextureOutput = false;

    UPROPERTY(BlueprintReadWrite)
    bool bHasSceneColor = false;

    UPROPERTY(BlueprintReadWrite)
    bool bHasPerInstanceCustomData = false;

    UPROPERTY(BlueprintReadWrite)
    bool bHasWorldPositionOffset = false;

    UPROPERTY(BlueprintReadWrite)
    bool bHasVertexInterpolator = false;

    UPROPERTY(BlueprintReadWrite)
    bool bHasCustomizedUVs = false;

    UPROPERTY(BlueprintReadWrite)
    bool bHasMeshPaintTexture = false;

    UPROPERTY(BlueprintReadWrite)
    bool bCastsRayTracedShadows = false;

    UPROPERTY(BlueprintReadWrite)
    bool bIsDisplacementFadeEnabled = false;

    UPROPERTY(BlueprintReadWrite)
    float MaxWorldPositionOffsetDisplacement = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    TArray<FTextureSemanticMetadata> ReferencedTextures;

    UPROPERTY(BlueprintReadWrite)
    TMap<FString, FString> TextureMaps;

    UPROPERTY(BlueprintReadWrite)
    TMap<FString, float> ScalarParameters;

    UPROPERTY(BlueprintReadWrite)
    TMap<FString, FLinearColor> VectorParameters;

    UPROPERTY(BlueprintReadWrite)
    TMap<FString, bool> StaticSwitchParameters;
};

UCLASS()
class UNREALCV_API UMetaDataBPLib : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "UnrealCV|SemanticAnnotation")
    static FActorSemanticMetadata GetActorSemanticMetadata(AActor* Actor);

    UFUNCTION(BlueprintCallable, Category = "UnrealCV|SemanticAnnotation")
    static FClassSemanticMetadata GetClassSemanticMetadata(AActor* Actor);

    UFUNCTION(BlueprintCallable, Category = "UnrealCV|SemanticAnnotation")
    static TArray<FMaterialSemanticMetadata> GetActorMaterialsMetadata(AActor* Actor);

    UFUNCTION(BlueprintCallable, Category = "UnrealCV|SemanticAnnotation")
    static FJsonObjectBP ActorMetadataToJson(const FActorSemanticMetadata& Metadata);

    UFUNCTION(BlueprintCallable, Category = "UnrealCV|SemanticAnnotation")
    static FJsonObjectBP ClassMetadataToJson(const FClassSemanticMetadata& Metadata);

    UFUNCTION(BlueprintCallable, Category = "UnrealCV|SemanticAnnotation")
    static FJsonObjectBP MaterialMetadataToJson(const FMaterialSemanticMetadata& Metadata);

    UFUNCTION(BlueprintCallable, Category = "UnrealCV|SemanticAnnotation")
    static FTextureSemanticMetadata GetTextureSemanticMetadata(UTexture* Texture);

    UFUNCTION(BlueprintCallable, Category = "UnrealCV|SemanticAnnotation")
    static FJsonObjectBP TextureMetadataToJson(const FTextureSemanticMetadata& Metadata);

    UFUNCTION(BlueprintCallable, Category = "UnrealCV|SemanticAnnotation")
    static TMap<FString, FJsonObjectBP> GetSceneSemanticAnnotations(UWorld* World);

    UFUNCTION(BlueprintCallable, Category = "UnrealCV|SemanticAnnotation")
    static FJsonObjectBP GetSemanticAnnotationsJson(UWorld* World);

    UFUNCTION(BlueprintPure, Category = "UnrealCV|SemanticAnnotation")
    static FString GetBlendModeString(EBlendMode BlendMode);

    UFUNCTION(BlueprintPure, Category = "UnrealCV|SemanticAnnotation")
    static FString GetShadingModelString(EMaterialShadingModel ShadingModel);

    UFUNCTION(BlueprintPure, Category = "UnrealCV|SemanticAnnotation")
    static FString GetMaterialDomainString(EMaterialDomain Domain);

    UFUNCTION(BlueprintPure, Category = "UnrealCV|SemanticAnnotation")
    static FString GetTextureCompressionSettingsString(TextureCompressionSettings CompressionSettings);

    UFUNCTION(BlueprintPure, Category = "UnrealCV|SemanticAnnotation")
    static FString GetTextureClassString(ETextureClass TextureClass);
};
