#pragma once
#include "CoreMinimal.h"
#include "Misc/FrameRate.h"
#include "UnrealCVMP4EncoderCommon.generated.h"

UENUM(BlueprintType)
enum class EUnrealCVMP4EncodeProfile : uint8
{
	Baseline,
	Main,
	High,
};

UENUM(BlueprintType)
enum class EUnrealCVMP4EncodeLevel : uint8
{
	Auto = 0,
	Level1 = 10,
	Level1_b = 11,
	Level1_1 = 11,
	Level1_2 = 12,
	Level1_3 = 13,
	Level2 = 20,
	Level2_1 = 21,
	Level2_2 = 22,
	Level3 = 30,
	Level3_1 = 31,
	Level3_2 = 32,
	Level4 = 40,
	Level4_1 = 41,
	Level4_2 = 42,
	Level5 = 50,
	Level5_1 = 51,
	Level5_2 = 52,
};

UENUM(BlueprintType)
enum class EUnrealCVMP4EncodeRateControlMode : uint8
{
	ConstantQP UMETA(Hidden),
	Quality,
	VariableBitRate,
	VariableBitRate_Constrained UMETA(Hidden),
	ConstantBitRate UMETA(Hidden),
};

struct FUnrealCVMP4EncoderOptions
{
	FUnrealCVMP4EncoderOptions()
		: OutputFilename()
		, Width(0)
		, Height(0)
		, FrameRate(30, 1)
		, bIncludeAudio(true)
		, AudioChannelCount(2)
		, AudioSampleRate(48000)
		, AudioAverageBitRate(24000)
		, CommonMeanBitRate(12 * 1024 * 1024)
		, CommonMaxBitRate(16 * 1024 * 1024)
		, CommonQualityVsSpeed(100)
		, CommonConstantRateFactor(18)
		, EncodingProfile(EUnrealCVMP4EncodeProfile::High)
		, EncodingLevel(EUnrealCVMP4EncodeLevel::Auto)
		, EncodingRateControl(EUnrealCVMP4EncodeRateControlMode::VariableBitRate)
	{}

	FString OutputFilename;
	uint32 Width;
	uint32 Height;
	FFrameRate FrameRate;
	bool bIncludeAudio;
	uint32 AudioChannelCount;
	uint32 AudioSampleRate;
	uint32 AudioAverageBitRate;
	uint32 CommonMeanBitRate;
	uint32 CommonMaxBitRate;
	uint32 CommonQualityVsSpeed;
	uint32 CommonConstantRateFactor;
	EUnrealCVMP4EncodeProfile EncodingProfile;
	EUnrealCVMP4EncodeLevel EncodingLevel;
	EUnrealCVMP4EncodeRateControlMode EncodingRateControl;
};
