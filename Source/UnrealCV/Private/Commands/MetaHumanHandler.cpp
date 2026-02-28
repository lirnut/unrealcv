#include "MetaHumanHandler.h"
#include "BPFunctionLib/MetaHumanBPLib.h"
#include "Utils/MetaHumanCacheManager.h"

FExecStatus FMetaHumanHandler::GetAllMetaHumanPaths(const TArray<FString>& Args)
{
	TArray<FString> Paths = UMetaHumanBPLib::GetAllMetaHumanBlueprintPaths();

	if (Paths.Num() == 0)
	{
		return FExecStatus::Error("No MetaHuman found");
	}

	FString Result;
	for (const FString& Path : Paths)
	{
		Result += Path + TEXT("\n");
	}
	return FExecStatus::OK(Result);
}

FExecStatus FMetaHumanHandler::UpdateCache(const TArray<FString>& Args)
{
	FMetaHumanCacheManager& CacheManager = FMetaHumanCacheManager::Get();
	TArray<FString> Paths = CacheManager.GetAllMetaHumanPaths();

	if (Paths.Num() == 0)
	{
		return FExecStatus::Error("No MetaHuman found in AssetRegistry");
	}

	CacheManager.SaveCacheToFile(Paths);
	return FExecStatus::OK(FString::Printf(TEXT("Cached %d MetaHumans"), Paths.Num()));
}

FExecStatus FMetaHumanHandler::GetCachePath(const TArray<FString>& Args)
{
	FString CachePath = FMetaHumanCacheManager::GetCachePath();
	return FExecStatus::OK(CachePath);
}

FExecStatus FMetaHumanHandler::FilterBatchGenerated(const TArray<FString>& Args)
{
	FMetaHumanCacheManager& CacheManager = FMetaHumanCacheManager::Get();
	TArray<FString> AllPaths = CacheManager.LoadCacheFromFile();

	if (AllPaths.Num() == 0)
	{
		AllPaths = CacheManager.GetAllMetaHumanPaths();
	}

	TArray<FString> FilteredPaths = UMetaHumanBPLib::FilterBatchGeneratedMetaHumans(AllPaths);

	FString Result;
	for (const FString& Path : FilteredPaths)
	{
		Result += Path + TEXT("\n");
	}
	return FExecStatus::OK(Result);
}

void FMetaHumanHandler::RegisterCommands()
{
	FDispatcherDelegate Cmd;
	FString Help;

	Cmd = FDispatcherDelegate::CreateRaw(this, &FMetaHumanHandler::GetAllMetaHumanPaths);
	Help = "Get all MetaHuman blueprint paths (scans AssetRegistry and saves cache)";
	CommandDispatcher->BindCommand(TEXT("vget /metahuman/all_paths"), Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FMetaHumanHandler::UpdateCache);
	Help = "Scan AssetRegistry and update MetaHuman cache file";
	CommandDispatcher->BindCommand(TEXT("vset /metahuman/update_cache"), Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FMetaHumanHandler::GetCachePath);
	Help = "Get the path to MetaHuman cache file";
	CommandDispatcher->BindCommand(TEXT("vget /metahuman/cache_path"), Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FMetaHumanHandler::FilterBatchGenerated);
	Help = "Filter batch generated MetaHumans from cache";
	CommandDispatcher->BindCommand(TEXT("vget /metahuman/filter_batch"), Cmd, Help);
}
