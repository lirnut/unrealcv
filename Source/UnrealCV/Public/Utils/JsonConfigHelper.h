#pragma once

#include "CoreMinimal.h"
#include "Json.h"

class FJsonConfigHelper
{
public:
	static FString ExtractMapNameFromPath(const FString& MapPath);

	static bool ParseJsonValue(const TSharedPtr<FJsonValue>& JsonValue, FString& OutString);

	static bool ParseJsonNumber(const TSharedPtr<FJsonValue>& JsonValue, double& OutNumber);
};
