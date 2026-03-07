// Weichao Qiu @ 2017
// Performance Optimization: shc @ 2025
#include "BaseCameraSensor.h"
#include "Runtime/Engine/Classes/Components/StaticMeshComponent.h"
#include "Runtime/Engine/Classes/Engine/CollisionProfile.h"
#include "Runtime/Engine/Classes/Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "TextureReader.h"
#include "UnrealcvServer.h"
#include "UnrealcvStats.h"
#include "UnrealcvLog.h"
#include "ImageUtil.h"
#include "RHIGPUReadback.h"
#include "RenderingThread.h"
#include "RHISurfaceDataConversionOpt.h"
#include "SetAlpha.h"
#include "BPFunctionLib/AnnotationBPLib.h"

DECLARE_CYCLE_STAT(TEXT("ReadBuffer"), STAT_ReadBuffer, STATGROUP_UnrealCV);
DECLARE_CYCLE_STAT(TEXT("ReadBufferFast"), STAT_ReadBufferFast, STATGROUP_UnrealCV);
// DECLARE_CYCLE_STAT(TEXT("ReadPixels"), STAT_ReadPixels, STATGROUP_UnrealCV);

// FImageWorker UBaseCameraSensor::ImageWorker;

UBaseCameraSensor::UBaseCameraSensor(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// BUG FIX: Skip initialization during async loading (non-game thread)
	// USceneCaptureComponent registers delegates in its constructor which requires game thread
	// This happens when AFusionCameraActor is loaded via FAsyncPackage2::EventDrivenCreateExport
	if (!IsInGameThread())
	{
		return;
	}

	// static ConstructorHelpers::FObjectFinder<UStaticMesh> EditorCameraMesh(TEXT("/Engine/EditorMeshes/MatineeCam_SM"));
	// Another choice is "StaticMesh'/Engine/EditorMeshes/Camera/SM_CineCam.SM_CineCam'"
	this->ShowFlags.SetPostProcessing(true);
	// this->ShowFlags.SetPostProcessMaterial(true);
	bCaptureEveryFrame = false;
	bCaptureOnMovement = false;
	PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;
	HiddenComponents.Reset();
	UAnnotationBPLib::GetAnnotationComponents(this->GetWorld(), HiddenComponents);
	CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	// CaptureSource = ESceneCaptureSource::SCS_FinalColorHDR;
	// CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
	bUseRayTracingIfEnabled = true;
	bAlwaysPersistRenderingState = true;

	// this->ShowFlags.SetAntiAliasing(true);
	// this->ShowFlags.SetTemporalAA(true);
	// this->ShowFlags.SetEyeAdaptation(false); // Eye adaption is a slow temporal procedure, not useful for image capture

    // this->ShowFlags.SetMotionBlur(false);
    // this->ShowFlags.SetDepthOfField(false);

    // this->ShowFlags.SetDynamicShadows(false);  // 如果不需要动态阴影
    // this->ShowFlags.SetBloom(false);           // 如果不需要泛光

	// bRenderInMainRenderer = true;  // optimization


	FServerConfig& Config = FUnrealcvServer::Get().Config;
	FilmWidth = Config.Width == 0 ? 640 : Config.Width;
	FilmHeight = Config.Height == 0 ? 480 : Config.Height;
	FOVAngle = Config.FOV == 0 ? 90 : Config.FOV;

	bUseFastCapture = Config.UseFastCapture;
	bAsyncCaptureNextFrame = true;
	// bUseFastCapture = true;
	// bool bSetLinearToGamma = false;
	// QueuedCaptures.Empty();
	InFlight = 0;
	MaxInFlight = 5;
}

void UBaseCameraSensor::PostInitProperties()
{
	Super::PostInitProperties();

	// BUG FIX: Complete initialization that was skipped during async loading
	// The constructor may have returned early if not on game thread
	if (!bCaptureEveryFrame && !bCaptureOnMovement)
	{
		// Already initialized in constructor (on game thread)
		return;
	}

	// Deferred initialization for async loaded objects
	this->ShowFlags.SetPostProcessing(true);
	bCaptureEveryFrame = false;
	bCaptureOnMovement = false;
	PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;
	HiddenComponents.Reset();
	UAnnotationBPLib::GetAnnotationComponents(this->GetWorld(), HiddenComponents);
	CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	bUseRayTracingIfEnabled = true;
	bAlwaysPersistRenderingState = true;

	FServerConfig& Config = FUnrealcvServer::Get().Config;
	FilmWidth = Config.Width == 0 ? 640 : Config.Width;
	FilmHeight = Config.Height == 0 ? 480 : Config.Height;
	FOVAngle = Config.FOV == 0 ? 90 : Config.FOV;

	bUseFastCapture = Config.UseFastCapture;
	bAsyncCaptureNextFrame = true;
	InFlight = 0;
	MaxInFlight = 5;
}

// Explicitly make a request to render frames
// This is needed if we want to disable bCaptureEveryFrame
// https://answers.unrealengine.com/questions/723947/scene-capture-with-post-process-mat-works-only-wit.html?sort=oldest

// if (GetOwner()) // Check whether this is a template project
// if (!IsTemplate())

// NOTE: Avoid creating TextureTarget in the CTOR, this will make CamSensor not savable in a BP actor
// TextureTarget = CreateDefaultSubobject<UTextureRenderTarget2D>(TEXT("CamSensorRenderTarget"));

void UBaseCameraSensor::InitTextureTarget(int filmWidth, int filmHeight)
{
	// this should be the same with https://github.com/unrealcv/unrealcv/blob/5.2/Source/UnrealCV/Private/Sensor/CameraSensor/BaseCameraSensor.cpp
	InitUInt8TextureTarget(filmWidth, filmHeight, false);

	// I set bUseLinearGamma to false, just because the default value is false in UnrealCV 5.2
	// But for RGB, It should be true to get a regular color
	// UE may do gamma correction twice if it is false
}

void UBaseCameraSensor::InitFloat16TextureTarget(int filmWidth, int filmHeight)
{

	// //PF_FloatRGBA            =10, // RGBA16F
	// EPixelFormat PixelFormat = EPixelFormat::PF_FloatRGBA;
	// TextureTarget = NewObject<UTextureRenderTarget2D>(this);
	// TextureTarget->InitCustomFormat(filmWidth, filmHeight, PixelFormat, bUseLinearGamma);
	// TextureTarget->InitAutoFormat(filmWidth, filmHeight);

	bool bUseLinearGamma = true;
	EPixelFormat PixelFormat = EPixelFormat::PF_FloatRGBA;
	TextureTarget = NewObject<UTextureRenderTarget2D>(this);
	TextureTarget->InitCustomFormat(filmWidth, filmHeight, PixelFormat, bUseLinearGamma);
	// TextureTarget->TargetGamma = GEngine->GetDisplayGamma();

	TextureTarget->bNoFastClear = true;
	TextureTarget->ClearColor = FLinearColor::Black;
}

void UBaseCameraSensor::InitUInt8TextureTarget(int filmWidth, int filmHeight, bool bUseLinearGamma)
{
	EPixelFormat PixelFormat = EPixelFormat::PF_B8G8R8A8;
	TextureTarget = NewObject<UTextureRenderTarget2D>(this);
	TextureTarget->InitCustomFormat(filmWidth, filmHeight, PixelFormat, bUseLinearGamma);
	// TextureTarget->TargetGamma = GEngine->GetDisplayGamma();

	TextureTarget->bNoFastClear = true;
	TextureTarget->ClearColor = FLinearColor::Black;
}

void UBaseCameraSensor::ConfigureMaxQualityLumen()
{
	// FPostProcessSettings& PPSettings = this->PostProcessSettings;
    // PPSettings.bOverride_ReflectionMethod = 1;
    // PPSettings.ReflectionMethod = EReflectionMethod::Type::Lumen;
    // // PPSettings.ReflectionMethod = EReflectionMethod::Type::ScreenSpace;
    // PPSettings.bOverride_DynamicGlobalIlluminationMethod = 1;
    // PPSettings.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::Type::Lumen;
    // // PPSettings.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::Type::ScreenSpace;
	// PPSettings.bOverride_LumenRayLightingMode = 1;
	// PPSettings.LumenRayLightingMode = ELumenRayLightingModeOverride::HitLightingForReflections;
	// PPSettings.bOverride_LumenSceneLightingQuality = 1;
	// PPSettings.LumenSceneLightingQuality = 4.0f;
	// PPSettings.bOverride_LumenSceneDetail = 1;
	// PPSettings.LumenSceneDetail = 4.0f;
	// // PPSettings.bOverride_LumenSceneViewDistance = 1;
	// // PPSettings.LumenSceneViewDistance = 2097152.0f;
	// PPSettings.bOverride_LumenFinalGatherQuality = 1;
	// PPSettings.LumenFinalGatherQuality = 5.0f;
	// PPSettings.bOverride_LumenFinalGatherScreenTraces = 1;
	// PPSettings.LumenFinalGatherScreenTraces = 1;
	// // PPSettings.bOverride_LumenMaxTraceDistance = 1;
	// // PPSettings.LumenMaxTraceDistance = 2097152.0f;
	// PPSettings.bOverride_LumenReflectionQuality = 1;
	// PPSettings.LumenReflectionQuality = 2.0f;
	// // PPSettings.bOverride_LumenReflectionsScreenTraces = 1;
	// // PPSettings.LumenReflectionsScreenTraces = 1;
	// // PPSettings.bOverride_LumenFrontLayerTranslucencyReflections = 1;
	// // PPSettings.LumenFrontLayerTranslucencyReflections = 1;
	// // PPSettings.bOverride_LumenMaxRoughnessToTraceReflections = 1;
	// // PPSettings.LumenMaxRoughnessToTraceReflections = 1.0f;
	// // PPSettings.bOverride_LumenMaxReflectionBounces = 1;
	// // PPSettings.LumenMaxReflectionBounces = 8;
	// // PPSettings.bOverride_LumenMaxRefractionBounces = 1;
	// // PPSettings.LumenMaxRefractionBounces = 64;
	// PPSettings.bOverride_LumenSurfaceCacheResolution = 1;
	// PPSettings.LumenSurfaceCacheResolution = 1.0f;

	// // PPSettings.bOverride_AmbientOcclusionQuality = true;
	// // PPSettings.AmbientOcclusionQuality = 80.0f;
	// // PPSettings.bOverride_RayTracingAO = true;
	// // PPSettings.RayTracingAO = true;
	// // PPSettings.bOverride_RayTracingAOSamplesPerPixel = true;
	// // PPSettings.RayTracingAOSamplesPerPixel = 4;

	// /////////////////////////////////////////////////////////
	// // reduce ghosting phenomenon
	// // https://www.reddit.com/r/UnrealEngine5/comments/182y8br/lumen_ghosting_on_moving_objects_please_help/
	// // https://forums.unrealengine.com/t/desperate-for-a-definitve-answer-on-lumen-ghosting-issue/661853
	// // https://dev.epicgames.com/community/learning/tutorials/mjo7/unreal-engine-temporal-quality-guide
	// // PPSettings.bOverride_LumenSceneLightingUpdateSpeed = 1;
	// // PPSettings.LumenSceneLightingUpdateSpeed = 2.0f;
	// // if (GlobalSettings.Override_LumenFinalGatherLightingUpdateSpeed)
	// // {
	// // 	PPSettings.bOverride_LumenFinalGatherLightingUpdateSpeed = 1;
	// // 	PPSettings.LumenFinalGatherLightingUpdateSpeed = GlobalSettings.LumenFinalGatherLightingUpdateSpeed;
	// // }
	// // PPSettings.bOverride_LumenFinalGatherScreenTraces = 1;
	// // PPSettings.LumenFinalGatherScreenTraces = 0;
	// // PPSettings.bOverride_AmbientOcclusionTemporalBlendWeight = 1;
	// // PPSettings.AmbientOcclusionTemporalBlendWeight = 0.0f;
	// /////////////////////////////////////////////////////////

	// // PPSettings.bOverride_AutoExposureMethod = 1;
	// // PPSettings.AutoExposureMethod = GlobalSettings.ExposureMethod;
	// // // PPSettings.AutoExposureMethod = AEM_Manual;
	// // PPSettings.bOverride_AutoExposureBias = 1;
	// // // PPSettings.AutoExposureBias = GlobalSettings.ExposureBias;
	// // PPSettings.AutoExposureBias = 15.0f;
	// // // PPSettings.bOverride_AutoExposureMinBrightness = 1;
	// // // PPSettings.AutoExposureMinBrightness = GlobalSettings.AutoExposureMinBrightness;
	// // // PPSettings.bOverride_AutoExposureMaxBrightness = 1;
	// // // PPSettings.AutoExposureMaxBrightness = GlobalSettings.AutoExposureMaxBrightness;
    // // PPSettings.bOverride_AutoExposureSpeedDown = 1;
    // // PPSettings.AutoExposureSpeedDown = 20.0f;
    // // PPSettings.bOverride_AutoExposureSpeedUp = 1;
    // // PPSettings.AutoExposureSpeedUp = 20.0f;

  	// // DOF
  	// PPSettings.bOverride_DepthOfFieldScale = true;
  	// PPSettings.DepthOfFieldScale = 0.0f;
  	// // PPSettings.bOverride_DepthOfFieldFstop = true;
  	// // PPSettings.DepthOfFieldFstop = 8.0f;
	// // PPSettings.bOverride_DepthOfFieldSensorWidth = true;
	// // PPSettings.DepthOfFieldSensorWidth = 35.0f;
    // // PPSettings.bOverride_DepthOfFieldFocalDistance = true;
    // // PPSettings.DepthOfFieldFocalDistance = 100.0f;
    // // PPSettings.bOverride_DepthOfFieldFocalRegion = true;
    // // PPSettings.DepthOfFieldFocalRegion = 2000.0f;

	// PPSettings.bOverride_MotionBlurAmount = 1;
	// PPSettings.bOverride_MotionBlurMax = 1;
	// PPSettings.bOverride_MotionBlurTargetFPS = 1;
	// PPSettings.bOverride_MotionBlurPerObjectSize = 1;
	// PPSettings.MotionBlurAmount = 0.05f;
	// PPSettings.MotionBlurMax = 2.0f;  // default 5.0
	// PPSettings.MotionBlurTargetFPS = 30;  // default 30
	// PPSettings.MotionBlurPerObjectSize = 0.f;
	// // PPSettings.MotionBlurAmount = 0.0f;  // default 0.5
	// // PPSettings.MotionBlurMax = 0.f;  // default 5.0
	// // PPSettings.MotionBlurTargetFPS = 24;  // default 30
	// // PPSettings.MotionBlurPerObjectSize = 0.f;
}

void UBaseCameraSensor::SetFilmSize(int Width, int Height)
{
	this->FilmWidth = Width;
	this->FilmHeight = Height;
	if (!IsValid(TextureTarget))
	{
		TextureTarget = NewObject<UTextureRenderTarget2D>(this); 
		// TextureTarget = CreateDefaultSubobject<UTextureRenderTarget2D>(TEXT("CamSensorRenderTarget"));
	}

	if (TextureTarget->SizeX != Width || TextureTarget->SizeY != Height) 
	{
		InitTextureTarget(Width, Height);
	}
}

int UBaseCameraSensor::GetFilmWidth()
{
	if (!IsValid(TextureTarget)) return 0;
	return TextureTarget->SizeX;
}

int UBaseCameraSensor::GetFilmHeight()
{
	if (!IsValid(TextureTarget)) return 0;
	return TextureTarget->SizeY;
}


// TODO: Split the logic, move data serialization code outside
// Serialize the data to png and npy, check the speed.

// EPixelFormat::PF_B8G8R8A8
/*
This is defined in FColor
	#ifdef _MSC_VER
	// Win32 x86
	union { struct{ uint8 B,G,R,A; }; uint32 AlignmentDummy; };
#else
	// Linux x86, etc
	uint8 B GCC_ALIGN(4);
	uint8 G,R,A;
*/
bool UBaseCameraSensor::CheckTextureTarget()
{
	if (!IsValid(TextureTarget))
	{
		UE_LOG(LogTemp, Warning, TEXT("The TextureTarget was not initialized."));
		return false;
	}
	if (TextureTarget->SizeX == 0 || TextureTarget->SizeY == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("The TextureTarget has invalid size."));
		return false;
	}
	return true;
}

void UBaseCameraSensor::Capture(TArray<FColor>& ImageData, int& Width, int& Height)
{
	if (!CheckTextureTarget())
	{
		UE_LOG(LogTemp, Error, TEXT("The TextureTarget was not initialized. Capture failed."));
		return;
	}

	if (bUseFastCapture)
	{
		CaptureFast(ImageData, Width, Height);
		return;
	}
	else
    {
		SCOPE_CYCLE_COUNTER(STAT_ReadBuffer);
		this->CaptureScene();

		ReadTextureRenderTarget(TextureTarget, ImageData, Width, Height);
	}
}

void UBaseCameraSensor::Capture(TArray<FFloat16Color>& ImageData, int& Width, int& Height)
{
	if (!CheckTextureTarget())
	{
		UE_LOG(LogTemp, Error, TEXT("The TextureTarget was not initialized. Capture failed."));
		return;
	}

	if (bUseFastCapture)
	{
		CaptureFast(ImageData, Width, Height);
		return;
	}
	else
    {
		SCOPE_CYCLE_COUNTER(STAT_ReadBuffer);
		this->CaptureScene();

		Width = TextureTarget->SizeX;
		Height = TextureTarget->SizeY;
		ImageData.Empty();
		ImageData.SetNumUninitialized(Width * Height);
		FTextureRenderTargetResource* RenderTargetResource = this->TextureTarget->GameThread_GetRenderTargetResource();
		RenderTargetResource->ReadFloat16Pixels(ImageData);
	}
}

// void UBaseCameraSensor::CaptureToFile(const FString& Filename)
// {
//     if (!CheckTextureTarget())
//     {
//         UE_LOG(LogTemp, Error, TEXT("TextureTarget not initialized, CaptureToFile failed."));
//         return;
//     }

//     this->CaptureScene();

//     FTextureRenderTargetResource* RenderTargetResource = TextureTarget->GameThread_GetRenderTargetResource();
//     int32 Width = TextureTarget->SizeX;
//     int32 Height = TextureTarget->SizeY;

//     FString OutputPath = Filename;

//     ENQUEUE_RENDER_COMMAND(CaptureToFileCommand)(
//         [RenderTargetResource, Width, Height, OutputPath](FRHICommandListImmediate& RHICmdList)
//         {
//             TArray<FColor> PixelData;
//             PixelData.AddUninitialized(Width * Height);

//             FReadSurfaceDataFlags ReadFlags(RCM_UNorm, CubeFace_MAX);
//             RHICmdList.ReadSurfaceData(
//                 RenderTargetResource->GetRenderTargetTexture(),
//                 FIntRect(0, 0, Width, Height),
//                 PixelData,
//                 ReadFlags
//             );

//             AsyncTask(ENamedThreads::AnyThread, [PixelData = MoveTemp(PixelData), Width, Height, OutputPath]()
//             {
//                 if (SerializeData(PixelData, Width, Height, OutputPath) == FExecStatusType::OK)
//                 {
//                     UE_LOG(LogTemp, Log, TEXT("[CaptureToFile] Saved async capture to %s"), *OutputPath);
//                 }
//                 else
//                 {
//                     UE_LOG(LogTemp, Error, TEXT("[CaptureToFile] Failed to save %s"), *OutputPath);
//                 }
//             });
//         }
//     );
// }



void UBaseCameraSensor::CaptureFastToFile(const FString& Filename)
{
	if (!CheckTextureTarget())
	{
		UE_LOG(LogTemp, Error, TEXT("The TextureTarget was not initialized. Capture failed."));
		return;
	}


	if (InFlight >= MaxInFlight)
	{
		UE_LOG(LogTemp, Warning, TEXT("CaptureFastToFile in flight N = %d, MaxInFlight = %d"), InFlight, MaxInFlight);

		// busy wait
		double WaitStartTime = FPlatformTime::Seconds();
		while (InFlight >= MaxInFlight && (FPlatformTime::Seconds() - WaitStartTime) < 0.5)
		{
			FPlatformProcess::Sleep(0.001f);
		}
	}
	InFlight++;

	CheckCaptureCache(ECaptureFormat::Invalid);

	if (!bCaptureLaunched)
	{
		LaunchCapture();
	}
	bCaptureLaunched = false;
	
	EPixelFormat PixelFormat = TextureTarget->GetFormat();
	UE_LOG(LogTemp, Warning, TEXT("[DEBUG] TextureTarget Format: %d, SRGB: %d, Gamma: %f"),
		(int32)PixelFormat,
		TextureTarget->SRGB,
		TextureTarget->TargetGamma);

	FTextureRenderTargetResource* RenderTargetResource = TextureTarget->GameThread_GetRenderTargetResource();
	int32 Width = TextureTarget->SizeX;
	int32 Height = TextureTarget->SizeY;

	// FQueuedCapture Capture;
	TSharedPtr<FQueuedCapture> Capture = MakeShared<FQueuedCapture>();
	Capture->Readback = new FRHIGPUTextureReadback(
		// random name
		*FString::Printf(TEXT("Capture_%d"), FMath::Rand())
	);
	Capture->OutputPath = Filename;
	Capture->Width = Width;
	Capture->Height = Height;
	Capture->PixelFormat = PixelFormat;

	ENQUEUE_RENDER_COMMAND(EnqueueGPUCopy)(
		[this, RenderTargetResource, Capture, Filename](FRHICommandListImmediate& RHICmdList)
		{
			// RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
			// RHICmdList.Transition(FRHITransitionInfo(Texture, ERHIAccess::SRVMask, ERHIAccess::CopySrc));
			Capture->Readback->EnqueueCopy(RHICmdList, RenderTargetResource->GetRenderTargetTexture());
			// RHICmdList.Transition(FRHITransitionInfo(Texture, ERHIAccess::CopySrc, ERHIAccess::SRVMask));
			// Capture->Readback->Wait(RHICmdList, FRHIGPUMask::GPU0());
    		RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);

			// static thread_local FRenderQueryPoolRHIRef RenderQueryPool =
			// 	RHICreateRenderQueryPool(RQT_AbsoluteTime);
			// auto Query = RenderQueryPool->AllocateQuery();
			// RHICmdList.EndRenderQuery(Query.GetQuery());
			// uint64 DeltaTime;
			// auto StartTime = FPlatformTime::Seconds();
			// RHIGetRenderQueryResult(Query.GetQuery(), DeltaTime, true);
			// Query.ReleaseQuery();
			// UE_LOG(LogTemp, Log, TEXT("[CaptureFastToFile] Copy Waiting time: %lf"), FPlatformTime::Seconds() - StartTime);

			// AsyncTask(ENamedThreads::AnyHiPriThreadHiPriTask,
			AsyncTask(ENamedThreads::AnyBackgroundHiPriTask,
				[this, Capture, Filename] (){ 
					auto StartTime = FPlatformTime::Seconds();
					while (!Capture->Readback->IsReady()) {
						FPlatformProcess::Sleep(0.001f);
					}
					InFlight--;
					UE_LOG(LogTemp, Log, TEXT("[CaptureFastToFile] Copy Waiting time: %lf"), FPlatformTime::Seconds() - StartTime);

					AsyncTask(ENamedThreads::GameThread,
						[Capture, Filename] (){ 

							auto StartTime = FPlatformTime::Seconds();
							int32 RowPitchInPixels;
							void* RawData = Capture->Readback->Lock(RowPitchInPixels);
							void* RawDataCopy = FMemory::Malloc(  RowPitchInPixels * Capture->Height * GPixelFormats[Capture->PixelFormat].BlockBytes);
							FMemory::BigBlockMemcpy(RawDataCopy, RawData, RowPitchInPixels * Capture->Height * GPixelFormats[Capture->PixelFormat].BlockBytes);
							Capture->Readback->Unlock();
							delete Capture->Readback;
							UE_LOG(LogTemp, Log, TEXT("[CaptureFastToFile] CPU Mem Copy time: %lf"), FPlatformTime::Seconds() - StartTime);

							AsyncTask(ENamedThreads::AnyBackgroundHiPriTask,
								[RawDataCopy, OutputPath = Capture->OutputPath, Width = Capture->Width, Height = Capture->Height, PixelFormat = Capture->PixelFormat, RowPitchInPixels = RowPitchInPixels]()
								{

									TArray<FColor> PixelData;
									PixelData.AddUninitialized(Width * Height);
									FReadSurfaceDataFlags ReadFlags(RCM_MinMax); // do not norm
									// FReadSurfaceDataFlags ReadFlags(RCM_UNorm); // norm
									ReadFlags.SetLinearToGamma(false);  // no gamma correction
									// ReadFlags.SetLinearToGamma(true);  // gamma correction

									uint32 SrcPitch = RowPitchInPixels * GPixelFormats[PixelFormat].BlockBytes;
									UE_LOG(LogTemp, Warning, TEXT("CaptureFastToFile: SrcPitch = %d, RowPitchInPixels = %d, BlockBytes = %d"), SrcPitch, RowPitchInPixels, GPixelFormats[PixelFormat].BlockBytes);
									ConvertRAWSurfaceDataToFColorOpt(
										PixelFormat,
										Width,
										Height,
										(uint8*)RawDataCopy,
										SrcPitch,
										PixelData.GetData(),
										ReadFlags
									);
									FMemory::Free(RawDataCopy);

									if (PixelFormat == EPixelFormat::PF_B8G8R8A8 && PixelData[0].A == 0)
									{
										SetAlpha(PixelData);
									}
									
									AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask,
										[PixelData = MoveTemp(PixelData), OutputPath , Width, Height]()
										{
											double SerializeStartTime = FPlatformTime::Seconds();
											SerializeData(PixelData, Width, Height, OutputPath);
											double SerializeTime = FPlatformTime::Seconds() - SerializeStartTime;
											UE_LOG(LogTemp, Log, TEXT("[CaptureToFile] Saved async capture to %s in %.3f ms"), *OutputPath, SerializeTime * 1000.0);
										}
									);
								}	
							);
						}
					);
				}
			);
		}
	);

	if (bAsyncCaptureNextFrame)
	{
		LaunchCapture();
	}
}

// void UBaseCameraSensor::CaptureFastToFile(const FString& Filename)
// {
// 	TArray<FColor> PixelData;
// 	int Width, Height;
// 	CaptureFast(PixelData, Width, Height);
// 	if (PixelData.Num() == Width * Height && Width > 0 && Height > 0)
// 	{
// 		AsyncTask(ENamedThreads::AnyThread,
// 			[PixelData = MoveTemp(PixelData), OutputPath = Filename, Width = Width, Height = Height]()
// 			{
// 				double SerializeStartTime = FPlatformTime::Seconds();
// 				SerializeData(PixelData, Width, Height, OutputPath);
// 				double SerializeTime = FPlatformTime::Seconds() - SerializeStartTime;
// 				UE_LOG(LogTemp, Log, TEXT("[CaptureToFile] Saved async capture to %s in %.3f ms"), *OutputPath, SerializeTime * 1000.0);
// 			}
// 		);
// 	}
// 	else
// 	{
// 		UE_LOG(LogTemp, Error, TEXT("UBaseCameraSensor::CaptureFastToFile: PixelData is empty or size not match"));
// 	}
// }

void UBaseCameraSensor::CaptureFast(TArray<FColor>& ImageData, int& Width, int& Height)
{
	if (!CheckTextureTarget())
	{
		UE_LOG(LogTemp, Error, TEXT("The TextureTarget was not initialized. Capture failed."));
		return;
	}

	CheckCaptureCache(ECaptureFormat::UInt8);

	double CaptureFastStartTime = FPlatformTime::Seconds();
	if (CopyFormat != ECaptureFormat::UInt8)
	{
		UE_LOG(LogTemp, Warning, TEXT("UBaseCameraSensor::CaptureFast: Copy not launched for UInt8, launch it"));
		LaunchCapture();
		CopyBackCapture(ECaptureFormat::UInt8);
		UE_LOG(LogTemp, Log, TEXT("CaptureFast: [X1] fallback start copy !"));
	}
	UE_LOG(LogTemp, Log, TEXT("CaptureFast: [X1] fallback start copy cost %.3f ms"), (FPlatformTime::Seconds() - CaptureFastStartTime) * 1000.0);
	bCaptureLaunched = false;

	// busy wait
	double WaitStartTime = FPlatformTime::Seconds();
	double CaptureCacheTimeOut = 5.0;
	while (!bCaptureCacheValid && (FPlatformTime::Seconds() - WaitStartTime) < CaptureCacheTimeOut)
	{
		FPlatformProcess::Sleep(0.00001f);
	}
	if (!bCaptureCacheValid)
	{
		UE_LOG(LogTemp, Error, TEXT("UBaseCameraSensor::CaptureFast: CaptureCache not valid, failed after %lf"), (FPlatformTime::Seconds() - WaitStartTime));
		CleanCaptureCache();
		CaptureScene();
		ReadTextureRenderTarget(TextureTarget, ImageData, Width, Height);
	}
	else 
	{
		UE_LOG(LogTemp, Log, TEXT("CaptureFast: [X2] wait cache %.3f ms"), (FPlatformTime::Seconds() - WaitStartTime) * 1000.0);

		// TArray<FColor> PixelData;
		double CopyStartTime = FPlatformTime::Seconds();
		if (CaptureCache.Num() == FilmWidth * FilmHeight)
		{
			ImageData = MoveTemp(CaptureCache);
			Width = FilmWidth;
			Height = FilmHeight;
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("UBaseCameraSensor::CaptureFast: CaptureCache size not match, failed"));
			check(false);
		}
		UE_LOG(LogTemp, Log, TEXT("CaptureFast: [X3] copy cache %.3f ms"), (FPlatformTime::Seconds() - CopyStartTime) * 1000.0);

		bCaptureCacheValid = false;
		CaptureCache = {};
	}

	if (bAsyncCaptureNextFrame)
	{
		double LaunchStartTime = FPlatformTime::Seconds();
		LaunchCapture();
		CopyBackCapture(ECaptureFormat::UInt8);
		UE_LOG(LogTemp, Log, TEXT("CaptureFast: [X4] launch capture and copy %.3f ms"), (FPlatformTime::Seconds() - LaunchStartTime) * 1000.0);
	}
}

void UBaseCameraSensor::CaptureFast(TArray<FFloat16Color>& ImageData, int& Width, int& Height)
{
	if (!CheckTextureTarget())
	{
		UE_LOG(LogTemp, Error, TEXT("The TextureTarget was not initialized. Capture failed."));
		return;
	}

	CheckCaptureCache(ECaptureFormat::F16);

	if (TextureTarget->GetFormat() != PF_FloatRGBA)
	{
		UE_LOG(LogTemp, Error, TEXT("UBaseCameraSensor::CaptureFast: TextureTarget format not PF_FloatRGBA, Go use CaptureFast FColor ver. or set the PixelFormat to PF_FloatRGBA"));
		return;
	}

	double CaptureFastStartTime = FPlatformTime::Seconds();
	if (CopyFormat != ECaptureFormat::F16)
	{
		UE_LOG(LogTemp, Warning, TEXT("UBaseCameraSensor::CaptureFast F16: Copy not launched for F16, launch it"));
		LaunchCapture();
		CopyBackCapture(ECaptureFormat::F16);
		UE_LOG(LogTemp, Log, TEXT("CaptureFast: [X1] fallback start copy !\n"));
	}
	UE_LOG(LogTemp, Log, TEXT("CaptureFast: [X1] fallback start copy cost %.3f ms"), (FPlatformTime::Seconds() - CaptureFastStartTime) * 1000.0);
	bCaptureLaunched = false;

	// busy wait
	double WaitStartTime = FPlatformTime::Seconds();
	while (!bCaptureCacheValid && (FPlatformTime::Seconds() - WaitStartTime) < 1.0)
	{
		FPlatformProcess::Sleep(0.00001f); // sleep 0.01 ms
	}
	if (!bCaptureCacheValid)
	{
		UE_LOG(LogTemp, Error, TEXT("UBaseCameraSensor::CaptureFast F16: CaptureCache not valid, failed"));
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("CaptureFast: [X2] wait cache %.3f ms"), (FPlatformTime::Seconds() - WaitStartTime) * 1000.0);

	double CopyStartTime = FPlatformTime::Seconds();
	if (CaptureCacheFloat16.Num() == FilmWidth * FilmHeight)
	{
		ImageData = MoveTemp(CaptureCacheFloat16);
		Width = FilmWidth;
		Height = FilmHeight;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("UBaseCameraSensor::CaptureFast F16: CaptureCache size not match, failed"));
	}
	UE_LOG(LogTemp, Log, TEXT("CaptureFast: [X3] copy cache %.3f ms"), (FPlatformTime::Seconds() - CopyStartTime) * 1000.0);

	bCaptureCacheValid = false;
	CaptureCacheFloat16 = {};

	if (bAsyncCaptureNextFrame)
	{
		double LaunchStartTime = FPlatformTime::Seconds();
		LaunchCapture();
		CopyBackCapture(ECaptureFormat::F16);
		UE_LOG(LogTemp, Log, TEXT("CaptureFast: [X4] launch capture and copy %.3f ms"), (FPlatformTime::Seconds() - LaunchStartTime) * 1000.0);
	}
}



void UBaseCameraSensor::SetPostProcessMaterial(TScriptInterface<IBlendableInterface> PostProcessMaterial)
{
	PostProcessSettings.WeightedBlendables.Array.Empty();
	PostProcessSettings.AddBlendable(PostProcessMaterial, 1);
	this->PostProcessBlendWeight = 1.0f;
}

void UBaseCameraSensor::GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView)
{
	DesiredView.Location = GetComponentLocation();
	DesiredView.Rotation = GetComponentRotation();
	DesiredView.FOV = this->FOVAngle;
	DesiredView.ProjectionMode = ECameraProjectionMode::Perspective;
	DesiredView.OrthoWidth = OrthoWidth;

	if (TextureTarget && TextureTarget->SizeX > 0 && TextureTarget->SizeY > 0)
	{
		DesiredView.AspectRatio = (float)TextureTarget->SizeX / (float)TextureTarget->SizeY;
		DesiredView.bConstrainAspectRatio = false;
		DesiredView.AspectRatioAxisConstraint = EAspectRatioAxisConstraint::AspectRatio_MaintainYFOV;
	}

	DesiredView.PostProcessBlendWeight = PostProcessBlendWeight;
	if (PostProcessBlendWeight > 0.0f)
	{
		DesiredView.PostProcessSettings = PostProcessSettings;
	}

	if (TextureTarget)
	{
		float ComputedAspectRatio = (float)TextureTarget->SizeX / (float)TextureTarget->SizeY;
		FMatrix ProjectionMatrix = DesiredView.CalculateProjectionMatrix();

		UE_LOG(LogUnrealCV, Warning, TEXT("BaseCameraSensor::GetCameraView CAMERA DEBUG:"));
		UE_LOG(LogUnrealCV, Warning, TEXT("  - Location: X=%.6f, Y=%.6f, Z=%.6f"), DesiredView.Location.X, DesiredView.Location.Y, DesiredView.Location.Z);
		UE_LOG(LogUnrealCV, Warning, TEXT("  - Rotation: P=%.6f, Y=%.6f, R=%.6f"), DesiredView.Rotation.Pitch, DesiredView.Rotation.Yaw, DesiredView.Rotation.Roll);
		UE_LOG(LogUnrealCV, Warning, TEXT("  - TextureTarget Size: %dx%d"), TextureTarget->SizeX, TextureTarget->SizeY);
		UE_LOG(LogUnrealCV, Warning, TEXT("  - FOV: %.6f"), DesiredView.FOV);
		UE_LOG(LogUnrealCV, Warning, TEXT("  - AspectRatio (from view): %.6f"), DesiredView.AspectRatio);
		UE_LOG(LogUnrealCV, Warning, TEXT("  - AspectRatio (computed): %.6f"), ComputedAspectRatio);
		UE_LOG(LogUnrealCV, Warning, TEXT("  - bConstrainAspectRatio: %d"), DesiredView.bConstrainAspectRatio);
		UE_LOG(LogUnrealCV, Warning, TEXT("  - AspectRatioAxisConstraint: %d"), DesiredView.AspectRatioAxisConstraint.Get(EAspectRatioAxisConstraint::AspectRatio_MaintainXFOV));
		UE_LOG(LogUnrealCV, Warning, TEXT("  - ProjectionMatrix M[0][0]: %.6f, M[0][1]: %.6f"), ProjectionMatrix.M[0][0], ProjectionMatrix.M[0][1]);
		UE_LOG(LogUnrealCV, Warning, TEXT("  - ProjectionMatrix M[1][0]: %.6f, M[1][1]: %.6f"), ProjectionMatrix.M[1][0], ProjectionMatrix.M[1][1]);

		float FovRad = FMath::DegreesToRadians(DesiredView.FOV);
		float HalfHeight = TextureTarget->SizeY * 0.5f;
		float HalfWidth = TextureTarget->SizeX * 0.5f;
		float Fy = HalfHeight / FMath::Tan(FovRad * 0.5f);
		float Fx = Fy / ComputedAspectRatio;
		float Cx = HalfWidth;
		float Cy = HalfHeight;
		UE_LOG(LogUnrealCV, Warning, TEXT("  - Intrinsics K: fx=%.2f, fy=%.2f, cx=%.2f, cy=%.2f"), Fx, Fy, Cx, Cy);
	}

}
void UBaseCameraSensor::ReadCaptureResults(TArray<FColor>& Data)
{
	FReadSurfaceDataFlags ReadSurfaceDataFlags;
	ReadSurfaceDataFlags.SetLinearToGamma(false);
	TextureTarget->GameThread_GetRenderTargetResource()->ReadPixels(Data, ReadSurfaceDataFlags);
	if (Data.Num() == 0)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("Captured lit data is empty."));
	}
}

void UBaseCameraSensor::SetShowOnlyList(const TArray<TWeakObjectPtr<UPrimitiveComponent>>& InShowOnlyComponents)
{
	if (PrimitiveRenderMode != ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("SetShowOnlyList: PrimitiveRenderMode not PRM_UseShowOnlyList, but setting ShowOnlyList !!!"));
		// UE_LOG(LogUnrealCV, Warning, TEXT("SetShowOnlyList: PrimitiveRenderMode set to PRM_UseShowOnlyList"));
		// PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	}

	ShowOnlyComponents.Reset();
	ShowOnlyComponents = InShowOnlyComponents;
}

// void UBaseCameraSensor::HideOneActor(AActor* Actor)
// {
// 	if (!IsValid(Actor))
// 	{
// 		UE_LOG(LogUnrealCV, Warning, TEXT("HideOneActor: Invalid actor"));
// 		return;
// 	}

// 	if ( PrimitiveRenderMode != ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives )
// 	{
// 		UE_LOG(LogUnrealCV, Warning, TEXT("HideOneActor: PrimitiveRenderMode not PRM_RenderScenePrimitives, but setting ShowOnlyList"));
// 		PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;
// 	}
// 	HiddenActors.Reset();
// 	HiddenActors.AddUnique(Actor);
// }

void UBaseCameraSensor::HideActor(AActor* Actor)
{
	if (!IsValid(Actor))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("HideOneActor: Invalid actor"));
		return;
	}

	if ( PrimitiveRenderMode != ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives )
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("HideOneActor: PrimitiveRenderMode not PRM_RenderScenePrimitives, but setting ShowOnlyList"));
	}
	if (!HiddenActors.Contains(Actor))
	{
		HiddenActors.AddUnique(Actor);
	}
}

void UBaseCameraSensor::ShowActor(AActor* Actor)
{
	if (!IsValid(Actor))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("HideOneActor: Invalid actor"));
		return;
	}

	if ( PrimitiveRenderMode != ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives )
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("HideOneActor: PrimitiveRenderMode not PRM_RenderScenePrimitives, but setting ShowOnlyList"));
	}
	if (HiddenActors.Contains(Actor))
	{
		HiddenActors.Remove(Actor);
	}
	// while (HiddenActors.Contains(Actor))
	// {
	// 	HiddenActors.Remove(Actor);
	// }
}

// void UBaseCameraSensor::ShowAllActors()
// {
// 	HiddenActors.Reset();
// }

void UBaseCameraSensor::LaunchCapture()
{
	if (!CheckTextureTarget())
	{
		UE_LOG(LogTemp, Error, TEXT("TextureTarget not initialized, CaptureToGPUQueue failed."));
		return;
	}
	this->CaptureScene();
	CaptureTimestamp = FPlatformTime::Seconds();
	bCaptureLaunched = true;
}

// void UBaseCameraSensor::CopyBackCapture(ECaptureFormat Format)
// {
// 	EPixelFormat PixelFormat = TextureTarget->GetFormat();
// 	UE_LOG(LogTemp, Warning, TEXT("[DEBUG] TextureTarget Format: %d, SRGB: %d, Gamma: %f"),
// 		(int32)PixelFormat,
// 		TextureTarget->SRGB,
// 		TextureTarget->TargetGamma);

// 	FTextureRenderTargetResource* RenderTargetResource = TextureTarget->GameThread_GetRenderTargetResource();
// 	int32 Width = TextureTarget->SizeX;
// 	int32 Height = TextureTarget->SizeY;

// 	FQueuedCapture Capture;
// 	Capture.Readback = new FRHIGPUTextureReadback(
// 		*FString::Printf(TEXT("Capture_%d"), FMath::Rand())
// 	);
// 	Capture.OutputPath = TEXT("");
// 	Capture.Width = Width;
// 	Capture.Height = Height;
// 	Capture.PixelFormat = PixelFormat;

// 	double RenderStartTime = FPlatformTime::Seconds();

// 	void* PixelDataPtr;
// 	if (Format == ECaptureFormat::F16)
// 	{
// 		CaptureCacheFloat16.Empty();
// 		CaptureCacheFloat16.AddUninitialized(Capture.Width * Capture.Height);
// 		PixelDataPtr = CaptureCacheFloat16.GetData();
// 	}
// 	else if (Format == ECaptureFormat::UInt8)
// 	{
// 		CaptureCache.Empty();
// 		CaptureCache.AddUninitialized(Capture.Width * Capture.Height);
// 		PixelDataPtr = CaptureCache.GetData();
// 	}
// 	else
// 	{
// 		UE_LOG(LogTemp, Error, TEXT("UBaseCameraSensor::CopyBackCapture: Invalid format, failed"));
// 		return;
// 	}
// 	bCaptureCacheValid = false;

// 	ENQUEUE_RENDER_COMMAND(EnqueueGPUCopy)(
// 		[RenderTargetResource, Readback = Capture.Readback, RenderStartTime](FRHICommandListImmediate& RHICmdList) mutable
// 		{
// 			UE_LOG(LogTemp, Log, "[R0] Start time: %.3f ms", (FPlatformTime::Seconds() - RenderStartTime) * 1000.0);

// 			double EnqueueStartTime = FPlatformTime::Seconds();
// 			Readback->EnqueueCopy(RHICmdList, RenderTargetResource->GetRenderTargetTexture());
// 			RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
// 			UE_LOG(LogTemp, Log, "[R2] EnqueueCopy time: %.3f ms", (FPlatformTime::Seconds() - EnqueueStartTime) * 1000.0);
// 		}
// 	);


// 	auto WaitStartTime = FPlatformTime::Seconds();
// 	while (!Capture.Readback->IsReady()) {
// 		FPlatformProcess::Sleep(0.001f);
// 	}
// 	UE_LOG(LogTemp, Log, "[R3] GPU Wait time: %.3f ms", (FPlatformTime::Seconds() - WaitStartTime) * 1000.0);

// 	double LockStartTime = FPlatformTime::Seconds();
// 	int32 RowPitchInPixels;
// 	const void* RawData = Capture.Readback->Lock(RowPitchInPixels);
// 	UE_LOG(LogTemp, Log, "[R4] Lock time: %.3f ms", (FPlatformTime::Seconds() - LockStartTime) * 1000.0);

// 	// Process data immediately on render thread to avoid accessing invalid memory


// // FReadSurfaceDataFlags ReadSurfaceDataFlags = bNormalize ? FReadSurfaceDataFlags() : FReadSurfaceDataFlags(RCM_MinMax);
// 	FReadSurfaceDataFlags ReadFlags(RCM_MinMax); // do not norm
// 	// FReadSurfaceDataFlags ReadFlags(RCM_UNorm); // norm
// 	ReadFlags.SetLinearToGamma(false);  // no gamma correction
// 	// ReadFlags.SetLinearToGamma(true);  // gamma correction

// 	uint32 SrcPitch = RowPitchInPixels * GPixelFormats[Capture.PixelFormat].BlockBytes;
// 	UE_LOG(LogTemp, Warning, TEXT("CopyBackCapture: SrcPitch = %d, RowPitchInPixels = %d, BlockBytes = %d"), SrcPitch, RowPitchInPixels, GPixelFormats[Capture.PixelFormat].BlockBytes);

// 	double ConvertStartTime = FPlatformTime::Seconds();
// 	if (Format == ECaptureFormat::UInt8)
// 	{
// 		ConvertRAWSurfaceDataToFColorOpt(
// 			Capture.PixelFormat,
// 			Capture.Width,
// 			Capture.Height,
// 			(uint8*)RawData,
// 			SrcPitch,
// 			(FColor*)PixelDataPtr,
// 			ReadFlags
// 		);
// 	}
// 	else if (Format == ECaptureFormat::F16)
// 	{
// 		ConvertRAWSurfaceDataToFFloat16ColorOpt(
// 			Capture.PixelFormat,
// 			Capture.Width,
// 			Capture.Height,
// 			(uint8*)RawData,
// 			SrcPitch,
// 			(FFloat16Color*)PixelDataPtr,
// 			ReadFlags
// 		);
// 	}
// 	else
// 	{
// 		UE_LOG(LogTemp, Error, TEXT("UBaseCameraSensor::CopyBackCapture: Invalid format, failed"));
// 		check(0);
// 	}
// 	bCaptureCacheValid = true;
// 	UE_LOG(LogTemp, Log, "[R5] ConvertRAWSurfaceData time: %.3f ms", (FPlatformTime::Seconds() - ConvertStartTime) * 1000.0);

// 	// Unlock the readback data
// 	double UnlockStartTime = FPlatformTime::Seconds();
// 	Capture.Readback->Unlock();
// 	delete Capture.Readback;
// 	UE_LOG(LogTemp, Log, "[R6] Unlock time: %.3f ms", (FPlatformTime::Seconds() - UnlockStartTime) * 1000.0);

// 	double TotalTime = FPlatformTime::Seconds() - RenderStartTime;
// 	UE_LOG(LogTemp, Log, "[R7] Total time: %.3f ms", TotalTime * 1000.0);

// 	CopyFormat = Format;
// }

void UBaseCameraSensor::CopyBackCapture(ECaptureFormat Format)
{
	EPixelFormat PixelFormat = TextureTarget->GetFormat();
	UE_LOG(LogTemp, Warning, TEXT("[DEBUG] TextureTarget Format: %d, SRGB: %d, Gamma: %f"),
		(int32)PixelFormat,
		TextureTarget->SRGB,
		TextureTarget->TargetGamma);

	FTextureRenderTargetResource* RenderTargetResource = TextureTarget->GameThread_GetRenderTargetResource();
	int32 Width = TextureTarget->SizeX;
	int32 Height = TextureTarget->SizeY;

	FQueuedCapture Capture;
	Capture.Readback = new FRHIGPUTextureReadback(
		*FString::Printf(TEXT("Capture_%d"), FMath::Rand())
	);
	Capture.OutputPath = TEXT("");
	Capture.Width = Width;
	Capture.Height = Height;
	Capture.PixelFormat = PixelFormat;

	double RenderStartTime = FPlatformTime::Seconds();

	void* PixelDataPtr;
	if (Format == ECaptureFormat::F16)
	{
		CaptureCacheFloat16.Empty();
		CaptureCacheFloat16.AddUninitialized(Capture.Width * Capture.Height);
		PixelDataPtr = CaptureCacheFloat16.GetData();
	}
	else if (Format == ECaptureFormat::UInt8)
	{
		CaptureCache.Empty();
		CaptureCache.AddUninitialized(Capture.Width * Capture.Height);
		PixelDataPtr = CaptureCache.GetData();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("UBaseCameraSensor::CopyBackCapture: Invalid format, failed"));
		return;
	}
	bCaptureCacheValid = false;


	ENQUEUE_RENDER_COMMAND(EnqueueGPUCopy)(
		[RenderTargetResource, Capture = MoveTemp(Capture), Format = Format, RenderStartTime, PixelData = PixelDataPtr, bCaptureCacheValidPtr = &bCaptureCacheValid](FRHICommandListImmediate& RHICmdList)
		{
			double EnqueueStartTime = FPlatformTime::Seconds();
			Capture.Readback->EnqueueCopy(RHICmdList, RenderTargetResource->GetRenderTargetTexture());
			UE_LOG(LogTemp, Log, TEXT("[R2] EnqueueCopy time: %.3f ms"), (FPlatformTime::Seconds() - EnqueueStartTime) * 1000.0);
			RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);

			auto WaitStartTime = FPlatformTime::Seconds();
			while (!Capture.Readback->IsReady()) {
				FPlatformProcess::Sleep(0.001f);
			}
			UE_LOG(LogTemp, Log, TEXT("[R2.5] GPU Wait time: %.3f ms"), (FPlatformTime::Seconds() - WaitStartTime) * 1000.0);

			double LockStartTime = FPlatformTime::Seconds();
			int32 RowPitchInPixels;
			const void* RawData = Capture.Readback->Lock(RowPitchInPixels);
			UE_LOG(LogTemp, Log, TEXT("[R3] Lock time: %.3f ms"), (FPlatformTime::Seconds() - LockStartTime) * 1000.0);

			// Process data immediately on render thread to avoid accessing invalid memory


	// FReadSurfaceDataFlags ReadSurfaceDataFlags = bNormalize ? FReadSurfaceDataFlags() : FReadSurfaceDataFlags(RCM_MinMax);
			FReadSurfaceDataFlags ReadFlags(RCM_MinMax); // do not norm
			// FReadSurfaceDataFlags ReadFlags(RCM_UNorm); // norm
			ReadFlags.SetLinearToGamma(false);  // no gamma correction
			// ReadFlags.SetLinearToGamma(true);  // gamma correction

			uint32 SrcPitch = RowPitchInPixels * GPixelFormats[Capture.PixelFormat].BlockBytes;
			UE_LOG(LogTemp, Warning, TEXT("CopyBackCapture: SrcPitch = %d, RowPitchInPixels = %d, BlockBytes = %d"), SrcPitch, RowPitchInPixels, GPixelFormats[Capture.PixelFormat].BlockBytes);

			double ConvertStartTime = FPlatformTime::Seconds();
			if (Format == ECaptureFormat::UInt8)
			{
				ConvertRAWSurfaceDataToFColorOpt(
					Capture.PixelFormat,
					Capture.Width,
					Capture.Height,
					(uint8*)RawData,
					SrcPitch,
					(FColor*)PixelData,
					ReadFlags
				);
			}
			else if (Format == ECaptureFormat::F16)
			{
				ConvertRAWSurfaceDataToFFloat16ColorOpt(
					Capture.PixelFormat,
					Capture.Width,
					Capture.Height,
					(uint8*)RawData,
					SrcPitch,
					(FFloat16Color*)PixelData,
					ReadFlags
				);
			} 
			else
			{
				UE_LOG(LogTemp, Error, TEXT("UBaseCameraSensor::CopyBackCapture: Invalid format, failed"));
				check(0);
			}
			*bCaptureCacheValidPtr = true;
			UE_LOG(LogTemp, Log, TEXT("[R5] ConvertRAWSurfaceData time: %.3f ms"), (FPlatformTime::Seconds() - ConvertStartTime) * 1000.0);

			// Unlock the readback data
			double UnlockStartTime = FPlatformTime::Seconds();
			Capture.Readback->Unlock();
			delete Capture.Readback;
			UE_LOG(LogTemp, Log, TEXT("[R6] Unlock time: %.3f ms"), (FPlatformTime::Seconds() - UnlockStartTime) * 1000.0);

			double TotalRenderTime = FPlatformTime::Seconds() - RenderStartTime;
			UE_LOG(LogTemp, Log, TEXT("[R7] Total render thread time: %.3f ms"), TotalRenderTime * 1000.0);

		}
	);


	CopyFormat = Format;
}

void UBaseCameraSensor::CleanCaptureCache()
{
	CaptureCache.Empty();
	CaptureCacheFloat16.Empty();
	bCaptureCacheValid = false;
	CopyFormat = ECaptureFormat::Invalid;
	bCaptureLaunched = false;
}

void UBaseCameraSensor::CheckCaptureCache(ECaptureFormat Format)
{
	bool bValid = true;
	if (Format != ECaptureFormat::Invalid)
	{
		bValid = CopyFormat == Format;
	}

	if (bCaptureCacheValid && CaptureTimestamp > 0.0 && bValid)
	{
		double ElapsedTime = FPlatformTime::Seconds() - CaptureTimestamp;
		if (ElapsedTime > 1.0)
		{
			UE_LOG(LogTemp, Warning, TEXT("UBaseCameraSensor::CheckCaptureCache: Cache expired (%.3f seconds), clearing"), ElapsedTime);
			CleanCaptureCache();
		}
	}
}

// void UBaseCameraSensor::ConvertCapture(TArray<FColor>& OutPixelData, int32& OutWidth, int32& OutHeight)
// {
// 	if (!bCaptureCacheValid)
// 	{
// 		UE_LOG(LogTemp, Error, TEXT("CaptureCache not valid, ConvertCapture failed."));
// 		return;
// 	}

// 	double WaitStartTime = FPlatformTime::Seconds();
// 	if (!CaptureCache.Readback->IsReady())
// 	{
// 		// busy wait
// 		while (!CaptureCache.Readback->IsReady() && (FPlatformTime::Seconds() - WaitStartTime) < 1.0)
// 		{
// 			FPlatformProcess::Sleep(0.0001f); // sleep 0.1 ms
// 		}

// 		if (!CaptureCache.Readback->IsReady())
// 		{
// 			UE_LOG(LogTemp, Error, TEXT("CaptureCache.Readback not ready after 1s, ConvertCapture failed."));
// 			return;
// 		}
// 	}
// 	UE_LOG(LogTemp, Warning, TEXT("Wait time: %.3f ms"), (FPlatformTime::Seconds() - WaitStartTime) * 1000.0);

// 	FQueuedCapture Capture = MoveTemp(CaptureCache);
// 	bCaptureCacheValid = false;

// 	double LockStartTime = FPlatformTime::Seconds();
// 	int32 RowPitchInPixels;
// 	const void* RawData = Capture.Readback->Lock(RowPitchInPixels);
// 	UE_LOG(LogTemp, Log, "[R3] Lock time: %.3f ms", (FPlatformTime::Seconds() - LockStartTime) * 1000.0);

// 	// Process data immediately on render thread to avoid accessing invalid memory
// 	double AllocStartTime = FPlatformTime::Seconds();
// 	OutPixelData.Empty();
// 	OutPixelData.AddUninitialized(Capture.Width * Capture.Height);
// 	UE_LOG(LogTemp, Log, "[R4] PixelData allocation time: %.3f ms", (FPlatformTime::Seconds() - AllocStartTime) * 1000.0);

// 	FReadSurfaceDataFlags ReadFlags;
// 	ReadFlags.SetLinearToGamma(false);

// 	uint32 SrcPitch = RowPitchInPixels * GPixelFormats[Capture.PixelFormat].BlockBytes;

// 	double ConvertStartTime = FPlatformTime::Seconds();
// 	ConvertRAWSurfaceDataToFColorOpt(
// 		Capture.PixelFormat,
// 		Capture.Width,
// 		Capture.Height,
// 		(uint8*)RawData,
// 		SrcPitch,
// 		OutPixelData.GetData(),
// 		ReadFlags
// 	);
// 	UE_LOG(LogTemp, Log, "[R5] ConvertRAWSurfaceData time: %.3f ms", (FPlatformTime::Seconds() - ConvertStartTime) * 1000.0);
	
// 	// Unlock the readback data
// 	double UnlockStartTime = FPlatformTime::Seconds();
// 	Capture.Readback->Unlock();
// 	UE_LOG(LogTemp, Log, "[R6] Unlock time: %.3f ms", (FPlatformTime::Seconds() - UnlockStartTime) * 1000.0);

// 	OutWidth = Capture.Width;
// 	OutHeight = Capture.Height;
// }

// void UBaseCameraSensor::CaptureToGPUQueue(const FString& Filename)
// {
// 	if (!CheckTextureTarget())
// 	{
// 		UE_LOG(LogTemp, Error, TEXT("TextureTarget not initialized, CaptureToGPUQueue failed."));
// 		return;
// 	}

// 	EPixelFormat PixelFormat = TextureTarget->GetFormat();
// 	UE_LOG(LogTemp, Warning, TEXT("[DEBUG] TextureTarget Format: %d, SRGB: %d, Gamma: %f"),
// 		(int32)PixelFormat,
// 		TextureTarget->SRGB,
// 		TextureTarget->TargetGamma);

// 	this->CaptureScene();

// 	FTextureRenderTargetResource* RenderTargetResource = TextureTarget->GameThread_GetRenderTargetResource();
// 	int32 Width = TextureTarget->SizeX;
// 	int32 Height = TextureTarget->SizeY;

// 	FQueuedCapture NewCapture;
// 	NewCapture.Readback = MakeUnique<FRHIGPUTextureReadback>(
// 		*FString::Printf(TEXT("QueuedCapture_%d"), QueuedCaptures.Num())
// 	);
// 	NewCapture.OutputPath = Filename;
// 	NewCapture.Width = Width;
// 	NewCapture.Height = Height;
// 	NewCapture.PixelFormat = PixelFormat;

// 	double RenderStartTime = FPlatformTime::Seconds();

// 	ENQUEUE_RENDER_COMMAND(EnqueueGPUCopy)(
// 		[RenderTargetResource, Capture = MoveTemp(NewCapture), RenderStartTime](FRHICommandListImmediate& RHICmdList)
// 		{
// 			UE_LOG(LogTemp, Log, "[R0] Start time: %.3f ms", (FPlatformTime::Seconds() - RenderStartTime) * 1000.0);
// 			double FlushStartTime = FPlatformTime::Seconds();
// 			RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
// 			UE_LOG(LogTemp, Log, "[R1] Flush time: %.3f ms", (FPlatformTime::Seconds() - FlushStartTime) * 1000.0);

// 			double EnqueueStartTime = FPlatformTime::Seconds();
// 			Capture.Readback->EnqueueCopy(RHICmdList, RenderTargetResource->GetRenderTargetTexture());
// 			UE_LOG(LogTemp, Log, "[R2] EnqueueCopy time: %.3f ms", (FPlatformTime::Seconds() - EnqueueStartTime) * 1000.0);

// 			double LockStartTime = FPlatformTime::Seconds();
// 			int32 RowPitchInPixels;
// 			const void* RawData = Capture.Readback->Lock(RowPitchInPixels);
// 			UE_LOG(LogTemp, Log, "[R3] Lock time: %.3f ms", (FPlatformTime::Seconds() - LockStartTime) * 1000.0);

// 			// Process data immediately on render thread to avoid accessing invalid memory
// 			double AllocStartTime = FPlatformTime::Seconds();
// 			TArray<FColor> PixelData;
// 			PixelData.AddUninitialized(Capture.Width * Capture.Height);
// 			UE_LOG(LogTemp, Log, "[R4] PixelData allocation time: %.3f ms", (FPlatformTime::Seconds() - AllocStartTime) * 1000.0);

// 			FReadSurfaceDataFlags ReadFlags;
// 			ReadFlags.SetLinearToGamma(false);

// 			uint32 SrcPitch = RowPitchInPixels * GPixelFormats[Capture.PixelFormat].BlockBytes;

// 			double ConvertStartTime = FPlatformTime::Seconds();
// 			ConvertRAWSurfaceDataToFColorOpt(
// 				Capture.PixelFormat,
// 				Capture.Width,
// 				Capture.Height,
// 				(uint8*)RawData,
// 				SrcPitch,
// 				PixelData.GetData(),
// 				ReadFlags
// 			);
// 			UE_LOG(LogTemp, Log, "[R5] ConvertRAWSurfaceData time: %.3f ms", (FPlatformTime::Seconds() - ConvertStartTime) * 1000.0);

// 			// Unlock the readback data
// 			double UnlockStartTime = FPlatformTime::Seconds();
// 			Capture.Readback->Unlock();
// 			UE_LOG(LogTemp, Log, "[R6] Unlock time: %.3f ms", (FPlatformTime::Seconds() - UnlockStartTime) * 1000.0);

// 			double TotalRenderTime = FPlatformTime::Seconds() - RenderStartTime;
// 			UE_LOG(LogTemp, Log, "[R7] Total render thread time: %.3f ms", TotalRenderTime * 1000.0);

// 			// Move to game thread for file I/O
// 			AsyncTask(ENamedThreads::AnyThread,
// 				[PixelData = MoveTemp(PixelData), OutputPath = Capture.OutputPath, Width = Capture.Width, Height = Capture.Height]()
// 				{
// 					double SerializeStartTime = FPlatformTime::Seconds();

// 					if (SerializeData(PixelData, Width, Height, OutputPath) == FExecStatusType::OK)
// 					{
// 						double SerializeTime = FPlatformTime::Seconds() - SerializeStartTime;
// 						UE_LOG(LogTemp, Log, "[A1] Saved %s in %.3f ms",
// 							TCHAR_TO_UTF8(*OutputPath), SerializeTime * 1000.0);
// 					}
// 					else
// 					{
// 						UE_LOG(LogTemp, Log, "[A1] Failed to save %s", TCHAR_TO_UTF8(*OutputPath));
// 					}
// 				});
// 		}
// 	);

// 	QueuedCaptures.Add(MoveTemp(NewCapture));

// 	UE_LOG(LogUnrealCV, Verbose, TEXT("CaptureToGPUQueue: Enqueued %s, total=%d"),
// 		*Filename, QueuedCaptures.Num());
// }

// void UBaseCameraSensor::FlushCapturesToDisk()
// {
// 	return;


















// 	if (QueuedCaptures.Num() == 0)
// 	{
// 		UE_LOG(LogUnrealCV, Warning, TEXT("FlushCapturesToDisk: No captures queued"));
// 		return;
// 	}

// 	int32 NumCaptures = QueuedCaptures.Num();
// 	double FlushStartTime = FPlatformTime::Seconds();
// 	UE_LOG(LogUnrealCV, Log, TEXT("FlushCapturesToDisk: Processing %d captures"), NumCaptures);

// 	TArray<FQueuedCapture> CapturesToFlush = MoveTemp(QueuedCaptures);
// 	QueuedCaptures.Empty();

// 	ENQUEUE_RENDER_COMMAND(FlushQueuedCaptures)(
// 		[CapturesToFlush = MoveTemp(CapturesToFlush), NumCaptures, FlushStartTime](FRHICommandListImmediate& RHICmdList) mutable
// 		{
// 			double RenderThreadStartTime = FPlatformTime::Seconds();
// 			double TotalWaitTime = 0.0;
// 			double TotalLockTime = 0.0;
// 			double TotalConversionTime = 0.0;
// 			int32 NumBlocked = 0;

// 			struct FLockedCaptureData
// 			{
// 				const void* RawData;
// 				int32 RowPitchInPixels;
// 				FString OutputPath;
// 				int32 Width;
// 				int32 Height;
// 				EPixelFormat PixelFormat;
// 			};

// 			TArray<FLockedCaptureData> LockedCaptures;
// 			LockedCaptures.Reserve(CapturesToFlush.Num());

// 			double LockStartTime = FPlatformTime::Seconds();

// 			for (int32 i = 0; i < CapturesToFlush.Num(); ++i)
// 			{
// 				auto& Capture = CapturesToFlush[i];

// 				if (!Capture.Readback->IsReady())
// 				{
// 					double WaitStartTime = FPlatformTime::Seconds();
// 					RHICmdList.BlockUntilGPUIdle();
// 					TotalWaitTime += FPlatformTime::Seconds() - WaitStartTime;
// 					NumBlocked++;
// 				}

// 				int32 RowPitchInPixels;
// 				const void* RawData = Capture.Readback->Lock(RowPitchInPixels);

// 				if (RawData)
// 				{
// 					FLockedCaptureData LockedData;
// 					LockedData.RawData = RawData;
// 					LockedData.RowPitchInPixels = RowPitchInPixels;
// 					LockedData.OutputPath = Capture.OutputPath;
// 					LockedData.Width = Capture.Width;
// 					LockedData.Height = Capture.Height;
// 					LockedData.PixelFormat = Capture.PixelFormat;
// 					LockedCaptures.Add(LockedData);
// 				}
// 			}

// 			TotalLockTime = FPlatformTime::Seconds() - LockStartTime;

// 			double ConversionStartTime = FPlatformTime::Seconds();

// 			TArray<TArray<FColor>> AllPixelData;
// 			AllPixelData.SetNum(LockedCaptures.Num());

// 			for (int32 i = 0; i < AllPixelData.Num(); ++i)
// 			{
// 				AllPixelData[i].SetNumUninitialized(LockedCaptures[i].Width * LockedCaptures[i].Height);
// 			}

// 			ParallelFor(LockedCaptures.Num(), [&](int32 i)
// 			{
// 				const FLockedCaptureData& LockedData = LockedCaptures[i];

// 				// FReadSurfaceDataFlags ReadFlags(RCM_UNorm, CubeFace_MAX);
// 				FReadSurfaceDataFlags ReadFlags;
// 				ReadFlags.SetLinearToGamma(false);

// 				uint32 SrcPitch = LockedData.RowPitchInPixels * GPixelFormats[LockedData.PixelFormat].BlockBytes;

// 				ConvertRAWSurfaceDataToFColorOpt(
// 					LockedData.PixelFormat,
// 					LockedData.Width,
// 					LockedData.Height,
// 					(uint8*)LockedData.RawData,
// 					SrcPitch,
// 					AllPixelData[i].GetData(),
// 					ReadFlags
// 				);
// 				// // AllPixelData[i].GetData()
// 				// for (int32 Index = 0; Index < AllPixelData[i].Num(); ++Index)
// 				// {
// 				// 	AllPixelData[i][Index] = AllPixelData[i][Index].ReinterpretAsLinear();
// 				// }
// 			});

// 			TotalConversionTime = FPlatformTime::Seconds() - ConversionStartTime;

// 			for (auto& Capture : CapturesToFlush)
// 			{
// 				Capture.Readback->Unlock();
// 			}

// 			for (int32 i = 0; i < LockedCaptures.Num(); ++i)
// 			{
// 				FString OutputPath = LockedCaptures[i].OutputPath;
// 				int32 Width = LockedCaptures[i].Width;
// 				int32 Height = LockedCaptures[i].Height;

// 				AsyncTask(ENamedThreads::AnyThread,
// 					[PixelData = MoveTemp(AllPixelData[i]), OutputPath, Width, Height]()
// 				{
// 					double SerializeStartTime = FPlatformTime::Seconds();

// 					if (SerializeData(PixelData, Width, Height, OutputPath) == FExecStatusType::OK)
// 					{
// 						double SerializeTime = FPlatformTime::Seconds() - SerializeStartTime;
// 						UE_LOG(LogTemp, Log, "[PERF] Saved %s in %.3f ms",
// 							TCHAR_TO_UTF8(*OutputPath), SerializeTime * 1000.0);
// 					}
// 					else
// 					{
// 						UE_LOG(LogTemp, Log, "[ERROR] Failed to save %s", TCHAR_TO_UTF8(*OutputPath));
// 					}
// 				});
// 			}

// 			double RenderThreadTotalTime = FPlatformTime::Seconds() - RenderThreadStartTime;
// 			double GameThreadTotalTime = FPlatformTime::Seconds() - FlushStartTime;

// 			UE_LOG(LogTemp, Log, "[PERF SUMMARY] FlushCapturesToDisk: %d captures", NumCaptures);
// 			UE_LOG(LogTemp, Log, "[PERF] Total game thread time: %.3f ms (%.3f ms/frame)",
// 				GameThreadTotalTime * 1000.0, (GameThreadTotalTime * 1000.0) / NumCaptures);
// 			UE_LOG(LogTemp, Log, "[PERF] Total render thread time: %.3f ms (%.3f ms/frame)",
// 				RenderThreadTotalTime * 1000.0, (RenderThreadTotalTime * 1000.0) / NumCaptures);
// 			UE_LOG(LogTemp, Log, "[PERF] GPU wait time: %.3f ms (%.3f ms/frame) - %d frames blocked",
// 				TotalWaitTime * 1000.0, NumBlocked > 0 ? (TotalWaitTime * 1000.0) / NumBlocked : 0.0, NumBlocked);
// 			UE_LOG(LogTemp, Log, "[PERF] Lock time (GPU->CPU DMA): %.3f ms (%.3f ms/frame)",
// 				TotalLockTime * 1000.0, (TotalLockTime * 1000.0) / NumCaptures);
// 			UE_LOG(LogTemp, Log, "[PERF] Pixel conversion time (ParallelFor): %.3f ms (%.3f ms/frame)",
// 				TotalConversionTime * 1000.0, (TotalConversionTime * 1000.0) / NumCaptures);
// 			UE_LOG(LogTemp, Log, "[PERF] Theoretical speedup: %.2fx (serial: %.3f ms -> parallel: %.3f ms)",
// 				(TotalLockTime + 7401.0) / (TotalLockTime + TotalConversionTime),
// 				7401.0, TotalConversionTime * 1000.0);
// 		}
// 	);
// }

// ============================================================================
// Anti-Aliasing Configuration Notes
// ============================================================================
//
// Current Configuration (Line 42-43):
//     this->ShowFlags.SetAntiAliasing(true);   // Enable AA (required)
//     this->ShowFlags.SetTemporalAA(false);    // Use FXAA (fastest)
//
// Performance: FXAA is the fastest AA method, suitable for high-throughput recording
// Quality: Good edge smoothing with minimal performance cost
//
// ============================================================================
// Alternative AA Methods (commented out for reference):
// ============================================================================
//
// Method 1: Temporal AA (Higher Quality, Slower)
// ----------------------------------------------
// Temporal AA provides better quality but requires multiple frames to accumulate
// and has higher performance cost. Not recommended for dataset recording.
//
//     this->ShowFlags.SetAntiAliasing(true);
//     this->ShowFlags.SetTemporalAA(true);     // Use Temporal AA instead of FXAA
//
// Performance Impact: ~15-30% slower than FXAA
// Quality: Best edge quality, reduces temporal aliasing
// Use Case: High-quality single-frame captures where performance is not critical
//
// ============================================================================
// Method 2: MSAA via RenderTarget (Not Available)
// ----------------------------------------------
// MSAA is not supported for UTextureRenderTarget2D in UE5.
// According to TextureRenderTarget2D.cpp:
//     ETextureRenderTargetSampleCount UTextureRenderTarget2D::GetSampleCount() const
//     {
//         // Note: MSAA is currently only supported in UCanvasRenderTarget2D
//         return ETextureRenderTargetSampleCount::RTSC_1;
//     }
//
// If MSAA were available, you would configure it like this:
//
//     // In InitTextureTarget() or InitUInt8TextureTarget():
//     TextureTarget = NewObject<UTextureRenderTarget2D>(this);
//     TextureTarget->InitCustomFormat(filmWidth, filmHeight, PixelFormat, bUseLinearGamma);
//     // TextureTarget->NumSamples = 4;  // NOT SUPPORTED - would set MSAA 4x
//
// Alternative: Use UCanvasRenderTarget2D if MSAA is required
//     UCanvasRenderTarget2D* CanvasRT = UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(...);
//     // Configure MSAA on CanvasRT (implementation details vary)
//
// Performance Impact: ~20-40% slower than FXAA (if it were available)
// Quality: Excellent edge quality, no temporal artifacts
// Memory Cost: 2x-8x more memory depending on sample count
//
// ============================================================================
// Method 3: No Anti-Aliasing (Fastest, Lowest Quality)
// ----------------------------------------------
// Disable AA completely for maximum performance
//
//     this->ShowFlags.SetAntiAliasing(false);
//
// Performance Impact: ~5-10% faster than FXAA
// Quality: Visible jagged edges, not recommended for production datasets
// Use Case: Debug/testing only
//
// ============================================================================
// Performance Comparison Summary:
// ============================================================================
// Method          | Relative Speed | Quality | Memory | Recommended Use
// ----------------|----------------|---------|--------|------------------
// No AA           | 100%           | Poor    | 1x     | Debug only
// FXAA (current)  | 95%            | Good    | 1x     | Production (default)
// Temporal AA     | 70-85%         | Best    | 1x     | High-quality captures
// MSAA 4x         | N/A            | N/A     | N/A    | Not supported
//
// ============================================================================
// Implementation Location:
// ============================================================================
// - Constructor (line 26-54): Initial ShowFlags configuration
// - ShowFlags are inherited from USceneCaptureComponent2D
// - ShowFlags.h (Engine): Full list of available flags
// - ShowFlagsValues.inl (Engine): Flag definitions
//
// ============================================================================