#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "ImagePixelData.h"
#include "MainViewportRenderComponent.generated.h"

class UGameViewportClient;
class FSceneViewport;
class FUnrealCVImageWriteQueue;
struct FUnrealCVSurfaceQueue;

USTRUCT()
struct FMVRCSettings
{
	GENERATED_BODY()

	UPROPERTY()
	bool bUseSyncCapture = false;
};

UCLASS(ClassGroup = (UnrealCV), meta = (BlueprintSpawnableComponent))
class UNREALCV_API UMainViewportRenderComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	static FMVRCSettings GlobalSettings;

	UMainViewportRenderComponent();
	virtual ~UMainViewportRenderComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "MainViewportRender")
	void Initialize(int32 ResolutionX, int32 ResolutionY);

	UFUNCTION(BlueprintCallable, Category = "MainViewportRender")
	FIntPoint GetViewportSize() const;

	UFUNCTION(BlueprintCallable, Category = "MainViewportRender")
	void Shutdown();

	void CaptureFrame(TFunction<void(TUniquePtr<FImagePixelData>&&)> OnPixelDataReady);

	void CaptureFrameSync(TFunction<void(TUniquePtr<FImagePixelData>&&)> OnPixelDataReady);

	void CaptureFrameToFile(const FString& OutputPath, TFunction<void(bool)> OnComplete = nullptr);

	UFUNCTION(BlueprintCallable, Category = "MainViewportRender")
	bool IsInitialized() const { return bIsInitialized; }

	UFUNCTION(BlueprintCallable, Category = "MainViewportRender")
	void CaptureScreen();

	UFUNCTION(BlueprintCallable, Category = "MainViewportRender")
	void SetFOV(float InFOV);

	UFUNCTION(BlueprintCallable, Category = "MainViewportRender")
	float GetFOV() const { return FOV; }

	UFUNCTION(BlueprintCallable, Category = "MainViewportRender")
	float GetActualFOV() const;

protected:
	bool bIsInitialized = false;
	float FOV = 90.0f;

	UPROPERTY()
	UGameViewportClient* ViewportClient = nullptr;

	FSceneViewport* SceneViewport = nullptr;

	TSharedPtr<FUnrealCVImageWriteQueue> ImageWriteQueue;
	TSharedPtr<FUnrealCVSurfaceQueue, ESPMode::ThreadSafe> SurfaceQueue;
};
