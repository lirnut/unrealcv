// Weichao Qiu @ 2018
#include "SensorBPLib.h"
#include "UnrealcvServer.h"
#include "FusionCamSensor.h"
#include "Runtime/Engine/Classes/GameFramework/Pawn.h"
#include "Runtime/CoreUObject/Public/UObject/UObjectHash.h"
#include "Runtime/Launch/Resources/Version.h"

static FCameraIDManager* GCameraIDManager = nullptr;

FCameraIDManager& FCameraIDManager::Get()
{
	if (!GCameraIDManager)
	{
		GCameraIDManager = new FCameraIDManager();
	}
	return *GCameraIDManager;
}

FCameraIDManager::FCameraIDManager() : WorldMem(nullptr)
{
}



void FCameraIDManager::DetectWorldChange()
{
	if (WorldMem == nullptr)
	{
		WorldMem = FUnrealcvServer::Get().GetWorld();
		if (!IsValid(WorldMem))
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get world from UnrealCV server."));
		}
	}
	else
	{
		UWorld* ThisWorld = FUnrealcvServer::Get().GetWorld();
		if (IsValid(ThisWorld))
		{
			if (ThisWorld != WorldMem)
			{
				UE_LOG(LogTemp, Warning, TEXT("FCameraIDManager::GenerateUUID: World change detected"));
				WorldMem = ThisWorld;
				UsedCameraIDs.Empty();
				SensorToCameraIDMap.Empty();
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get world from UnrealCV server."));
		}
	}

}

FString FCameraIDManager::GenerateUUID(UFusionCamSensor* Sensor)
{
	FString ParentActorName = TEXT("Unknown");

	if (IsValid(Sensor))
	{
		AActor* Owner = Sensor->GetOwner();
		if (IsValid(Owner))
		{
			ParentActorName = Owner->GetName();
		}
	}

	FString GeneratedID;
	
	int32 RetryCount = 0;
	const int32 MAX_RETRY = 50;

	do
	{
		if (RetryCount < 16)
		{
			GeneratedID = FString::Printf(TEXT("CID-%s-%02x"), *ParentActorName, RetryCount);
		}
		else
		{
			uint32 RandomValue = FMath::Rand() & 0xff;
			// GeneratedID = FString::Printf(TEXT("CID_%02x_%s"), RandomValue, *ParentActorName);
			// GeneratedID = FString::Printf(TEXT("CID/%s/%02x"), *ParentActorName, RandomValue);
			GeneratedID = FString::Printf(TEXT("CID-%s-%02x"), *ParentActorName, RandomValue);
		}
		RetryCount++;
	} while (UsedCameraIDs.Contains(GeneratedID) && RetryCount < MAX_RETRY);

	if (RetryCount >= MAX_RETRY)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to generate unique camera ID after %d retries for sensor: %s"), MAX_RETRY, *ParentActorName);
		return GeneratedID;
	}

	UsedCameraIDs.Add(GeneratedID);
	return GeneratedID;
}


TArray<UFusionCamSensor*> FCameraIDManager::Sync()
{
	DetectWorldChange();
	TArray<UFusionCamSensor*> SensorList = USensorBPLib::GetFusionSensorList();
	TMap<UFusionCamSensor*, FString> SensorToCameraIDMap_;
	for (UFusionCamSensor* Sensor : SensorList)
	{
		if (!SensorToCameraIDMap.Contains(Sensor))
		{
			SensorToCameraIDMap_.Add(Sensor, GenerateUUID(Sensor));
		}
		else
		{
			SensorToCameraIDMap_.Add(Sensor, SensorToCameraIDMap[Sensor]);
		}
	}
	Swap(SensorToCameraIDMap, SensorToCameraIDMap_);
	return SensorList;
}


UFusionCamSensor* FCameraIDManager::GetSensorByAnyID(const FString& IDString)
{
	DetectWorldChange();
	if (IDString.IsEmpty())
	{
		return nullptr;
	}

	if (IDString.StartsWith(TEXT("CID")))
	{
		UFusionCamSensor* Ret = nullptr;
		TArray<UFusionCamSensor*> SensorList = USensorBPLib::GetFusionSensorList();
		TMap<UFusionCamSensor*, FString> SensorToCameraIDMap_;
		for (UFusionCamSensor* Sensor : SensorList)
		{
			FString ThisID;
			if (!SensorToCameraIDMap.Contains(Sensor))
			{
				ThisID = GenerateUUID(Sensor);
				SensorToCameraIDMap_.Add(Sensor, ThisID);
			}
			else
			{
				ThisID = SensorToCameraIDMap[Sensor];
				SensorToCameraIDMap_.Add(Sensor, ThisID);
			}

			if (ThisID == IDString)
			{
				if (Ret == nullptr)
				{
					Ret = Sensor;
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("Multiple sensors have the same ID: %s"), *IDString);
					check(false);
				}
			}
		}

		Swap(SensorToCameraIDMap, SensorToCameraIDMap_);
		return Ret;
	}
	else
	{
		int32 Index = FCString::Atoi(*IDString);
		TArray<UFusionCamSensor*> SensorList = USensorBPLib::GetFusionSensorList();
		if (Index >= 0 && Index < SensorList.Num())
		{
			return SensorList[Index];
		}
		return nullptr;
	}
}


int32 FCameraIDManager::GetIndexByAnyID(const FString& IDString)
{
	DetectWorldChange();
	const int32 INVALID_RET = -1;
	if (IDString.IsEmpty())
	{
		return INVALID_RET;
	}

	if (IDString.StartsWith(TEXT("CID")))
	{
		int32 Ret = INVALID_RET;
		TArray<UFusionCamSensor*> SensorList = USensorBPLib::GetFusionSensorList();
		TMap<UFusionCamSensor*, FString> SensorToCameraIDMap_;
		for (int32 Index = 0; Index < SensorList.Num(); Index++)
		{
			UFusionCamSensor* Sensor = SensorList[Index];
			FString ThisID;
			if (!SensorToCameraIDMap.Contains(Sensor))
			{
				ThisID = GenerateUUID(Sensor);
				SensorToCameraIDMap_.Add(Sensor, ThisID);
			}
			else
			{
				ThisID = SensorToCameraIDMap[Sensor];
				SensorToCameraIDMap_.Add(Sensor, ThisID);
			}

			if (ThisID == IDString)
			{
				if (Ret == INVALID_RET)
				{
					Ret = Index;
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("Multiple sensors have the same ID: %s"), *IDString);
					check(false);
				}
			}
		}
		Swap(SensorToCameraIDMap, SensorToCameraIDMap_);
		return Ret;
	}
	else
	{
		int32 Index = FCString::Atoi(*IDString);
		TArray<UFusionCamSensor*> SensorList = USensorBPLib::GetFusionSensorList();
		if (Index >= 0 && Index < SensorList.Num())
		{
			return Index;
		}
		return INVALID_RET;
	}
}

FString FCameraIDManager::GetNewFormatID(UFusionCamSensor* Sensor)
{
	DetectWorldChange();
	if (!IsValid(Sensor))
	{
		return FString();
	}

	if (SensorToCameraIDMap.Contains(Sensor))
	{
		return SensorToCameraIDMap[Sensor];
	}

	Sync();

	if (SensorToCameraIDMap.Contains(Sensor))
	{
		return SensorToCameraIDMap[Sensor];
	}
	return FString();
}

void FCameraIDManager::PrintIDMappings() const
{
	UE_LOG(LogTemp, Warning, TEXT("=== Camera ID Mappings ==="));
	for (const auto& Pair : SensorToCameraIDMap)
	{
		UFusionCamSensor* Sensor = Pair.Key;
		UE_LOG(LogTemp, Warning, TEXT("  %s"), *Pair.Value);
	}
}

TArray<UFusionCamSensor*> USensorBPLib::GetFusionSensorList()
{
	TArray<UFusionCamSensor*> SensorList;

	UWorld* World = FUnrealcvServer::Get().GetWorld();
	if (!World) return SensorList;

	APawn* Pawn = FUnrealcvServer::Get().GetPawn();

	if (IsValid(Pawn))
	{
		TArray<UActorComponent*> PawnComponents = FUnrealcvServer::Get().GetPawn()->K2_GetComponentsByClass(UFusionCamSensor::StaticClass());
		for (UActorComponent* FusionCamSensor : PawnComponents)
		{
			SensorList.Add(Cast<UFusionCamSensor>(FusionCamSensor));
		}
	}

	TArray<UObject*> UObjectList;
	bool bIncludeDerivedClasses = false;
	EObjectFlags ExclusionFlags = EObjectFlags::RF_ClassDefaultObject;
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 4
	EInternalObjectFlags ExclusionInternalFlags = EInternalObjectFlags_AllFlags;
#else
	EInternalObjectFlags ExclusionInternalFlags = EInternalObjectFlags::AllFlags;
#endif
	GetObjectsOfClass(UFusionCamSensor::StaticClass(), UObjectList, bIncludeDerivedClasses, ExclusionFlags);

	// Filter out objects not belong to the game world (editor world for example)
	for (UObject* SensorObject : UObjectList)
	{
		UFusionCamSensor *FusionSensor = Cast<UFusionCamSensor>(SensorObject);
		if (!IsValid(FusionSensor)) continue;
		if (FusionSensor->GetWorld() != World) continue;
		if (SensorList.Contains(FusionSensor) == false)
		{
			SensorList.Add(FusionSensor);
		}
	}
	return SensorList;
}

UFusionCamSensor* USensorBPLib::GetSensorById(int SensorId)
{
	FUnrealcvServer::Get().InitWorldController();

	TArray<UFusionCamSensor*> SensorList = USensorBPLib::GetFusionSensorList();
	if (SensorId < 0 || SensorId >= SensorList.Num()) return nullptr;
	return SensorList[SensorId];
}

UFusionCamSensor* USensorBPLib::GetSensorByAnyID(const FString& IDString)
{
	return FCameraIDManager::Get().GetSensorByAnyID(IDString);
}

int32 USensorBPLib::GetIndexByAnyID(const FString& IDString)
{
	return FCameraIDManager::Get().GetIndexByAnyID(IDString);
}

TArray<FString> USensorBPLib::GetFusionSensorListWithNewIDs()
{
	TArray<FString> NewFormatIDs;
	TArray<UFusionCamSensor*> SensorList = USensorBPLib::GetFusionSensorList();

	for (UFusionCamSensor* Sensor : SensorList)
	{
		FString NewID = FCameraIDManager::Get().GetNewFormatID(Sensor);
		if (!NewID.IsEmpty())
		{
			NewFormatIDs.Add(NewID);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get new ID for sensor: %s, which is from FusionCamSensor list"), *Sensor->GetName());
		}
	}

	return NewFormatIDs;
}

FString USensorBPLib::GetSensorNewFormatID(UFusionCamSensor* Sensor)
{
	if (!IsValid(Sensor))
	{
		return FString();
	}
	return FCameraIDManager::Get().GetNewFormatID(Sensor);
}

int32 USensorBPLib::GetSensorOldFormatID(const FString& NewFormatID)
{
	UFusionCamSensor* Sensor = GetSensorByAnyID(NewFormatID);
	if (!IsValid(Sensor))
	{
		return -1;
	}

	TArray<UFusionCamSensor*> SensorList = GetFusionSensorList();
	return SensorList.Find(Sensor);
}

void USensorBPLib::PrintCameraIDMappings()
{
	FCameraIDManager::Get().PrintIDMappings();
}

