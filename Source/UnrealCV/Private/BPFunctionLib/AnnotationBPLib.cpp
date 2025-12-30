#include "AnnotationBPLib.h"
#include "UnrealcvServer.h"
#include "UnrealcvLog.h"

void UAnnotationBPLib::AnnotateActor(AActor* Actor)
{
	if (!IsValid(Actor))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("AnnotateActor: Actor is invalid"));
		return;
	}

	TWeakObjectPtr<AUnrealcvWorldController> WorldController = FUnrealcvServer::Get().WorldController;

	if (!WorldController.IsValid())
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("AnnotateActor: WorldController is not available"));
		return;
	}

	FObjectAnnotator& Annotator = WorldController->ObjectAnnotator;
	

	FColor AnnotationColor;
	Annotator.GetAnnotationColor(Actor, AnnotationColor);
	Annotator.SetAnnotationColor(Actor, AnnotationColor);
	FlushRenderingCommands();

	UE_LOG(LogUnrealCV, Log, TEXT("AnnotateActor: Annotated %s with color %s"),
		*Actor->GetName(), *AnnotationColor.ToString());
}

void UAnnotationBPLib::AnnotateWorld()
{
	TWeakObjectPtr<AUnrealcvWorldController> WorldController = FUnrealcvServer::Get().WorldController;

	if (!WorldController.IsValid())
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("AnnotateActor: WorldController is not available"));
		return;
	}

	FObjectAnnotator& Annotator = WorldController->ObjectAnnotator;
	Annotator.AnnotateWorld(FUnrealcvServer::Get().GetWorld());
	UE_LOG(LogUnrealCV, Log, TEXT("AnnotateWorld: World annotation completed"));
}
