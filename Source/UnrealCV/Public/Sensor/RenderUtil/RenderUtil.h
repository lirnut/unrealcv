#pragma once

#include "CoreMinimal.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RHI.h"
#include "RHITypes.h"
#include <functional>

namespace UnrealCV
{
namespace RenderUtil
{

using FImageReadCallbackRaw = std::function<bool(
    const void* PixelData,
    int32 RowPitchInPixels,
    int32 Height,
    EPixelFormat Format,
    FIntPoint Size
)>;

using FImageReadCallbackFColor = std::function<bool(
    TArrayView<const FColor> PixelData,
    FIntPoint Size
)>;

using FImageReadCallbackFloat16 = std::function<bool(
    TArrayView<const FFloat16Color> PixelData,
    FIntPoint Size
)>;

UNREALCV_API bool ReadImageDataAsync(
    UTextureRenderTarget2D& RenderTarget,
    FImageReadCallbackRaw&& Callback
);

UNREALCV_API bool ReadImageDataAsync(
    UTextureRenderTarget2D& RenderTarget,
    FImageReadCallbackFColor&& Callback
);

UNREALCV_API bool ReadImageDataAsync(
    UTextureRenderTarget2D& RenderTarget,
    FImageReadCallbackFloat16&& Callback
);

UNREALCV_API bool SaveImageDataAsync(
    UTextureRenderTarget2D& RenderTarget,
    const FString& OutputPath
);

UNREALCV_API bool ReadImageDataSync(
    UTextureRenderTarget2D& RenderTarget,
    TArray<FColor>& OutPixelData,
    int32& OutWidth,
    int32& OutHeight
);

UNREALCV_API bool ReadImageDataSync(
    UTextureRenderTarget2D& RenderTarget,
    TArray<FFloat16Color>& OutPixelData,
    int32& OutWidth,
    int32& OutHeight
);

}
}
