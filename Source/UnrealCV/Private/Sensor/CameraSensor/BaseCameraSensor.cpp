// Weichao Qiu @ 2017
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

	QueuedCaptures.Empty();
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
	// bool bUseLinearGamma = false;
	EPixelFormat PixelFormat = EPixelFormat::PF_B8G8R8A8;
	bool bUseLinearGamma = false;
	TextureTarget = NewObject<UTextureRenderTarget2D>(this); 
	TextureTarget->InitCustomFormat(filmWidth, filmHeight, PixelFormat, bUseLinearGamma);
	TextureTarget->TargetGamma = GEngine->GetDisplayGamma();
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
	SCOPE_CYCLE_COUNTER(STAT_ReadBuffer);

	if (!CheckTextureTarget())
	{
		UE_LOG(LogTemp, Error, TEXT("The TextureTarget was not initialized. Capture failed."));
		return;
	}
	this->CaptureScene();

	ReadTextureRenderTarget(TextureTarget, ImageData, Width, Height);
}

void UBaseCameraSensor::CaptureToFile(const FString& Filename)
{
    if (!CheckTextureTarget())
    {
        UE_LOG(LogTemp, Error, TEXT("TextureTarget not initialized, CaptureToFile failed."));
        return;
    }

    this->CaptureScene();

    FTextureRenderTargetResource* RenderTargetResource = TextureTarget->GameThread_GetRenderTargetResource();
    int32 Width = TextureTarget->SizeX;
    int32 Height = TextureTarget->SizeY;

    FString OutputPath = Filename;

    ENQUEUE_RENDER_COMMAND(CaptureToFileCommand)(
        [RenderTargetResource, Width, Height, OutputPath](FRHICommandListImmediate& RHICmdList)
        {
            TArray<FColor> PixelData;
            PixelData.AddUninitialized(Width * Height);

            FReadSurfaceDataFlags ReadFlags(RCM_UNorm, CubeFace_MAX);
            RHICmdList.ReadSurfaceData(
                RenderTargetResource->GetRenderTargetTexture(),
                FIntRect(0, 0, Width, Height),
                PixelData,
                ReadFlags
            );

            AsyncTask(ENamedThreads::AnyThread, [PixelData = MoveTemp(PixelData), Width, Height, OutputPath]()
            {
                if (SerializeData(PixelData, Width, Height, OutputPath) == FExecStatusType::OK)
                {
                    UE_LOG(LogTemp, Log, TEXT("[CaptureToFile] Saved async capture to %s"), *OutputPath);
                }
                else
                {
                    UE_LOG(LogTemp, Error, TEXT("[CaptureToFile] Failed to save %s"), *OutputPath);
                }
            });
        }
    );
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

void UBaseCameraSensor::CaptureToGPUQueue(const FString& Filename)
{
	if (!CheckTextureTarget())
	{
		UE_LOG(LogTemp, Error, TEXT("TextureTarget not initialized, CaptureToGPUQueue failed."));
		return;
	}

	EPixelFormat PixelFormat = TextureTarget->GetFormat();
	UE_LOG(LogTemp, Warning, TEXT("[DEBUG] TextureTarget Format: %d, SRGB: %d, Gamma: %f"),
		(int32)PixelFormat,
		TextureTarget->SRGB,
		TextureTarget->TargetGamma);

	this->CaptureScene();

	FTextureRenderTargetResource* RenderTargetResource = TextureTarget->GameThread_GetRenderTargetResource();
	int32 Width = TextureTarget->SizeX;
	int32 Height = TextureTarget->SizeY;

	FQueuedCapture NewCapture;
	NewCapture.Readback = MakeUnique<FRHIGPUTextureReadback>(
		*FString::Printf(TEXT("QueuedCapture_%d"), QueuedCaptures.Num())
	);
	NewCapture.OutputPath = Filename;
	NewCapture.Width = Width;
	NewCapture.Height = Height;
	NewCapture.PixelFormat = PixelFormat;

	ENQUEUE_RENDER_COMMAND(EnqueueGPUCopy)(
		[RenderTargetResource, Capture = MoveTemp(NewCapture)](FRHICommandListImmediate& RHICmdList)
		{
			RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
			Capture.Readback->EnqueueCopy(RHICmdList, RenderTargetResource->GetRenderTargetTexture());
			AsyncTask(ENamedThreads::AnyThread, [RenderTargetResource, Capture](){
				int32 RowPitchInPixels;
				const void* RawData = Capture.Readback->Lock(RowPitchInPixels);

				TArray<FColor> PixelData;
				PixelData.AddUninitialized(Capture.Width * Capture.Height);

				FReadSurfaceDataFlags ReadFlags;
				ReadFlags.SetLinearToGamma(false);

				uint32 SrcPitch = RowPitchInPixels * GPixelFormats[Capture.PixelFormat].BlockBytes;

				ConvertRAWSurfaceDataToFColorOpt(
					Capture.PixelFormat,
					Capture.Width,
					Capture.Height,
					(uint8*)RawData,
					SrcPitch,
					PixelData.GetData(),
					ReadFlags
				);

				AsyncTask(ENamedThreads::AnyThread,
					[PixelData = MoveTemp(PixelData), OutputPath = MoveTemp(Capture.OutputPath), Width = Capture.Width, Height = Capture.Height]()
				{
					double SerializeStartTime = FPlatformTime::Seconds();

					if (SerializeData(PixelData, Width, Height, OutputPath) == FExecStatusType::OK)
					{
						double SerializeTime = FPlatformTime::Seconds() - SerializeStartTime;
						SL::get().printf("[A1] Saved %s in %.3f ms",
							TCHAR_TO_UTF8(*OutputPath), SerializeTime * 1000.0);
					}
					else
					{
						SL::get().printf("[A1] Failed to save %s", TCHAR_TO_UTF8(*OutputPath));
					}
				});
			});
		}
	);

	QueuedCaptures.Add(MoveTemp(NewCapture));

	UE_LOG(LogUnrealCV, Verbose, TEXT("CaptureToGPUQueue: Enqueued %s, total=%d"),
		*Filename, QueuedCaptures.Num());
}

void UBaseCameraSensor::FlushCapturesToDisk()
{
	return;


















	if (QueuedCaptures.Num() == 0)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("FlushCapturesToDisk: No captures queued"));
		return;
	}

	int32 NumCaptures = QueuedCaptures.Num();
	double FlushStartTime = FPlatformTime::Seconds();
	UE_LOG(LogUnrealCV, Log, TEXT("FlushCapturesToDisk: Processing %d captures"), NumCaptures);

	TArray<FQueuedCapture> CapturesToFlush = MoveTemp(QueuedCaptures);
	QueuedCaptures.Empty();

	ENQUEUE_RENDER_COMMAND(FlushQueuedCaptures)(
		[CapturesToFlush = MoveTemp(CapturesToFlush), NumCaptures, FlushStartTime](FRHICommandListImmediate& RHICmdList) mutable
		{
			double RenderThreadStartTime = FPlatformTime::Seconds();
			double TotalWaitTime = 0.0;
			double TotalLockTime = 0.0;
			double TotalConversionTime = 0.0;
			int32 NumBlocked = 0;

			struct FLockedCaptureData
			{
				const void* RawData;
				int32 RowPitchInPixels;
				FString OutputPath;
				int32 Width;
				int32 Height;
				EPixelFormat PixelFormat;
			};

			TArray<FLockedCaptureData> LockedCaptures;
			LockedCaptures.Reserve(CapturesToFlush.Num());

			double LockStartTime = FPlatformTime::Seconds();

			for (int32 i = 0; i < CapturesToFlush.Num(); ++i)
			{
				auto& Capture = CapturesToFlush[i];

				if (!Capture.Readback->IsReady())
				{
					double WaitStartTime = FPlatformTime::Seconds();
					RHICmdList.BlockUntilGPUIdle();
					TotalWaitTime += FPlatformTime::Seconds() - WaitStartTime;
					NumBlocked++;
				}

				int32 RowPitchInPixels;
				const void* RawData = Capture.Readback->Lock(RowPitchInPixels);

				if (RawData)
				{
					FLockedCaptureData LockedData;
					LockedData.RawData = RawData;
					LockedData.RowPitchInPixels = RowPitchInPixels;
					LockedData.OutputPath = Capture.OutputPath;
					LockedData.Width = Capture.Width;
					LockedData.Height = Capture.Height;
					LockedData.PixelFormat = Capture.PixelFormat;
					LockedCaptures.Add(LockedData);
				}
			}

			TotalLockTime = FPlatformTime::Seconds() - LockStartTime;

			double ConversionStartTime = FPlatformTime::Seconds();

			TArray<TArray<FColor>> AllPixelData;
			AllPixelData.SetNum(LockedCaptures.Num());

			for (int32 i = 0; i < AllPixelData.Num(); ++i)
			{
				AllPixelData[i].SetNumUninitialized(LockedCaptures[i].Width * LockedCaptures[i].Height);
			}

			ParallelFor(LockedCaptures.Num(), [&](int32 i)
			{
				const FLockedCaptureData& LockedData = LockedCaptures[i];

				// FReadSurfaceDataFlags ReadFlags(RCM_UNorm, CubeFace_MAX);
				FReadSurfaceDataFlags ReadFlags;
				ReadFlags.SetLinearToGamma(false);

				uint32 SrcPitch = LockedData.RowPitchInPixels * GPixelFormats[LockedData.PixelFormat].BlockBytes;

				ConvertRAWSurfaceDataToFColorOpt(
					LockedData.PixelFormat,
					LockedData.Width,
					LockedData.Height,
					(uint8*)LockedData.RawData,
					SrcPitch,
					AllPixelData[i].GetData(),
					ReadFlags
				);
				// // AllPixelData[i].GetData()
				// for (int32 Index = 0; Index < AllPixelData[i].Num(); ++Index)
				// {
				// 	AllPixelData[i][Index] = AllPixelData[i][Index].ReinterpretAsLinear();
				// }
			});

			TotalConversionTime = FPlatformTime::Seconds() - ConversionStartTime;

			for (auto& Capture : CapturesToFlush)
			{
				Capture.Readback->Unlock();
			}

			for (int32 i = 0; i < LockedCaptures.Num(); ++i)
			{
				FString OutputPath = LockedCaptures[i].OutputPath;
				int32 Width = LockedCaptures[i].Width;
				int32 Height = LockedCaptures[i].Height;

				AsyncTask(ENamedThreads::AnyThread,
					[PixelData = MoveTemp(AllPixelData[i]), OutputPath, Width, Height]()
				{
					double SerializeStartTime = FPlatformTime::Seconds();

					if (SerializeData(PixelData, Width, Height, OutputPath) == FExecStatusType::OK)
					{
						double SerializeTime = FPlatformTime::Seconds() - SerializeStartTime;
						SL::get().printf("[PERF] Saved %s in %.3f ms",
							TCHAR_TO_UTF8(*OutputPath), SerializeTime * 1000.0);
					}
					else
					{
						SL::get().printf("[ERROR] Failed to save %s", TCHAR_TO_UTF8(*OutputPath));
					}
				});
			}

			double RenderThreadTotalTime = FPlatformTime::Seconds() - RenderThreadStartTime;
			double GameThreadTotalTime = FPlatformTime::Seconds() - FlushStartTime;

			SL::get().printf("[PERF SUMMARY] FlushCapturesToDisk: %d captures", NumCaptures);
			SL::get().printf("[PERF] Total game thread time: %.3f ms (%.3f ms/frame)",
				GameThreadTotalTime * 1000.0, (GameThreadTotalTime * 1000.0) / NumCaptures);
			SL::get().printf("[PERF] Total render thread time: %.3f ms (%.3f ms/frame)",
				RenderThreadTotalTime * 1000.0, (RenderThreadTotalTime * 1000.0) / NumCaptures);
			SL::get().printf("[PERF] GPU wait time: %.3f ms (%.3f ms/frame) - %d frames blocked",
				TotalWaitTime * 1000.0, NumBlocked > 0 ? (TotalWaitTime * 1000.0) / NumBlocked : 0.0, NumBlocked);
			SL::get().printf("[PERF] Lock time (GPU->CPU DMA): %.3f ms (%.3f ms/frame)",
				TotalLockTime * 1000.0, (TotalLockTime * 1000.0) / NumCaptures);
			SL::get().printf("[PERF] Pixel conversion time (ParallelFor): %.3f ms (%.3f ms/frame)",
				TotalConversionTime * 1000.0, (TotalConversionTime * 1000.0) / NumCaptures);
			SL::get().printf("[PERF] Theoretical speedup: %.2fx (serial: %.3f ms -> parallel: %.3f ms)",
				(TotalLockTime + 7401.0) / (TotalLockTime + TotalConversionTime),
				7401.0, TotalConversionTime * 1000.0);
		}
	);
}