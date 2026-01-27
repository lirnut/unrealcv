#pragma once

#include "CoreMinimal.h"

namespace UnrealCV
{
namespace RenderUtil
{

TFuture<bool> SaveImageData(
    const TArray<FColor>& PixelData,
    int32 Width, int32 Height,
    const FString& Path);

}
}
