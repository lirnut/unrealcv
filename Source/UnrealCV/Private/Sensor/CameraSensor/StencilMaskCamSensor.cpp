#include "StencilMaskCamSensor.h"
#include "UnrealcvLog.h"
#include "UnrealcvStats.h"
#include "Server/ServerConfig.h"
#include "Server/UnrealcvServer.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "BPFunctionLib/StencilBPLib.h"

DECLARE_CYCLE_STAT(TEXT("UStencilMaskCamSensor::CaptureStencilMask"), STAT_CaptureStencilMask, STATGROUP_UnrealCV);

UStencilMaskCamSensor::UStencilMaskCamSensor(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;

	ShowFlags.SetLighting(false);
	ShowFlags.SetDynamicShadows(false);
	ShowFlags.SetContactShadows(false);
	ShowFlags.SetPostProcessing(true);
	ShowFlags.SetPostProcessMaterial(true);

	FString StencilMaskMaterialPath = TEXT("Material'/UnrealCV/OneObjMask.OneObjMask'");
	ConstructorHelpers::FObjectFinder<UMaterial> Material(*StencilMaskMaterialPath);

	if (Material.Object)
	{
		StencilMaskMaterial = Material.Object;
		UE_LOG(LogUnrealCV, Log, TEXT("StencilMaskCamSensor: Loaded OneObjMask material"));

		UMaterialInstanceDynamic* PostProcessMaterialInstance = UMaterialInstanceDynamic::Create(StencilMaskMaterial, nullptr);
		if (!PostProcessMaterialInstance)
		{
			UE_LOG(LogUnrealCV, Error, TEXT("StencilMaskCamSensor: Could not create material instance dynamic"));
		}
		else
		{
			PostProcessMaterialInstance->SetScalarParameterValue(TEXT("StencilIndex"), 1.0f);
			SetPostProcessMaterial(PostProcessMaterialInstance);
			UE_LOG(LogUnrealCV, Log, TEXT("StencilMaskCamSensor: Set StencilIndex=1.0"));
		}
	}
	else
	{
		UE_LOG(LogUnrealCV, Error, TEXT("StencilMaskCamSensor: Failed to load OneObjMask material at %s"), *StencilMaskMaterialPath);
	}
}

void UStencilMaskCamSensor::InitTextureTarget(int filmWidth, int filmHeight)
{
	InitUInt8TextureTarget(filmWidth, filmHeight, true);

	if (StencilMaskMaterial)
	{
		UMaterialInstanceDynamic* PostProcessMaterialInstance = UMaterialInstanceDynamic::Create(StencilMaskMaterial, nullptr);
		if (!PostProcessMaterialInstance)
		{
			UE_LOG(LogUnrealCV, Error, TEXT("StencilMaskCamSensor: Could not create material instance dynamic"));
		}
		else
		{
			PostProcessMaterialInstance->SetScalarParameterValue(TEXT("StencilIndex"), 1.0f);
			SetPostProcessMaterial(PostProcessMaterialInstance);
			UE_LOG(LogUnrealCV, Log, TEXT("StencilMaskCamSensor: Set StencilIndex=1.0 in InitTextureTarget"));
		}
	}
	else
	{
		UE_LOG(LogUnrealCV, Error, TEXT("StencilMaskCamSensor: StencilMaskMaterial is null"));
	}
}

void UStencilMaskCamSensor::SetupForActor(AActor* TargetActor)
{
	if (!IsValid(TargetActor))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("SetupForActor: Invalid target actor"));
		return;
	}

	UStencilBPLib::EnableCustomDepthForActor(TargetActor, 1);

	UE_LOG(LogUnrealCV, Log, TEXT("StencilMaskCamSensor: Setup complete for %s"), *TargetActor->GetName());
}

void UStencilMaskCamSensor::Cleanup(AActor* TargetActor)
{
	if (IsValid(TargetActor))
	{
		UStencilBPLib::DisableCustomDepthForActor(TargetActor);
	}
	UE_LOG(LogUnrealCV, Log, TEXT("StencilMaskCamSensor: Cleaned up for %s"), *GetNameSafe(TargetActor));
}

void UStencilMaskCamSensor::CaptureStencilMask(TArray<FColor>& Image, int& Width, int& Height)
{
	SCOPE_CYCLE_COUNTER(STAT_CaptureStencilMask);
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

void UStencilMaskCamSensor::CaptureStencilMaskToFile(FString Filename)
{
	SCOPE_CYCLE_COUNTER(STAT_CaptureStencilMask);
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
