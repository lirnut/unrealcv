#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "RHI.h"
#include "RHIResources.h"
#include "SceneView.h"
#include "MovieRenderPipelineDataTypes.h"
#include "MovieQualityRenderComponent.generated.h"

struct FMoviePipelineSurfaceQueue;
class FUnrealCVImageWriteQueue;
class UFusionCamSensor;

UCLASS(ClassGroup = (UnrealCV), meta = (BlueprintSpawnableComponent))
class UNREALCV_API UMovieQualityRenderComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TEnumAsByte<EAntiAliasingMethod> AntiAliasingMethod;

public:
	UMovieQualityRenderComponent();
	virtual ~UMovieQualityRenderComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	FEngineShowFlags ShowFlags;

	UFUNCTION(BlueprintCallable, Category = "Movie Quality Render")
	void Initialize(int32 ResolutionX, int32 ResolutionY);

	UFUNCTION(BlueprintCallable, Category = "Movie Quality Render")
	FIntPoint GetResolution() { return Resolution; };

	UFUNCTION(BlueprintCallable, Category = "Movie Quality Render")
	void Shutdown();

	void FlushPendingFrames();

	void CaptureFrame(TFunction<void(TUniquePtr<FImagePixelData>&&)> OnPixelDataReady);

	void CaptureFrameToFile(const FString& OutputPath, TFunction<void(bool)> OnComplete = nullptr);

	void SaveLitToFile(const FString& OutputPath, TFunction<void(bool)> OnComplete = nullptr)
	{
		CaptureFrameToFile(OutputPath, OnComplete);
	}

	UFUNCTION(BlueprintCallable, Category = "Movie Quality Render")
	TEnumAsByte<ESceneCaptureSource> GetCaptureSource() { return CaptureSource; }

	UFUNCTION(BlueprintCallable, Category = "Movie Quality Render")
	void SetFOV(float InFOV) { FOV = InFOV; }

	UFUNCTION(BlueprintPure, Category = "Movie Quality Render")
	float GetFOV() const { return FOV; }

	bool IsInitialized() const { return bIsInitialized; }

protected:
	UPROPERTY()
	TEnumAsByte<ESceneCaptureSource> CaptureSource;

	UPROPERTY()
	FIntPoint Resolution;

	UPROPERTY()
	float FOV;

protected:
	TSharedPtr<FSceneViewFamilyContext> CreateViewFamily(UTextureRenderTarget2D* RenderTarget);
	FSceneView* CreateSceneView(FSceneViewFamily* ViewFamily);

	void SubmitToRendererWithCallback(
		FSceneViewFamily* ViewFamily,
		UTextureRenderTarget2D* RenderTarget,
		TFunction<void(TUniquePtr<FImagePixelData>&&)> OnPixelDataReady
	);

	// float GetTargetGamma() const;

	virtual void SetPostProcessSettings(FPostProcessSettings& PPSettings);

private:
	void SetDefaultPostProcessSettings(FPostProcessSettings& PPSettings);

protected:
	bool bIsInitialized;
	EPixelFormat PixelFormat;
	bool bForceLinearGamma;
	float ForceTargetGamma;

	TSharedPtr<FMoviePipelineSurfaceQueue, ESPMode::ThreadSafe> SurfaceQueue;
	FSceneViewStateReference ViewState;
	TSharedPtr<FUnrealCVImageWriteQueue> ImageWriteQueue;

	UPROPERTY()
	TMap<FString, UTextureRenderTarget2D*> RenderTargetPool;
};
