#include "ImageWriteQueue.h"
#include "CameraSensor/SetAlpha.h"
#include "CameraSensor/RHISurfaceDataConversionOpt.h"
#include "Async/Async.h"
#include "Misc/FileHelper.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"

FUnrealCVImageWriteQueue::FUnrealCVImageWriteQueue()
	: bIsShuttingDown(false)
{
}

FUnrealCVImageWriteQueue::~FUnrealCVImageWriteQueue()
{
	Shutdown();
}

void FUnrealCVImageWriteQueue::Enqueue(TUniquePtr<FUnrealCVImageWriteTask>&& Task)
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

void FUnrealCVImageWriteQueue::Shutdown()
{
	bIsShuttingDown = true;
}

void FUnrealCVImageWriteQueue::ProcessTask(TUniquePtr<FUnrealCVImageWriteTask> Task)
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
	bool bSuccess = true;
	FImageView InView = Task->PixelData->GetImageView();
	InView.GammaSpace = EGammaSpace::Linear;

	TArray64<uint8> CompressedData;
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	ImageWrapperModule.CompressImage(CompressedData, Task->Format, InView, Task->CompressionQuality);

	if (CompressedData.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("ImageWriteQueue: Failed to compress image data"));
		bSuccess = false;
	}

	if (bSuccess)
	{
		bSuccess = FFileHelper::SaveArrayToFile(CompressedData, *Task->Filename);
	}

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

