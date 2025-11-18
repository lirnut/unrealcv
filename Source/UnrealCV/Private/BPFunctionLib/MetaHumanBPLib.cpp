#include "BPFunctionLib/MetaHumanBPLib.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/ARFilter.h"
#include "Engine/Blueprint.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimBlueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"

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

bool UMetaHumanBPLib::SetMetaHumanAnimationBlueprint(const FString& MetaHumanBPPath, const FString& AnimBlueprintPath)
{
	UBlueprint* MetaHumanBP = LoadObject<UBlueprint>(nullptr, *MetaHumanBPPath);
	if (!MetaHumanBP)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load MetaHuman Blueprint: %s"), *MetaHumanBPPath);
		return false;
	}

	UClass* AnimBPClass = LoadObject<UClass>(nullptr, *AnimBlueprintPath);
	if (!AnimBPClass)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load Animation Blueprint class: %s"), *AnimBlueprintPath);
		return false;
	}

	USkeletalMeshComponent* BodyComponentTemplate = nullptr;

	if (MetaHumanBP->SimpleConstructionScript)
	{
		const TArray<USCS_Node*>& AllNodes = MetaHumanBP->SimpleConstructionScript->GetAllNodes();
		UE_LOG(LogTemp, Log, TEXT("Found %d SCS nodes in MetaHuman Blueprint"), AllNodes.Num());

		for (USCS_Node* Node : AllNodes)
		{
			if (Node && Node->ComponentTemplate)
			{
				FString NodeName = Node->GetVariableName().ToString();
				FString ComponentName = Node->ComponentTemplate->GetName();
				UE_LOG(LogTemp, Log, TEXT("  Node: %s, Component: %s, Class: %s"),
					*NodeName, *ComponentName, *Node->ComponentTemplate->GetClass()->GetName());

				if (USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>(Node->ComponentTemplate))
				{
					if (NodeName.Contains(TEXT("Body")) || ComponentName.Contains(TEXT("Body")))
					{
						BodyComponentTemplate = SkelMeshComp;
						UE_LOG(LogTemp, Log, TEXT("  -> Matched 'Body' component!"));
						break;
					}
				}
			}
		}
	}

	if (!BodyComponentTemplate)
	{
		UE_LOG(LogTemp, Error, TEXT("No 'Body' SkeletalMeshComponent found in SimpleConstructionScript"));
		return false;
	}

	BodyComponentTemplate->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	BodyComponentTemplate->AnimClass = AnimBPClass;

	MetaHumanBP->Modify();
	MetaHumanBP->MarkPackageDirty();

	UE_LOG(LogTemp, Log, TEXT("Successfully set Animation Blueprint for %s to %s"), *MetaHumanBPPath, *AnimBlueprintPath);
	return true;
}

TArray<FString> UMetaHumanBPLib::SetupAllMetaHumansWithAnimation(const FString& AnimBlueprintPath)
{
	TArray<FString> AllMetaHumans = GetAllMetaHumanBlueprintPaths();
	TArray<FString> SuccessfulPaths;

	for (const FString& MetaHumanPath : AllMetaHumans)
	{
		if (SetMetaHumanAnimationBlueprint(MetaHumanPath, AnimBlueprintPath))
		{
			SuccessfulPaths.Add(MetaHumanPath);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("SetupAllMetaHumansWithAnimation: %d/%d MetaHumans configured"), SuccessfulPaths.Num(), AllMetaHumans.Num());
	return SuccessfulPaths;
}
