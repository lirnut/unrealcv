// Weichao Qiu @ 2018
#pragma once

#include "Runtime/Engine/Classes/Kismet/BlueprintFunctionLibrary.h"
#include "AnimationBPLib.generated.h"

class AActor;
class UAnimInstance;
class USkeletalMeshComponent;

/** A static BP library to control the animation in UE4 */
UCLASS()
class UNREALCV_API UAnimationBPLib : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	static FString GetAnimationName(USkeletalMeshComponent* SkeletalMeshComponent);

	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	static TArray<UAnimationAsset*> GetAllAnimationOfSkeleton(USkeleton* Skeleton);

	UFUNCTION(BlueprintCallable, Category = "unrealcv")
	static TArray<UAnimSequence*> GetAllAnimationSequenceOfSkeleton(USkeleton* Skeleton);

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Animation")
	static bool SetActorAnimationBlueprint(AActor* Actor, FString AnimBlueprintPath);

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Animation")
	static bool SetSkeletalMeshAnimationBlueprint(USkeletalMeshComponent* SkeletalMeshComponent, FString AnimBlueprintPath);

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Animation")
	static UAnimInstance* GetCurrentAnimInstance(USkeletalMeshComponent* SkeletalMeshComponent);
};