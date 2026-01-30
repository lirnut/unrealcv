#pragma once

#include "Math/Float16Color.h"
#include "Math/PackedVector.h"
#include "Math/Plane.h"
#include "Math/UnrealMathUtility.h"
#include "RHI.h"
#include "RHITypes.h"
#include "RHISurfaceDataConversion.h"
#include "UnrealcvLog.h"


// inline void ConvertRawR8G8B8A8DataToFColorOpt(uint32 Width, uint32 Height, uint8 *In, uint32 SrcPitch, FColor* Out)
// {
// 	for (uint32 Y = 0; Y < Height; Y++)
// 	{
// 		FColor* SrcPtr = (FColor*)(In + Y * SrcPitch);
// 		FColor* DestPtr = Out + Y * Width;
// 		for (uint32 X = 0; X < Width; X++)
// 		{
// 			*DestPtr = FColor(SrcPtr->B, SrcPtr->G, SrcPtr->R, SrcPtr->A);
// 			++SrcPtr;
// 			++DestPtr;
// 		}
// 	}
// }

inline void ConvertRawB8G8R8A8DataToFColorOpt(uint32 Width, uint32 Height, uint8 *In, uint32 SrcPitch, FColor* Out)
{
	// UE_LOG(LogTemp, Warning, TEXT("ConvertRawB8G8R8A8DataToFColorOpt: called"));
	const uint32 DstPitch = Width * sizeof(FColor);

	// If source & dest pitch matches, perform a single memcpy.
	if (DstPitch == SrcPitch)
	{
		FPlatformMemory::Memcpy(Out, In, Width * Height * sizeof(FColor));
	}
	else
	{
		check(SrcPitch > DstPitch);
		// UE_LOG(LogTemp, Warning, TEXT("ConvertRawB8G8R8A8DataToFColorOpt: SrcPitch != DstPitch"));
		// UE_LOG(LogTemp, Warning, TEXT("ConvertRawB8G8R8A8DataToFColorOpt: SrcPitch = %d, DstPitch = %d"), SrcPitch, DstPitch);

		// Need to copy row wise since the Pitch does not match the Width.
		for (uint32 Y = 0; Y < Height; Y++)
		{
			FColor* SrcPtr = (FColor*)(In + Y * SrcPitch);
			FColor* DestPtr = Out + Y * Width;
			FMemory::Memcpy(DestPtr, SrcPtr, DstPitch);
		}
	}
}

inline void ConvertRawB8G8R8A8DataToFColorOptWithMinMaxMapping(uint32 Width, uint32 Height, uint8 *In, uint32 SrcPitch, FColor* Out, bool LinearToGamma)
{
	// UE_LOG(LogTemp, Warning, TEXT("ConvertRawB8G8R8A8DataToFColorOptWithMinMaxMapping: called"));
	FPlane	MinValue(255.0f, 255.0f, 255.0f, 255.0f),
		MaxValue(0.0f, 0.0f, 0.0f, 0.0f);

	ParallelFor(Height, [&](int32 Y)
	{
		FColor* SrcPtr = (FColor*)(In + Y * SrcPitch);

		for (uint32 X = 0; X < Width; X++)
		{
			MinValue.X = FMath::Min<float>((float)SrcPtr->R, MinValue.X);
			MinValue.Y = FMath::Min<float>((float)SrcPtr->G, MinValue.Y);
			MinValue.Z = FMath::Min<float>((float)SrcPtr->B, MinValue.Z);
			MinValue.W = FMath::Min<float>((float)SrcPtr->A, MinValue.W);
			MaxValue.X = FMath::Max<float>((float)SrcPtr->R, MaxValue.X);
			MaxValue.Y = FMath::Max<float>((float)SrcPtr->G, MaxValue.Y);
			MaxValue.Z = FMath::Max<float>((float)SrcPtr->B, MaxValue.Z);
			MaxValue.W = FMath::Max<float>((float)SrcPtr->A, MaxValue.W);
			++SrcPtr;
		}
	});

	ParallelFor(Height, [&](int32 Y)
	{
		FColor* SrcPtr = (FColor*)(In + Y * SrcPitch);
		FColor* DestPtr = Out + Y * Width;

		for (uint32 X = 0; X < Width; X++)
		{
			float R = (SrcPtr->R - MinValue.X) / (MaxValue.X - MinValue.X);
			float G = (SrcPtr->G - MinValue.Y) / (MaxValue.Y - MinValue.Y);
			float B = (SrcPtr->B - MinValue.Z) / (MaxValue.Z - MinValue.Z);
			float A = (SrcPtr->A - MinValue.W) / (MaxValue.W - MinValue.W);
			FLinearColor LinearColor(R, G, B, A);
			*DestPtr = LinearColor.ToFColor(LinearToGamma);
			++SrcPtr;
			++DestPtr;
		}
	});
}

inline void ConvertRawR16G16B16A16FDataToFFloat16ColorOpt(uint32 Width, uint32 Height, uint8* In, uint32 SrcPitch, FFloat16Color* Out)
{
	// UE_LOG(LogTemp, Warning, TEXT("ConvertRawR16G16B16A16FDataToFFloat16ColorOpt: called"));
	const uint32 DstPitch = Width * sizeof(FFloat16Color);

	// If source & dest pitch matches, perform a single memcpy.
	if (DstPitch == SrcPitch)
	{
		FPlatformMemory::Memcpy(Out, In, Width * Height * sizeof(FFloat16Color));
	}
	else
	{
		check(SrcPitch > DstPitch);

		// Need to copy row wise since the Pitch does not match the Width.
		for (uint32 Y = 0; Y < Height; Y++)
		{
			FFloat16Color* SrcPtr = (FFloat16Color*)(In + Y * SrcPitch);
			FFloat16Color* DestPtr = Out + Y * Width;
			FMemory::Memcpy(DestPtr, SrcPtr, DstPitch);
		}
	}
}


inline void ConvertRawR16G16B16A16FDataToFColorOptWithMinMaxMapping(uint32 Width, uint32 Height, uint8 *In, uint32 SrcPitch, FColor* Out, bool LinearToGamma)
{
	// UE_LOG(LogTemp, Warning, TEXT("ConvertRawR16G16B16A16FDataToFColorOptWithMinMaxMapping: called"));
	FPlane	MinValue(0.0f, 0.0f, 0.0f, 0.0f),
		MaxValue(1.0f, 1.0f, 1.0f, 1.0f);

	check(sizeof(FFloat16) == sizeof(uint16));

	ParallelFor(Height, [&](int32 Y)
	{
		FFloat16* SrcPtr = (FFloat16*)(In + Y * SrcPitch);

		for (uint32 X = 0; X < Width; X++)
		{
			MinValue.X = FMath::Min<float>(SrcPtr[0], MinValue.X);
			MinValue.Y = FMath::Min<float>(SrcPtr[1], MinValue.Y);
			MinValue.Z = FMath::Min<float>(SrcPtr[2], MinValue.Z);
			MinValue.W = FMath::Min<float>(SrcPtr[3], MinValue.W);
			MaxValue.X = FMath::Max<float>(SrcPtr[0], MaxValue.X);
			MaxValue.Y = FMath::Max<float>(SrcPtr[1], MaxValue.Y);
			MaxValue.Z = FMath::Max<float>(SrcPtr[2], MaxValue.Z);
			MaxValue.W = FMath::Max<float>(SrcPtr[3], MaxValue.W);
			SrcPtr += 4;
		}
	});

	ParallelFor(Height, [&](int32 Y)
	{
		FFloat16* SrcPtr = (FFloat16*)(In + Y * SrcPitch);
		FColor* DestPtr = Out + Y * Width;

		for (uint32 X = 0; X < Width; X++)
		{
			*DestPtr =
				FLinearColor(
				(SrcPtr[0] - MinValue.X) / (MaxValue.X - MinValue.X),
					(SrcPtr[1] - MinValue.Y) / (MaxValue.Y - MinValue.Y),
					(SrcPtr[2] - MinValue.Z) / (MaxValue.Z - MinValue.Z),
					(SrcPtr[3] - MinValue.W) / (MaxValue.W - MinValue.W)
				).ToFColor(LinearToGamma);
			SrcPtr += 4;
			++DestPtr;
		}
	});
}




inline void ConvertRawR16G16B16A16FDataToFColorOpt(uint32 Width, uint32 Height, uint8 *In, uint32 SrcPitch, FColor* Out, bool LinearToGamma)
{
	// UE_LOG(LogTemp, Warning, TEXT("ConvertRawR16G16B16A16FDataToFColorOpt: called"));
	check(sizeof(FFloat16) == sizeof(uint16));

	ParallelFor(Height, [&](int32 Y)
	{
		FFloat16* SrcPtr = (FFloat16*)(In + Y * SrcPitch);
		FColor* DestPtr = Out + Y * Width;

		for (uint32 X = 0; X < Width; X++)
		{
			float R = (float)SrcPtr[0];
			float G = (float)SrcPtr[1];
			float B = (float)SrcPtr[2];
			float A = (float)SrcPtr[3];

			*DestPtr = FLinearColor(R, G, B, A).ToFColor(LinearToGamma);
			SrcPtr += 4;
			++DestPtr;
		}
	});
}



inline bool ConvertRAWSurfaceDataToFColorOpt(EPixelFormat Format, uint32 Width, uint32 Height, uint8 *In, uint32 SrcPitch, FColor* Out, FReadSurfaceDataFlags InFlags_)
{
	// UE_LOG(LogTemp, Warning, TEXT("ConvertRAWSurfaceDataToFColorOpt: called"));
	FReadSurfaceDataFlags InFlags(RCM_MinMax); // Read Data withoud changing them
	bool bLinearToGamma = InFlags.GetLinearToGamma();

	if (Format == PF_B8G8R8A8)
	{
		ConvertRawB8G8R8A8DataToFColorOpt(Width, Height, In, SrcPitch, Out);
		// ConvertRawB8G8R8A8DataToFColorOptWithMinMaxMapping(Width, Height, In, SrcPitch, Out, bLinearToGamma);
		return true;
	}
	else if (Format == PF_FloatRGBA)
	{
		ConvertRawR16G16B16A16FDataToFColorOpt(Width, Height, In, SrcPitch, Out, bLinearToGamma);
		// ConvertRawR16G16B16A16FDataToFColorOptWithMinMaxMapping(Width, Height, In, SrcPitch, Out, bLinearToGamma);
		return true;
	}
	else
	{
		return ConvertRAWSurfaceDataToFColor(Format, Width, Height, In, SrcPitch, Out, InFlags);
	}
}

inline bool ConvertRAWSurfaceDataToFFloat16ColorOpt(EPixelFormat Format, uint32 Width, uint32 Height, uint8 *In, uint32 SrcPitch, FFloat16Color* Out, FReadSurfaceDataFlags InFlags_)
{
	// UE_LOG(LogTemp, Warning, TEXT("ConvertRAWSurfaceDataToFFloat16ColorOpt: called"));
	FReadSurfaceDataFlags InFlags(RCM_MinMax); // Read Data withoud changing them
	bool bLinearToGamma = InFlags.GetLinearToGamma();

	if (Format == PF_B8G8R8A8)
	{
		// unsupported format; checked for this earlier
		// UE RHIReadback 也不支持这种做法
		// 还有几种Float32, Float10的格式，估计用不到，不管了
		check(0);
		return false;
	}
	else if (Format == PF_FloatRGBA)
	{
		ConvertRawR16G16B16A16FDataToFFloat16ColorOpt(Width, Height, In, SrcPitch, Out);
		// ConvertRawR16G16B16A16FDataToFFloat16ColorOptWithMinMax(Width, Height, In, SrcPitch, Out);
		return true;
	}
	else
	{
		// unsupported format; checked for this earlier
		// UE RHIReadback 也不支持这种做法
		// 还有几种Float32, Float10的格式，估计用不到，不管了
		check(0);
		return false;
	}
}
