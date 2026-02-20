#include "Utils/MetaHumanCacheManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/ARFilter.h"
#include "Engine/Blueprint.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Engine/AssetManager.h"
#include "Engine/PrimaryAssetLabel.h"

FMetaHumanCacheManager* FMetaHumanCacheManager::Singleton = nullptr;

FMetaHumanCacheManager& FMetaHumanCacheManager::Get()
{
	if (!Singleton)
	{
		Singleton = new FMetaHumanCacheManager();
	}
	return *Singleton;
}

FString FMetaHumanCacheManager::GetCachePath()
{
	return FPaths::ProjectSavedDir() / TEXT("MetaHumanCache.json");
}

TArray<FString> FMetaHumanCacheManager::GetAllMetaHumanPaths()
{
	TArray<FString> MetaHumanPaths;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	Filter.PackagePaths.Add("/Game/MetaHumans");
	Filter.bRecursivePaths = true;
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	for (const FAssetData& AssetData : AssetDataList)
	{
		if (!AssetData.IsValid())
		{
			continue;
		}

		FString ObjectPath = AssetData.GetObjectPathString();
		MetaHumanPaths.Add(ObjectPath);
	}

	UE_LOG(LogTemp, Log, TEXT("GetAllMetaHumanPaths: Found %d MetaHumans from AssetRegistry"), MetaHumanPaths.Num());

	return MetaHumanPaths;
}

void FMetaHumanCacheManager::SaveCacheToFile(const TArray<FString>& MetaHumanPaths)
{
	TSharedPtr<FJsonObject> RootObject = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> PathsArray;

	for (const FString& Path : MetaHumanPaths)
	{
		PathsArray.Add(MakeShared<FJsonValueString>(Path));
	}

	RootObject->SetArrayField(TEXT("MetaHumanPaths"), PathsArray);
	RootObject->SetStringField(TEXT("CacheTime"), FDateTime::Now().ToString());

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);

	FString CachePath = GetCachePath();
	if (FFileHelper::SaveStringToFile(OutputString, *CachePath))
	{
		UE_LOG(LogTemp, Log, TEXT("MetaHumanCache saved to: %s (%d entries)"), *CachePath, MetaHumanPaths.Num());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to save MetaHumanCache to: %s"), *CachePath);
	}
}

TArray<FString> FMetaHumanCacheManager::LoadCacheFromFile()
{
	TArray<FString> MetaHumanPaths;

	FString CachePath = GetCachePath();
	FString JsonContent;

	if (!FFileHelper::LoadFileToString(JsonContent, *CachePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("MetaHumanCache file not found: %s"), *CachePath);
		return MetaHumanPaths;
	}

	TSharedPtr<FJsonObject> RootObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);

	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to parse MetaHumanCache JSON from: %s"), *CachePath);
		return MetaHumanPaths;
	}

	TArray<TSharedPtr<FJsonValue>> PathsArray = RootObject->GetArrayField(TEXT("MetaHumanPaths"));
	for (const TSharedPtr<FJsonValue>& Value : PathsArray)
	{
		MetaHumanPaths.Add(Value->AsString());
	}

	FString CacheTime = RootObject->GetStringField(TEXT("CacheTime"));
	UE_LOG(LogTemp, Log, TEXT("MetaHumanCache loaded from: %s (Cache time: %s, %d entries)"), *CachePath, *CacheTime, MetaHumanPaths.Num());

	return MetaHumanPaths;
}

void FMetaHumanCacheManager::RegisterWithAssetManager()
{
	if (!UAssetManager::IsInitialized())
	{
		UE_LOG(LogTemp, Warning, TEXT("MetaHumanCacheManager: AssetManager not initialized"));
		return;
	}

	UAssetManager& Manager = UAssetManager::Get();
	FPrimaryAssetType MetaHumanType(TEXT("MetaHuman"));

	TArray<FString> Paths = LoadCacheFromFile();
	if (Paths.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("MetaHumanCacheManager: No cached paths, using live scan"));
		Paths = GetAllMetaHumanPaths();
	}

	for (const FString& Path : Paths)
	{
		FSoftObjectPath SoftPath(Path);
		FAssetData AssetData;
		if (Manager.GetAssetDataForPath(SoftPath, AssetData))
		{
			FPrimaryAssetId AssetId = Manager.ExtractPrimaryAssetIdFromData(AssetData, MetaHumanType);
			if (AssetId.IsValid())
			{
				FAssetBundleData BundleData;
				BundleData.AddBundleAsset(FName("MetaHuman"), FTopLevelAssetPath(Path));
				Manager.AddDynamicAsset(AssetId, SoftPath, BundleData);
				UE_LOG(LogTemp, Log, TEXT("MetaHumanCacheManager: Registered %s"), *Path);
			}
		}
	}

	FPrimaryAssetRules Rules;
	Rules.Priority = 64;
	Rules.ChunkId = 0;
	Rules.bApplyRecursively = true;
	Rules.CookRule = EPrimaryAssetCookRule::AlwaysCook;
	Manager.SetPrimaryAssetTypeRules(MetaHumanType, Rules);

	UE_LOG(LogTemp, Log, TEXT("MetaHumanCacheManager: Registered %d MetaHumans with AssetManager"), Paths.Num());
}

void FMetaHumanCacheManager::UnregisterFromAssetManager()
{
	if (!UAssetManager::IsInitialized())
	{
		return;
	}

	UAssetManager& Manager = UAssetManager::Get();

	TArray<FString> Paths = LoadCacheFromFile();
	for (const FString& Path : Paths)
	{
		FPrimaryAssetId AssetId(FPrimaryAssetType(TEXT("MetaHuman")), FName(*Path));
		Manager.UnloadPrimaryAsset(AssetId);
	}

	UE_LOG(LogTemp, Log, TEXT("MetaHumanCacheManager: Unregistered %d MetaHumans from AssetManager"), Paths.Num());
}
