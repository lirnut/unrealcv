#pragma once

#include "CoreMinimal.h"
#include "Async/AsyncWork.h"
#include "ImagePixelData.h"
#include "ImageWriteTypes.h"

struct FUnrealCVImageWriteTask
{
	TUniquePtr<FImagePixelData> PixelData;
	FString Filename;
	EImageFormat Format = EImageFormat::PNG;
	int32 CompressionQuality = 100;
	TFunction<void(bool)> OnCompleted;
};

class UNREALCV_API FImageWriteQueue
{
public:
	FImageWriteQueue();
	~FImageWriteQueue();

	void Enqueue(TUniquePtr<FUnrealCVImageWriteTask>&& Task);

	void Shutdown();

private:
	void ProcessTask(TUniquePtr<FUnrealCVImageWriteTask> Task);

	bool SaveImage(const TArray<FColor>& PixelData, int32 Width, int32 Height, const FString& Filename, EImageFormat Format, int32 CompressionQuality);

	FCriticalSection QueueLock;
	TArray<TUniquePtr<FUnrealCVImageWriteTask>> PendingTasks;
	std::atomic<bool> bIsShuttingDown;
};
