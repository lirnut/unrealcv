#include "PakMountBPLib.h"
#include "IPlatformFilePak.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/ARFilter.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "UnrealcvLog.h"
#include "Utils/AssetPoolManager.h"
#include "UObject/UObjectGlobals.h"

bool UPakMountBPLib::MountPakFile(const FString& PakFilePath, int32 PakOrder)
{
	if (!FPaths::FileExists(PakFilePath))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("MountPakFile: File does not exist: %s"), *PakFilePath);
		return false;
	}

	FPakPlatformFile* PakPlatform = (FPakPlatformFile*)(FPlatformFileManager::Get().FindPlatformFile(TEXT("PakFile")));
	if (!PakPlatform)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("MountPakFile: PakPlatformFile not found"));
		return false;
	}

	bool bMounted = PakPlatform->Mount(*PakFilePath, PakOrder);
	if (bMounted)
	{
		UE_LOG(LogUnrealCV, Log, TEXT("MountPakFile: Successfully mounted '%s' with order %d"), *PakFilePath, PakOrder);
	}
	else
	{
		UE_LOG(LogUnrealCV, Error, TEXT("MountPakFile: Failed to mount '%s'"), *PakFilePath);
	}

	return bMounted;
}

bool UPakMountBPLib::UnmountPakFile(const FString& PakFilePath)
{
	FPakPlatformFile* PakPlatform = (FPakPlatformFile*)(FPlatformFileManager::Get().FindPlatformFile(TEXT("PakFile")));
	if (!PakPlatform)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("UnmountPakFile: PakPlatformFile not found"));
		return false;
	}

	bool bUnmounted = PakPlatform->Unmount(*PakFilePath);
	if (bUnmounted)
	{
		UE_LOG(LogUnrealCV, Log, TEXT("UnmountPakFile: Successfully unmounted '%s'"), *PakFilePath);
	}
	else
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("UnmountPakFile: Failed to unmount '%s' (may not be mounted)"), *PakFilePath);
	}

	return bUnmounted;
}

void UPakMountBPLib::ScanMountedAssets(const FString& MountPoint, bool bForceRescan)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FString> PathsToScan;
	PathsToScan.Add(MountPoint);

	UE_LOG(LogUnrealCV, Log, TEXT("ScanMountedAssets: Scanning path '%s' (ForceRescan=%d)"), *MountPoint, bForceRescan);

	AssetRegistry.ScanPathsSynchronous(PathsToScan, bForceRescan);

	UE_LOG(LogUnrealCV, Log, TEXT("ScanMountedAssets: Scan completed for '%s'"), *MountPoint);
}

TArray<FString> UPakMountBPLib::GetMountedPakFiles()
{
	TArray<FString> MountedPaks;

	FPakPlatformFile* PakPlatform = (FPakPlatformFile*)(FPlatformFileManager::Get().FindPlatformFile(TEXT("PakFile")));
	if (!PakPlatform)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("GetMountedPakFiles: PakPlatformFile not found"));
		return MountedPaks;
	}

	PakPlatform->GetMountedPakFilenames(MountedPaks);

	UE_LOG(LogUnrealCV, Log, TEXT("GetMountedPakFiles: Found %d mounted pak files"), MountedPaks.Num());

	return MountedPaks;
}

bool UPakMountBPLib::IsPakFileMounted(const FString& PakFilePath)
{
	TArray<FString> MountedPaks = GetMountedPakFiles();
	return MountedPaks.Contains(PakFilePath);
}

UObject* UPakMountBPLib::LoadAssetFromPak(const FString& AssetPath, UClass* AssetClass)
{
	if (AssetPath.IsEmpty())
	{
		UE_LOG(LogUnrealCV, Error, TEXT("LoadAssetFromPak: AssetPath is empty"));
		return nullptr;
	}

	if (!AssetClass)
	{
		AssetClass = UObject::StaticClass();
	}

	UObject* LoadedAsset = StaticLoadObject(AssetClass, nullptr, *AssetPath);

	if (LoadedAsset)
	{
		UE_LOG(LogUnrealCV, Log, TEXT("LoadAssetFromPak: Successfully loaded '%s'"), *AssetPath);
	}
	else
	{
		UE_LOG(LogUnrealCV, Error, TEXT("LoadAssetFromPak: Failed to load '%s'"), *AssetPath);
	}

	return LoadedAsset;
}

TArray<FString> UPakMountBPLib::GetAllAssetsInPath(const FString& PackagePath, UClass* AssetClass)
{
	TArray<FString> AssetPaths;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	Filter.PackagePaths.Add(FName(*PackagePath));
	Filter.bRecursivePaths = true;

	if (AssetClass)
	{
		Filter.ClassPaths.Add(AssetClass->GetClassPathName());
	}

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	for (const FAssetData& AssetData : AssetDataList)
	{
		AssetPaths.Add(AssetData.GetObjectPathString());
	}

	UE_LOG(LogUnrealCV, Log, TEXT("GetAllAssetsInPath: Found %d assets in '%s'"), AssetPaths.Num(), *PackagePath);

	return AssetPaths;
}

bool UPakMountBPLib::RegisterAssetsToAssetPool(const FString& PackagePath, const FString& Category)
{
	TArray<FString> AssetPaths = GetAllAssetsInPath(PackagePath, nullptr);

	if (AssetPaths.Num() == 0)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("RegisterAssetsToAssetPool: No assets found in '%s'"), *PackagePath);
		return false;
	}

	FAssetPoolManager& PoolManager = FAssetPoolManager::Get();

	for (const FString& AssetPath : AssetPaths)
	{
		TMap<FString, FString> Metadata;
		Metadata.Add(TEXT("Path"), AssetPath);

		if (AssetPath.Contains(TEXT("BP_")) || AssetPath.EndsWith(TEXT("_C")))
		{
			Metadata.Add(TEXT("Type"), TEXT("Blueprint"));
		}
		else if (AssetPath.Contains(TEXT("SM_")))
		{
			Metadata.Add(TEXT("Type"), TEXT("StaticMesh"));
		}
		else
		{
			Metadata.Add(TEXT("Type"), TEXT("StaticMesh"));
		}

		PoolManager.RegisterAssetWithMetadata(Category, Metadata);
	}

	UE_LOG(LogUnrealCV, Log, TEXT("RegisterAssetsToAssetPool: Registered %d assets to category '%s'"), AssetPaths.Num(), *Category);

	return true;
}
