#pragma once

#include "Math/Float16Color.h"
#include "Math/PackedVector.h"
#include "Math/Plane.h"
#include "Math/UnrealMathUtility.h"
#include "RHI.h"
#include "RHITypes.h"
#include "RHISurfaceDataConversion.h"

namespace UnrealCV
{
namespace RenderUtil
{

inline void ConvertRawB8G8R8A8ToFColor(
    uint32 Width, uint32 Height,
    const uint8* In, uint32 SrcPitch,
    FColor* Out)
{
    const uint32 DstPitch = Width * sizeof(FColor);

    if (DstPitch == SrcPitch)
    {
        FPlatformMemory::Memcpy(Out, In, Width * Height * sizeof(FColor));
    }
    else
    {
        check(SrcPitch > DstPitch);
        for (uint32 Y = 0; Y < Height; Y++)
        {
            const FColor* SrcPtr = (const FColor*)(In + Y * SrcPitch);
            FColor* DestPtr = Out + Y * Width;
            FMemory::Memcpy(DestPtr, SrcPtr, DstPitch);
        }
    }
}

inline void ConvertRawR16G16B16A16FToFloat16(
    uint32 Width, uint32 Height,
    const uint8* In, uint32 SrcPitch,
    FFloat16Color* Out)
{
    const uint32 DstPitch = Width * sizeof(FFloat16Color);

    if (DstPitch == SrcPitch)
    {
        FPlatformMemory::Memcpy(Out, In, Width * Height * sizeof(FFloat16Color));
    }
    else
    {
        check(SrcPitch > DstPitch);
        for (uint32 Y = 0; Y < Height; Y++)
        {
            const FFloat16Color* SrcPtr = (const FFloat16Color*)(In + Y * SrcPitch);
            FFloat16Color* DestPtr = Out + Y * Width;
            FMemory::Memcpy(DestPtr, SrcPtr, DstPitch);
        }
    }
}

inline void ConvertRawR16G16B16A16FToFColor(
    uint32 Width, uint32 Height,
    const uint8* In, uint32 SrcPitch,
    FColor* Out, bool LinearToGamma)
{
    check(sizeof(FFloat16) == sizeof(uint16));

    ParallelFor(Height, [&](int32 Y)
    {
        const FFloat16* SrcPtr = (const FFloat16*)(In + Y * SrcPitch);
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

inline bool ConvertRawSurfaceToFColor(
    EPixelFormat Format,
    uint32 Width, uint32 Height,
    const uint8* In, uint32 SrcPitch,
    FColor* Out,
    FReadSurfaceDataFlags InFlags)
{
    bool bLinearToGamma = InFlags.GetLinearToGamma();

    if (Format == PF_B8G8R8A8)
    {
        ConvertRawB8G8R8A8ToFColor(Width, Height, In, SrcPitch, Out);
        return true;
    }
    else if (Format == PF_FloatRGBA)
    {
        ConvertRawR16G16B16A16FToFColor(Width, Height, In, SrcPitch, Out, bLinearToGamma);
        return true;
    }
    else
    {
        return ConvertRAWSurfaceDataToFColor(Format, Width, Height, const_cast<uint8*>(const_cast<uint8*>(In)), SrcPitch, Out, InFlags);
    }
}

inline bool ConvertRawSurfaceToFloat16(
    EPixelFormat Format,
    uint32 Width, uint32 Height,
    const uint8* In, uint32 SrcPitch,
    FFloat16Color* Out,
    FReadSurfaceDataFlags InFlags)
{
    if (Format == PF_FloatRGBA)
    {
        ConvertRawR16G16B16A16FToFloat16(Width, Height, In, SrcPitch, Out);
        return true;
    }
    else
    {
        check(0);
        return false;
    }
}

}
}
