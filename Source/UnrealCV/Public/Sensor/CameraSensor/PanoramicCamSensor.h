#pragma once

#include "CoreMinimal.h"
#include "Runtime/Engine/Classes/Components/SceneCaptureComponentCube.h"
#include "Runtime/Engine/Classes/Engine/TextureRenderTargetCube.h"
#include "PanoramicCamSensor.generated.h"

UCLASS()
class UNREALCV_API UPanoramicCamSensor : public USceneCaptureComponentCube
{
	GENERATED_BODY()

public:
	UPanoramicCamSensor(const FObjectInitializer& ObjectInitializer);

	void SetCubemapResolution(int32 Resolution);
	int32 GetCubemapResolution() const { return CubemapResolution; }

	void CaptureEquirectangular(TArray<FColor>& OutPixelData, int32& Width, int32& Height, int32 EquirectWidth = 4096, int32 EquirectHeight = 2048);

	void CaptureEquirectangularToFile(const FString& OutputPath, int32 EquirectWidth = 4096, int32 EquirectHeight = 2048);

	void InitCubemapTarget(int32 Resolution);

	bool CheckCubemapTarget();

protected:
	void ConvertCubemapToEquirectangular(
		const TArray<FColor>* CubeFaces,
		int32 FaceSize,
		TArray<FColor>& OutEquirect,
		int32 EquirectWidth,
		int32 EquirectHeight
	);

	FColor SampleCubemapDirection(
		const TArray<FColor>* CubeFaces,
		int32 FaceSize,
		const FVector& Direction
	);

	void ReadCubemapFaces(TArray<FColor>* OutFaces, int32& OutFaceSize);

protected:
	int32 CubemapResolution;
};
