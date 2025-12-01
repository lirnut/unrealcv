// Copyright 2025 UnrealCV Team. All Rights Reserved.
#include "AssetPoolManager.h"
#include "UnrealcvLog.h"
#include "BPFunctionLib/MetaHumanBPLib.h"

FAssetPoolManager& FAssetPoolManager::Get()
{
	static FAssetPoolManager Instance;
	return Instance;
}

FAssetPoolManager::FAssetPoolManager()
{
	UE_LOG(LogUnrealCV, Log, TEXT("FAssetPoolManager initialized"));
	// LoadStableAssetsPack();
}

void FAssetPoolManager::LoadStableAssetsPack()
{
    // ========== Foreground Assets ==========

	// Human - Different ages, skin colors, genders (SOW requirement)
	AssetPools.Add(TEXT("Foreground_Human"), {
		// // Adults
		// {
		// 	{"Path", TEXT("/Game/HumanCharacter/Businessmen/Man_In_Suit/Meshes/Man_in_Jaket.Man_in_Jaket")},
		// 	{"Type", TEXT("SM+AnimSeq")},
		// 	{"AnimSequence", TEXT("/Game/SocialAnimsBundle/SocialNPCAnimations/Animations/AS_Talk4.AS_Talk4")}
		// }

		// // Elderly
		// {{"Path", TEXT("/Game/Assets/Characters/Human_Elder_Male_01")}, {"Type", TEXT("Blueprint")}},

		// // Youth
		// {{"Path", TEXT("/Game/Assets/Characters/Human_Teen_Male_01")}, {"Type", TEXT("Blueprint")}},

		// // Children
		// {{"Path", TEXT("/Game/Assets/Characters/Human_Child_Male_01")}, {"Type", TEXT("Blueprint")}},

		// // Infants
		// {{"Path", TEXT("/Game/Assets/Characters/Human_Infant_01")}, {"Type", TEXT("Blueprint")}},
	});
	TArray<FString> MetaHumanPaths = UMetaHumanBPLib::SetupAllMetaHumansWithAnimation(TEXT("/Game/MetaHumans/ABP_RandomIdle.ABP_RandomIdle_C"));
	for (const FString& MetaHumanPath : MetaHumanPaths)
	{
		AssetPools[TEXT("Foreground_Human")].Add({
			{"Path", MetaHumanPath},
			{"Type", TEXT("Blueprint")}
		});
	}



	TArray<FString> HumanAnimations = {
		TEXT("/Game/SocialAnimsBundle/SocialNPCAnimations/Animations/AS_Talk7.AS_Talk7"),
		TEXT("/Game/SocialAnimsBundle/SocialNPCAnimations/Animations/AS_Talk6.AS_Talk6"),
		TEXT("/Game/SocialAnimsBundle/SocialNPCAnimations/Animations/AS_Talk5.AS_Talk5"),
		TEXT("/Game/SocialAnimsBundle/SocialNPCAnimations/Animations/AS_Talk4.AS_Talk4"),
		TEXT("/Game/SocialAnimsBundle/SocialNPCAnimations/Animations/AS_Talk3.AS_Talk3"),
		TEXT("/Game/SocialAnimsBundle/SocialNPCAnimations/Animations/AS_Talk2.AS_Talk2"),
		TEXT("/Game/SocialAnimsBundle/SocialNPCAnimations/Animations/AS_Talk1.AS_Talk1"),
		TEXT("/Game/SocialAnimsBundle/SocialNPCAnimations/Animations/AS_Talk.AS_Talk"),
		TEXT("/Game/SocialAnimsBundle/SocialNPCAnimations/Animations/AS_Thinking.AS_Thinking"),
		TEXT("/Game/SocialAnimsBundle/SocialNPCAnimations/Animations/AS_Smoking.AS_Smoking"),
		TEXT("/Game/SocialAnimsBundle/SocialNPCAnimations/Animations/AS_OverThere2.AS_OverThere2"),
		TEXT("/Game/SocialAnimsBundle/SocialNPCAnimations/Animations/AS_OverThere.AS_OverThere"),
		TEXT("/Game/SocialAnimsBundle/SocialNPCAnimations/Animations/AS_OverHere.AS_OverHere"),
		TEXT("/Game/SocialAnimsBundle/SocialNPCAnimations/Animations/AS_No.AS_No")
	};
	TArray<FString> SKMs = {
		TEXT("/Game/HumanCharacter/Businessmen/Man_In_Suit/Meshes/Man_in_Jaket.Man_in_Jaket"),
		TEXT("/Game/HumanCharacter/Businessmen/Man_In_Suit/Meshes/Man_in_Jaket_NoGlass.Man_in_Jaket_NoGlass"),
		TEXT("/Game/HumanCharacter/Businessmen/Man_In_Suit/Meshes/Man_in_ShortGolf.Man_in_ShortGolf"),
		TEXT("/Game/HumanCharacter/Businessmen/Man_In_Suit/Meshes/Man_in_ShortGolf_NoGlass.Man_in_ShortGolf_NoGlass"),
		TEXT("/Game/HumanCharacter/Businessmen/Man_in_Shirt/Meshes/Man_In_Shirt.Man_In_Shirt"),
		TEXT("/Game/HumanCharacter/Businessmen/Man_in_Shirt/Meshes/Man_In_Shirt_Suit.Man_In_Shirt_Suit"),
		TEXT("/Game/HumanCharacter/Businessmen/Man_in_Shirt/Meshes/Man_In_Shirt_Vest.Man_In_Shirt_Vest"),
		TEXT("/Game/HumanCharacter/Businessmen/Man_In_Polo/Meshes/Man_In_Polo.Man_In_Polo"),
		TEXT("/Game/HumanCharacter/Businessmen/Man_In_Polo/Meshes/Man_In_Polo_Glasses.Man_In_Polo_Glasses")
	};

	AssetPools.Add(TEXT("Foreground_Human_SKM"), {});
	for (FString& Anim : HumanAnimations)
	{
		for (FString& SKM : SKMs)
		{
			AssetPools["Foreground_Human_SKM"].Add({
				{"Path", SKM},
				{"Type", TEXT("SM+AnimSeq")},
				{"AnimSequence", Anim}
			});
		}
	}

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
	// AssetPools.Add(TEXT("Occluder_Urban"), {
	// 	TEXT("/Game/SuburbNeighborhoodHousePack/Meshes_usable/Exterior_props_usable/BP_Trashbin.BP_Trashbin"),
	// });
	AssetPools.Add(TEXT("Occluder_All"), {});
	TArray<FString> Occluders = {
		TEXT("/Game/SuburbNeighborhoodHousePack/Meshes_usable/Exterior_props_usable/BP_Trashbin.BP_Trashbin"),
		TEXT("/Game/SuburbNeighborhoodHousePack/Meshes_usable/Exterior_props_usable/SM_Trashbag.SM_Trashbag"),
		TEXT("/Game/SuburbNeighborhoodHousePack/Meshes_usable/Exterior_props_usable/SM_Lamp_Garden_Pilar.SM_Lamp_Garden_Pilar"),
		TEXT("/Game/SuburbNeighborhoodHousePack/Meshes_usable/Exterior_props_usable/SM_Sun_Umbrella.SM_Sun_Umbrella"),

		TEXT("/Game/DogRobot/BP_DogRobotSimple.BP_DogRobotSimple"),
		TEXT("/Game/Animal_pack_ultra_2/BP_SlowDogRobotSimple.BP_SlowDogRobotSimple"),
		TEXT("/Game/Animal_pack_ultra_2/BP_StaticPetSimple.BP_StaticPetSimple"),
		TEXT("/Game/Animal_pack_ultra_2/BP_SlowPetSimple.BP_SlowPetSimple"),
		///////////
		TEXT("/Game/DogRobot/BP_DogRobotSimple.BP_DogRobotSimple"),
		TEXT("/Game/Animal_pack_ultra_2/BP_SlowDogRobotSimple.BP_SlowDogRobotSimple"),
		TEXT("/Game/Animal_pack_ultra_2/BP_StaticPetSimple.BP_StaticPetSimple"),
		TEXT("/Game/Animal_pack_ultra_2/BP_SlowPetSimple.BP_SlowPetSimple"),
		// TEXT("/Game/Animal_pack_ultra_2/BP_Beagle.BP_Beagle"),

		TEXT("/Game/SuburbNeighborhoodHousePack/Meshes_usable/Foliage_usable/SM_Plant_HedgeBush.SM_Plant_HedgeBush"),
		TEXT("/Game/SuburbNeighborhoodHousePack/Meshes_usable/Foliage_usable/SM_Plant_FlowersWhiteBush.SM_Plant_FlowersWhiteBush"),
		TEXT("/Game/SuburbNeighborhoodHousePack/Meshes_usable/Foliage_usable/SM_Plant_Box_C.SM_Plant_Box_C"),
		// TEXT("/Game/SuburbNeighborhoodHousePack/Meshes_usable/Foliage_usable/SM_Plant_Box_D.SM_Plant_Box_D"),
		TEXT("/Game/SuburbNeighborhoodHousePack/Meshes_usable/Foliage_usable/SM_Plant_Box_B.SM_Plant_Box_B"),
		TEXT("/Game/SuburbNeighborhoodHousePack/Meshes_usable/Foliage_usable/SM_Plant_Bush_B.SM_Plant_Bush_B"),
		// TEXT("/Game/SuburbNeighborhoodHousePack/Meshes_usable/Foliage_usable/SM_Plant_Bush_A.SM_Plant_Bush_A"),
		TEXT("/Game/SuburbNeighborhoodHousePack/Meshes_usable/Foliage_usable/SM_Plant_Box_Hanging.SM_Plant_Box_Hanging"),
		// TEXT("/Game/SuburbNeighborhoodHousePack/Meshes_usable/Foliage_usable/SM_Plant_Drygrass.SM_Plant_Drygrass"),

	};

	for (const FString& Path : Occluders)
	{
		if (Path.Contains(TEXT("BP_")))
		{
			AssetPools[TEXT("Occluder_All")].Add({
				{TEXT("Path"), Path},
				{TEXT("Type"), TEXT("Blueprint")},
			});
		}
		else if(Path.Contains(TEXT("SM_")))
		{
			AssetPools[TEXT("Occluder_All")].Add({
				{TEXT("Path"), Path},
				{TEXT("Type"), TEXT("StaticMesh")},
			});
		}
		else
		{
			UE_LOG(LogUnrealCV, Error, TEXT("FAssetPoolManager::AddAssetToPool: Unknown asset type for path '%s'"), *Path);
		}
	}

	// Log asset pool statistics
	for (const auto& Pair : AssetPools)
	{
		UE_LOG(LogUnrealCV, Log, TEXT("  Category '%s': %d assets"), *Pair.Key, Pair.Value.Num());
	}
}

FString FAssetPoolManager::GetRandomAsset(const FString& Category)
{
	TMap<FString, FString> Metadata = GetRandomAssetMetadata(Category);
	if (Metadata.Contains(TEXT("Path")))
	{
		return Metadata[TEXT("Path")];
	}
	return FString();
}

TMap<FString, FString> FAssetPoolManager::GetRandomAssetMetadata(const FString& Category)
{
	if (!AssetPools.Contains(Category))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("FAssetPoolManager::GetRandomAssetMetadata: Category '%s' not found"), *Category);
		return TMap<FString, FString>();
	}

	const TArray<TMap<FString, FString>>& Assets = AssetPools[Category];
	if (Assets.Num() == 0)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("FAssetPoolManager::GetRandomAssetMetadata: Category '%s' is empty"), *Category);
		return TMap<FString, FString>();
	}

	int32 RandomIndex = FMath::RandRange(0, Assets.Num() - 1);
	const TMap<FString, FString>& SelectedAsset = Assets[RandomIndex];

	FString AssetPath = SelectedAsset.Contains(TEXT("Path")) ? SelectedAsset[TEXT("Path")] : TEXT("(no path)");
	UE_LOG(LogUnrealCV, Log, TEXT("FAssetPoolManager::GetRandomAssetMetadata: Category='%s', Selected [%d/%d]: '%s'"),
		*Category, RandomIndex, Assets.Num(), *AssetPath);

	return SelectedAsset;
}

TArray<FString> FAssetPoolManager::GetAssetsInCategory(const FString& Category) const
{
	TArray<FString> Paths;
	if (!AssetPools.Contains(Category))
	{
		return Paths;
	}

	const TArray<TMap<FString, FString>>& Assets = AssetPools[Category];
	for (const TMap<FString, FString>& Metadata : Assets)
	{
		if (Metadata.Contains(TEXT("Path")))
		{
			Paths.Add(Metadata[TEXT("Path")]);
		}
	}
	return Paths;
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
	TMap<FString, FString> Metadata;
	Metadata.Add(TEXT("Path"), AssetPath);
	Metadata.Add(TEXT("Type"), TEXT("StaticMesh"));
	RegisterAssetWithMetadata(Category, Metadata);
}

bool FAssetPoolManager::ValidateMetadata(const TMap<FString, FString>& Metadata, FString& OutErrorMessage)
{
	if (!Metadata.Contains(TEXT("Path")))
	{
		OutErrorMessage = TEXT("Metadata must contain 'Path'");
		return false;
	}

	if (!Metadata.Contains(TEXT("Type")))
	{
		OutErrorMessage = TEXT("Metadata must contain 'Type' (valid values: 'StaticMesh', 'Blueprint', 'SM+AnimSeq')");
		return false;
	}

	const FString& Type = Metadata[TEXT("Type")];
	if (Type != TEXT("StaticMesh") && Type != TEXT("Blueprint") && Type != TEXT("SM+AnimSeq"))
	{
		OutErrorMessage = FString::Printf(TEXT("Invalid Type '%s'. Valid values: 'StaticMesh', 'Blueprint', 'SM+AnimSeq'"), *Type);
		return false;
	}

	if (Type == TEXT("SM+AnimSeq"))
	{
		if (!Metadata.Contains(TEXT("AnimSequence")))
		{
			OutErrorMessage = TEXT("Type 'SM+AnimSeq' requires 'AnimSequence' key");
			return false;
		}

		const FString& AnimSeqPath = Metadata[TEXT("AnimSequence")];
		if (AnimSeqPath.IsEmpty())
		{
			OutErrorMessage = TEXT("'AnimSequence' path cannot be empty for Type 'SM+AnimSeq'");
			return false;
		}
	}

	return true;
}

void FAssetPoolManager::RegisterAssetWithMetadata(const FString& Category, const TMap<FString, FString>& Metadata)
{
	FString ErrorMessage;
	if (!ValidateMetadata(Metadata, ErrorMessage))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("RegisterAssetWithMetadata: %s"), *ErrorMessage);
		return;
	}

	if (!AssetPools.Contains(Category))
	{
		AssetPools.Add(Category, TArray<TMap<FString, FString>>());
	}

	bool bAlreadyExists = false;
	const FString& NewPath = Metadata[TEXT("Path")];
	for (const TMap<FString, FString>& ExistingMetadata : AssetPools[Category])
	{
		if (ExistingMetadata.Contains(TEXT("Path")) && ExistingMetadata[TEXT("Path")] == NewPath)
		{
			bAlreadyExists = true;
			break;
		}
	}

	if (!bAlreadyExists)
	{
		AssetPools[Category].Add(Metadata);
		UE_LOG(LogUnrealCV, Log, TEXT("RegisterAssetWithMetadata: Category='%s', Path='%s', Type='%s'"),
			*Category, *NewPath, *Metadata[TEXT("Type")]);
	}
}

void FAssetPoolManager::PrintAllAssets() const
{
	UE_LOG(LogUnrealCV, Log, TEXT("========== Asset Pool Debug =========="));
	UE_LOG(LogUnrealCV, Log, TEXT("Total categories: %d"), AssetPools.Num());

	for (const auto& Pair : AssetPools)
	{
		UE_LOG(LogUnrealCV, Log, TEXT(""));
		UE_LOG(LogUnrealCV, Log, TEXT("Category: '%s' (%d assets)"), *Pair.Key, Pair.Value.Num());

		int32 Index = 0;
		for (const TMap<FString, FString>& Metadata : Pair.Value)
		{
			FString AssetPath = Metadata.Contains(TEXT("Path")) ? Metadata[TEXT("Path")] : TEXT("(no path)");
			FString AssetType = Metadata.Contains(TEXT("Type")) ? Metadata[TEXT("Type")] : TEXT("(auto)");
			FString AnimSeq = Metadata.Contains(TEXT("AnimSequence")) ? Metadata[TEXT("AnimSequence")] : TEXT("");

			if (!AnimSeq.IsEmpty())
			{
				UE_LOG(LogUnrealCV, Log, TEXT("  [%d] %s (Type: %s, Anim: %s)"), Index++, *AssetPath, *AssetType, *AnimSeq);
			}
			else
			{
				UE_LOG(LogUnrealCV, Log, TEXT("  [%d] %s (Type: %s)"), Index++, *AssetPath, *AssetType);
			}
		}
	}

	UE_LOG(LogUnrealCV, Log, TEXT("======================================"));
}
