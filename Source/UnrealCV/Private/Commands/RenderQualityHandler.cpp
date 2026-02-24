#include "RenderQualityHandler.h"
#include "Sensor/CameraSensor/MovieQualityRenderComponent.h"
#include "Utils/StrFormatter.h"

void FMQRCHandler::RegisterCommands()
{
	CommandDispatcher->BindCommand(
		TEXT("vget /mqrc/antialiasing"),
		FDispatcherDelegate::CreateRaw(this, &FMQRCHandler::GetAntiAliasingMethod),
		TEXT("Get anti-aliasing method")
	);

	CommandDispatcher->BindCommand(
		TEXT("vset /mqrc/antialiasing [str]"),
		FDispatcherDelegate::CreateRaw(this, &FMQRCHandler::SetAntiAliasingMethod),
		TEXT("Set anti-aliasing method: fxaa, temporal_aa, tsr, none")
	);

	CommandDispatcher->BindCommand(
		TEXT("vget /mqrc/exposure_method"),
		FDispatcherDelegate::CreateRaw(this, &FMQRCHandler::GetExposureMethod),
		TEXT("Get exposure method")
	);

	CommandDispatcher->BindCommand(
		TEXT("vset /mqrc/exposure_method [str]"),
		FDispatcherDelegate::CreateRaw(this, &FMQRCHandler::SetExposureMethod),
		TEXT("Set exposure method: histogram, basic, manual")
	);

	CommandDispatcher->BindCommand(
		TEXT("vget /mqrc/exposure_bias"),
		FDispatcherDelegate::CreateRaw(this, &FMQRCHandler::GetExposureBias),
		TEXT("Get exposure bias")
	);

	CommandDispatcher->BindCommand(
		TEXT("vset /mqrc/exposure_bias [float]"),
		FDispatcherDelegate::CreateRaw(this, &FMQRCHandler::SetExposureBias),
		TEXT("Set exposure bias value")
	);

	CommandDispatcher->BindCommand(
		TEXT("vget /mqrc/motion_blur"),
		FDispatcherDelegate::CreateRaw(this, &FMQRCHandler::GetMotionBlur),
		TEXT("Get motion blur amount")
	);

	CommandDispatcher->BindCommand(
		TEXT("vset /mqrc/motion_blur [float]"),
		FDispatcherDelegate::CreateRaw(this, &FMQRCHandler::SetMotionBlur),
		TEXT("Set motion blur amount")
	);

	CommandDispatcher->BindCommand(
		TEXT("vget /mqrc/lumen_quality"),
		FDispatcherDelegate::CreateRaw(this, &FMQRCHandler::GetLumenQuality),
		TEXT("Get Lumen quality settings (scene_quality gather_quality)")
	);

	CommandDispatcher->BindCommand(
		TEXT("vset /mqrc/lumen_quality [float] [float]"),
		FDispatcherDelegate::CreateRaw(this, &FMQRCHandler::SetLumenQuality),
		TEXT("Set Lumen quality settings: scene_quality, gather_quality")
	);

	CommandDispatcher->BindCommand(
		TEXT("vget /mqrc/saturation"),
		FDispatcherDelegate::CreateRaw(this, &FMQRCHandler::GetSaturation),
		TEXT("Get color saturation")
	);

	CommandDispatcher->BindCommand(
		TEXT("vset /mqrc/saturation [float]"),
		FDispatcherDelegate::CreateRaw(this, &FMQRCHandler::SetSaturation),
		TEXT("Set color saturation value")
	);

	CommandDispatcher->BindCommand(
		TEXT("vget /mqrc/contrast"),
		FDispatcherDelegate::CreateRaw(this, &FMQRCHandler::GetContrast),
		TEXT("Get color contrast")
	);

	CommandDispatcher->BindCommand(
		TEXT("vset /mqrc/contrast [float]"),
		FDispatcherDelegate::CreateRaw(this, &FMQRCHandler::SetContrast),
		TEXT("Set color contrast value")
	);

	CommandDispatcher->BindCommand(
		TEXT("vget /mqrc/gamma"),
		FDispatcherDelegate::CreateRaw(this, &FMQRCHandler::GetGamma),
		TEXT("Get color gamma")
	);

	CommandDispatcher->BindCommand(
		TEXT("vset /mqrc/gamma [float]"),
		FDispatcherDelegate::CreateRaw(this, &FMQRCHandler::SetGamma),
		TEXT("Set color gamma value")
	);

	CommandDispatcher->BindCommand(
		TEXT("vget /mqrc/gain"),
		FDispatcherDelegate::CreateRaw(this, &FMQRCHandler::GetGain),
		TEXT("Get color gain")
	);

	CommandDispatcher->BindCommand(
		TEXT("vset /mqrc/gain [float]"),
		FDispatcherDelegate::CreateRaw(this, &FMQRCHandler::SetGain),
		TEXT("Set color gain value")
	);

	CommandDispatcher->BindCommand(
		TEXT("vget /mqrc/auto_exposure_min_brightness"),
		FDispatcherDelegate::CreateRaw(this, &FMQRCHandler::GetAutoExposureMinBrightness),
		TEXT("Get auto-exposure minimum brightness")
	);

	CommandDispatcher->BindCommand(
		TEXT("vset /mqrc/auto_exposure_min_brightness [float]"),
		FDispatcherDelegate::CreateRaw(this, &FMQRCHandler::SetAutoExposureMinBrightness),
		TEXT("Set auto-exposure minimum brightness")
	);

	CommandDispatcher->BindCommand(
		TEXT("vget /mqrc/auto_exposure_max_brightness"),
		FDispatcherDelegate::CreateRaw(this, &FMQRCHandler::GetAutoExposureMaxBrightness),
		TEXT("Get auto-exposure maximum brightness")
	);

	CommandDispatcher->BindCommand(
		TEXT("vset /mqrc/auto_exposure_max_brightness [float]"),
		FDispatcherDelegate::CreateRaw(this, &FMQRCHandler::SetAutoExposureMaxBrightness),
		TEXT("Set auto-exposure maximum brightness")
	);

	CommandDispatcher->BindCommand(
		TEXT("vget /mqrc/depth_of_field_scale"),
		FDispatcherDelegate::CreateRaw(this, &FMQRCHandler::GetDepthOfFieldScale),
		TEXT("Get depth of field scale")
	);

	CommandDispatcher->BindCommand(
		TEXT("vset /mqrc/depth_of_field_scale [float]"),
		FDispatcherDelegate::CreateRaw(this, &FMQRCHandler::SetDepthOfFieldScale),
		TEXT("Set depth of field scale")
	);
}

FExecStatus FMQRCHandler::GetAntiAliasingMethod(const TArray<FString>& Args)
{
	const TEnumAsByte<EAntiAliasingMethod> Method = UMovieQualityRenderComponent::GlobalSettings.AntiAliasingMethod;

	FString MethodName;
	switch (Method.GetValue())
	{
		case AAM_FXAA:
			MethodName = TEXT("fxaa");
			break;
		case AAM_TemporalAA:
			MethodName = TEXT("temporal_aa");
			break;
		case AAM_TSR:
			MethodName = TEXT("tsr");
			break;
		case AAM_None:
			MethodName = TEXT("none");
			break;
		default:
			MethodName = TEXT("unknown");
			break;
	}

	return FExecStatus::OK(MethodName);
}

FExecStatus FMQRCHandler::SetAntiAliasingMethod(const TArray<FString>& Args)
{
	if (Args.Num() != 1)
	{
		return FExecStatus::GetInvalidArgument();
	}

	FString MethodStr = Args[0].ToLower();

	if (MethodStr == TEXT("fxaa"))
	{
		UMovieQualityRenderComponent::GlobalSettings.AntiAliasingMethod = AAM_FXAA;
		return FExecStatus::OK();
	}
	else if (MethodStr == TEXT("temporal_aa"))
	{
		UMovieQualityRenderComponent::GlobalSettings.AntiAliasingMethod = AAM_TemporalAA;
		return FExecStatus::OK();
	}
	else if (MethodStr == TEXT("tsr"))
	{
		UMovieQualityRenderComponent::GlobalSettings.AntiAliasingMethod = AAM_TSR;
		return FExecStatus::OK();
	}
	else if (MethodStr == TEXT("none"))
	{
		UMovieQualityRenderComponent::GlobalSettings.AntiAliasingMethod = AAM_None;
		return FExecStatus::OK();
	}
	else
	{
		FString ErrorMsg = FString::Printf(TEXT("Can not support anti-aliasing method %s, available options are fxaa, temporal_aa, tsr, none"), *MethodStr);
		return FExecStatus::Error(ErrorMsg);
	}
}

FExecStatus FMQRCHandler::GetExposureMethod(const TArray<FString>& Args)
{
	const EAutoExposureMethod Method = UMovieQualityRenderComponent::GlobalSettings.ExposureMethod;

	FString MethodName;
	switch (Method)
	{
		case EAutoExposureMethod::AEM_Histogram:
			MethodName = TEXT("histogram");
			break;
		case EAutoExposureMethod::AEM_Basic:
			MethodName = TEXT("basic");
			break;
		case EAutoExposureMethod::AEM_Manual:
			MethodName = TEXT("manual");
			break;
		default:
			MethodName = TEXT("unknown");
			break;
	}

	return FExecStatus::OK(MethodName);
}

FExecStatus FMQRCHandler::SetExposureMethod(const TArray<FString>& Args)
{
	if (Args.Num() != 1)
	{
		return FExecStatus::GetInvalidArgument();
	}

	FString MethodStr = Args[0].ToLower();

	if (MethodStr == TEXT("histogram"))
	{
		UMovieQualityRenderComponent::GlobalSettings.ExposureMethod = EAutoExposureMethod::AEM_Histogram;
		return FExecStatus::OK();
	}
	else if (MethodStr == TEXT("basic"))
	{
		UMovieQualityRenderComponent::GlobalSettings.ExposureMethod = EAutoExposureMethod::AEM_Basic;
		return FExecStatus::OK();
	}
	else if (MethodStr == TEXT("manual"))
	{
		UMovieQualityRenderComponent::GlobalSettings.ExposureMethod = EAutoExposureMethod::AEM_Manual;
		return FExecStatus::OK();
	}
	else
	{
		FString ErrorMsg = FString::Printf(TEXT("Can not support exposure method %s, available options are histogram, basic, manual"), *MethodStr);
		return FExecStatus::Error(ErrorMsg);
	}
}

FExecStatus FMQRCHandler::GetExposureBias(const TArray<FString>& Args)
{
	float BiasValue = UMovieQualityRenderComponent::GlobalSettings.ExposureBias;
	return FExecStatus::OK(FString::Printf(TEXT("%f"), BiasValue));
}

FExecStatus FMQRCHandler::SetExposureBias(const TArray<FString>& Args)
{
	if (Args.Num() != 1)
	{
		return FExecStatus::GetInvalidArgument();
	}

	float BiasValue = FCString::Atof(*Args[0]);
	UMovieQualityRenderComponent::GlobalSettings.ExposureBias = BiasValue;
	return FExecStatus::OK();
}

FExecStatus FMQRCHandler::GetMotionBlur(const TArray<FString>& Args)
{
	float BlurAmount = UMovieQualityRenderComponent::GlobalSettings.MotionBlurAmount;
	return FExecStatus::OK(FString::Printf(TEXT("%f"), BlurAmount));
}

FExecStatus FMQRCHandler::SetMotionBlur(const TArray<FString>& Args)
{
	if (Args.Num() != 1)
	{
		return FExecStatus::GetInvalidArgument();
	}

	float BlurAmount = FCString::Atof(*Args[0]);
	UMovieQualityRenderComponent::GlobalSettings.MotionBlurAmount = BlurAmount;
	return FExecStatus::OK();
}

FExecStatus FMQRCHandler::GetLumenQuality(const TArray<FString>& Args)
{
	FStrFormatter Formatter;
	Formatter << UMovieQualityRenderComponent::GlobalSettings.LumenSceneLightingQuality;
	Formatter << UMovieQualityRenderComponent::GlobalSettings.LumenFinalGatherQuality;
	return FExecStatus::OK(Formatter.ToString());
}

FExecStatus FMQRCHandler::SetLumenQuality(const TArray<FString>& Args)
{
	if (Args.Num() != 2)
	{
		return FExecStatus::GetInvalidArgument();
	}

	float SceneQuality = FCString::Atof(*Args[0]);
	float GatherQuality = FCString::Atof(*Args[1]);

	UMovieQualityRenderComponent::GlobalSettings.LumenSceneLightingQuality = SceneQuality;
	UMovieQualityRenderComponent::GlobalSettings.LumenFinalGatherQuality = GatherQuality;
	return FExecStatus::OK();
}

FExecStatus FMQRCHandler::GetSaturation(const TArray<FString>& Args)
{
	float Value = UMovieQualityRenderComponent::GlobalSettings.Saturation;
	return FExecStatus::OK(FString::Printf(TEXT("%f"), Value));
}

FExecStatus FMQRCHandler::SetSaturation(const TArray<FString>& Args)
{
	if (Args.Num() != 1)
	{
		return FExecStatus::GetInvalidArgument();
	}

	float Value = FCString::Atof(*Args[0]);
	UMovieQualityRenderComponent::GlobalSettings.Saturation = Value;
	return FExecStatus::OK();
}

FExecStatus FMQRCHandler::GetContrast(const TArray<FString>& Args)
{
	float Value = UMovieQualityRenderComponent::GlobalSettings.Contrast;
	return FExecStatus::OK(FString::Printf(TEXT("%f"), Value));
}

FExecStatus FMQRCHandler::SetContrast(const TArray<FString>& Args)
{
	if (Args.Num() != 1)
	{
		return FExecStatus::GetInvalidArgument();
	}

	float Value = FCString::Atof(*Args[0]);
	UMovieQualityRenderComponent::GlobalSettings.Contrast = Value;
	return FExecStatus::OK();
}

FExecStatus FMQRCHandler::GetGamma(const TArray<FString>& Args)
{
	float Value = UMovieQualityRenderComponent::GlobalSettings.Gamma;
	return FExecStatus::OK(FString::Printf(TEXT("%f"), Value));
}

FExecStatus FMQRCHandler::SetGamma(const TArray<FString>& Args)
{
	if (Args.Num() != 1)
	{
		return FExecStatus::GetInvalidArgument();
	}

	float Value = FCString::Atof(*Args[0]);
	UMovieQualityRenderComponent::GlobalSettings.Gamma = Value;
	return FExecStatus::OK();
}

FExecStatus FMQRCHandler::GetGain(const TArray<FString>& Args)
{
	float Value = UMovieQualityRenderComponent::GlobalSettings.Gain;
	return FExecStatus::OK(FString::Printf(TEXT("%f"), Value));
}

FExecStatus FMQRCHandler::SetGain(const TArray<FString>& Args)
{
	if (Args.Num() != 1)
	{
		return FExecStatus::GetInvalidArgument();
	}

	float Value = FCString::Atof(*Args[0]);
	UMovieQualityRenderComponent::GlobalSettings.Gain = Value;
	return FExecStatus::OK();
}

FExecStatus FMQRCHandler::GetAutoExposureMinBrightness(const TArray<FString>& Args)
{
	float Value = UMovieQualityRenderComponent::GlobalSettings.AutoExposureMinBrightness;
	return FExecStatus::OK(FString::Printf(TEXT("%f"), Value));
}

FExecStatus FMQRCHandler::SetAutoExposureMinBrightness(const TArray<FString>& Args)
{
	if (Args.Num() != 1)
	{
		return FExecStatus::GetInvalidArgument();
	}

	float Value = FCString::Atof(*Args[0]);
	UMovieQualityRenderComponent::GlobalSettings.AutoExposureMinBrightness = Value;
	return FExecStatus::OK();
}

FExecStatus FMQRCHandler::GetAutoExposureMaxBrightness(const TArray<FString>& Args)
{
	float Value = UMovieQualityRenderComponent::GlobalSettings.AutoExposureMaxBrightness;
	return FExecStatus::OK(FString::Printf(TEXT("%f"), Value));
}

FExecStatus FMQRCHandler::SetAutoExposureMaxBrightness(const TArray<FString>& Args)
{
	if (Args.Num() != 1)
	{
		return FExecStatus::GetInvalidArgument();
	}

	float Value = FCString::Atof(*Args[0]);
	UMovieQualityRenderComponent::GlobalSettings.AutoExposureMaxBrightness = Value;
	return FExecStatus::OK();
}

FExecStatus FMQRCHandler::GetDepthOfFieldScale(const TArray<FString>& Args)
{
	float Value = UMovieQualityRenderComponent::GlobalSettings.DepthOfFieldScale;
	return FExecStatus::OK(FString::Printf(TEXT("%f"), Value));
}

FExecStatus FMQRCHandler::SetDepthOfFieldScale(const TArray<FString>& Args)
{
	if (Args.Num() != 1)
	{
		return FExecStatus::GetInvalidArgument();
	}

	float Value = FCString::Atof(*Args[0]);
	UMovieQualityRenderComponent::GlobalSettings.DepthOfFieldScale = Value;
	return FExecStatus::OK();
}
