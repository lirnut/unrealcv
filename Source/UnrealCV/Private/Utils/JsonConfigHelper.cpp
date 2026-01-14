#include "JsonConfigHelper.h"

FString FJsonConfigHelper::ExtractMapNameFromPath(const FString& MapPath)
{
	int32 OutIndex = INDEX_NONE;
	if (MapPath.FindLastChar(TEXT('/'), OutIndex))
	{
		return MapPath.Mid(OutIndex + 1);
	}
	return MapPath;
}

bool FJsonConfigHelper::ParseJsonValue(const TSharedPtr<FJsonValue>& JsonValue, FString& OutString)
{
	if (!JsonValue.IsValid())
	{
		return false;
	}

	if (JsonValue->Type == EJson::String)
	{
		OutString = JsonValue->AsString();
		return true;
	}
	else if (JsonValue->Type == EJson::Number)
	{
		OutString = FString::FromInt((int32)JsonValue->AsNumber());
		return true;
	}

	return false;
}

bool FJsonConfigHelper::ParseJsonNumber(const TSharedPtr<FJsonValue>& JsonValue, double& OutNumber)
{
	if (!JsonValue.IsValid())
	{
		return false;
	}

	if (JsonValue->Type == EJson::Number)
	{
		OutNumber = JsonValue->AsNumber();
		return true;
	}

	return false;
}
