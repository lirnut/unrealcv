#pragma once

#include "Runtime/Engine/Classes/Kismet/BlueprintFunctionLibrary.h"
#include "PakMountBPLib.generated.h"

UCLASS()
class UNREALCV_API UPakMountBPLib : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "UnrealCV|PakMount")
	static bool MountPakFile(const FString& PakFilePath, int32 PakOrder = 0);

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|PakMount")
	static bool UnmountPakFile(const FString& PakFilePath);

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|PakMount")
	static void ScanMountedAssets(const FString& MountPoint, bool bForceRescan = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|PakMount")
	static TArray<FString> GetMountedPakFiles();

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|PakMount")
	static bool IsPakFileMounted(const FString& PakFilePath);

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|PakMount")
	static UObject* LoadAssetFromPak(const FString& AssetPath, UClass* AssetClass);

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|PakMount")
	static TArray<FString> GetAllAssetsInPath(const FString& PackagePath, UClass* AssetClass = nullptr);

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|PakMount")
	static bool RegisterAssetsToAssetPool(const FString& PackagePath, const FString& Category);
};
