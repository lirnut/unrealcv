#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "RHI.h"
#include "RHIResources.h"
#include "SceneView.h"
#include "MovieRenderPipelineDataTypes.h"
#include "MovieQualityRenderComponent.generated.h"

struct FMoviePipelineSurfaceQueue;
class IImageWriteQueue;
class UFusionCamSensor;

UCLASS(ClassGroup = (UnrealCV), meta = (BlueprintSpawnableComponent))
class UNREALCV_API UMovieQualityRenderComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UMovieQualityRenderComponent();
	virtual ~UMovieQualityRenderComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movie Quality Render")
	FIntPoint Resolution;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movie Quality Render")
	bool bApplyMovieQualitySettings;

	UFUNCTION(BlueprintCallable, Category = "Movie Quality Render")
	void Initialize();

	UFUNCTION(BlueprintCallable, Category = "Movie Quality Render")
	void Shutdown();

	// UFUNCTION(BlueprintCallable, Category = "Movie Quality Render")
	void SaveLitToFile(const FString& OutputPath, TFunction<void(bool)> OnComplete = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Movie Quality Render")
	void ApplyMovieQualitySettings();

	UFUNCTION(BlueprintCallable, Category = "Movie Quality Render")
	void RestoreQualitySettings();

	bool IsInitialized() const { return bIsInitialized; }

	void SetParentSensor(UFusionCamSensor* InSensor) { ParentSensor = InSensor; }
	UFusionCamSensor* GetParentSensor() const { return ParentSensor; }

private:
	TSharedPtr<FSceneViewFamilyContext> CreateViewFamily(UTextureRenderTarget2D* RenderTarget);
	FSceneView* CreateSceneView(FSceneViewFamily* ViewFamily);
	void SubmitToRenderer(
		FSceneViewFamily* ViewFamily,
		UTextureRenderTarget2D* RenderTarget,
		const FString& OutputPath,
		TFunction<void(bool)> OnComplete
	);

	float GetTargetGamma() const;

private:
	UPROPERTY()
	UFusionCamSensor* ParentSensor;

	bool bIsInitialized;
	EPixelFormat PixelFormat;
	bool bForceLinearGamma;

	TSharedPtr<FMoviePipelineSurfaceQueue, ESPMode::ThreadSafe> SurfaceQueue;
	FSceneViewStateReference ViewState;
	IImageWriteQueue* ImageWriteQueue;

	TMap<FString, float> PreviousQualitySettings;

	UPROPERTY()
	TMap<FString, UTextureRenderTarget2D*> RenderTargetPool;
};
