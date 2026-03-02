#pragma once

#include "CoreMinimal.h"
#include "Misc/FrameRate.h"
#include "UnrealCVMP4EncoderCommon.h"
#include "ImagePixelData.h"

#if PLATFORM_WINDOWS

class UNREALCV_API FUnrealCVMP4Encoder
{
public:
	FUnrealCVMP4Encoder(const FUnrealCVMP4EncoderOptions& InOptions);
	~FUnrealCVMP4Encoder();

	bool Initialize();
	void Finalize();
	bool TryFinalize();
	bool WriteFrame(const uint8* InFrameData, EImagePixelType InPixelFormat = EImagePixelType::Float16);
	bool WriteAudioSample(const TArrayView<int16>& InAudioSamples);

	const FUnrealCVMP4EncoderOptions& GetOptions() const { return Options; }
	bool IsInitialized() const { return bInitialized; }
	int32 GetWidth() const { return Options.Width; }
	int32 GetHeight() const { return Options.Height; }

private:
	bool InitializeEncoder();

private:
	FUnrealCVMP4EncoderOptions Options;
	bool bInitialized;
	bool bFinalized;
	uint64 NumVideoSamplesWritten;
	uint64 NumAudioSamplesWritten;
	struct IMFSinkWriter* SinkWriter;
	uint32 VideoStreamIndex;
	uint32 AudioStreamIndex;
};

#else

class UNREALCV_API FUnrealCVMP4Encoder
{
public:
	FUnrealCVMP4Encoder(const FUnrealCVMP4EncoderOptions& InOptions) {}
	~FUnrealCVMP4Encoder() {}

	bool Initialize() { return false; }
	void Finalize() {}
	bool TryFinalize() { return false; }
	bool WriteFrame(const uint8* InFrameData, EImagePixelType InPixelFormat = EImagePixelType::Float16) { return false; }
	bool WriteAudioSample(const TArrayView<int16>& InAudioSamples) { return false; }

	const FUnrealCVMP4EncoderOptions& GetOptions() const { static FUnrealCVMP4EncoderOptions Dummy; return Dummy; }
	bool IsInitialized() const { return false; }
	int32 GetWidth() const { return 0; }
	int32 GetHeight() const { return 0; }
};

#endif
