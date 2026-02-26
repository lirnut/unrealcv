#include "Sensor/CameraSensor/UnrealCVSurfaceReader.h"
#include "MovieRenderPipelineDataTypes.h"
#include "RenderingThread.h"
#include "RHICommandList.h"
#include "RHIStaticStates.h"
#include "CommonRenderResources.h"
#include "RenderTargetPool.h"
#include "ScreenRendering.h"
#include "GlobalShader.h"
#include "Shader.h"
#include "StaticBoundShaderState.h"
#include "Modules/ModuleManager.h"
#include "UnrealcvLog.h"

DECLARE_CYCLE_STAT(TEXT("STAT_UnrealCV_SurfaceReadback"), STAT_UnrealCV_SurfaceReadback, STATGROUP_Game);

FUnrealCVSurfaceReader::FUnrealCVSurfaceReader(EPixelFormat InPixelFormat, bool bInInvertAlpha)
{
	AvailableEvent = nullptr;
	ReadbackTexture = nullptr;
	PixelFormat = InPixelFormat;
	bQueuedForCapture = false;
	bInvertAlpha = bInInvertAlpha;
}

FUnrealCVSurfaceReader::~FUnrealCVSurfaceReader()
{
	BlockUntilAvailable();

	ReadbackTexture = nullptr;
}

void FUnrealCVSurfaceReader::Initialize()
{
	check(!AvailableEvent);
	AvailableEvent = FPlatformProcess::GetSynchEventFromPool();
}

void FUnrealCVSurfaceReader::ResizeImpl(uint32 Width, uint32 Height)
{
	ReadbackTexture = nullptr;
	Size = FIntPoint((int32)Width, (int32)Height);

	ENQUEUE_RENDER_COMMAND(CreateCaptureFrameTexture)(
		[SurfaceReader = SharedThis(this)](FRHICommandListImmediate& RHICmdList)
		{
			SurfaceReader->ReadbackTexture = MakeUnique<FRHIGPUTextureReadback>(TEXT("FUnrealCVSurfaceReader"));
		});
}

void FUnrealCVSurfaceReader::BlockUntilAvailable()
{
	if (AvailableEvent)
	{
		AvailableEvent->Wait(~0);

		FPlatformProcess::ReturnSynchEventToPool(AvailableEvent);
		AvailableEvent = nullptr;
	}
}

void FUnrealCVSurfaceReader::Reset()
{
	if (AvailableEvent)
	{
		AvailableEvent->Trigger();
	}
	BlockUntilAvailable();
	bQueuedForCapture = false;
}

void FUnrealCVSurfaceReader::ResolveSampleToReadbackTexture_RenderThread(const FTextureRHIRef& SourceSurfaceSample)
{
	static const FName RendererModuleName("Renderer");
	IRendererModule* RendererModule = &FModuleManager::GetModuleChecked<IRendererModule>(RendererModuleName);

	bQueuedForCapture = true;
	FRHICommandListImmediate& RHICmdList = GetImmediateCommandList_ForRenderCommand();

	const FIntPoint TargetSize(Size);

	FPooledRenderTargetDesc OutputDesc = FPooledRenderTargetDesc::Create2DDesc(
		TargetSize,
		PixelFormat,
		FClearValueBinding::None,
		TexCreate_None,
		TexCreate_RenderTargetable,
		false);
	TRefCountPtr<IPooledRenderTarget> ResampleTexturePooledRenderTarget;
	GRenderTargetPool.FindFreeElement(RHICmdList, OutputDesc, ResampleTexturePooledRenderTarget, TEXT("ResampleTexture"));
	check(ResampleTexturePooledRenderTarget);

	RHICmdList.Transition(FRHITransitionInfo(ResampleTexturePooledRenderTarget->GetRHI(), ERHIAccess::Unknown, ERHIAccess::RTV));

	FRHIRenderPassInfo RPInfo(ResampleTexturePooledRenderTarget->GetRHI(), ERenderTargetActions::Load_Store);
	RHICmdList.BeginRenderPass(RPInfo, TEXT("UnrealCVSurfaceResolveRenderTarget"));
	{
		RHICmdList.SetViewport(0, 0, 0.0f, TargetSize.X, TargetSize.Y, 1.0f);

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

		const ERHIFeatureLevel::Type FeatureLevel = GMaxRHIFeatureLevel;

		FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(FeatureLevel);
		TShaderMapRef<FScreenVS> VertexShader(ShaderMap);

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		if (bInvertAlpha)
		{
			TShaderMapRef<FScreenPSInvertAlpha> PixelShader(ShaderMap);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

			SetShaderParametersLegacyPS(RHICmdList, PixelShader, TStaticSamplerState<SF_Point>::GetRHI(), SourceSurfaceSample);
		}
		else
		{
			TShaderMapRef<FScreenPS> PixelShader(ShaderMap);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

			SetShaderParametersLegacyPS(RHICmdList, PixelShader, TStaticSamplerState<SF_Point>::GetRHI(), SourceSurfaceSample);
		}

		RendererModule->DrawRectangle(
			RHICmdList,
			0, 0,
			TargetSize.X,
			TargetSize.Y,
			0, 0,
			1, 1,
			TargetSize,
			FIntPoint(1, 1),
			VertexShader,
			EDRF_Default);
	}
	RHICmdList.EndRenderPass();

	RHICmdList.Transition(FRHITransitionInfo(ResampleTexturePooledRenderTarget->GetRHI(), ERHIAccess::RTV, ERHIAccess::CopySrc));
	ReadbackTexture->EnqueueCopy(RHICmdList, ResampleTexturePooledRenderTarget->GetRHI());
}

void FUnrealCVSurfaceReader::CopyReadbackTexture_RenderThread(TUniqueFunction<void(TUniquePtr<FImagePixelData>&&)>&& InFunctionCallback, TSharedPtr<FImagePixelDataPayload, ESPMode::ThreadSafe> InFramePayload)
{
	static const FName RendererModuleName("Renderer");
	IRendererModule* RendererModule = &FModuleManager::GetModuleChecked<IRendererModule>(RendererModuleName);

	FRHICommandListImmediate& RHICmdList = GetImmediateCommandList_ForRenderCommand();
	{
#if WITH_MGPU
		FRHIGPUMask GPUMask = RHICmdList.GetGPUMask();

		if (!GPUMask.HasSingleIndex())
		{
			GPUMask = FRHIGPUMask::FromIndex(GPUMask.GetFirstIndex());
		}

		SCOPED_GPU_MASK(RHICmdList, GPUMask);
#endif

		int32 ActualSizeX = 0, ActualSizeY = 0;
		void* ColorDataBuffer = ReadbackTexture->Lock(ActualSizeX, &ActualSizeY);

		int32 ExpectedSizeX = Size.X;
		int32 ExpectedSizeY = Size.Y;

		TUniquePtr<FImagePixelData> PixelData;
		uint8* TypeErasedPixels = nullptr;
		int32 SizeOfColor = 0;
		switch (PixelFormat)
		{
		case EPixelFormat::PF_FloatRGBA:
		{
			TUniquePtr<TImagePixelData<FFloat16Color>> NewPixelData = MakeUnique < TImagePixelData<FFloat16Color>>(FIntPoint(Size.X, Size.Y), InFramePayload);
			NewPixelData->Pixels.SetNumUninitialized(ExpectedSizeX * ExpectedSizeY);
			SizeOfColor = sizeof(FFloat16Color);
			TypeErasedPixels = reinterpret_cast<uint8*>(NewPixelData->Pixels.GetData());
			PixelData = MoveTemp(NewPixelData);
			break;
		}
		case EPixelFormat::PF_B8G8R8A8:
		{
			TUniquePtr<TImagePixelData<FColor>> NewPixelData = MakeUnique < TImagePixelData<FColor>>(FIntPoint(Size.X, Size.Y), InFramePayload);
			NewPixelData->Pixels.SetNumUninitialized(ExpectedSizeX * ExpectedSizeY);
			SizeOfColor = sizeof(FColor);
			TypeErasedPixels = reinterpret_cast<uint8*>(NewPixelData->Pixels.GetData());
			PixelData = MoveTemp(NewPixelData);
			break;
		}
		default:
			check(0);
		}


		if (ExpectedSizeX == ActualSizeX && ExpectedSizeY == ActualSizeY)
		{
			FMemory::BigBlockMemcpy(TypeErasedPixels, ColorDataBuffer, (ExpectedSizeX * ExpectedSizeY) * SizeOfColor);
		}
		else
		{
			check(ExpectedSizeX <= ActualSizeX);
			check(ExpectedSizeY <= ActualSizeY);

			int32 SrcPitchElem = ActualSizeX;
			int32 DstPitchElem = ExpectedSizeX;

			const uint8* SrcColorData = reinterpret_cast<const uint8*>(ColorDataBuffer);

			for (int32 RowIndex = 0; RowIndex < ExpectedSizeY; RowIndex++)
			{
				const void* SrcPtr = SrcColorData + RowIndex * SrcPitchElem * SizeOfColor;
				void* DstPtr = TypeErasedPixels + RowIndex * DstPitchElem * SizeOfColor;
				FMemory::Memcpy(DstPtr, SrcPtr, DstPitchElem * SizeOfColor);
			}
		}

		ReadbackTexture->Unlock();

		InFunctionCallback(MoveTemp(PixelData));

		Reset();
	}
}

FUnrealCVSurfaceQueue::FUnrealCVSurfaceQueue(FIntPoint InSurfaceSize, EPixelFormat InPixelFormat, uint32 InNumSurfaces, bool bInInvertAlpha)
{
	CurrentFrameIndex = 0;
	check(InNumSurfaces > 0);

	Surfaces.Reserve(InNumSurfaces);
	for (uint32 Index = 0; Index < InNumSurfaces; Index++)
	{
		Surfaces.Emplace(InPixelFormat, InSurfaceSize, bInInvertAlpha);
	}

	FrameResolveLatency = 1;

	UpdateLastUsedFrame();
}

FUnrealCVSurfaceQueue::~FUnrealCVSurfaceQueue()
{
}

void FUnrealCVSurfaceQueue::BlockUntilAnyAvailable()
{
	bool bAnyAvailable = false;
	for(FResolveSurface& ResolveSurface : Surfaces)
	{
		if (ResolveSurface.Surface->IsAvailable())
		{
			bAnyAvailable = true;
			break;
		}
	}

	if (!bAnyAvailable)
	{
		const int32 OldestIndex = (CurrentFrameIndex + 1) % Surfaces.Num();
		Surfaces[OldestIndex].Surface->BlockUntilAvailable();
	}
}

void FUnrealCVSurfaceQueue::Shutdown()
{
	for (int32 Index = 0; Index < Surfaces.Num(); Index++)
	{
		FResolveSurface* ResolveSurface = &Surfaces[Index];
		ENQUEUE_RENDER_COMMAND(PerformReadback)(
			[ResolveSurface](FRHICommandListImmediate& RHICmdList)
			{
				if (ResolveSurface->Surface->WasEverQueued())
				{
					ResolveSurface->Surface->CopyReadbackTexture_RenderThread(MoveTemp(ResolveSurface->FunctionCallback), ResolveSurface->FunctionPayload);
				}
			});
	}

	FlushRenderingCommands();

	for (FResolveSurface& ResolveSurface : Surfaces)
	{
		ensureMsgf(ResolveSurface.Surface->IsAvailable(), TEXT("Flushed rendering commands but surface reader didn't perform the readback on a surface!"));
	}
}

void FUnrealCVSurfaceQueue::UpdateLastUsedFrame()
{
	LastUsedFrame = GFrameCounter;
}

bool FUnrealCVSurfaceQueue::IsStale() const
{
	constexpr int32 StaleThreshold = 3;
	return GFrameCounter - LastUsedFrame > StaleThreshold;
}

void FUnrealCVSurfaceQueue::OnRenderTargetReady_RenderThread(const FTextureRHIRef InRenderTarget, TSharedRef<FImagePixelDataPayload, ESPMode::ThreadSafe> InFramePayload, TUniqueFunction<void(TUniquePtr<FImagePixelData>&&)>&& InFunctionCallback)
{
	ensure(IsInRenderingThread());

	FResolveSurface* NextResolveTarget = &Surfaces[CurrentFrameIndex];
	if (!NextResolveTarget->Surface->IsAvailable())
	{
		check(false);
	}

	NextResolveTarget->Surface->Initialize();
	NextResolveTarget->FunctionCallback = MoveTemp(InFunctionCallback);
	NextResolveTarget->FunctionPayload = InFramePayload;

	NextResolveTarget->Surface->ResolveSampleToReadbackTexture_RenderThread(InRenderTarget);

	{
		const int32 PrevCaptureIndexOffset = FMath::Clamp(FrameResolveLatency, 0, Surfaces.Num() - 1);

		const int32 PrevCaptureIndex = (CurrentFrameIndex - PrevCaptureIndexOffset) < 0 ? Surfaces.Num() - (PrevCaptureIndexOffset - CurrentFrameIndex) : (CurrentFrameIndex - PrevCaptureIndexOffset);

		FResolveSurface* OldestResolveTarget = &Surfaces[PrevCaptureIndex];

		if (OldestResolveTarget->Surface->WasEverQueued())
		{
			SCOPE_CYCLE_COUNTER(STAT_UnrealCV_SurfaceReadback);
			OldestResolveTarget->Surface->CopyReadbackTexture_RenderThread(MoveTemp(OldestResolveTarget->FunctionCallback), OldestResolveTarget->FunctionPayload);
		}
	}


	CurrentFrameIndex = (CurrentFrameIndex + 1) % Surfaces.Num();
}
