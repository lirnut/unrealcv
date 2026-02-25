#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Components/PrimitiveComponent.h"
#include "RHI.h"
#include "RHIResources.h"
#include "SceneView.h"
#include "SceneViewExtension.h"
#include "MovieRenderPipelineDataTypes.h"
#include "MovieQualityRenderComponent.generated.h"

struct FMoviePipelineSurfaceQueue;
class FUnrealCVImageWriteQueue;
class UFusionCamSensor;

USTRUCT()
struct FMQRCSettings
{
	GENERATED_BODY()

	UPROPERTY()
	bool bRenderImmediately = true;

	// UPROPERTY()
	TEnumAsByte<EAntiAliasingMethod> AntiAliasingMethod = EAntiAliasingMethod::AAM_TemporalAA;

	// UPROPERTY()
	EAutoExposureMethod ExposureMethod = EAutoExposureMethod::AEM_Histogram;

	UPROPERTY()
	float ExposureBias = 0.0f;

	UPROPERTY()
	float MotionBlurAmount = 0.0f;

	UPROPERTY()
	float LumenSceneLightingQuality = 2.0f;

	UPROPERTY()
	float LumenFinalGatherQuality = 4.0f;

	UPROPERTY()
	float Saturation = 1.0f;

	UPROPERTY()
	float Contrast = 0.80f;

	UPROPERTY()
	float Gamma = 1.0f;

	UPROPERTY()
	float Gain = 1.0f;

	UPROPERTY()
	float AutoExposureMinBrightness = -10.0f;

	UPROPERTY()
	float AutoExposureMaxBrightness = 20.0f;

	UPROPERTY()
	float DepthOfFieldScale = 0.0f;
};

UCLASS(ClassGroup = (UnrealCV), meta = (BlueprintSpawnableComponent))
class UNREALCV_API UMovieQualityRenderComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	static FMQRCSettings GlobalSettings;

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

	void Render();

	void CaptureFrame(TFunction<void(TUniquePtr<FImagePixelData>&&)> OnPixelDataReady);

	void CaptureFrameToFile(const FString& OutputPath, TFunction<void(bool)> OnComplete = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Movie Quality Render")
	TEnumAsByte<ESceneCaptureSource> GetCaptureSource() { return CaptureSource; }

	UFUNCTION(BlueprintCallable, Category = "Movie Quality Render")
	void SetFOV(float InFOV) { FOV = InFOV; }

	UFUNCTION(BlueprintPure, Category = "Movie Quality Render")
	float GetFOV() const { return FOV; }

	bool IsInitialized() const { return bIsInitialized; }

	void SetShowOnlyComponents(const TArray<TWeakObjectPtr<UPrimitiveComponent>>& InComponents);

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lumen")
	bool bRenderEveryFrame = true;

	UPROPERTY(interp, Category = "PostProcess", meta = (ShowOnlyInnerProperties))
	FPostProcessSettings PostProcessSettings;

	UPROPERTY(interp, Category = "PostProcess", BlueprintReadWrite, meta = (UIMin = "0.0", UIMax = "1.0"))
	float PostProcessBlendWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcess")
	bool bInheritMainViewPostProcessSettings = true;

	void CaptureDiscardFrame();

	TSharedPtr<ISceneViewExtension, ESPMode::ThreadSafe> ViewExtension;
	FFinalPostProcessSettings CachedMainViewPostProcessSettings;
	bool bHasCachedMainViewPostProcessSettings = false;


protected:
	UPROPERTY()
	TEnumAsByte<ESceneCaptureSource> CaptureSource;

	UPROPERTY()
	FIntPoint Resolution;

	UPROPERTY()
	float FOV;

	TArray<TWeakObjectPtr<UPrimitiveComponent>> ShowOnlyComponents;

protected:
	TSharedPtr<FSceneViewFamilyContext> CreateViewFamily(UTextureRenderTarget2D* RenderTarget);
	virtual FSceneView* CreateSceneView(FSceneViewFamily* ViewFamily);

	void SubmitToRendererWithCallback(
		FSceneViewFamily* ViewFamily,
		UTextureRenderTarget2D* RenderTarget,
		TFunction<void(TUniquePtr<FImagePixelData>&&)> OnPixelDataReady
	);

	// float GetTargetGamma() const;

	virtual void SetPostProcessSettings(FPostProcessSettings& PPSettings);
	
	void SetDefaultPostProcessSettings(FPostProcessSettings& PPSettings);
	void ExecuteCaptureFrame(TFunction<void(TUniquePtr<FImagePixelData>&&)> OnPixelDataReady);

	TOptional<TFunction<void(TUniquePtr<FImagePixelData>&&)>> PendingCaptureCallback;

protected:
	bool bIsInitialized;
	EPixelFormat PixelFormat;
	bool bForceLinearGamma;
	float ForceTargetGamma;
	uint32 FrameCounter;

	TSharedPtr<FMoviePipelineSurfaceQueue, ESPMode::ThreadSafe> SurfaceQueue;
	FSceneViewStateReference ViewState;
	TSharedPtr<FUnrealCVImageWriteQueue> ImageWriteQueue;

	UPROPERTY()
	TMap<FString, UTextureRenderTarget2D*> RenderTargetPool;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};
