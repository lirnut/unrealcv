// Weichao Qiu @ 2017
#pragma once

#include "Runtime/Engine/Classes/Components/PrimitiveComponent.h"
#include "Runtime/Engine/Classes/Camera/CameraTypes.h"
#include "FusionCamSensor.generated.h"

UENUM(BlueprintType)
enum class ELitMode : uint8
{
	Lit,
	Slow
};

UENUM(BlueprintType)
enum class EDepthMode : uint8
{
	PlaneDepth,
	DistToCamCenter
};

UENUM(BlueprintType)
enum class ESegMode : uint8
{
	AnnotationComponent,
	VertexColor,
	CustomStencil
};

UENUM(BlueprintType)
enum class EPresetFilmSize : uint8
{
	F640x480,
	F720p,
	F1080p
};

UCLASS(meta = (BlueprintSpawnableComponent))
class UNREALCV_API UFusionCamSensor : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UFusionCamSensor(const FObjectInitializer& ObjectInitializer);

	void checkFusionSensors();

	// virtual void OnRegister() override;
	virtual bool GetEditorPreviewInfo(float DeltaTime, FMinimalViewInfo& ViewOut);

public:

	// /** Get rgbm data */
	// UFUNCTION(BlueprintPure, Category = "unrealcv")
	// void GetLitSeg(TArray<FColor>& DataRGB, TArray<FColor>& DataSeg, int& InOutWidth, int& InOutHeight);

	/** Get one object mask data */
	UFUNCTION(BlueprintPure, Category = "unrealcv")
	void GetOneObjMask(AActor* Actor, TArray<FColor>& Data, int& InOutWidth, int& InOutHeight);
	void SaveOneObjMaskToFile(AActor* Actor, const FString& Filename);

	/** Get one object lit data */
	// UFUNCTION(BlueprintPure, Category = "unrealcv")
	// void GetOneObjLit(AActor* Actor, TArray<FColor>& Data, int& InOutWidth, int& InOutHeight);
	void SaveOneObjLitToFile(AActor* Actor, const FString& Filename);

	/** Get shadow catcher data (object RGB + shadow) */
	UFUNCTION(BlueprintPure, Category = "unrealcv")
	void GetShadowCatcher(AActor* Actor, TArray<FColor>& Data, int& InOutWidth, int& InOutHeight);
	void SaveShadowCatcherToFile(AActor* Actor, const FString& Filename);

	UFUNCTION(BlueprintPure, Category = "unrealcv")
	void GetStencilMask(AActor* Actor, TArray<FColor>& Data, int& InOutWidth, int& InOutHeight);
	void SaveStencilMaskToFile(AActor* Actor, const FString& Filename);

	/** Get rgb data */
	UFUNCTION(BlueprintPure, Category = "unrealcv")
	void GetLit(TArray<FColor>& LitData, int& InOutWidth, int& InOutHeight, ELitMode LitMode = ELitMode::Lit);
	// UFUNCTION(BlueprintPure, Category = "unrealcv")
	void SaveLitToFile(const FString& Filename);

	/** Get depth data */
	UFUNCTION(BlueprintPure, Category = "unrealcv")
	void GetDepth(TArray<float>& DepthData, int& InOutWidth, int& InOutHeight, EDepthMode DepthMode = EDepthMode::PlaneDepth);
	// void CaptureDepthToFile(const FString& Filename);
	// UFUNCTION(BlueprintPure, Category = "unrealcv")
	void SaveDepthToFile(const FString& Filename);

	/** Get surface normal data */
	UFUNCTION(BlueprintPure, Category = "unrealcv")
	void GetNormal(TArray<FColor>& NormalData, int& Width, int& Height);
	// UFUNCTION(BlueprintPure, Category = "unrealcv")
	void SaveNormalToFile(const FString& Filename);

	/** Get optical flow data */
	UFUNCTION(BlueprintPure, Category = "unrealcv")
	void GetFlow(TArray<FColor>& FlowData, int& Width, int& Height);
	// UFUNCTION(BlueprintPure, Category = "unrealcv")
	void SaveFlowToFile(const FString& Filename);

	/** Get object mask data, the annotation color can be extracted from FObjectAnnotator */
	UFUNCTION(BlueprintPure, Category = "unrealcv")
	void GetSeg(TArray<FColor>& ObjMaskData, int& Width, int& Height, ESegMode SegMode = ESegMode::AnnotationComponent);
	// UFUNCTION(BlueprintPure, Category = "unrealcv")
	void SaveSegToFile(const FString& Filename);

	UFUNCTION(BlueprintPure, Category = "unrealcv")
	FVector GetSensorLocation();

	UFUNCTION(BlueprintPure, Category = "unrealcv")
	FRotator GetSensorRotation();

	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	void SetSensorLocation(FVector Location);

	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	void SetSensorRotation(FRotator Rotator);

	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	void SetFilmSize(int Width, int Height);

	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	float GetFilmWidth() { return FilmWidth; }

	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	float GetFilmHeight() { return FilmHeight; }

	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	float GetSensorFOV();

	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	void SetSensorFOV(float FOV);

	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	void SetProjectionType(ECameraProjectionMode::Type ProjectionType);

	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	void SetOrthoWidth(float OrthoWidth);

	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	void SetLitCaptureSource(ESceneCaptureSource CaptureSource);

    UFUNCTION(BlueprintCallable, Category = "unrealcv")
    void SetReflectionMethod(EReflectionMethod::Type Method);

    UFUNCTION(BlueprintCallable, Category = "unrealcv")
    void SetGlobalIlluminationMethod(EDynamicGlobalIlluminationMethod::Type Method);

    UFUNCTION(BlueprintCallable, Category = "unrealcv")
	void SetExposureMethod(EAutoExposureMethod ExposureMethod);

	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	void SetExposureBias(float ExposureBias);

	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	void SetAutoExposureSpeed(float ExposureSpeedDown, float ExposureSpeedUp);

	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	void SetAutoExposureBrightness(float MinBrightness, float MaxBrightness);

	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	void SetApplyPhysicalCameraExposure(int ApplyPhysicalCameraExposure);

	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	void SetMotionBlurParams(float MotionBlurAmount, float MotionBlurMax, float MotionBlurPerObjectSize, int MotionBlurTargetFPS);

    UFUNCTION(BlueprintCallable, Category = "unrealcv")
    void SetFocalParams(float FocalDistance, float FocalRegion);

	// Simulated lens artifacts
	void SetChromaticAberration(float Intensity);
	void SetVignetteIntensity(float Intensity);
	void SetFilmGrain(float Intensity, float TexelSize = 1.0f);

	// High-end bloom (requires a Texture2D kernel)
	void SetConvolutionBloom(EBloomMethod Method, UTexture2D* KernelTexture, float Intensity = 1.0f);


	// UFUNCTION(BlueprintPure, Category = "unrealcv")
	// float GetFilmHeight();

	// UFUNCTION(BlueprintPure, Category = "unrealcv")
	// float GetFilmWidth();

	// void SetFilmSize(int Width, int Height);

	UPROPERTY(meta = (AllowPrivateAccess= "true"))
	class UCameraComponent* PreviewCamera;

	virtual void BeginPlay() override;


	static TArray<UFusionCamSensor*> GetComponents(AActor* Actor);

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent) override;
#endif

private:
	UPROPERTY(EditInstanceOnly, meta=(AllowPrivateAccess = "true"), Category = "unrealcv")
	EPresetFilmSize PresetFilmSize;

	UPROPERTY(EditInstanceOnly, meta=(AllowPrivateAccess = "true"), Category = "unrealcv")
	int FilmWidth;

	UPROPERTY(EditInstanceOnly, meta=(AllowPrivateAccess = "true"), Category = "unrealcv")
	int FilmHeight;

	UPROPERTY(EditInstanceOnly, meta=(AllowPrivateAccess = "true"), Category = "unrealcv")
	float FOV;

protected:
	UPROPERTY()
	TArray<class UBaseCameraSensor*> FusionSensors;

	UPROPERTY(EditDefaultsOnly, Category = "unrealcv")
	class UDepthCamSensor* DepthCamSensor;

	UPROPERTY(EditDefaultsOnly, Category = "unrealcv")
	class UNormalCamSensor* NormalCamSensor;

	UPROPERTY(EditDefaultsOnly, Category = "unrealcv")
	class UAnnotationCamSensor* AnnotationCamSensor;

	UPROPERTY(EditDefaultsOnly, Category = "unrealcv")
	class ULitCamSensor* LitCamSensor;

	/* LogOutputDevice: Error: Ensure condition failed: false  [File:D:\build\++UE5\Sync\Engine\Source\Runtime\Engine\Private\Components\SceneComponent.cpp] [Line: 2104] 
	 * LogOutputDevice: Error: Template Mismatch during attachment. Attaching instanced component to template component. Parent 'FusionCamSensor_GEN_VARIABLE' (Owner 'None') Self 'FusionCamSensor_GEN_VARIABLE_FlowCamSensor' (Owner 'BP_Drone01_C_1').
	 * 'FusionCamSensor_1_AnnotationCamSensor': FusionCamSensor /Game/SuburbNeighborhood_Day_dooropen.SuburbNeighborhood_Day_dooropen:PersistentLevel.BP_Drone01_C_1.FusionCamSensor
	 * 'FusionCamSensor_1_PreviewCamera': FusionCamSensor /Game/SuburbNeighborhood_Day_dooropen.SuburbNeighborhood_Day_dooropen:PersistentLevel.BP_Drone01_C_1.FusionCamSensor
	 * 'FusionCamSensor_1_DepthCamSensor': FusionCamSensor /Game/SuburbNeighborhood_Day_dooropen.SuburbNeighborhood_Day_dooropen:PersistentLevel.BP_Drone01_C_1.FusionCamSensor
	 * 'FusionCamSensor_1_LitCamSensor': FusionCamSensor /Game/SuburbNeighborhood_Day_dooropen.SuburbNeighborhood_Day_dooropen:PersistentLevel.BP_Drone01_C_1.FusionCamSensor
	 * 'FusionCamSensor_1_NormalCamSensor': FusionCamSensor /Game/SuburbNeighborhood_Day_dooropen.SuburbNeighborhood_Day_dooropen:PersistentLevel.BP_Drone01_C_1.FusionCamSensor
	 */
	// UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "unrealcv")
	UPROPERTY(EditDefaultsOnly, Category = "unrealcv")
	class UFlowCamSensor* FlowCamSensor;

	UPROPERTY(EditDefaultsOnly, Category = "unrealcv")
	class UAnnotationCamSensor* OneObjectMaskCamSensor;

	UPROPERTY(EditDefaultsOnly, Category = "unrealcv")
	// class UMovieQualityLitCamSensor* OneObjectLitCamSensor;
	class ULitCamSensor* OneObjectLitCamSensor;

	UPROPERTY(EditDefaultsOnly, Category = "unrealcv")
	class UShadowCatcherCamSensor* ShadowCatcherCamSensor;

	UPROPERTY(EditDefaultsOnly, Category = "unrealcv")
	class UStencilMaskCamSensor* StencilMaskCamSensor;

	UPROPERTY(EditDefaultsOnly, Category = "unrealcv")
	class UMovieQualityRenderComponent* MovieQualityRenderer;


	/** This preview camera is used for UE version < 4.17 which only support UCameraComponent PIP preview
	See the difference between
	https://github.com/EpicGames/UnrealEngine/blob/4.17/Engine/Source/Editor/LevelEditor/Private/SLevelViewport.cpp#L3927
	and
	https://github.com/EpicGames/UnrealEngine/blob/4.16/Engine/Source/Editor/LevelEditor/Private/SLevelViewport.cpp#L3908
	*/

public:
	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	UDepthCamSensor* GetDepthCamSensor() const { return DepthCamSensor; }

	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	UNormalCamSensor* GetNormalCamSensor() const { return NormalCamSensor; }

	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	UAnnotationCamSensor* GetAnnotationCamSensor() const { return AnnotationCamSensor; }

	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	ULitCamSensor* GetLitCamSensor() const { return LitCamSensor; }

	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	class UFlowCamSensor* GetFlowCamSensor() const { return FlowCamSensor; }

	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	class UShadowCatcherCamSensor* GetShadowCatcherCamSensor() const { return ShadowCatcherCamSensor; }

	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	class UStencilMaskCamSensor* GetStencilMaskCamSensor() const { return StencilMaskCamSensor; }

	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	class UMovieQualityRenderComponent* GetMovieQualityRenderer() const { return MovieQualityRenderer; }

	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	TArray<class UBaseCameraSensor*> GetSensors() const { return FusionSensors; }

	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	void SetAsyncCaptureNextFrame(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	void SetUseFastCapture(bool bInUseAsync);

	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	bool GetUseFastCapture() const;

	// Getters for camera parameters set in PrepareTargetCamera()
	UFUNCTION(BlueprintPure, Category = "unrealcv")
	EReflectionMethod::Type GetReflectionMethod() const;

	UFUNCTION(BlueprintPure, Category = "unrealcv")
	EDynamicGlobalIlluminationMethod::Type GetGlobalIlluminationMethod() const;

	UFUNCTION(BlueprintPure, Category = "unrealcv")
	EAutoExposureMethod GetExposureMethod() const;

	UFUNCTION(BlueprintPure, Category = "unrealcv")
	void GetAutoExposureSpeed(float& OutExposureSpeedDown, float& OutExposureSpeedUp) const;

	UFUNCTION(BlueprintPure, Category = "unrealcv")
	void GetMotionBlurParams(float& OutMotionBlurAmount, float& OutMotionBlurMax, float& OutMotionBlurPerObjectSize, int& OutMotionBlurTargetFPS) const;

	UFUNCTION(BlueprintPure, Category = "unrealcv")
	void GetFocalParams(float& OutFocalDistance, float& OutFocalRegion) const;

	UFUNCTION(BlueprintPure, Category = "unrealcv")
	float GetChromaticAberration() const;

	UFUNCTION(BlueprintPure, Category = "unrealcv")
	float GetVignetteIntensity() const;

	// UFUNCTION(BlueprintPure, Category = "unrealcv")
	void GetBloomParams(EBloomMethod& OutBloomMethod, float& OutBloomIntensity) const;
};
