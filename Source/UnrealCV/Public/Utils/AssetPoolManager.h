// Copyright 2025 UnrealCV Team. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

/**
 * Asset Pool Manager - Manages asset metadata for randomized scene generation.
 *
 * This singleton class maintains pools of asset metadata organized by category.
 * Used by USceneCompositionBPLib to spawn random foreground actors and occluders.
 *
 * Each asset stores metadata as TMap<FString, FString>:
 * - "Path": Main asset path (REQUIRED)
 * - "Type": Asset type (REQUIRED: "StaticMesh" | "Blueprint" | "SM+AnimSeq")
 * - "AnimSequence": Animation path (REQUIRED for "SM+AnimSeq" type only)
 *
 * Asset paths are hardcoded in the constructor for Phase 1.
 * Future: Migrate to config file (JSON/INI) for easier asset management.
 */
class UNREALCV_API FAssetPoolManager
{
public:
	static FAssetPoolManager& Get();

	void LoadStableAssetsPack();
	
	FString GetRandomAsset(const FString& Category);
	TMap<FString, FString> GetRandomAssetMetadata(const FString& Category);
	TArray<FString> GetAssetsInCategory(const FString& Category) const;
	bool HasCategory(const FString& Category) const;
	TArray<FString> GetAllCategories() const;
	int32 GetAssetCount(const FString& Category) const;

	void RegisterAsset(const FString& Category, const FString& AssetPath);
	void RegisterAssetWithMetadata(const FString& Category, const TMap<FString, FString>& Metadata);

	static bool ValidateMetadata(const TMap<FString, FString>& Metadata, FString& OutErrorMessage);

	void PrintAllAssets() const;

private:
	FAssetPoolManager();
	FAssetPoolManager(const FAssetPoolManager&) = delete;
	FAssetPoolManager& operator=(const FAssetPoolManager&) = delete;

	TMap<FString, TArray<TMap<FString, FString>>> AssetPools;
};
