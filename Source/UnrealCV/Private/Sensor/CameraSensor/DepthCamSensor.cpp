// Weichao Qiu @ 2017
#include "DepthCamSensor.h"
#include "AnnotationCamSensor.h"
#include "TextureResource.h"
#include "Runtime/Core/Public/Async/ParallelFor.h"
#include "Serialization.h"
#include "ImageUtil.h"
#include "UnrealcvLog.h"

UDepthCamSensor::UDepthCamSensor(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	// this->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	this->CaptureSource = ESceneCaptureSource::SCS_SceneDepth;
	bIgnoreTransparentObjects = false;
}

void UDepthCamSensor::InitTextureTarget(int filmWidth, int filmHeight)
{
	EPixelFormat PixelFormat = EPixelFormat::PF_FloatRGBA;
	bool bUseLinearGamma = true;
	TextureTarget->InitCustomFormat(filmWidth, filmHeight, EPixelFormat::PF_FloatRGBA, bUseLinearGamma);
}

void UDepthCamSensor::CaptureDepth(TArray<float>& DepthData, int& Width, int& Height)
{
	if (!bIgnoreTransparentObjects)
	{
		TArray<TWeakObjectPtr<UPrimitiveComponent> > ComponentList;
		UAnnotationCamSensor::GetAnnotationComponents(this->GetWorld(), ComponentList);
		this->ShowOnlyComponents = ComponentList;
		this->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		this->ShowFlags.SetMaterials(false); // This will make annotation component visible
	}

	if (!CheckTextureTarget()) return;

	// if (bUseAsyncCapture)
	// {
	// 	TArray<FFloat16Color> FloatColorDepthData;
	// 	int TempWidth, TempHeight;
	// 	CaptureFloat16(FloatColorDepthData, TempWidth, TempHeight);

	// 	Width = TempWidth;
	// 	Height = TempHeight;
	// 	DepthData.SetNum(Width * Height);

	// 	ParallelFor(FloatColorDepthData.Num(), [&](int32 i)
	// 	{
	// 		if (i >= 0 && i < FloatColorDepthData.Num() && i < DepthData.Num())
	// 		{
	// 			FFloat16Color& FloatColor = FloatColorDepthData[i];
	// 			DepthData[i] = FloatColor.R;
	// 		}
	// 	});
	// }
	// else
	// {
		this->CaptureScene();
		Width = this->TextureTarget->SizeX;
		Height = TextureTarget->SizeY;
		DepthData.AddZeroed(Width * Height);
		FTextureRenderTargetResource* RenderTargetResource = this->TextureTarget->GameThread_GetRenderTargetResource();
		TArray<FFloat16Color> FloatColorDepthData;
		RenderTargetResource->ReadFloat16Pixels(FloatColorDepthData);

		ParallelFor(FloatColorDepthData.Num(), [&](int32 i)
		{
			if (i >= 0 && i < FloatColorDepthData.Num() && i < DepthData.Num())
			{
				FFloat16Color& FloatColor = FloatColorDepthData[i];
				DepthData[i] = FloatColor.R;
			}
		});
	// }
}

void UDepthCamSensor::CaptureDepthToFile(const FString& Filename)
{
	if (!CheckTextureTarget())
	{
		UE_LOG(LogUnrealCV, Error, TEXT("TextureTarget not initialized, CaptureDepthToFile failed."));
		return;
	}

	if (!bIgnoreTransparentObjects)
	{
		TArray<TWeakObjectPtr<UPrimitiveComponent>> ComponentList;
		UAnnotationCamSensor::GetAnnotationComponents(this->GetWorld(), ComponentList);
		this->ShowOnlyComponents = ComponentList;
		this->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		this->ShowFlags.SetMaterials(false);
	}

	this->CaptureScene();

	FTextureRenderTargetResource* RenderTargetResource = TextureTarget->GameThread_GetRenderTargetResource();
	int32 Width = TextureTarget->SizeX;
	int32 Height = TextureTarget->SizeY;

	FString OutputPath = Filename;
	FString DepthPreviewPath = OutputPath.Replace(TEXT(".npy"), TEXT("_preview.png"));

	ENQUEUE_RENDER_COMMAND(CaptureDepthToFileCommand)(
		[RenderTargetResource, Width, Height, OutputPath, DepthPreviewPath](FRHICommandListImmediate& RHICmdList)
		{
			TArray<FFloat16Color> FloatColorData;
			RenderTargetResource->ReadFloat16Pixels(FloatColorData);

			TArray<float> DepthData;
			DepthData.SetNum(Width * Height);

			ParallelFor(FloatColorData.Num(), [&](int32 i)
			{
				if (i >= 0 && i < FloatColorData.Num() && i < DepthData.Num())
				{
					DepthData[i] = FloatColorData[i].R;
				}
			});

			TArray<FColor> DepthPreview;
			ConvertDepthToPreview(DepthData, DepthPreview);

			AsyncTask(ENamedThreads::GameThread, [DepthData = MoveTemp(DepthData), DepthPreview = MoveTemp(DepthPreview), Width, Height, OutputPath, DepthPreviewPath]()
			{
				if (SerializeData(DepthData, Width, Height, OutputPath) == FExecStatusType::OK)
				{
					UE_LOG(LogUnrealCV, Log, TEXT("[CaptureDepthToFile] Saved depth to %s"), *OutputPath);
				}
				else
				{
					UE_LOG(LogUnrealCV, Error, TEXT("[CaptureDepthToFile] Failed to save depth %s"), *OutputPath);
				}

				if (SerializeData(DepthPreview, Width, Height, DepthPreviewPath) == FExecStatusType::OK)
				{
					UE_LOG(LogUnrealCV, Verbose, TEXT("[CaptureDepthToFile] Saved depth preview to %s"), *DepthPreviewPath);
				}
			});
		}
	);
}