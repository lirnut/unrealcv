#include "AssetDiscoveryBPLib.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Blueprint.h"
#include "UnrealcvLog.h"

TArray<FDiscoveredAsset> UAssetDiscoveryBPLib::ScanSpawnableAssets(
	const FString& SearchPath,
	bool bIncludeStaticMeshes,
	bool bIncludeSkeletalMeshes,
	bool bIncludeBlueprints,
	bool bRecursive)
{
	TArray<FDiscoveredAsset> DiscoveredAssets;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	Filter.PackagePaths.Add(FName(*SearchPath));
	Filter.bRecursivePaths = bRecursive;

	if (bIncludeStaticMeshes)
	{
		Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());
	}
	if (bIncludeSkeletalMeshes)
	{
		Filter.ClassPaths.Add(USkeletalMesh::StaticClass()->GetClassPathName());
	}
	if (bIncludeBlueprints)
	{
		Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	}

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	UE_LOG(LogUnrealCV, Log, TEXT("ScanSpawnableAssets: Found %d potential assets in '%s'"), AssetDataList.Num(), *SearchPath);

	for (const FAssetData& AssetData : AssetDataList)
	{
		FDiscoveredAsset Asset;
		Asset.AssetPath = AssetData.GetObjectPathString();
		Asset.AssetName = AssetData.AssetName.ToString();
		Asset.bIsActorBlueprint = false;

		if (AssetData.AssetClassPath == UStaticMesh::StaticClass()->GetClassPathName())
		{
			Asset.AssetType = TEXT("StaticMesh");
			DiscoveredAssets.Add(Asset);
		}
		else if (AssetData.AssetClassPath == USkeletalMesh::StaticClass()->GetClassPathName())
		{
			Asset.AssetType = TEXT("SkeletalMesh");
			DiscoveredAssets.Add(Asset);
		}
		else if (AssetData.AssetClassPath == UBlueprint::StaticClass()->GetClassPathName())
		{
			if (IsActorBlueprint(AssetData))
			{
				Asset.AssetType = TEXT("Blueprint");
				Asset.bIsActorBlueprint = true;
				DiscoveredAssets.Add(Asset);
			}
		}
	}

	UE_LOG(LogUnrealCV, Log, TEXT("ScanSpawnableAssets: Filtered to %d spawnable assets"), DiscoveredAssets.Num());
	return DiscoveredAssets;
}

bool UAssetDiscoveryBPLib::IsActorBlueprint(const FAssetData& AssetData)
{
	FAssetTagValueRef GeneratedClassPath = AssetData.TagsAndValues.FindTag(TEXT("GeneratedClass"));
	if (!GeneratedClassPath.IsSet())
	{
		return false;
	}

	FString ClassPathStr = GeneratedClassPath.AsString();
	if (ClassPathStr.IsEmpty())
	{
		return false;
	}

	FTopLevelAssetPath ClassPath(ClassPathStr);
	UClass* Class = FindObject<UClass>(ClassPath);

	if (!Class)
	{
		FSoftClassPath SoftClassPath(ClassPathStr);
		Class = SoftClassPath.TryLoadClass<UObject>();
	}

	return Class && Class->IsChildOf(AActor::StaticClass());
}
