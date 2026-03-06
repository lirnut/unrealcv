// Weichao Qiu @ 2017
#include "Controller/ObjectAnnotator.h"
#include "Controller/IAnnotatorImpl.h"
#include "Controller/ProxyAnnotator.h"
#include "Controller/DirectAnnotator.h"
#include "UnrealcvServer.h"
#include "UnrealcvLog.h"

TSharedPtr<IAnnotatorImpl> FObjectAnnotator::Implementation = nullptr;
bool FObjectAnnotator::bUseDirectAnnotation = false;
FColorGenerator FObjectAnnotator::ColorGenerator;

void FObjectAnnotator::Initialize()
{
	bool bUseDirect = FUnrealcvServer::Get().Config.UseDirectAnnotation;
	SetAnnotationMode(bUseDirect);

	UE_LOG(LogUnrealCV, Warning, TEXT("FObjectAnnotator initialized with mode: %s"),
		bUseDirectAnnotation ? TEXT("DirectAnnotation") : TEXT("ProxyAnnotation"));
}

void FObjectAnnotator::Shutdown()
{
	if (Implementation.IsValid())
	{
		Implementation.Reset();
	}
}

void FObjectAnnotator::SetAnnotationMode(bool bUseDirect)
{
	if (bUseDirect != bUseDirectAnnotation || !Implementation.IsValid())
	{
		bUseDirectAnnotation = bUseDirect;

		if (bUseDirectAnnotation)
		{
			Implementation = MakeShared<FDirectAnnotator>();
			UE_LOG(LogUnrealCV, Log, TEXT("Switched to DirectAnnotation mode"));
		}
		else
		{
			Implementation = MakeShared<FProxyAnnotator>();
			UE_LOG(LogUnrealCV, Log, TEXT("Switched to ProxyAnnotation mode"));
		}
	}
}

bool FObjectAnnotator::IsUsingDirectAnnotation()
{
	return bUseDirectAnnotation;
}

void FObjectAnnotator::AnnotateWorld(UWorld* World)
{
	if (!Implementation.IsValid())
	{
		Initialize();
	}

	if (Implementation.IsValid())
	{
		Implementation->AnnotateWorld(World);
	}
	else
	{
		UE_LOG(LogUnrealCV, Error, TEXT("FObjectAnnotator::AnnotateWorld - Implementation is not valid"));
	}
}

void FObjectAnnotator::DeannotateWorld(UWorld* World)
{
	if (Implementation.IsValid())
	{
		Implementation->DeannotateWorld(World);
	}
	else
	{
		UE_LOG(LogUnrealCV, Error, TEXT("FObjectAnnotator::DeannotateWorld - Implementation is not valid"));
	}
}

int32 FObjectAnnotator::SetAnnotationColor(AActor* Actor, const FColor& AnnotationColor)
{
	if (!Implementation.IsValid())
	{
		Initialize();
	}

	if (Implementation.IsValid())
	{
		return Implementation->SetAnnotationColor(Actor, AnnotationColor);
	}
	else
	{
		UE_LOG(LogUnrealCV, Error, TEXT("FObjectAnnotator::SetAnnotationColor - Implementation is not valid"));
		return 0;
	}
}

void FObjectAnnotator::GetAnnotationColor(AActor* Actor, FColor& AnnotationColor)
{
	if (Implementation.IsValid())
	{
		Implementation->GetAnnotationColor(Actor, AnnotationColor);
	}
	else
	{
		UE_LOG(LogUnrealCV, Error, TEXT("FObjectAnnotator::GetAnnotationColor - Implementation is not valid"));
	}
}

TMap<FString, FColor> FObjectAnnotator::GetAnnotationColors()
{
	if (Implementation.IsValid())
	{
		return Implementation->GetAnnotationColors();
	}
	else
	{
		UE_LOG(LogUnrealCV, Error, TEXT("FObjectAnnotator::GetAnnotationColors - Implementation is not valid"));
		return TMap<FString, FColor>();
	}
}

int32 FColorGenerator::GetChannelValue(uint32 Index)
{
	static int32 Values[256] = { 0 };
	static bool Init = false;
	if (!Init)
	{
		float Step = 256;
		uint32 Iter = 0;
		Values[0] = 0;
		while (Step >= 1)
		{
			for (uint32 Value = Step - 1; Value <= 256; Value += Step * 2)
			{
				Iter++;
				Values[Iter] = Value;
			}
			Step /= 2;
		}
		Init = true;
	}
	if (Index >= 0 && Index <= 255)
	{
		return Values[Index];
	}
	else
	{
		UE_LOG(LogUnrealCV, Error, TEXT("Invalid channel index"));
		check(false);
		return -1;
	}
}

void FColorGenerator::GetColors(int32 MaxVal, bool Fix1, bool Fix2, bool Fix3, TArray<FColor>& ColorMap)
{
	for (int32 I = 0; I <= (Fix1 ? 0 : MaxVal - 1); I++)
	{
		for (int32 J = 0; J <= (Fix2 ? 0 : MaxVal - 1); J++)
		{
			for (int32 K = 0; K <= (Fix3 ? 0 : MaxVal - 1); K++)
			{
				uint8 R = (uint8)GetChannelValue(Fix1 ? MaxVal : I);
				uint8 G = (uint8)GetChannelValue(Fix2 ? MaxVal : J);
				uint8 B = (uint8)GetChannelValue(Fix3 ? MaxVal : K);
				FColor Color(R, G, B, 255);
				ColorMap.Add(Color);
			}
		}
	}
}

// FColor FColorGenerator::GetColorFromColorMap(int32 ObjectIndex)
// {
// 	static TArray<FColor> ColorMap;
// 	int NumPerChannel = 32;
// 	if (ColorMap.Num() == 0)
// 	{
// 		// 32 ^ 3
// 		for (int32 MaxChannelIndex = 0; MaxChannelIndex < NumPerChannel; MaxChannelIndex++) // Get color map for 1000 objects
// 		{
// 			// GetColors(MaxChannelIndex, false, false, false, ColorMap);
// 			GetColors(MaxChannelIndex, false, false, true, ColorMap);
// 			GetColors(MaxChannelIndex, false, true, false, ColorMap);
// 			GetColors(MaxChannelIndex, false, true, true, ColorMap);
// 			GetColors(MaxChannelIndex, true, false, false, ColorMap);
// 			GetColors(MaxChannelIndex, true, false, true, ColorMap);
// 			GetColors(MaxChannelIndex, true, true, false, ColorMap);
// 			GetColors(MaxChannelIndex, true, true, true, ColorMap);
// 		}
// 	}
// 	if (ObjectIndex < 0 || ObjectIndex >= pow(NumPerChannel, 3))
// 	{
// 		UE_LOG(LogUnrealCV, Error, TEXT("Object index %d is out of the color map boundary [%d, %d]"), ObjectIndex, 0, (int) pow(NumPerChannel, 3));
// 	}
// 	return ColorMap[ObjectIndex];
// }
// FColor FColorGenerator::GetColorFromColorMap(int32 ObjectIndex)
// {
//     constexpr int32 NumPerChannel = 50;
//     constexpr int32 MaxIndex = NumPerChannel * NumPerChannel * NumPerChannel;

//     if (ObjectIndex < 0 || ObjectIndex >= MaxIndex)
//     {
//         UE_LOG(LogUnrealCV, Error,
//             TEXT("Object index %d is out of the color map boundary [%d, %d]"),
//             ObjectIndex, 0, MaxIndex - 1);
//         return FColor::Black;
//     }

//     int32 Layer = ObjectIndex / (NumPerChannel * NumPerChannel); // MaxChannelIndex
//     int32 Rem   = ObjectIndex % (NumPerChannel * NumPerChannel);

//     int32 Sub   = Rem / NumPerChannel;
//     int32 Base  = Rem % NumPerChannel;

//     static const bool FlipTable[7][3] =
//     {
//         { false, false, true  },
//         { false, true,  false },
//         { false, true,  true  },
//         { true,  false, false },
//         { true,  false, true  },
//         { true,  true,  false },
//         { true,  true,  true  },
//     };

//     const bool* Flip = FlipTable[Sub % 7];

//     auto Apply = [](int32 v, bool flip)
//     {
//         return flip ? (NumPerChannel - 1 - v) : v;
//     };

//     uint8 R = Apply(Layer, Flip[0]) * 255 / (NumPerChannel - 1);
//     uint8 G = Apply(Layer, Flip[1]) * 255 / (NumPerChannel - 1);
//     uint8 B = Apply(Base,  Flip[2]) * 255 / (NumPerChannel - 1);

//     return FColor(R, G, B, 255);
// }
FColor FColorGenerator::GetColorFromColorMap(int32 ObjectIndex)
{
    static TArray<FColor> ColorMap;
    constexpr int32 NumPerChannel = 32;

    if (ObjectIndex < 0 || ObjectIndex >= NumPerChannel * NumPerChannel * NumPerChannel)
    {
        UE_LOG(LogUnrealCV, Error, TEXT("Object index %d out of range"), ObjectIndex);
        return FColor::Black;
    }

    // Lazy build
    static int32 BuiltMaxChannel = 0;

    while (ColorMap.Num() <= ObjectIndex && BuiltMaxChannel < NumPerChannel)
    {
        int32 MaxVal = BuiltMaxChannel;
        int32 PrevNum = ColorMap.Num();

        GetColors(MaxVal, false, false, true,  ColorMap);
        GetColors(MaxVal, false, true,  false, ColorMap);
        GetColors(MaxVal, false, true,  true,  ColorMap);
        GetColors(MaxVal, true,  false, false, ColorMap);
        GetColors(MaxVal, true,  false, true,  ColorMap);
        GetColors(MaxVal, true,  true,  false, ColorMap);
        GetColors(MaxVal, true,  true,  true,  ColorMap);

        // Remove white and black colors from newly added entries
        for (int32 i = ColorMap.Num() - 1; i >= PrevNum; --i)
        {
            if (ColorMap[i] == FColor::White || ColorMap[i] == FColor::Black)
            {
                ColorMap.RemoveAt(i);
            }
        }

        BuiltMaxChannel++;
    }

    if (ObjectIndex >= ColorMap.Num())
    {
        UE_LOG(LogUnrealCV, Warning, TEXT("ObjectIndex %d exceeds available non-white colors %d, returning black"), ObjectIndex, ColorMap.Num());
        return FColor::Black;
    }

    return ColorMap[ObjectIndex];
}
