#include "PanoramicCamSensor.h"
#include "UnrealcvServer.h"
#include "UnrealcvLog.h"
#include "ImageUtil.h"
#include "Engine/TextureRenderTargetCube.h"
#include "TextureResource.h"
#include "RenderingThread.h"
#include "RHICommandList.h"
#include "RenderTargetPool.h"

UPanoramicCamSensor::UPanoramicCamSensor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	this->ShowFlags.SetPostProcessing(true);
	bCaptureEveryFrame = false;
	bCaptureOnMovement = false;
	CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	bUseRayTracingIfEnabled = true;
	bAlwaysPersistRenderingState = true;

	this->ShowFlags.SetAntiAliasing(true);
	this->ShowFlags.SetTemporalAA(false);

	FServerConfig& Config = FUnrealcvServer::Get().Config;
	CubemapResolution = Config.Width == 0 ? 1024 : Config.Width;
}

void UPanoramicCamSensor::SetCubemapResolution(int32 Resolution)
{
	CubemapResolution = Resolution;
	if (TextureTarget)
	{
		InitCubemapTarget(Resolution);
	}
}

void UPanoramicCamSensor::InitCubemapTarget(int32 Resolution)
{
	if (!IsValid(TextureTarget))
	{
		TextureTarget = NewObject<UTextureRenderTargetCube>(this);
	}

	UTextureRenderTargetCube* CubeTarget = Cast<UTextureRenderTargetCube>(TextureTarget);
	if (CubeTarget && (CubeTarget->SizeX != Resolution))
	{
		CubeTarget->InitAutoFormat(Resolution);
		CubeTarget->ClearColor = FLinearColor::Black;
		CubeTarget->bAutoGenerateMips = false;
		CubeTarget->UpdateResourceImmediate(true);
	}
	CubemapResolution = Resolution;
}

bool UPanoramicCamSensor::CheckCubemapTarget()
{
	if (!IsValid(TextureTarget))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("PanoramicCamSensor: TextureTarget not initialized"));
		return false;
	}

	UTextureRenderTargetCube* CubeTarget = Cast<UTextureRenderTargetCube>(TextureTarget);
	if (!CubeTarget)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("PanoramicCamSensor: TextureTarget is not a cube"));
		return false;
	}

	if (CubeTarget->SizeX == 0)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("PanoramicCamSensor: TextureTarget has invalid size"));
		return false;
	}

	return true;
}

void UPanoramicCamSensor::ReadCubemapFaces(TArray<FColor>* OutFaces, int32& OutFaceSize)
{
	if (!CheckCubemapTarget())
	{
		return;
	}

	UTextureRenderTargetCube* CubeTarget = Cast<UTextureRenderTargetCube>(TextureTarget);
	OutFaceSize = CubeTarget->SizeX;

	FTextureRenderTargetCubeResource* CubeResource = static_cast<FTextureRenderTargetCubeResource*>(
		CubeTarget->GameThread_GetRenderTargetResource()
	);

	FReadSurfaceDataFlags ReadFlags(RCM_UNorm, CubeFace_MAX);
	ReadFlags.SetLinearToGamma(false);

	for (int32 FaceIndex = 0; FaceIndex < 6; ++FaceIndex)
	{
		OutFaces[FaceIndex].Empty();
		OutFaces[FaceIndex].SetNumUninitialized(OutFaceSize * OutFaceSize);

		ReadFlags.SetCubeFace((ECubeFace)FaceIndex);

		FRenderTarget* RenderTarget = CubeResource;
		RenderTarget->ReadPixels(OutFaces[FaceIndex], ReadFlags);

		if (OutFaces[FaceIndex].Num() == 0)
		{
			UE_LOG(LogUnrealCV, Warning, TEXT("PanoramicCamSensor: Failed to read cube face %d"), FaceIndex);
		}
	}
}

FColor UPanoramicCamSensor::SampleCubemapDirection(
	const TArray<FColor>* CubeFaces,
	int32 FaceSize,
	const FVector& Direction)
{
	FVector Dir = Direction.GetSafeNormal();
	int32 FaceIndex;
	float u, v;

	FVector AbsDir = Dir.GetAbs();

	if (AbsDir.X >= AbsDir.Y && AbsDir.X >= AbsDir.Z)
	{
		if (Dir.X > 0)
		{
			FaceIndex = 0;
			u = -Dir.Z / Dir.X;
			v = -Dir.Y / Dir.X;
		}
		else
		{
			FaceIndex = 1;
			u = Dir.Z / -Dir.X;
			v = -Dir.Y / -Dir.X;
		}
	}
	else if (AbsDir.Y >= AbsDir.Z)
	{
		if (Dir.Y > 0)
		{
			FaceIndex = 2;
			u = Dir.X / Dir.Y;
			v = Dir.Z / Dir.Y;
		}
		else
		{
			FaceIndex = 3;
			u = Dir.X / -Dir.Y;
			v = -Dir.Z / -Dir.Y;
		}
	}
	else
	{
		if (Dir.Z > 0)
		{
			FaceIndex = 4;
			u = Dir.X / Dir.Z;
			v = -Dir.Y / Dir.Z;
		}
		else
		{
			FaceIndex = 5;
			u = -Dir.X / -Dir.Z;
			v = -Dir.Y / -Dir.Z;
		}
	}

	u = (u + 1.0f) * 0.5f;
	v = (v + 1.0f) * 0.5f;

	int32 px = FMath::Clamp((int32)(u * FaceSize), 0, FaceSize - 1);
	int32 py = FMath::Clamp((int32)(v * FaceSize), 0, FaceSize - 1);

	return CubeFaces[FaceIndex][py * FaceSize + px];
}

void UPanoramicCamSensor::ConvertCubemapToEquirectangular(
	const TArray<FColor>* CubeFaces,
	int32 FaceSize,
	TArray<FColor>& OutEquirect,
	int32 EquirectWidth,
	int32 EquirectHeight)
{
	OutEquirect.SetNum(EquirectWidth * EquirectHeight);

	for (int32 y = 0; y < EquirectHeight; ++y)
	{
		for (int32 x = 0; x < EquirectWidth; ++x)
		{
			float u = (float)x / EquirectWidth;
			float v = (float)y / EquirectHeight;

			float Theta = u * 2.0f * PI;
			float Phi = v * PI;

			FVector Dir;
			Dir.X = FMath::Sin(Phi) * FMath::Cos(Theta);
			Dir.Y = FMath::Sin(Phi) * FMath::Sin(Theta);
			Dir.Z = FMath::Cos(Phi);

			FColor SampledColor = SampleCubemapDirection(CubeFaces, FaceSize, Dir);
			OutEquirect[y * EquirectWidth + x] = SampledColor;
		}
	}
}

void UPanoramicCamSensor::CaptureEquirectangular(
	TArray<FColor>& OutPixelData,
	int32& Width,
	int32& Height,
	int32 EquirectWidth,
	int32 EquirectHeight)
{
	if (!CheckCubemapTarget())
	{
		InitCubemapTarget(CubemapResolution);
	}

	this->CaptureScene();

	TArray<FColor> CubeFaces[6];
	int32 FaceSize;
	ReadCubemapFaces(CubeFaces, FaceSize);

	ConvertCubemapToEquirectangular(CubeFaces, FaceSize, OutPixelData, EquirectWidth, EquirectHeight);

	Width = EquirectWidth;
	Height = EquirectHeight;

	UE_LOG(LogUnrealCV, Log, TEXT("PanoramicCamSensor: Captured equirectangular %dx%d from cubemap %dx%d"),
		Width, Height, FaceSize, FaceSize);
}

void UPanoramicCamSensor::CaptureEquirectangularToFile(
	const FString& OutputPath,
	int32 EquirectWidth,
	int32 EquirectHeight)
{
	TArray<FColor> PixelData;
	int32 Width, Height;

	CaptureEquirectangular(PixelData, Width, Height, EquirectWidth, EquirectHeight);

	if (PixelData.Num() == Width * Height && Width > 0 && Height > 0)
	{
		AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask,
			[PixelData = MoveTemp(PixelData), OutputPath, Width, Height]()
			{
				double StartTime = FPlatformTime::Seconds();
				SerializeData(PixelData, Width, Height, OutputPath);
				double ElapsedTime = FPlatformTime::Seconds() - StartTime;
				UE_LOG(LogUnrealCV, Log, TEXT("PanoramicCamSensor: Saved panoramic to %s in %.3f ms"),
					*OutputPath, ElapsedTime * 1000.0);
			}
		);
	}
	else
	{
		UE_LOG(LogUnrealCV, Error, TEXT("PanoramicCamSensor: Failed to capture equirectangular data"));
	}
}
