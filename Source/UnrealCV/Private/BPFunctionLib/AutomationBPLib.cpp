#include "AutomationBPLib.h"
#include "Scalability.h"
#include "Utils/GenericTickableObject.h"
#include "UnrealcvLog.h"
#include "UnrealcvServer.h"
#include "Engine/World.h"

TQueue<FString> UAutomationBPLib::CommandQueue;
FGenericTickableObject* UAutomationBPLib::TickableObject = nullptr;
bool UAutomationBPLib::bIsActive = false;

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

	// PushCommand(TEXT("r.AntiAliasingMethod 2"));  // TAA
	// PushCommand(TEXT("r.TemporalAACurrentFrameWeight 0.12"));
	PushCommand(TEXT("r.TemporalAA.Quality 3"));
	PushCommand(TEXT("r.TemporalAAPauseCorrect 1"));
	PushCommand(TEXT("r.FXAA.Quality 5"));

	// Lumen Temporal Filter for Ghosting Fix
	// PushCommand(TEXT("r.Lumen.ScreenProbeGather.Temporal 1"));
	PushCommand(TEXT("r.Lumen.ScreenProbeGather.Temporal.MaxFramesAccumulated 10"));
	// PushCommand(TEXT("r.Lumen.ScreenProbeGather.Temporal.DistanceThreshold 0.01"));
	// PushCommand(TEXT("r.Lumen.ScreenProbeGather.Temporal.NormalThreshold 10"));
	PushCommand(TEXT("r.Lumen.ScreenProbeGather.Temporal.FastUpdateModeUseNeighborhoodClamp 1"));
	// PushCommand(TEXT("r.Lumen.ScreenProbeGather.Temporal.RejectBasedOnNormal 1"));
	// PushCommand(TEXT("r.Lumen.ScreenProbeGather.HairStrands.ScreenTrace 1"));
// #endif
	PushCommand(TEXT("r.NGX.Automation.NonGameViews 1"));
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
	FString Command;
	while (CommandQueue.Dequeue(Command))
	{
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
