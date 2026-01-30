// Weichao Qiu @ 2017
#include "LitCamSensor.h"
#include "UnrealcvLog.h"
#include "UnrealcvStats.h"
#include "Server/ServerConfig.h"
#include "Server/UnrealcvServer.h"

#include "Runtime/Engine/Classes/Engine/Engine.h"
#include "TextureResource.h"
#include "SL.h"

DECLARE_CYCLE_STAT(TEXT("ULitCamSensor::CaptureLit"), STAT_CaptureLit, STATGROUP_UnrealCV);

ULitCamSensor::ULitCamSensor(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	CaptureSource = ESceneCaptureSource::SCS_FinalColorHDR;
	// CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
}

// void ULitCamSensor::SetupRenderTarget()
// {
// 	bool bUseLinearGamma = false;
// 	TextureTarget->InitCustomFormat(FilmWidth, FilmHeight, EPixelFormat::PF_B8G8R8A8, bUseLinearGamma);
// }

// bool bUseLinearGamma = false;
// bool bUseLinearGamma = true;  // true by default
// bUseLinearGamma requires CaptureEveryFrame!
// TextureTarget->InitAutoFormat(Width, Height);
// TextureTarget->TargetGamma = GEngine->GetDisplayGamma();
// TextureTarget->TargetGamma = 1;

void ULitCamSensor::InitTextureTarget(int filmWidth, int filmHeight)
{
	FServerConfig& Config = FUnrealcvServer::Get().Config;
	bool bUseBGRA8 = Config.bLitUseBGRA8;
	if (bUseBGRA8)
	{
		bool bUseLinearGamma = CaptureSource == ESceneCaptureSource::SCS_FinalColorLDR;
		InitUInt8TextureTarget(filmWidth, filmHeight, bUseLinearGamma);
	}
	else
	{
		InitFloat16TextureTarget(filmWidth, filmHeight);
	}
	// TextureTarget->bNoFastClear = true;
    // TextureTarget->ClearColor = FLinearColor::Black;
}

void ULitCamSensor::CaptureLit(TArray<FColor>& Image, int& Width, int& Height)
{
	SCOPE_CYCLE_COUNTER(STAT_CaptureLit);
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

void ULitCamSensor::CaptureLitToFile(FString Filename)
{
	SCOPE_CYCLE_COUNTER(STAT_CaptureLit);
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



// void ULitCamSensor::CaptureToGPUQueue(const FString& Filename)
// {
// 	if (!CheckTextureTarget())
// 	{
// 		InitTextureTarget(this->FilmWidth, this->FilmHeight);
// 		if (!CheckTextureTarget())
// 		{
// 			UE_LOG(LogUnrealCV, Error, TEXT("Failed to initialize TextureTarget for CaptureToGPUQueue."));
// 			return;
// 		}
// 	}

// 	Super::CaptureToGPUQueue(Filename);
// }