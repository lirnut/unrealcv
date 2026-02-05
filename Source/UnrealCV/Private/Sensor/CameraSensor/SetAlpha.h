#pragma once

#include "emmintrin.h"
#include "Math/Float16Color.h"
#include "Math/PackedVector.h"
#include "Math/Plane.h"
#include "Math/UnrealMathUtility.h"
#include "RHI.h"
#include "RHITypes.h"
#include "UnrealcvLog.h"

inline void SetAlphaSSE2(TArray<FColor>& PixelData)
{
    const int32 Num = PixelData.Num();
    if (Num == 0) return;

    FColor* Ptr = PixelData.GetData();
    __m128i AlphaMask = _mm_set1_epi32(0xFF000000);
    __m128i RGBMask = _mm_set1_epi32(0x00FFFFFF);

    uint32_t* Data = reinterpret_cast<uint32_t*>(Ptr);
    int32 i = 0;

    for (; i + 3 < Num; i += 4)
    {
        __m128i Pixels = _mm_loadu_si128((const __m128i*)&Data[i]);
        __m128i Result = _mm_or_si128(_mm_and_si128(Pixels, RGBMask), AlphaMask);
        _mm_storeu_si128((__m128i*)&Data[i], Result);
    }

    for (; i < Num; ++i)
    {
        Ptr[i].A = 255;
    }
}

#if defined(__AVX2__) || defined(__AVX__)
inline void SetAlphaAVX2(TArray<FColor>& PixelData)
{
    const int32 Num = PixelData.Num();
    if (Num == 0) return;

    FColor* Ptr = PixelData.GetData();
    __m256i AlphaMask = _mm256_set1_epi32(0xFF000000);
    __m256i RGBMask = _mm256_set1_epi32(0x00FFFFFF);

    uint32_t* Data = reinterpret_cast<uint32_t*>(Ptr);
    int32 i = 0;

    for (; i + 7 < Num; i += 8)
    {
        __m256i Pixels = _mm256_loadu_si256((const __m256i*)&Data[i]);
        __m256i Result = _mm256_or_si256(_mm256_and_si256(Pixels, RGBMask), AlphaMask);
        _mm256_storeu_si256((__m256i*)&Data[i], Result);
    }

    for (; i < Num; ++i)
    {
        Ptr[i].A = 255;
    }
}
#endif

inline void SetAlpha(TArray<FColor>& PixelData)
{
#if defined(__AVX2__) || defined(__AVX__)
    SetAlphaAVX2(PixelData);
#else
    SetAlphaSSE2(PixelData);
#endif
}
