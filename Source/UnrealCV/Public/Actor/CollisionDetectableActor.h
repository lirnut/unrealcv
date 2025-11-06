#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CollisionDetectableActor.generated.h"

UCLASS(Abstract)
class UNREALCV_API ACollisionDetectableActor : public AActor
{
	GENERATED_BODY()

public:
	ACollisionDetectableActor();

	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision Detection")
	bool bEnableCollisionDetection = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision Detection")
	bool bLogOverlapEvents = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision Detection")
	bool bLogHitEvents = true;

	UFUNCTION(BlueprintCallable, Category = "Collision Detection")
	bool IsOverlappingAnyActor() const;

	UFUNCTION(BlueprintCallable, Category = "Collision Detection")
	void GetOverlappingActorNames(TArray<FString>& OutActorNames) const;

protected:
	void SetupCollisionDetection(UPrimitiveComponent* Component);

	UFUNCTION()
	virtual void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	virtual void OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION()
	virtual void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

	TSet<AActor*> OverlappingActors;
};
