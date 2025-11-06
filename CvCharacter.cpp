// Weichao Qiu @ 2019

#include "CvCharacter.h"
#include "Runtime/Engine/Classes/Animation/AnimSingleNodeInstance.h"
#include "Runtime/Engine/Classes/Kismet/KismetMathLibrary.h"
#include "Runtime/Json/Public/Serialization/JsonSerializer.h"

#include "ImageUtil.h"
#include "AnnotationComponent.h"
//#include "Component/VertexCaptureComponent.h"
//#include "Component/MaterialControlComponent.h"
#include "Serialization.h"

// template<class T>
// static TArray<T*> GetChildren(USceneComponent* Parent)
// {
// 	TArray<USceneComponent*> Children = Parent->GetAttachChildren();
// 	TArray<T*> AnnotationComps;
// 	for (USceneComponent* Comp : Children)
// 	{
// 		T* AnnotationComp = Cast<T>(Comp);
// 		if (AnnotationComp)
// 		{
// 			AnnotationComps.Add(AnnotationComp);
// 		}
// 	}
// 	return AnnotationComps;
// }

// Sets default values
ACvCharacter::ACvCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true; // Is this important???

	// comment out by ch
    /*TrackingCamera = CreateDefaultSubobject<UFusionCamSensor>(TEXT("TrackingCamera"));
    TrackingCamera->SetupAttachment(RootComponent);*/

	GetMesh()->SetRelativeLocation(FVector(0, 0, -80));
    GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);

	// SegComponent = CreateDefaultSubobject<UAnnotationComponent>(TEXT("SegComponent"));
	// SegComponent->SetupAttachment(GetMesh());
	// SegComponent->SetAnnotationColor(FColor(0, 127, 0));

	/*MaterialComponent = CreateDefaultSubobject<UMaterialControlComponent>(TEXT("MaterialComponent"));
	MaterialComponent->SetupAttachment(GetMesh());*/
	// MaterialComponent->RegisterComponent();

    BindCommands(); // TODO: What is the best time to bind commands?
}

// Called when the game starts or when spawned
void ACvCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ACvCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void ACvCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

// void ACvCharacter::SetAnimationLocation(float Time) {
// 	GetMesh()->SetPosition(Time);
// }

int ACvCharacter::GetAnimationFrames(FString AnimationPath)
{
	UAnimSequence* AnimSeq = LoadObject<UAnimSequence>(nullptr, *AnimationPath);
	if (IsValid(AnimSeq))
	{
		return AnimSeq->GetNumberOfFrames();
	}
	else
	{
		return 0;
	}
}

FExecStatus ACvCharacter::GetAnimationFrames(const TArray<FString>& Args)
{
	if (Args.Num() != 1) return FExecStatus::InvalidArgument;

	FString AnimationPath = Args[0];
	int NumFrames = GetAnimationFrames(AnimationPath);
	return FExecStatus::OK(FString::Printf(TEXT("%d"), NumFrames));
}

void ACvCharacter::SetAnimationTime(FString AnimationPath, float Time) 
{
    UAnimSequence* AnimSeq = LoadObject<UAnimSequence>(nullptr, *AnimationPath);
	GetMesh()->SetAnimation(AnimSeq);
	GetMesh()->SetPosition(Time);
	GetMesh()->MarkRenderStateDirty();
}

bool ACvCharacter::SetAnimationRatio(FString AnimationPath, float Ratio)
{
    UAnimSequence* AnimSeq = LoadObject<UAnimSequence>(nullptr, *AnimationPath);
	if (!IsValid(AnimSeq)) return false;
    float TotalLength = AnimSeq->GetPlayLength();
    float Time = Ratio * TotalLength;
	GetMesh()->SetAnimation(AnimSeq);
	GetMesh()->SetPosition(Time);
	GetMesh()->MarkRenderStateDirty();

	return true;
}

void ACvCharacter::BindCommands()
{
    // Make sure these commands can be used in the editor mode.
    FDispatcherDelegate SetMeshCmd = FDispatcherDelegate::CreateUObject(this, &ACvCharacter::SetSkelMesh);
    /*FUnrealcvServer::Get().CommandDispatcher->BindCommand(
        FString::Printf(TEXT("vset /animal/%s/mesh [str]"), *GetName()),
        SetMeshCmd,
        "Set animal mesh path"
    );*/
    FUnrealcvServer::Get().CommandDispatcher->BindCommand(
        FString::Printf(TEXT("vset /human/%s/mesh [str]"), *GetName()),
        SetMeshCmd,
        "Set animal mesh path"
    );

    FDispatcherDelegate SetAnimationTimeCmd = FDispatcherDelegate::CreateUObject(this, &ACvCharacter::SetAnimationTime);
    /*FUnrealcvServer::Get().CommandDispatcher->BindCommand(
        FString::Printf(TEXT("vset /animal/%s/animation/time [str] [float]"), *GetName()),
        SetAnimationTimeCmd,
        "Set animal animation"
    );*/
    FUnrealcvServer::Get().CommandDispatcher->BindCommand(
        FString::Printf(TEXT("vset /human/%s/animation/time [str] [float]"), *GetName()),
        SetAnimationTimeCmd,
        "Set animal animation"
    );

    FDispatcherDelegate SetAnimationRatioCmd = FDispatcherDelegate::CreateUObject(this, &ACvCharacter::SetAnimationRatio);
    /*FUnrealcvServer::Get().CommandDispatcher->BindCommand(
        FString::Printf(TEXT("vset /animal/%s/animation/ratio [str] [float]"), *GetName()),
        SetAnimationRatioCmd,
        "Set animal animation"
    );*/
    FUnrealcvServer::Get().CommandDispatcher->BindCommand(
        FString::Printf(TEXT("vset /human/%s/animation/ratio [str] [float]"), *GetName()),
        SetAnimationRatioCmd,
        "Set animal animation"
    );

    FDispatcherDelegate GetAnimationFramesCmd = FDispatcherDelegate::CreateUObject(this, &ACvCharacter::GetAnimationFrames);
    /*FUnrealcvServer::Get().CommandDispatcher->BindCommand(
        FString::Printf(TEXT("vget /animal/%s/animation/frames [str]"), *GetName()),
        GetAnimationFramesCmd,
        "Set animal animation"
    );*/

    FUnrealcvServer::Get().CommandDispatcher->BindCommand(
        FString::Printf(TEXT("vget /human/%s/animation/frames [str]"), *GetName()),
        GetAnimationFramesCmd,
        "Set animal animation"
    );

    //FDispatcherDelegate SetCameraCmd = FDispatcherDelegate::CreateUObject(this, &ACvCharacter::SetCamera);
    /*FUnrealcvServer::Get().CommandDispatcher->BindCommand(
        FString::Printf(TEXT("vset /animal/%s/camera [float] [float] [float]"), *GetName()),
        SetCameraCmd,
        "Set camera attached to this animal"
    );*/
    
    //FDispatcherDelegate GetImgCmd = FDispatcherDelegate::CreateUObject(this, &ACvCharacter::GetImage);
    /*FUnrealcvServer::Get().CommandDispatcher->BindCommand(
        FString::Printf(TEXT("vget /animal/%s/image"), *GetName()),
        GetImgCmd,
        "Get image from attached camera"
    );*/

    /*FDispatcherDelegate GetDepthCmd = FDispatcherDelegate::CreateUObject(this, &ACvCharacter::GetDepth);
    FUnrealcvServer::Get().CommandDispatcher->BindCommand(
        FString::Printf(TEXT("vget /animal/%s/depth"), *GetName()),
        GetDepthCmd,
        "Get image from attached camera"
    );*/

    //FDispatcherDelegate GetSegCmd = FDispatcherDelegate::CreateUObject(this, &ACvCharacter::GetSeg);
    /*FUnrealcvServer::Get().CommandDispatcher->BindCommand(
        FString::Printf(TEXT("vget /animal/%s/seg"), *GetName()),
        GetSegCmd,
        "Get image from attached camera"
    );*/

    //FDispatcherDelegate GetKpCmd = FDispatcherDelegate::CreateUObject(this, &ACvCharacter::GetKeypoint);
    /*FUnrealcvServer::Get().CommandDispatcher->BindCommand(
        FString::Printf(TEXT("vget /animal/%s/keypoint"), *GetName()),
        GetKpCmd,
        "Get keypoint annotation from attached camera"
    );*/
    /*FUnrealcvServer::Get().CommandDispatcher->BindCommand(
        FString::Printf(TEXT("vget /human/%s/keypoint"), *GetName()),
        GetKpCmd,
        "Get keypoint annotation from attached camera"
    );*/
    
    FDispatcherDelegate Get3dKpCmd = FDispatcherDelegate::CreateUObject(this, &ACvCharacter::Get3DKeypoint);
    /*FUnrealcvServer::Get().CommandDispatcher->BindCommand(
        FString::Printf(TEXT("vget /animal/%s/3d_keypoint"), *GetName()),
        Get3dKpCmd,
        "Get 3d keypoint annotation in world coordinate"
    );*/
    FUnrealcvServer::Get().CommandDispatcher->BindCommand(
        FString::Printf(TEXT("vget /human/%s/3d_keypoint"), *GetName()),
        Get3dKpCmd,
        "Get 3d keypoint annotation in world coordinate"
    );

    //FDispatcherDelegate GetVertexCmd = FDispatcherDelegate::CreateUObject(this, &ACvCharacter::GetVertex); 
    /*FUnrealcvServer::Get().CommandDispatcher->BindCommand(
        FString::Printf(TEXT("vget /animal/%s/vertex [str]"), *GetName()),
        GetVertexCmd,
        "Save mesh data to an obj file"
    );*/
    /*FUnrealcvServer::Get().CommandDispatcher->BindCommand(
        FString::Printf(TEXT("vget /human/%s/vertex [str]"), *GetName()),
        GetVertexCmd,
        "Save mesh data to an obj file"
    );*/

	FDispatcherDelegate SetTextureCmd = FDispatcherDelegate::CreateUObject(this, &ACvCharacter::SetTexture);
	/*FUnrealcvServer::Get().CommandDispatcher->BindCommand(
		FString::Printf(TEXT("vset /animal/%s/texture [str]"), *GetName()),
		SetTextureCmd,
		"Set texture of animal"
	);*/
	FUnrealcvServer::Get().CommandDispatcher->BindCommand(
		FString::Printf(TEXT("vset /human/%s/texture [str]"), *GetName()),
		SetTextureCmd,
		"Set texture of animal"
	);

	//FDispatcherDelegate SetMaterialCmd = FDispatcherDelegate::CreateUObject(this, &ACvCharacter::SetMaterial);
	/*FUnrealcvServer::Get().CommandDispatcher->BindCommand(
		FString::Printf(TEXT("vset /animal/%s/material [uint] [str]"), *GetName()),
		SetMaterialCmd,
		"Set material of animal"
	);*/

	/*FDispatcherDelegate GetJointLocationCmd = FDispatcherDelegate::CreateUObject(this, &ACvCharacter::GetJointLocation);
	FUnrealcvServer::Get().CommandDispatcher->BindCommand(
		FString::Printf(TEXT("vget /human/%s/joint/[str]/location"), *GetName()),
		GetJointLocationCmd,
		"Get joint of a human"
	);*/
}


//FExecStatus ACvCharacter::GetImage(const TArray<FString>& Args)
//{
//	FlushRenderingCommands();
//	TArray<FColor> Data;
//	TArray<uint8> BinaryData;
//	int Width, Height;
//    this->TrackingCamera->GetLit(Data, Width, Height);
//	FImageUtil ImageUtil;
//    ImageUtil.ConvertToPng(Data, Width, Height, BinaryData);
//    return FExecStatus::Binary(BinaryData);
//}
//
//FExecStatus ACvCharacter::GetSeg(const TArray<FString>& Args)
//{
//	FlushRenderingCommands();
//	TArray<FColor> Data;
//	TArray<uint8> BinaryData;
//	int Width, Height;
//    this->TrackingCamera->GetSeg(Data, Width, Height);
//	FImageUtil ImageUtil;
//    ImageUtil.ConvertToPng(Data, Width, Height, BinaryData);
//    return FExecStatus::Binary(BinaryData);
//}
//
//FExecStatus ACvCharacter::GetDepth(const TArray<FString>& Args)
//{
//	FlushRenderingCommands();
//	TArray<float> Data;
//	TArray<uint8> BinaryData;
//	int Width, Height;
//    this->TrackingCamera->GetDepth(Data, Width, Height);
//	FImageUtil ImageUtil;
//	int Channel = Data.Num() / (Width * Height);
//    BinaryData = FSerializationUtils::Array2Npy(Data, Width, Height, Channel);
//    return FExecStatus::Binary(BinaryData);
//}

//void ACvCharacter::GetKeypoint(TArray<FString>& KpName, TArray<FVector2D>& KpScreenLocation)
//{
//    // TArray<FString> KpName;
//    TArray<FVector> KpWorldLocation;
//    TArray<FVector> KpCompLocation;
//
//    Get3DKeypoint(KpName, KpWorldLocation, KpCompLocation);
//
//    // FSceneView SceneView;
//    // SceneView.ViewLocation = TrackingCamera->GetSensorLocation();
//    // SceneView.ViewRotation = TrackingCamera->GetSensorRotation();
//    // SceneView.FOV = TrackingCamera->GetSensorFOV();
//	// SceneView.UpdateProjectionMatrix();
//	// SceneView.UpdateViewMatrix();
//    FVector ViewLocation = TrackingCamera->GetSensorLocation();
//    FRotator ViewRotation = TrackingCamera->GetSensorRotation();
//	// FViewMatrices ViewMatrices;
//	// ViewMatrices.UpdateViewMatrix(ViewLocation, ViewRotation);
//
//	FMatrix ViewPlanesMatrix = FMatrix(
//		FPlane(0, 0, 1, 0),
//		FPlane(1, 0, 0, 0),
//		FPlane(0, 1, 0, 0),
//		FPlane(0, 0, 0, 1));
//
//	const FMatrix ViewRotationMatrix = FInverseRotationMatrix(ViewRotation) * ViewPlanesMatrix;
//	FMatrix ViewMatrix = FTranslationMatrix(-ViewLocation) * ViewRotationMatrix;
//	float HalfFOV = TrackingCamera->GetSensorFOV() / 2;
//	int Width = TrackingCamera->GetFilmWidth();
//	int Height = TrackingCamera->GetFilmHeight();
//	FPerspectiveMatrix ProjectionMatrix(HalfFOV, Width, Height, 0);
//	FMatrix ViewProjectionMatrix = ViewMatrix * ProjectionMatrix;
//	
//	// ViewMatrices.Projection
//
//	for (FVector KpWorld : KpWorldLocation)
//	{
//		FVector2D KpScreen;
//        // GetWorld()->GetFirstPlayerController()->ProjectWorldLocationToScreen(KpWorld, KpScreen);
//		// static bool ProjectWorldToScreen(const FVector& WorldPosition, const FIntRect& ViewRect, const FMatrix& ViewProjectionMatrix, FVector2D& out_ScreenPos);
//
//		// Need camera location and rotation.
//		FSceneView::ProjectWorldToScreen(KpWorld, FIntRect(0, 0, Width, Height), ViewProjectionMatrix, KpScreen);
//		// SceneView.WorldToPixel(KpWorld, KpScreen)
//
//		// Build a projection matrix from here, http://www.geodesic.games/2019/03/27/projection-matrices-in-unreal-engine/
//        KpScreenLocation.Add(KpScreen);
//	}
//}
//
//FExecStatus ACvCharacter::GetKeypoint(const TArray<FString>& Args)
//{
//    TArray<FString> KpName;
//    TArray<FVector2D> KpScreenLocation;
//	GetKeypoint(KpName, KpScreenLocation);
//
//	FJsonObject JsonObj;
//	TArray<TSharedPtr<FJsonValue> > JsonArray;
//
//	for (int i = 0; i < KpName.Num(); i++)
//	{
//		TSharedPtr<FJsonObject> JsonObj = MakeShareable(new FJsonObject());
//
//		TSharedPtr<FJsonObject> KpScreen = MakeShareable(new FJsonObject());
//		KpScreen->SetNumberField("X", KpScreenLocation[i].X);
//		KpScreen->SetNumberField("Y", KpScreenLocation[i].Y);
//
//	
//		JsonObj->SetStringField("Name", KpName[i]);
//		JsonObj->SetObjectField("KpScreen", KpScreen); 
//		JsonArray.Add(MakeShareable(new FJsonValueObject(JsonObj)));
//	}
//
//	FString OutputString;
//	TSharedRef<TJsonWriter<> > Writer = TJsonWriterFactory<>::Create(&OutputString);
//	FJsonSerializer::Serialize(JsonArray, Writer);
//	// Return a json string for the data.
//    return FExecStatus::OK(OutputString);
//}

void ACvCharacter::Get3DKeypoint(TArray<FString>& KpName, TArray<FVector>& KpWorldLocation, TArray<FVector>& KpCompLocation)
{
    const TArray<FBoneIndexType>& RequiredBones = GetMesh()->RequiredBones;
    USkeletalMesh* SkeletalMesh = GetMesh()->SkeletalMesh;
    const FTransformArrayA2& ComponentSpaceTransforms = GetMesh()->GetComponentSpaceTransforms();
    const FTransformArrayA2& BoneSpaceTransforms = GetMesh()->BoneSpaceTransforms;
    const FTransform& ComponentToWorld = GetMesh()->GetComponentToWorld();

    for (int32 Index = 0; Index < RequiredBones.Num(); ++Index)
    {
        int32 BoneIndex = RequiredBones[Index];
        FName BoneName = SkeletalMesh->RefSkeleton.GetBoneName(BoneIndex);

        FTransform BoneTM = BoneSpaceTransforms[BoneIndex];
        FTransform ComponentTM = ComponentSpaceTransforms[BoneIndex];
        FTransform WorldTM = ComponentTM * ComponentToWorld;

        int32 ParentIndex = SkeletalMesh->RefSkeleton.GetParentIndex(BoneIndex);

        KpName.Add(BoneName.ToString());
        KpWorldLocation.Add(WorldTM.GetLocation());
        KpCompLocation.Add(ComponentTM.GetLocation());
    }
}

FExecStatus ACvCharacter::Get3DKeypoint(const TArray<FString>& Args)
{
	TArray<FString> KpName;
    TArray<FVector> KpWorldLocation;
    TArray<FVector> KpCompLocation;
	Get3DKeypoint(KpName, KpWorldLocation, KpCompLocation);

	FJsonObject JsonObj;
	TArray<TSharedPtr<FJsonValue> > JsonArray;

	for (int i = 0; i < KpName.Num(); i++)
	{
		TSharedPtr<FJsonObject> JsonObj = MakeShareable(new FJsonObject());

		TSharedPtr<FJsonObject> KpWorld = MakeShareable(new FJsonObject());
		KpWorld->SetNumberField("X", KpWorldLocation[i].X);
		KpWorld->SetNumberField("Y", KpWorldLocation[i].Y);
		KpWorld->SetNumberField("Z", KpWorldLocation[i].Z);

		TSharedPtr<FJsonObject> KpComp = MakeShareable(new FJsonObject());
		KpComp->SetNumberField("X", KpCompLocation[i].X);
		KpComp->SetNumberField("Y", KpCompLocation[i].Y);
		KpComp->SetNumberField("Z", KpCompLocation[i].Z);
	
		JsonObj->SetStringField("Name", KpName[i]);
		JsonObj->SetObjectField("KpWorld", KpWorld); 
		// Whether this will cause a big issue, when trying to release the pointer?
		JsonObj->SetObjectField("KpComp", KpComp);
		JsonArray.Add(MakeShareable(new FJsonValueObject(JsonObj)));
	}

	FString OutputString;
	TSharedRef<TJsonWriter<> > Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(JsonArray, Writer);
	// Return a json string for the data.
    return FExecStatus::OK(OutputString);
}

FExecStatus ACvCharacter::SetAnimationTime(const TArray<FString>& Args)
{
	if (Args.Num() != 2) return FExecStatus::InvalidArgument;

	FString AnimPath = Args[0];
	float Time = FCString::Atof(*Args[1]);

	this->AnimPath = AnimPath;
	this->AnimTime = Time;
	SetAnimationTime(AnimPath, Time);

    return FExecStatus::OK();
}

FExecStatus ACvCharacter::SetAnimationRatio(const TArray<FString>& Args)
{
	if (Args.Num() != 2) return FExecStatus::InvalidArgument;

	FString AnimPath = Args[0];
	float Ratio = FCString::Atof(*Args[1]);

	this->AnimPath = AnimPath;
	if (SetAnimationRatio(AnimPath, Ratio))
	{
		return FExecStatus::OK();
	}
	else
	{
		FString ErrorMsg = FString::Printf(TEXT("Can not use animation %s"), *AnimPath);
		UE_LOG(LogTemp, Warning, TEXT("%s"), *ErrorMsg);
		return FExecStatus::Error(ErrorMsg);
	}
}

bool ACvCharacter::SetSkelMesh(FString MeshPath)
{
	USkeletalMesh* SkeletalMesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath);
	if (!IsValid(SkeletalMesh)) 
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to find skeletal mesh %s"), *MeshPath);
	return false;
	}
	// Load skeletal mesh according to the input string
	GetMesh()->SetSkeletalMesh(SkeletalMesh);
	// GetMesh()->SetAnimInstanceClass(HumanAnimClass);
	// Update force AnnotationComponent to update

	if (IsValid(SegComponent)) SegComponent->ForceUpdate();
	return true;
}

FExecStatus ACvCharacter::SetSkelMesh(const TArray<FString>& Args)
{
	if (Args.Num() != 1) return FExecStatus::InvalidArgument;

	FString MeshPath = Args[0];

	this->MeshPath = MeshPath;
	if (SetSkelMesh(MeshPath))
	{
		return FExecStatus::OK();
	}
	else
	{
		FString ErrorMsg = FString::Printf(TEXT("Can not use skeletal mesh %s"), *MeshPath);
		UE_LOG(LogTemp, Warning, TEXT("%s"), *ErrorMsg);
		return FExecStatus::Error(ErrorMsg);
	}
    return FExecStatus::OK();
}

void ACvCharacter::SetTexture(FString TexturePath)
{
	// comment out by ch
	/*MaterialComponent->SetTextureImage(TexturePath);*/
	
	// SegComponent->ForceUpdate();
}

FExecStatus ACvCharacter::SetTexture(const TArray<FString>& Args)
{
	if (Args.Num() != 1) return FExecStatus::InvalidArgument;

	FString TexturePath = Args[0];
	SetTexture(TexturePath);
	return FExecStatus::OK();
}


//void ACvCharacter::SetCamera(float Distance, float Az, float El)
//{
//    FVector Location = FVector(Distance, 0, 0);
//    // FRotator Rotation = FRotator(InPitch = El, InYaw = Az, InRoll = 0);
//    FRotator Rotation = FRotator(El, Az, 0);
//    FVector CameraLocation = Rotation.RotateVector(Location);
//	TrackingCamera->SetRelativeLocation(CameraLocation);
//	FRotator CameraRotation = UKismetMathLibrary::FindLookAtRotation(CameraLocation, FVector(0, 0, 0));
//    TrackingCamera->SetRelativeRotation(CameraRotation);
//}
//
//FExecStatus ACvCharacter::SetCamera(const TArray<FString>& Args)
//{
//	if (Args.Num() != 3) return FExecStatus::InvalidArgument;
//
//	float Distance = FCString::Atof(*Args[0]);
//	float Azimuth = FCString::Atof(*Args[1]);
//	float Elevation = FCString::Atof(*Args[2]);
//	
//	this->CamDistance = Distance;
//	this->CamAz = Azimuth;
//	this->CamEl = Elevation;
//	SetCamera(Distance, Azimuth, Elevation);
//
//    return FExecStatus::OK();
//}

void ACvCharacter::OnConstruction(const FTransform& Transform)
{
	// This will take effect when properties are changed
	// comment out by ch
    //SetCamera(CamDistance, CamAz, CamEl);
    SetAnimationTime(AnimPath, AnimTime);
    SetSkelMesh(MeshPath);
}

// comment out by ch
//TArray<FVector> ACvCharacter::GetVertex()
//{
//	TArray<FVector> VertexList = UVertexCaptureComponent::GetVertexArray(GetMesh());
//	return VertexList;
//}

//FExecStatus ACvCharacter::GetVertex(const TArray<FString>& Args)
//{
//	if (Args.Num() > 1) return FExecStatus::InvalidArgument;
//
//	TArray<FVector> VertexList = GetVertex();
//	FString Content = FSerializationUtils::VertexList2Obj(VertexList);
//
//	if (Args.Num() == 1)
//	{
//		FString ObjFilename = Args[0];
//
//		if (ObjFilename == "obj")
//		{ 
//			return FExecStatus::OK(Content);
//		}
//		else
//		{
//			// Save VertexList as an obj file
//			FFileHelper::SaveStringToFile(Content, *ObjFilename);
//			return FExecStatus::OK();
//		}
//	}
//	else
//	{
//		return FExecStatus::OK(Content);
//	}
//
//}

// dummy function by ch
//FExecStatus ACvCharacter::GetVertex(const TArray<FString>& Args)
//{
//	return FExecStatus::OK();
//}

// TMap<FString, FString> HorseTexture;
// HorseTexture.Emplace("v1", "Material'/Game/Animal_pack_ultra_2/Materials/M_Horse_material.M_Horse_material'");
// HorseTexture.Emplace("v2", "Material'/Game/Animal_pack_ultra_2/Materials/M_Horse_v2_material.M_Horse_v2_material'");
// HorseTexture.Emplace("v3", "Material'/Game/Animal_pack_ultra_2/Materials/M_Horse_v3_material.M_Horse_v3_material'");
// HorseTexture.Emplace("v4", "Material'/Game/Animal_pack_ultra_2/Materials/M_Horse_v4_material.M_Horse_v4_material'");
// HorseTexture.Emplace("v5", "Material'/Game/Animal_pack_ultra_2/Materials/M_Horse_v5_material.M_Horse_v5_material'");
// HorseTexture.Emplace("v6", "Material'/Game/Animal_pack_ultra_2/Materials/M_Horse_v6_material.M_Horse_v6_material'");

//bool ACvCharacter::SetMaterial(int ElementIndex, FString MaterialPath)
//{
//	// Read material asset
//	UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
//
//	if (!IsValid(Material))
//	{
//		UE_LOG(LogTemp, Error, TEXT("Can not find material %s"), *MaterialPath);
//		return false;
//	}
//
//	UMaterialInterface* CurrentMaterial = GetMesh()->GetMaterial(ElementIndex);
//	if (!IsValid(CurrentMaterial))
//	{
//		UE_LOG(LogTemp, Error, TEXT("The ElementIndex %d is invalid"), ElementIndex);
//		return false;
//	}
//
//	if (false == Material->bUsedWithSkeletalMesh)
//	{
//		UE_LOG(LogTemp, Error, TEXT("The material %s is not compatible with SkeletalMesh"), *MaterialPath);
//		return false;
//	}
//	GetMesh()->SetMaterial(ElementIndex, Material);
//	// GetMesh()->MarkRenderStateDirty();
//	return true;
//}

//FExecStatus ACvCharacter::SetMaterial(const TArray<FString>& Args)
//{
//	if (Args.Num() != 2) return FExecStatus::InvalidArgument;
//
//	int ElementIndex = FCString::Atoi(*Args[0]);
//	FString MaterialPath = Args[1];
//
//	if (SetMaterial(ElementIndex, MaterialPath))
//	{
//		return FExecStatus::OK();
//	}
//	else
//	{
//		return FExecStatus::Error("Check log for details");
//	}
//}

//FExecStatus ACvCharacter::GetJointLocation(const TArray<FString>& Args)
//{
//	FString JointName = Args[0];
//	TArray<FString> KpName;
//    TArray<FVector> KpWorldLocation;
//    TArray<FVector> KpCompLocation;
//	Get3DKeypoint(KpName, KpWorldLocation, KpCompLocation);
//
//	FJsonObject JsonObj;
//	TArray<TSharedPtr<FJsonValue> > JsonArray;
//
//	for (int i = 0; i < KpName.Num(); i++)
//	{
//		if (KpName[i] == JointName)
//		{
//			FVector Loc = KpWorldLocation[i];
//			FString KpLocation = FString::Printf(TEXT("%.2f %.2f %.2f"),
//				Loc.X, Loc.Y, Loc.Z
//			);
//			return FExecStatus::OK(KpLocation);
//		}
//	}
//
//    return FExecStatus::Error(FString::Printf(TEXT("Can not find joint %s"), *JointName));
//}
