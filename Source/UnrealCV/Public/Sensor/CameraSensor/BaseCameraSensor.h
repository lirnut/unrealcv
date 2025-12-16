// Weichao Qiu @ 2017
// Should not be used in blueprint

// shc @2025
// Performance improvement for Capture
// Not thread safe

#pragma once

#include "CoreMinimal.h"
#include "Runtime/Engine/Classes/Components/SceneCaptureComponent2D.h"
#include "Runtime/Engine/Classes/Engine/TextureRenderTarget2D.h"
#include "Materials/Material.h"
#include "Runtime/CoreUObject/Public/UObject/ConstructorHelpers.h"
#include "RHIGPUReadback.h"

#include "BaseCameraSensor.generated.h"

enum class ECaptureFormat : uint8
{
	Invalid = 0,
	F16 = 1,
	UInt8 = 2,
};

/**
 * A base camera sensor for ground truth capture
 */
UCLASS(abstract)
class UNREALCV_API UBaseCameraSensor : public USceneCaptureComponent2D
{
	GENERATED_BODY()

public:
	UBaseCameraSensor(const FObjectInitializer& ObjectInitializer);

	/** Get/set the sensor location / rotation */
	FVector GetSensorLocation()
	{
		return this->GetComponentLocation(); // World space
	}
	void SetSensorLocation(FVector Location)
	{
		this->SetWorldLocation(Location);
	}

	FRotator GetSensorRotation()
	{
		return this->GetComponentRotation(); // World space
	}
	void SetSensorRotation(FRotator Rotator)
	{
		this->SetWorldRotation(Rotator);
	}

	/** Get/set the FOV of this camera */
	float GetFOV() { return this->FOVAngle; }
	void SetFOV(float FOV) { this->FOVAngle = FOV; }

	/** Get/set the sensor film size */
	void SetFilmSize(int Width, int Height);
	int GetFilmWidth();
	int GetFilmHeight();

	// /** Get the projection matrix of this camera */
	// FString GetProjectionMatrix();

	virtual void InitTextureTarget(int FilmWidth, int FilmHeight);

	void SetPostProcessMaterial(UMaterial* PostProcessMaterial);

	/** Similar function to GetCameraView in UCameraComponent, without lockToHMD feature */
	void GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView);
	
	/** Check whether the TextureTarget is correctly initialized */
	bool CheckTextureTarget();

	void ReadCaptureResults(TArray<FColor>& Data);

	// void InitializeAsyncCapture();
	// void ShutdownAsyncCapture();

	void SetUseFastCapture(bool bInUseFast) { bUseFastCapture = bInUseFast; }
	bool GetUseFastCapture() const { return bUseFastCapture; }
	void CleanCaptureCache();
	
public:
	/** The old version to read TextureBuffer, slow but is sync operation */
	void Capture(TArray<FColor>& ImageData, int& Width, int& Height);
	void Capture(TArray<FFloat16Color>& ImageData, int& Width, int& Height);

protected:
	// // 	FlushRenderingCommands() can make sure the rendering command is finished, but will slow down the game thread
	// void CaptureFast(TArray<FColor>& ImageData, int& Width, int& Height);

	/** Save lit to an image file, send the capture command to rendering thread */
	void CaptureFastToFile(const FString& Filename);

	/** Fast async ver. **/
	void CaptureFast(TArray<FColor>& ImageData, int& Width, int& Height);

	void CaptureFast(TArray<FFloat16Color>& ImageData, int& Width, int& Height);

	// void CaptureFloat16(TArray<FFloat16Color>& ImageData, int& Width, int& Height);


protected:
	// virtual void CaptureToGPUQueue(const FString& Filename);
	// void FlushCapturesToDisk();

	void CheckCaptureCache(ECaptureFormat Format);
	virtual void LaunchCapture();
	virtual void CopyBackCapture(ECaptureFormat Format);
	// virtual void ConvertCapture(TArray<FColor>& OutPixelData, int32& OutWidth, int32& OutHeight);


	void InitFloat16TextureTarget(int FilmWidth, int FilmHeight);
	void InitUInt8TextureTarget(int FilmWidth, int FilmHeight, bool bUseLinearGamma = true);
	

protected:
	int FilmWidth;

	int FilmHeight;

	// TSharedPtr<class FAsyncCapturePool> AsyncCapturePool;
	// int32 PendingCaptureRequestID;
	bool bUseFastCapture;

	struct FQueuedCapture
	{
		// TUniquePtr<FRHIGPUTextureReadback> Readback;
		TSharedPtr<FRHIGPUTextureReadback> Readback;
		FString OutputPath;
		int32 Width;
		int32 Height;
		EPixelFormat PixelFormat;
	};

	bool bCaptureLaunched = false;
	double CaptureTimestamp = 0.0;

	ECaptureFormat CopyFormat = ECaptureFormat::Invalid;

	// FQueuedCapture CaptureCache;
	bool bCaptureCacheValid = false;
	TArray<FColor> CaptureCache;
	TArray<FFloat16Color> CaptureCacheFloat16;

	// TArray<FQueuedCapture> QueuedCaptures;
};
