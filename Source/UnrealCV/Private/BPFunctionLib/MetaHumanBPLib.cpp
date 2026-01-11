#include "BPFunctionLib/MetaHumanBPLib.h"
#include "Utils/MetaHumanCacheManager.h"
#include "Engine/Blueprint.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimBlueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "UnrealcvServer.h"
#include "Engine/StreamableManager.h"
#include "Async/TaskGraphInterfaces.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "FileHelpers.h"

int32 UMetaHumanBPLib::BatchSize = 4;
TUniquePtr<FBatchContext> UMetaHumanBPLib::GBatchContext = nullptr;

void UMetaHumanBPLib::ProcessBatch(FBatchContext* Context)
{
	if (!Context || Context->bCancelled)
	{
		return;
	}

	UWorld* World = nullptr;
	// if (Context->bIsSpawn)
	{
		World = FUnrealcvServer::Get().GetWorld();
		if (!World)
		{
			UE_LOG(LogTemp, Error, TEXT("MetaHumanAsync: No valid world found"));
			Context->bCancelled = true;
			return;
		}
	}

	int32 EndIndex = FMath::Min(Context->CurrentIndex + Context->BatchSize, Context->AllPaths.Num());

	for (int32 i = Context->CurrentIndex; i < EndIndex; ++i)
	{
		const FString& MetaHumanPath = Context->AllPaths[i];
		bool bSuccess = false;

		bSuccess = UMetaHumanBPLib::SetMetaHumanAnimationBlueprint(MetaHumanPath, Context->AnimBlueprintPath);
		if (!bSuccess)
		{
			UE_LOG(LogTemp, Error, TEXT("MetaHumanAsync: Set animation failed"));
			Context->bCancelled = true;
			return;
		}

		// if (Context->bIsSpawn)
		{
			UClass* MetaHumanClass = LoadObject<UClass>(nullptr, *(MetaHumanPath + TEXT("_C")));
			if (MetaHumanClass)
			{
				FVector SpawnLocation(0.0f + i * 200.0f, 0.0f, -2000.0f);
				FRotator SpawnRotation(0.0f, 0.0f, 0.0f);
				FActorSpawnParameters SpawnParams;
				SpawnParams.Owner = nullptr;
				SpawnParams.Instigator = nullptr;
				SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

				AActor* SpawnedActor = World->SpawnActor<AActor>(MetaHumanClass, SpawnLocation, SpawnRotation, SpawnParams);
				if (SpawnedActor)
				{
					Context->SpawnedActors.Add(SpawnedActor);
					bSuccess = true;
					UE_LOG(LogTemp, Log, TEXT("Spawned MetaHuman %d/%d: %s"), i + 1, Context->AllPaths.Num(), *MetaHumanPath);
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("Failed to spawn metahuman %s"), *MetaHumanPath);
				}
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to load metahuman %s"),  *MetaHumanPath);
			}
		}
		// else
		// {

		// 	UE_LOG(LogTemp, Log, TEXT("Metahuman is not needed to spawn %s"), *MetaHumanPath);
		// }

		if (bSuccess)
		{
			Context->SuccessfulPaths.Add(MetaHumanPath);
		}

		UE_LOG(LogTemp, Log, TEXT("MetaHumanAsync: Progress %d/%d - %s (%s)"), i + 1, Context->AllPaths.Num(), *MetaHumanPath, bSuccess ? TEXT("OK") : TEXT("FAIL"));
	}

	Context->CurrentIndex = EndIndex;

	if (Context->CurrentIndex >= Context->AllPaths.Num())
	{
		UE_LOG(LogTemp, Log, TEXT("MetaHumanAsync: Batch complete. Success: %d/%d"), Context->SuccessfulPaths.Num(), Context->AllPaths.Num());

		if (UWorld* CurrentWorld = FUnrealcvServer::Get().GetWorld())
		{
			CurrentWorld->GetTimerManager().ClearTimer(Context->TimerHandle);
		}
		GBatchContext.Reset();
	}
	else
	{
		if (UWorld* CurrentWorld = FUnrealcvServer::Get().GetWorld())
		{
			CurrentWorld->GetTimerManager().SetTimer(
				Context->TimerHandle,
				[Context]() { ProcessBatch(Context); },
				5.0f,
				false
			);
		}

		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		UE_LOG(LogTemp, Log, TEXT("MetaHumanAsync: GC triggered at index %d/%d"), Context->CurrentIndex, Context->AllPaths.Num());
	}
	bool bPromptUserToSave = false;
	bool bSaveMapPackages = true;
	bool bSaveContentPackages = true;
	bool bFastSave = false;
	bool bNotifyNoPackagesSaved = false;
	bool bCanBeDeclined = false;

	if (FEditorFileUtils::SaveDirtyPackages(
		bPromptUserToSave,
		bSaveMapPackages,
		bSaveContentPackages,
		bFastSave,
		bNotifyNoPackagesSaved,
		bCanBeDeclined))
	{
		UE_LOG(LogTemp, Log, TEXT("✓ All generated assets saved successfully"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to save some packages, but character was assembled"));
	}
}


TArray<FString> UMetaHumanBPLib::GetAllMetaHumanBlueprintPaths()
{
	FMetaHumanCacheManager& CacheManager = FMetaHumanCacheManager::Get();

	TArray<FString> MetaHumanPaths = CacheManager.GetAllMetaHumanPaths();

	if (!MetaHumanPaths.IsEmpty())
	{
		UE_LOG(LogTemp, Log, TEXT("GetAllMetaHumanBlueprintPaths: Saving %d MetaHumans to cache"), MetaHumanPaths.Num());
		CacheManager.SaveCacheToFile(MetaHumanPaths);

		FString LogStr = TEXT("All MetaHuman Blueprint Paths: {");
		for (const FString& Path : MetaHumanPaths)
		{
			LogStr += Path + TEXT(", ");
		}
		LogStr += TEXT("}");
		UE_LOG(LogTemp, Log, TEXT("%s"), *LogStr);

		return MetaHumanPaths;
	}

	UE_LOG(LogTemp, Warning, TEXT("GetAllMetaHumanBlueprintPaths: AssetRegistry returned empty, loading from cache"));
	MetaHumanPaths = CacheManager.LoadCacheFromFile();

	return MetaHumanPaths;
}

TArray<FString> UMetaHumanBPLib::FilterBatchGeneratedMetaHumans(const TArray<FString>& AllPaths)
{
	TArray<FString> BatchGenPaths;

	for (const FString& Path : AllPaths)
	{
		if (Path.Contains(TEXT("BatchGen")))
		{
			BatchGenPaths.Add(Path);
		}
	}

	return BatchGenPaths;
}

bool UMetaHumanBPLib::SetMetaHumanAnimationBlueprint(const FString& MetaHumanBPPath, const FString& AnimBlueprintPath)
{
	UBlueprint* MetaHumanBP = LoadObject<UBlueprint>(nullptr, *MetaHumanBPPath);
	if (!MetaHumanBP)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load MetaHuman Blueprint: %s"), *MetaHumanBPPath);
		return false;
	}

	UClass* AnimBPClass = LoadObject<UClass>(nullptr, *AnimBlueprintPath);
	if (!AnimBPClass)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load Animation Blueprint class: %s"), *AnimBlueprintPath);
		return false;
	}

	USkeletalMeshComponent* BodyComponentTemplate = nullptr;

	if (MetaHumanBP->SimpleConstructionScript)
	{
		const TArray<USCS_Node*>& AllNodes = MetaHumanBP->SimpleConstructionScript->GetAllNodes();
		UE_LOG(LogTemp, Log, TEXT("Found %d SCS nodes in MetaHuman Blueprint"), AllNodes.Num());

		for (USCS_Node* Node : AllNodes)
		{
			if (Node && Node->ComponentTemplate)
			{
				FString NodeName = Node->GetVariableName().ToString();
				FString ComponentName = Node->ComponentTemplate->GetName();
				UE_LOG(LogTemp, Log, TEXT("  Node: %s, Component: %s, Class: %s"),
					*NodeName, *ComponentName, *Node->ComponentTemplate->GetClass()->GetName());

				if (USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>(Node->ComponentTemplate))
				{
					if (NodeName.Contains(TEXT("Body")) || ComponentName.Contains(TEXT("Body")))
					{
						BodyComponentTemplate = SkelMeshComp;
						UE_LOG(LogTemp, Log, TEXT("  -> Matched 'Body' component!"));
						break;
					}
				}
			}
		}
	}

	if (!BodyComponentTemplate)
	{
		UE_LOG(LogTemp, Error, TEXT("No 'Body' SkeletalMeshComponent found in SimpleConstructionScript"));
		return false;
	}

	BodyComponentTemplate->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	BodyComponentTemplate->AnimClass = AnimBPClass;

	MetaHumanBP->Modify();
	MetaHumanBP->MarkPackageDirty();

	UE_LOG(LogTemp, Log, TEXT("Successfully set Animation Blueprint for %s to %s"), *MetaHumanBPPath, *AnimBlueprintPath);
	return true;
}

// TArray<FString> UMetaHumanBPLib::SetupAllMetaHumansWithAnimation(const FString& AnimBlueprintPath)
// {
// 	TArray<FString> AllMetaHumans = GetAllMetaHumanBlueprintPaths();
// 	TArray<FString> SuccessfulPaths;

// 	for (const FString& MetaHumanPath : AllMetaHumans)
// 	{
// 		if (SetMetaHumanAnimationBlueprint(MetaHumanPath, AnimBlueprintPath))
// 		{
// 			SuccessfulPaths.Add(MetaHumanPath);
// 		}
// 	}

// 	UE_LOG(LogTemp, Log, TEXT("SetupAllMetaHumansWithAnimation: %d/%d MetaHumans configured"), SuccessfulPaths.Num(), AllMetaHumans.Num());
// 	return SuccessfulPaths;
// }

// TArray<AActor*> UMetaHumanBPLib::SpawnAllMetaHumansToMap(const FString& AnimBlueprintPath)
// {
// 	TArray<AActor*> SpawnedActors;

// 	TArray<FString> AllMetaHumans = GetAllMetaHumanBlueprintPaths();

// 	if (AllMetaHumans.IsEmpty())
// 	{
// 		UE_LOG(LogTemp, Warning, TEXT("SpawnAllMetaHumansToMap: No MetaHumans found"));
// 		return SpawnedActors;
// 	}

// 	// UWorld* World = nullptr;
// 	// if (GEditor)
// 	// {
// 	// 	World = GEditor->GetEditorWorldContext().World();
// 	// }
// 	UWorld* World = FUnrealcvServer::Get().GetWorld();

// 	if (!World)
// 	{
// 		UE_LOG(LogTemp, Error, TEXT("SpawnAllMetaHumansToMap: No valid world found"));
// 		return SpawnedActors;
// 	}

// 	SetupAllMetaHumansWithAnimation(AnimBlueprintPath);

// 	for (int32 i = 0; i < AllMetaHumans.Num(); ++i)
// 	{
// 		const FString& MetaHumanPath = AllMetaHumans[i];
// 		UClass* MetaHumanClass = LoadObject<UClass>(nullptr, *(MetaHumanPath + TEXT("_C")));

// 		if (!MetaHumanClass)
// 		{
// 			UE_LOG(LogTemp, Error, TEXT("SpawnAllMetaHumansToMap: Failed to load class %s"), *(MetaHumanPath + TEXT("_C")));
// 			continue;
// 		}

// 		FVector SpawnLocation(0.0f + i * 200.0f, 0.0f, -2000.0f);
// 		FRotator SpawnRotation(0.0f, 0.0f, 0.0f);

// 		FActorSpawnParameters SpawnParams;
// 		SpawnParams.Owner = nullptr;
// 		SpawnParams.Instigator = nullptr;
// 		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

// 		AActor* SpawnedActor = World->SpawnActor<AActor>(MetaHumanClass, SpawnLocation, SpawnRotation, SpawnParams);

// 		if (SpawnedActor)
// 		{
// 			SpawnedActors.Add(SpawnedActor);
// 			UE_LOG(LogTemp, Log, TEXT("Spawned MetaHuman at position (%.0f, 0, -2000): %s"), SpawnLocation.X, *MetaHumanPath);
// 		}
// 		else
// 		{
// 			UE_LOG(LogTemp, Error, TEXT("SpawnAllMetaHumansToMap: Failed to spawn MetaHuman: %s"), *MetaHumanPath);
// 		}
// 	}

// 	UE_LOG(LogTemp, Log, TEXT("SpawnAllMetaHumansToMap: Spawned %d/%d MetaHumans"), SpawnedActors.Num(), AllMetaHumans.Num());
// 	return SpawnedActors;
// }

void UMetaHumanBPLib::SetupAllMetaHumansWithAnimation(const FString& AnimBlueprintPath)
{

	if (GBatchContext)
	{
		UE_LOG(LogTemp, Warning, TEXT("MetaHumanAsync: Another batch is already running, cancelling it"));
		GBatchContext->bCancelled = true;
	}

	TArray<FString> AllMetaHumans = GetAllMetaHumanBlueprintPaths();
	if (AllMetaHumans.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("MetaHumanAsync: No MetaHumans found"));
		return;
	}

	GBatchContext = MakeUnique<FBatchContext>();
	GBatchContext->AllPaths = AllMetaHumans;
	GBatchContext->AnimBlueprintPath = AnimBlueprintPath;
	GBatchContext->BatchSize = FMath::Max(1, BatchSize);
	GBatchContext->CurrentIndex = 0;
	GBatchContext->bIsSpawn = false;
	GBatchContext->bCancelled = false;

	UE_LOG(LogTemp, Log, TEXT("MetaHumanAsync: Starting setup for %d MetaHumans (batch size %d)"), AllMetaHumans.Num(), GBatchContext->BatchSize);

	ProcessBatch(GBatchContext.Get());
}

void UMetaHumanBPLib::SpawnAllMetaHumansToMap(const FString& AnimBlueprintPath)
{

	if (GBatchContext)
	{
		UE_LOG(LogTemp, Warning, TEXT("MetaHumanAsync: Another batch is already running, cancelling it"));
		GBatchContext->bCancelled = true;
	}

	TArray<FString> AllMetaHumans = GetAllMetaHumanBlueprintPaths();
	if (AllMetaHumans.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("MetaHumanAsync: No MetaHumans found"));
		return;
	}

	UWorld* World = FUnrealcvServer::Get().GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("MetaHumanAsync: No valid world found"));
		return;
	}

	GBatchContext = MakeUnique<FBatchContext>();
	GBatchContext->AllPaths = AllMetaHumans;
	GBatchContext->AnimBlueprintPath = AnimBlueprintPath;
	GBatchContext->BatchSize = FMath::Max(1, BatchSize);
	GBatchContext->CurrentIndex = 0;
	GBatchContext->bIsSpawn = true;
	GBatchContext->bCancelled = false;

	SetupAllMetaHumansWithAnimation(AnimBlueprintPath);

	UE_LOG(LogTemp, Log, TEXT("MetaHumanAsync: Starting spawn for %d MetaHumans (batch size %d)"), AllMetaHumans.Num(), GBatchContext->BatchSize);

	ProcessBatch(GBatchContext.Get());
}

void UMetaHumanBPLib::CancelAsyncOperation()
{
	if (GBatchContext)
	{
		GBatchContext->bCancelled = true;
		UE_LOG(LogTemp, Log, TEXT("MetaHumanAsync: Async operation cancelled"));
	}
}
