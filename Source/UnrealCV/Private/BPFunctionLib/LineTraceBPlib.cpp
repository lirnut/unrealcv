#include "LineTraceBPlib.h"
#include "Engine/World.h"

FVector ULineTraceBPlib::SolveCameraSweepSlide(
	UObject* WorldContextObject,
	const FVector& Start,
	const FVector& End,
	float Radius,
	TEnumAsByte<ECollisionChannel> TraceChannel,
	bool& OutHit,
	FHitResult& OutHitResult
)
{
	OutHitResult = FHitResult();

	if (!WorldContextObject)
	{
		return End;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return End;
	}

	const FVector Delta = End - Start;
	if (Delta.IsNearlyZero())
	{
		return End;
	}

	FCollisionQueryParams Params;
	Params.bTraceComplex = true;

	FCollisionShape Shape = FCollisionShape::MakeSphere(Radius);

	bool bHit = World->SweepSingleByChannel(
		OutHitResult,
		Start,
		End,
		FQuat::Identity,
		TraceChannel,
		Shape,
		Params
	);

	if (!bHit)
	{
		return End;
	}
	OutHit = bHit;



	// Handle initial penetration
	if (OutHitResult.Time <= KINDA_SMALL_NUMBER)
	{
		return Start + OutHitResult.Normal * Radius;
	}

	// Move to impact point
	FVector SafeLocation = Start + Delta * OutHitResult.Time * 0.5f;

	// // Slide along surface
	// const FVector RemainingDelta = Delta * (1.0f - OutHitResult.Time);
	// const FVector SlideDelta =
	// 	FVector::VectorPlaneProject(RemainingDelta, OutHitResult.Normal);

	// return SafeLocation + SlideDelta;
	return SafeLocation;
}
