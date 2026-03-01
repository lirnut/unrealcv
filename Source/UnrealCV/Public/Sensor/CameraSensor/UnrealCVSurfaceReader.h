#pragma once

#include "CoreMinimal.h"
#include "PixelFormat.h"
#include "RHI.h"
#include "RHIGPUReadback.h"
#include "ImagePixelData.h"

struct FImagePixelDataPayload;

struct UNREALCV_API FUnrealCVSurfaceReader
	: public TSharedFromThis<FUnrealCVSurfaceReader, ESPMode::ThreadSafe>
{
	FUnrealCVSurfaceReader(EPixelFormat InPixelFormat, bool bInInvertAlpha);

	~FUnrealCVSurfaceReader();

	void Initialize();

	void BlockUntilAvailable();

	void Reset();

	bool WasEverQueued() const { return bQueuedForCapture; }
	bool IsAvailable() const { return AvailableEvent == nullptr; }

	void ResolveSampleToReadbackTexture_RenderThread(const FTextureRHIRef& SourceSurfaceSample);

	void CopyReadbackTexture_RenderThread(TUniqueFunction<void(TUniquePtr<FImagePixelData>&&)>&& InFunctionCallback, TSharedPtr<FImagePixelDataPayload, ESPMode::ThreadSafe> InFramePayload);

protected:
	friend struct FUnrealCVSurfaceQueue;
	void ResizeImpl(uint32 Width, uint32 Height);

protected:
	FEvent* AvailableEvent;

	TUniquePtr<FRHIGPUTextureReadback> ReadbackTexture;

	EPixelFormat PixelFormat;

	FIntPoint Size;

	bool bQueuedForCapture;
	bool bInvertAlpha;
};

struct UNREALCV_API FUnrealCVSurfaceQueue
{
public:
	FUnrealCVSurfaceQueue(FIntPoint InSurfaceSize, EPixelFormat InPixelFormat, uint32 InNumSurfaces, bool bInInvertAlpha);
	~FUnrealCVSurfaceQueue();

	FUnrealCVSurfaceQueue(FUnrealCVSurfaceQueue&&) = default;
	FUnrealCVSurfaceQueue& operator=(FUnrealCVSurfaceQueue&&) = default;

	FUnrealCVSurfaceQueue(const FUnrealCVSurfaceQueue&) = delete;
	FUnrealCVSurfaceQueue& operator=(const FUnrealCVSurfaceQueue&) = delete;
public:

	void OnRenderTargetReady_RenderThread(const FTextureRHIRef InRenderTarget, TSharedRef<FImagePixelDataPayload, ESPMode::ThreadSafe> InPayload, TUniqueFunction<void(TUniquePtr<FImagePixelData>&&)>&& InFunctionCallback);

	void BlockUntilAnyAvailable();
	void Shutdown();

	void UpdateLastUsedFrame();

	bool IsStale() const;

	void SetFrameResolveLatency(int32 InLatency)
	{
		FrameResolveLatency = FMath::Clamp(InLatency, 0, Surfaces.Num() - 1);
	}

private:
	struct FResolveSurface
	{
		UE_NONCOPYABLE(FResolveSurface);

		FResolveSurface(const EPixelFormat InPixelFormat, const FIntPoint InSurfaceSize, const bool bInInvertAlpha)
			: Surface(new FUnrealCVSurfaceReader(InPixelFormat, bInInvertAlpha))
		{
			Surface->ResizeImpl(InSurfaceSize.X, InSurfaceSize.Y);
		}

		const TSharedRef<FUnrealCVSurfaceReader, ESPMode::ThreadSafe> Surface;

		TUniqueFunction<void(TUniquePtr<FImagePixelData>&&)> FunctionCallback;
		TSharedPtr<FImagePixelDataPayload, ESPMode::ThreadSafe> FunctionPayload;
	};

	int32 CurrentFrameIndex;

	TArray<FResolveSurface> Surfaces;

	int32 FrameResolveLatency;

	uint64 LastUsedFrame;
};
