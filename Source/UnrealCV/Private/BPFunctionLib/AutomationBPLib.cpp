#include "AutomationBPLib.h"
#include "Scalability.h"
#include "Utils/GenericTickableObject.h"
#include "UnrealcvLog.h"
#include "UnrealcvServer.h"
#include "Engine/World.h"

TQueue<FString> UAutomationBPLib::CommandQueue;
FGenericTickableObject* UAutomationBPLib::TickableObject = nullptr;
bool UAutomationBPLib::bIsActive = false;
double UAutomationBPLib::SleepTo = 0.0;

void UAutomationBPLib::PushCommand(const FString& Command)
{
	CommandQueue.Enqueue(Command);
	UE_LOG(LogUnrealCV, Log, TEXT("AutomationBPLib: Command queued: %s"), *Command);
}

void UAutomationBPLib::StartTicking()
{
	if (TickableObject)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("AutomationBPLib: Already ticking"));
		delete TickableObject;
		TickableObject = nullptr;
	}

	bIsActive = true;
	TickableObject = new FGenericTickableObject();
	TickableObject->SetTickCallback(&UAutomationBPLib::OnTick);
	TickableObject->Activate();
	UE_LOG(LogUnrealCV, Log, TEXT("AutomationBPLib: Ticking started"));

	PushCommand(TEXT("stat FPS"));

	// PushCommand(TEXT("vset /datasetautomation/config/output_directory C:\\Users\\hulc\\AppData\\Local\\HUAWEI_Project\\Saved\\Dataset"));
	PushCommand(TEXT("vset /datasetautomation/config/output_directory"));

// #if WITH_EDITOR
	PushCommand(TEXT("vset /captureactor/spawn_free_cam"));
// #endif

	PushCommand(TEXT("r.ForceLOD 0"));
	PushCommand(TEXT("r.SkeletalMeshLODBias -10"));

	// https://dev.epicgames.com/community/learning/tutorials/Ya6o/unreal-engine-rendering-hair-fur
	// https://www.reddit.com/r/unrealengine/comments/154wry8/why_default_skylight_adds_too_much_noise_to_the/
	PushCommand(TEXT("r.HairStrands.Skylighting 0"));
	// PushCommand(TEXT("r.HairStrands.SkyLighting.SampleCount 256"));
	// PushCommand(TEXT("r.HairStrands.SkyLighting.IntegrationType 0"));
	PushCommand(TEXT("r.HairStrands.Voxelization.Virtual.VoxelWorldSize 0.1"));
	PushCommand(TEXT("r.HairStrands.RasterizationScale 0.5"));
	PushCommand(TEXT("r.HairStrands.Voxelization.Raymarching.SteppingScale 1.15"));
	// PushCommand(TEXT("r.HairStrands.Visibility.PPLL 1 "));
	// PushCommand(TEXT("r.HairStrands.DeepShadow.SuperSampling 1"));

	PushCommand(TEXT("r.SceneCapture.AllowRenderInMainRenderer 1"));
	PushCommand(TEXT("r.SceneCapture.CubeSinglePass 1"));
	PushCommand(TEXT("r.SceneCapture.DepthPrepassOptimization 1"));
	PushCommand(TEXT("r.SceneCapture.EnableLogging 1"));

	PushCommand(TEXT("r.TextureStreaming 0"));

// #if WITH_EDITOR
	PushCommand(TEXT("MaxQuality"));
	PushCommand(TEXT("r.ParticleLODBias -10"));
	PushCommand(TEXT("foliage.DitheredLOD 0"));
	PushCommand(TEXT("foliage.ForceLOD 0"));
	PushCommand(TEXT("r.ShadowQuality 5"));
	PushCommand(TEXT("r.Shadow.DistanceScale 10.0"));
	PushCommand(TEXT("r.Shadow.RadiusThreshold 0.001"));
	PushCommand(TEXT("r.ViewDistanceScale 50.0"));
	PushCommand(TEXT("r.VolumetricRenderTarget 1"));
	PushCommand(TEXT("r.VolumetricRenderTarget.Mode 3"));
	PushCommand(TEXT("r.SkyLight.RealTimeReflectionCapture.TimeSlice 0"));
	PushCommand(TEXT("r.PostProcessing.PropagateAlpha 1"));
	PushCommand(TEXT("r.RayTracing.SceneCaptures 1"));

	PushCommand(TEXT("r.Shadow.Denoiser 1"));
	PushCommand(TEXT("r.MotionBlur.Amount 0"));

	PushCommand(TEXT("r.AntiAliasingMethod 4"));
	// PushCommand(TEXT("r.TemporalAACurrentFrameWeight 0.12"));
	// PushCommand(TEXT("r.TemporalAACurrentFrameWeight 0.22"));
	PushCommand(TEXT("r.TemporalAA.Quality 3"));
	PushCommand(TEXT("r.TemporalAAPauseCorrect 1"));
	PushCommand(TEXT("r.FXAA.Quality 5"));

	// Lumen Temporal Filter for Ghosting Fix
	// PushCommand(TEXT("r.Lumen.ScreenProbeGather.Temporal 1"));
	// PushCommand(TEXT("r.Lumen.ScreenProbeGather.Temporal.MaxFramesAccumulated 16"));
	// PushCommand(TEXT("r.Lumen.ScreenProbeGather.Temporal.DistanceThreshold 0.01"));
	// PushCommand(TEXT("r.Lumen.ScreenProbeGather.Temporal.NormalThreshold 20"));
	PushCommand(TEXT("r.Lumen.ScreenProbeGather.Temporal.FastUpdateModeUseNeighborhoodClamp 1"));
	// PushCommand(TEXT("r.Lumen.ScreenProbeGather.Temporal.RejectBasedOnNormal 1"));
	// PushCommand(TEXT("r.Lumen.ScreenProbeGather.HairStrands.ScreenTrace 1"));
// #endif
	PushCommand(TEXT("r.NGX.Automation.NonGameViews 1"));



	PushCommand(TEXT("sleep 20"));
	PushCommand(TEXT("vset /datasetautomation/config/b_exit_on_complete true"));
	PushCommand(TEXT("vset /datasetautomation/start"));
}

void UAutomationBPLib::StopTicking()
{
	if (TickableObject)
	{
		delete TickableObject;
		TickableObject = nullptr;
	}

	bIsActive = false;
	CommandQueue.Empty();
	UE_LOG(LogUnrealCV, Log, TEXT("AutomationBPLib: Ticking stopped"));
}

bool UAutomationBPLib::IsTickingActive()
{
	if (bIsActive)
	{
		check(TickableObject)
	}
	else
	{
		check(!TickableObject)
	}
	return bIsActive;
}

void UAutomationBPLib::OnTick(double DeltaTime)
{
	if (!bIsActive)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("AutomationBPLib: Tick called but not active"));
		return;
	}

	ProcessCommands();
}

void UAutomationBPLib::ProcessCommands()
{
	double CurrentTime = FPlatformTime::Seconds();
	if ( SleepTo > 0 && (SleepTo - CurrentTime > 0) )
	{
		check( (SleepTo - CurrentTime) < (60 * 60 * 24) );
		return;
	}
	else
	{
		SleepTo = 0.0;
	}


	FString Command;
	while (CommandQueue.Dequeue(Command))
	{
		TArray<FString> Args;
		Command.ParseIntoArray(Args, TEXT(" "));
		if (Args.Num() >= 2 && Args[0] == TEXT("sleep"))
		{
			float SleepTime = FCString::Atof(*Args[1]);
			SleepTo = CurrentTime + static_cast<double>(SleepTime);
			break;
		}


		UE_LOG(LogUnrealCV, Log, TEXT("AutomationBPLib: Executing command: %s"), *Command);
		if (Command == TEXT("MaxQuality"))
		{
			Scalability::FQualityLevels QualityLevels;
  			QualityLevels.SetFromSingleQualityLevelRelativeToMax(0);
  			Scalability::SetQualityLevels(QualityLevels);
			continue;
		}

		UE_LOG(LogUnrealCV, Log, TEXT("AutomationBPLib: console command: %s"), *Command);

		UWorld* World = FUnrealcvServer::Get().GetWorld();
		if (World && World->GetFirstPlayerController())
		{
			FString Result = World->GetFirstPlayerController()->ConsoleCommand(Command, true);
			if (!Result.IsEmpty())
			{
				UE_LOG(LogUnrealCV, Log, TEXT("AutomationBPLib: Command result: %s"), *Result);
			}
			else
			{
				UE_LOG(LogUnrealCV, Log, TEXT("AutomationBPLib: Command executed (no result)"));
			}
		}
		else
		{
			UE_LOG(LogUnrealCV, Error, TEXT("AutomationBPLib: Failed to get World or PlayerController"));
		}
	}
}
// 1. 抗锯齿 (TAA) 将使边缘更平滑，减少渲染中的锯齿线。
 
// • r.TemporalAASamples=32
// （值越高边缘越平滑，但会增加 GPU 负载。）
 
// • r.TemporalAACurrentFrameWeight=0.1
// （控制时间性抗锯齿的混合权重。值越低，重影越少，但可能会出现闪烁。）
// __________________________________________________________________________________________________________
 
// 2. 屏幕空间反射 (SSR) 将提供更清晰、更细致的反射。
 
// • r.SSR.Quality=4
// （屏幕空间反射的最高质量。）
 
// • r.SSR.HalfResSceneColor=0
// （禁用半分辨率渲染以获得更好的清晰度。）
// __________________________________________________________________________________________________________
 
// 3. 阴影将变得更清晰、更逼真，并具有更远的绘制距离和更高的分辨率。
 
// • r.ShadowQuality=5
// （最高阴影质量。）
 
// • r.Shadow.MaxResolution=4096
// （提高阴影分辨率以获得更清晰的阴影。）
 
// • r.Shadow.DistanceScale=2
// （增加阴影绘制距离。）
// __________________________________________________________________________________________________________
 
// 4. 环境光遮蔽 (AO) 将改善缝隙和物体之间的阴影，使光照看起来更自然。
 
// • r.AmbientOcclusionLevels=3
// （定义AO的级别数。）
 
// • r.AmbientOcclusionRadiusScale=2.0
// （更宽的环境光遮蔽半径，使缝隙中的阴影更柔和。）
// __________________________________________________________________________________________________________
 
// 5. 纹理设置将使你的纹理显得更清晰，尤其是在从一定角度或远处观看时。
 
// • r.Streaming.MipBias=-3
// （强制使用更高分辨率的 Mipmap。）
 
// • r.MaxAnisotropy=16
//  （最大化各向异性过滤，使倾斜角度的纹理更清晰。）
// __________________________________________________________________________________________________________
 
// 6.光照质量和全局光照将确保逼真的光照和反射，尤其是在使用 Lumen 的情况下。
 
// • r.LightingDetailMode=2
// （启用完整的光照细节。）
 
// • r.HZBOcclusion=1
// （启用高质量的基于地平线的环境光遮蔽。）
 
// • r.VolumetricFog=1
// （启用体积雾，以获得更好的光照效果。）
 
// • r.Lumen.Reflections.HitLighting=1
// （确保Lumen反射包含命中光照，从而使反射表面上的光照更准确。）
 
// • r.Lumen.Reflections.HierarchicalScreenTraces
// （为Lumen反射启用分层屏幕追踪，提高场景中反射的精度和质量。）
// __________________________________________________________________________________________________________
 
// 7.景深
 
// • r.DepthOfFieldQuality=4
// （启用高质量的景深。）
// __________________________________________________________________________________________________________
 
// 8.运动模糊
 
// • r.MotionBlurQuality=4
// （最大化运动模糊质量，以获得更平滑的过渡。）
// __________________________________________________________________________________________________________
 
// 9.后期处理效果，如泛光、景深和运动模糊，将具有更高的质量和更具电影感的画面。
 
// • r.PostProcessAAQuality=6
// （提高后期处理中的抗锯齿质量。）
 
// • r.BloomQuality=5
// （最高的泛光质量，用于电影级光照。）
// __________________________________________________________________________________________________________
 
// 10.屏幕百分比提升超采样，以获得更高的像素密度和更清晰的图像。
 
// • r.ScreenPercentage=200
// （将渲染分辨率提升至 200% 以进行超采样。根据性能进行调整。）
// __________________________________________________________________________________________________________
 
// 11. 光线追踪（如果支持）可以进一步提升反射、阴影和全局光照质量。
 
// • r.RayTracing.Reflections=1
// （启用光线追踪反射，对于支持的硬件，它比屏幕空间反射更准确。）
 
// • r.RayTracing.Shadows=1
// （启用光线追踪阴影，提高阴影的精度和柔和度。）
 
// • r.RayTracing.GlobalIllumination=1
// （启用光线追踪全局光照，以获得更逼真和动态的光照。）
// __________________________________________________________________________________________________________
 
// 12. 虚拟纹理
 
// • r.VirtualTextures=1
// （启用虚拟纹理以便更好地管理大型纹理的内存。）
 
// • r.VT.TileSize=128
// （增加图块大小以获得更清晰的纹理。）
// __________________________________________________________________________________________________________
 
// 13.帧率（可选）
 
// • t.MaxFPS=60 or t.MaxFPS=120
// （调整最大帧率以平衡质量和性能。）
// __________________________________________________________________________________________________________