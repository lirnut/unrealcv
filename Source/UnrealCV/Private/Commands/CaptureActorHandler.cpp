#include "CaptureActorHandler.h"
#include "RecordingBPLib.h"
#include "SensorBPLib.h"
#include "UnrealcvServer.h"
#include "UnrealcvLog.h"
#include "AssetPoolManager.h"

void FCaptureActorHandler::RegisterCommands()
{
	auto BindCommandDualCameraID = [this](
		const FString& FormatStr,
		FDispatcherDelegate Delegate,
		const FString& HelpStr)
	{
		if (!FormatStr.Contains(TEXT("[camera_id]")))
		{
			UE_LOG(LogTemp, Error, TEXT("FormatStr must contain [camera_id] placeholder: %s"), *FormatStr);
			check(false);
		}

		FString UintFormatStr = FormatStr.Replace(TEXT("[camera_id]"), TEXT("[uint]"));
		CommandDispatcher->BindCommand(UintFormatStr, Delegate, HelpStr);

		FString StrFormatStr = FormatStr.Replace(TEXT("[camera_id]"), TEXT("[str]"));
		CommandDispatcher->BindCommand(StrFormatStr, Delegate, HelpStr);
	};

	FDispatcherDelegate Cmd;
	FString Help;

	Cmd = FDispatcherDelegate::CreateRaw(this, &FCaptureActorHandler::SpawnFreeCamera);
	Help = "Spawn a free camera at world origin (0, 0, 0)";
	CommandDispatcher->BindCommand("vset /captureactor/spawn_free_cam", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FCaptureActorHandler::GetTimeDilation);
	Help = "Get current time dilation value";
	CommandDispatcher->BindCommand("vget /captureactor/time_dilation", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FCaptureActorHandler::SetTimeDilation);
	Help = "Set time dilation for recording (0.1 to 10.0, default 1.0)";
	CommandDispatcher->BindCommand("vset /captureactor/time_dilation [float]", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FCaptureActorHandler::PrintAssetPool);
	Help = "Print all available assets in the asset pool (Category => Path pairs)";
	CommandDispatcher->BindCommand("vget /captureactor/asset_pool", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FCaptureActorHandler::StartSimpleRecording);
	Help = "Start simple recording: vset /captureactor/[id]/record [output_folder] [fps] [duration_seconds] [record_options]";
	Help += "\nRecord options (comma-separated): lit/rgb, mask/seg, normal, depth, optical_flow/flow,";
	Help += "\n                 oneobjmask, oneobjlit, oneobjgroomlit, shadowcatcher, stencilmask, metadata, audio, woTarget";
	Help += "\nExample: vset /captureactor/0/record ./output 30 10 lit,mask,oneobjlit,metadata";
	Help += "\n         vset /captureactor/0/record ./output 30 10 (empty for default: rgb only)";
	BindCommandDualCameraID("vset /captureactor/[camera_id]/record [str] [uint] [float]", Cmd, Help);
	BindCommandDualCameraID("vset /captureactor/[camera_id]/record [str] [uint] [float] [str]", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FCaptureActorHandler::IsRecording);
	Help = "Check if a camera is currently recording: vget /captureactor/[id]/is_recording";
	BindCommandDualCameraID("vget /captureactor/[camera_id]/is_recording", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FCaptureActorHandler::StopRecording);
	Help = "Stop recording for a camera: vset /captureactor/[id]/stop_record";
	BindCommandDualCameraID("vset /captureactor/[camera_id]/stop_record", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FCaptureActorHandler::GetUseMovieQualityRendering);
	Help = "Get bUseMovieQualityRendering (global setting)";
	CommandDispatcher->BindCommand("vget /captureactor/use_movie_quality_rendering", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FCaptureActorHandler::SetUseMovieQualityRendering);
	Help = "Set bUseMovieQualityRendering (global setting): 0 or 1";
	CommandDispatcher->BindCommand("vset /captureactor/use_movie_quality_rendering [uint]", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FCaptureActorHandler::GetRecordViaViewport);
	Help = "Get bRecordViaViewport (global setting)";
	CommandDispatcher->BindCommand("vget /captureactor/record_via_viewport", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FCaptureActorHandler::SetRecordViaViewport);
	Help = "Set bRecordViaViewport (global setting): 0 or 1";
	CommandDispatcher->BindCommand("vset /captureactor/record_via_viewport [uint]", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FCaptureActorHandler::GetVideoEncoderBitrate);
	Help = "Get video encoder bitrate settings (mean_mbps, max_mbps, quality)";
	CommandDispatcher->BindCommand("vget /captureactor/video_encoder_bitrate", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FCaptureActorHandler::SetVideoEncoderBitrate);
	Help = "Set video encoder bitrate (mean_mbps max_mbps quality): vset /captureactor/video_encoder_bitrate 35 60 100";
	CommandDispatcher->BindCommand("vset /captureactor/video_encoder_bitrate [uint] [uint] [uint]", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FCaptureActorHandler::GetH264Encoding);
	Help = "Get H264 encoding enabled state";
	CommandDispatcher->BindCommand("vget /captureactor/h264_encoding", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FCaptureActorHandler::SetH264Encoding);
	Help = "Set H264 encoding enabled: 0 or 1";
	CommandDispatcher->BindCommand("vset /captureactor/h264_encoding [uint]", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FCaptureActorHandler::GetAutoGenerateVideo);
	Help = "Get auto generate video after recording";
	CommandDispatcher->BindCommand("vget /captureactor/auto_generate_video", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FCaptureActorHandler::SetAutoGenerateVideo);
	Help = "Set auto generate video: 0 or 1";
	CommandDispatcher->BindCommand("vset /captureactor/auto_generate_video [uint]", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FCaptureActorHandler::GetWarmUpFrames);
	Help = "Get warm up frames count";
	CommandDispatcher->BindCommand("vget /captureactor/warmup_frames", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FCaptureActorHandler::SetWarmUpFrames);
	Help = "Set warm up frames count";
	CommandDispatcher->BindCommand("vset /captureactor/warmup_frames [uint]", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FCaptureActorHandler::GetVideoGenScriptPath);
	Help = "Get video generation script path";
	CommandDispatcher->BindCommand("vget /captureactor/video_gen_script_path", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FCaptureActorHandler::SetVideoGenScriptPath);
	Help = "Set video generation script path: vset /captureactor/video_gen_script_path [path/to/genvid.py]";
	CommandDispatcher->BindCommand("vset /captureactor/video_gen_script_path [str]", Cmd, Help);
}

FExecStatus FCaptureActorHandler::SpawnFreeCamera(const TArray<FString>& Args)
{
	UWorld* World = FUnrealcvServer::Get().GetWorld();
	if (!IsValid(World))
	{
		return FExecStatus::Error("Cannot get world");
	}

	int32 CameraID = URecordingBPLib::CreateFreeCamera(World, FVector::ZeroVector, FRotator::ZeroRotator);

	if (CameraID < 0)
	{
		return FExecStatus::Error("Failed to create free camera");
	}

	return FExecStatus::OK(FString::Printf(TEXT("%d"), CameraID));
}

FExecStatus FCaptureActorHandler::GetTimeDilation(const TArray<FString>& Args)
{
	float TimeDilation = URecordingBPLib::GetTimeDilation();
	return FExecStatus::OK(FString::Printf(TEXT("%.2f"), TimeDilation));
}

FExecStatus FCaptureActorHandler::SetTimeDilation(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		return FExecStatus::Error("Usage: vset /captureactor/time_dilation [float]");
	}

	float TimeDilation = FCString::Atof(*Args[0]);

	if (TimeDilation <= 0.0f)
	{
		return FExecStatus::Error("Time dilation must be positive");
	}

	URecordingBPLib::SetTimeDilation(TimeDilation);
	// return FExecStatus::OK(FString::Printf(TEXT("Time dilation set to %.2f"), URecordingBPLib::GetTimeDilation()));
	return FExecStatus::OK();
}

FExecStatus FCaptureActorHandler::PrintAssetPool(const TArray<FString>& Args)
{
	FAssetPoolManager& AssetPool = FAssetPoolManager::Get();
	AssetPool.PrintAssetPoolSummary();
	return FExecStatus::OK();
}

FExecStatus FCaptureActorHandler::StartSimpleRecording(const TArray<FString>& Args)
{
	if (Args.Num() < 4)
	{
		return FExecStatus::Error("Usage: vset /captureactor/[id]/record [filename] [fps] [duration_seconds] [record_options]");
	}

	FString IDString = Args[0];
	FString FileName = Args[1];
	int32 FPS = FCString::Atoi(*Args[2]);
	float DurationSeconds = FCString::Atof(*Args[3]);

	if (FPS <= 0)
	{
		return FExecStatus::Error(FString::Printf(TEXT("Invalid FPS: %d (must be > 0)"), FPS));
	}

	if (DurationSeconds <= 0.0f)
	{
		return FExecStatus::Error(FString::Printf(TEXT("Invalid duration: %.2f (must be > 0)"), DurationSeconds));
	}

	UFusionCamSensor* FusionCamSensor = USensorBPLib::GetSensorByAnyID(IDString);
	if (!IsValid(FusionCamSensor))
	{
		return FExecStatus::Error(FString::Printf(TEXT("Invalid camera ID: %s"), *IDString));
	}

	FRecordingDataTypesConfig RecordingConfig;
	if (Args.Num() >= 5 && !Args[4].IsEmpty())
	{
		RecordingConfig = FRecordingDataTypesConfig::ParseRecordingOptions(Args[4]);
	}
	else
	{
		RecordingConfig.bRecordRGB = true;
	}

	bool bSuccess = URecordingBPLib::StartSimpleRecording(IDString, FileName, FPS, DurationSeconds, RecordingConfig);

	if (bSuccess)
	{
		int32 TotalFrames = FMath::CeilToInt(FPS * DurationSeconds);
		FString RecordTypes = TEXT("(");
		if (RecordingConfig.bRecordRGB) RecordTypes += TEXT("rgb,");
		if (RecordingConfig.bRecordMask) RecordTypes += TEXT("mask,");
		if (RecordingConfig.bRecordNormal) RecordTypes += TEXT("normal,");
		if (RecordingConfig.bRecordDepth) RecordTypes += TEXT("depth,");
		if (RecordingConfig.bRecordFlow) RecordTypes += TEXT("flow,");
		if (RecordingConfig.bRecordOneObjectMask) RecordTypes += TEXT("oneobjmask,");
		if (RecordingConfig.bRecordOneObjectLit) RecordTypes += TEXT("oneobjlit,");
		if (RecordingConfig.bRecordOneObjectGroomLit) RecordTypes += TEXT("oneobjgroomlit,");
		if (RecordingConfig.bRecordShadowCatcher) RecordTypes += TEXT("shadowcatcher,");
		if (RecordingConfig.bRecordStencilMask) RecordTypes += TEXT("stencilmask,");
		if (RecordingConfig.bRecordMetadata) RecordTypes += TEXT("metadata,");
		if (RecordingConfig.bRecordAudio) RecordTypes += TEXT("audio,");
		if (RecordingConfig.bRecordWithoutTarget) RecordTypes += TEXT("woTarget,");
		if (RecordTypes.Len() > 1)
		{
			RecordTypes = RecordTypes.Left(RecordTypes.Len() - 1);
		}
		RecordTypes += TEXT(")");
		UE_LOG(LogUnrealCV, Warning, TEXT("Recording started: Camera %s, File: %s, FPS: %d, Frames: %d, Types: %s"),
			*IDString, *FileName, FPS, TotalFrames, *RecordTypes);
		return FExecStatus::OK();
	}
	else
	{
		return FExecStatus::Error(FString::Printf(TEXT("Failed to start recording for camera %s"), *IDString));
	}
}

FExecStatus FCaptureActorHandler::IsRecording(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		return FExecStatus::Error("Usage: vget /captureactor/[id]/is_recording");
	}

	FString IDString = Args[0];

	UFusionCamSensor* FusionCamSensor = USensorBPLib::GetSensorByAnyID(IDString);
	if (!IsValid(FusionCamSensor))
	{
		return FExecStatus::Error(FString::Printf(TEXT("Invalid camera ID: %s"), *IDString));
	}

	bool bIsRecording = URecordingBPLib::IsRecording(IDString);

	if (bIsRecording)
	{
		return FExecStatus::OK("true");
	}
	else
	{
		return FExecStatus::OK("false");
	}
}

FExecStatus FCaptureActorHandler::StopRecording(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		return FExecStatus::Error("Usage: vset /captureactor/[id]/stop_record");
	}

	FString IDString = Args[0];

	bool bSuccess = URecordingBPLib::StopRecording(IDString);

	if (bSuccess)
	{
		return FExecStatus::OK(FString::Printf(TEXT("Recording stopped for camera %s"), *IDString));
	}
	else
	{
		return FExecStatus::Error(FString::Printf(TEXT("Camera %s is not recording"), *IDString));
	}
}

FExecStatus FCaptureActorHandler::GetUseMovieQualityRendering(const TArray<FString>& Args)
{
	return FExecStatus::OK(AFusionCamCaptureActor::RecordingSettings.bUseMovieQualityRendering ? TEXT("1") : TEXT("0"));
}

FExecStatus FCaptureActorHandler::SetUseMovieQualityRendering(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		return FExecStatus::Error("Usage: vset /captureactor/use_movie_quality_rendering [0/1]");
	}

	int32 Value = FCString::Atoi(*Args[0]);
	AFusionCamCaptureActor::RecordingSettings.bUseMovieQualityRendering = (Value != 0);
	return FExecStatus::OK(FString::Printf(TEXT("bUseMovieQualityRendering = %d"), Value));
}

FExecStatus FCaptureActorHandler::GetRecordViaViewport(const TArray<FString>& Args)
{
	return FExecStatus::OK(AFusionCamCaptureActor::RecordingSettings.bRecordViaViewport ? TEXT("1") : TEXT("0"));
}

FExecStatus FCaptureActorHandler::SetRecordViaViewport(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		return FExecStatus::Error("Usage: vset /captureactor/record_via_viewport [0/1]");
	}

	int32 Value = FCString::Atoi(*Args[0]);
	AFusionCamCaptureActor::RecordingSettings.bRecordViaViewport = (Value != 0);
	return FExecStatus::OK(FString::Printf(TEXT("bRecordViaViewport = %d"), Value));
}

FExecStatus FCaptureActorHandler::GetVideoEncoderBitrate(const TArray<FString>& Args)
{
	uint32 MeanMbps = AFusionCamCaptureActor::RecordingSettings.VideoEncoder.MeanBitRate / (1024 * 1024);
	uint32 MaxMbps = AFusionCamCaptureActor::RecordingSettings.VideoEncoder.MaxBitRate / (1024 * 1024);
	uint32 Quality = AFusionCamCaptureActor::RecordingSettings.VideoEncoder.QualityVsSpeed;

	return FExecStatus::OK(FString::Printf(TEXT("mean_bps=%d max_bps=%d quality=%d"), MeanMbps, MaxMbps, Quality));
}

FExecStatus FCaptureActorHandler::SetVideoEncoderBitrate(const TArray<FString>& Args)
{
	if (Args.Num() < 2)
	{
		return FExecStatus::Error("Usage: vset /captureactor/video_encoder_bitrate [mean_mbps] [max_mbps] [quality]\nExample: vset /captureactor/video_encoder_bitrate 35 60 100");
	}

	uint32 MeanMbps = FCString::Atoi(*Args[0]);
	uint32 MaxMbps = FCString::Atoi(*Args[1]);
	uint32 Quality = Args.Num() >= 3 ? FCString::Atoi(*Args[2]) : 100;

	AFusionCamCaptureActor::RecordingSettings.VideoEncoder.MeanBitRate = MeanMbps * 1024 * 1024;
	AFusionCamCaptureActor::RecordingSettings.VideoEncoder.MaxBitRate = MaxMbps * 1024 * 1024;
	AFusionCamCaptureActor::RecordingSettings.VideoEncoder.QualityVsSpeed = Quality;

	return FExecStatus::OK(FString::Printf(TEXT("Video encoder bitrate updated: mean=%d Mbps, max=%d Mbps, quality=%d"), MeanMbps, MaxMbps, Quality));
}

FExecStatus FCaptureActorHandler::GetH264Encoding(const TArray<FString>& Args)
{
	return FExecStatus::OK(AFusionCamCaptureActor::RecordingSettings.bEnableH264Encoding ? TEXT("1") : TEXT("0"));
}

FExecStatus FCaptureActorHandler::SetH264Encoding(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		return FExecStatus::Error("Usage: vset /captureactor/h264_encoding [0/1]");
	}

	int32 Value = FCString::Atoi(*Args[0]);
	AFusionCamCaptureActor::RecordingSettings.bEnableH264Encoding = (Value != 0);
	return FExecStatus::OK(FString::Printf(TEXT("bEnableH264Encoding = %d"), Value));
}

FExecStatus FCaptureActorHandler::GetAutoGenerateVideo(const TArray<FString>& Args)
{
	return FExecStatus::OK(AFusionCamCaptureActor::RecordingSettings.bAutoGenerateVideo ? TEXT("1") : TEXT("0"));
}

FExecStatus FCaptureActorHandler::SetAutoGenerateVideo(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		return FExecStatus::Error("Usage: vset /captureactor/auto_generate_video [0/1]");
	}

	int32 Value = FCString::Atoi(*Args[0]);
	AFusionCamCaptureActor::RecordingSettings.bAutoGenerateVideo = (Value != 0);
	return FExecStatus::OK(FString::Printf(TEXT("bAutoGenerateVideo = %d"), Value));
}

FExecStatus FCaptureActorHandler::GetWarmUpFrames(const TArray<FString>& Args)
{
	return FExecStatus::OK(FString::Printf(TEXT("%d"), AFusionCamCaptureActor::RecordingSettings.WarmUpFrames));
}

FExecStatus FCaptureActorHandler::SetWarmUpFrames(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		return FExecStatus::Error("Usage: vset /captureactor/warmup_frames [uint]");
	}

	int32 Value = FCString::Atoi(*Args[0]);
	AFusionCamCaptureActor::RecordingSettings.WarmUpFrames = Value;
	return FExecStatus::OK(FString::Printf(TEXT("WarmUpFrames = %d"), Value));
}

FExecStatus FCaptureActorHandler::GetVideoGenScriptPath(const TArray<FString>& Args)
{
	return FExecStatus::OK(AFusionCamCaptureActor::RecordingSettings.VideoGenScriptPath);
}

FExecStatus FCaptureActorHandler::SetVideoGenScriptPath(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		return FExecStatus::Error("Usage: vset /captureactor/video_gen_script_path [path/to/genvid.py]");
	}

	AFusionCamCaptureActor::RecordingSettings.VideoGenScriptPath = Args[0];
	return FExecStatus::OK(FString::Printf(TEXT("VideoGenScriptPath = %s"), *Args[0]));
}
