#pragma once

#include "CoreMinimal.h"
#include <immintrin.h>
#include <emmintrin.h>

namespace UnrealCV
{
namespace RenderUtil
{

inline void SetAlphaAVX2(TArray<FColor>& PixelData)
{
    const int32 Num = PixelData.Num();
    FColor* Ptr = PixelData.GetData();

    __m256i AlphaMask = _mm256_set1_epi32(0xFF000000);
    __m256i RGBMask = _mm256_set1_epi32(0x00FFFFFF);

    uint32_t* Data = reinterpret_cast<uint32_t*>(Ptr);
    int32 i = 0;

    for (; i + 7 < Num; i += 8)
    {
        __m256i Pixels = _mm256_loadu_si256((__m256i*)&Data[i]);
        __m256i Result = _mm256_or_si256(
            _mm256_and_si256(Pixels, RGBMask),
            AlphaMask
        );
        _mm256_storeu_si256((__m256i*)&Data[i], Result);
    }

    for (; i < Num; ++i)
    {
        Ptr[i].A = 255;
    }
}

inline void SetAlphaSSE2(TArray<FColor>& PixelData)
{
    const int32 Num = PixelData.Num();
    FColor* Ptr = PixelData.GetData();

    __m128i AlphaMask = _mm_set1_epi32(0xFF000000);
    __m128i RGBMask = _mm_set1_epi32(0x00FFFFFF);

    uint32_t* Data = reinterpret_cast<uint32_t*>(Ptr);
    int32 i = 0;

    for (; i + 3 < Num; i += 4)
    {
        __m128i Pixels = _mm_loadu_si128((__m128i*)&Data[i]);
        __m128i Result = _mm_or_si128(_mm_and_si128(Pixels, RGBMask), AlphaMask);
        _mm_storeu_si128((__m128i*)&Data[i], Result);
    }

    for (; i < Num; ++i)
    {
        Ptr[i].A = 255;
    }
}

inline void SetAlpha(TArray<FColor>& PixelData)
{
    ParallelFor(PixelData.Num(), [&](int32 i)
    {
        PixelData.GetData()[i].A = 255;
    });
}

inline void FixAlphaIfNeeded(TArray<FColor>& PixelData, EPixelFormat Format)
{
    if (Format == EPixelFormat::PF_B8G8R8A8 && PixelData.Num() > 0 && PixelData[0].A == 0)
    {
        SetAlphaAVX2(PixelData);
    }
}

}
}
