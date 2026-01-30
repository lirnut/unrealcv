// shc @ 2025
// Custom SceneCaptureComponent2D that forces rendering in main renderer for maximum quality
#pragma once

#include "CoreMinimal.h"
#include "Components/SceneCaptureComponent2D.h"
#include "MainRendererSceneCaptureComponent2D.generated.h"

UCLASS(hidecategories=(Collision, Object, Physics, SceneComponent), ClassGroup=Rendering, editinlinenew, meta=(BlueprintSpawnableComponent))
class UNREALCV_API UMainRendererSceneCaptureComponent2D : public USceneCaptureComponent2D
{
	GENERATED_BODY()

public:
	UMainRendererSceneCaptureComponent2D(const FObjectInitializer& ObjectInitializer);

	virtual void OnRegister() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainRenderer")
	bool bForceMainRenderer = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainRenderer")
	bool bForceMainViewFamily = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainRenderer")
	bool bUseHighQualityFormat = true;
};
