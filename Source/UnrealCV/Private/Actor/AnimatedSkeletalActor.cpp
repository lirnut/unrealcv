#include "AnimatedSkeletalActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "UnrealcvLog.h"

AAnimatedSkeletalActor::AAnimatedSkeletalActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMeshComponent"));
	RootComponent = SkeletalMeshComponent;
	SkeletalMeshComponent->SetMobility(EComponentMobility::Movable);
}

void AAnimatedSkeletalActor::BeginPlay()
{
	Super::BeginPlay();

	if (bEnableCollisionDetection && IsValid(SkeletalMeshComponent))
	{
		SetupCollisionDetection(SkeletalMeshComponent);
	}

	if (IsValid(SkeletalMeshAsset) && IsValid(AnimSequenceAsset))
	{
		SkeletalMeshComponent->SetSkeletalMesh(SkeletalMeshAsset);
		SkeletalMeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		SkeletalMeshComponent->SetAnimation(AnimSequenceAsset);
		SkeletalMeshComponent->SetPlayRate(PlayRate);
		SkeletalMeshComponent->SetPosition(0.0f);
		SkeletalMeshComponent->Play(bLoopAnimation);
	}
}

void AAnimatedSkeletalActor::InitializeFromAssets(USkeletalMesh* InSkeletalMesh, UAnimSequence* InAnimSequence)
{
	if (!IsValid(InSkeletalMesh) || !IsValid(InAnimSequence))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("AAnimatedSkeletalActor::InitializeFromAssets: Invalid assets"));
		return;
	}

	SkeletalMeshAsset = InSkeletalMesh;
	AnimSequenceAsset = InAnimSequence;

	if (IsValid(SkeletalMeshComponent))
	{
		SkeletalMeshComponent->SetSkeletalMesh(InSkeletalMesh);
		SkeletalMeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		SkeletalMeshComponent->SetAnimation(InAnimSequence);
		SkeletalMeshComponent->SetPlayRate(PlayRate);
		SkeletalMeshComponent->SetPosition(0.0f);
		SkeletalMeshComponent->Play(bLoopAnimation);
	}
}
