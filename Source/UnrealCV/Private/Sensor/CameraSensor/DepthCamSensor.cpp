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

	ENQUEUE_RENDER_COMMAND(CaptureDepthToFileCommand)(
		[RenderTargetResource, Width, Height, OutputPath](FRHICommandListImmediate& RHICmdList)
		{
			// TArray<FFloat16Color> FloatColorData;
			TArray<FColor> FloatColorData;
			// RenderTargetResource->ReadFloat16Pixels(FloatColorData);
            FReadSurfaceDataFlags ReadFlags(RCM_UNorm, CubeFace_MAX);
            RHICmdList.ReadSurfaceData(
                RenderTargetResource->GetRenderTargetTexture(),
                FIntRect(0, 0, Width, Height),
                FloatColorData,
                ReadFlags
            );

			TArray<float> DepthData;
			DepthData.SetNum(Width * Height);

			// ParallelFor(FloatColorData.Num(), [&](int32 i)
			// {
			// 	if (i >= 0 && i < FloatColorData.Num() && i < DepthData.Num())
			// 	{
			// 		DepthData[i] = FloatColorData[i].R;
			// 	}
			// });
			const int32 CACHE_LINE_SIZE = 64;
			const int32 PIXELS_PER_CACHE_LINE = CACHE_LINE_SIZE / sizeof(FColor);
			int32 NumCores = FPlatformMisc::NumberOfCores();
			int32 TotalPixels = FloatColorData.Num();

			int32 BlockSize = FMath::Max(PIXELS_PER_CACHE_LINE, (TotalPixels + NumCores - 1) / NumCores);
			BlockSize = (BlockSize + PIXELS_PER_CACHE_LINE - 1) / PIXELS_PER_CACHE_LINE * PIXELS_PER_CACHE_LINE;

			int32 NumBlocks = (TotalPixels + BlockSize - 1) / BlockSize;

			ParallelFor(NumBlocks, [&](int32 BlockIndex)
			{
				int32 StartIndex = BlockIndex * BlockSize;
				int32 EndIndex = FMath::Min(StartIndex + BlockSize, TotalPixels);
				
				for (int32 i = StartIndex; i < EndIndex && i < DepthData.Num(); i++)
				{
					DepthData[i] = FloatColorData[i].R;
				}
			});



			AsyncTask(ENamedThreads::AnyThread, [DepthData = MoveTemp(DepthData), Width, Height, OutputPath]()
			{
				FString OutputPathPNG = OutputPath.Replace(TEXT(".npy"), TEXT(".png"));
				FString OutputPathPNG10KM = OutputPathPNG.Replace(TEXT(".png"), TEXT("_10km.png"));
				FString DepthPreviewPath = OutputPathPNG.Replace(TEXT(".png"), TEXT("_preview.png"));
				TArray<FColor> DepthPreview;
				TArray<FColor> DepthPNG;
				TArray<FColor> DepthPNG10KM;
				ConvertDepthToPNG_RGB24(DepthData, DepthPNG10KM, 0.0f, 1000000.0f);  // 10km
				// ConvertDepthToPNG_RGB24(DepthData, DepthPNG, 0.0f, 5000.0f); // 50m
				ConvertDepthToPNG_RGB24(DepthData, DepthPNG, 0.0f, 100000.0f); // 1km
				ConvertDepthToPreview(DepthData, DepthPreview);
				// | 你能接受的误差（cm）   | 对应的 MaxDepth（cm）                     |
				// | ------------- | ------------------------------------ |
				// | 100 cm（1 m）   | **3,355,443,000 cm**  ≈ 33,554 km    |
				// | 50 cm（0.5 m）  | **1,677,721,500 cm**  ≈ 16,777 km    |
				// | 10 cm（0.1 m）  | **335,544,300 cm**   ≈ 3,355 km      |
				// | 1000 cm（10 m） | **33,554,430,000 cm** ≈ 335,544 km   |
				// | 2000 cm（20 m） | **67,108,860,000 cm** ≈ 671,088 km   |
				// | 4000 cm（40 m） | **134,217,720,000 cm**≈ 1,342,177 km |
				SerializeData(DepthPNG, Width, Height, OutputPathPNG);
				SerializeData(DepthPNG10KM, Width, Height, OutputPathPNG10KM);
				SerializeData(DepthPreview, Width, Height, DepthPreviewPath);
			});
		}
	);
}