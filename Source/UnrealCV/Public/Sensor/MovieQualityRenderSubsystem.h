#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RHI.h"
#include "RHIResources.h"
#include "SceneView.h"
#include "MovieRenderPipelineDataTypes.h"
#include "MovieQualityRenderSubsystem.generated.h"

struct FMoviePipelineSurfaceQueue;
class IImageWriteQueue;
class UFusionCamSensor;

USTRUCT()
struct FMovieQualityFrameData
{
  GENERATED_BODY()

  FVector CameraLocation;
  FRotator CameraRotation;
  float FOV;
  int32 FrameNumber;
  FString PassName;
};

UCLASS()
class UNREALCV_API UMovieQualityRenderSubsystem : public UObject
{
  GENERATED_BODY()

public:
  UMovieQualityRenderSubsystem();
  virtual ~UMovieQualityRenderSubsystem();

  void Initialize(UWorld* InWorld, FIntPoint InResolution);
  void Shutdown();

  void CaptureFrame(
    UFusionCamSensor* Sensor,
    const FString& OutputPath,
    const FString& PassName,
    int32 FrameNumber,
    TFunction<void(bool)> OnComplete
  );

  void ApplyMovieQualitySettings();
  void RestoreQualitySettings();

  bool IsInitialized() const { return bIsInitialized; }

private:
  TSharedPtr<FSceneViewFamilyContext> CreateViewFamily(
    UFusionCamSensor* Sensor,
    UTextureRenderTarget2D* RenderTarget
  );

  FSceneView* CreateSceneView(
    FSceneViewFamily* ViewFamily,
    UFusionCamSensor* Sensor
  );

  void SubmitToRenderer(
    FSceneViewFamily* ViewFamily,
    UTextureRenderTarget2D* RenderTarget,
    const FString& OutputPath,
    const FString& PassName,
    int32 FrameNumber,
    TFunction<void(bool)> OnComplete
  );

private:
  UPROPERTY()
  UWorld* World;

  FIntPoint Resolution;
  bool bIsInitialized;

  TSharedPtr<FMoviePipelineSurfaceQueue, ESPMode::ThreadSafe> SurfaceQueue;
  FSceneViewStateReference ViewState;
  IImageWriteQueue* ImageWriteQueue;

  TMap<FString, float> PreviousQualitySettings;

  UPROPERTY()
  TMap<FString, UTextureRenderTarget2D*> RenderTargetPool;
};