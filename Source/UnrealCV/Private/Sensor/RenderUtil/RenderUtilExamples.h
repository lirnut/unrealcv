#pragma once

#include "Sensor/RenderUtil/ImageUtils.h"

namespace UnrealCV
{
namespace RenderUtil
{

inline void Example_SaveImageAsync(UTextureRenderTarget2D* RenderTarget, const FString& OutputPath)
{
    if (!RenderTarget) return;

    SaveImageDataAsync(*RenderTarget, OutputPath);
}

inline void Example_ReadAndProcess(UTextureRenderTarget2D* RenderTarget)
{
    if (!RenderTarget) return;

    ReadImageDataAsync(*RenderTarget, [](
        TArrayView<const FColor> Pixels,
        FIntPoint Size) -> bool
    {
        UE_LOG(LogTemp, Log, TEXT("Read %dx%d image with %d pixels"),
            Size.X, Size.Y, Pixels.Num());
        return true;
    });
}

inline void Example_ReadFloat16(UTextureRenderTarget2D* RenderTarget)
{
    if (!RenderTarget) return;

    ReadImageDataAsync(*RenderTarget, [](
        TArrayView<const FFloat16Color> Pixels,
        FIntPoint Size) -> bool
    {
        UE_LOG(LogTemp, Log, TEXT("Read Float16 image: %dx%d"), Size.X, Size.Y);
        return true;
    });
}

inline void Example_ReadRawAndConvert(UTextureRenderTarget2D* RenderTarget)
{
    if (!RenderTarget) return;

    ReadImageDataAsync(*RenderTarget, [](
        const void* PixelData,
        int32 RowPitchInPixels,
        int32 Height,
        EPixelFormat Format,
        FIntPoint Size) -> bool
    {
        UE_LOG(LogTemp, Log, TEXT("Raw data: Format=%d, Size=%dx%d, Pitch=%d"),
            (int32)Format, Size.X, Size.Y, RowPitchInPixels);
        return true;
    });
}

inline void Example_SyncRead(UTextureRenderTarget2D* RenderTarget)
{
    if (!RenderTarget) return;

    TArray<FColor> Pixels;
    int32 Width, Height;
    if (ReadImageDataSync(*RenderTarget, Pixels, Width, Height))
    {
        UE_LOG(LogTemp, Log, TEXT("Sync read: %dx%d"), Width, Height);
    }
}

}
}
