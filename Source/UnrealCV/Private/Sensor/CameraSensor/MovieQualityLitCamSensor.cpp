#include "MovieQualityLitCamSensor.h"
#include "Runtime/Engine/Classes/Components/PrimitiveComponent.h"
#include "Runtime/Engine/Classes/Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Engine/Canvas.h"
#include "SceneView.h"
#include "EngineModule.h"
#include "RenderingThread.h"
#include "UnrealcvLog.h"

UMovieQualityLitCamSensor::UMovieQualityLitCamSensor()
	: Super()
{
	SetComponentTickEnabled(false);
	bRenderEveryFrame = false;

	CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;

	ShowFlags.SetLighting(false);
	ShowFlags.SetSkyLighting(false);
	ShowFlags.SetFog(false);
	ShowFlags.SetVolumetricFog(false);
	ShowFlags.SetPostProcessing(false);
	ShowFlags.SetCloud(false);
	ShowFlags.SetAtmosphere(false);
	ShowFlags.SetLumenGlobalIllumination(false);
	ShowFlags.SetGlobalIllumination(false);
	ShowFlags.SetLumenReflections(false);
	ShowFlags.SetScreenSpaceReflections(false);
	ShowFlags.SetDistanceFieldAO(false);
	ShowFlags.SetScreenSpaceAO(false);
	ShowFlags.SetAntiAliasing(true);
	ShowFlags.SetTemporalAA(true);
}

void UMovieQualityLitCamSensor::BeginPlay()
{
	Super::BeginPlay();

	if (!SyncRenderTarget)
	{
		SyncRenderTarget = NewObject<UTextureRenderTarget2D>(this);
		SyncRenderTarget->InitCustomFormat(Resolution.X, Resolution.Y, PF_B8G8R8A8, false);
		SyncRenderTarget->ClearColor = FLinearColor::Black;
	}
}

void UMovieQualityLitCamSensor::SetPostProcessSettings(FPostProcessSettings& PPSettings)
{
	SetDefaultPostProcessSettings(PPSettings);
	PPSettings.bOverride_ReflectionMethod = 1;
	PPSettings.ReflectionMethod = EReflectionMethod::Type::None;
	PPSettings.bOverride_DynamicGlobalIlluminationMethod = 1;
	PPSettings.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::Type::None;
}

void UMovieQualityLitCamSensor::CaptureLit(TArray<FColor>& Image, int& Width, int& Height)
{
	if (!IsInitialized())
	{
		UE_LOG(LogUnrealCV, Error, TEXT("MovieQualityLitCamSensor not initialized"));
		Image.Empty();
		Width = 0;
		Height = 0;
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		Image.Empty();
		Width = 0;
		Height = 0;
		return;
	}

	Width = Resolution.X;
	Height = Resolution.Y;

	if (!SyncRenderTarget || SyncRenderTarget->GetSurfaceWidth() != Width || SyncRenderTarget->GetSurfaceHeight() != Height)
	{
		if (SyncRenderTarget)
		{
			SyncRenderTarget->RemoveFromRoot();
		}
		SyncRenderTarget = NewObject<UTextureRenderTarget2D>(this);
		SyncRenderTarget->InitCustomFormat(Width, Height, PF_B8G8R8A8, false);
		SyncRenderTarget->ClearColor = FLinearColor::Black;
		SyncRenderTarget->AddToRoot();
	}

	TSharedPtr<FSceneViewFamilyContext> ViewFamily = CreateViewFamily(SyncRenderTarget);
	if (!ViewFamily.IsValid())
	{
		Image.Empty();
		return;
	}

	FSceneView* View = CreateSceneView(ViewFamily.Get());
	if (!View)
	{
		Image.Empty();
		return;
	}

	World->SendAllEndOfFrameUpdates();

	FRenderTarget* RenderTargetResource = SyncRenderTarget->GameThread_GetRenderTargetResource();
	FCanvas Canvas(RenderTargetResource, nullptr, World, ViewFamily->GetFeatureLevel(), FCanvas::CDM_DeferDrawing, 1.0f);
	GetRendererModule().BeginRenderingViewFamily(&Canvas, ViewFamily.Get());

	FlushRenderingCommands();

	FReadSurfaceDataFlags ReadSurfaceDataFlags;
	ReadSurfaceDataFlags.SetLinearToGamma(false);
	RenderTargetResource->ReadPixels(Image, ReadSurfaceDataFlags);

	if (Image.Num() == 0)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("MovieQualityLitCamSensor::CaptureLit - Captured data is empty"));
	}
}

void UMovieQualityLitCamSensor::CaptureLitToFile(const FString& Filename)
{
	if (!IsInitialized())
	{
		UE_LOG(LogUnrealCV, Error, TEXT("MovieQualityLitCamSensor not initialized"));
		return;
	}

	CaptureFrameToFile(Filename);
}
