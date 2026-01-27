#include "ImageSerializer.h"
#include "Runtime/ImageWrapper/Public/IImageWrapper.h"
#include "Runtime/ImageWrapper/Public/IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "Misc/FileHelper.h"
#include "Async/Async.h"

namespace UnrealCV
{
namespace RenderUtil
{

TFuture<bool> SaveImageData(
    const TArray<FColor>& PixelData,
    int32 Width, int32 Height,
    const FString& Path)
{
    TPromise<bool> Promise;
    TFuture<bool> Future = Promise.GetFuture();

    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [
        PixelData, Width, Height, Path, Promise = MoveTemp(Promise)
    ]() mutable
    {
        IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
        TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

        if (!ImageWrapper.IsValid())
        {
            Promise.SetValue(false);
            return;
        }

        ImageWrapper->SetRaw(PixelData.GetData(), PixelData.GetAllocatedSize(), Width, Height, ERGBFormat::BGRA, 8);
        const TArray64<uint8>& CompressedData = ImageWrapper->GetCompressed();

        bool bSuccess = FFileHelper::SaveArrayToFile(CompressedData, *Path);
        Promise.SetValue(bSuccess);
    });

    return Future;
}

}
}

