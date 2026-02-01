#include "PakHandler.h"
#include "BPFunctionLib/PakMountBPLib.h"
#include "UnrealcvLog.h"

void FPakHandler::RegisterCommands()
{
	FDispatcherDelegate Cmd;
	FString Help;

	Cmd = FDispatcherDelegate::CreateRaw(this, &FPakHandler::MountPak);
	Help = "Mount a pak file at runtime. Args: [PakFilePath] [PakOrder=0]";
	CommandDispatcher->BindCommand("vset /pak/mount [str] [uint]", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FPakHandler::UnmountPak);
	Help = "Unmount a pak file. Args: [PakFilePath]";
	CommandDispatcher->BindCommand("vset /pak/unmount [str]", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FPakHandler::GetMountedPaks);
	Help = "Get list of all mounted pak files";
	CommandDispatcher->BindCommand("vget /pak/mounted", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FPakHandler::IsPakMounted);
	Help = "Check if a pak file is mounted. Args: [PakFilePath]";
	CommandDispatcher->BindCommand("vget /pak/ismounted [str]", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FPakHandler::ScanPakAssets);
	Help = "Scan assets in mounted pak. Args: [MountPoint] [bForceRescan=1]";
	CommandDispatcher->BindCommand("vset /pak/scan [str] [uint]", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FPakHandler::LoadAsset);
	Help = "Load an asset from pak. Args: [AssetPath]";
	CommandDispatcher->BindCommand("vget /pak/load [str]", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FPakHandler::GetAssetsInPath);
	Help = "Get all assets in a package path. Args: [PackagePath]";
	CommandDispatcher->BindCommand("vget /pak/assets [str]", Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FPakHandler::RegisterToAssetPool);
	Help = "Register pak assets to AssetPoolManager. Args: [PackagePath] [Category]";
	CommandDispatcher->BindCommand("vset /pak/register [str] [str]", Cmd, Help);
}

FExecStatus FPakHandler::MountPak(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		return FExecStatus::Error("Missing PakFilePath argument");
	}

	FString PakFilePath = Args[0];
	int32 PakOrder = 0;
	if (Args.Num() >= 2)
	{
		PakOrder = FCString::Atoi(*Args[1]);
	}

	bool bSuccess = UPakMountBPLib::MountPakFile(PakFilePath, PakOrder);
	if (bSuccess)
	{
		return FExecStatus::OK(FString::Printf(TEXT("Mounted: %s (Order: %d)"), *PakFilePath, PakOrder));
	}
	else
	{
		return FExecStatus::Error(FString::Printf(TEXT("Failed to mount: %s"), *PakFilePath));
	}
}

FExecStatus FPakHandler::UnmountPak(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		return FExecStatus::Error("Missing PakFilePath argument");
	}

	FString PakFilePath = Args[0];
	bool bSuccess = UPakMountBPLib::UnmountPakFile(PakFilePath);
	if (bSuccess)
	{
		return FExecStatus::OK(FString::Printf(TEXT("Unmounted: %s"), *PakFilePath));
	}
	else
	{
		return FExecStatus::Error(FString::Printf(TEXT("Failed to unmount: %s"), *PakFilePath));
	}
}

FExecStatus FPakHandler::GetMountedPaks(const TArray<FString>& Args)
{
	TArray<FString> MountedPaks = UPakMountBPLib::GetMountedPakFiles();

	FString Result;
	for (int32 i = 0; i < MountedPaks.Num(); i++)
	{
		Result += MountedPaks[i];
		if (i < MountedPaks.Num() - 1)
		{
			Result += TEXT("\n");
		}
	}

	if (Result.IsEmpty())
	{
		Result = TEXT("No pak files mounted");
	}

	return FExecStatus::OK(Result);
}

FExecStatus FPakHandler::IsPakMounted(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		return FExecStatus::Error("Missing PakFilePath argument");
	}

	FString PakFilePath = Args[0];
	bool bMounted = UPakMountBPLib::IsPakFileMounted(PakFilePath);

	return FExecStatus::OK(bMounted ? TEXT("1") : TEXT("0"));
}

FExecStatus FPakHandler::ScanPakAssets(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		return FExecStatus::Error("Missing MountPoint argument");
	}

	FString MountPoint = Args[0];
	bool bForceRescan = true;
	if (Args.Num() >= 2)
	{
		bForceRescan = (FCString::Atoi(*Args[1]) != 0);
	}

	UPakMountBPLib::ScanMountedAssets(MountPoint, bForceRescan);
	return FExecStatus::OK(FString::Printf(TEXT("Scanned: %s"), *MountPoint));
}

FExecStatus FPakHandler::LoadAsset(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		return FExecStatus::Error("Missing AssetPath argument");
	}

	FString AssetPath = Args[0];
	UObject* LoadedAsset = UPakMountBPLib::LoadAssetFromPak(AssetPath, nullptr);

	if (LoadedAsset)
	{
		return FExecStatus::OK(FString::Printf(TEXT("Loaded: %s (Class: %s)"),
			*AssetPath, *LoadedAsset->GetClass()->GetName()));
	}
	else
	{
		return FExecStatus::Error(FString::Printf(TEXT("Failed to load: %s"), *AssetPath));
	}
}

FExecStatus FPakHandler::GetAssetsInPath(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		return FExecStatus::Error("Missing PackagePath argument");
	}

	FString PackagePath = Args[0];
	TArray<FString> AssetPaths = UPakMountBPLib::GetAllAssetsInPath(PackagePath, nullptr);

	FString Result;
	for (int32 i = 0; i < AssetPaths.Num(); i++)
	{
		Result += AssetPaths[i];
		if (i < AssetPaths.Num() - 1)
		{
			Result += TEXT("\n");
		}
	}

	if (Result.IsEmpty())
	{
		Result = FString::Printf(TEXT("No assets found in: %s"), *PackagePath);
	}

	return FExecStatus::OK(Result);
}

FExecStatus FPakHandler::RegisterToAssetPool(const TArray<FString>& Args)
{
	if (Args.Num() < 2)
	{
		return FExecStatus::Error("Missing arguments. Usage: [PackagePath] [Category]");
	}

	FString PackagePath = Args[0];
	FString Category = Args[1];

	bool bSuccess = UPakMountBPLib::RegisterAssetsToAssetPool(PackagePath, Category);
	if (bSuccess)
	{
		return FExecStatus::OK(FString::Printf(TEXT("Registered assets from '%s' to category '%s'"),
			*PackagePath, *Category));
	}
	else
	{
		return FExecStatus::Error(FString::Printf(TEXT("Failed to register assets from: %s"), *PackagePath));
	}
}
