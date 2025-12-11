#pragma once

#include "CoreMinimal.h"

class FMetaHumanCacheManager
{
public:
	static FMetaHumanCacheManager& Get();

	TArray<FString> GetAllMetaHumanPaths();

	void SaveCacheToFile(const TArray<FString>& MetaHumanPaths);

	TArray<FString> LoadCacheFromFile();

	static FString GetCachePath();

private:
	FMetaHumanCacheManager() = default;

	static FMetaHumanCacheManager* Singleton;
};
