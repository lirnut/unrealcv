// Copyright 2025 UnrealCV Team. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"

class FGenericTickableObject : public FTickableGameObject
{
public:
	using FTickCallback = TFunction<void(double)>;

	FGenericTickableObject();
	virtual ~FGenericTickableObject();

	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return bIsActive; }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(FGenericTickableObject, STATGROUP_Tickables); }

	void Activate();
	void Deactivate();
	void SetTickCallback(FTickCallback InCallback);

private:
	bool bIsActive = false;
	double LastRealTime = 0.0;
	FTickCallback TickCallback;
};
