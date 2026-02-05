// Weichao Qiu @ 2017
#pragma once

#include "Runtime/Engine/Classes/GameFramework/Actor.h"

class IAnnotatorImpl;

class FColorGenerator
{
public:
	FColor GetColorFromColorMap(int32 ObjectIndex);

private:
	int32 GetChannelValue(uint32 Index);
	void GetColors(int32 MaxVal, bool Fix1, bool Fix2, bool Fix3, TArray<FColor>& ColorMap);
};

class UNREALCV_API FObjectAnnotator
{
public:
	static void Initialize();
	static void Shutdown();

	static void AnnotateWorld(UWorld* World);
	static void DeannotateWorld(UWorld* World);
	static int32 SetAnnotationColor(AActor* Actor, const FColor& AnnotationColor);
	static void GetAnnotationColor(AActor* Actor, FColor& AnnotationColor);
	static TMap<FString, FColor> GetAnnotationColors();

	static void SetAnnotationMode(bool bUseDirect);
	static bool IsUsingDirectAnnotation();

private:
	static TSharedPtr<IAnnotatorImpl> Implementation;
	static bool bUseDirectAnnotation;
	static FColorGenerator ColorGenerator;
};
