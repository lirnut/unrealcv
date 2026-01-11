// Weichao Qiu @ 2017
#include "ImageUtil.h"
#include "Runtime/Core/Public/Serialization/BufferArchive.h"
#include "Runtime/ImageWrapper/Public/BmpImageSupport.h"
#include "Runtime/Core/Public/Misc/FileHelper.h"
#include "UnrealcvStats.h"
#include "UnrealcvLog.h"
#include "ExecStatus.h"
#include "Serialization.h"
#include "Containers/ArrayView.h"


DECLARE_CYCLE_STAT(TEXT("FImageUtil::ConvertToPng"), STAT_ConvertToPng, STATGROUP_UnrealCV);
DECLARE_CYCLE_STAT(TEXT("FColorToJpg"), STAT_FColorToJpg, STATGROUP_UnrealCV);
DECLARE_CYCLE_STAT(TEXT("FColorToBmp"), STAT_FColorToBmp, STATGROUP_UnrealCV);
DECLARE_CYCLE_STAT(TEXT("SerializeBmpData"), STAT_SerializeBmp, STATGROUP_UnrealCV);
// DECLARE_CYCLE_STAT(TEXT("SaveFile"), STAT_SaveFile, STATGROUP_UnrealCV);

bool FImageUtil::ConvertToPng(const TArray<FColor>& ImageData, int Width, int Height, TArray<uint8>& PngData)
{
	SCOPE_CYCLE_COUNTER(STAT_ConvertToPng);

	if (ImageData.Num() == 0 || ImageData.Num() != Width * Height)
	{
		return false;
	}

	PngImageWrapper->SetRaw(ImageData.GetData(), ImageData.GetAllocatedSize(), Width, Height, ERGBFormat::BGRA, 8);
	PngData = PngImageWrapper->GetCompressed();
	return true;
}

bool FImageUtil::ConvertToJpg(const TArray<FColor>& ImageData, int Width, int Height, TArray<uint8>& JpgData)
{
	SCOPE_CYCLE_COUNTER(STAT_FColorToJpg);

	if (ImageData.Num() == 0 || ImageData.Num() != Width * Height)
	{
		return false;
	}

	JpgImageWrapper->SetRaw(ImageData.GetData(), ImageData.GetAllocatedSize(), Width, Height, ERGBFormat::BGRA, 8);
	JpgData = JpgImageWrapper->GetCompressed();
	return true;
}

/*
FArchive& operator<<(FArchive& Ar, FBitmapFileHeader& FileHeader)
{
	Ar << FileHeader.bfOffBits << FileHeader.bfReserved1 << FileHeader.bfReserved2 << FileHeader.bfSize << FileHeader.bfType;
	return Ar;
}
*/

bool FImageUtil::ConvertToBmp(const TArray<FColor>& ImageData, int Width, int Height, TArray<uint8>& BmpData)
{
	SCOPE_CYCLE_COUNTER(STAT_FColorToBmp);

	if (ImageData.Num() == 0 || ImageData.Num() != Width * Height)
	{
		return false;
	}

	FBitmapFileHeader BitmapFileHeader;
	// The header field used to identify the BMP and DIB file is 0x42 0x4D in hexadecimal, same as BM in ASCII. The following entries are possible:
	BitmapFileHeader.bfType = 0x4D42; // Check this, big endian or little endian?
	BitmapFileHeader.bfSize = sizeof(FBitmapFileHeader) + sizeof(FBitmapInfoHeader) + Width * Height * 4;
	BitmapFileHeader.bfOffBits = sizeof(FBitmapFileHeader) + sizeof(FBitmapInfoHeader);

	// Check, https://en.wikipedia.org/wiki/BMP_file_format
	FBitmapInfoHeader BitmapInfoHeader;
	BitmapInfoHeader.biSize = sizeof(FBitmapInfoHeader);
	check(BitmapInfoHeader.biSize == 40);
	BitmapInfoHeader.biWidth = Width;
	BitmapInfoHeader.biHeight = -Height; // Use a negative value to make the image up/side down
	// https://stackoverflow.com/questions/4669041/bmp-image-generated-but-displayed-inverted
	BitmapInfoHeader.biPlanes = 1;
	BitmapInfoHeader.biBitCount = 32;
	BitmapInfoHeader.biCompression = 0; // No compression
	BitmapInfoHeader.biSizeImage = 0; // Can be 0
	BitmapInfoHeader.biXPelsPerMeter = 1024; // Is this right?
	BitmapInfoHeader.biYPelsPerMeter = 1024;
	BitmapInfoHeader.biClrUsed = 0; // No color plate
	BitmapInfoHeader.biClrImportant = 0; // No color plate

	// FBufferArchive Writer;
	// Writer << BitmapFileHeader << BitmapInfoHeader;

	// TArray<uint8> Bytes;
	// Bytes.AddUninitialized(Width * Height * 4);
	// {
	// 	SCOPE_CYCLE_COUNTER(STAT_SerializeBmp);
	// 	// Writer << ImageData; // Slow
	// 	// ImageData.BulkSerialize(Writer); // Slow
	// 	FMemory::Memcpy(Bytes.GetData(), ImageData.GetData(), Bytes.Num());
	// 	Writer << Bytes;
	// 	// TArrayView<uint8> ByteView((uint8*)(ImageData.GetData()), 
	// 	// 						ImageData.Num() * sizeof(FColor));
	// 	// Writer << ByteView;
	// }
	// BmpData = Writer;
	// // Writer << BitmapInfoHeader;
	// // Writer << ImageData;

	// 2025, I want make it more efficient, just copy once
    const int32 TotalSize = sizeof(FBitmapFileHeader) + sizeof(FBitmapInfoHeader) + ImageData.Num() * 4;
    BmpData.Empty(TotalSize);
    BmpData.AddUninitialized(TotalSize);
    
    uint8* WritePtr = BmpData.GetData();
    
    FMemory::Memcpy(WritePtr, &BitmapFileHeader, sizeof(FBitmapFileHeader));
    WritePtr += sizeof(FBitmapFileHeader);
    
    FMemory::Memcpy(WritePtr, &BitmapInfoHeader, sizeof(FBitmapInfoHeader));
    WritePtr += sizeof(FBitmapInfoHeader);
    
    FMemory::Memcpy(WritePtr, ImageData.GetData(), ImageData.Num() * 4);


	// JpgImageWrapper->SetRaw(ImageData.GetData(), ImageData.GetAllocatedSize(), Width, Height, ERGBFormat::BGRA, 8);
	// JpgData = JpgImageWrapper->GetCompressed(ImageCompression::Uncompressed);
	return true;
}

bool FImageUtil::SaveFile(const TArray<uint8>& BinaryData, const FString& Filename)
{
	// SCOPE_CYCLE_COUNTER(STAT_SaveFile);

	if (FFileHelper::SaveArrayToFile(BinaryData, *Filename))
	{
		return true;
	}
	else
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("Can not save to file %s"), *Filename);
		return false;
	}
}

EFilenameType ParseFilenameType(const FString& Filename)
{
	bool bIncludeDot = false;
	FString FileExtension = FPaths::GetExtension(Filename);
	FileExtension.ToLowerInline();

	// A hacky way to check whether the input is just a file extension
	int DotIndex;
	if (!Filename.FindChar('.', DotIndex)) FileExtension = Filename;

	if (FileExtension == Filename) // The filename only contains extension, which means the binary mode
	{
		if (FileExtension == TEXT("png")) return EFilenameType::PngBinary;
		if (FileExtension == TEXT("bmp")) return EFilenameType::BmpBinary;
		if (FileExtension == TEXT("npy")) return EFilenameType::NpyBinary;
	}
	else
	{
		if (FileExtension == TEXT("png")) return EFilenameType::Png;
		if (FileExtension == TEXT("bmp")) return EFilenameType::Bmp;
		if (FileExtension == TEXT("npy")) return EFilenameType::Npy;
		if (FileExtension == TEXT("exr")) return EFilenameType::Exr;
	}
	return EFilenameType::Invalid;
}

/** Serialize data according to filename format */
FExecStatus SerializeData(const TArray<FColor>& Data, int Width, int Height, const FString& Filename)
{
	if (Width <= 0 || Height <= 0)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("SerializeData: Invalid dimensions %dx%d, using default 640x480 with black data"), Width, Height);
		return FExecStatus::Error(FString::Printf(TEXT("Invalid dimensions %dx%d"), Width, Height));
	}
	// static FImageUtil ImageUtil;
	FImageUtil ImageUtil; // Multi Thread Safe
	EFilenameType FilenameType = ParseFilenameType(Filename);

	UE_LOG(LogUnrealCV, Warning, TEXT("filename %s"), *Filename);
	TArray<uint8> BinaryData;
	switch (FilenameType)
	{
	case EFilenameType::BmpBinary:
		ImageUtil.ConvertToBmp(Data, Width, Height, BinaryData);
		return FExecStatus::Binary(BinaryData, true);
	case EFilenameType::Bmp:
		ImageUtil.SaveBmpFile(Data, Width, Height, Filename);
		return FExecStatus::OK(Filename);
	case EFilenameType::PngBinary:
		ImageUtil.ConvertToPng(Data, Width, Height, BinaryData);
		return FExecStatus::Binary(BinaryData, true);
	case EFilenameType::Png:
		ImageUtil.SavePngFile(Data, Width, Height, Filename);
		return FExecStatus::OK(Filename);
	}
	return FExecStatus::Error(FString::Printf(TEXT("Invalid filename type, filename %s"), *Filename));
}

FExecStatus SerializeData(const TArray<FFloat16Color>& Data, int Width, int Height, const FString& Filename)
{
	if (Width <= 0 || Height <= 0)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("SerializeData: Invalid dimensions %dx%d, using default 640x480 with black data"), Width, Height);
		return FExecStatus::Error(FString::Printf(TEXT("Invalid dimensions %dx%d"), Width, Height));
	}
	thread_local FImageUtil ImageUtil;
	EFilenameType FilenameType = ParseFilenameType(Filename);

	TArray<uint8> BinaryData;
	int Channel = Data.Num() / (Width * Height);
	switch (FilenameType)
	{
	case EFilenameType::NpyBinary:
		BinaryData = FSerializationUtils::Array2Npy(Data, Width, Height, Channel);
		return FExecStatus::Binary(BinaryData);
	case EFilenameType::Npy:
		BinaryData = FSerializationUtils::Array2Npy(Data, Width, Height, Channel);
		ImageUtil.SaveFile(BinaryData, Filename);
		return FExecStatus::OK(Filename);
	}
	return FExecStatus::Error(FString::Printf(TEXT("Invalid filename type, filename %s"), *Filename));
}

FExecStatus SerializeData(const TArray<float>& Data, int Width, int Height, const FString& Filename)
{
	if (Width <= 0 || Height <= 0)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("SerializeData: Invalid dimensions %dx%d, using default 640x480 with black data"), Width, Height);
		return FExecStatus::Error(FString::Printf(TEXT("Invalid dimensions %dx%d"), Width, Height));
	}
	thread_local FImageUtil ImageUtil;
	EFilenameType FilenameType = ParseFilenameType(Filename);

	TArray<uint8> BinaryData;
	int Channel = Data.Num() / (Width * Height);
	switch (FilenameType)
	{
	case EFilenameType::NpyBinary:
		BinaryData = FSerializationUtils::Array2Npy(Data, Width, Height, Channel);
		return FExecStatus::Binary(BinaryData);
	case EFilenameType::Npy:
		BinaryData = FSerializationUtils::Array2Npy(Data, Width, Height, Channel);
		ImageUtil.SaveFile(BinaryData, Filename);
		return FExecStatus::OK(Filename);
	}
	return FExecStatus::Error(FString::Printf(TEXT("Invalid filename type, filename %s"), *Filename));
}

void ConvertDepthToPNG(
    const TArray<float>& DepthData, 
    TArray<FColor>& Out,
    float GlobalMinDepth,
    float GlobalMaxDepth)
{
    Out.SetNum(DepthData.Num());

    const float DepthRange = FMath::Max(GlobalMaxDepth - GlobalMinDepth, 1e-6f);

    for (int32 i = 0; i < DepthData.Num(); i++)
    {
        float Depth = DepthData[i];

        float Normalized = (Depth - GlobalMinDepth) / DepthRange;

        uint8 Gray = static_cast<uint8>(FMath::Clamp(Normalized, 0.0f, 1.0f) * 255.0f);

        Out[i] = FColor(Gray, Gray, Gray, 255);
    }
}

void ConvertDepthToPNG_RGB24(
    const TArray<float>& DepthData,
    TArray<FColor>& Out,
    float GlobalMinDepth,
    float GlobalMaxDepth)
{
    Out.SetNum(DepthData.Num());

    const float DepthRange = FMath::Max(GlobalMaxDepth - GlobalMinDepth, 1e-6f);

    for (int32 i = 0; i < DepthData.Num(); i++)
    {
        float Depth = DepthData[i];

        float Normalized = (Depth - GlobalMinDepth) / DepthRange;
        Normalized = FMath::Clamp(Normalized, 0.0f, 1.0f);

        // 24-bit depth (0 ~ 16777215)
        uint32 Depth24 = static_cast<uint32>(Normalized * 16777215.0f);

        uint8 R = (Depth24 >> 16) & 0xFF;
        uint8 G = (Depth24 >> 8) & 0xFF;
        uint8 B = Depth24 & 0xFF;

        Out[i] = FColor(R, G, B, 255);
    }
}

float DecodeDepthFromRGB24(FColor C, float GlobalMinDepth, float GlobalMaxDepth)
{
    uint32 Depth24 = (C.R << 16) | (C.G << 8) | C.B;

    float Normalized = Depth24 / 16777215.0f;

    return Normalized * (GlobalMaxDepth - GlobalMinDepth) + GlobalMinDepth;
}


void ConvertDepthToPreview(const TArray<float>& DepthData, TArray<FColor>& OutPreview)
{
	OutPreview.SetNum(DepthData.Num());

	if (DepthData.Num() == 0)
	{
		return;
	}

	// float MinDepth = TNumericLimits<float>::Max();
	// float MaxDepth = TNumericLimits<float>::Min();
	// for (float Depth : DepthData)
	// {
	// 	if (Depth < MinDepth) MinDepth = Depth;
	// 	if (Depth > MaxDepth) MaxDepth = Depth;
	// }
	// if (MaxDepth > 5000.0f)
	// {
	// 	MaxDepth = 5000.0f;
	// }

	float MinDepth = 0.0f;
	float MaxDepth = 10000.0f;

	float DepthRange = MaxDepth - MinDepth;
	if (DepthRange > 0.0f)
	{
		for (int32 i = 0; i < DepthData.Num(); i++)
		{
			uint8 NormalizedDepth = static_cast<uint8>(FMath::Clamp((DepthData[i] - MinDepth) / DepthRange, 0.0f, 1.0f) * 255.0f);
			OutPreview[i] = FColor(NormalizedDepth, NormalizedDepth, NormalizedDepth, 255);
		}
	}
	else
	{
		for (int32 i = 0; i < DepthData.Num(); i++)
		{
			OutPreview[i] = FColor(0, 0, 0, 255);
		}
	}
}

