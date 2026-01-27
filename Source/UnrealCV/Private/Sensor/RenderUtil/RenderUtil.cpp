#include "Sensor/RenderUtil/RenderUtil.h"
#include "ImageReadback.h"
#include "ImageConversion.h"
#include "ImageProcessing.h"
#include "ImageSerializer.h"
#include "UnrealcvLog.h"

namespace UnrealCV
{
namespace RenderUtil
{

bool ReadImageDataAsync(
    UTextureRenderTarget2D& RenderTarget,
    FImageReadCallbackRaw&& Callback)
{
    if (IsInRenderingThread())
    {
        FReadbackContext Context;
        ReadImageDataBegin(Context, RenderTarget, [Callback = std::move(Callback), &Context](
            const void* MappedPtr, int32 RowPitchInPixels, int32 BufferHeight)
        {
            Callback(MappedPtr, RowPitchInPixels, BufferHeight, Context.Format, Context.Size);
        });
        ReadImageDataEnd(Context);
    }
    else
    {
        ENQUEUE_RENDER_COMMAND(ReadImageDataAsyncCmd)([
            &RenderTarget, Callback = std::move(Callback)
        ](auto& CmdList) mutable
        {
            FReadbackContext Context;
            ReadImageDataBegin(Context, RenderTarget, [Callback = std::move(Callback), &Context](
                const void* MappedPtr, int32 RowPitchInPixels, int32 BufferHeight)
            {
                Callback(MappedPtr, RowPitchInPixels, BufferHeight, Context.Format, Context.Size);
            });
            ReadImageDataEnd(Context);
        });
    }
    return true;
}

bool ReadImageDataAsync(
    UTextureRenderTarget2D& RenderTarget,
    FImageReadCallbackFColor&& Callback)
{
    return ReadImageDataAsync(RenderTarget, [Callback = std::move(Callback)](
        const void* Mapping,
        int32 RowPitchInPixels,
        int32 BufferHeight,
        EPixelFormat Format,
        FIntPoint Size) -> bool
    {
        FReadSurfaceDataFlags Flags(RCM_MinMax);
        Flags.SetLinearToGamma(false);

        TArray<FColor> Pixels;
        Pixels.SetNumUninitialized(Size.X * Size.Y);

        uint32 SrcPitch = RowPitchInPixels * GPixelFormats[Format].BlockBytes;
        if (!ConvertRawSurfaceToFColor(Format, Size.X, Size.Y, (const uint8*)Mapping, SrcPitch, Pixels.GetData(), Flags))
            return false;

        FixAlphaIfNeeded(Pixels, Format);
        return Callback(Pixels, Size);
    });
}

bool ReadImageDataAsync(
    UTextureRenderTarget2D& RenderTarget,
    FImageReadCallbackFloat16&& Callback)
{
    return ReadImageDataAsync(RenderTarget, [Callback = std::move(Callback)](
        const void* Mapping,
        int32 RowPitchInPixels,
        int32 BufferHeight,
        EPixelFormat Format,
        FIntPoint Size) -> bool
    {
        FReadSurfaceDataFlags Flags(RCM_MinMax);
        Flags.SetLinearToGamma(false);

        TArray<FFloat16Color> Pixels;
        Pixels.SetNumUninitialized(Size.X * Size.Y);

        uint32 SrcPitch = RowPitchInPixels * GPixelFormats[Format].BlockBytes;
        if (!ConvertRawSurfaceToFloat16(Format, Size.X, Size.Y, (const uint8*)Mapping, SrcPitch, Pixels.GetData(), Flags))
            return false;

        return Callback(Pixels, Size);
    });
}

bool SaveImageDataAsync(
    UTextureRenderTarget2D& RenderTarget,
    const FString& OutputPath)
{
    return ReadImageDataAsync(RenderTarget, [OutputPath](
        TArrayView<const FColor> Pixels,
        FIntPoint Size) -> bool
    {
        TArray<FColor> PixelsCopy(Pixels.GetData(), Pixels.Num());
        AsyncTask(ENamedThreads::AnyThread, [PixelsCopy = MoveTemp(PixelsCopy), Size, OutputPath]()
        {
            SaveImageData(PixelsCopy, Size.X, Size.Y, OutputPath);
        });
        return true;
    });
}

bool ReadImageDataSync(
    UTextureRenderTarget2D& RenderTarget,
    TArray<FColor>& OutPixelData,
    int32& OutWidth,
    int32& OutHeight)
{
    OutWidth = RenderTarget.SizeX;
    OutHeight = RenderTarget.SizeY;
    OutPixelData.Empty();
    OutPixelData.SetNumUninitialized(OutWidth * OutHeight);

    FTextureRenderTargetResource* RenderTargetResource = RenderTarget.GameThread_GetRenderTargetResource();
    FReadSurfaceDataFlags ReadFlags(RCM_MinMax);
    ReadFlags.SetLinearToGamma(false);
    return RenderTargetResource->ReadPixels(OutPixelData, ReadFlags);
}

bool ReadImageDataSync(
    UTextureRenderTarget2D& RenderTarget,
    TArray<FFloat16Color>& OutPixelData,
    int32& OutWidth,
    int32& OutHeight)
{
    OutWidth = RenderTarget.SizeX;
    OutHeight = RenderTarget.SizeY;
    OutPixelData.Empty();
    OutPixelData.SetNumUninitialized(OutWidth * OutHeight);

    FTextureRenderTargetResource* RenderTargetResource = RenderTarget.GameThread_GetRenderTargetResource();
    return RenderTargetResource->ReadFloat16Pixels(OutPixelData);
}

}
}
