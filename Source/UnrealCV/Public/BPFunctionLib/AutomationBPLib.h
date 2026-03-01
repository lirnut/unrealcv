#pragma once

#include "CoreMinimal.h"
#include "Runtime/Engine/Classes/Kismet/BlueprintFunctionLibrary.h"
#include "AutomationBPLib.generated.h"

class FGenericTickableObject;

UCLASS()
class UNREALCV_API UAutomationBPLib : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Automation")
	static void PushCommand(const FString& Command);

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Automation")
	static void StartTicking();

	UFUNCTION(BlueprintCallable, Category = "UnrealCV|Automation")
	static void StopTicking();

	UFUNCTION(BlueprintPure, Category = "UnrealCV|Automation")
	static bool IsTickingActive();

private:
	static TQueue<FString> CommandQueue;
	static FGenericTickableObject* TickableObject;
	static bool bIsActive;

	static double SleepTo;

	static void OnTick(double DeltaTime);
	static void ProcessCommands();
};
