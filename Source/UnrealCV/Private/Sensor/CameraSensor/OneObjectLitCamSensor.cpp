#include "OneObjectLitCamSensor.h"
#include "UnrealcvServer.h"
#include "UnrealcvLog.h"

UOneObjectLitCamSensor::UOneObjectLitCamSensor(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
	ShowFlags.SetLighting(false);
	ShowFlags.SetSkyLighting(false);
	ShowFlags.SetFog(false);
	ShowFlags.SetVolumetricFog(false);
	ShowFlags.SetPostProcessing(true);
	ShowFlags.SetPostProcessMaterial(true);
	ShowFlags.SetCloud(false);
	ShowFlags.SetAtmosphere(false);
	ShowFlags.SetLumenGlobalIllumination(false);
	ShowFlags.SetGlobalIllumination(false);
	ShowFlags.SetLumenReflections(false);
	ShowFlags.SetScreenSpaceReflections(false);
	ShowFlags.SetDistanceFieldAO(false);
	ShowFlags.SetScreenSpaceAO(false);
	ShowFlags.SetAntiAliasing(true);
	ShowFlags.SetTemporalAA(true);

	// FString OneObjOpacityMaterialPath = TEXT("Material'/UnrealCV/OneObjOpacity.OneObjOpacity'");
	// ConstructorHelpers::FObjectFinder<UMaterial> Material(*OneObjOpacityMaterialPath);

	// if (Material.Object)
	// {
	// 	OneObjOpacityMaterial = Material.Object;
	// }
	// else
	// {
	// 	UE_LOG(LogTemp, Error, TEXT("OneObjOpacity material not found at %s"), *OneObjOpacityMaterialPath);
	// }
	UE_LOG(LogTemp, Warning, TEXT("OneObjectLitCamSensor::ctor: %p"), this);
}

void UOneObjectLitCamSensor::InitTextureTarget(int InFilmWidth, int InFilmHeight)
{
	UE_LOG(LogTemp, Warning, TEXT("OneObjectLitCamSensor::InitTextureTarget: %d x %d"), InFilmWidth, InFilmHeight);
	Super::InitTextureTarget(InFilmWidth, InFilmHeight);
	UE_LOG(LogTemp, Warning, TEXT("OneObjectLitCamSensor::InitTextureTarget: %d x %d"), TextureTarget->SizeX, TextureTarget->SizeY);

	// if (OneObjOpacityMaterial)
	// {
	// 	UMaterialInstanceDynamic* PostProcessMaterialInstance = UMaterialInstanceDynamic::Create(OneObjOpacityMaterial, nullptr);
	// 	if (PostProcessMaterialInstance)
	// 	{
	// 		SetPostProcessMaterial(PostProcessMaterialInstance);
	// 	}
	// 	else
	// 	{
	// 		UE_LOG(LogTemp, Error, TEXT("%s: Could not create the material instance dynamic"), *FString(__FUNCTION__));
	// 	}
	// }
}
