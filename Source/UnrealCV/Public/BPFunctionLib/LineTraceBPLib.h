#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LineTraceBPLib.generated.h"

UCLASS()
class ULineTraceBPlib : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Sweep from Start to End with a sphere and compute a safe sliding location.
	 *
	 * @param WorldContextObject   World context
	 * @param Start                Start location
	 * @param End                  Desired target location
	 * @param Radius               Sphere radius for sweep
	 * @param TraceChannel         Collision channel
	 * @param OutHit               Optional hit result
	 * @return                     Final safe location (may be adjusted)
	 */
	UFUNCTION(BlueprintCallable, Category = "FusionCam|Collision", meta = (WorldContext = "WorldContextObject"))
	static FVector SolveCameraSweepSlide(
		UObject* WorldContextObject,
		const FVector& Start,
		const FVector& End,
		float Radius,
		TEnumAsByte<ECollisionChannel> TraceChannel,
		bool& OutHit,
		FHitResult& OutHitResult
	);
};
