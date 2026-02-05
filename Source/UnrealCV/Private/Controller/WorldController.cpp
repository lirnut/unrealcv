// Weichao Qiu @ 2017
#include "WorldController.h"

#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"
#include "Runtime/Engine/Classes/GameFramework/Pawn.h"
#include "Runtime/Engine/Classes/Engine/World.h"
#include "Runtime/Engine/Classes/Engine/GameViewportClient.h"

#include "Sensor/CameraSensor/PawnCamSensor.h"
#include "VisionBPLib.h"
#include "UnrealcvServer.h"
#include "PlayerViewMode.h"
#include "UnrealcvLog.h"

AUnrealcvWorldController::AUnrealcvWorldController(const FObjectInitializer& ObjectInitializer)
{
	PlayerViewMode = CreateDefaultSubobject<UPlayerViewMode>(TEXT("PlayerViewMode"));
}

void AUnrealcvWorldController::AttachPawnSensor()
{
	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	check(PlayerController);
	APawn* Pawn = PlayerController->GetPawn();
	FUnrealcvServer& Server = FUnrealcvServer::Get();
	if (!IsValid(Pawn))
	{
		UE_LOG(LogTemp, Warning, TEXT("No available pawn to mount a PawnSensor"));
		return;
	}

	UE_LOG(LogUnrealCV, Display, TEXT("Attach a UnrealcvSensor to the pawn"));
	// Make sure this is the first one.
	UPawnCamSensor* PawnCamSensor = NewObject<UPawnCamSensor>(Pawn, TEXT("PawnSensor")); // Make Pawn as the owner of the component
	UE_LOG(LogUnrealCV, Warning, TEXT("AttachPawnSensor: Created PawnCamSensor=%p, Owner=%p(%s)"),
		PawnCamSensor, Pawn, *Pawn->GetName());
	// UFusionCamSensor* FusionCamSensor = ConstructObject<UFusionCamSensor>(UFusionCamSensor::StaticClass(), Pawn);

	UWorld *PawnWorld = Pawn->GetWorld();
	UWorld *GameWorld = FUnrealcvServer::Get().GetWorld();

	check(IsValid(GameWorld));
	check(PawnWorld == GameWorld);

	PawnCamSensor->AttachToComponent(Pawn->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	// AActor* OwnerActor = FusionCamSensor->GetOwner();
	PawnCamSensor->RegisterComponent(); // Is this neccessary?
	PawnCamSensor->SetActive(true);
	PawnCamSensor->SetComponentTickEnabled(true);
	// UE_LOG(LogUnrealCV, Warning, TEXT("AttachPawnSensor: After Register - IsRegistered=%d, bCanEverTick=%d, IsActive=%d"),
	// 	PawnCamSensor->IsRegistered(), PawnCamSensor->PrimaryComponentTick.bCanEverTick, PawnCamSensor->IsActive());
	// UE_LOG(LogUnrealCV, Warning, TEXT("AttachPawnSensor: After Activation - IsActive=%d, IsComponentTickEnabled=%d"),
	// 	PawnCamSensor->IsActive(), PawnCamSensor->IsComponentTickEnabled());
	UVisionBPLib::UpdateInput(Pawn, Server.Config.EnableInput);
}

void AUnrealcvWorldController::InitWorld()
{
	UE_LOG(LogUnrealCV, Display, TEXT("Overwrite the world setting with some UnrealCV extensions"));
	FUnrealcvServer& UnrealcvServer = FUnrealcvServer::Get();
	if (UnrealcvServer.TcpServer && !UnrealcvServer.TcpServer->IsListening())
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("The tcp server is not running"));
	}

	FEngineShowFlags ShowFlags = GetWorld()->GetGameViewport()->EngineShowFlags;
	this->PlayerViewMode->SaveGameDefault(ShowFlags);

	this->AttachPawnSensor();

	bool AnnotateWorld = FUnrealcvServer::Get().Config.AnnotateWorld;
	if (AnnotateWorld)
	{
		// Delay annotation to next tick to avoid GPU crashes when Lumen is initializing
		// This prevents simultaneous GPU proxy creation for AnnotationComponent and SkeletalMesh TLAS building
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimerForNextTick([this, World]()
			{
				if (!IsValid(this) || !IsValid(World))
				{
					UE_LOG(LogUnrealCV, Error, TEXT("WorldController is not valid"));
					return;
				}
				UE_LOG(LogUnrealCV, Display, TEXT("Delayed world annotation starting..."));
				FlushRenderingCommands();
				FObjectAnnotator::AnnotateWorld(World);
				FlushRenderingCommands();
				UE_LOG(LogUnrealCV, Display, TEXT("Delayed world annotation completed"));
			});
		}
	}
	else
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("Annoataion is not enabled, you can use the editor menu to manually annotate world if you're using UE Editor"))
		UE_LOG(LogUnrealCV, Warning, TEXT("	- Enable AnnotationWolrd in unrealcv config file unrealcv.ini"))
		UE_LOG(LogUnrealCV, Warning, TEXT("	- AnnotationWolrd in first tick is NOT SAFE for Skeletal Mesh"))
	}


	// TODO: remove legacy code
	// Update camera FOV

	//PlayerController->PlayerCameraManager->SetFOV(Server.Config.FOV);
	//FCaptureManager::Get().AttachGTCaptureComponentToCamera(Pawn);
}

void AUnrealcvWorldController::PostActorCreated()
{
	UE_LOG(LogTemp, Display, TEXT("AUnrealcvWorldController::PostActorCreated"));
	Super::PostActorCreated();
}


void AUnrealcvWorldController::BeginPlay()
{
	UE_LOG(LogTemp, Display, TEXT("AUnrealcvWorldController::BeginPlay"));
	Super::BeginPlay();
}

void AUnrealcvWorldController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UE_LOG(LogTemp, Display, TEXT("AUnrealcvWorldController::EndPlay"));

	// UWorld* World = GetWorld();
	// if (IsValid(World))
	// {
	// 	FObjectAnnotator::DeannotateWorld(World);
	// }

	Super::EndPlay(EndPlayReason);
}



void AUnrealcvWorldController::OpenLevel(FName LevelName)
{
	UWorld* World = GetWorld();
	if (!World) return;

	UGameplayStatics::OpenLevel(World, LevelName);
	UGameplayStatics::FlushLevelStreaming(World);
	UE_LOG(LogUnrealCV, Warning, TEXT("Level loaded"));
}


void AUnrealcvWorldController::Tick(float DeltaTime)
{

}
