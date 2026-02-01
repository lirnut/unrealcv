#include "ImageWriteQueue.h"
#include "ImageUtil.h"
#include "CameraSensor/SetAlpha.h"
#include "Async/Async.h"
#include "Misc/FileHelper.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"

FImageWriteQueue::FImageWriteQueue()
	: bIsShuttingDown(false)
{
}

FImageWriteQueue::~FImageWriteQueue()
{
	Shutdown();
}

void FImageWriteQueue::Enqueue(TUniquePtr<FUnrealCVImageWriteTask>&& Task)
{
	if (bIsShuttingDown)
	{
		if (Task->OnCompleted)
		{
			Task->OnCompleted(false);
		}
		return;
	}

	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask,
		[this, Task = MoveTemp(Task)]() mutable
		{
			ProcessTask(MoveTemp(Task));
		}
	);
}

void FImageWriteQueue::Shutdown()
{
	bIsShuttingDown = true;
}

void FImageWriteQueue::ProcessTask(TUniquePtr<FUnrealCVImageWriteTask> Task)
{
	if (!Task || !Task->PixelData.IsValid())
	{
		if (Task && Task->OnCompleted)
		{
			Task->OnCompleted(false);
		}
		return;
	}

	double StartTime = FPlatformTime::Seconds();

	TArray<FColor> PixelData;
	int32 Width = Task->PixelData->GetSize().X;
	int32 Height = Task->PixelData->GetSize().Y;

	if (Task->PixelData->GetType() == EImagePixelType::Color)
	{
		const TImagePixelData<FColor>* ColorData = static_cast<const TImagePixelData<FColor>*>(Task->PixelData.Get());
		PixelData = TArray<FColor>(ColorData->Pixels.GetData(), ColorData->Pixels.Num());
	}
	else if (Task->PixelData->GetType() == EImagePixelType::Float16)
	{
		const TImagePixelData<FFloat16Color>* Float16Data = static_cast<const TImagePixelData<FFloat16Color>*>(Task->PixelData.Get());
		PixelData.SetNum(Float16Data->Pixels.Num());
		for (int32 i = 0; i < Float16Data->Pixels.Num(); ++i)
		{
			FLinearColor LinearColor(
				Float16Data->Pixels[i].R.GetFloat(),
				Float16Data->Pixels[i].G.GetFloat(),
				Float16Data->Pixels[i].B.GetFloat(),
				Float16Data->Pixels[i].A.GetFloat()
			);
			PixelData[i] = LinearColor.ToFColor(false);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ImageWriteQueue: Unsupported pixel format"));
		if (Task->OnCompleted)
		{
			Task->OnCompleted(false);
		}
		return;
	}

	EPixelFormat PixelFormat = EPixelFormat::PF_B8G8R8A8;
	if (PixelFormat == EPixelFormat::PF_B8G8R8A8 && PixelData.Num() > 0 && PixelData[0].A == 0)
	{
		SetAlphaAVX2(PixelData);
	}

	bool bSuccess = SaveImage(PixelData, Width, Height, Task->Filename, Task->Format, Task->CompressionQuality);

	double SerializeTime = FPlatformTime::Seconds() - StartTime;
	if (bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("[ImageWriteQueue] Saved %s in %.3f ms"), *Task->Filename, SerializeTime * 1000.0);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[ImageWriteQueue] Failed to save %s"), *Task->Filename);
	}

	if (Task->OnCompleted)
	{
		AsyncTask(ENamedThreads::GameThread, [OnCompleted = Task->OnCompleted, bSuccess]()
		{
			OnCompleted(bSuccess);
		});
	}
}

bool FImageWriteQueue::SaveImage(const TArray<FColor>& PixelData, int32 Width, int32 Height, const FString& Filename, EImageFormat Format, int32 CompressionQuality)
{
	if (PixelData.Num() == 0 || PixelData.Num() != Width * Height)
	{
		return false;
	}

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(Format);

	if (!ImageWrapper.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("ImageWriteQueue: Failed to create image wrapper for format %d"), (int32)Format);
		return false;
	}

	ImageWrapper->SetRaw(PixelData.GetData(), PixelData.GetAllocatedSize(), Width, Height, ERGBFormat::BGRA, 8);
	TArray64<uint8> CompressedData = ImageWrapper->GetCompressed(CompressionQuality);

	if (CompressedData.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("ImageWriteQueue: Failed to compress image data"));
		return false;
	}

	return FFileHelper::SaveArrayToFile(CompressedData, *Filename);
}
