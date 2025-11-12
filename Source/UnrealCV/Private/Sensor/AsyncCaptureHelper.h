#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RHICommandList.h"
#include "RHIGPUReadback.h"
#include "HAL/Event.h"
#include "HAL/ThreadSafeBool.h"
#include "PixelFormat.h"

struct FAsyncCaptureFrame
{
    TArray<FColor> Data;
    TArray<FFloat16Color> Float16Data;
    int32 Width;
    int32 Height;
    bool bIsReady;
    bool bIsFloat16;

    FAsyncCaptureFrame() : Width(0), Height(0), bIsReady(false), bIsFloat16(false) {}
};

class FAsyncSurfaceReader
{
public:
    FAsyncSurfaceReader(EPixelFormat InPixelFormat, FIntPoint InBufferSize);
    ~FAsyncSurfaceReader();

    void Initialize();
    void Deinitialize(FRHICommandListImmediate& RHICmdList);

    void BlockUntilAvailable();
    void Reset();

    void ResolveRenderTarget(
        const FTextureRHIRef& TextureToResolve,
        TFunction<void(const FColor*, int32, int32)> Callback
    );

    void ResolveRenderTargetFloat16(
        const FTextureRHIRef& TextureToResolve,
        TFunction<void(const FFloat16Color*, int32, int32)> Callback
    );

    bool IsReady() const;

    EPixelFormat GetPixelFormat() const { return PixelFormat; }

private:
    void Resize(uint32 Width, uint32 Height);

    FThreadSafeBool bEnabled;
    FEvent* AvailableEvent;

    int32 TargetWidth;
    int32 TargetHeight;
    TUniquePtr<FRHIGPUTextureReadback> Readback;

    EPixelFormat PixelFormat;
    bool bIsEnabled;
    bool bQueuedForCapture;
};

class FAsyncCapturePool
{
public:
    static constexpr int32 PoolSize = 3;

    FAsyncCapturePool(EPixelFormat InPixelFormat, FIntPoint InSize);
    ~FAsyncCapturePool();

    void Initialize();
    void Shutdown();

    int32 RequestCapture(const FTextureRHIRef& RenderTargetTexture);
    int32 RequestCaptureFloat16(const FTextureRHIRef& RenderTargetTexture);
    bool GetCapturedFrame(int32 RequestID, FAsyncCaptureFrame& OutFrame);
    bool IsCaptureReady(int32 RequestID) const;

private:
    struct FCaptureSlot
    {
        TSharedPtr<FAsyncSurfaceReader> Reader;
        FAsyncCaptureFrame Frame;
        int32 RequestID;
        bool bInUse;

        FCaptureSlot() : RequestID(-1), bInUse(false) {}
    };

    FCaptureSlot Slots[PoolSize];
    int32 NextSlotIndex;
    int32 NextRequestID;
    EPixelFormat PixelFormat;
    FIntPoint BufferSize;
    FCriticalSection SlotMutex;
};
