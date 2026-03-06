// shc @ 2025
#include "FlowCamSensor.h"
#include "UnrealcvServer.h"
#include "UnrealcvLog.h"
#include "Server/ServerConfig.h"

UFlowCamSensor::UFlowCamSensor(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	FServerConfig& Config = FUnrealcvServer::Get().Config;
	bRenderInMainRenderer = Config.bRenderInMainRenderer;
	FString OpticalFlowPPMaterialPath = TEXT("Material'/UnrealCV/OpticalFlowMaterialNative.OpticalFlowMaterialNative'");

	// Assertion failed: IsInGameThread() [File:D:\build\++UE5\Sync\Engine\Source\Runtime\CoreUObject\Private\Serialization\AsyncLoading.cpp] [Line: 7453] 
	// LoadPackageAsync is only thread-safe when using the zenloader (i.e. AsyncLoading2).
	// This error may occur if UnrealCV has been updated, but the project hasn't been fully rebuilt.
	// To resolve this, try loading the material in the BeginPlay method.
	// Note: A similar FObjectFinder is used in the constructor of NormalCamSensor.
	// To fix this issue, try rebuilding the project completely.
	ConstructorHelpers::FObjectFinder<UMaterial> Material(*OpticalFlowPPMaterialPath);

	if (Material.Object)
	{
		OpticalFlowPPMaterial = Material.Object;
		// SetPostProcessMaterial(OpticalFlowPPMaterial);
		UMaterialInstanceDynamic* PostProcessMaterialInstance = UMaterialInstanceDynamic::Create(OpticalFlowPPMaterial, nullptr);
		if (!PostProcessMaterialInstance)
		{
			UE_LOG(LogTemp, Error, TEXT("%s: Could not create the material instance dynamic"), *FString(__FUNCTION__))
		} else {
			PostProcessMaterialInstance->SetScalarParameterValue(TEXT("OpticalFlowScale"), Config.OpticalFlowScale);
			SetPostProcessMaterial(PostProcessMaterialInstance);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("OpticalFlowMaterial not found at %s"), *OpticalFlowPPMaterialPath);
	}
}

void UFlowCamSensor::SetFilmSize(int Width, int Height)
{
	Super::SetFilmSize(Width, Height);
	// if (OpticalFlowPPMaterial)
	// {
	// 	UMaterialInstanceDynamic* PostProcessMaterialInstance = UMaterialInstanceDynamic::Create(OpticalFlowPPMaterial, nullptr);
	// 	if (!PostProcessMaterialInstance)
	// 	{
	// 		UE_LOG(LogTemp, Error, TEXT("%s: Could not create the material instance dynamic"), *FString(__FUNCTION__))
	// 	} else {
	// 		FServerConfig& Config = FUnrealcvServer::Get().Config;
	// 		PostProcessMaterialInstance->SetScalarParameterValue(TEXT("OpticalFlowScale"), Config.OpticalFlowScale);
	// 		SetPostProcessMaterial(PostProcessMaterialInstance);
	// 	}
	// }
}

void UFlowCamSensor::InitTextureTarget(int filmWidth, int filmHeight)
{
	InitUInt8TextureTarget(filmWidth, filmHeight, true);
	if (OpticalFlowPPMaterial)
	{
		UMaterialInstanceDynamic* PostProcessMaterialInstance = UMaterialInstanceDynamic::Create(OpticalFlowPPMaterial, nullptr);
		if (!PostProcessMaterialInstance)
		{
			UE_LOG(LogTemp, Error, TEXT("%s: Could not create the material instance dynamic"), *FString(__FUNCTION__))
		} else {
			FServerConfig& Config = FUnrealcvServer::Get().Config;
			PostProcessMaterialInstance->SetScalarParameterValue(TEXT("OpticalFlowScale"), Config.OpticalFlowScale);
			SetPostProcessMaterial(PostProcessMaterialInstance);
		}
	}
}

void UFlowCamSensor::CaptureFlow(TArray<FColor>& Image, int& Width, int& Height)
{
	if (!CheckTextureTarget())
	{
		InitTextureTarget(this->FilmWidth, this->FilmHeight);
		if (!CheckTextureTarget())
		{
			UE_LOG(LogUnrealCV, Error, TEXT("Failed to initialize TextureTarget."));
			return;
		}
	}
	Capture(Image, Width, Height);
}

void UFlowCamSensor::CaptureFlowToFile(FString Filename)
{
	if (!CheckTextureTarget())
	{
		InitTextureTarget(this->FilmWidth, this->FilmHeight);
		if (!CheckTextureTarget())
		{
			UE_LOG(LogUnrealCV, Error, TEXT("Failed to initialize TextureTarget."));
			return;
		}
	}
	CaptureFastToFile(Filename);
}