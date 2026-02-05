#include "PanoramicBPLib.h"
#include "Sensor/CameraSensor/PanoramicCamSensor.h"
#include "Engine/World.h"
#include "UnrealcvLog.h"

UPanoramicCamSensor* UPanoramicBPLib::SpawnPanoramicCamera(
	UObject* WorldContextObject,
	FVector Location,
	int32 CubemapResolution)
{
	if (!IsValid(WorldContextObject))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("PanoramicBPLib::SpawnPanoramicCamera: Invalid WorldContextObject"));
		return nullptr;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!IsValid(World))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("PanoramicBPLib::SpawnPanoramicCamera: Invalid World"));
		return nullptr;
	}

	AActor* PanoActor = World->SpawnActor<AActor>(AActor::StaticClass(), Location, FRotator::ZeroRotator);
	if (!IsValid(PanoActor))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("PanoramicBPLib::SpawnPanoramicCamera: Failed to spawn actor"));
		return nullptr;
	}

	UPanoramicCamSensor* PanoSensor = NewObject<UPanoramicCamSensor>(PanoActor);
	if (!IsValid(PanoSensor))
	{
		PanoActor->Destroy();
		UE_LOG(LogUnrealCV, Error, TEXT("PanoramicBPLib::SpawnPanoramicCamera: Failed to create sensor"));
		return nullptr;
	}

	PanoSensor->RegisterComponent();
	PanoSensor->SetCubemapResolution(CubemapResolution);
	PanoSensor->InitCubemapTarget(CubemapResolution);
	PanoActor->SetRootComponent(PanoSensor);

	UE_LOG(LogUnrealCV, Log, TEXT("PanoramicBPLib::SpawnPanoramicCamera: Created at %s with resolution %d"),
		*Location.ToString(), CubemapResolution);

	return PanoSensor;
}

void UPanoramicBPLib::CapturePanoramicToFile(
	UPanoramicCamSensor* PanoramicSensor,
	const FString& OutputPath,
	int32 EquirectWidth,
	int32 EquirectHeight)
{
	if (!IsValid(PanoramicSensor))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("PanoramicBPLib::CapturePanoramicToFile: Invalid sensor"));
		return;
	}

	PanoramicSensor->CaptureEquirectangularToFile(OutputPath, EquirectWidth, EquirectHeight);
}

void UPanoramicBPLib::GetPanoramicPixelData(
	UPanoramicCamSensor* PanoramicSensor,
	TArray<FColor>& OutPixelData,
	int32& OutWidth,
	int32& OutHeight,
	int32 EquirectWidth,
	int32 EquirectHeight)
{
	if (!IsValid(PanoramicSensor))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("PanoramicBPLib::GetPanoramicPixelData: Invalid sensor"));
		OutPixelData.Empty();
		OutWidth = 0;
		OutHeight = 0;
		return;
	}

	PanoramicSensor->CaptureEquirectangular(OutPixelData, OutWidth, OutHeight, EquirectWidth, EquirectHeight);
}

void UPanoramicBPLib::DestroyPanoramicCamera(UPanoramicCamSensor* PanoramicSensor)
{
	if (!IsValid(PanoramicSensor))
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("PanoramicBPLib::DestroyPanoramicCamera: Invalid sensor"));
		return;
	}

	AActor* Owner = PanoramicSensor->GetOwner();
	if (IsValid(Owner))
	{
		Owner->Destroy();
		UE_LOG(LogUnrealCV, Log, TEXT("PanoramicBPLib::DestroyPanoramicCamera: Destroyed panoramic camera"));
	}
}
