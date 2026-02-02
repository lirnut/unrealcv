#include "Encoder/UnrealCVMP4Encoder.h"
#include "UnrealcvLog.h"

#if PLATFORM_WINDOWS

#include "Sensor/CameraSensor/RHISurfaceDataConversionOpt.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/WindowsPlatformMisc.h"
#include "Windows/WindowsHWrapper.h"

THIRD_PARTY_INCLUDES_START
#include <Windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <Mfreadwrite.h>
#include <mferror.h>
#include <Codecapi.h>
#include <strmif.h>
THIRD_PARTY_INCLUDES_END

#include "Windows/HideWindowsPlatformTypes.h"

namespace
{
	FString GetCodecApiDllPath(ICodecAPI* InCodecAPI)
	{
		static const FString UnknownDLL(TEXT("Unknown"));

		if (!InCodecAPI)
		{
			return UnknownDLL;
		}

		void** VtableAddress = *reinterpret_cast<void***>(InCodecAPI);
		const void* QueryInterfaceAddress = VtableAddress[0];

		MEMORY_BASIC_INFORMATION MemoryInformation;
		if (VirtualQueryEx(GetCurrentProcess(), QueryInterfaceAddress, &MemoryInformation, sizeof(MemoryInformation)))
		{
			WCHAR DllPath[MAX_PATH] = {};
			if (GetModuleFileNameW(static_cast<HMODULE>(MemoryInformation.AllocationBase), DllPath, MAX_PATH))
			{
				return FString(DllPath);
			}
		}

		return UnknownDLL;
	}
}

FUnrealCVMP4Encoder::FUnrealCVMP4Encoder(const FUnrealCVMP4EncoderOptions& InOptions)
	: Options(InOptions)
	, bInitialized(false)
	, bFinalized(false)
	, NumVideoSamplesWritten(0)
	, NumAudioSamplesWritten(0)
	, SinkWriter(nullptr)
	, VideoStreamIndex(0)
	, AudioStreamIndex(0)
{
}

FUnrealCVMP4Encoder::~FUnrealCVMP4Encoder()
{
	Finalize();
}

bool FUnrealCVMP4Encoder::Initialize()
{
	if (!FWindowsPlatformMisc::CoInitialize())
	{
		UE_LOG(LogUnrealCV, Error, TEXT("Failed to initialize COM library."));
		return false;
	}

	HRESULT Result = MFStartup(MF_VERSION);
	if (!SUCCEEDED(Result))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("Failed to initialize Microsoft Media Foundation."));
		return false;
	}

	bool bResult = InitializeEncoder();
	if (!bResult)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("Failed to initialize Sink Writer."));
		return false;
	}

	bInitialized = true;
	return true;
}

void FUnrealCVMP4Encoder::Finalize()
{
	if (bFinalized || !bInitialized)
	{
		return;
	}

	if (SinkWriter)
	{
		HRESULT Result = SinkWriter->Finalize();
		if (!SUCCEEDED(Result))
		{
			UE_LOG(LogUnrealCV, Error, TEXT("Failed to finalize Sink Writer."));
		}

		SinkWriter->Release();
		SinkWriter = nullptr;
	}

	HRESULT Result = MFShutdown();
	if (!SUCCEEDED(Result))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("Failed to shut down Microsoft Media Foundation."));
	}

	FWindowsPlatformMisc::CoUninitialize();

	bFinalized = true;
}

bool FUnrealCVMP4Encoder::WriteFrame(const uint8* InFrameData, EUnrealCVPixelFormat InPixelFormat)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UnrealCVMP4Encoder_WriteFrame);
	if (!ensureMsgf(bInitialized && !bFinalized, TEXT("WriteFrame should not be called if not initialized or after finalize! Initialized: %d Finalized: %d"), bInitialized, bFinalized))
	{
		return false;
	}

	IMFSample* Sample = nullptr;
	IMFMediaBuffer* Buffer = nullptr;
	HRESULT Result;

	const LONG DestStride = Options.Width * 4;
	const LONG BufferSize = DestStride * Options.Height;

	TArray<uint8> ConvertedData;
	ConvertedData.SetNum(BufferSize);

	if (InPixelFormat == EUnrealCVPixelFormat::Float16)
	{
		const uint32 InputStride = Options.Width * 8;
		ConvertRawR16G16B16A16FDataToFColorOpt(
			Options.Width,
			Options.Height,
			(uint8*)InFrameData,
			InputStride,
			(FColor*)ConvertedData.GetData(),
			true
		);
	}
	else if (InPixelFormat == EUnrealCVPixelFormat::Uint8)
	{
		const uint32 InputStride = Options.Width * 4;
		ConvertRawB8G8R8A8DataToFColorOpt(
			Options.Width,
			Options.Height,
			(uint8*)InFrameData,
			InputStride,
			(FColor*)ConvertedData.GetData()
		);
	}
	else
	{
		UE_LOG(LogUnrealCV, Error, TEXT("Unsupported pixel format: %d"), (int32)InPixelFormat);
		return false;
	}

	{
		Result = MFCreateMemoryBuffer(BufferSize, &Buffer);

		if (!SUCCEEDED(Result))
		{
			UE_LOG(LogUnrealCV, Error, TEXT("Failed to allocate Media Foundation Buffer to write frame. Width: %d Height: %d Size: %d"), Options.Width, Options.Height, BufferSize);
			return false;
		}
	}

	BYTE* DestinationData = nullptr;
	{
		Result = Buffer->Lock(&DestinationData, NULL, NULL);
	}

	if (SUCCEEDED(Result))
	{
		Result = MFCopyImage(DestinationData, DestStride, ConvertedData.GetData(), DestStride, DestStride, Options.Height);
	}

	if (SUCCEEDED(Result))
	{
		Result = Buffer->Unlock();
	}

	if (SUCCEEDED(Result))
	{
		Result = Buffer->SetCurrentLength(BufferSize);
	}

	if (SUCCEEDED(Result))
	{
		Result = MFCreateSample(&Sample);
	}

	if (SUCCEEDED(Result) && Sample)
	{
		Result = Sample->AddBuffer(Buffer);
	}

	if (SUCCEEDED(Result) && Sample)
	{
		uint64 FrameDuration = (10 * 1000 * 1000) * Options.FrameRate.AsInterval();
		Result = Sample->SetSampleTime(FrameDuration * NumVideoSamplesWritten);
		NumVideoSamplesWritten++;
	}

	if (SUCCEEDED(Result) && Sample)
	{
		uint64 FrameDuration = (10 * 1000 * 1000) * Options.FrameRate.AsInterval();
		Result = Sample->SetSampleDuration(FrameDuration);
	}

	if (SUCCEEDED(Result) && Sample)
	{
		Result = SinkWriter->WriteSample(VideoStreamIndex, Sample);
	}

	if (Sample)
	{
		Sample->Release();
	}
	if (Buffer)
	{
		Buffer->Release();
	}

	return Result == S_OK;
}

bool FUnrealCVMP4Encoder::WriteAudioSample(const TArrayView<int16>& InAudioSamples)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UnrealCVMP4Encoder_WriteAudioSample);

	if (!Options.bIncludeAudio)
	{
		return true;
	}

	if (!ensureMsgf(bInitialized && !bFinalized, TEXT("WriteAudioSample should not be called if not initialized or after finalize! Initialized: %d Finalized: %d"), bInitialized, bFinalized))
	{
		return false;
	}

	int32 SampleCountPerChannel = InAudioSamples.Num() / Options.AudioChannelCount;
	int32 SamplesPerFrame = Options.AudioSampleRate * Options.FrameRate.AsInterval();
	int32 NumFramesOfData = SampleCountPerChannel / SamplesPerFrame;

	HRESULT Result = S_OK;

	for (int32 SampleIndex = 0; SampleIndex < NumFramesOfData; SampleIndex++)
	{
		IMFSample* Sample = nullptr;
		IMFMediaBuffer* Buffer = nullptr;

		const LONG BufferSize = SamplesPerFrame * sizeof(int16) * Options.AudioChannelCount;
		Result = MFCreateMemoryBuffer(BufferSize, &Buffer);

		if (!SUCCEEDED(Result))
		{
			UE_LOG(LogUnrealCV, Error, TEXT("Failed to allocate Media Foundation Buffer to write audio sample. Buffer Size: %d"), BufferSize);
			return false;
		}

		BYTE* DestinationData = nullptr;
		Result = Buffer->Lock(&DestinationData, NULL, NULL);

		if (SUCCEEDED(Result))
		{
			int16* SourceOffset = (int16*)InAudioSamples.GetData() + (SampleIndex * (SamplesPerFrame * Options.AudioChannelCount));

			Result = MFCopyImage(DestinationData,
				BufferSize,
				(BYTE*)SourceOffset,
				BufferSize,
				BufferSize,
				1);
		}

		Buffer->Unlock();

		if (SUCCEEDED(Result))
		{
			Result = Buffer->SetCurrentLength(BufferSize);
		}

		if (SUCCEEDED(Result))
		{
			Result = MFCreateSample(&Sample);
		}

		if (SUCCEEDED(Result) && Sample)
		{
			Result = Sample->AddBuffer(Buffer);
		}

		if (SUCCEEDED(Result) && Sample)
		{
			uint64 FrameDuration = (10 * 1000 * 1000) * Options.FrameRate.AsInterval();
			Result = Sample->SetSampleDuration(FrameDuration);
		}

		if (SUCCEEDED(Result) && Sample)
		{
			uint64 FrameDuration = (10 * 1000 * 1000) * Options.FrameRate.AsInterval();
			Result = Sample->SetSampleTime(NumAudioSamplesWritten * FrameDuration);

			NumAudioSamplesWritten++;
		}

		if (SUCCEEDED(Result) && Sample)
		{
			Result = SinkWriter->WriteSample(AudioStreamIndex, Sample);
		}

		if (Sample)
		{
			Sample->Release();
		}
		if (Buffer)
		{
			Buffer->Release();
		}

		if (!SUCCEEDED(Result))
		{
			return false;
		}
	}

	return Result == S_OK;
}

static uint32_t GetEncodingProfile(const EUnrealCVMP4EncodeProfile InProfile)
{
	switch (InProfile)
	{
	case EUnrealCVMP4EncodeProfile::Baseline: return eAVEncH264VProfile_Base;
	case EUnrealCVMP4EncodeProfile::Main: return eAVEncH264VProfile_Main;
	case EUnrealCVMP4EncodeProfile::High: return eAVEncH264VProfile_High;
	default:
		check(false);
	}
	return 0;
}

static uint32_t GetEncodingRateControl(const EUnrealCVMP4EncodeRateControlMode InProfile)
{
	switch (InProfile)
	{
	case EUnrealCVMP4EncodeRateControlMode::ConstantBitRate: return eAVEncCommonRateControlMode_CBR;
	case EUnrealCVMP4EncodeRateControlMode::VariableBitRate_Constrained: return eAVEncCommonRateControlMode_PeakConstrainedVBR;
	case EUnrealCVMP4EncodeRateControlMode::VariableBitRate: return eAVEncCommonRateControlMode_UnconstrainedVBR;
	case EUnrealCVMP4EncodeRateControlMode::Quality: return eAVEncCommonRateControlMode_Quality;
	case EUnrealCVMP4EncodeRateControlMode::ConstantQP: return eAVEncCommonRateControlMode_PeakConstrainedVBR;
	default:
		check(false);
	}
	return 0;
}

static HRESULT CreateVideoMediaOutputStream(IMFMediaType** OutVideoMediaTypeOutput, const FUnrealCVMP4EncoderOptions& InOptions)
{
	HRESULT Result = S_OK;
	if (SUCCEEDED(Result))
	{
		Result = MFCreateMediaType(OutVideoMediaTypeOutput);
	}

 	if (SUCCEEDED(Result))
	{
		Result = (*OutVideoMediaTypeOutput)->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	}

	if (SUCCEEDED(Result))
	{
		Result = (*OutVideoMediaTypeOutput)->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
	}

	if (SUCCEEDED(Result))
	{
		Result = (*OutVideoMediaTypeOutput)->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
	}

	if (SUCCEEDED(Result))
	{
		Result = MFSetAttributeSize((*OutVideoMediaTypeOutput), MF_MT_FRAME_SIZE, InOptions.Width, InOptions.Height);
	}

	if (SUCCEEDED(Result))
	{
		Result = MFSetAttributeRatio((*OutVideoMediaTypeOutput), MF_MT_FRAME_RATE, InOptions.FrameRate.Numerator, InOptions.FrameRate.Denominator);
	}

	if (SUCCEEDED(Result))
	{
		Result = MFSetAttributeRatio((*OutVideoMediaTypeOutput), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
	}

	if (SUCCEEDED(Result))
	{
		Result = (*OutVideoMediaTypeOutput)->SetUINT32(MF_MT_VIDEO_PRIMARIES, MFVideoPrimaries_BT709);
	}

	if (SUCCEEDED(Result))
	{
		Result = (*OutVideoMediaTypeOutput)->SetUINT32(MF_MT_TRANSFER_FUNCTION, MFVideoTransFunc_sRGB);
	}

	if (SUCCEEDED(Result))
	{
		Result = (*OutVideoMediaTypeOutput)->SetUINT32(MF_MT_YUV_MATRIX, MFVideoTransferMatrix_BT709);
	}

	if (SUCCEEDED(Result))
	{
		Result = (*OutVideoMediaTypeOutput)->SetUINT32(MF_MT_VIDEO_NOMINAL_RANGE, MFNominalRange_16_235);
	}

	if (SUCCEEDED(Result))
	{
		Result = (*OutVideoMediaTypeOutput)->SetUINT32(CODECAPI_AVEncCommonQualityVsSpeed, InOptions.CommonQualityVsSpeed);
	}

	{
		if (SUCCEEDED(Result))
		{
			Result = (*OutVideoMediaTypeOutput)->SetUINT32(MF_MT_MPEG2_PROFILE, GetEncodingProfile(InOptions.EncodingProfile));
		}
		if (SUCCEEDED(Result))
		{
			EUnrealCVMP4EncodeLevel Level = InOptions.EncodingLevel;
			uint32_t LevelValue = Level == EUnrealCVMP4EncodeLevel::Auto ? -1 : (uint32_t)Level;
			Result = (*OutVideoMediaTypeOutput)->SetUINT32(MF_MT_MPEG2_LEVEL, LevelValue);
		}

		if (SUCCEEDED(Result))
		{
			Result = (*OutVideoMediaTypeOutput)->SetUINT32(CODECAPI_AVEncH264CABACEnable, true);
		}

		if (SUCCEEDED(Result))
		{
			Result = (*OutVideoMediaTypeOutput)->SetUINT32(CODECAPI_AVEncCommonRateControlMode, GetEncodingRateControl(InOptions.EncodingRateControl));
		}

		if (SUCCEEDED(Result))
		{
			Result = (*OutVideoMediaTypeOutput)->SetUINT32(CODECAPI_AVEncMPVDefaultBPictureCount, 2);
		}
	}

	return Result;
}

static HRESULT CreateAudioMediaOutputStream(IMFMediaType** OutVideoMediaTypeOutput, const FUnrealCVMP4EncoderOptions& InOptions)
{
	HRESULT Result = S_OK;
	if (SUCCEEDED(Result))
	{
		Result = MFCreateMediaType(OutVideoMediaTypeOutput);
	}

	if (SUCCEEDED(Result))
	{
		Result = (*OutVideoMediaTypeOutput)->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
	}

	if (SUCCEEDED(Result))
	{
		Result = (*OutVideoMediaTypeOutput)->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC);
	}

	if (SUCCEEDED(Result))
	{
		Result = (*OutVideoMediaTypeOutput)->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
	}

	if (SUCCEEDED(Result))
	{
		Result = (*OutVideoMediaTypeOutput)->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, InOptions.AudioSampleRate);
	}

	if (SUCCEEDED(Result))
	{
		Result = (*OutVideoMediaTypeOutput)->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, InOptions.AudioChannelCount);
	}

	if (SUCCEEDED(Result))
	{
		Result = (*OutVideoMediaTypeOutput)->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, InOptions.AudioAverageBitRate);
	}

	return Result;
}

static HRESULT CreateVideoMediaTypeIn(IMFMediaType** OutVideoMediaTypeIn, const FUnrealCVMP4EncoderOptions& InOptions)
{
	HRESULT Result = S_OK;

	if (SUCCEEDED(Result))
	{
		Result = MFCreateMediaType(OutVideoMediaTypeIn);
	}

	if (SUCCEEDED(Result))
	{
		Result = (*OutVideoMediaTypeIn)->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	}

	if (SUCCEEDED(Result))
	{
		Result = (*OutVideoMediaTypeIn)->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_ARGB32);
	}

	if (SUCCEEDED(Result))
	{
		Result = (*OutVideoMediaTypeIn)->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
	}

	if (SUCCEEDED(Result))
	{
		Result = MFSetAttributeSize((*OutVideoMediaTypeIn), MF_MT_FRAME_SIZE, InOptions.Width, InOptions.Height);
	}

	if (SUCCEEDED(Result))
	{
		Result = MFSetAttributeRatio((*OutVideoMediaTypeIn), MF_MT_FRAME_RATE, InOptions.FrameRate.Numerator, InOptions.FrameRate.Denominator);
	}

	if (SUCCEEDED(Result))
	{
		Result = MFSetAttributeRatio((*OutVideoMediaTypeIn), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
	}

	if (SUCCEEDED(Result))
	{
		Result = (*OutVideoMediaTypeIn)->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, true);
	}

	const uint32 Stride = InOptions.Width * 4;
	if (SUCCEEDED(Result))
	{
		Result = (*OutVideoMediaTypeIn)->SetUINT32(MF_MT_DEFAULT_STRIDE, Stride);
	}

	if (SUCCEEDED(Result))
	{
		Result = (*OutVideoMediaTypeIn)->SetUINT32(MF_MT_SAMPLE_SIZE, Stride * InOptions.Height);
	}

	return Result;
}

static HRESULT CreateAudioMediaTypeIn(IMFMediaType** OutAudioMediaTypeIn, const FUnrealCVMP4EncoderOptions& InOptions)
{
	HRESULT Result = S_OK;
	if (SUCCEEDED(Result))
	{
		Result = MFCreateMediaType(OutAudioMediaTypeIn);
	}

	if (SUCCEEDED(Result))
	{
		Result = (*OutAudioMediaTypeIn)->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
	}

	if (SUCCEEDED(Result))
	{
		Result = (*OutAudioMediaTypeIn)->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
	}

	const int32 BytesPerChannel = 2;

	if (SUCCEEDED(Result))
	{
		Result = (*OutAudioMediaTypeIn)->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, BytesPerChannel * 8);
	}

	if (SUCCEEDED(Result))
	{
		Result = (*OutAudioMediaTypeIn)->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, InOptions.AudioSampleRate);
	}

	if (SUCCEEDED(Result))
	{
		Result = (*OutAudioMediaTypeIn)->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, InOptions.AudioChannelCount);
	}

	if (SUCCEEDED(Result))
	{
		Result = (*OutAudioMediaTypeIn)->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, true);
	}

	if (SUCCEEDED(Result))
	{
		Result = (*OutAudioMediaTypeIn)->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, InOptions.AudioChannelCount * BytesPerChannel);
	}

	if (SUCCEEDED(Result))
	{
		Result = (*OutAudioMediaTypeIn)->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, (InOptions.AudioChannelCount * BytesPerChannel) * InOptions.AudioSampleRate);
	}

	return Result;
}

static HRESULT GetVideoStreamEncoderAttributes(IMFAttributes** OutEncoderAttributes, const FUnrealCVMP4EncoderOptions& InOptions)
{
	HRESULT Result = MFCreateAttributes(OutEncoderAttributes, 1);

	{
		if (InOptions.EncodingRateControl == EUnrealCVMP4EncodeRateControlMode::ConstantBitRate)
		{
			if (SUCCEEDED(Result))
			{
				Result = (*OutEncoderAttributes)->SetUINT32(CODECAPI_AVEncCommonMeanBitRate, InOptions.CommonMeanBitRate);
			}

			if (SUCCEEDED(Result))
			{
				Result = (*OutEncoderAttributes)->SetUINT32(CODECAPI_AVEncCommonBufferSize, InOptions.CommonMeanBitRate);
			}

		}
		else if (InOptions.EncodingRateControl == EUnrealCVMP4EncodeRateControlMode::VariableBitRate_Constrained)
		{
			if (SUCCEEDED(Result))
			{
				Result = (*OutEncoderAttributes)->SetUINT32(CODECAPI_AVEncCommonBufferSize, InOptions.CommonMeanBitRate);

			}
			if (SUCCEEDED(Result))
			{
				Result = (*OutEncoderAttributes)->SetUINT32(CODECAPI_AVEncCommonMaxBitRate, InOptions.CommonMaxBitRate);
			}
			if (SUCCEEDED(Result))
			{
				Result = (*OutEncoderAttributes)->SetUINT32(CODECAPI_AVEncCommonMeanBitRate, InOptions.CommonMeanBitRate);
			}
		}
		else if (InOptions.EncodingRateControl == EUnrealCVMP4EncodeRateControlMode::VariableBitRate)
		{
			if (SUCCEEDED(Result))
			{
				Result = (*OutEncoderAttributes)->SetUINT32(CODECAPI_AVEncCommonMaxBitRate, InOptions.CommonMaxBitRate);
			}
			if (SUCCEEDED(Result))
			{
				Result = (*OutEncoderAttributes)->SetUINT32(CODECAPI_AVEncCommonMeanBitRate, InOptions.CommonMeanBitRate);
			}
		}
		else if (InOptions.EncodingRateControl == EUnrealCVMP4EncodeRateControlMode::Quality)
		{
			if (SUCCEEDED(Result))
			{
				Result = (*OutEncoderAttributes)->SetUINT32(CODECAPI_AVEncCommonRateControlMode, eAVEncCommonRateControlMode_Quality);
			}
			if (SUCCEEDED(Result))
			{
				Result = (*OutEncoderAttributes)->SetUINT64(CODECAPI_AVEncVideoEncodeQP, InOptions.CommonConstantRateFactor);
			}
			if (SUCCEEDED(Result))
			{
				Result = (*OutEncoderAttributes)->SetUINT32(CODECAPI_AVEncCommonQuality, 0);
			}
		}
		else if (InOptions.EncodingRateControl == EUnrealCVMP4EncodeRateControlMode::ConstantQP)
		{
			if (SUCCEEDED(Result))
			{
				Result = (*OutEncoderAttributes)->SetUINT32(CODECAPI_AVEncCommonQuality, InOptions.CommonConstantRateFactor);
			}
			if (SUCCEEDED(Result))
			{
				Result = (*OutEncoderAttributes)->SetUINT32(CODECAPI_AVEncVideoMinQP, InOptions.CommonConstantRateFactor);
			}
			if (SUCCEEDED(Result))
			{
				Result = (*OutEncoderAttributes)->SetUINT32(CODECAPI_AVEncVideoMaxQP, InOptions.CommonConstantRateFactor);
			}
		}
	}

	return Result;
}

bool FUnrealCVMP4Encoder::InitializeEncoder()
{
	SinkWriter = nullptr;

	IMFMediaType* VideoMediaTypeOut = nullptr;
	IMFMediaType* VideoMediaTypeIn = nullptr;

	IMFMediaType* AudioMediaTypeOut = nullptr;
	IMFMediaType* AudioMediaTypeIn = nullptr;

	IMFAttributes* ConfigAttributes = nullptr;
	HRESULT Result = MFCreateAttributes(&ConfigAttributes, 1);

	if (SUCCEEDED(Result))
	{
		ConfigAttributes->SetUINT32(CODECAPI_AVLowLatencyMode, false);

		ConfigAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, true);

		ConfigAttributes->SetUINT32(MF_SINK_WRITER_DISABLE_THROTTLING, true);
	}

	if (SUCCEEDED(Result))
	{
		Result = MFCreateSinkWriterFromURL(*Options.OutputFilename, NULL, ConfigAttributes, &SinkWriter);
	}
	if (SUCCEEDED(Result))
	{
		Result = CreateVideoMediaOutputStream(&VideoMediaTypeOut, Options);
	}

	if (SUCCEEDED(Result) && SinkWriter)
	{
		Result = SinkWriter->AddStream(VideoMediaTypeOut, (DWORD*)&VideoStreamIndex);
	}

	if (Options.bIncludeAudio)
	{
		if (SUCCEEDED(Result))
		{
			Result = CreateAudioMediaOutputStream(&AudioMediaTypeOut, Options);
		}

		if (SUCCEEDED(Result) && SinkWriter)
		{
			Result = SinkWriter->AddStream(AudioMediaTypeOut, (DWORD*)&AudioStreamIndex);
		}

		if (SUCCEEDED(Result))
		{
			Result = CreateAudioMediaTypeIn(&AudioMediaTypeIn, Options);
		}
	}

	if (SUCCEEDED(Result))
	{
		Result = CreateVideoMediaTypeIn(&VideoMediaTypeIn, Options);
	}

	if (SUCCEEDED(Result) && SinkWriter)
	{
		IMFAttributes* pEncAttrs = nullptr;
		Result = GetVideoStreamEncoderAttributes(&pEncAttrs, Options);
		if (SUCCEEDED(Result))
		{
			Result = SinkWriter->SetInputMediaType(VideoStreamIndex, VideoMediaTypeIn, pEncAttrs);
		}

		if (pEncAttrs)
		{
			pEncAttrs->Release();
		}
	}

	if (Options.bIncludeAudio)
	{
		if (SUCCEEDED(Result) && SinkWriter)
		{
			Result = SinkWriter->SetInputMediaType(AudioStreamIndex, AudioMediaTypeIn, NULL);
		}
	}

	if (SUCCEEDED(Result) && SinkWriter)
	{
		ICodecAPI* CodecApi = nullptr;
		Result = SinkWriter->GetServiceForStream(VideoStreamIndex, GUID_NULL, __uuidof(ICodecAPI), (LPVOID*)&CodecApi);

		const FString CodecApiDllPath = GetCodecApiDllPath(CodecApi);
		UE_LOG(LogUnrealCV, Display, TEXT("Using the following encoder for the MP4 encode: %s"), *CodecApiDllPath);
	}

	if (SUCCEEDED(Result) && SinkWriter)
	{
		SinkWriter->BeginWriting();
	}

	if (SUCCEEDED(Result) && SinkWriter)
	{
		SinkWriter->AddRef();
	}

	if (SinkWriter)
	{
		SinkWriter->Release();
	}

	if (VideoMediaTypeOut)
	{
		VideoMediaTypeOut->Release();
	}
	if (VideoMediaTypeIn)
	{
		VideoMediaTypeIn->Release();
	}
	if (AudioMediaTypeOut)
	{
		AudioMediaTypeOut->Release();
	}
	if (AudioMediaTypeIn)
	{
		AudioMediaTypeIn->Release();
	}

	return Result == S_OK;
}

#endif
