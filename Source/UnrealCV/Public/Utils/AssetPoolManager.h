// Copyright 2025 UnrealCV Team. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

/**
 * Asset Pool Manager - Manages asset paths for randomized scene generation.
 *
 * This singleton class maintains pools of asset paths organized by category.
 * Used by USceneCompositionBPLib to spawn random foreground actors and occluders.
 *
 * Asset paths are hardcoded in the constructor for Phase 1.
 * Future: Migrate to config file (JSON/INI) for easier asset management.
 */
class UNREALCV_API FAssetPoolManager
{
public:
	static FAssetPoolManager& Get();

	FString GetRandomAsset(const FString& Category);
	TArray<FString> GetAssetsInCategory(const FString& Category) const;
	bool HasCategory(const FString& Category) const;
	TArray<FString> GetAllCategories() const;
	int32 GetAssetCount(const FString& Category) const;

	void RegisterAsset(const FString& Category, const FString& AssetPath);

private:
	FAssetPoolManager();
	FAssetPoolManager(const FAssetPoolManager&) = delete;
	FAssetPoolManager& operator=(const FAssetPoolManager&) = delete;

	TMap<FString, TArray<FString>> AssetPools;
};
