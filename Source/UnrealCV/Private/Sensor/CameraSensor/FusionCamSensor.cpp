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
#include "FlowCamSensor.h"

#include "Utils/UObjectUtils.h"
#include "Component/AnnotationComponent.h"
#include "SL.h"
#include "Utils/ImageUtil.h"
#include "SensorBPLib.h"

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

	// FlowCamSensor is created later to avoid template mismatch issues
	// Unhandled Exception: EXCEPTION_ACCESS_VIOLATION reading address 0x0000000000000008 UnrealEditor_Engine UnrealEditor_UnrealCV!AUnrealcvWorldController::OpenLevel() [C:\Users\hulc\Desktop\HUAWEI_Project\Plugins\unrealcv\Source\UnrealCV\Private\Controller\WorldController.cpp:90]
	// FlowCamSensor = nullptr;
	ComponentName = FString::Printf(TEXT("%s_%s"), *this->GetName(), TEXT("FlowCamSensor"));
	FlowCamSensor = CreateDefaultSubobject<UFlowCamSensor>(*ComponentName);
	// FlowCamSensor = NewObject<UFlowCamSensor>(this, UFlowCamSensor::StaticClass()); /*NewObject with empty name can't be used to create default subobjects*/
	FusionSensors.Add(FlowCamSensor);

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
			if (Sensor != FlowCamSensor) { Sensor->SetupAttachment(this); }
		}
		else
		{
			// UE_LOG(LogUnrealCV, Warning, TEXT("Invalid sensor is found in the ctor of FusionCamSensor"));
			UE_LOG(LogUnrealCV, Error, TEXT("Invalid sensor is found in the ctor of FusionCamSensor"));
		}
	}
	// SetFilmSize(FilmWidth, FilmHeight); // This should not not be done in CTOR.
	// print pointers in FusionSensors
}

void UFusionCamSensor::checkFusionSensors()
{
	for (int i = 0; i < FusionSensors.Num(); i++)
	{
		if (!IsValid(FusionSensors[i]))
		{
			// UE_LOG(LogUnrealCV, Error, TEXT("UFusionCamSensor::checkFusionSensors: Invalid sensor id=%d p=%p, total=%d, this=%p"), i, FusionSensors[i], FusionSensors.Num(), this);
			UE_LOG(LogTemp, Warning, TEXT("Sensor %d within FusionCamSensor is invalid."), i);
		}
	}
}

void UFusionCamSensor::BeginPlay()
{
	Super::BeginPlay();

	// LogOutputDevice: Error: Ensure condition failed: false  [File:D:\build\++UE5\Sync\Engine\Source\Runtime\Engine\Private\Components\SceneComponent.cpp] [Line: 2104] 
	// LogOutputDevice: Error: Template Mismatch during attachment. Attaching instanced component to template component. Parent 'FusionCamSensor_GEN_VARIABLE' (Owner 'None') Self 'FusionCamSensor_GEN_VARIABLE_FlowCamSensor' (Owner 'BP_Drone01_C_1').
	// So we have to attach FlowCamSensor after the actor is spawned, in BeginPlay.
	// If we put this in the ctor, I think all the blueprints have to be rebuild to fix this bug.
	// Howerver, because we put the AttachToComponent here, we can no longger use editor to adjust the FlowCam transform in blueprint.
	if (IsValid(FlowCamSensor))
	{
		// Ensure condition failed: !bRegistered  [File:D:\build\++UE5\Sync\Engine\Source\Runtime\Engine\Private\Components\SceneComponent.cpp] [Line: 1958] 
		// SetupAttachment should only be used to initialize AttachParent and AttachSocketName for a future AttachToComponent. Once a component is registered you must use AttachToComponent. Owner [/Game/SuburbNeighborhoodHousePack/Maps/SuburbNeighborhood_Day.SuburbNeighborhood_Day:PersistentLevel.BP_Character_C_1], InParent [FusionCamSensor], InSocketName [None]
		// FlowCamSensor->SetupAttachment(this);

		FlowCamSensor->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
	    const FTransform LitTransform = LitCamSensor->GetComponentTransform();
	    FlowCamSensor->SetWorldTransform(LitTransform);
		// const FTransform LitRelativeTransform = LitCamSensor->GetRelativeTransform();
		// FlowCamSensor->SetRelativeTransform(LitRelativeTransform);
	}
	else 
	{
		UE_LOG(LogUnrealCV, Error, TEXT("FlowCamSensor is not initialized. Flow data will be empty."));
	}

	SetFilmSize(FilmWidth, FilmHeight);
	SetSensorFOV(FOV);

	// for (UBaseCameraSensor* Sensor : FusionSensors)
	// {
	// 	if (IsValid(Sensor))
	// 	{
	// 		Sensor->InitializeAsyncCapture();
	// 	}
	// }
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


// void UFusionCamSensor::GetLitSeg(TArray<FColor>& DataRGB, TArray<FColor>& DataSeg, int& InOutWidth, int& InOutHeight)
// {
// 	if (!LitCamSensor->CheckTextureTarget()) {
// 		LitCamSensor->InitTextureTarget(this->FilmWidth, this->FilmHeight);
// 		if (!LitCamSensor->CheckTextureTarget()) {
// 			SL::get().print("LitCamSensor InitTextureTarget failed.");
// 			UE_LOG(LogUnrealCV, Error, TEXT("No TextureTarget."));
// 			return;
// 		}
// 	}
// 	if (!AnnotationCamSensor->CheckTextureTarget()) {
// 		AnnotationCamSensor->InitTextureTarget(this->FilmWidth, this->FilmHeight);
// 		if (!AnnotationCamSensor->CheckTextureTarget()) {
// 			SL::get().print("AnnotationCamSensor InitTextureTarget failed.");
// 			UE_LOG(LogUnrealCV, Error, TEXT("No TextureTarget."));
// 			return;
// 		}
// 	}

// 	LitCamSensor->CaptureScene();

// 	TArray<TWeakObjectPtr<UPrimitiveComponent>> ComponentList;
// 	AnnotationCamSensor->GetAnnotationComponents(this->GetWorld(), ComponentList);
// 	AnnotationCamSensor->ShowOnlyComponents = ComponentList;
// 	AnnotationCamSensor->CaptureScene();

// 	LitCamSensor->ReadCaptureResults(DataRGB);
// 	AnnotationCamSensor->ReadCaptureResults(DataSeg);

// 	int32 LitW = LitCamSensor->GetFilmWidth();
// 	int32 LitH = LitCamSensor->GetFilmHeight();
// 	int32 SegW = AnnotationCamSensor->GetFilmWidth();
// 	int32 SegH = AnnotationCamSensor->GetFilmHeight();

// 	// SL::get().printf("UFusionCamSensor::GetLitSeg DataRGB size: %d, width: %d, height: %d", DataRGB.Num(), LitW, LitH);
// 	// SL::get().printf("UFusionCamSensor::GetLitSeg DataSeg size: %d, width: %d, height: %d", DataSeg.Num(), SegW, SegH);

// 	if (!((LitW == SegW) && (LitH == SegH)))
// 	{
// 		SL::get().print("ERROR: Rendered frame size does not match.");
// 		UE_LOG(LogUnrealCV, Error, TEXT("Rendered frame size does not match."));
// 		DataRGB.Empty();
// 		DataSeg.Empty();
// 		return;
// 	}
// 	InOutWidth = LitW;
// 	InOutHeight = LitH;
// }


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

// color
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
	this->NormalCamSensor->CaptureNormal(NormalData, Width, Height);
}
void UFusionCamSensor::GetFlow(TArray<FColor>& FlowData, int& Width, int& Height)
{
	if (!FlowCamSensor)
	{
		UE_LOG(LogUnrealCV, Error, TEXT("FlowCamSensor is not initialized. Flow data will be empty."));
		FlowData.Empty();
		Width = 0;
		Height = 0;
		return;
	}
	this->FlowCamSensor->CaptureFlow(FlowData, Width, Height);
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
	if (this->GetComponentLocation() != Location)
	{
		this->SetWorldLocation(Location);
	}
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

	// There are still bugs in compiled blueprints, I tried to fix them in the ctor, but it still fails.
	// So I have to manually init the texture target for FlowCamSensor.
	if (IsValid(FlowCamSensor))
	{
		FlowCamSensor->SetFilmSize(Width, Height);
	}
	else
	{
		UE_LOG(LogUnrealCV, Error, TEXT("FlowCamSensor is not initialized. Flow data will be empty."));
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
			// UE_LOG(LogTemp, Error, TEXT("SetFilmSize: Sensor %d within FusionCamSensor is invalid. this: %p"), i, this);
			UE_LOG(LogTemp, Warning, TEXT("SetFilmSize: Sensor %d within FusionCamSensor is invalid."), i);
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
			// UE_LOG(LogTemp, Error, TEXT("SetProjectionType: Sensor %d within FusionCamSensor is invalid."), i);
			UE_LOG(LogTemp, Warning, TEXT("SetFilmSize: Sensor %d within FusionCamSensor is invalid."), i);
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
			// UE_LOG(LogTemp, Error, TEXT("SetOrthoWidth: Sensor %d within FusionCamSensor is invalid."), i);
			UE_LOG(LogTemp, Warning, TEXT("SetFilmSize: Sensor %d within FusionCamSensor is invalid."), i);
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

void UFusionCamSensor::SetChromaticAberration(float Intensity)
{
    // Unreal refers to Chromatic Aberration as "SceneFringe"
    this->LitCamSensor->PostProcessSettings.bOverride_SceneFringeIntensity = true;
    this->LitCamSensor->PostProcessSettings.SceneFringeIntensity = Intensity; // Realistic: 0.1 - 0.4
}

void UFusionCamSensor::SetVignetteIntensity(float Intensity)
{
    this->LitCamSensor->PostProcessSettings.bOverride_VignetteIntensity = true;
    this->LitCamSensor->PostProcessSettings.VignetteIntensity = Intensity; // Realistic: 0.4
}

void UFusionCamSensor::SetFilmGrain(float Intensity, float TexelSize)
{
    this->LitCamSensor->PostProcessSettings.bOverride_FilmGrainIntensity = true;
    this->LitCamSensor->PostProcessSettings.FilmGrainIntensity = Intensity; // Realistic: 0.1
    this->LitCamSensor->PostProcessSettings.bOverride_FilmGrainTexelSize = true;
    this->LitCamSensor->PostProcessSettings.FilmGrainTexelSize = TexelSize;
}

void UFusionCamSensor::SetConvolutionBloom(EBloomMethod Method, UTexture2D* KernelTexture, float Intensity)
{
    // Switch method to Convolution for realistic light scattering
    this->LitCamSensor->PostProcessSettings.bOverride_BloomMethod = true;
    this->LitCamSensor->PostProcessSettings.BloomMethod = Method;
    // this->LitCamSensor->PostProcessSettings.BloomMethod = BM_FFT;
    // this->LitCamSensor->PostProcessSettings.BloomMethod = BM_SOG;

    // Apply the kernel (a texture representing the physical shape of the lens flare)
    if (KernelTexture)
    {
        this->LitCamSensor->PostProcessSettings.bOverride_BloomConvolutionTexture = true;
        this->LitCamSensor->PostProcessSettings.BloomConvolutionTexture = KernelTexture;
    }

    this->LitCamSensor->PostProcessSettings.bOverride_BloomIntensity = true;
    this->LitCamSensor->PostProcessSettings.BloomIntensity = Intensity;
}

void UFusionCamSensor::SetUseFastCapture(bool bInUseFast)
{
	for (UBaseCameraSensor* Sensor : FusionSensors)
	{
		if (IsValid(Sensor))
		{
			Sensor->SetUseFastCapture(bInUseFast);
		}
	}
	this->LitCamSensor->SetUseFastCapture(bInUseFast);
	this->DepthCamSensor->SetUseFastCapture(bInUseFast);
	this->AnnotationCamSensor->SetUseFastCapture(bInUseFast);
	this->NormalCamSensor->SetUseFastCapture(bInUseFast);
	this->FlowCamSensor->SetUseFastCapture(bInUseFast);
}

bool UFusionCamSensor::GetUseFastCapture() const
{
	bool bLitFastCapture = this->LitCamSensor->GetUseFastCapture();
	bool bDepthFastCapture = this->DepthCamSensor->GetUseFastCapture();
	bool bAnnotationFastCapture = this->AnnotationCamSensor->GetUseFastCapture();
	bool bNormalFastCapture = this->NormalCamSensor->GetUseFastCapture();
	bool bFlowFastCapture = this->FlowCamSensor->GetUseFastCapture();
	if (!(bLitFastCapture == bDepthFastCapture && bLitFastCapture == bAnnotationFastCapture && bLitFastCapture == bNormalFastCapture && bLitFastCapture == bFlowFastCapture))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("UFusionCamSensor::GetUseFastCapture: Inconsistent fast settings detected!"));
	}
	return bLitFastCapture;
}


void UFusionCamSensor::SaveLitToFile(const FString& Filename) { LitCamSensor->CaptureLitToFile(Filename); }
void UFusionCamSensor::SaveDepthToFile(const FString& Filename) { DepthCamSensor->CaptureDepthToFile(Filename); }
void UFusionCamSensor::SaveNormalToFile(const FString& Filename) { NormalCamSensor->CaptureNormalToFile(Filename); }		
void UFusionCamSensor::SaveFlowToFile(const FString& Filename) { FlowCamSensor->CaptureFlowToFile(Filename); }
void UFusionCamSensor::SaveSegToFile(const FString& Filename) { AnnotationCamSensor->CaptureSegToFile(Filename); }