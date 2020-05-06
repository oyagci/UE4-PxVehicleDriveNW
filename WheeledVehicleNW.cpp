#include "WheeledVehicleNW.h"

// Copyright Unreal Engine Community.

#include "MyVehicleProject.h"
//#include "VehicleProjectWheelFront.h"
//#include "VehicleProjectWheelRear.h"
//#include "VehicleProjectHud.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/TextRenderComponent.h"
#include "Sound/SoundCue.h"
#include "VehicleMovementComponentNW.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Engine/SkeletalMesh.h"
#include "WheeledVehicleNW.h"
#include "MyVehicleWheel.h"
#include "Components/AudioComponent.h"

#ifdef HMD_INTGERATION
// Needed for VR Headset.
#include "Engine.h"
#include "IHeadMountedDisplay.h"
#endif // HMD_INTGERATION.

const FName AWheeledVehicleNW::LookUpBinding("LookUp");
const FName AWheeledVehicleNW::LookRightBinding("LookRight");
const FName AWheeledVehicleNW::EngineAudioRPM("RPM");

#define LOCTEXT_NAMESPACE "VehicleNW"

AWheeledVehicleNW::AWheeledVehicleNW(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UVehicleMovementComponentNW>(VehicleMovementComponentName))
{
	// Car mesh.
//	static ConstructorHelpers::FObjectFinder<USkeletalMesh> CarMesh(TEXT("/Game/VehicleAdv/Vehicle/Vehicle_SkelMesh.Vehicle_SkelMesh"));
//	GetMesh()->SetSkeletalMesh(CarMesh.Object);
//
//	static ConstructorHelpers::FClassFinder<UObject> AnimBPClass(TEXT("/Game/VehicleAdv/Vehicle/VehicleAnimationBlueprint"));
//	GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
//	GetMesh()->SetAnimInstanceClass(AnimBPClass.Class);
//
//	// Setup friction materials.
//	static ConstructorHelpers::FObjectFinder<UPhysicalMaterial> SlipperyMat(TEXT("/Game/Content/Slippery.uasset"));
//	SlipperyMaterial = SlipperyMat.Object;
//
//	static ConstructorHelpers::FObjectFinder<UPhysicalMaterial> NonSlipperyMat(TEXT("/Game/Content/NonSlippery.uasset"));
//	NonSlipperyMaterial = NonSlipperyMat.Object;

	UVehicleMovementComponentNW* VehicleNW = CastChecked<UVehicleMovementComponentNW>(GetVehicleMovementComponent());

	check(VehicleNW->WheelSetups.Num() == 4);

	// Wheels/Tires.
	// Setup the wheels.
	VehicleNW->WheelSetups[0].WheelClass = UMyVehicleWheel::StaticClass();
	VehicleNW->WheelSetups[0].BoneName = FName("Wheel_Left_1");
	VehicleNW->WheelSetups[0].AdditionalOffset = FVector(0.f, 0.f, 0.f);

	VehicleNW->WheelSetups[1].WheelClass = UMyVehicleWheel::StaticClass();
	VehicleNW->WheelSetups[1].BoneName = FName("Wheel_Right_1");
	VehicleNW->WheelSetups[1].AdditionalOffset = FVector(0.f, 0.f, 0.f);

	VehicleNW->WheelSetups[2].WheelClass = UMyVehicleWheel::StaticClass();
	VehicleNW->WheelSetups[2].BoneName = FName("Wheel_Left_4");
	VehicleNW->WheelSetups[2].AdditionalOffset = FVector(0.f, 0.f, 0.f);

	VehicleNW->WheelSetups[3].WheelClass = UMyVehicleWheel::StaticClass();
	VehicleNW->WheelSetups[3].BoneName = FName("Wheel_Right_4");
	VehicleNW->WheelSetups[3].AdditionalOffset = FVector(0.f, 0.f, 0.f);

	// Adjust the tire loading.
	VehicleNW->MinNormalizedTireLoad = 0.0f;
	VehicleNW->MinNormalizedTireLoadFiltered = 0.2f;
	VehicleNW->MaxNormalizedTireLoad = 2.0f;
	VehicleNW->MaxNormalizedTireLoadFiltered = 2.0f;

	// Engine.
	// Torque setup.
	VehicleNW->MaxEngineRPM = 5700.0f;
	VehicleNW->EngineSetup.TorqueCurve.GetRichCurve()->Reset();
	VehicleNW->EngineSetup.TorqueCurve.GetRichCurve()->AddKey(0.0f, 400.0f);
	VehicleNW->EngineSetup.TorqueCurve.GetRichCurve()->AddKey(1890.0f, 500.0f);
	VehicleNW->EngineSetup.TorqueCurve.GetRichCurve()->AddKey(5730.0f, 400.0f);

	// Adjust the steering .
	VehicleNW->SteeringCurve.GetRichCurve()->Reset();
	VehicleNW->SteeringCurve.GetRichCurve()->AddKey(0.0f, 1.0f);
	VehicleNW->SteeringCurve.GetRichCurve()->AddKey(40.0f, 0.7f);
	VehicleNW->SteeringCurve.GetRichCurve()->AddKey(120.0f, 0.6f);

	// Automatic gearbox.
	VehicleNW->TransmissionSetup.bUseGearAutoBox = true;
	VehicleNW->TransmissionSetup.GearSwitchTime = 0.15f;
	VehicleNW->TransmissionSetup.GearAutoBoxLatency = 1.0f;

	// Physics settings.
	// Adjust the center of mass - the buggy is quite low.
	UPrimitiveComponent* UpdatedPrimitive = Cast<UPrimitiveComponent>(VehicleNW->UpdatedComponent);
	if (UpdatedPrimitive)
	{
		UpdatedPrimitive->BodyInstance.COMNudge = FVector(8.0f, 0.0f, 0.0f);
	}

	// Set the inertia scale. This controls how the mass of the vehicle is distributed.
	VehicleNW->InertiaTensorScale = FVector(1.0f, 1.333f, 1.2f);

	// Colors for the in-car gear display. One for normal one for reverse.
	GearDisplayReverseColor = FColor(255, 0, 0, 255);
	GearDisplayColor = FColor(255, 255, 255, 255);

	bIsLowFriction = false;
	bInReverseGear = false;
}

void AWheeledVehicleNW::SetupPlayerInputComponent(class UInputComponent* Input)
{
	// Set up gameplay key bindings.
	check(Input);

	Input->BindAction("Handbrake", IE_Pressed, this, &AWheeledVehicleNW::OnHandbrakePressed);
	Input->BindAction("Handbrake", IE_Released, this, &AWheeledVehicleNW::OnHandbrakeReleased);
}

void AWheeledVehicleNW::MoveForward(float Val)
{
	UE_LOG(LogTemp, Warning, TEXT("MoveForward %f"), Val);
	GetVehicleMovementComponent()->SetThrottleInput(Val);
}

void AWheeledVehicleNW::MoveRight(float Val)
{
	GetVehicleMovementComponent()->SetSteeringInput(Val);
}

void AWheeledVehicleNW::OnHandbrakePressed()
{
	GetVehicleMovementComponent()->SetHandbrakeInput(true);
}

void AWheeledVehicleNW::OnHandbrakeReleased()
{
	GetVehicleMovementComponent()->SetHandbrakeInput(false);
}

void AWheeledVehicleNW::OnToggleCamera()
{
	EnableIncarView(!bInCarCameraActive);
}

void AWheeledVehicleNW::EnableIncarView(const bool bState)
{
}

void AWheeledVehicleNW::Tick(float Delta)
{
	Super::Tick(Delta);

	// Setup the flag to say we are in reverse gear.
	bInReverseGear = GetVehicleMovementComponent()->GetCurrentGear() < 0;

	// Update physics material.
	UpdatePhysicsMaterial();
}

void AWheeledVehicleNW::BeginPlay()
{
	Super::BeginPlay();
}

void AWheeledVehicleNW::OnResetVR()
{
#ifdef HMD_INTGERATION
	if (GEngine->HMDDevice.IsValid())
	{
		GEngine->HMDDevice->ResetOrientationAndPosition();
		InternalCamera->SetRelativeLocation(InternalCameraOrigin);
		GetController()->SetControlRotation(FRotator());
	}
#endif // HMD_INTGERATION.
}

void AWheeledVehicleNW::UpdateHUDStrings()
{
}

void AWheeledVehicleNW::SetupInCarHUD()
{
}

void AWheeledVehicleNW::UpdatePhysicsMaterial()
{
	if (GetActorUpVector().Z < 0)
	{
		if (bIsLowFriction == true)
		{
			//GetMesh()->SetPhysMaterialOverride(NonSlipperyMaterial);
			bIsLowFriction = false;
		}
		else
		{
			//GetMesh()->SetPhysMaterialOverride(SlipperyMaterial);
			bIsLowFriction = true;
		}
	}
}