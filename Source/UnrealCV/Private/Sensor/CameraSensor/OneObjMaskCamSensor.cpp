#include "OneObjMaskCamSensor.h"
#include "UnrealcvLog.h"
#include "UnrealcvStats.h"
#include "Server/ServerConfig.h"
#include "Server/UnrealcvServer.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "BPFunctionLib/StencilBPLib.h"

DECLARE_CYCLE_STAT(TEXT("UOneObjMaskCamSensor::CaptureOneObjMask"), STAT_CaptureOneObjMask, STATGROUP_UnrealCV);

UOneObjMaskCamSensor::UOneObjMaskCamSensor(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	bRenderInMainRenderer = true;  // optimization
	CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;

	ShowFlags.SetLighting(false);
	ShowFlags.SetDynamicShadows(false);
	ShowFlags.SetContactShadows(false);
	ShowFlags.SetPostProcessing(true);
	ShowFlags.SetPostProcessMaterial(true);

	FString OneObjMaskMaterialPath = TEXT("Material'/UnrealCV/OneObjMask.OneObjMask'");
	ConstructorHelpers::FObjectFinder<UMaterial> Material(*OneObjMaskMaterialPath);

	if (Material.Object)
	{
		OneObjMaskMaterial = Material.Object;
		UE_LOG(LogUnrealCV, Log, TEXT("OneObjMaskCamSensor: Loaded OneObjMask material"));

		UMaterialInstanceDynamic* PostProcessMaterialInstance = UMaterialInstanceDynamic::Create(OneObjMaskMaterial, nullptr);
		if (!PostProcessMaterialInstance)
		{
			UE_LOG(LogUnrealCV, Error, TEXT("OneObjMaskCamSensor: Could not create material instance dynamic"));
		}
		else
		{
			PostProcessMaterialInstance->SetScalarParameterValue(TEXT("StencilIndex"), 1.0f);
			SetPostProcessMaterial(PostProcessMaterialInstance);
			UE_LOG(LogUnrealCV, Log, TEXT("OneObjMaskCamSensor: Set StencilIndex=1.0"));
		}
	}
	else
	{
		UE_LOG(LogUnrealCV, Error, TEXT("OneObjMaskCamSensor: Failed to load OneObjMask material at %s"), *OneObjMaskMaterialPath);
	}
}

void UOneObjMaskCamSensor::InitTextureTarget(int filmWidth, int filmHeight)
{
	InitUInt8TextureTarget(filmWidth, filmHeight, true);

	if (OneObjMaskMaterial)
	{
		UMaterialInstanceDynamic* PostProcessMaterialInstance = UMaterialInstanceDynamic::Create(OneObjMaskMaterial, nullptr);
		if (!PostProcessMaterialInstance)
		{
			UE_LOG(LogUnrealCV, Error, TEXT("OneObjMaskCamSensor: Could not create material instance dynamic"));
		}
		else
		{
			PostProcessMaterialInstance->SetScalarParameterValue(TEXT("StencilIndex"), 1.0f);
			SetPostProcessMaterial(PostProcessMaterialInstance);
			UE_LOG(LogUnrealCV, Log, TEXT("OneObjMaskCamSensor: Set StencilIndex=1.0 in InitTextureTarget"));
		}
	}
	else
	{
		UE_LOG(LogUnrealCV, Error, TEXT("OneObjMaskCamSensor: OneObjMaskMaterial is null"));
	}
}

void UOneObjMaskCamSensor::SetupForActor(AActor* TargetActor)
{
	if (!IsValid(TargetActor))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("SetupForActor: Invalid target actor"));
		return;
	}

	UStencilBPLib::EnableCustomDepthForActor(TargetActor, 1);

	UE_LOG(LogUnrealCV, Log, TEXT("OneObjMaskCamSensor: Setup complete for %s"), *TargetActor->GetName());
}

void UOneObjMaskCamSensor::Cleanup(AActor* TargetActor)
{
	if (IsValid(TargetActor))
	{
		UStencilBPLib::DisableCustomDepthForActor(TargetActor);
	}
	UE_LOG(LogUnrealCV, Log, TEXT("OneObjMaskCamSensor: Cleaned up for %s"), *GetNameSafe(TargetActor));
}

void UOneObjMaskCamSensor::CaptureOneObjMask(TArray<FColor>& Image, int& Width, int& Height)
{
	SCOPE_CYCLE_COUNTER(STAT_CaptureOneObjMask);
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

void UOneObjMaskCamSensor::CaptureOneObjMaskToFile(FString Filename)
{
	SCOPE_CYCLE_COUNTER(STAT_CaptureOneObjMask);
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
