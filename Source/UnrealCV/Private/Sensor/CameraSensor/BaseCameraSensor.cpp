// Weichao Qiu @ 2017
// Performance Optimization: shc @ 2025
#include "BaseCameraSensor.h"
#include "Runtime/Engine/Classes/Components/StaticMeshComponent.h"
#include "Runtime/Engine/Classes/Engine/CollisionProfile.h"
#include "Runtime/Engine/Classes/Engine/StaticMesh.h"
#include "TextureReader.h"
#include "UnrealcvServer.h"
#include "UnrealcvStats.h"
#include "UnrealcvLog.h"
#include "ImageUtil.h"
#include "RHIGPUReadback.h"
#include "RenderingThread.h"
#include "RHISurfaceDataConversionOpt.h"
#include "SL.h"

DECLARE_CYCLE_STAT(TEXT("ReadBuffer"), STAT_ReadBuffer, STATGROUP_UnrealCV);
DECLARE_CYCLE_STAT(TEXT("ReadBufferFast"), STAT_ReadBufferFast, STATGROUP_UnrealCV);
// DECLARE_CYCLE_STAT(TEXT("ReadPixels"), STAT_ReadPixels, STATGROUP_UnrealCV);

// FImageWorker UBaseCameraSensor::ImageWorker;

UBaseCameraSensor::UBaseCameraSensor(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// static ConstructorHelpers::FObjectFinder<UStaticMesh> EditorCameraMesh(TEXT("/Engine/EditorMeshes/MatineeCam_SM"));
	// Another choice is "StaticMesh'/Engine/EditorMeshes/Camera/SM_CineCam.SM_CineCam'"
	this->ShowFlags.SetPostProcessing(true);
	bCaptureEveryFrame = false;
	bCaptureOnMovement = false;
	PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;
	CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	bUseRayTracingIfEnabled = true;
	bAlwaysPersistRenderingState = true;

	FServerConfig& Config = FUnrealcvServer::Get().Config;
	FilmWidth = Config.Width == 0 ? 640 : Config.Width;
	FilmHeight = Config.Height == 0 ? 480 : Config.Height;
	FOVAngle = Config.FOV == 0 ? 90 : Config.FOV;

	bUseFastCapture = Config.UseFastCapture;
	bUseFastCapture = true;
	// bool bSetLinearToGamma = false;
	// QueuedCaptures.Empty();
}

// Explicitly make a request to render frames
// This is needed if we want to disable bCaptureEveryFrame
// https://answers.unrealengine.com/questions/723947/scene-capture-with-post-process-mat-works-only-wit.html?sort=oldest

// if (GetOwner()) // Check whether this is a template project
// if (!IsTemplate())

// NOTE: Avoid creating TextureTarget in the CTOR, this will make CamSensor not savable in a BP actor
// TextureTarget = CreateDefaultSubobject<UTextureRenderTarget2D>(TEXT("CamSensorRenderTarget"));

void UBaseCameraSensor::InitTextureTarget(int filmWidth, int filmHeight)
{
	// this should be the same with https://github.com/unrealcv/unrealcv/blob/5.2/Source/UnrealCV/Private/Sensor/CameraSensor/BaseCameraSensor.cpp
	InitUInt8TextureTarget(filmWidth, filmHeight, false);

	// I set bUseLinearGamma to false, just because the default value is false in UnrealCV 5.2
	// But for RGB, It should be true to get a regular color
	// UE may do gamma correction twice if it is false
}

void UBaseCameraSensor::InitFloat16TextureTarget(int filmWidth, int filmHeight)
{
	//PF_FloatRGBA            =10, // RGBA16F
	TextureTarget = NewObject<UTextureRenderTarget2D>(this); 
	TextureTarget->InitAutoFormat(filmWidth, filmHeight);
	TextureTarget->TargetGamma = GEngine->GetDisplayGamma();
}

void UBaseCameraSensor::InitUInt8TextureTarget(int filmWidth, int filmHeight, bool bUseLinearGamma)
{
	EPixelFormat PixelFormat = EPixelFormat::PF_B8G8R8A8;
	TextureTarget = NewObject<UTextureRenderTarget2D>(this);
	TextureTarget->InitCustomFormat(filmWidth, filmHeight, PixelFormat, bUseLinearGamma);
	// TextureTarget->TargetGamma = GEngine->GetDisplayGamma();
}

void UBaseCameraSensor::SetFilmSize(int Width, int Height)
{
	this->FilmWidth = Width;
	this->FilmHeight = Height;
	if (!IsValid(TextureTarget))
	{
		TextureTarget = NewObject<UTextureRenderTarget2D>(this); 
		// TextureTarget = CreateDefaultSubobject<UTextureRenderTarget2D>(TEXT("CamSensorRenderTarget"));
	}

	if (TextureTarget->SizeX != Width || TextureTarget->SizeY != Height) 
	{
		InitTextureTarget(Width, Height);
	}
}

int UBaseCameraSensor::GetFilmWidth()
{
	if (!IsValid(TextureTarget)) return 0;
	return TextureTarget->SizeX;
}

int UBaseCameraSensor::GetFilmHeight()
{
	if (!IsValid(TextureTarget)) return 0;
	return TextureTarget->SizeY;
}


// TODO: Split the logic, move data serialization code outside
// Serialize the data to png and npy, check the speed.

// EPixelFormat::PF_B8G8R8A8
/*
This is defined in FColor
	#ifdef _MSC_VER
	// Win32 x86
	union { struct{ uint8 B,G,R,A; }; uint32 AlignmentDummy; };
#else
	// Linux x86, etc
	uint8 B GCC_ALIGN(4);
	uint8 G,R,A;
*/
bool UBaseCameraSensor::CheckTextureTarget()
{
	if (!IsValid(TextureTarget))
	{
		UE_LOG(LogTemp, Warning, TEXT("The TextureTarget was not initialized."));
		return false;
	}
	if (TextureTarget->SizeX == 0 || TextureTarget->SizeY == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("The TextureTarget has invalid size."));
		return false;
	}
	return true;
}

void UBaseCameraSensor::Capture(TArray<FColor>& ImageData, int& Width, int& Height)
{
	if (bUseFastCapture)
	{
		CaptureFast(ImageData, Width, Height);
		return;
	}
	else
    {
		SCOPE_CYCLE_COUNTER(STAT_ReadBuffer);

		if (!CheckTextureTarget())
		{
			UE_LOG(LogTemp, Error, TEXT("The TextureTarget was not initialized. Capture failed."));
			return;
		}
		this->CaptureScene();

		ReadTextureRenderTarget(TextureTarget, ImageData, Width, Height);
	}
}

void UBaseCameraSensor::Capture(TArray<FFloat16Color>& ImageData, int& Width, int& Height)
{
	if (bUseFastCapture)
	{
		CaptureFast(ImageData, Width, Height);
		return;
	}
	else
    {
		SCOPE_CYCLE_COUNTER(STAT_ReadBuffer);

		if (!CheckTextureTarget())
		{
			UE_LOG(LogTemp, Error, TEXT("The TextureTarget was not initialized. Capture failed."));
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
}

// void UBaseCameraSensor::CaptureToFile(const FString& Filename)
// {
//     if (!CheckTextureTarget())
//     {
//         UE_LOG(LogTemp, Error, TEXT("TextureTarget not initialized, CaptureToFile failed."));
//         return;
//     }

//     this->CaptureScene();

//     FTextureRenderTargetResource* RenderTargetResource = TextureTarget->GameThread_GetRenderTargetResource();
//     int32 Width = TextureTarget->SizeX;
//     int32 Height = TextureTarget->SizeY;

//     FString OutputPath = Filename;

//     ENQUEUE_RENDER_COMMAND(CaptureToFileCommand)(
//         [RenderTargetResource, Width, Height, OutputPath](FRHICommandListImmediate& RHICmdList)
//         {
//             TArray<FColor> PixelData;
//             PixelData.AddUninitialized(Width * Height);

//             FReadSurfaceDataFlags ReadFlags(RCM_UNorm, CubeFace_MAX);
//             RHICmdList.ReadSurfaceData(
//                 RenderTargetResource->GetRenderTargetTexture(),
//                 FIntRect(0, 0, Width, Height),
//                 PixelData,
//                 ReadFlags
//             );

//             AsyncTask(ENamedThreads::AnyThread, [PixelData = MoveTemp(PixelData), Width, Height, OutputPath]()
//             {
//                 if (SerializeData(PixelData, Width, Height, OutputPath) == FExecStatusType::OK)
//                 {
//                     UE_LOG(LogTemp, Log, TEXT("[CaptureToFile] Saved async capture to %s"), *OutputPath);
//                 }
//                 else
//                 {
//                     UE_LOG(LogTemp, Error, TEXT("[CaptureToFile] Failed to save %s"), *OutputPath);
//                 }
//             });
//         }
//     );
// }



void UBaseCameraSensor::CaptureFastToFile(const FString& Filename)
{
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
	Capture.Readback = MakeShared<FRHIGPUTextureReadback>(
		// random name
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
			// RHICmdList.Transition(FRHITransitionInfo(Texture, ERHIAccess::SRVMask, ERHIAccess::CopySrc));
			Capture.Readback->EnqueueCopy(RHICmdList, RenderTargetResource->GetRenderTargetTexture());
			// RHICmdList.Transition(FRHITransitionInfo(Texture, ERHIAccess::CopySrc, ERHIAccess::SRVMask));

			void* RawDataCopy = FMemory::Malloc(Capture.Width * Capture.Height * GPixelFormats[Capture.PixelFormat].BlockBytes);
			int32 RowPitchInPixels;
			{
				const void* RawData = Capture.Readback->Lock(RowPitchInPixels);
				FMemory::Memcpy(RawDataCopy, RawData, Capture.Width * Capture.Height * GPixelFormats[Capture.PixelFormat].BlockBytes);
				Capture.Readback->Unlock();
			}

			AsyncTask(ENamedThreads::AnyThread,
				[RawDataCopy, OutputPath = Filename, Width = Capture.Width, Height = Capture.Height, PixelFormat = Capture.PixelFormat, RowPitchInPixels = RowPitchInPixels]()
				{

					TArray<FColor> PixelData;
					PixelData.AddUninitialized(Width * Height);
					FReadSurfaceDataFlags ReadFlags(RCM_MinMax); // do not norm
					// FReadSurfaceDataFlags ReadFlags(RCM_UNorm); // norm
					ReadFlags.SetLinearToGamma(false);  // no gamma correction
					// ReadFlags.SetLinearToGamma(true);  // gamma correction

					uint32 SrcPitch = RowPitchInPixels * GPixelFormats[PixelFormat].BlockBytes;
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

					double SerializeStartTime = FPlatformTime::Seconds();
					SerializeData(PixelData, Width, Height, OutputPath);
					double SerializeTime = FPlatformTime::Seconds() - SerializeStartTime;
					UE_LOG(LogTemp, Log, TEXT("[CaptureToFile] Saved async capture to %s in %.3f ms"), *OutputPath, SerializeTime * 1000.0);
				}
			);
		}
	);

	LaunchCapture();
}

// void UBaseCameraSensor::CaptureFastToFile(const FString& Filename)
// {
// 	TArray<FColor> PixelData;
// 	int Width, Height;
// 	CaptureFast(PixelData, Width, Height);
// 	if (PixelData.Num() == Width * Height && Width > 0 && Height > 0)
// 	{
// 		AsyncTask(ENamedThreads::AnyThread,
// 			[PixelData = MoveTemp(PixelData), OutputPath = Filename, Width = Width, Height = Height]()
// 			{
// 				double SerializeStartTime = FPlatformTime::Seconds();
// 				SerializeData(PixelData, Width, Height, OutputPath);
// 				double SerializeTime = FPlatformTime::Seconds() - SerializeStartTime;
// 				UE_LOG(LogTemp, Log, TEXT("[CaptureToFile] Saved async capture to %s in %.3f ms"), *OutputPath, SerializeTime * 1000.0);
// 			}
// 		);
// 	}
// 	else
// 	{
// 		UE_LOG(LogTemp, Error, TEXT("UBaseCameraSensor::CaptureFastToFile: PixelData is empty or size not match"));
// 	}
// }

void UBaseCameraSensor::CaptureFast(TArray<FColor>& ImageData, int& Width, int& Height)
{
	CheckCaptureCache(ECaptureFormat::UInt8);

	double CaptureFastStartTime = FPlatformTime::Seconds();
	if (CopyFormat != ECaptureFormat::UInt8)
	{
		UE_LOG(LogTemp, Warning, TEXT("UBaseCameraSensor::CaptureToFile: Copy not launched for UInt8, launch it"));
		LaunchCapture();
		CopyBackCapture(ECaptureFormat::UInt8);
		SL::get().printf("CaptureFast: [X1] fallback start copy !\n");
	}
	SL::get().printf("CaptureFast: [X1] fallback start copy cost %.3f ms\n", (FPlatformTime::Seconds() - CaptureFastStartTime) * 1000.0);
	bCaptureLaunched = false;

	// busy wait
	double WaitStartTime = FPlatformTime::Seconds();
	while (!bCaptureCacheValid && (FPlatformTime::Seconds() - WaitStartTime) < 1.0)
	{
		FPlatformProcess::Sleep(0.0001f); // sleep 0.1 ms
	}
	if (!bCaptureCacheValid)
	{
		UE_LOG(LogTemp, Error, TEXT("UBaseCameraSensor::CaptureToFile: CaptureCache not valid, failed"));
		return;
	}
	SL::get().printf("CaptureFast: [X2] wait cache %.3f ms\n", (FPlatformTime::Seconds() - WaitStartTime) * 1000.0);

	// TArray<FColor> PixelData;
	double CopyStartTime = FPlatformTime::Seconds();
	if (CaptureCache.Num() == FilmWidth * FilmHeight)
	{
		ImageData = MoveTemp(CaptureCache);
		Width = FilmWidth;
		Height = FilmHeight;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("UBaseCameraSensor::CaptureToFile: CaptureCache size not match, failed"));
	}
	SL::get().printf("CaptureFast: [X3] copy cache %.3f ms\n", (FPlatformTime::Seconds() - CopyStartTime) * 1000.0);

	bCaptureCacheValid = false;
	CaptureCache = {};

	double LaunchStartTime = FPlatformTime::Seconds();
	LaunchCapture();
	CopyBackCapture(ECaptureFormat::UInt8);
	SL::get().printf("CaptureFast: [X4] launch capture and copy %.3f ms\n", (FPlatformTime::Seconds() - LaunchStartTime) * 1000.0);
}

void UBaseCameraSensor::CaptureFast(TArray<FFloat16Color>& ImageData, int& Width, int& Height)
{
	CheckCaptureCache(ECaptureFormat::F16);

	if (TextureTarget->GetFormat() != PF_FloatRGBA)
	{
		UE_LOG(LogTemp, Error, TEXT("UBaseCameraSensor::CaptureFast: TextureTarget format not PF_FloatRGBA, Go use CaptureFast FColor ver. or set the PixelFormat to PF_FloatRGBA"));
		return;
	}

	double CaptureFastStartTime = FPlatformTime::Seconds();
	if (CopyFormat != ECaptureFormat::F16)
	{
		UE_LOG(LogTemp, Warning, TEXT("UBaseCameraSensor::CaptureFast F16: Copy not launched for F16, launch it"));
		LaunchCapture();
		CopyBackCapture(ECaptureFormat::F16);
		SL::get().printf("CaptureFast: [X1] fallback start copy !\n");
	}
	SL::get().printf("CaptureFast: [X1] fallback start copy cost %.3f ms\n", (FPlatformTime::Seconds() - CaptureFastStartTime) * 1000.0);
	bCaptureLaunched = false;

	// busy wait
	double WaitStartTime = FPlatformTime::Seconds();
	while (!bCaptureCacheValid && (FPlatformTime::Seconds() - WaitStartTime) < 1.0)
	{
		FPlatformProcess::Sleep(0.0001f); // sleep 0.1 ms
	}
	if (!bCaptureCacheValid)
	{
		UE_LOG(LogTemp, Error, TEXT("UBaseCameraSensor::CaptureToFile: CaptureCache not valid, failed"));
		return;
	}
	SL::get().printf("CaptureFast: [X2] wait cache %.3f ms\n", (FPlatformTime::Seconds() - WaitStartTime) * 1000.0);

	double CopyStartTime = FPlatformTime::Seconds();
	if (CaptureCacheFloat16.Num() == FilmWidth * FilmHeight)
	{
		ImageData = MoveTemp(CaptureCacheFloat16);
		Width = FilmWidth;
		Height = FilmHeight;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("UBaseCameraSensor::CaptureToFile: CaptureCache size not match, failed"));
	}
	SL::get().printf("CaptureFast: [X3] copy cache %.3f ms\n", (FPlatformTime::Seconds() - CopyStartTime) * 1000.0);

	bCaptureCacheValid = false;
	CaptureCacheFloat16 = {};

	double LaunchStartTime = FPlatformTime::Seconds();
	LaunchCapture();
	CopyBackCapture(ECaptureFormat::F16);
	SL::get().printf("CaptureFast: [X4] launch capture and copy %.3f ms\n", (FPlatformTime::Seconds() - LaunchStartTime) * 1000.0);
}



void UBaseCameraSensor::SetPostProcessMaterial(UMaterial* PostProcessMaterial)
{
	PostProcessSettings.AddBlendable(PostProcessMaterial, 1);
}

void UBaseCameraSensor::GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView)
{
	DesiredView.Location = GetComponentLocation();
	DesiredView.Rotation = GetComponentRotation();
	DesiredView.FOV = this->FOVAngle;
	// DesiredView.FOV = FieldOfView;
	// DesiredView.bConstrainAspectRatio = bConstrainAspectRatio;
	// DesiredView.bUseFieldOfViewForLOD = bUseFieldOfViewForLOD;
	// DesiredView.ProjectionMode = ProjectionMode;
	DesiredView.ProjectionMode = ECameraProjectionMode::Perspective;
	DesiredView.OrthoWidth = OrthoWidth;
	// DesiredView.OrthoNearClipPlane = OrthoNearClipPlane;
	// DesiredView.OrthoFarClipPlane = OrthoFarClipPlane;

	// See if the CameraActor wants to override the PostProcess settings used.
	DesiredView.PostProcessBlendWeight = PostProcessBlendWeight;
	if (PostProcessBlendWeight > 0.0f)
	{
		DesiredView.PostProcessSettings = PostProcessSettings;
	}

}
void UBaseCameraSensor::ReadCaptureResults(TArray<FColor>& Data)
{
	FReadSurfaceDataFlags ReadSurfaceDataFlags;
	ReadSurfaceDataFlags.SetLinearToGamma(false);
	TextureTarget->GameThread_GetRenderTargetResource()->ReadPixels(Data, ReadSurfaceDataFlags);
	if (Data.Num() == 0)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("Captured lit data is empty."));
	}
}

void UBaseCameraSensor::LaunchCapture()
{
	if (!CheckTextureTarget())
	{
		UE_LOG(LogTemp, Error, TEXT("TextureTarget not initialized, CaptureToGPUQueue failed."));
		return;
	}
	this->CaptureScene();
	CaptureTimestamp = FPlatformTime::Seconds();
	bCaptureLaunched = true;
}

void UBaseCameraSensor::CopyBackCapture(ECaptureFormat Format)
{
	EPixelFormat PixelFormat = TextureTarget->GetFormat();
	UE_LOG(LogTemp, Warning, TEXT("[DEBUG] TextureTarget Format: %d, SRGB: %d, Gamma: %f"),
		(int32)PixelFormat,
		TextureTarget->SRGB,
		TextureTarget->TargetGamma);

	FTextureRenderTargetResource* RenderTargetResource = TextureTarget->GameThread_GetRenderTargetResource();
	int32 Width = TextureTarget->SizeX;
	int32 Height = TextureTarget->SizeY;

	FQueuedCapture Capture;
	Capture.Readback = MakeShared<FRHIGPUTextureReadback>(
		*FString::Printf(TEXT("Capture_%d"), FMath::Rand())
	);
	Capture.OutputPath = TEXT("");
	Capture.Width = Width;
	Capture.Height = Height;
	Capture.PixelFormat = PixelFormat;

	double RenderStartTime = FPlatformTime::Seconds();

	// ENQUEUE_RENDER_COMMAND(EnqueueGPUCopy)(
	// 	[RenderTargetResource, Readback = CaptureCache.Readback.Get(), RenderStartTime](FRHICommandListImmediate& RHICmdList)
	// 	{
	// 		SL::get().printf("[R0] Start time: %.3f ms", (FPlatformTime::Seconds() - RenderStartTime) * 1000.0);
	// 		double FlushStartTime = FPlatformTime::Seconds();
	// 		RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	// 		SL::get().printf("[R1] Flush time: %.3f ms", (FPlatformTime::Seconds() - FlushStartTime) * 1000.0);

	// 		double EnqueueStartTime = FPlatformTime::Seconds();
	// 		Readback->EnqueueCopy(RHICmdList, RenderTargetResource->GetRenderTargetTexture());
	// 		SL::get().printf("[R2] EnqueueCopy time: %.3f ms", (FPlatformTime::Seconds() - EnqueueStartTime) * 1000.0);

	// 		double TotalRenderTime = FPlatformTime::Seconds() - RenderStartTime;
	// 		SL::get().printf("[R012] Total render thread time: %.3f ms", TotalRenderTime * 1000.0);
	// 	}
	// );

	void* PixelDataPtr;
	if (Format == ECaptureFormat::F16)
	{
		PixelDataPtr = CaptureCacheFloat16.GetData();
		CaptureCache.Empty();
		CaptureCache.AddUninitialized(Capture.Width * Capture.Height);
	}
	else if (Format == ECaptureFormat::UInt8)
	{
		PixelDataPtr = CaptureCache.GetData();
		CaptureCacheFloat16.Empty();
		CaptureCacheFloat16.AddUninitialized(Capture.Width * Capture.Height);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("UBaseCameraSensor::CopyBackCapture: Invalid format, failed"));
		return;
	}
	bCaptureCacheValid = false;


	ENQUEUE_RENDER_COMMAND(EnqueueGPUCopy)(
		[RenderTargetResource, Capture = MoveTemp(Capture), Format = Format, RenderStartTime, PixelData = PixelDataPtr, bCaptureCacheValidPtr = &bCaptureCacheValid](FRHICommandListImmediate& RHICmdList)
		{
			SL::get().printf("[R0] Start time: %.3f ms", (FPlatformTime::Seconds() - RenderStartTime) * 1000.0);
			double FlushStartTime = FPlatformTime::Seconds();
			RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
			SL::get().printf("[R1] Flush time: %.3f ms", (FPlatformTime::Seconds() - FlushStartTime) * 1000.0);

			double EnqueueStartTime = FPlatformTime::Seconds();
			Capture.Readback->EnqueueCopy(RHICmdList, RenderTargetResource->GetRenderTargetTexture());
			SL::get().printf("[R2] EnqueueCopy time: %.3f ms", (FPlatformTime::Seconds() - EnqueueStartTime) * 1000.0);

			double LockStartTime = FPlatformTime::Seconds();
			int32 RowPitchInPixels;
			const void* RawData = Capture.Readback->Lock(RowPitchInPixels);
			SL::get().printf("[R3] Lock time: %.3f ms", (FPlatformTime::Seconds() - LockStartTime) * 1000.0);

			// Process data immediately on render thread to avoid accessing invalid memory


	// FReadSurfaceDataFlags ReadSurfaceDataFlags = bNormalize ? FReadSurfaceDataFlags() : FReadSurfaceDataFlags(RCM_MinMax);
			FReadSurfaceDataFlags ReadFlags(RCM_MinMax); // do not norm
			// FReadSurfaceDataFlags ReadFlags(RCM_UNorm); // norm
			ReadFlags.SetLinearToGamma(false);  // no gamma correction
			// ReadFlags.SetLinearToGamma(true);  // gamma correction

			uint32 SrcPitch = RowPitchInPixels * GPixelFormats[Capture.PixelFormat].BlockBytes;

			double ConvertStartTime = FPlatformTime::Seconds();
			if (Format == ECaptureFormat::UInt8)
			{
				ConvertRAWSurfaceDataToFColorOpt(
					Capture.PixelFormat,
					Capture.Width,
					Capture.Height,
					(uint8*)RawData,
					SrcPitch,
					(FColor*)PixelData,
					ReadFlags
				);
			}
			else if (Format == ECaptureFormat::F16)
			{
				ConvertRAWSurfaceDataToFFloat16ColorOpt(
					Capture.PixelFormat,
					Capture.Width,
					Capture.Height,
					(uint8*)RawData,
					SrcPitch,
					(FFloat16Color*)PixelData,
					ReadFlags
				);
			} 
			else
			{
				UE_LOG(LogTemp, Error, TEXT("UBaseCameraSensor::CopyBackCapture: Invalid format, failed"));
				check(0);
			}
			*bCaptureCacheValidPtr = true;
			SL::get().printf("[R5] ConvertRAWSurfaceData time: %.3f ms", (FPlatformTime::Seconds() - ConvertStartTime) * 1000.0);

			// Unlock the readback data
			double UnlockStartTime = FPlatformTime::Seconds();
			Capture.Readback->Unlock();
			SL::get().printf("[R6] Unlock time: %.3f ms", (FPlatformTime::Seconds() - UnlockStartTime) * 1000.0);

			double TotalRenderTime = FPlatformTime::Seconds() - RenderStartTime;
			SL::get().printf("[R7] Total render thread time: %.3f ms", TotalRenderTime * 1000.0);

		}
	);


	CopyFormat = Format;
}

void UBaseCameraSensor::CleanCaptureCache()
{
	CaptureCache.Empty();
	CaptureCacheFloat16.Empty();
	bCaptureCacheValid = false;
	CopyFormat = ECaptureFormat::Invalid;
	bCaptureLaunched = false;
}

void UBaseCameraSensor::CheckCaptureCache(ECaptureFormat Format)
{
	bool bValid = true;
	if (Format != ECaptureFormat::Invalid)
	{
		bValid = CopyFormat == Format;
	}
	
	if (bCaptureCacheValid && CaptureTimestamp > 0.0 && bValid)
	{
		double ElapsedTime = FPlatformTime::Seconds() - CaptureTimestamp;
		if (ElapsedTime > 1.0)
		{
			UE_LOG(LogTemp, Warning, TEXT("UBaseCameraSensor::CheckCaptureCache: Cache expired (%.3f seconds), clearing"), ElapsedTime);
			CleanCaptureCache();
		}
	}
}

// void UBaseCameraSensor::ConvertCapture(TArray<FColor>& OutPixelData, int32& OutWidth, int32& OutHeight)
// {
// 	if (!bCaptureCacheValid)
// 	{
// 		UE_LOG(LogTemp, Error, TEXT("CaptureCache not valid, ConvertCapture failed."));
// 		return;
// 	}

// 	double WaitStartTime = FPlatformTime::Seconds();
// 	if (!CaptureCache.Readback->IsReady())
// 	{
// 		// busy wait
// 		while (!CaptureCache.Readback->IsReady() && (FPlatformTime::Seconds() - WaitStartTime) < 1.0)
// 		{
// 			FPlatformProcess::Sleep(0.0001f); // sleep 0.1 ms
// 		}

// 		if (!CaptureCache.Readback->IsReady())
// 		{
// 			UE_LOG(LogTemp, Error, TEXT("CaptureCache.Readback not ready after 1s, ConvertCapture failed."));
// 			return;
// 		}
// 	}
// 	UE_LOG(LogTemp, Warning, TEXT("Wait time: %.3f ms"), (FPlatformTime::Seconds() - WaitStartTime) * 1000.0);

// 	FQueuedCapture Capture = MoveTemp(CaptureCache);
// 	bCaptureCacheValid = false;

// 	double LockStartTime = FPlatformTime::Seconds();
// 	int32 RowPitchInPixels;
// 	const void* RawData = Capture.Readback->Lock(RowPitchInPixels);
// 	SL::get().printf("[R3] Lock time: %.3f ms", (FPlatformTime::Seconds() - LockStartTime) * 1000.0);

// 	// Process data immediately on render thread to avoid accessing invalid memory
// 	double AllocStartTime = FPlatformTime::Seconds();
// 	OutPixelData.Empty();
// 	OutPixelData.AddUninitialized(Capture.Width * Capture.Height);
// 	SL::get().printf("[R4] PixelData allocation time: %.3f ms", (FPlatformTime::Seconds() - AllocStartTime) * 1000.0);

// 	FReadSurfaceDataFlags ReadFlags;
// 	ReadFlags.SetLinearToGamma(false);

// 	uint32 SrcPitch = RowPitchInPixels * GPixelFormats[Capture.PixelFormat].BlockBytes;

// 	double ConvertStartTime = FPlatformTime::Seconds();
// 	ConvertRAWSurfaceDataToFColorOpt(
// 		Capture.PixelFormat,
// 		Capture.Width,
// 		Capture.Height,
// 		(uint8*)RawData,
// 		SrcPitch,
// 		OutPixelData.GetData(),
// 		ReadFlags
// 	);
// 	SL::get().printf("[R5] ConvertRAWSurfaceData time: %.3f ms", (FPlatformTime::Seconds() - ConvertStartTime) * 1000.0);
	
// 	// Unlock the readback data
// 	double UnlockStartTime = FPlatformTime::Seconds();
// 	Capture.Readback->Unlock();
// 	SL::get().printf("[R6] Unlock time: %.3f ms", (FPlatformTime::Seconds() - UnlockStartTime) * 1000.0);

// 	OutWidth = Capture.Width;
// 	OutHeight = Capture.Height;
// }

// void UBaseCameraSensor::CaptureToGPUQueue(const FString& Filename)
// {
// 	if (!CheckTextureTarget())
// 	{
// 		UE_LOG(LogTemp, Error, TEXT("TextureTarget not initialized, CaptureToGPUQueue failed."));
// 		return;
// 	}

// 	EPixelFormat PixelFormat = TextureTarget->GetFormat();
// 	UE_LOG(LogTemp, Warning, TEXT("[DEBUG] TextureTarget Format: %d, SRGB: %d, Gamma: %f"),
// 		(int32)PixelFormat,
// 		TextureTarget->SRGB,
// 		TextureTarget->TargetGamma);

// 	this->CaptureScene();

// 	FTextureRenderTargetResource* RenderTargetResource = TextureTarget->GameThread_GetRenderTargetResource();
// 	int32 Width = TextureTarget->SizeX;
// 	int32 Height = TextureTarget->SizeY;

// 	FQueuedCapture NewCapture;
// 	NewCapture.Readback = MakeUnique<FRHIGPUTextureReadback>(
// 		*FString::Printf(TEXT("QueuedCapture_%d"), QueuedCaptures.Num())
// 	);
// 	NewCapture.OutputPath = Filename;
// 	NewCapture.Width = Width;
// 	NewCapture.Height = Height;
// 	NewCapture.PixelFormat = PixelFormat;

// 	double RenderStartTime = FPlatformTime::Seconds();

// 	ENQUEUE_RENDER_COMMAND(EnqueueGPUCopy)(
// 		[RenderTargetResource, Capture = MoveTemp(NewCapture), RenderStartTime](FRHICommandListImmediate& RHICmdList)
// 		{
// 			SL::get().printf("[R0] Start time: %.3f ms", (FPlatformTime::Seconds() - RenderStartTime) * 1000.0);
// 			double FlushStartTime = FPlatformTime::Seconds();
// 			RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
// 			SL::get().printf("[R1] Flush time: %.3f ms", (FPlatformTime::Seconds() - FlushStartTime) * 1000.0);

// 			double EnqueueStartTime = FPlatformTime::Seconds();
// 			Capture.Readback->EnqueueCopy(RHICmdList, RenderTargetResource->GetRenderTargetTexture());
// 			SL::get().printf("[R2] EnqueueCopy time: %.3f ms", (FPlatformTime::Seconds() - EnqueueStartTime) * 1000.0);

// 			double LockStartTime = FPlatformTime::Seconds();
// 			int32 RowPitchInPixels;
// 			const void* RawData = Capture.Readback->Lock(RowPitchInPixels);
// 			SL::get().printf("[R3] Lock time: %.3f ms", (FPlatformTime::Seconds() - LockStartTime) * 1000.0);

// 			// Process data immediately on render thread to avoid accessing invalid memory
// 			double AllocStartTime = FPlatformTime::Seconds();
// 			TArray<FColor> PixelData;
// 			PixelData.AddUninitialized(Capture.Width * Capture.Height);
// 			SL::get().printf("[R4] PixelData allocation time: %.3f ms", (FPlatformTime::Seconds() - AllocStartTime) * 1000.0);

// 			FReadSurfaceDataFlags ReadFlags;
// 			ReadFlags.SetLinearToGamma(false);

// 			uint32 SrcPitch = RowPitchInPixels * GPixelFormats[Capture.PixelFormat].BlockBytes;

// 			double ConvertStartTime = FPlatformTime::Seconds();
// 			ConvertRAWSurfaceDataToFColorOpt(
// 				Capture.PixelFormat,
// 				Capture.Width,
// 				Capture.Height,
// 				(uint8*)RawData,
// 				SrcPitch,
// 				PixelData.GetData(),
// 				ReadFlags
// 			);
// 			SL::get().printf("[R5] ConvertRAWSurfaceData time: %.3f ms", (FPlatformTime::Seconds() - ConvertStartTime) * 1000.0);

// 			// Unlock the readback data
// 			double UnlockStartTime = FPlatformTime::Seconds();
// 			Capture.Readback->Unlock();
// 			SL::get().printf("[R6] Unlock time: %.3f ms", (FPlatformTime::Seconds() - UnlockStartTime) * 1000.0);

// 			double TotalRenderTime = FPlatformTime::Seconds() - RenderStartTime;
// 			SL::get().printf("[R7] Total render thread time: %.3f ms", TotalRenderTime * 1000.0);

// 			// Move to game thread for file I/O
// 			AsyncTask(ENamedThreads::AnyThread,
// 				[PixelData = MoveTemp(PixelData), OutputPath = Capture.OutputPath, Width = Capture.Width, Height = Capture.Height]()
// 				{
// 					double SerializeStartTime = FPlatformTime::Seconds();

// 					if (SerializeData(PixelData, Width, Height, OutputPath) == FExecStatusType::OK)
// 					{
// 						double SerializeTime = FPlatformTime::Seconds() - SerializeStartTime;
// 						SL::get().printf("[A1] Saved %s in %.3f ms",
// 							TCHAR_TO_UTF8(*OutputPath), SerializeTime * 1000.0);
// 					}
// 					else
// 					{
// 						SL::get().printf("[A1] Failed to save %s", TCHAR_TO_UTF8(*OutputPath));
// 					}
// 				});
// 		}
// 	);

// 	QueuedCaptures.Add(MoveTemp(NewCapture));

// 	UE_LOG(LogUnrealCV, Verbose, TEXT("CaptureToGPUQueue: Enqueued %s, total=%d"),
// 		*Filename, QueuedCaptures.Num());
// }

// void UBaseCameraSensor::FlushCapturesToDisk()
// {
// 	return;


















// 	if (QueuedCaptures.Num() == 0)
// 	{
// 		UE_LOG(LogUnrealCV, Warning, TEXT("FlushCapturesToDisk: No captures queued"));
// 		return;
// 	}

// 	int32 NumCaptures = QueuedCaptures.Num();
// 	double FlushStartTime = FPlatformTime::Seconds();
// 	UE_LOG(LogUnrealCV, Log, TEXT("FlushCapturesToDisk: Processing %d captures"), NumCaptures);

// 	TArray<FQueuedCapture> CapturesToFlush = MoveTemp(QueuedCaptures);
// 	QueuedCaptures.Empty();

// 	ENQUEUE_RENDER_COMMAND(FlushQueuedCaptures)(
// 		[CapturesToFlush = MoveTemp(CapturesToFlush), NumCaptures, FlushStartTime](FRHICommandListImmediate& RHICmdList) mutable
// 		{
// 			double RenderThreadStartTime = FPlatformTime::Seconds();
// 			double TotalWaitTime = 0.0;
// 			double TotalLockTime = 0.0;
// 			double TotalConversionTime = 0.0;
// 			int32 NumBlocked = 0;

// 			struct FLockedCaptureData
// 			{
// 				const void* RawData;
// 				int32 RowPitchInPixels;
// 				FString OutputPath;
// 				int32 Width;
// 				int32 Height;
// 				EPixelFormat PixelFormat;
// 			};

// 			TArray<FLockedCaptureData> LockedCaptures;
// 			LockedCaptures.Reserve(CapturesToFlush.Num());

// 			double LockStartTime = FPlatformTime::Seconds();

// 			for (int32 i = 0; i < CapturesToFlush.Num(); ++i)
// 			{
// 				auto& Capture = CapturesToFlush[i];

// 				if (!Capture.Readback->IsReady())
// 				{
// 					double WaitStartTime = FPlatformTime::Seconds();
// 					RHICmdList.BlockUntilGPUIdle();
// 					TotalWaitTime += FPlatformTime::Seconds() - WaitStartTime;
// 					NumBlocked++;
// 				}

// 				int32 RowPitchInPixels;
// 				const void* RawData = Capture.Readback->Lock(RowPitchInPixels);

// 				if (RawData)
// 				{
// 					FLockedCaptureData LockedData;
// 					LockedData.RawData = RawData;
// 					LockedData.RowPitchInPixels = RowPitchInPixels;
// 					LockedData.OutputPath = Capture.OutputPath;
// 					LockedData.Width = Capture.Width;
// 					LockedData.Height = Capture.Height;
// 					LockedData.PixelFormat = Capture.PixelFormat;
// 					LockedCaptures.Add(LockedData);
// 				}
// 			}

// 			TotalLockTime = FPlatformTime::Seconds() - LockStartTime;

// 			double ConversionStartTime = FPlatformTime::Seconds();

// 			TArray<TArray<FColor>> AllPixelData;
// 			AllPixelData.SetNum(LockedCaptures.Num());

// 			for (int32 i = 0; i < AllPixelData.Num(); ++i)
// 			{
// 				AllPixelData[i].SetNumUninitialized(LockedCaptures[i].Width * LockedCaptures[i].Height);
// 			}

// 			ParallelFor(LockedCaptures.Num(), [&](int32 i)
// 			{
// 				const FLockedCaptureData& LockedData = LockedCaptures[i];

// 				// FReadSurfaceDataFlags ReadFlags(RCM_UNorm, CubeFace_MAX);
// 				FReadSurfaceDataFlags ReadFlags;
// 				ReadFlags.SetLinearToGamma(false);

// 				uint32 SrcPitch = LockedData.RowPitchInPixels * GPixelFormats[LockedData.PixelFormat].BlockBytes;

// 				ConvertRAWSurfaceDataToFColorOpt(
// 					LockedData.PixelFormat,
// 					LockedData.Width,
// 					LockedData.Height,
// 					(uint8*)LockedData.RawData,
// 					SrcPitch,
// 					AllPixelData[i].GetData(),
// 					ReadFlags
// 				);
// 				// // AllPixelData[i].GetData()
// 				// for (int32 Index = 0; Index < AllPixelData[i].Num(); ++Index)
// 				// {
// 				// 	AllPixelData[i][Index] = AllPixelData[i][Index].ReinterpretAsLinear();
// 				// }
// 			});

// 			TotalConversionTime = FPlatformTime::Seconds() - ConversionStartTime;

// 			for (auto& Capture : CapturesToFlush)
// 			{
// 				Capture.Readback->Unlock();
// 			}

// 			for (int32 i = 0; i < LockedCaptures.Num(); ++i)
// 			{
// 				FString OutputPath = LockedCaptures[i].OutputPath;
// 				int32 Width = LockedCaptures[i].Width;
// 				int32 Height = LockedCaptures[i].Height;

// 				AsyncTask(ENamedThreads::AnyThread,
// 					[PixelData = MoveTemp(AllPixelData[i]), OutputPath, Width, Height]()
// 				{
// 					double SerializeStartTime = FPlatformTime::Seconds();

// 					if (SerializeData(PixelData, Width, Height, OutputPath) == FExecStatusType::OK)
// 					{
// 						double SerializeTime = FPlatformTime::Seconds() - SerializeStartTime;
// 						SL::get().printf("[PERF] Saved %s in %.3f ms",
// 							TCHAR_TO_UTF8(*OutputPath), SerializeTime * 1000.0);
// 					}
// 					else
// 					{
// 						SL::get().printf("[ERROR] Failed to save %s", TCHAR_TO_UTF8(*OutputPath));
// 					}
// 				});
// 			}

// 			double RenderThreadTotalTime = FPlatformTime::Seconds() - RenderThreadStartTime;
// 			double GameThreadTotalTime = FPlatformTime::Seconds() - FlushStartTime;

// 			SL::get().printf("[PERF SUMMARY] FlushCapturesToDisk: %d captures", NumCaptures);
// 			SL::get().printf("[PERF] Total game thread time: %.3f ms (%.3f ms/frame)",
// 				GameThreadTotalTime * 1000.0, (GameThreadTotalTime * 1000.0) / NumCaptures);
// 			SL::get().printf("[PERF] Total render thread time: %.3f ms (%.3f ms/frame)",
// 				RenderThreadTotalTime * 1000.0, (RenderThreadTotalTime * 1000.0) / NumCaptures);
// 			SL::get().printf("[PERF] GPU wait time: %.3f ms (%.3f ms/frame) - %d frames blocked",
// 				TotalWaitTime * 1000.0, NumBlocked > 0 ? (TotalWaitTime * 1000.0) / NumBlocked : 0.0, NumBlocked);
// 			SL::get().printf("[PERF] Lock time (GPU->CPU DMA): %.3f ms (%.3f ms/frame)",
// 				TotalLockTime * 1000.0, (TotalLockTime * 1000.0) / NumCaptures);
// 			SL::get().printf("[PERF] Pixel conversion time (ParallelFor): %.3f ms (%.3f ms/frame)",
// 				TotalConversionTime * 1000.0, (TotalConversionTime * 1000.0) / NumCaptures);
// 			SL::get().printf("[PERF] Theoretical speedup: %.2fx (serial: %.3f ms -> parallel: %.3f ms)",
// 				(TotalLockTime + 7401.0) / (TotalLockTime + TotalConversionTime),
// 				7401.0, TotalConversionTime * 1000.0);
// 		}
// 	);
// }