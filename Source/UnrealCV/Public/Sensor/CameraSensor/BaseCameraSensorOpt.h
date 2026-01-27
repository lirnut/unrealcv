#pragma once

#include "CoreMinimal.h"
#include "Runtime/Engine/Classes/Components/SceneCaptureComponent2D.h"
#include "Runtime/Engine/Classes/Engine/TextureRenderTarget2D.h"
#include "Materials/Material.h"
#include "Runtime/CoreUObject/Public/UObject/ConstructorHelpers.h"
#include "Sensor/RenderUtil/ImageReadback.h"
#include "BaseCameraSensorOpt.generated.h"

UCLASS(abstract)
class UNREALCV_API UBaseCameraSensorOpt : public USceneCaptureComponent2D
{
	GENERATED_BODY()

public:
	UBaseCameraSensorOpt(const FObjectInitializer& ObjectInitializer);

	FVector GetSensorLocation() { return this->GetComponentLocation(); }
	void SetSensorLocation(FVector Location) { this->SetWorldLocation(Location); }

	FRotator GetSensorRotation() { return this->GetComponentRotation(); }
	void SetSensorRotation(FRotator Rotator) { this->SetWorldRotation(Rotator); }

	float GetFOV() { return this->FOVAngle; }
	void SetFOV(float FOV) { this->FOVAngle = FOV; }

	void SetFilmSize(int Width, int Height);
	int GetFilmWidth();
	int GetFilmHeight();

	virtual void InitTextureTarget(int FilmWidth, int FilmHeight);

	void SetPostProcessMaterial(TScriptInterface<IBlendableInterface> PostProcessMaterial);

	void GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView);

	bool CheckTextureTarget();

	void ReadCaptureResults(TArray<FColor>& Data);

	void SetShowOnlyList(const TArray<TWeakObjectPtr<UPrimitiveComponent>>& InShowOnlyComponents);

	void HideActor(AActor* Actor);
	void ShowActor(AActor* Actor);

	void SetUsePipelinedCapture(bool bInUsePipelined) { bUsePipelinedCapture = bInUsePipelined; }
	bool GetUsePipelinedCapture() const { return bUsePipelinedCapture; }

public:
	void Capture(TArray<FColor>& ImageData, int& Width, int& Height);
	void Capture(TArray<FFloat16Color>& ImageData, int& Width, int& Height);

protected:
	void CaptureFastToFile(const FString& Filename);

	void CaptureFast(TArray<FColor>& ImageData, int& Width, int& Height);
	void CaptureFast(TArray<FFloat16Color>& ImageData, int& Width, int& Height);

	void LaunchPipelinedCapture();
	bool ConsumePipelinedCapture(TArray<FColor>& OutPixels);

protected:
	void InitFloat16TextureTarget(int FilmWidth, int FilmHeight);
	void InitUInt8TextureTarget(int FilmWidth, int FilmHeight, bool bUseLinearGamma = true);

protected:
	int FilmWidth;
	int FilmHeight;

	bool bUsePipelinedCapture;
	UnrealCV::RenderUtil::FReadbackContext PipelinedContext;
};
