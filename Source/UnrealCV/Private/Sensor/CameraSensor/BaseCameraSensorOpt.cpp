#include "BaseCameraSensorOpt.h"
#include "Sensor/RenderUtil/RenderUtil.h"
#include "Sensor/RenderUtil/ImageReadback.h"
#include "Sensor/RenderUtil/ImageConversion.h"
#include "Sensor/RenderUtil/ImageProcessing.h"
#include "Sensor/RenderUtil/ImageSerializer.h"
#include "TextureReader.h"
#include "UnrealcvServer.h"
#include "UnrealcvLog.h"
#include "BPFunctionLib/AnnotationBPLib.h"

using namespace UnrealCV::RenderUtil;

UBaseCameraSensorOpt::UBaseCameraSensorOpt(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	this->ShowFlags.SetPostProcessing(true);
	bCaptureEveryFrame = false;
	bCaptureOnMovement = false;
	PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;
	HiddenComponents.Reset();
	UAnnotationBPLib::GetAnnotationComponents(this->GetWorld(), HiddenComponents);
	CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	bUseRayTracingIfEnabled = true;
	bAlwaysPersistRenderingState = true;

	FServerConfig& Config = FUnrealcvServer::Get().Config;
	FilmWidth = Config.Width == 0 ? 640 : Config.Width;
	FilmHeight = Config.Height == 0 ? 480 : Config.Height;
	FOVAngle = Config.FOV == 0 ? 90 : Config.FOV;

	bUsePipelinedCapture = false;
}

void UBaseCameraSensorOpt::InitTextureTarget(int filmWidth, int filmHeight)
{
	InitUInt8TextureTarget(filmWidth, filmHeight, false);
}

void UBaseCameraSensorOpt::InitFloat16TextureTarget(int filmWidth, int filmHeight)
{
	bool bUseLinearGamma = true;
	EPixelFormat PixelFormat = EPixelFormat::PF_FloatRGBA;
	TextureTarget = NewObject<UTextureRenderTarget2D>(this);
	TextureTarget->InitCustomFormat(filmWidth, filmHeight, PixelFormat, bUseLinearGamma);
}

void UBaseCameraSensorOpt::InitUInt8TextureTarget(int filmWidth, int filmHeight, bool bUseLinearGamma)
{
	EPixelFormat PixelFormat = EPixelFormat::PF_B8G8R8A8;
	TextureTarget = NewObject<UTextureRenderTarget2D>(this);
	TextureTarget->InitCustomFormat(filmWidth, filmHeight, PixelFormat, bUseLinearGamma);
}

void UBaseCameraSensorOpt::SetFilmSize(int Width, int Height)
{
	this->FilmWidth = Width;
	this->FilmHeight = Height;
	if (!IsValid(TextureTarget))
	{
		TextureTarget = NewObject<UTextureRenderTarget2D>(this);
	}

	if (TextureTarget->SizeX != Width || TextureTarget->SizeY != Height)
	{
		InitTextureTarget(Width, Height);
	}
}

int UBaseCameraSensorOpt::GetFilmWidth()
{
	if (!IsValid(TextureTarget)) return 0;
	return TextureTarget->SizeX;
}

int UBaseCameraSensorOpt::GetFilmHeight()
{
	if (!IsValid(TextureTarget)) return 0;
	return TextureTarget->SizeY;
}

bool UBaseCameraSensorOpt::CheckTextureTarget()
{
	if (!IsValid(TextureTarget))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("The TextureTarget was not initialized."));
		return false;
	}
	if (TextureTarget->SizeX == 0 || TextureTarget->SizeY == 0)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("The TextureTarget has invalid size."));
		return false;
	}
	return true;
}

void UBaseCameraSensorOpt::SetPostProcessMaterial(TScriptInterface<IBlendableInterface> PostProcessMaterial)
{
	PostProcessSettings.WeightedBlendables.Array.Empty();
	PostProcessSettings.AddBlendable(PostProcessMaterial, 1);
	this->PostProcessBlendWeight = 1.0f;
}

void UBaseCameraSensorOpt::GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView)
{
	DesiredView.Location = GetComponentLocation();
	DesiredView.Rotation = GetComponentRotation();
	DesiredView.FOV = this->FOVAngle;
	DesiredView.ProjectionMode = ECameraProjectionMode::Perspective;
	DesiredView.OrthoWidth = OrthoWidth;

	DesiredView.PostProcessBlendWeight = PostProcessBlendWeight;
	if (PostProcessBlendWeight > 0.0f)
	{
		DesiredView.PostProcessSettings = PostProcessSettings;
	}
}

void UBaseCameraSensorOpt::ReadCaptureResults(TArray<FColor>& Data)
{
	FReadSurfaceDataFlags ReadSurfaceDataFlags;
	ReadSurfaceDataFlags.SetLinearToGamma(false);
	TextureTarget->GameThread_GetRenderTargetResource()->ReadPixels(Data, ReadSurfaceDataFlags);
	if (Data.Num() == 0)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("Captured lit data is empty."));
	}
}

void UBaseCameraSensorOpt::SetShowOnlyList(const TArray<TWeakObjectPtr<UPrimitiveComponent>>& InShowOnlyComponents)
{
	if (PrimitiveRenderMode != ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("SetShowOnlyList: PrimitiveRenderMode not PRM_UseShowOnlyList, but setting ShowOnlyList !!!"));
	}

	ShowOnlyComponents.Reset();
	ShowOnlyComponents = InShowOnlyComponents;
}

void UBaseCameraSensorOpt::HideActor(AActor* Actor)
{
	if (!IsValid(Actor))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("HideActor: Invalid actor"));
		return;
	}

	if ( PrimitiveRenderMode != ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives )
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("HideActor: PrimitiveRenderMode not PRM_RenderScenePrimitives, but setting ShowOnlyList"));
	}
	if (!HiddenActors.Contains(Actor))
	{
		HiddenActors.AddUnique(Actor);
	}
}

void UBaseCameraSensorOpt::ShowActor(AActor* Actor)
{
	if (!IsValid(Actor))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("ShowActor: Invalid actor"));
		return;
	}

	if ( PrimitiveRenderMode != ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives )
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("ShowActor: PrimitiveRenderMode not PRM_RenderScenePrimitives, but setting ShowOnlyList"));
	}
	if (HiddenActors.Contains(Actor))
	{
		HiddenActors.Remove(Actor);
	}
}

void UBaseCameraSensorOpt::LaunchPipelinedCapture()
{
	if (!CheckTextureTarget())
	{
		UE_LOG(LogUnrealCV, Error, TEXT("TextureTarget not initialized"));
		return;
	}

	this->CaptureScene();
	ReadImageDataBeginPipelined(PipelinedContext, *TextureTarget);
}

bool UBaseCameraSensorOpt::ConsumePipelinedCapture(TArray<FColor>& OutPixels)
{
	if (!PipelinedContext.bValid)
		return false;

	bool bSuccess = ReadImageDataEndPipelined(PipelinedContext, OutPixels);

	if (bSuccess && PipelinedContext.Format == EPixelFormat::PF_B8G8R8A8)
	{
		FixAlphaIfNeeded(OutPixels, PipelinedContext.Format);
	}

	PipelinedContext.bValid = false;
	return bSuccess;
}

void UBaseCameraSensorOpt::CaptureFastToFile(const FString& Filename)
{
	if (!CheckTextureTarget())
	{
		UE_LOG(LogUnrealCV, Error, TEXT("TextureTarget not initialized"));
		return;
	}

	if (bUsePipelinedCapture)
	{
		TArray<FColor> Pixels;
		if (ConsumePipelinedCapture(Pixels))
		{
			int32 Width = PipelinedContext.Size.X;
			int32 Height = PipelinedContext.Size.Y;

			AsyncTask(ENamedThreads::AnyThread, [
				Pixels = MoveTemp(Pixels),
				Width,
				Height,
				Filename
			]()
			{
				SaveImageData(Pixels, Width, Height, Filename);
			});
		}

		LaunchPipelinedCapture();
	}
	else
	{
		this->CaptureScene();
		SaveImageDataAsync(*TextureTarget, Filename);
	}
}

void UBaseCameraSensorOpt::CaptureFast(TArray<FColor>& ImageData, int& Width, int& Height)
{
	if (!CheckTextureTarget())
	{
		UE_LOG(LogUnrealCV, Error, TEXT("TextureTarget not initialized"));
		return;
	}

	if (bUsePipelinedCapture)
	{
		if (ConsumePipelinedCapture(ImageData))
		{
			Width = PipelinedContext.Size.X;
			Height = PipelinedContext.Size.Y;
		}
		LaunchPipelinedCapture();
	}
	else
	{
		this->CaptureScene();
		ReadImageDataSync(*TextureTarget, ImageData, Width, Height);
	}
}

void UBaseCameraSensorOpt::CaptureFast(TArray<FFloat16Color>& ImageData, int& Width, int& Height)
{
	if (!CheckTextureTarget())
	{
		UE_LOG(LogUnrealCV, Error, TEXT("TextureTarget not initialized"));
		return;
	}

	this->CaptureScene();
	ReadImageDataSync(*TextureTarget, ImageData, Width, Height);
}

void UBaseCameraSensorOpt::Capture(TArray<FColor>& ImageData, int& Width, int& Height)
{
	if (!CheckTextureTarget())
	{
		UE_LOG(LogUnrealCV, Error, TEXT("The TextureTarget was not initialized. Capture failed."));
		return;
	}

	this->CaptureScene();
	ReadTextureRenderTarget(TextureTarget, ImageData, Width, Height);
}

void UBaseCameraSensorOpt::Capture(TArray<FFloat16Color>& ImageData, int& Width, int& Height)
{
	if (!CheckTextureTarget())
	{
		UE_LOG(LogUnrealCV, Error, TEXT("The TextureTarget was not initialized. Capture failed."));
		return;
	}

	this->CaptureScene();

	Width = TextureTarget->SizeX;
	Height = TextureTarget->SizeY;
	ImageData.Empty();
	ImageData.SetNumUninitialized(Width * Height);
	FTextureRenderTargetResource* RenderTargetResource = this->TextureTarget->GameThread_GetRenderTargetResource();
	RenderTargetResource->ReadFloat16Pixels(ImageData);
}
