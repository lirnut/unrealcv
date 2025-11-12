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
#include "Sensor/AsyncCaptureHelper.h"

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

	PendingCaptureRequestID = -1;
	bUseAsyncCapture = true;
	// Avoid calling virtual function in a constructor
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
	}

	if (TextureTarget->SizeX != Width || TextureTarget->SizeY != Height)
	{
		InitTextureTarget(Width, Height);
		ShutdownAsyncCapture();
		InitializeAsyncCapture();
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

	if (bUseAsyncCapture && AsyncCapturePool.IsValid())
	{
		if (PendingCaptureRequestID != -1)
		{
			FAsyncCaptureFrame Frame;
			if (AsyncCapturePool->GetCapturedFrame(PendingCaptureRequestID, Frame))
			{
				ImageData = MoveTemp(Frame.Data);
				Width = Frame.Width;
				Height = Frame.Height;
			}
			else
			{
				UE_LOG(LogUnrealCV, Warning, TEXT("Previous frame not ready yet, using sync readback"));
				ReadTextureRenderTarget(TextureTarget, ImageData, Width, Height);
			}
			PendingCaptureRequestID = -1;
		}
		else
		{
			ReadTextureRenderTarget(TextureTarget, ImageData, Width, Height);
		}

		auto RenderTargetResource = TextureTarget->GameThread_GetRenderTargetResource();
		if (RenderTargetResource)
		{
			PendingCaptureRequestID = AsyncCapturePool->RequestCapture(
				RenderTargetResource->GetRenderTargetTexture()
			);
		}
	}
	else
	{
		ReadTextureRenderTarget(TextureTarget, ImageData, Width, Height);
	}
}

void UBaseCameraSensor::CaptureFloat16(TArray<FFloat16Color>& ImageData, int& Width, int& Height)
{
	if (!CheckTextureTarget())
	{
		UE_LOG(LogTemp, Error, TEXT("The TextureTarget was not initialized. CaptureFloat16 failed."));
		return;
	}

	this->CaptureScene();

	if (bUseAsyncCapture && AsyncCapturePool.IsValid())
	{
		if (PendingCaptureRequestID != -1)
		{
			FAsyncCaptureFrame Frame;
			if (AsyncCapturePool->GetCapturedFrame(PendingCaptureRequestID, Frame))
			{
				if (Frame.bIsFloat16)
				{
					ImageData = MoveTemp(Frame.Float16Data);
					Width = Frame.Width;
					Height = Frame.Height;
				}
				else
				{
					UE_LOG(LogUnrealCV, Warning, TEXT("Previous frame was not Float16, using sync readback"));
					FTextureRenderTargetResource* RenderTargetResource = TextureTarget->GameThread_GetRenderTargetResource();
					RenderTargetResource->ReadFloat16Pixels(ImageData);
					Width = TextureTarget->SizeX;
					Height = TextureTarget->SizeY;
				}
			}
			else
			{
				UE_LOG(LogUnrealCV, Warning, TEXT("Previous Float16 frame not ready yet, using sync readback"));
				FTextureRenderTargetResource* RenderTargetResource = TextureTarget->GameThread_GetRenderTargetResource();
				RenderTargetResource->ReadFloat16Pixels(ImageData);
				Width = TextureTarget->SizeX;
				Height = TextureTarget->SizeY;
			}
			PendingCaptureRequestID = -1;
		}
		else
		{
			FTextureRenderTargetResource* RenderTargetResource = TextureTarget->GameThread_GetRenderTargetResource();
			RenderTargetResource->ReadFloat16Pixels(ImageData);
			Width = TextureTarget->SizeX;
			Height = TextureTarget->SizeY;
		}

		auto RenderTargetResource = TextureTarget->GameThread_GetRenderTargetResource();
		if (RenderTargetResource)
		{
			PendingCaptureRequestID = AsyncCapturePool->RequestCaptureFloat16(
				RenderTargetResource->GetRenderTargetTexture()
			);
		}
	}
	else
	{
		FTextureRenderTargetResource* RenderTargetResource = TextureTarget->GameThread_GetRenderTargetResource();
		RenderTargetResource->ReadFloat16Pixels(ImageData);
		Width = TextureTarget->SizeX;
		Height = TextureTarget->SizeY;
	}
}

void UBaseCameraSensor::InitializeAsyncCapture()
{
	if (!bUseAsyncCapture || !IsValid(TextureTarget))
	{
		return;
	}

	if (AsyncCapturePool.IsValid())
	{
		return;
	}

	AsyncCapturePool = MakeShared<FAsyncCapturePool>(
		EPixelFormat::PF_B8G8R8A8,
		FIntPoint(FilmWidth, FilmHeight)
	);
	AsyncCapturePool->Initialize();

	UE_LOG(LogUnrealCV, Log, TEXT("Async capture initialized for %dx%d"), FilmWidth, FilmHeight);
}

void UBaseCameraSensor::ShutdownAsyncCapture()
{
	if (AsyncCapturePool.IsValid())
	{
		AsyncCapturePool->Shutdown();
		AsyncCapturePool.Reset();
		PendingCaptureRequestID = -1;
	}
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