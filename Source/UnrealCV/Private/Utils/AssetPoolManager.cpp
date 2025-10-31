// Copyright 2025 UnrealCV Team. All Rights Reserved.
#include "AssetPoolManager.h"
#include "UnrealcvLog.h"

FAssetPoolManager& FAssetPoolManager::Get()
{
	static FAssetPoolManager Instance;
	return Instance;
}

FAssetPoolManager::FAssetPoolManager()
{
	UE_LOG(LogUnrealCV, Log, TEXT("FAssetPoolManager initialized"));
       // ========== Foreground Assets ==========

	// Human - Different ages, skin colors, genders (SOW requirement)
	AssetPools.Add(TEXT("Foreground_Human"), {
		// Adults
		TEXT("/Game/SocialAnimsBundle/Demo/Characters/Mannequins/girl_01_aAS_Talk7.girl_01_aAS_Talk7"),

		// // Elderly
		// TEXT("/Game/Assets/Characters/Human_Elder_Male_01"),

		// // Youth
		// TEXT("/Game/Assets/Characters/Human_Teen_Male_01"),

		// // Children
		// TEXT("/Game/Assets/Characters/Human_Child_Male_01"),

		// // Infants
		// TEXT("/Game/Assets/Characters/Human_Infant_01"),
	});

	// // Pets - Cats (5+ breeds, SOW requirement)
	// AssetPools.Add(TEXT("Foreground_Pet_Cat"), {
	//      TEXT("/Game/Assets/Animals/Cat_Tabby"),
	// });

	// // Pets - Dogs (5+ breeds, SOW requirement)
	// AssetPools.Add(TEXT("Foreground_Pet_Dog"), {
	//      TEXT("/Game/Assets/Animals/Dog_Beagle"),
	// });

	// // Pets - Birds (5+ breeds, SOW requirement)
	// AssetPools.Add(TEXT("Foreground_Pet_Bird"), {
	//      TEXT("/Game/Assets/Animals/Bird_Finch"),
	// });

	// // Pets - Exotic (SOW requirement: hamster, turtle, lizard, snake, spider)
	// AssetPools.Add(TEXT("Foreground_Pet_Exotic"), {
	//      TEXT("/Game/Assets/Animals/Spider_Tarantula"),
	// });

	// // Vehicles
	// AssetPools.Add(TEXT("Foreground_Vehicle"), {
	//      TEXT("/Game/Assets/Vehicles/Bus_01"),
	// });

	// // Buildings (SOW requirement: 20+)
	// AssetPools.Add(TEXT("Foreground_Building"), {
	//      TEXT("/Game/Assets/Buildings/Gazebo_01"),
	// });

	// // Food (SOW requirement: 20+)
	// AssetPools.Add(TEXT("Foreground_Food"), {
	//      TEXT("/Game/Assets/Food/Juice"),
	// });

	// // Books (SOW requirement: 20+)
	// AssetPools.Add(TEXT("Foreground_Book"), {
	//      TEXT("/Game/Assets/Books/Book_Reference"),
	// });

	// // Tableware (SOW requirement: 10+)
	// AssetPools.Add(TEXT("Foreground_Tableware"), {
	//      TEXT("/Game/Assets/Tableware/Pan"),
	// });

	// // Plants (SOW requirement: 10+)
	// AssetPools.Add(TEXT("Foreground_Plant"), {
	//      TEXT("/Game/Assets/Plants/Ivy"),
	// });

	// ========== Occluder Assets ==========

	// // Trees (natural occluders)
	// AssetPools.Add(TEXT("Occluder_Tree"), {
	//      TEXT("/Game/Assets/Environment/Tree_Bamboo_01"),
	// });

	// // Pillars (architectural occluders)
	// AssetPools.Add(TEXT("Occluder_Pillar"), {
	//      TEXT("/Game/Assets/Architecture/Post_Modern_01"),
	// });

	// // Walls (structural occluders)
	// AssetPools.Add(TEXT("Occluder_Wall"), {
	//      TEXT("/Game/Assets/Architecture/Screen_Folding_01"),
	// });

	// // Furniture (indoor occluders)
	// AssetPools.Add(TEXT("Occluder_Furniture"), {
	//      TEXT("/Game/Assets/Furniture/Shelf_01"),
	// });

	// // Vehicles (can also be occluders)
	// AssetPools.Add(TEXT("Occluder_Vehicle"), {
	//      TEXT("/Game/Assets/Vehicles/Bus_Parked_01"),
	// });

	// // Rocks (natural occluders)
	// AssetPools.Add(TEXT("Occluder_Rock"), {
	//      TEXT("/Game/Assets/Environment/Stone_Formation_01"),
	// });

	// // Bushes/Shrubs (natural occluders)
	// AssetPools.Add(TEXT("Occluder_Bush"), {
	//      TEXT("/Game/Assets/Environment/Grass_Tall_01"),
	// });

	// Urban objects (street occluders)
	AssetPools.Add(TEXT("Occluder_Urban"), {
		TEXT("/Game/SuburbNeighborhoodHousePack/Meshes_usable/Exterior_props_usable/BP_Trashbin.BP_Trashbin"),
	});

	// // Log asset pool statistics
	// for (const auto& Pair : AssetPools)
	// {
	// 	UE_LOG(LogUnrealCV, Log, TEXT("  Category '%s': %d assets"), *Pair.Key, Pair.Value.Num());
	// }
}

FString FAssetPoolManager::GetRandomAsset(const FString& Category)
{
	if (!AssetPools.Contains(Category))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("FAssetPoolManager::GetRandomAsset: Category '%s' not found"), *Category);
		return FString();
	}

	const TArray<FString>& Assets = AssetPools[Category];
	if (Assets.Num() == 0)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("FAssetPoolManager::GetRandomAsset: Category '%s' is empty"), *Category);
		return FString();
	}

	int32 RandomIndex = FMath::RandRange(0, Assets.Num() - 1);
	return Assets[RandomIndex];
}

TArray<FString> FAssetPoolManager::GetAssetsInCategory(const FString& Category) const
{
	if (!AssetPools.Contains(Category))
	{
		return TArray<FString>();
	}
	return AssetPools[Category];
}

bool FAssetPoolManager::HasCategory(const FString& Category) const
{
	return AssetPools.Contains(Category);
}

TArray<FString> FAssetPoolManager::GetAllCategories() const
{
	TArray<FString> Categories;
	AssetPools.GetKeys(Categories);
	return Categories;
}

int32 FAssetPoolManager::GetAssetCount(const FString& Category) const
{
	if (!AssetPools.Contains(Category))
	{
		return 0;
	}
	return AssetPools[Category].Num();
}

void FAssetPoolManager::RegisterAsset(const FString& Category, const FString& AssetPath)
{
	if (!AssetPools.Contains(Category))
	{
		AssetPools.Add(Category, TArray<FString>());
	}
	if (!AssetPools[Category].Contains(AssetPath))
	{
		AssetPools[Category].Add(AssetPath);
	}
}
