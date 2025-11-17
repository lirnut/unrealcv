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
#include "RHISurfaceDataConversion.h"

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

            AsyncTask(ENamedThreads::GameThread, [PixelData = MoveTemp(PixelData), Width, Height, OutputPath]()
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
		[RenderTargetResource, ReadbackPtr = NewCapture.Readback.Get()](FRHICommandListImmediate& RHICmdList)
		{
			RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
			ReadbackPtr->EnqueueCopy(RHICmdList, RenderTargetResource->GetRenderTargetTexture());
		}
	);

	QueuedCaptures.Add(MoveTemp(NewCapture));

	UE_LOG(LogUnrealCV, Verbose, TEXT("CaptureToGPUQueue: Enqueued %s, total=%d"),
		*Filename, QueuedCaptures.Num());
}

void UBaseCameraSensor::FlushCapturesToDisk()
{
	if (QueuedCaptures.Num() == 0)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("FlushCapturesToDisk: No captures queued"));
		return;
	}

	int32 NumCaptures = QueuedCaptures.Num();
	UE_LOG(LogUnrealCV, Log, TEXT("FlushCapturesToDisk: Processing %d captures"), NumCaptures);

	TArray<FQueuedCapture> CapturesToFlush = MoveTemp(QueuedCaptures);
	QueuedCaptures.Empty();

	ENQUEUE_RENDER_COMMAND(FlushQueuedCaptures)(
		[CapturesToFlush = MoveTemp(CapturesToFlush)](FRHICommandListImmediate& RHICmdList) mutable
		{

			for (auto& Capture : CapturesToFlush)
			{
				if (!Capture.Readback->IsReady())
				{
					RHICmdList.BlockUntilGPUIdle();
				}
				TArray<FColor> PixelData;
				PixelData.SetNumUninitialized(Capture.Width * Capture.Height);

				int32 RowPitchInPixels;
				const void* RawData = Capture.Readback->Lock(RowPitchInPixels);

				if (RawData)
				{
					FReadSurfaceDataFlags ReadFlags(RCM_UNorm, CubeFace_MAX);
					ReadFlags.SetLinearToGamma(false);

					uint32 SrcPitch = RowPitchInPixels * GPixelFormats[Capture.PixelFormat].BlockBytes;

					bool bConversionSuccess = ConvertRAWSurfaceDataToFColor(
						Capture.PixelFormat,
						Capture.Width,
						Capture.Height,
						(uint8*)RawData,
						SrcPitch,
						PixelData.GetData(),
						ReadFlags
					);

					Capture.Readback->Unlock();

					FString OutputPath = Capture.OutputPath;
					int32 Width = Capture.Width;
					int32 Height = Capture.Height;
					int32 RowPitch = RowPitchInPixels;
					FColor FirstPixel = PixelData.Num() > 0 ? PixelData[0] : FColor(0, 0, 0, 0);
					EPixelFormat PixelFormat = Capture.PixelFormat;

					AsyncTask(ENamedThreads::GameThread,
						[PixelData = MoveTemp(PixelData), OutputPath, Width, Height, RowPitch, FirstPixel, PixelFormat, bConversionSuccess]()
					{
						// UE_LOG(LogTemp, Warning, TEXT("[DEBUG] RowPitch=%d, Width=%d, Height=%d, Match=%d"),
						// 	RowPitch, Width, Height, RowPitch == Width);
						// UE_LOG(LogTemp, Warning, TEXT("[DEBUG] PixelFormat=%d, ConversionSuccess=%d"),
						// 	(int32)PixelFormat, bConversionSuccess);
						// UE_LOG(LogTemp, Warning, TEXT("[DEBUG] First pixel: R=%d G=%d B=%d A=%d"),
						// 	FirstPixel.R, FirstPixel.G, FirstPixel.B, FirstPixel.A);

						if (SerializeData(PixelData, Width, Height, OutputPath) == FExecStatusType::OK)
						{
							UE_LOG(LogUnrealCV, Verbose, TEXT("FlushCapturesToDisk: Saved %s"), *OutputPath);
						}
						else
						{
							UE_LOG(LogTemp, Error, TEXT("FlushCapturesToDisk: Failed to save %s"), *OutputPath);
						}
					});
				}
			}
		}
	);
}