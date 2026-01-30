// shc @ 2025
// Custom SceneCaptureComponent2D that forces rendering in main renderer for maximum quality
#include "MainRendererSceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "UnrealcvLog.h"

UMainRendererSceneCaptureComponent2D::UMainRendererSceneCaptureComponent2D(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bForceMainRenderer = true;
	bForceMainViewFamily = true;
	bUseHighQualityFormat = true;

	CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
	bRenderInMainRenderer = true;
	bMainViewFamily = true;

	bCaptureEveryFrame = false;
	bCaptureOnMovement = false;
	bAlwaysPersistRenderingState = true;
	bUseRayTracingIfEnabled = true;

	ShowFlags.SetPostProcessing(true);
	ShowFlags.SetAntiAliasing(true);
	ShowFlags.SetTemporalAA(true);
	ShowFlags.SetMotionBlur(true);
	ShowFlags.SetBloom(true);
	ShowFlags.SetDynamicShadows(true);
	ShowFlags.SetGlobalIllumination(true);
	ShowFlags.SetReflectionEnvironment(true);
	ShowFlags.SetAmbientOcclusion(true);
	ShowFlags.SetScreenSpaceReflections(true);

	PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;

	UE_LOG(LogUnrealCV, Log, TEXT("MainRendererSceneCaptureComponent2D: Initialized with main renderer mode"));
}

void UMainRendererSceneCaptureComponent2D::OnRegister()
{
	Super::OnRegister();

	if (bForceMainRenderer)
	{
		CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
		bRenderInMainRenderer = true;
	}

	if (bForceMainViewFamily)
	{
		bMainViewFamily = true;
	}

	if (bUseHighQualityFormat && TextureTarget)
	{
		if (TextureTarget->RenderTargetFormat != RTF_RGBA16f)
		{
			UE_LOG(LogUnrealCV, Warning, TEXT("MainRendererSceneCaptureComponent2D: TextureTarget format is not RGBA16f. Consider using Float16 format for better quality."));
		}
	}

	UE_LOG(LogUnrealCV, Log, TEXT("MainRendererSceneCaptureComponent2D: OnRegister - bRenderInMainRenderer=%d, bMainViewFamily=%d, CaptureSource=%d"),
		bRenderInMainRenderer, bMainViewFamily, (int32)CaptureSource);
}
