#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"

class UNREALCV_API FDeferredTaskScheduler : public FTickableGameObject
{
public:
	using FTaskCallback = TFunction<void()>;

	FDeferredTaskScheduler();
	virtual ~FDeferredTaskScheduler();

	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return PendingTasks.Num() > 0; }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(FDeferredTaskScheduler, STATGROUP_Tickables); }

	void ScheduleTask(FTaskCallback InCallback, float DelaySeconds);
	void ScheduleTaskForNextTick(FTaskCallback InCallback);
	void Clear();

	static FDeferredTaskScheduler& Get();

private:
	struct FScheduledTask
	{
		FTaskCallback Callback;
		double ExecutionTime;
		int32 FrameDelay;

		FScheduledTask(FTaskCallback InCallback, double InExecutionTime, int32 InFrameDelay = 0)
			: Callback(InCallback)
			, ExecutionTime(InExecutionTime)
			, FrameDelay(InFrameDelay)
		{}
	};

	TArray<FScheduledTask> PendingTasks;
	FCriticalSection TaskLock;
};
