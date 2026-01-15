// Copyright 2025 UnrealCV Team. All Rights Reserved.
#include "Utils/GenericTickableObject.h"
#include "HAL/PlatformTime.h"

FGenericTickableObject::FGenericTickableObject()
	: bIsActive(false), LastRealTime(FPlatformTime::Seconds())
{
}

FGenericTickableObject::~FGenericTickableObject()
{
}

void FGenericTickableObject::Tick(float DeltaTime)
{
	if (!bIsActive || !TickCallback)
	{
		return;
	}

	double CurrentRealTime = FPlatformTime::Seconds();
	double RealDeltaTime = CurrentRealTime - LastRealTime;
	LastRealTime = CurrentRealTime;

	TickCallback(RealDeltaTime);
}

void FGenericTickableObject::Activate()
{
	bIsActive = true;
	LastRealTime = FPlatformTime::Seconds();
}

void FGenericTickableObject::Deactivate()
{
	bIsActive = false;
}

void FGenericTickableObject::SetTickCallback(FTickCallback InCallback)
{
	TickCallback = InCallback;
}
