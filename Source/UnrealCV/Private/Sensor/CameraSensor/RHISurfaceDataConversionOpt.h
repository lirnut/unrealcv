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
	const uint32 DstPitch = Width * sizeof(FColor);

	// If source & dest pitch matches, perform a single memcpy.
	if (DstPitch == SrcPitch)
	{
		UE_LOG(LogTemp, Warning, TEXT("ConvertRawB8G8R8A8DataToFColorOpt: SrcPitch == DstPitch"));
		
		FPlatformMemory::Memcpy(Out, In, Width * Height * sizeof(FColor));
	}
	else
	{
		check(SrcPitch > DstPitch);
		UE_LOG(LogTemp, Warning, TEXT("ConvertRawB8G8R8A8DataToFColorOpt: SrcPitch != DstPitch"));

		// Need to copy row wise since the Pitch does not match the Width.
		for (uint32 Y = 0; Y < Height; Y++)
		{
			FColor* SrcPtr = (FColor*)(In + Y * SrcPitch);
			FColor* DestPtr = Out + Y * Width;
			FMemory::Memcpy(DestPtr, SrcPtr, DstPitch);
		}
	}
}

// inline void ConvertRawR16G16B16A16FDataToFFloat16ColorOpt(uint32 Width, uint32 Height, uint8* In, uint32 SrcPitch, FFloat16Color* Out)
// {
// 	const uint32 DstPitch = Width * sizeof(FFloat16Color);

// 	// If source & dest pitch matches, perform a single memcpy.
// 	if (DstPitch == SrcPitch)
// 	{
// 		FPlatformMemory::Memcpy(Out, In, Width * Height * sizeof(FFloat16Color));
// 	}
// 	else
// 	{
// 		check(SrcPitch > DstPitch);

// 		// Need to copy row wise since the Pitch does not match the Width.
// 		for (uint32 Y = 0; Y < Height; Y++)
// 		{
// 			FFloat16Color* SrcPtr = (FFloat16Color*)(In + Y * SrcPitch);
// 			FFloat16Color* DestPtr = Out + Y * Width;
// 			FMemory::Memcpy(DestPtr, SrcPtr, DstPitch);
// 		}
// 	}
// }



// inline
bool ConvertRAWSurfaceDataToFColorOpt(EPixelFormat Format, uint32 Width, uint32 Height, uint8 *In, uint32 SrcPitch, FColor* Out, FReadSurfaceDataFlags InFlags)
{
	// bool bLinearToGamma = InFlags.GetLinearToGamma();

	if (Format == PF_B8G8R8A8)
	{
		ConvertRawB8G8R8A8DataToFColorOpt(Width, Height, In, SrcPitch, Out);
		return true;
	}
	else
	{
		return ConvertRAWSurfaceDataToFColor(Format, Width, Height, In, SrcPitch, Out, InFlags);
	}
}