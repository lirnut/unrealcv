#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Components/PrimitiveComponent.h"
#include "RHI.h"
#include "SceneViewExtension.h"
#include "RHIResources.h"
#include "SceneView.h"
#include "SceneViewExtension.h"
#include "Sensor/CameraSensor/UnrealCVSurfaceReader.h"
#include "MovieQualityRenderComponent.generated.h"

class FUnrealCVImageWriteQueue;
class UFusionCamSensor;

// ViewExtension implementation to capture main view's PostProcessSettings
class FMovieQualityViewExtension : public FSceneViewExtensionBase
{
public:
	FMovieQualityViewExtension(const FAutoRegister& AutoRegister, UMovieQualityRenderComponent* InComponent)
		: FSceneViewExtensionBase(AutoRegister)
		, Component(InComponent)
	{
	}

	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;

	virtual int32 GetPriority() const override { return 100; }

	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}

private:
	TWeakObjectPtr<UMovieQualityRenderComponent> Component;
};


USTRUCT()
struct FMQRCSettings
{
	GENERATED_BODY()

	// UPROPERTY()
	// TEnumAsByte<EAntiAliasingMethod> AntiAliasingMethod = EAntiAliasingMethod::AAM_TSR;	
	TEnumAsByte<EAntiAliasingMethod> AntiAliasingMethod = EAntiAliasingMethod::AAM_TemporalAA;
	// TEnumAsByte<EAntiAliasingMethod> AntiAliasingMethod = EAntiAliasingMethod::AAM_FXAA;
	// TEnumAsByte<EAntiAliasingMethod> AntiAliasingMethod = EAntiAliasingMethod::AAM_MSAA;

	EPrimaryScreenPercentageMethod PrimaryScreenPercentageMethod = EPrimaryScreenPercentageMethod::TemporalUpscale;

	// UPROPERTY()
	EAutoExposureMethod ExposureMethod = EAutoExposureMethod::AEM_Histogram;

	UPROPERTY()
	float ExposureBias = 0.0f;

	UPROPERTY()
	float MotionBlurAmount = 0.0f;

	UPROPERTY()
	float LumenSceneLightingQuality = 3.0f;

	UPROPERTY()
	float LumenFinalGatherQuality = 5.0f;

	UPROPERTY()
	float LumenFinalGatherLightingUpdateSpeed = 4.0f;

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

	UPROPERTY()
	float ScreenPercentage = 0.85f;
};

UCLASS(ClassGroup = (UnrealCV), meta = (BlueprintSpawnableComponent))
class UNREALCV_API UMovieQualityRenderComponent : public USceneComponent
{
	GENERATED_BODY()
private:
	friend class FMovieQualityViewExtension;

public:
	UPROPERTY()
	bool bRenderEveryFrame = false;

	UPROPERTY()
	int32 NumWarmup = 0;

	UPROPERTY()
	bool bRenderImmediately = false;

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
	UPROPERTY(interp, Category = "PostProcess", meta = (ShowOnlyInnerProperties))
	FPostProcessSettings PostProcessSettings;

	UPROPERTY(interp, Category = "PostProcess", BlueprintReadWrite, meta = (UIMin = "0.0", UIMax = "1.0"))
	float PostProcessBlendWeight = 1.0f;

	void CaptureDiscardFrame();

	TSharedPtr<ISceneViewExtension, ESPMode::ThreadSafe> ViewExtension;
	FFinalPostProcessSettings CachedMainViewPostProcessSettings;
	bool bHasCachedMainViewPostProcessSettings = false;

	uint32 LastMainViewportFrameNumber = 0;


	void ProcessDeferredCaptures();

protected:
	// Deferred capture system
	struct FDeferredCaptureRequest
	{
		TFunction<void(TUniquePtr<FImagePixelData>&&)> OnPixelDataReady;
		double EnqueueTime;
		uint32 FrameNumber;
		bool bIsDiscardFrame;
	};

	TQueue<FDeferredCaptureRequest, EQueueMode::Spsc> DeferredCaptureQueue;
	FCriticalSection QueueLock;
	bool bFirstDeferredCapture = true;

	void EnqueueDeferredCapture(TFunction<void(TUniquePtr<FImagePixelData>&&)> Callback, bool bIsDiscardFrame);
	void ResetAllTemporalState();
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

protected:
	bool bIsInitialized;
	EPixelFormat PixelFormat;
	bool bForceLinearGamma;
	float ForceTargetGamma;
	uint32 FrameCounter;

	TSharedPtr<FUnrealCVSurfaceQueue, ESPMode::ThreadSafe> SurfaceQueue;
	FSceneViewStateReference ViewState;
	TSharedPtr<FUnrealCVImageWriteQueue> ImageWriteQueue;

	UPROPERTY()
	TMap<FString, UTextureRenderTarget2D*> RenderTargetPool;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};
