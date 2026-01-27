#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RHIGPUReadback.h"
#include "RenderingThread.h"
#include "RHICommandList.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageConversion.h"
#include <functional>

namespace UnrealCV
{
namespace RenderUtil
{

template <typename F>
class TScopedCallback
{
    F Fn;
public:
    constexpr TScopedCallback(F&& InFn) : Fn(InFn) {}
    ~TScopedCallback() { Fn(); }
};

struct FReadbackContext
{
    EPixelFormat Format;
    FIntPoint Size;
    std::function<void(const void*, int32, int32)> Callback;
    TSharedPtr<FRHIGPUTextureReadback> Readback;
    double Timestamp;
    bool bValid;

    FReadbackContext() : Timestamp(0.0), bValid(false) {}
};

inline void ReadImageDataBeginPipelined(
    FReadbackContext& Context,
    UTextureRenderTarget2D& RenderTarget)
{
    auto Resource = static_cast<FTextureRenderTarget2DResource*>(
        RenderTarget.GetResource());
    auto Texture = Resource->GetRenderTargetTexture();
    if (Texture == nullptr)
        return;

    Context.Readback = MakeShared<FRHIGPUTextureReadback>(
        TEXT("UnrealCV-ImageReadback"));
    Context.Size = Texture->GetSizeXY();
    Context.Format = Texture->GetFormat();
    Context.Timestamp = FPlatformTime::Seconds();
    Context.bValid = false;

    ENQUEUE_RENDER_COMMAND(EnqueueGPUCopyPipelined)(
        [Resource, Context = &Context](FRHICommandListImmediate& RHICmdList)
        {
            FResolveRect ResolveRect;
            Context->Readback->EnqueueCopy(RHICmdList, Resource->GetRenderTargetTexture(), ResolveRect);
            Context->bValid = true;
        }
    );
}

inline void ReadImageDataBegin(
    FReadbackContext& Context,
    UTextureRenderTarget2D& RenderTarget,
    std::function<void(const void*, int32, int32)>&& Callback)
{
    static thread_local FRenderQueryPoolRHIRef RenderQueryPool =
        RHICreateRenderQueryPool(RQT_AbsoluteTime);

    auto& CmdList = FRHICommandListImmediate::Get();
    auto Resource = static_cast<FTextureRenderTarget2DResource*>(
        RenderTarget.GetResource());
    auto Texture = Resource->GetRenderTargetTexture();
    if (Texture == nullptr)
        return;

    Context.Callback = std::move(Callback);
    Context.Readback = MakeShared<FRHIGPUTextureReadback>(
        TEXT("UnrealCV-ImageReadback"));
    Context.Size = Texture->GetSizeXY();
    Context.Format = Texture->GetFormat();
    Context.Timestamp = FPlatformTime::Seconds();
    Context.bValid = false;

    FResolveRect ResolveRect;
    Context.Readback->EnqueueCopy(CmdList, Texture, ResolveRect);

    auto Query = RenderQueryPool->AllocateQuery();
    CmdList.EndRenderQuery(Query.GetQuery());
    CmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);

    uint64 DeltaTime;
    RHIGetRenderQueryResult(Query.GetQuery(), DeltaTime, true);
    Query.ReleaseQuery();

    Context.bValid = true;
}

inline bool ReadImageDataEndPipelined(
    FReadbackContext& Context,
    TArray<FColor>& OutPixels)
{
    if (!Context.bValid || !Context.Readback.IsValid())
        return false;

    while (!Context.Readback->IsReady())
    {
        FPlatformProcess::Sleep(0.0001f);
    }

    int32 RowPitchInPixels, BufferHeight;
    auto MappedPtr = Context.Readback->Lock(RowPitchInPixels, &BufferHeight);
    if (MappedPtr == nullptr)
        return false;

    TScopedCallback Unlock = [&] { Context.Readback->Unlock(); };

    OutPixels.SetNumUninitialized(Context.Size.X * Context.Size.Y);

    FReadSurfaceDataFlags ReadFlags(RCM_MinMax);
    ReadFlags.SetLinearToGamma(false);
    uint32 SrcPitch = RowPitchInPixels * GPixelFormats[Context.Format].BlockBytes;

    return ConvertRawSurfaceToFColor(
        Context.Format,
        Context.Size.X,
        Context.Size.Y,
        (const uint8*)MappedPtr,
        SrcPitch,
        OutPixels.GetData(),
        ReadFlags
    );
}

inline void ReadImageDataEnd(FReadbackContext& Context)
{
    if (!Context.bValid || !Context.Readback.IsValid())
        return;

    int32 RowPitchInPixels, BufferHeight;
    auto MappedPtr = Context.Readback->Lock(RowPitchInPixels, &BufferHeight);
    if (MappedPtr != nullptr)
    {
        TScopedCallback Unlock = [&] { Context.Readback->Unlock(); };
        if (Context.Callback)
        {
            Context.Callback(MappedPtr, RowPitchInPixels, BufferHeight);
        }
    }
}

inline void ReadImageDataEndAsync(FReadbackContext&& Context)
{
    AsyncTask(
        ENamedThreads::HighTaskPriority, [
        Context = std::move(Context)]() mutable
    {
        while (!Context.Readback->IsReady())
            FPlatformProcess::Sleep(0.0001f);
        ReadImageDataEnd(Context);
    });
}

}
}
