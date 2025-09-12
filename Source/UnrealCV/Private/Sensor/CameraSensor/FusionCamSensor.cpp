// Weichao Qiu @ 2018
#include "FusionCamSensor.h"
#include "Runtime/Engine/Classes/Camera/CameraComponent.h"
#include "ImageUtil.h"
#include "Serialization.h"
#include "UnrealcvLog.h"
#include "UnrealcvServer.h"

// Sensors included in FusionSensor
#include "LitCamSensor.h"
#include "DepthCamSensor.h"
#include "NormalCamSensor.h"
#include "AnnotationCamSensor.h"

#include "Utils/UObjectUtils.h"
#include "Component/AnnotationComponent.h"
#include "SL.h"
#include "Utils/ImageUtil.h"
#include "Controller/ActorController.h"
#include "SensorBPLib.h"
#include "AudioDevice.h"
#include "AudioMixerDevice.h"
#include "Sound/SoundWave.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include <Serialization/BufferArchive.h>

static void CollectShowOnlyForActor(
    AActor* Actor, UWorld* World,
    TArray<TWeakObjectPtr<UPrimitiveComponent>>& OutComponents)
{
    OutComponents.Reset();
    if (!IsValid(World) || !IsValid(Actor)) return;

    {
        TArray<UAnnotationComponent*> AnnotationComps;
        Actor->GetComponents<UAnnotationComponent>(AnnotationComps, /*bIncludeFromChildActors*/ true);

        for (UAnnotationComponent* C : AnnotationComps)
        {
            if (IsValid(C) && C->IsRegistered() && C->GetWorld() == World)
            {
                OutComponents.Add(C);
            }
        }
    }
}

UFusionCamSensor::UFusionCamSensor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FString ComponentName;
	ComponentName = FString::Printf(TEXT("%s_%s"), *this->GetName(), TEXT("PreviewCamera"));
	PreviewCamera = CreateDefaultSubobject<UCameraComponent>(*ComponentName);
	PreviewCamera->SetupAttachment(this);

	ComponentName = FString::Printf(TEXT("%s_%s"), *this->GetName(), TEXT("DepthCamSensor"));
	DepthCamSensor = CreateDefaultSubobject<UDepthCamSensor>(*ComponentName);
	FusionSensors.Add(DepthCamSensor);

	ComponentName = FString::Printf(TEXT("%s_%s"), *this->GetName(), TEXT("NormalCamSensor"));
	NormalCamSensor = CreateDefaultSubobject<UNormalCamSensor>(*ComponentName);
	FusionSensors.Add(NormalCamSensor);

	ComponentName = FString::Printf(TEXT("%s_%s"), *this->GetName(), TEXT("AnnotationCamSensor"));
	AnnotationCamSensor = CreateDefaultSubobject<UAnnotationCamSensor>(*ComponentName);
	FusionSensors.Add(AnnotationCamSensor);

	ComponentName = FString::Printf(TEXT("%s_%s"), *this->GetName(), TEXT("LitCamSensor"));
	LitCamSensor = CreateDefaultSubobject<ULitCamSensor>(*ComponentName);
	FusionSensors.Add(LitCamSensor);

	// The config loading code should not be placed into the ctor, otherwise it will break the copy behavior
	FServerConfig& Config = FUnrealcvServer::Get().Config;
	FilmWidth = Config.Width == 0 ? 640 : Config.Width;
	FilmHeight = Config.Height == 0 ? 480 : Config.Height;
	FOV = Config.FOV == 0 ? 90 : Config.FOV; 
	// Note: If FOV == 0, the render will give FMod assert error.
	// Need to call update functions after copy operator (in BeginPlay), here just sets value

	for (UBaseCameraSensor* Sensor : FusionSensors)
	{
		if (IsValid(Sensor))
		{
			Sensor->SetupAttachment(this);
		}
		else
		{
			UE_LOG(LogUnrealCV, Warning, TEXT("Invalid sensor is found in the ctor of FusionCamSensor"));
		}
	}
	// SetFilmSize(FilmWidth, FilmHeight); // This should not not be done in CTOR.
}

void UFusionCamSensor::BeginPlay()
{
	Super::BeginPlay();

	SetFilmSize(FilmWidth, FilmHeight);
	SetSensorFOV(FOV);
}

// void UFusionCamSensor::OnRegister()
// {
// 	Super::OnRegister();

// 	for (UBaseCameraSensor* Sensor : FusionSensors)
// 	{
// 		if (IsValid(Sensor))
// 		{
// 			Sensor->RegisterComponent();
// 		}
// 		else
// 		{
// 			UE_LOG(LogUnrealCV, Warning, TEXT("Invalid sensor is found in the OnRegister of FusionCamSensor"));
// 		}
// 	}
// }

bool UFusionCamSensor::GetEditorPreviewInfo(float DeltaTime, FMinimalViewInfo& ViewOut)
{
	// From CameraComponent
	if (this->IsActive())
	{
		this->LitCamSensor->GetCameraView(DeltaTime, ViewOut);
		return true;
	}
	else
	{
		return false;
	}
}


void UFusionCamSensor::StartRecord(const FString& FileName, float Duration, int32 FPS, AActor* Target)
{
    if (bIsRecording)
    {
        StopRecord();
    }

    AWorldSettings* WorldSettings = FUnrealcvServer::Get().GetWorld()->GetWorldSettings();
    // WorldSettings->SetTimeDilation(0.2f);
	TimeDilation = WorldSettings->TimeDilation;

    RecordFileName = FileName;
    RecordDuration = Duration;
    RecordFPS = FPS;
    TimePerFrame = 1.0f / FPS;
    ElapsedTime = 0.0f;
	ElapsedSteps = 0;
    bIsRecording = true;
	TargetToHide = Target;
	BulletTimeState = EBulletTimeState::Waiting;

    OnTimerRecord();

    GetWorld()->GetTimerManager().SetTimer(
        TimerHandle_Record,
        this,
        &UFusionCamSensor::OnTimerRecord,
        TimePerFrame,
        true
    );

	StartCameraAudioRecord();
}

void UFusionCamSensor::StartBulletTimeRecord(const FString& FileName, float Duration, int32 FPS, AActor* Target)
{
	UseBulletTime = true;
	if (Target == nullptr) 
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("Target is null in StartBulletTimeRecord"));
		SL::get().printf("Target is null in StartBulletTimeRecord");
		return;
	}
	StartRecord(FileName, Duration, FPS, Target);
}

void UFusionCamSensor::StopRecord()
{
    if (bIsRecording)
    {
		StopCameraAudioRecord();
        GetWorld()->GetTimerManager().ClearTimer(TimerHandle_Record);
        bIsRecording = false;
		TargetToHide = nullptr;
		UseBulletTime = false;
		BulletTimeState = EBulletTimeState::Waiting;

		// AWorldSettings* WorldSettings = FUnrealcvServer::Get().GetWorld()->GetWorldSettings();
		// WorldSettings->SetTimeDilation(1.0f);
    }
}

void UFusionCamSensor::GetBulletTime(TArray<TArray<FColor>> *DataLit, TArray<TArray<FColor>> *DataSeg,  int& InOutWidth, int& InOutHeight)
{
	FVector TargetLocation = TargetToHide->GetActorLocation();  // [x,y,z]
	FRotator TargetRotation = TargetToHide->GetActorRotation();  // [pitch,yaw,roll]

	FVector CamLocationSaver; FRotator CamRotationSaver;
	FVector CamLocation = GetSensorLocation(); CamLocationSaver = CamLocation;
	FRotator CamRotation = GetSensorRotation(); CamRotationSaver = CamRotation;

	// Pause the world to start bullet time
	SL::get().printf("BulletTimeState: SetPause(true)");
	this->GetWorld()->GetFirstPlayerController()->SetPause(true);



	// SetSensorLocation(); and SetSensorRotation(); can be used to face the cam to TargetToHide and turn the camera 360 deg
	// deg/frame is BulletTimeSpeedDeg;
	// done deg is BulletTimeElapsedDeg; from 0 deg to 360 deg
	float BulletTimeElapsedDeg = 0.0f;

	FVector Offset = CamLocation - TargetLocation;
	FVector BulletTimeOffset = Offset;  // 保存相对偏移
	FVector BulletTimeTargetLocation = TargetLocation;
	
	FRotator InitRotation = CamRotation - (BulletTimeTargetLocation - CamLocation).Rotation();

	// int index = 0;
	while (BulletTimeElapsedDeg + BulletTimeSpeedDeg <= 360.0f) {
		BulletTimeElapsedDeg += BulletTimeSpeedDeg;
		{  // transform fixed
			FQuat RotationQuat = FQuat(FVector::UpVector, FMath::DegreesToRadians(BulletTimeSpeedDeg));
			BulletTimeOffset = RotationQuat.RotateVector(BulletTimeOffset);
			FVector NewCameraLocation = BulletTimeTargetLocation + BulletTimeOffset;
			SetSensorLocation(NewCameraLocation);
			// to face the target
			FRotator NewRotation = (BulletTimeTargetLocation - NewCameraLocation).Rotation() + InitRotation;
			SetSensorRotation(NewRotation);
		}

		if (DataSeg != nullptr){
			TArray<FColor> DataRGB, DataM;
			GetLitSeg(DataRGB, DataM, InOutWidth, InOutHeight);

			if (DataLit != nullptr) { DataLit->Add(DataRGB); }
			if (DataSeg != nullptr) { DataSeg->Add(DataM); }
		} else {
			TArray<FColor> DataRGB;
			GetLit(DataRGB, InOutWidth, InOutHeight);

			if (DataLit != nullptr) { DataLit->Add(DataRGB); }
		}
	}

	SL::get().printf("BulletTimeState: SetPause(false)");
	this->GetWorld()->GetFirstPlayerController()->SetPause(false);

	// reset the cam
	SetSensorLocation(CamLocationSaver);
	SetSensorRotation(CamRotationSaver);
}

void UFusionCamSensor::OnTimerRecord()
{
	// FScopeLock ScopeLock(&RecordCriticalSection);

    if (ElapsedTime >= RecordDuration)
    {
        StopRecord();
        return;
    }
    AWorldSettings* WorldSettings = FUnrealcvServer::Get().GetWorld()->GetWorldSettings();
	if (std::abs(TimeDilation - WorldSettings->TimeDilation) < 0.05) {
		WorldSettings->SetTimeDilation(TimeDilation);
	}
	else if (TimeDilation != WorldSettings->TimeDilation) {
		WorldSettings->SetTimeDilation((0.2*TimeDilation + 0.8*WorldSettings->TimeDilation));
	}

	int32 index;
	if (!RecordFileName.FindLastChar(TEXT('.'), index)) {
		index = RecordFileName.Len();
	}

	if (UseBulletTime && (BulletTimeState == EBulletTimeState::Waiting) && (ElapsedTime > (RecordDuration/2))) {
    	WorldSettings->SetTimeDilation(0.1f);
		TArray<TArray<FColor>> DataRGB, DataM;
		int Width, Height;
		GetBulletTime(&DataRGB, &DataM, Width, Height);	

		TArray<TArray<FColor>> DataRGBNoTarget;
		FActorController TargetController(TargetToHide);
		TargetController.Hide();
		GetBulletTime(&DataRGBNoTarget, nullptr, Width, Height);	
		TargetController.Show();

		for (int i = 0; i < DataRGB.Num(); i++) {
			FString FileNameRGB = RecordFileName; FileNameRGB.InsertAt(index, FString::Printf(TEXT("%d_rgb"), ElapsedSteps));
			FString FileNameM = RecordFileName; FileNameM.InsertAt(index, FString::Printf(TEXT("%d_mask"), ElapsedSteps));
			FString FileNameRGBNoTarget = RecordFileName; FileNameRGBNoTarget.InsertAt(index, FString::Printf(TEXT("%d_rgb_no_target"), ElapsedSteps));
			SerializeData(DataRGB[i], Width, Height, FileNameRGB);
			SerializeData(DataM[i], Width, Height, FileNameM);
			SerializeData(DataRGBNoTarget[i], Width, Height, FileNameRGBNoTarget);
			ElapsedSteps++;
		}

 	    // ElapsedTime += TimePerFrame;
		BulletTimeState = EBulletTimeState::Finished;
    	// WorldSettings->SetTimeDilation(OldTimeDilation);
		return;
	}

	{
		if (TargetToHide) {
			TArray<FColor> DataRGBNoTarget;
			FActorController TargetController(TargetToHide);
			TargetController.Show();
		}

		TArray<FColor> DataRGB, DataM;
		int Width, Height;
		FString FileNameRGB = RecordFileName; FileNameRGB.InsertAt(index, FString::Printf(TEXT("%d_rgb"), ElapsedSteps));
		FString FileNameM = RecordFileName; FileNameM.InsertAt(index, FString::Printf(TEXT("%d_mask"), ElapsedSteps));

		GetLitSeg(DataRGB, DataM, Width, Height);
		SerializeData(DataRGB, Width, Height, FileNameRGB);
		SerializeData(DataM, Width, Height, FileNameM);

	}

	if (TargetToHide) {
		TArray<FColor> DataRGBNoTarget;
		FString FileNameRGBNoTarget = RecordFileName; FileNameRGBNoTarget.InsertAt(index, FString::Printf(TEXT("%d_rgb_no_target"), ElapsedSteps));

		int Width, Height;
		FActorController TargetController(TargetToHide);
		TargetController.Hide();
		GetLit(DataRGBNoTarget, Width, Height);
		TargetController.Show();
		SerializeData(DataRGBNoTarget, Width, Height, FileNameRGBNoTarget);
	}


 	ElapsedTime += TimePerFrame;
	ElapsedSteps++;
}


Audio::FMixerDevice* UFusionCamSensor::GetAudioMixer()
{
	SL::get().print("UFusionCamSensor::GetAudioMixer called");

	UWorld* World = FUnrealcvServer::Get().GetWorld();
	FVector CamLocation = GetSensorLocation();
	FRotator CamRotation = GetSensorRotation();

	FAudioDevice* AudioDevice = World->GetAudioDeviceRaw();
	if (!AudioDevice) {
		return nullptr;
	}

	FTransform ListenerTransform(CamRotation, CamLocation);
	AudioDevice->SetListener(
		World,
		0,
		ListenerTransform,
		0.0f // DeltaTime
	);

	Audio::FMixerDevice* MixerDevice = static_cast<Audio::FMixerDevice*>(AudioDevice);
	if (!MixerDevice) {
		return nullptr;
	}
	SL::get().print("UFusionCamSensor::GetAudioMixer returned");
	return MixerDevice;
}


void UFusionCamSensor::StartCameraAudioRecord()
{
	SL::get().print("UFusionCamSensor::StartCameraAudioRecord called");
	Audio::FMixerDevice* MixerDevice = GetAudioMixer();
	if (MixerDevice == nullptr) {
		SL::get().print("Error: GetAudioMixer() failed");
		return;
	}
	FString target_name = TEXT("");
	// USoundSubmix* TargetSubmix = nullptr;
	// if (TargetToHide) {
	// 	TargetSubmix = NewObject<USoundSubmix>(USoundSubmix::StaticClass());
	// }

	MixerDevice->StartRecording(nullptr, 100.0f);
	SL::get().print("FCameraHandler::StartCameraAudioRecord returned");
}

void UFusionCamSensor::StopCameraAudioRecord()
{
	SL::get().print("UFusionCamSensor::StopCameraAudioRecord called");
	Audio::FMixerDevice* MixerDevice = GetAudioMixer();
	if (MixerDevice == nullptr) {
		SL::get().print("Error: GetAudioMixer() failed");
		return;
	}

	float NumChannels = 1.0f;
	float SampleRate = 44.1 * 1000;
	Audio::FAlignedFloatBuffer& RecordedBuffer = MixerDevice->StopRecording(nullptr, NumChannels, SampleRate);

	TArray<int16> PCM16Data;
	PCM16Data.Reserve(RecordedBuffer.Num());

	for (float Sample : RecordedBuffer)
	{
		// Clamp 到 [-1.0, 1.0] 再转为 int16
		float Clamped = FMath::Clamp(Sample, -1.0f, 1.0f);
		PCM16Data.Add((int16)(Clamped * 32767.0f));
	}

	// 写 WAV 文件头 + 数据
	int32 index;
	if (!RecordFileName.FindLastChar(TEXT('.'), index)) {
		index = RecordFileName.Len();
	}
	SL::get().printf("RecordFileName: %s", TCHAR_TO_UTF8(*RecordFileName));
	FString WavFileName = RecordFileName;
	WavFileName.RemoveAt(index, RecordFileName.Len() - index);
	WavFileName += TEXT("audio.wav");
	SL::get().printf("WavFileName: %s", TCHAR_TO_UTF8(*WavFileName));
	FBufferArchive WaveData;

	int32 NumSamples = PCM16Data.Num();
	int32 NumBytes = NumSamples * sizeof(int16);

	// 写 WAV Header (PCM 16-bit, NumChannels, SampleRate)
	WaveData.Serialize((void*)"RIFF", 4);
	int32 ChunkSize = 36 + NumBytes;
	WaveData << ChunkSize;
	WaveData.Serialize((void*)"WAVE", 4);

	// fmt chunk
	WaveData.Serialize((void*)"fmt ", 4);
	int32 SubChunk1Size = 16;
	WaveData << SubChunk1Size;
	int16 AudioFormat = 1; // PCM
	WaveData << AudioFormat;
	int16 Channels = (int16)NumChannels;
	WaveData << Channels;
	int32 SR = (int32)SampleRate;
	WaveData << SR;
	int32 ByteRate = SR * Channels * sizeof(int16);
	WaveData << ByteRate;
	int16 BlockAlign = Channels * sizeof(int16);
	WaveData << BlockAlign;
	int16 BitsPerSample = 16;
	WaveData << BitsPerSample;

	// data chunk
	WaveData.Serialize((void*)"data", 4);
	WaveData << NumBytes;
	WaveData.Serialize(PCM16Data.GetData(), NumBytes);

	// 保存到文件
	FFileHelper::SaveArrayToFile(WaveData, *WavFileName);
	WaveData.FlushCache();
	WaveData.Empty();

	SL::get().print("FCameraHandler::StopCameraAudioRecord returned");
}


void UFusionCamSensor::GetLitSeg(TArray<FColor>& DataRGB, TArray<FColor>& DataSeg, int& InOutWidth, int& InOutHeight)
{
	if (LitCamSensor->CheckTextureTarget()) {
		LitCamSensor->InitTextureTarget(this->FilmWidth, this->FilmHeight);
		if (!LitCamSensor->CheckTextureTarget()) {
			SL::get().print("LitCamSensor InitTextureTarget failed.");
			UE_LOG(LogUnrealCV, Error, TEXT("No TextureTarget."));
			return;
		}
	}
	if (!AnnotationCamSensor->CheckTextureTarget()) {
		AnnotationCamSensor->InitTextureTarget(this->FilmWidth, this->FilmHeight);
		if (!AnnotationCamSensor->CheckTextureTarget()) {
			SL::get().print("AnnotationCamSensor InitTextureTarget failed.");
			UE_LOG(LogUnrealCV, Error, TEXT("No TextureTarget."));
			return;
		}
	}

	LitCamSensor->CaptureScene();

	TArray<TWeakObjectPtr<UPrimitiveComponent>> ComponentList;
	AnnotationCamSensor->GetAnnotationComponents(this->GetWorld(), ComponentList);
	AnnotationCamSensor->ShowOnlyComponents = ComponentList;
	AnnotationCamSensor->CaptureScene();

	LitCamSensor->ReadCaptureResults(DataRGB);
	AnnotationCamSensor->ReadCaptureResults(DataSeg);

	int32 LitW = LitCamSensor->GetFilmWidth();
	int32 LitH = LitCamSensor->GetFilmHeight();
	int32 SegW = AnnotationCamSensor->GetFilmWidth();
	int32 SegH = AnnotationCamSensor->GetFilmHeight();

	// SL::get().printf("UFusionCamSensor::GetLitSeg DataRGB size: %d, width: %d, height: %d", DataRGB.Num(), LitW, LitH);
	// SL::get().printf("UFusionCamSensor::GetLitSeg DataSeg size: %d, width: %d, height: %d", DataSeg.Num(), SegW, SegH);

	if (!((LitW == SegW) && (LitH == SegH)))
	{
		SL::get().print("ERROR: Rendered frame size does not match.");
		UE_LOG(LogUnrealCV, Error, TEXT("Rendered frame size does not match."));
		DataRGB.Empty();
		DataSeg.Empty();
		return;
	}
	InOutWidth = LitW;
	InOutHeight = LitH;
}


void UFusionCamSensor::GetObjMask(FString ObjId, TArray<FColor>& Data, int& InOutWidth, int& InOutHeight)
{
	SL::get().print("GetObjMask called");

	AActor* Actor = GetActorById(FUnrealcvServer::Get().GetWorld(), ObjId);
	if (!Actor) {UE_LOG(LogUnrealCV, Error, TEXT("Can not find object")); return;}
	
	TArray<TWeakObjectPtr<UPrimitiveComponent>> ComponentList;
	CollectShowOnlyForActor(Actor, FUnrealcvServer::Get().GetWorld(), ComponentList);
	SL::get().printf("ComponentList Num: %d", ComponentList.Num());

	auto* CamSensor = this->AnnotationCamSensor;
	// auto* CamSensor = this->LitCamSensor;
	CamSensor->ShowOnlyComponents = ComponentList;
	CamSensor->CaptureScene();
	CamSensor->ReadCaptureResults(Data);
	InOutWidth = CamSensor->GetFilmWidth();
	InOutHeight = CamSensor->GetFilmHeight();
	if (Data.Num() == 0) 
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("Captured obj mask data is empty."));
		return;
	}
	SL::get().print("GetObjMask returned");
}

void UFusionCamSensor::GetLit(TArray<FColor>& LitData, int& Width, int& Height, ELitMode LitMode)
{
	this->LitCamSensor->CaptureLit(LitData, Width, Height);
}

void UFusionCamSensor::GetDepth(TArray<float>& DepthData, int& Width, int& Height, EDepthMode DepthMode)
{
	this->DepthCamSensor->CaptureDepth(DepthData, Width, Height);
}

void UFusionCamSensor::GetNormal(TArray<FColor>& NormalData, int& Width, int& Height)
{
	this->NormalCamSensor->Capture(NormalData, Width, Height);
}

void UFusionCamSensor::GetSeg(TArray<FColor>& ObjMaskData, int& Width, int& Height, ESegMode SegMode)
{
	this->AnnotationCamSensor->CaptureSeg(ObjMaskData, Width, Height);
}

FVector UFusionCamSensor::GetSensorLocation()
{
	return this->GetComponentLocation(); // World space
}

FRotator UFusionCamSensor::GetSensorRotation()
{
	return this->GetComponentRotation(); // World space
}

void UFusionCamSensor::SetSensorLocation(FVector Location)
{
	this->SetWorldLocation(Location);
}

void UFusionCamSensor::SetSensorRotation(FRotator Rotator)
{
	this->SetWorldRotation(Rotator);
}

void UFusionCamSensor::SetFilmSize(int Width, int Height)
{
	this->FilmWidth = Width;
	this->FilmHeight = Height;
	if (Height == 0 || Width == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid film size %d x %d"), Width, Height);
		return;
	}
	for (int i = 0; i < FusionSensors.Num(); i++)
	{
		UBaseCameraSensor* Sensor = FusionSensors[i];
		if (IsValid(Sensor))
		{
			Sensor->SetFilmSize(FilmWidth, FilmHeight);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Sensor %d within FusionCamSensor is invalid."), i);
		}
	}
}

float UFusionCamSensor::GetSensorFOV()
{
	return this->LitCamSensor->GetFOV(); 
}

void UFusionCamSensor::SetSensorFOV(float fov)
{
	this->FOV = fov;
	for (UBaseCameraSensor* Sensor: FusionSensors)
	{
		if (IsValid(Sensor))
		{
			Sensor->SetFOV(fov);
		}
	}
}

TArray<UFusionCamSensor*> UFusionCamSensor::GetComponents(AActor* Actor)
{
	TArray<UFusionCamSensor*> Components;
	if (!IsValid(Actor))
	{
		UE_LOG(LogTemp, Warning, TEXT("Actor is invalid"));
		return Components;
	}

	TArray<UActorComponent*> ChildComponents = Actor->K2_GetComponentsByClass(UFusionCamSensor::StaticClass());
	for (UActorComponent* Component : ChildComponents)
	{
		Components.Add(Cast<UFusionCamSensor>(Component));
	}
	return Components;
}

#if WITH_EDITOR
void UFusionCamSensor::PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FName PropertyName = (PropertyChangedEvent.Property != NULL) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UFusionCamSensor, PresetFilmSize))
	{
		switch(PresetFilmSize)
		{
		case EPresetFilmSize::F640x480:
			SetFilmSize(640, 480);
			break;
		case EPresetFilmSize::F1080p:
			SetFilmSize(1920, 1080);
			break;
		case EPresetFilmSize::F720p:
			SetFilmSize(1280, 720);
			break;
		}
	}
}
#endif


void UFusionCamSensor::SetProjectionType(ECameraProjectionMode::Type ProjectionType)
{
	for (int i = 0; i < FusionSensors.Num(); i++)
	{
		UBaseCameraSensor* Sensor = FusionSensors[i];
		if (IsValid(Sensor))
		{
			Sensor->ProjectionType = ProjectionType;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Sensor %d within FusionCamSensor is invalid."), i);
		}
	}
}

void UFusionCamSensor::SetOrthoWidth(float OrthoWidth)
{
	for (int i = 0; i < FusionSensors.Num(); i++)
	{
		UBaseCameraSensor* Sensor = FusionSensors[i];
		if (!IsValid(Sensor))
		{
			UE_LOG(LogTemp, Warning, TEXT("Sensor %d within FusionCamSensor is invalid."), i);
			continue;
		}
		Sensor->OrthoWidth = OrthoWidth;
	}
}

void UFusionCamSensor::SetLitCaptureSource(ESceneCaptureSource CaptureSource)
{
    this->LitCamSensor->CaptureSource = CaptureSource;
}

// Configure the post process settings
void UFusionCamSensor::SetReflectionMethod(EReflectionMethod::Type Method)
{
    // None, Lumen, ScreenSpace, RayTraced
    this->LitCamSensor->PostProcessSettings.bOverride_ReflectionMethod = true;
    this->LitCamSensor->PostProcessSettings.ReflectionMethod = Method;
}

void UFusionCamSensor::SetGlobalIlluminationMethod(EDynamicGlobalIlluminationMethod::Type Method)
{
    // None, Lumen, ScreenSpace, RayTraced, Plugin,
    this->LitCamSensor->PostProcessSettings.bOverride_DynamicGlobalIlluminationMethod = true;
    this->LitCamSensor->PostProcessSettings.DynamicGlobalIlluminationMethod = Method;
}

void UFusionCamSensor::SetExposureMethod(EAutoExposureMethod Method)
{
    this->LitCamSensor->PostProcessSettings.bOverride_AutoExposureMethod = true;
    this->LitCamSensor->PostProcessSettings.AutoExposureMethod = Method;
}

void UFusionCamSensor::SetExposureBias(float ExposureBias)
{
    this->LitCamSensor->PostProcessSettings.bOverride_AutoExposureBias = true;
    this->LitCamSensor->PostProcessSettings.AutoExposureBias = ExposureBias;
}

void UFusionCamSensor::SetAutoExposureSpeed(float SpeedDown, float SpeedUp)
{
    this->LitCamSensor->PostProcessSettings.bOverride_AutoExposureSpeedDown = true;
    this->LitCamSensor->PostProcessSettings.AutoExposureSpeedDown = SpeedDown;
    this->LitCamSensor->PostProcessSettings.bOverride_AutoExposureSpeedUp = true;
    this->LitCamSensor->PostProcessSettings.AutoExposureSpeedUp = SpeedUp;
}

void UFusionCamSensor::SetAutoExposureBrightness(float MinBrightness, float MaxBrightness)
{
    // Brightness range for the auto exposure algorithm
    if (MinBrightness > MaxBrightness)
    {
        UE_LOG(LogUnrealCV, Warning, TEXT("MinBrightness should be smaller than MaxBrightness"));
        return;
    }
    // Auto-Exposure minimum adaptation.
    this->LitCamSensor->PostProcessSettings.bOverride_AutoExposureMinBrightness = true;
    this->LitCamSensor->PostProcessSettings.AutoExposureMinBrightness = MinBrightness;
    // Auto-Exposure
    this->LitCamSensor->PostProcessSettings.bOverride_AutoExposureMaxBrightness = true;
    this->LitCamSensor->PostProcessSettings.AutoExposureMaxBrightness = MaxBrightness;
}

void UFusionCamSensor::SetApplyPhysicalCameraExposure(int ApplyPhysicalCameraExposure)
{
    this->LitCamSensor->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
    this->LitCamSensor->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = ApplyPhysicalCameraExposure;
}


void UFusionCamSensor::SetMotionBlurParams(float MotionBlurAmount, float MotionBlurMax, float MotionBlurPerObjectSize, int MotionBlurTargetFPS)
{
    // Strength of motion blur, 0:off
    this->LitCamSensor->PostProcessSettings.bOverride_MotionBlurAmount = true;
    this->LitCamSensor->PostProcessSettings.MotionBlurAmount = MotionBlurAmount;
    // Max distortion caused by motion blur, in percent of the screen width, 0:off
    this->LitCamSensor->PostProcessSettings.bOverride_MotionBlurMax = true;
    this->LitCamSensor->PostProcessSettings.MotionBlurMax = MotionBlurMax;
    // The minimum projected screen radius for a primitive to be drawn in the velocity pass, percentage of screen width.
    this->LitCamSensor->PostProcessSettings.bOverride_MotionBlurPerObjectSize = true;
    this->LitCamSensor->PostProcessSettings.MotionBlurPerObjectSize = MotionBlurPerObjectSize;
    // Target frame rate for motion blur
    this->LitCamSensor->PostProcessSettings.bOverride_MotionBlurTargetFPS = true;
    this->LitCamSensor->PostProcessSettings.MotionBlurTargetFPS = MotionBlurTargetFPS;
}

void UFusionCamSensor::SetFocalParams(float FocalDistance, float FocalRegion)
{
    this->LitCamSensor->PostProcessSettings.bOverride_DepthOfFieldFocalDistance = true;
    this->LitCamSensor->PostProcessSettings.DepthOfFieldFocalDistance = FocalDistance;
    this->LitCamSensor->PostProcessSettings.bOverride_DepthOfFieldFocalRegion = true;
    this->LitCamSensor->PostProcessSettings.DepthOfFieldFocalRegion = FocalRegion;
}