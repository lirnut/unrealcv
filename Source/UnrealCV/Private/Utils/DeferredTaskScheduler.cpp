#include "Utils/DeferredTaskScheduler.h"
#include "HAL/PlatformTime.h"

static FDeferredTaskScheduler* GDeferredTaskScheduler = nullptr;

FDeferredTaskScheduler::FDeferredTaskScheduler()
{
}

FDeferredTaskScheduler::~FDeferredTaskScheduler()
{
	Clear();
}

void FDeferredTaskScheduler::Tick(float DeltaTime)
{
	TArray<FTaskCallback> ReadyCallbacks;

	{
		FScopeLock Lock(&TaskLock);

		double CurrentTime = FPlatformTime::Seconds();

		UE_LOG(LogTemp, Warning, TEXT("FDeferredTaskScheduler::Tick, delta time %f"), DeltaTime)

		for (int32 i = PendingTasks.Num() - 1; i >= 0; --i)
		{
			FScheduledTask& Task = PendingTasks[i];

			if (Task.FrameDelay > 0)
			{
				Task.FrameDelay--;
				continue;
			}

			if (CurrentTime >= Task.ExecutionTime)
			{
				ReadyCallbacks.Add(MoveTemp(Task.Callback));
				PendingTasks.RemoveAtSwap(i);
			}
		}
	}

	for (FTaskCallback& Callback : ReadyCallbacks)
	{
		Callback();
	}
}

void FDeferredTaskScheduler::ScheduleTask(FTaskCallback InCallback, float DelaySeconds)
{
	FScopeLock Lock(&TaskLock);

	double ExecutionTime = FPlatformTime::Seconds() + DelaySeconds;
	PendingTasks.Emplace(InCallback, ExecutionTime, 0);
}

void FDeferredTaskScheduler::ScheduleTaskForNextTick(FTaskCallback InCallback)
{
	FScopeLock Lock(&TaskLock);

	PendingTasks.Emplace(InCallback, 0.0, 1);
}

void FDeferredTaskScheduler::Clear()
{
	FScopeLock Lock(&TaskLock);
	PendingTasks.Empty();
}

FDeferredTaskScheduler& FDeferredTaskScheduler::Get()
{
	if (!GDeferredTaskScheduler)
	{
		GDeferredTaskScheduler = new FDeferredTaskScheduler();
	}
	return *GDeferredTaskScheduler;
}
