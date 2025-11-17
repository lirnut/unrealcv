#include "BPFunctionLib/MetaHumanBPLib.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/ARFilter.h"
#include "Engine/Blueprint.h"

TArray<FString> UMetaHumanBPLib::GetAllMetaHumanBlueprintPaths()
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
		FString ObjectPath = AssetData.GetObjectPathString();
		MetaHumanPaths.Add(ObjectPath);
	}

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
