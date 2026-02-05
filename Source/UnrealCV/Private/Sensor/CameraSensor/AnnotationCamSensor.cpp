// Weichao Qiu @ 2017
#include "AnnotationCamSensor.h"
#include "Runtime/Engine/Classes/Engine/TextureRenderTarget2D.h"
#include "Runtime/Engine/Classes/Engine/World.h"
#include "Runtime/CoreUObject/Public/UObject/UObjectHash.h"

#include "Component/AnnotationComponent.h"
#include "UnrealcvLog.h"
#include "Runtime/Core/Public/Async/ParallelFor.h"
#include "Serialization.h"
#include "ImageUtil.h"
#include "RHISurfaceDataConversionOpt.h"
#include "SetAlpha.h"
#include "BPFunctionLib/AnnotationBPLib.h"

UAnnotationCamSensor::UAnnotationCamSensor(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	bUseShowOnlyComponentsOverride(false)
{
	this->PrimaryComponentTick.bCanEverTick = true;
	this->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	this->HiddenComponents.Reset();

	this->ShowFlags.SetMaterials(false);
	this->ShowFlags.SetLighting(false);
	this->ShowFlags.SetPostProcessing(false);
	this->ShowFlags.SetColorGrading(false);
	this->ShowFlags.SetTonemapper(false);
	this->ShowFlags.SetAtmosphere(false);
	this->ShowFlags.SetFog(false);

	this->PostProcessSettings.bOverride_AutoExposureBias = true;
	this->PostProcessSettings.AutoExposureBias = 0;

	bRenderInMainRenderer = true;  // optimization
}


void UAnnotationCamSensor::InitTextureTarget(int filmWidth, int filmHeight)
{
	InitUInt8TextureTarget(filmWidth, filmHeight, true);
}

void UAnnotationCamSensor::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction * T)
{
	Super::TickComponent(DeltaTime, TickType, T);
}

void UAnnotationCamSensor::CaptureSeg(TArray<FColor>& ImageData, int& Width, int& Height)
{
	if (!CheckTextureTarget())
	{
		UE_LOG(LogUnrealCV, Error, TEXT("TextureTarget not initialized, CaptureSeg failed."));
		return;
	}

	PrepareShowOnlyComponents();

	Capture(ImageData, Width, Height);

	if (ImageData.Num() != 0)
	{
		if (Width > 0 && Height > 0 && static_cast<uint32>(Width * Height) == ImageData.Num())
		{
			SetAlpha(ImageData);
		}
		else
		{
			UE_LOG(LogUnrealCV, Warning, TEXT("Invalid Width or Height for ImageData in CaptureSeg"));
		}
	}
}

void UAnnotationCamSensor::CaptureSegToFile(const FString& Filename)
{
	if (!CheckTextureTarget())
	{
		UE_LOG(LogUnrealCV, Error, TEXT("TextureTarget not initialized, CaptureSegToFile failed."));
		return;
	}

	PrepareShowOnlyComponents();

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

					TArray<FColor> PixelData;
					PixelData.AddUninitialized(Width * Height);
					FReadSurfaceDataFlags ReadFlags(RCM_MinMax);
					ReadFlags.SetLinearToGamma(false);

					uint32 SrcPitch = RowPitchInPixels * GPixelFormats[PixelFormat].BlockBytes;
					// uint32 SrcPitch = Width * GPixelFormats[PixelFormat].BlockBytes;
					UE_LOG(LogTemp, Warning, TEXT("CaptureSegToFile: SrcPitch = %d, RowPitchInPixels = %d, BlockBytes = %d"), SrcPitch, RowPitchInPixels, GPixelFormats[PixelFormat].BlockBytes);
					ConvertRAWSurfaceDataToFColorOpt(
						PixelFormat,
						Width,
						Height,
						(uint8*)RawDataCopy,
						SrcPitch,
						PixelData.GetData(),
						ReadFlags
					);
					FMemory::Free(RawDataCopy);

					SetAlpha(PixelData);

					double SerializeStartTime = FPlatformTime::Seconds();
					SerializeData(PixelData, Width, Height, OutputPath);
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



void UAnnotationCamSensor::PrepareShowOnlyComponents()
{
	// this->HiddenActors.Reset();
	// this->HiddenComponents.Reset();
	if (bUseShowOnlyComponentsOverride)
	{
		ShowOnlyComponents.Reset();
		this->ShowOnlyComponents = this->ShowOnlyComponentsOverride;
	} 
	else
	{

		TArray<TWeakObjectPtr<UPrimitiveComponent>> ComponentList;
		UAnnotationBPLib::GetAnnotationComponents(this->GetWorld(), ComponentList);
		this->ShowOnlyComponents = ComponentList;
	}
}

