#include "CollisionDetectableActor.h"
#include "Components/PrimitiveComponent.h"
#include "UnrealcvLog.h"

ACollisionDetectableActor::ACollisionDetectableActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ACollisionDetectableActor::BeginPlay()
{
	Super::BeginPlay();
}

void ACollisionDetectableActor::SetupCollisionDetection(UPrimitiveComponent* Component)
{
	if (!IsValid(Component) || !bEnableCollisionDetection)
	{
		return;
	}

	Component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Component->SetCollisionObjectType(ECC_PhysicsBody);
	Component->SetCollisionResponseToAllChannels(ECR_Block);
	Component->SetGenerateOverlapEvents(true);
	Component->SetNotifyRigidBodyCollision(true);

	Component->OnComponentBeginOverlap.AddDynamic(this, &ACollisionDetectableActor::OnOverlapBegin);
	Component->OnComponentEndOverlap.AddDynamic(this, &ACollisionDetectableActor::OnOverlapEnd);
	Component->OnComponentHit.AddDynamic(this, &ACollisionDetectableActor::OnHit);
}

void ACollisionDetectableActor::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!IsValid(OtherActor) || OtherActor == this)
	{
		return;
	}

	OverlappingActors.Add(OtherActor);

	if (bLogOverlapEvents)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("[%s] Overlap BEGIN with [%s] at location %s"),
			*GetName(), *OtherActor->GetName(), *SweepResult.Location.ToString());
	}
}

void ACollisionDetectableActor::OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!IsValid(OtherActor))
	{
		return;
	}

	OverlappingActors.Remove(OtherActor);

	if (bLogOverlapEvents)
	{
		UE_LOG(LogUnrealCV, Log, TEXT("[%s] Overlap END with [%s]"), *GetName(), *OtherActor->GetName());
	}
}

void ACollisionDetectableActor::OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!IsValid(OtherActor) || OtherActor == this)
	{
		return;
	}

	if (bLogHitEvents)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("[%s] HIT with [%s] at location %s, Normal: %s, Impulse: %s"),
			*GetName(), *OtherActor->GetName(), *Hit.Location.ToString(),
			*Hit.Normal.ToString(), *NormalImpulse.ToString());
	}
}

bool ACollisionDetectableActor::IsOverlappingAnyActor() const
{
	return OverlappingActors.Num() > 0;
}

void ACollisionDetectableActor::GetOverlappingActorNames(TArray<FString>& OutActorNames) const
{
	OutActorNames.Empty();
	for (AActor* Actor : OverlappingActors)
	{
		if (IsValid(Actor))
		{
			OutActorNames.Add(Actor->GetName());
		}
	}
}
