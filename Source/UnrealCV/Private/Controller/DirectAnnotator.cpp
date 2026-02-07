#include "Controller/DirectAnnotator.h"
#include "Runtime/Engine/Public/EngineUtils.h"
#include "Runtime/Engine/Classes/Components/PrimitiveComponent.h"
#include "UnrealcvLog.h"
#include "Controller/ObjectAnnotator.h"
#include "BPFunctionLib/StencilBPLib.h"

FDirectAnnotator::FDirectAnnotator()
{
	ColorGenerator = MakeShared<FColorGenerator>();
}

FDirectAnnotator::~FDirectAnnotator()
{
}

void FDirectAnnotator::AnnotateWorld(UWorld* World)
{
	if (!IsValid(World))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("[DirectAnnotator] Cannot annotate world, the world is not valid"));
		return;
	}

	TArray<AActor*> ActorArray;
	GetAnnotableActors(World, ActorArray);

	UE_LOG(LogUnrealCV, Log, TEXT("[DirectAnnotator] Annotating %d actors using CustomPrimitiveData"), ActorArray.Num());

	for (int32 i = 0; i < ActorArray.Num(); ++i)
	{
		AActor* Actor = ActorArray[i];
		if (!IsValid(Actor))
		{
			continue;
		}

		FColor AnnotationColor = GetDefaultColor(Actor);
		SetAnnotationColor(Actor, AnnotationColor);
		UStencilBPLib::EnableCustomDepthForActor(Actor, Actor->GetUniqueID());
	}

	UE_LOG(LogUnrealCV, Log, TEXT("[DirectAnnotator] Annotated %d actors, %d unique colors"),
		ActorArray.Num(), AnnotationColors.Num());
}

void FDirectAnnotator::DeannotateWorld(UWorld* World)
{
	if (!IsValid(World))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("[DirectAnnotator] Cannot deannotate world, the world is not valid"));
		return;
	}

	int32 ClearedCount = 0;

	for (TActorIterator<AActor> ActorItr(World); ActorItr; ++ActorItr)
	{
		AActor* Actor = *ActorItr;
		if (!IsValid(Actor))
		{
			continue;
		}

		TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
		Actor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

		for (UPrimitiveComponent* Primitive : PrimitiveComponents)
		{
			if (IsValid(Primitive))
			{
				FVector4 ZeroData(0.0f, 0.0f, 0.0f, 0.0f);
				Primitive->SetCustomPrimitiveDataVector4(4, ZeroData);
				++ClearedCount;
			}
		}

		// UStencilBPLib::DisableCustomDepthForActor(Actor);
	}

	AnnotationColors.Empty();

	UE_LOG(LogUnrealCV, Log, TEXT("[DirectAnnotator] Cleared annotation data from %d primitives"), ClearedCount);
}

int32 FDirectAnnotator::SetAnnotationColor(AActor* Actor, const FColor& AnnotationColor)
{
	if (!IsValid(Actor))
	{
		return 0;
	}

	int32 AnnotatedCount = 0;
	int32 ActorID = Actor->GetUniqueID();

	TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
	Actor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	for (UPrimitiveComponent* Primitive : PrimitiveComponents)
	{
		if (IsValid(Primitive))
		{
			SetPrimitiveAnnotationData(Primitive, AnnotationColor, ActorID);
			++AnnotatedCount;
		}
	}

	if (AnnotatedCount > 0)
	{
		AnnotationColors.Emplace(Actor->GetName(), AnnotationColor);
	}

	return AnnotatedCount;
}

void FDirectAnnotator::GetAnnotationColor(AActor* Actor, FColor& AnnotationColor)
{
	if (!IsValid(Actor))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("[DirectAnnotator] Actor is invalid in GetAnnotationColor"));
		return;
	}

	FString ActorName = Actor->GetName();
	if (AnnotationColors.Contains(ActorName))
	{
		AnnotationColor = AnnotationColors[ActorName];
		return;
	}

	TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
	Actor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	if (PrimitiveComponents.Num() > 0 && IsValid(PrimitiveComponents[0]))
	{
		const auto& CustomData = PrimitiveComponents[0]->GetCustomPrimitiveData();
		AnnotationColor.R = FMath::RoundToInt(CustomData.Data[4]);
		AnnotationColor.G = FMath::RoundToInt(CustomData.Data[5]);
		AnnotationColor.B = FMath::RoundToInt(CustomData.Data[6]);
		AnnotationColor.A = 255;
	}
}

void FDirectAnnotator::GetAnnotableActors(UWorld* World, TArray<AActor*>& ActorArray)
{
	if (!IsValid(World))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("[DirectAnnotator] World is invalid in GetAnnotableActors"));
		return;
	}

	for (TActorIterator<AActor> ActorItr(World); ActorItr; ++ActorItr)
	{
		AActor* Actor = *ActorItr;
		ActorArray.Add(Actor);
	}
}

FColor FDirectAnnotator::GetDefaultColor(AActor* Actor)
{
	FString ActorName = Actor->GetName();
	if (AnnotationColors.Contains(ActorName))
	{
		return AnnotationColors[ActorName];
	}

	int ColorIndex = AnnotationColors.Num();
	FColor AnnotationColor = ColorGenerator->GetColorFromColorMap(ColorIndex);

	return AnnotationColor;
}

void FDirectAnnotator::SetPrimitiveAnnotationData(UPrimitiveComponent* Primitive, const FColor& AnnotationColor, int32 ActorID)
{
	if (!IsValid(Primitive))
	{
		return;
	}

	FVector4 AnnotationData(
		(float)AnnotationColor.R,
		(float)AnnotationColor.G,
		(float)AnnotationColor.B,
		(float)ActorID
	);

	Primitive->SetCustomPrimitiveDataVector4(4, AnnotationData);

	UE_LOG(LogUnrealCV, Log, TEXT("[DirectAnnotator] Set annotation data for %s: R=%d G=%d B=%d ActorID=%d"),
		*Primitive->GetName(), AnnotationColor.R, AnnotationColor.G, AnnotationColor.B, ActorID);
}
