#pragma once

#include "CoreMinimal.h"
#include "Actor/CollisionDetectableActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "AnimatedSkeletalActor.generated.h"

UCLASS()
class UNREALCV_API AAnimatedSkeletalActor : public ACollisionDetectableActor
{
	GENERATED_BODY()

public:
	AAnimatedSkeletalActor();

	virtual void BeginPlay() override;

	void InitializeFromAssets(USkeletalMesh* InSkeletalMesh, UAnimSequence* InAnimSequence);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh")
	USkeletalMeshComponent* SkeletalMeshComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	USkeletalMesh* SkeletalMeshAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	UAnimSequence* AnimSequenceAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	bool bLoopAnimation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	float PlayRate = 1.0f;
};
