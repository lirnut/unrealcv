// Weichao Qiu @ 2017
#include "Controller/ObjectAnnotator.h"
#include "Runtime/Engine/Public/EngineUtils.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Component/AnnotationComponent.h"
#include "UnrealcvServer.h"
#include "UnrealcvLog.h"
#include "UnrealcvLog.h"

// For UE4 < 17
// check https://github.com/unrealcv/unrealcv/blob/1369a72be8428547318d8a52ae2d63e1eb57a001/Source/UnrealCV/Private/Controller/ObjectAnnotator.cpp#L1


TMap<FString, FColor> FObjectAnnotator::AnnotationColors = {}; // Store annotation data
FColorGenerator FObjectAnnotator::ColorGenerator;


// FObjectAnnotator::FObjectAnnotator()
// {
// }

/** Annotate all static mesh in the world */
void FObjectAnnotator::AnnotateWorld(UWorld* World)
{
	if (!IsValid(World))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("Can not annotate world, the world is not valid"));
		return;
	}

	TArray<AActor*> ActorArray;
	GetAnnotableActors(World, ActorArray);

	// log all ActorArray
	UE_LOG(LogUnrealCV, Log, TEXT("ActorArray: "));
	for (int32 i = 0; i < ActorArray.Num(); ++i)
	{
		UE_LOG(LogUnrealCV, Log, TEXT("ActorArray[%d]: %s"), i, *ActorArray[i]->GetName());
	}

	// Batch annotation with GPU sync to prevent crashes in complex scenes
	// Process actors in chunks to avoid massive GPU resource allocation spike
	const int32 BatchSize = 1;  // Number of actors to annotate before GPU sync
	int32 ProcessedCount = 0;

	for (int32 i = 0; i < ActorArray.Num(); ++i)
	{
		AActor* Actor = ActorArray[i];
		if (!IsValid(Actor))
		{
			UE_LOG(LogUnrealCV, Warning, TEXT("Found invalid actor in AnnotateWorld"));
			continue;
		}

		// TArray<UActorComponent*> AnnotationComponents = Actor->K2_GetComponentsByClass(UAnnotationComponent::StaticClass());
		// if (AnnotationComponents.Num() == 0)
		{
			FColor AnnotationColor = GetDefaultColor(Actor);
			// Use VertexColor as annotation
			SetAnnotationColor(Actor, AnnotationColor);
			++ProcessedCount;
		}

		// Insert GPU sync after every batch to prevent accumulation of GPU commands
		if (ProcessedCount >= BatchSize && i < ActorArray.Num() - 1)
		{
			FlushRenderingCommands();
			ProcessedCount = 0;
		}
	}

	// Final sync to ensure all annotations complete
	FlushRenderingCommands();

	UE_LOG(LogUnrealCV, Log, TEXT("Annotate mesh of the scene (%d actors processed, %d colors generated)"),
		ActorArray.Num(), AnnotationColors.Num());
}

void FObjectAnnotator::DeannotateWorld(UWorld* World)
{
	if (!IsValid(World))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("Can not deannotate world, the world is not valid"));
		return;
	}

	int32 DestroyedCount = 0;
	int32 ComponentCount = 0;

	for (TActorIterator<AActor> ActorItr(World); ActorItr; ++ActorItr)
	{
		AActor* Actor = *ActorItr;
		if (!IsValid(Actor))
		{
			continue;
		}

		TArray<UActorComponent*> AnnotationComponents = Actor->K2_GetComponentsByClass(UAnnotationComponent::StaticClass());
		ComponentCount += AnnotationComponents.Num();

		for (UActorComponent* Component : AnnotationComponents)
		{
			if (IsValid(Component))
			{
				// cast to UAnnotationComponent* to destroy the component
				UAnnotationComponent* AnnotationComponent = Cast<UAnnotationComponent>(Component);
				if (AnnotationComponent)
				{
					// AnnotationComponent->SetVisibility(false);
					// AnnotationComponent->MarkRenderStateDirty();
					// FlushRenderingCommands();
					AnnotationComponent->DestroyComponent();
					++DestroyedCount;
				}
				else
				{
					UE_LOG(LogUnrealCV, Warning, TEXT("DeannotateWorld: Can not cast to UAnnotationComponent*"));
				}
			}
		}
	}

	AnnotationColors.Empty();

	FlushRenderingCommands();

	UE_LOG(LogUnrealCV, Log, TEXT("Deannotate world completed (%d components destroyed out of %d found, annotation colors cleared)"),
		DestroyedCount, ComponentCount);
}


int32 FObjectAnnotator::SetAnnotationColor(AActor* Actor, const FColor& AnnotationColor)
{
	if (!IsValid(Actor))
	{
		return 0;
	}
	// CHECK: Add the annotation color regardless successful or not
	TArray<UActorComponent*> AnnotationComponents = Actor->K2_GetComponentsByClass(UAnnotationComponent::StaticClass());
	if (AnnotationComponents.Num() == 0)
	{
		CreateAnnotationComponent(Actor, AnnotationColor);
	}
	else
	{
		UpdateAnnotationComponent(Actor, AnnotationColor);
	}
	AnnotationColors.Emplace(Actor->GetName(), AnnotationColor);
	// TODO: Remote AnnotationColor Map!
	return AnnotationComponents.Num();
}

void FObjectAnnotator::GetAnnotationColor(AActor* Actor, FColor& AnnotationColor)
{
	if (!IsValid(Actor))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("InActor is invalid in GetAnnotationColor"));
		return;
	}

	// FString ActorName = Actor->GetName();
	// if (!this->AnnotationColors.Contains(ActorName))
	// {
	// 	UE_LOG(LogUnrealCV, Warning, TEXT("Can not find actor %s in GetAnnotationColor"), *ActorName);
	// 	return;
	// }
	// AnnotationColor = this->AnnotationColors[ActorName];

	// Another way to get annotation color is directly read color from AnnotationComponent
	// TODO: Remove the first only leave the second method

	// Check its direct children, do not recursive, otherwise it is very easy to trigger the warning.
	TArray<UActorComponent*> AnnotationComponents = Actor->K2_GetComponentsByClass(UAnnotationComponent::StaticClass());
	TArray<UActorComponent*> MeshComponents = Actor->K2_GetComponentsByClass(UMeshComponent::StaticClass());
	// Note: Strange that the MeshComponents.Num() is twice the number of AnnotationComponents.Num()
	if (AnnotationComponents.Num() == 0) return;
	if (AnnotationComponents.Num() != MeshComponents.Num())
	{
		// UE_LOG(LogTemp, Warning, TEXT("More than one AnnotationComponent for MeshComponent."));
		UE_LOG(LogTemp, Warning, TEXT("In actor %s, the number of MeshComponent (%d) and AnnotationComponent (%d) is different."), *Actor->GetName(), MeshComponents.Num(), AnnotationComponents.Num());
		// for (UActorComponent* Component : MeshComponents)
		// {
		// 	UE_LOG(LogTemp, Warning, TEXT("%s"), *Component->GetName()); 
		// }
	}
	UAnnotationComponent* AnnotationComponent = Cast<UAnnotationComponent>(AnnotationComponents[0]);
	// check(AnnotationColor == AnnotationComponent->AnnotationColor);
	AnnotationColor = AnnotationComponent->GetAnnotationColor();
}

void FObjectAnnotator::GetAnnotableActors(UWorld* World, TArray<AActor*>& ActorArray)
{
	if (!IsValid(World))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("The world is invalid in GetAnnotableActors"));
		return;
	}

	for (TActorIterator<AActor> ActorItr(World); ActorItr; ++ActorItr)
	{
		AActor *Actor = *ActorItr;
		ActorArray.Add(Actor);
	}
}

/**
 * Debug tips:
 * AnnotationComponent->AnnotationColor = FColor::MakeRandomColor();
 */
void FObjectAnnotator::CreateAnnotationComponent(AActor* Actor, const FColor& AnnotationColor)
{
	// Two special type of actors
	// https://api.unrealengine.com/INT/API/Runtime/Landscape/ALandscape/index.html
	// https://api.unrealengine.com/INT/API/Runtime/Foliage/AInstancedFoliageActor/index.html
	if (!IsValid(Actor))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("Invalid actor in CreateAnnotationComponent"));
		return;
	}
	TArray<UActorComponent*> AnnotationComponents = Actor->K2_GetComponentsByClass(UAnnotationComponent::StaticClass());
	if (AnnotationComponents.Num() != 0)
	{
		UE_LOG(LogUnrealCV, Log, TEXT("Skip annotated actor %s"), *Actor->GetName());
		return;
	}

	TArray<UActorComponent*> MeshComponents = Actor->K2_GetComponentsByClass(UMeshComponent::StaticClass());
	if (MeshComponents.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("Annotate actor %s (%s) with color %s, MeshComponents: %d"), *Actor->GetActorNameOrLabel(), *Actor->GetName(), *AnnotationColor.ToString(), MeshComponents.Num());

		for (UActorComponent* Component : MeshComponents)
		{
			bool DisableSKMAnnotation = FUnrealcvServer::Get().Config.DisableSKMAnnotation;
			if (DisableSKMAnnotation)
			{
				if (Component->IsA<USkeletalMeshComponent>())
				{
					UE_LOG(LogUnrealCV, Log, TEXT("Skipping SkeletalMeshComponent annotation for %s (use Depth/Annotation cameras for skeletal meshes)"),
						*Actor->GetName());
					continue;
				}
			}

			UMeshComponent* MeshComponent = Cast<UMeshComponent>(Component);
			check(MeshComponent)

			UE_LOG(LogUnrealCV, Log, TEXT("  MeshComponent: %s, Class: %s"), *MeshComponent->GetName(), *MeshComponent->GetClass()->GetName());

			// bool bAllTransparentMaterial = true;
			// for (int ComponentMaterialIdx = 0; ComponentMaterialIdx < MeshComponent->GetNumMaterials(); ++ComponentMaterialIdx)
			// {
			// 	UMaterialInterface* MaterialInterface = MeshComponent->GetMaterial(ComponentMaterialIdx);
			// 	if (MaterialInterface != nullptr)
			// 	{
			// 		EBlendMode BlendMode = MaterialInterface->GetBlendMode();
			// 		// OneComponent->SetMaterial(ComponentMaterialIdx, DynamicMaterialInstance);
			// 		if (BlendMode != EBlendMode::BLEND_Translucent)
			// 		{
			// 			bAllTransparentMaterial = false;
			// 			break;
			// 		}
			// 	}
			// }

			// if (bAllTransparentMaterial)
			// {
			// 	UE_LOG(LogUnrealCV, Warning, TEXT("Skip annotation for mesh component %s, because it is all transparent material"), *MeshComponent->GetName());
			// 	continue;
			// }


			UAnnotationComponent* AnnotationComponent = NewObject<UAnnotationComponent>(MeshComponent);
			AnnotationComponent->SetupAttachment(MeshComponent);
			AnnotationComponent->RegisterComponent();

			// Set annotation color after the component is registered
			AnnotationComponent->SetAnnotationColor(AnnotationColor);

			// Mark dirty to update GPU resources
			AnnotationComponent->MarkRenderStateDirty();
		}
	}
}


void FObjectAnnotator::UpdateAnnotationComponent(AActor* Actor, const FColor& AnnotationColor)
{
	if (!IsValid(Actor))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("Invalid actor in CreateAnnotationComponent"));
		return;
	}
	TArray<UActorComponent*> AnnotationComponents = Actor->K2_GetComponentsByClass(UAnnotationComponent::StaticClass());
	for (UActorComponent* Component : AnnotationComponents)
	{
		UAnnotationComponent* AnnotationComponent = Cast<UAnnotationComponent>(Component);
		AnnotationComponent->SetAnnotationColor(AnnotationColor);
		AnnotationComponent->MarkRenderStateDirty();
	}
}

FColor FObjectAnnotator::GetDefaultColor(AActor* Actor)
{
	FString ActorName = Actor->GetName();
	if (AnnotationColors.Contains(ActorName))
	{
		// Already initialized
		return AnnotationColors[ActorName];
	}

	int ColorIndex = AnnotationColors.Num();
	FColor AnnotationColor = ColorGenerator.GetColorFromColorMap(ColorIndex);

	return AnnotationColor;
}


/**
void FObjectAnnotator::AnnotateMeshComponents(UWorld* World)
{
	if (!IsValid(World))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("Can not annotate world, the world is not valid"));
		return;
	}

	// List all MeshComponents in the scene
	TArray<UMeshComponent*> ComponentList;
	TArray<UObject*> UObjectList;
	bool bIncludeDerivedClasses = true;
	EObjectFlags ExclusionFlags = EObjectFlags::RF_ClassDefaultObject;
	EInternalObjectFlags ExclusionInternalFlags = EInternalObjectFlags::AllFlags;
	GetObjectsOfClass(UMeshComponent::StaticClass(), UObjectList, bIncludeDerivedClasses, ExclusionFlags, ExclusionInternalFlags);
	for (UObject* Object : UObjectList)
	{
		UMeshComponent* Component = Cast<UMeshComponent>(Object);

		if (Component->GetWorld() == World
		&& !ComponentList.Contains(Component))
		{
			ComponentList.Add(Component);
		}
	}

	for (int i = 0; i < ComponentList.Num(); i++)	
	// for (UMeshComponent* MeshComponent : ComponentList)
	{
		UMeshComponent* MeshComponent = ComponentList[i];
		if (!IsValid(MeshComponent))
		{
			UE_LOG(LogTemp, Warning, TEXT("MeshComponent is invalid."));
			continue;
		}

		UAnnotationComponent* AnnotationComponent = nullptr;
		TArray<USceneComponent*> AttachChildren = MeshComponent->GetAttachChildren();
		for (USceneComponent* Child : AttachChildren)
		{
			AnnotationComponent = Cast<UAnnotationComponent>(Child);
			if (IsValid(AnnotationComponent))
			{
				break;
			}
		}
		if (!IsValid(AnnotationComponent))
		{
			// Create a new one
			AnnotationComponent = NewObject<UAnnotationComponent>(MeshComponent);
			AnnotationComponent->SetupAttachment(MeshComponent);
			AnnotationComponent->RegisterComponent();
			AnnotationComponent->MarkRenderStateDirty();
		}
		// UE_LOG(LogTemp, Log, TEXT("Annotate %s with color %s"), *MeshComponent->GetName(), *AnnotationColor.ToString());
		// FColor AnnotationColor = FColor::MakeRandomColor();
		FColor AnnotationColor = ColorGenerator.GetColorFromColorMap(i);
		AnnotationComponent->SetAnnotationColor(AnnotationColor);
		// AnnotationComponent->AnnotationColor = FColor::MakeRandomColor(); // Debug
	}
}
*/

/** Utility function to generate color map */
int32 FColorGenerator::GetChannelValue(uint32 Index)
{
	static int32 Values[256] = { 0 };
	static bool Init = false;
	if (!Init)
	{
		float Step = 256;
		uint32 Iter = 0;
		Values[0] = 0;
		while (Step >= 1)
		{
			for (uint32 Value = Step - 1; Value <= 256; Value += Step * 2)
			{
				Iter++;
				Values[Iter] = Value;
			}
			Step /= 2;
		}
		Init = true;
	}
	if (Index >= 0 && Index <= 255)
	{
		return Values[Index];
	}
	else
	{
		UE_LOG(LogUnrealCV, Error, TEXT("Invalid channel index"));
		check(false);
		return -1;
	}
}

void FColorGenerator::GetColors(int32 MaxVal, bool Fix1, bool Fix2, bool Fix3, TArray<FColor>& ColorMap)
{
	for (int32 I = 0; I <= (Fix1 ? 0 : MaxVal - 1); I++)
	{
		for (int32 J = 0; J <= (Fix2 ? 0 : MaxVal - 1); J++)
		{
			for (int32 K = 0; K <= (Fix3 ? 0 : MaxVal - 1); K++)
			{
				uint8 R = (uint8)GetChannelValue(Fix1 ? MaxVal : I);
				uint8 G = (uint8)GetChannelValue(Fix2 ? MaxVal : J);
				uint8 B = (uint8)GetChannelValue(Fix3 ? MaxVal : K);
				FColor Color(R, G, B, 255);
				ColorMap.Add(Color);
			}
		}
	}
}

// FColor FColorGenerator::GetColorFromColorMap(int32 ObjectIndex)
// {
// 	static TArray<FColor> ColorMap;
// 	int NumPerChannel = 32;
// 	if (ColorMap.Num() == 0)
// 	{
// 		// 32 ^ 3
// 		for (int32 MaxChannelIndex = 0; MaxChannelIndex < NumPerChannel; MaxChannelIndex++) // Get color map for 1000 objects
// 		{
// 			// GetColors(MaxChannelIndex, false, false, false, ColorMap);
// 			GetColors(MaxChannelIndex, false, false, true, ColorMap);
// 			GetColors(MaxChannelIndex, false, true, false, ColorMap);
// 			GetColors(MaxChannelIndex, false, true, true, ColorMap);
// 			GetColors(MaxChannelIndex, true, false, false, ColorMap);
// 			GetColors(MaxChannelIndex, true, false, true, ColorMap);
// 			GetColors(MaxChannelIndex, true, true, false, ColorMap);
// 			GetColors(MaxChannelIndex, true, true, true, ColorMap);
// 		}
// 	}
// 	if (ObjectIndex < 0 || ObjectIndex >= pow(NumPerChannel, 3))
// 	{
// 		UE_LOG(LogUnrealCV, Error, TEXT("Object index %d is out of the color map boundary [%d, %d]"), ObjectIndex, 0, (int) pow(NumPerChannel, 3));
// 	}
// 	return ColorMap[ObjectIndex];
// }
// FColor FColorGenerator::GetColorFromColorMap(int32 ObjectIndex)
// {
//     constexpr int32 NumPerChannel = 50;
//     constexpr int32 MaxIndex = NumPerChannel * NumPerChannel * NumPerChannel;

//     if (ObjectIndex < 0 || ObjectIndex >= MaxIndex)
//     {
//         UE_LOG(LogUnrealCV, Error,
//             TEXT("Object index %d is out of the color map boundary [%d, %d]"),
//             ObjectIndex, 0, MaxIndex - 1);
//         return FColor::Black;
//     }

//     int32 Layer = ObjectIndex / (NumPerChannel * NumPerChannel); // MaxChannelIndex
//     int32 Rem   = ObjectIndex % (NumPerChannel * NumPerChannel);

//     int32 Sub   = Rem / NumPerChannel;
//     int32 Base  = Rem % NumPerChannel;

//     static const bool FlipTable[7][3] =
//     {
//         { false, false, true  },
//         { false, true,  false },
//         { false, true,  true  },
//         { true,  false, false },
//         { true,  false, true  },
//         { true,  true,  false },
//         { true,  true,  true  },
//     };

//     const bool* Flip = FlipTable[Sub % 7];

//     auto Apply = [](int32 v, bool flip)
//     {
//         return flip ? (NumPerChannel - 1 - v) : v;
//     };

//     uint8 R = Apply(Layer, Flip[0]) * 255 / (NumPerChannel - 1);
//     uint8 G = Apply(Layer, Flip[1]) * 255 / (NumPerChannel - 1);
//     uint8 B = Apply(Base,  Flip[2]) * 255 / (NumPerChannel - 1);

//     return FColor(R, G, B, 255);
// }
FColor FColorGenerator::GetColorFromColorMap(int32 ObjectIndex)
{
    static TArray<FColor> ColorMap;
    constexpr int32 NumPerChannel = 32;

    if (ObjectIndex < 0 || ObjectIndex >= NumPerChannel * NumPerChannel * NumPerChannel)
    {
        UE_LOG(LogUnrealCV, Error, TEXT("Object index %d out of range"), ObjectIndex);
        return FColor::Black;
    }

    // Lazy build
    static int32 BuiltMaxChannel = 0;

    while (ColorMap.Num() <= ObjectIndex && BuiltMaxChannel < NumPerChannel)
    {
        int32 MaxVal = BuiltMaxChannel;

        GetColors(MaxVal, false, false, true,  ColorMap);
        GetColors(MaxVal, false, true,  false, ColorMap);
        GetColors(MaxVal, false, true,  true,  ColorMap);
        GetColors(MaxVal, true,  false, false, ColorMap);
        GetColors(MaxVal, true,  false, true,  ColorMap);
        GetColors(MaxVal, true,  true,  false, ColorMap);
        GetColors(MaxVal, true,  true,  true,  ColorMap);

        BuiltMaxChannel++;
    }

    return ColorMap[ObjectIndex];
}
