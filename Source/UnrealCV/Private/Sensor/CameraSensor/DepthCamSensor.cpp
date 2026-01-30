// Weichao Qiu @ 2017
#include "DepthCamSensor.h"
#include "AnnotationCamSensor.h"
#include "TextureResource.h"
#include "Runtime/Core/Public/Async/ParallelFor.h"
#include "Serialization.h"
#include "ImageUtil.h"
#include "UnrealcvLog.h"
#include "RHISurfaceDataConversionOpt.h"
#include "BPFunctionLib/AnnotationBPLib.h"

UDepthCamSensor::UDepthCamSensor(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	this->CaptureSource = ESceneCaptureSource::SCS_SceneDepth;
	bIgnoreTransparentObjects = true;
	bRenderInMainRenderer = true;  // optimization
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
		UAnnotationBPLib::GetAnnotationComponents(this->GetWorld(), ComponentList);
		this->ShowOnlyComponents = ComponentList;
		this->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		this->ShowFlags.SetMaterials(false);
	}

	if (!CheckTextureTarget()) return;


	TArray<FFloat16Color> FloatColorDepthData;
	this->Capture(FloatColorDepthData, Width, Height);
	if (FloatColorDepthData.Num() == 0)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("FloatColorDepthData is empty, CaptureDepth failed."));
		return;
	}
	DepthData.Empty();
	DepthData.AddUninitialized(FloatColorDepthData.Num());
	check(DepthData.Num() == FloatColorDepthData.Num());

	ParallelFor(FloatColorDepthData.Num(), [&](int32 i)
	{
		if (i >= 0 && i < FloatColorDepthData.Num() && i < DepthData.Num())
		{
			FFloat16Color& FloatColor = FloatColorDepthData[i];
			DepthData[i] = FloatColor.R;
		}
	});
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
		UAnnotationBPLib::GetAnnotationComponents(this->GetWorld(), ComponentList);
		this->ShowOnlyComponents = ComponentList;
		this->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		this->ShowFlags.SetMaterials(false);
	}


	/*
	 ****************      Below is copied from BaseCameraSensor.cpp     ********************
	*/

	CheckCaptureCache(ECaptureFormat::Invalid);

	if (!bCaptureLaunched)
	{
		LaunchCapture();
	}
	bCaptureLaunched = false;

	EPixelFormat PixelFormat = TextureTarget->GetFormat();
	UE_LOG(LogTemp, Warning, TEXT("[DEBUG] TextureTarget Format: %d, SRGB: %d, Gamma: %f"),
		(int32)PixelFormat,
		TextureTarget->SRGB,
		TextureTarget->TargetGamma);

	FTextureRenderTargetResource* RenderTargetResource = TextureTarget->GameThread_GetRenderTargetResource();
	int32 Width = TextureTarget->SizeX;
	int32 Height = TextureTarget->SizeY;

	FQueuedCapture Capture;
	Capture.Readback = new FRHIGPUTextureReadback(
		*FString::Printf(TEXT("Capture_%d"), FMath::Rand())
	);
	Capture.OutputPath = TEXT("");
	Capture.Width = Width;
	Capture.Height = Height;
	Capture.PixelFormat = PixelFormat;

	ENQUEUE_RENDER_COMMAND(EnqueueGPUCopy)(
		[RenderTargetResource, Capture = MoveTemp(Capture), Filename](FRHICommandListImmediate& RHICmdList)
		{
			RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
			Capture.Readback->EnqueueCopy(RHICmdList, RenderTargetResource->GetRenderTargetTexture());

			int32 RowPitchInPixels;
			const void* RawData = Capture.Readback->Lock(RowPitchInPixels);
			void* RawDataCopy = FMemory::Malloc(  RowPitchInPixels * Capture.Height * GPixelFormats[Capture.PixelFormat].BlockBytes);
			FMemory::Memcpy(RawDataCopy, RawData, RowPitchInPixels * Capture.Height * GPixelFormats[Capture.PixelFormat].BlockBytes);
			Capture.Readback->Unlock();
			delete Capture.Readback;

			AsyncTask(ENamedThreads::AnyThread,
				[RawDataCopy, OutputPath = Filename, Width = Capture.Width, Height = Capture.Height, PixelFormat = Capture.PixelFormat, RowPitchInPixels = RowPitchInPixels]()
				{

					TArray<FFloat16Color> PixelData;
					PixelData.AddUninitialized(Width * Height);
					FReadSurfaceDataFlags ReadFlags(RCM_MinMax);
					ReadFlags.SetLinearToGamma(false);

					uint32 SrcPitch = RowPitchInPixels * GPixelFormats[PixelFormat].BlockBytes;
					ConvertRAWSurfaceDataToFFloat16ColorOpt(
						PixelFormat,
						Width,
						Height,
						(uint8*)RawDataCopy,
						SrcPitch,
						PixelData.GetData(),
						ReadFlags
					);
					FMemory::Free(RawDataCopy);

					TArray<float> DepthData;
					DepthData.AddZeroed(Width * Height);

					ParallelFor(PixelData.Num(), [&](int32 i)
					{
						if (i >= 0 && i < PixelData.Num() && i < DepthData.Num())
						{
							FFloat16Color& FloatColor = PixelData[i];
							DepthData[i] = FloatColor.R;
						}
					});

					double SerializeStartTime = FPlatformTime::Seconds();
					FString OutputPathBase = OutputPath.Replace(TEXT(".npy"), TEXT(""));
					FString OutputPathPNG1KM = OutputPathBase + TEXT("1km.png");
					FString OutputPathPNG20KM = OutputPathBase + TEXT("20km.png");
					FString DepthPreviewPath = OutputPathBase + TEXT("_preview.png");
					FString DepthNpyPath = OutputPathBase + TEXT(".npy");
					TArray<FColor> DepthPreview;
					TArray<FColor> DepthPNG1KM;
					TArray<FColor> DepthPNG20KM;
					ConvertDepthToPNG_RGB24(DepthData, DepthPNG1KM, 0.0f, 100000.0f);
					ConvertDepthToPNG_RGB24(DepthData, DepthPNG20KM, 0.0f, 2000000.0f);
					ConvertDepthToPreview(DepthData, DepthPreview);
					// | 你能接受的误差（cm）   | 对应的 MaxDepth（cm）                     |
					// | ------------- | ------------------------------------ |
					// | 100 cm（1 m）   | **3,355,443,000 cm**  ≈ 33,554 km    |
					// | 50 cm（0.5 m）  | **1,677,721,500 cm**  ≈ 16,777 km    |
					// | 10 cm（0.1 m）  | **335,544,300 cm**   ≈ 3,355 km      |
					// | 1000 cm（10 m） | **33,554,430,000 cm** ≈ 335,544 km   |
					// | 2000 cm（20 m） | **67,108,860,000 cm** ≈ 671,088 km   |
					// | 4000 cm（40 m） | **134,217,720,000 cm**≈ 1,342,177 km |
					SerializeData(DepthPNG1KM, Width, Height, OutputPathPNG1KM);
					SerializeData(DepthPNG20KM, Width, Height, OutputPathPNG20KM);
					SerializeData(DepthPreview, Width, Height, DepthPreviewPath);
					SerializeData(DepthData, Width, Height, DepthNpyPath);
					double SerializeTime = FPlatformTime::Seconds() - SerializeStartTime;
					UE_LOG(LogTemp, Log, TEXT("[CaptureToFile] Saved async capture to %s in %.3f ms"), *OutputPath, SerializeTime * 1000.0);
				}
			);
		}
	);

	if (bAsyncCaptureNextFrame)
	{
		LaunchCapture();
	}
}