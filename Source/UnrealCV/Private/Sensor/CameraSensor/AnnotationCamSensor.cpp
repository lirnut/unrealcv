// Weichao Qiu @ 2017
#include "AnnotationCamSensor.h"
#include "Runtime/Engine/Classes/Engine/TextureRenderTarget2D.h"
#include "Runtime/Engine/Classes/Engine/World.h"
#include "Runtime/CoreUObject/Public/UObject/UObjectHash.h"

#include "Component/AnnotationComponent.h"
#include "UnrealcvLog.h"
#include "Runtime/Core/Public/Async/ParallelFor.h"
#include "Serialization.h"
#include "ImageUtil.h"
#include "RHISurfaceDataConversionOpt.h"
#include "SetAlpha.h"

TMap<UWorld*, TArray<TWeakObjectPtr<UPrimitiveComponent>>> UAnnotationCamSensor::CachedAnnotationComponents;
TMap<UWorld*, int32> UAnnotationCamSensor::CachedWorldFrameNumbers;
bool UAnnotationCamSensor::bCacheEnabled = false;

UAnnotationCamSensor::UAnnotationCamSensor(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	this->PrimaryComponentTick.bCanEverTick = true;
	this->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;

	this->ShowFlags.SetMaterials(false); // Check AnnotationComponent.cpp::GetViewRelevance
	this->ShowFlags.SetLighting(false);
	this->ShowFlags.SetPostProcessing(false);
	this->ShowFlags.SetColorGrading(false);
	this->ShowFlags.SetTonemapper(false); // Important to disable this
	this->ShowFlags.SetAtmosphere(false);
	this->ShowFlags.SetFog(false);

	this->PostProcessSettings.bOverride_AutoExposureBias = true;
	this->PostProcessSettings.AutoExposureBias = 0; // Overwrite anychange in the scene.
	// Note: The "exposure compensation" in "PostProcessVolume3" in the RR map will destroy the color
	// Note: Saturate the color to 1. This is a mysterious behavior after tedious debug.
	// Note: and the PostProcessing false here seems obvious not enough.

	// Note: the default value of CaptureSource is FinalColorLDR, but the FinalColor might be impact by PostProcessing
	// Note: even though PostProcessing flag is disabled
	// Note: Another option is tuning the PostProcess option flag in the SceneCaptureComponent2D
	// Note: CaptureSource = ESceneCaptureSource::SCS_BaseColor;

	// Note: The generated image is completely dark, which might be caused be the PixelFormat
	// Note: After a lot of trial and error, this is a solution that can work.
}


// this->ShowFlags.SetBSPTriangles(true);
// this->ShowFlags.SetVertexColors(true);
// this->ShowFlags.SetHMDDistortion(false);
// this->ShowFlags.SetTonemapper(false); // This won't take effect here
// GVertexColorViewMode = EVertexColorViewMode::Color;


// Only available for 4.17+
// this->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
// TODO: Write an article about LinearGamma

void UAnnotationCamSensor::InitTextureTarget(int filmWidth, int filmHeight)
{
	InitUInt8TextureTarget(filmWidth, filmHeight, true);
}

void UAnnotationCamSensor::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction * T)
{
	Super::TickComponent(DeltaTime, TickType, T);
}

void UAnnotationCamSensor::GetAnnotationComponents(UWorld* World, TArray<TWeakObjectPtr<UPrimitiveComponent> >& ComponentList)
{
	if (!IsValid(World))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("Can not get AnnotationComponents, World is invalid"));
		return;
	}

	// Use cache if enabled and valid
	if (bCacheEnabled && CachedAnnotationComponents.Contains(World))
	{
		int32 CachedFrameNumber = CachedWorldFrameNumbers.FindRef(World);
		int32 CurrentFrameNumber = World->GetTimeSeconds() * 60; // Approximate frame number

		// Check if cache is still valid (within last 120 frames)
		if (CurrentFrameNumber - CachedFrameNumber < 120)
		{
			ComponentList = CachedAnnotationComponents.FindRef(World);

			// Validate cached components are still valid
			TArray<TWeakObjectPtr<UPrimitiveComponent>> ValidComponents;
			for (const TWeakObjectPtr<UPrimitiveComponent>& WeakComponent : ComponentList)
			{
				if (WeakComponent.IsValid())
				{
					ValidComponents.Add(WeakComponent);
				}
			}

			// Update cache with valid components
			if (ValidComponents.Num() != ComponentList.Num())
			{
				CachedAnnotationComponents.Add(World, ValidComponents);
				ComponentList = ValidComponents;
			}

			return;
		}
	}

	// Cache miss or expired, rebuild component list
	TArray<UObject*> UObjectList;
	bool bIncludeDerivedClasses = false;
	EObjectFlags ExclusionFlags = EObjectFlags::RF_ClassDefaultObject;

	GetObjectsOfClass(UAnnotationComponent::StaticClass(), UObjectList, bIncludeDerivedClasses, ExclusionFlags);

    TArray<TArray<UPrimitiveComponent*>> TempComponentLists;
    TempComponentLists.SetNum(UObjectList.Num());

    //initial every item in TempComponentLists
    for (int32 i = 0; i < UObjectList.Num(); ++i)
    {
        TempComponentLists[i].Empty();
    }
    ParallelFor(UObjectList.Num(), [&](int32 Index)
    {
        UObject* Object = UObjectList[Index];
        UPrimitiveComponent* Component = Cast<UPrimitiveComponent>(Object);

        if (Component && Component->GetWorld() == World)
        {
            TempComponentLists[Index].Add(Component);
        }
    });

    // Clear ComponentList to ensure it's empty before adding new elements
    ComponentList.Empty();

    for (const TArray<UPrimitiveComponent*>& TempComponentList : TempComponentLists)
    {
        for (UPrimitiveComponent* Component : TempComponentList)
        {
            TWeakObjectPtr<UPrimitiveComponent> WeakComponent = Component;
            ComponentList.Add(WeakComponent);
        }
    }

	// Update cache
	if (bCacheEnabled)
	{
		int32 CurrentFrameNumber = World->GetTimeSeconds() * 60;
		CachedAnnotationComponents.Add(World, ComponentList);
		CachedWorldFrameNumbers.Add(World, CurrentFrameNumber);
	}

	if (ComponentList.Num() == 0)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("No annotation in the scene to show, fall back to lit mode"));
	}
}

void UAnnotationCamSensor::SetCacheEnabled(bool bEnabled)
{
	bCacheEnabled = bEnabled;
	if (!bEnabled)
	{
		ClearCache();
	}
}

void UAnnotationCamSensor::ClearCache()
{
	CachedAnnotationComponents.Empty();
	CachedWorldFrameNumbers.Empty();
}

void UAnnotationCamSensor::CaptureSeg(TArray<FColor>& ImageData, int& Width, int& Height)
{
	TArray<TWeakObjectPtr<UPrimitiveComponent>> ComponentList;
	GetAnnotationComponents(this->GetWorld(), ComponentList);

	this->ShowOnlyComponents = ComponentList;

	Capture(ImageData, Width, Height);

    if (ImageData.Num() != 0)
    {
        if (Width > 0 && Height > 0 && static_cast<uint32>(Width * Height) == ImageData.Num())
        {
            SetAlphaAVX2(ImageData);
        }
        else
        {
            UE_LOG(LogUnrealCV, Warning, TEXT("Invalid Width or Height for ImageData in CaptureSeg"));
        }
    }
}

void UAnnotationCamSensor::CaptureSegToFile(const FString& Filename)
{
	if (!CheckTextureTarget())
	{
		UE_LOG(LogUnrealCV, Error, TEXT("TextureTarget not initialized, CaptureSegToFile failed."));
		return;
	}

	TArray<TWeakObjectPtr<UPrimitiveComponent>> ComponentList;
	GetAnnotationComponents(this->GetWorld(), ComponentList);
	this->ShowOnlyComponents = ComponentList;


	/*
	 ****************      Below is copied from BaseCameraSensor.cpp     ********************
	*/

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

	FQueuedCapture Capture;
	Capture.Readback = MakeShared<FRHIGPUTextureReadback>(
		// random name
		*FString::Printf(TEXT("Capture_%d"), FMath::Rand())
	);
	Capture.OutputPath = TEXT("");
	Capture.Width = Width;
	Capture.Height = Height;
	Capture.PixelFormat = PixelFormat;

	ENQUEUE_RENDER_COMMAND(EnqueueGPUCopy)(
		[RenderTargetResource, Capture = MoveTemp(Capture), Filename](FRHICommandListImmediate& RHICmdList)
		{
			RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
			// RHICmdList.Transition(FRHITransitionInfo(Texture, ERHIAccess::SRVMask, ERHIAccess::CopySrc));
			Capture.Readback->EnqueueCopy(RHICmdList, RenderTargetResource->GetRenderTargetTexture());
			// RHICmdList.Transition(FRHITransitionInfo(Texture, ERHIAccess::CopySrc, ERHIAccess::SRVMask));

			void* RawDataCopy = FMemory::Malloc(Capture.Width * Capture.Height * GPixelFormats[Capture.PixelFormat].BlockBytes);
			int32 RowPitchInPixels;
			{
				const void* RawData = Capture.Readback->Lock(RowPitchInPixels);
				FMemory::Memcpy(RawDataCopy, RawData, Capture.Width * Capture.Height * GPixelFormats[Capture.PixelFormat].BlockBytes);
				Capture.Readback->Unlock();
			}

			AsyncTask(ENamedThreads::AnyThread,
				[RawDataCopy, OutputPath = Filename, Width = Capture.Width, Height = Capture.Height, PixelFormat = Capture.PixelFormat, RowPitchInPixels = RowPitchInPixels]()
				{

					TArray<FColor> PixelData;
					PixelData.AddUninitialized(Width * Height);
					FReadSurfaceDataFlags ReadFlags(RCM_MinMax); // do not norm
					// FReadSurfaceDataFlags ReadFlags(RCM_UNorm); // norm
					ReadFlags.SetLinearToGamma(false);  // no gamma correction
					// ReadFlags.SetLinearToGamma(true);  // gamma correction

					uint32 SrcPitch = RowPitchInPixels * GPixelFormats[PixelFormat].BlockBytes;
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

					SetAlphaAVX2(PixelData);

					double SerializeStartTime = FPlatformTime::Seconds();
					SerializeData(PixelData, Width, Height, OutputPath);
					double SerializeTime = FPlatformTime::Seconds() - SerializeStartTime;
					UE_LOG(LogTemp, Log, TEXT("[CaptureToFile] Saved async capture to %s in %.3f ms"), *OutputPath, SerializeTime * 1000.0);
				}
			);
		}
	);

	LaunchCapture();
}

