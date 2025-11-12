#include "AsyncCaptureHelper.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RHIStaticStates.h"
#include "UnrealcvLog.h"

FAsyncSurfaceReader::FAsyncSurfaceReader(EPixelFormat InPixelFormat, FIntPoint InBufferSize)
    : bEnabled(false)
    , AvailableEvent(nullptr)
    , TargetWidth(InBufferSize.X)
    , TargetHeight(InBufferSize.Y)
    , PixelFormat(InPixelFormat)
    , bIsEnabled(false)
    , bQueuedForCapture(false)
{
}

FAsyncSurfaceReader::~FAsyncSurfaceReader()
{
    if (AvailableEvent)
    {
        FPlatformProcess::ReturnSynchEventToPool(AvailableEvent);
        AvailableEvent = nullptr;
    }
}

void FAsyncSurfaceReader::Initialize()
{
    if (!AvailableEvent)
    {
        AvailableEvent = FPlatformProcess::GetSynchEventFromPool(true);
    }
    bIsEnabled = true;
    bEnabled = true;
}

void FAsyncSurfaceReader::Deinitialize(FRHICommandListImmediate& RHICmdList)
{
    if (Readback)
    {
        Readback.Reset();
    }
    bIsEnabled = false;
    bEnabled = false;
}

void FAsyncSurfaceReader::BlockUntilAvailable()
{
    if (AvailableEvent && bQueuedForCapture)
    {
        AvailableEvent->Wait();
    }
}

void FAsyncSurfaceReader::Reset()
{
    if (AvailableEvent)
    {
        AvailableEvent->Trigger();
    }
    bQueuedForCapture = false;
}

void FAsyncSurfaceReader::Resize(uint32 Width, uint32 Height)
{
    TargetWidth = Width;
    TargetHeight = Height;

    if (Readback)
    {
        Readback.Reset();
    }

    Readback = MakeUnique<FRHIGPUTextureReadback>(TEXT("UnrealCVAsyncReadback"));
}

void FAsyncSurfaceReader::ResolveRenderTarget(
    const FTextureRHIRef& TextureToResolve,
    TFunction<void(const FColor*, int32, int32)> Callback)
{
    if (!bEnabled || !TextureToResolve.IsValid())
    {
        return;
    }

    FIntPoint TextureSize(TextureToResolve->GetSizeX(), TextureToResolve->GetSizeY());
    if (TextureSize.X != TargetWidth || TextureSize.Y != TargetHeight)
    {
        Resize(TextureSize.X, TextureSize.Y);
    }

    if (!Readback)
    {
        Readback = MakeUnique<FRHIGPUTextureReadback>(TEXT("UnrealCVAsyncReadback"));
    }

    bQueuedForCapture = true;
    if (AvailableEvent)
    {
        AvailableEvent->Reset();
    }

    ENQUEUE_RENDER_COMMAND(AsyncCaptureResolve)(
        [this, TextureToResolve, Callback](FRHICommandListImmediate& RHICmdList)
        {
            Readback->EnqueueCopy(RHICmdList, TextureToResolve);

            RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);

            if (Readback->IsReady())
            {
                int32 RowPitchInPixels;
                const FColor* ColorData = static_cast<const FColor*>(Readback->Lock(RowPitchInPixels));

                if (ColorData)
                {
                    Callback(ColorData, RowPitchInPixels, TargetHeight);
                    Readback->Unlock();
                }

                if (AvailableEvent)
                {
                    AvailableEvent->Trigger();
                }
                bQueuedForCapture = false;
            }
        }
    );
}

void FAsyncSurfaceReader::ResolveRenderTargetFloat16(
    const FTextureRHIRef& TextureToResolve,
    TFunction<void(const FFloat16Color*, int32, int32)> Callback)
{
    if (!bEnabled || !TextureToResolve.IsValid())
    {
        return;
    }

    FIntPoint TextureSize(TextureToResolve->GetSizeX(), TextureToResolve->GetSizeY());
    if (TextureSize.X != TargetWidth || TextureSize.Y != TargetHeight)
    {
        Resize(TextureSize.X, TextureSize.Y);
    }

    if (!Readback)
    {
        Readback = MakeUnique<FRHIGPUTextureReadback>(TEXT("UnrealCVAsyncReadback"));
    }

    bQueuedForCapture = true;
    if (AvailableEvent)
    {
        AvailableEvent->Reset();
    }

    ENQUEUE_RENDER_COMMAND(AsyncCaptureResolveFloat16)(
        [this, TextureToResolve, Callback](FRHICommandListImmediate& RHICmdList)
        {
            Readback->EnqueueCopy(RHICmdList, TextureToResolve);

            RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);

            if (Readback->IsReady())
            {
                int32 RowPitchInPixels;
                const void* RawData = Readback->Lock(RowPitchInPixels);
                const FFloat16Color* Float16Data = static_cast<const FFloat16Color*>(RawData);

                if (Float16Data)
                {
                    Callback(Float16Data, RowPitchInPixels, TargetHeight);
                    Readback->Unlock();
                }

                if (AvailableEvent)
                {
                    AvailableEvent->Trigger();
                }
                bQueuedForCapture = false;
            }
        }
    );
}

bool FAsyncSurfaceReader::IsReady() const
{
    return Readback && Readback->IsReady();
}

FAsyncCapturePool::FAsyncCapturePool(EPixelFormat InPixelFormat, FIntPoint InSize)
    : NextSlotIndex(0)
    , NextRequestID(1)
    , PixelFormat(InPixelFormat)
    , BufferSize(InSize)
{
}

FAsyncCapturePool::~FAsyncCapturePool()
{
    Shutdown();
}

void FAsyncCapturePool::Initialize()
{
    for (int32 i = 0; i < PoolSize; ++i)
    {
        Slots[i].Reader = MakeShared<FAsyncSurfaceReader>(PixelFormat, BufferSize);
        Slots[i].Reader->Initialize();
        Slots[i].bInUse = false;
        Slots[i].RequestID = -1;
    }
}

void FAsyncCapturePool::Shutdown()
{
    FlushRenderingCommands();

    for (int32 i = 0; i < PoolSize; ++i)
    {
        if (Slots[i].Reader.IsValid())
        {
            ENQUEUE_RENDER_COMMAND(DeinitReader)(
                [Reader = Slots[i].Reader](FRHICommandListImmediate& RHICmdList)
                {
                    Reader->Deinitialize(RHICmdList);
                }
            );
        }
        Slots[i].Reader.Reset();
        Slots[i].bInUse = false;
    }

    FlushRenderingCommands();
}

int32 FAsyncCapturePool::RequestCapture(const FTextureRHIRef& RenderTargetTexture)
{
    FScopeLock Lock(&SlotMutex);

    int32 SlotIndex = -1;
    for (int32 i = 0; i < PoolSize; ++i)
    {
        int32 TestIndex = (NextSlotIndex + i) % PoolSize;
        if (!Slots[TestIndex].bInUse)
        {
            SlotIndex = TestIndex;
            break;
        }
    }

    if (SlotIndex == -1)
    {
        Slots[0].Reader->BlockUntilAvailable();
        SlotIndex = 0;
    }

    int32 RequestID = NextRequestID++;
    NextSlotIndex = (SlotIndex + 1) % PoolSize;

    FCaptureSlot& Slot = Slots[SlotIndex];
    Slot.bInUse = true;
    Slot.RequestID = RequestID;
    Slot.Frame.bIsReady = false;

    Slot.Reader->ResolveRenderTarget(
        RenderTargetTexture,
        [SlotIndex, this](const FColor* ColorData, int32 RowPitchInPixels, int32 Height)
        {
            FScopeLock InnerLock(&SlotMutex);

            FCaptureSlot& CompletedSlot = Slots[SlotIndex];
            CompletedSlot.Frame.Width = RowPitchInPixels;
            CompletedSlot.Frame.Height = Height;
            CompletedSlot.Frame.Data.SetNum(RowPitchInPixels * Height);

            for (int32 Row = 0; Row < Height; ++Row)
            {
                FMemory::Memcpy(
                    &CompletedSlot.Frame.Data[Row * RowPitchInPixels],
                    &ColorData[Row * RowPitchInPixels],
                    RowPitchInPixels * sizeof(FColor)
                );
            }

            CompletedSlot.Frame.bIsReady = true;
        }
    );

    return RequestID;
}

int32 FAsyncCapturePool::RequestCaptureFloat16(const FTextureRHIRef& RenderTargetTexture)
{
    FScopeLock Lock(&SlotMutex);

    int32 SlotIndex = -1;
    for (int32 i = 0; i < PoolSize; ++i)
    {
        int32 TestIndex = (NextSlotIndex + i) % PoolSize;
        if (!Slots[TestIndex].bInUse)
        {
            SlotIndex = TestIndex;
            break;
        }
    }

    if (SlotIndex == -1)
    {
        Slots[0].Reader->BlockUntilAvailable();
        SlotIndex = 0;
    }

    int32 RequestID = NextRequestID++;
    NextSlotIndex = (SlotIndex + 1) % PoolSize;

    FCaptureSlot& Slot = Slots[SlotIndex];
    Slot.bInUse = true;
    Slot.RequestID = RequestID;
    Slot.Frame.bIsReady = false;
    Slot.Frame.bIsFloat16 = true;

    Slot.Reader->ResolveRenderTargetFloat16(
        RenderTargetTexture,
        [SlotIndex, this](const FFloat16Color* Float16Data, int32 RowPitchInPixels, int32 Height)
        {
            FScopeLock InnerLock(&SlotMutex);

            FCaptureSlot& CompletedSlot = Slots[SlotIndex];
            CompletedSlot.Frame.Width = RowPitchInPixels;
            CompletedSlot.Frame.Height = Height;
            CompletedSlot.Frame.Float16Data.SetNum(RowPitchInPixels * Height);

            for (int32 Row = 0; Row < Height; ++Row)
            {
                FMemory::Memcpy(
                    &CompletedSlot.Frame.Float16Data[Row * RowPitchInPixels],
                    &Float16Data[Row * RowPitchInPixels],
                    RowPitchInPixels * sizeof(FFloat16Color)
                );
            }

            CompletedSlot.Frame.bIsReady = true;
        }
    );

    return RequestID;
}

bool FAsyncCapturePool::GetCapturedFrame(int32 RequestID, FAsyncCaptureFrame& OutFrame)
{
    FScopeLock Lock(&SlotMutex);

    for (int32 i = 0; i < PoolSize; ++i)
    {
        if (Slots[i].RequestID == RequestID && Slots[i].bInUse)
        {
            if (!Slots[i].Frame.bIsReady)
            {
                return false;
            }

            OutFrame = MoveTemp(Slots[i].Frame);
            Slots[i].bInUse = false;
            Slots[i].RequestID = -1;
            return true;
        }
    }

    return false;
}

bool FAsyncCapturePool::IsCaptureReady(int32 RequestID) const
{
    FScopeLock Lock(const_cast<FCriticalSection*>(&SlotMutex));

    for (int32 i = 0; i < PoolSize; ++i)
    {
        if (Slots[i].RequestID == RequestID && Slots[i].bInUse)
        {
            return Slots[i].Frame.bIsReady;
        }
    }

    return false;
}
