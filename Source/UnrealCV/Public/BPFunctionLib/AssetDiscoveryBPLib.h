#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AssetDiscoveryBPLib.generated.h"

USTRUCT(BlueprintType)
struct FDiscoveredAsset
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	FString AssetPath;

	UPROPERTY(BlueprintReadWrite)
	FString AssetType;

	UPROPERTY(BlueprintReadWrite)
	FString AssetName;

	UPROPERTY(BlueprintReadWrite)
	bool bIsActorBlueprint;
};

UCLASS()
class UNREALCV_API UAssetDiscoveryBPLib : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "UnrealCV|AssetDiscovery")
	static TArray<FDiscoveredAsset> ScanSpawnableAssets(
		const FString& SearchPath = "/Game/",
		bool bIncludeStaticMeshes = true,
		bool bIncludeSkeletalMeshes = true,
		bool bIncludeBlueprints = true,
		bool bRecursive = true
	);

private:
	static bool IsActorBlueprint(const FAssetData& AssetData);
};
