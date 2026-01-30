#include "ShadowCatcherCamSensor.h"
#include "UnrealcvLog.h"
#include "UnrealcvStats.h"
#include "Server/ServerConfig.h"
#include "Server/UnrealcvServer.h"
#include "Materials/Material.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "BPFunctionLib/StencilBPLib.h"
#include "EngineUtils.h"
#include "Engine/Light.h"

DECLARE_CYCLE_STAT(TEXT("UShadowCatcherCamSensor::CaptureShadowCatcher"), STAT_CaptureShadowCatcher, STATGROUP_UnrealCV);

UShadowCatcherCamSensor::UShadowCatcherCamSensor(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	GroundPlaneActor = nullptr;

	CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;

	ShowFlags.SetLighting(true);
	ShowFlags.SetDynamicShadows(true);
	ShowFlags.SetContactShadows(true);
	ShowFlags.SetPostProcessing(true);
	ShowFlags.SetPostProcessMaterial(true);

	FString ShadowCatcherMaterialPath = TEXT("Material'/UnrealCV/ShadowCatcher.ShadowCatcher'");
	ConstructorHelpers::FObjectFinder<UMaterial> Material(*ShadowCatcherMaterialPath);

	if (Material.Object)
	{
		ShadowCatcherMaterial = Material.Object;
		UE_LOG(LogUnrealCV, Log, TEXT("ShadowCatcherCamSensor: Loaded ShadowCatcher material"));

		UMaterialInstanceDynamic* PostProcessMaterialInstance = UMaterialInstanceDynamic::Create(ShadowCatcherMaterial, nullptr);
		if (!PostProcessMaterialInstance)
		{
			UE_LOG(LogUnrealCV, Error, TEXT("ShadowCatcherCamSensor: Could not create material instance dynamic"));
		}
		else
		{
			PostProcessMaterialInstance->SetScalarParameterValue(TEXT("StencilIndex"), 1.0f);
			SetPostProcessMaterial(PostProcessMaterialInstance);
			UE_LOG(LogUnrealCV, Log, TEXT("ShadowCatcherCamSensor: Set StencilIndex=1.0"));
		}
	}
	else
	{
		UE_LOG(LogUnrealCV, Error, TEXT("ShadowCatcherCamSensor: Failed to load ShadowCatcher material at %s"), *ShadowCatcherMaterialPath);
	}
}

void UShadowCatcherCamSensor::InitTextureTarget(int filmWidth, int filmHeight)
{
	FServerConfig& Config = FUnrealcvServer::Get().Config;
	bool bUseBGRA8 = Config.bLitUseBGRA8;
	if (bUseBGRA8)
	{
		InitUInt8TextureTarget(filmWidth, filmHeight, true);
	}
	else
	{
		InitFloat16TextureTarget(filmWidth, filmHeight);
	}

	if (ShadowCatcherMaterial)
	{
		UMaterialInstanceDynamic* PostProcessMaterialInstance = UMaterialInstanceDynamic::Create(ShadowCatcherMaterial, nullptr);
		if (!PostProcessMaterialInstance)
		{
			UE_LOG(LogUnrealCV, Error, TEXT("ShadowCatcherCamSensor: Could not create material instance dynamic"));
		}
		else
		{
			PostProcessMaterialInstance->SetScalarParameterValue(TEXT("StencilIndex"), 1.0f);
			SetPostProcessMaterial(PostProcessMaterialInstance);
			UE_LOG(LogUnrealCV, Log, TEXT("ShadowCatcherCamSensor: Set StencilIndex=1.0"));
		}
	}
	else
	{
		UE_LOG(LogUnrealCV, Error, TEXT("ShadowCatcherCamSensor: ShadowCatcherMaterial is null"));
	}
}

void UShadowCatcherCamSensor::CreateGroundPlane(UWorld* World)
{
	if (!IsValid(World))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("CreateGroundPlane: Invalid world"));
		return;
	}

	if (IsValid(GroundPlaneActor))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("CreateGroundPlane: Ground plane already exists"));
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = FName(*FString::Printf(TEXT("ShadowCatcher_GroundPlane_%d"), FMath::Rand()));
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;

	GroundPlaneActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

	if (!IsValid(GroundPlaneActor))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("CreateGroundPlane: Failed to spawn ground plane actor"));
		return;
	}

	UStaticMeshComponent* MeshComp = GroundPlaneActor->FindComponentByClass<UStaticMeshComponent>();
	if (!IsValid(MeshComp))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("CreateGroundPlane: No StaticMeshComponent found"));
		return;
	}

	MeshComp->SetMobility(EComponentMobility::Movable);

	UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (IsValid(PlaneMesh))
	{
		MeshComp->SetStaticMesh(PlaneMesh);
	}

	UMaterial* WhiteMaterial = Cast<UMaterial>(StaticLoadObject(
		UMaterial::StaticClass(),
		nullptr,
		TEXT("Material'/UnrealCV/DefaultWhite.DefaultWhite'")
	));

	if (IsValid(WhiteMaterial))
	{
		MeshComp->SetMaterial(0, WhiteMaterial);
		UE_LOG(LogUnrealCV, Log, TEXT("CreateGroundPlane: Applied DefaultWhite material"));
	}
	else
	{
		UE_LOG(LogUnrealCV, Error, TEXT("CreateGroundPlane: Failed to load DefaultWhite material"));
	}

	MeshComp->SetWorldScale3D(FVector(1000.0f, 1000.0f, 1.0f));
	MeshComp->SetCastShadow(false);
	MeshComp->SetReceivesDecals(false);

	UE_LOG(LogUnrealCV, Log, TEXT("CreateGroundPlane: Created ground plane at %s"), *GroundPlaneActor->GetActorLocation().ToString());
}

void UShadowCatcherCamSensor::UpdateGroundPlaneTransform(AActor* TargetActor)
{
	if (!IsValid(GroundPlaneActor) || !IsValid(TargetActor))
	{
		return;
	}

	FVector ActorLocation = TargetActor->GetActorLocation();
	FBox ActorBounds = TargetActor->GetComponentsBoundingBox(true);

	FVector GroundLocation = FVector(ActorLocation.X, ActorLocation.Y, ActorBounds.Min.Z - 1.0f);
	GroundPlaneActor->SetActorLocation(GroundLocation);

	UE_LOG(LogUnrealCV, Log, TEXT("UpdateGroundPlaneTransform: Moved ground plane to %s"), *GroundLocation.ToString());
}

void UShadowCatcherCamSensor::CleanupGroundPlane()
{
	if (IsValid(GroundPlaneActor))
	{
		GroundPlaneActor->Destroy();
		GroundPlaneActor = nullptr;
		UE_LOG(LogUnrealCV, Log, TEXT("CleanupGroundPlane: Destroyed ground plane"));
	}
}

void UShadowCatcherCamSensor::Cleanup(AActor* TargetActor)
{
	if (IsValid(TargetActor))
	{
		UStencilBPLib::DisableCustomDepthForActor(TargetActor);
	}

	UWorld* World = FUnrealcvServer::Get().GetWorld();
	if (IsValid(World))
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (IsValid(Actor) && HiddenActors.Contains(Actor))
			{
				Actor->SetActorHiddenInGame(false);
			}
		}
		HiddenActors.Empty();
	}

	CleanupGroundPlane();
	UE_LOG(LogUnrealCV, Log, TEXT("Cleanup: Cleaned up ShadowCatcher for %s"), *GetNameSafe(TargetActor));
}

void UShadowCatcherCamSensor::SetupForActor(AActor* TargetActor, UWorld* World)
{
	if (!IsValid(TargetActor))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("SetupForActor: Invalid target actor"));
		return;
	}

	if (!IsValid(World))
	{
		World = FUnrealcvServer::Get().GetWorld();
		if (!IsValid(World))
		{
			UE_LOG(LogUnrealCV, Error, TEXT("SetupForActor: Invalid world"));
			return;
		}
	}

	UStencilBPLib::EnableCustomDepthForActor(TargetActor, 1);

	if (!IsValid(GroundPlaneActor))
	{
		CreateGroundPlane(World);
	}

	UpdateGroundPlaneTransform(TargetActor);

	HiddenActors.Empty();
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (IsValid(Actor) && Actor != TargetActor && Actor != GroundPlaneActor)
		{
			if (Actor->IsA(ALight::StaticClass()))
			{
				continue;
			}

			if (!Actor->IsHidden())
			{
				Actor->SetActorHiddenInGame(true);
				HiddenActors.Add(Actor);
			}
		}
	}

	UE_LOG(LogUnrealCV, Log, TEXT("SetupForActor: Setup complete for %s, hidden %d actors"),
		*TargetActor->GetName(), HiddenActors.Num());
}

void UShadowCatcherCamSensor::CaptureShadowCatcher(TArray<FColor>& Image, int& Width, int& Height)
{
	SCOPE_CYCLE_COUNTER(STAT_CaptureShadowCatcher);
	if (!CheckTextureTarget())
	{
		InitTextureTarget(this->FilmWidth, this->FilmHeight);
		if (!CheckTextureTarget())
		{
			UE_LOG(LogUnrealCV, Error, TEXT("Failed to initialize TextureTarget."));
			return;
		}
	}

	Capture(Image, Width, Height);
}

void UShadowCatcherCamSensor::CaptureShadowCatcherToFile(FString Filename)
{
	SCOPE_CYCLE_COUNTER(STAT_CaptureShadowCatcher);
	if (!CheckTextureTarget())
	{
		InitTextureTarget(this->FilmWidth, this->FilmHeight);
		if (!CheckTextureTarget())
		{
			UE_LOG(LogUnrealCV, Error, TEXT("Failed to initialize TextureTarget."));
			return;
		}
	}

	CaptureFastToFile(Filename);
}
