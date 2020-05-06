// Copyright Unreal Engine Community.

#include "VehicleMovementComponentNW.h"
#include "PhysXIncludes.h"
#include "PhysXPublic.h"
#include "PhysicsEngine/BodyInstance.h"

UVehicleMovementComponentNW::UVehicleMovementComponentNW(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
#if WITH_PHYSX_VEHICLES

	PxVehicleEngineData DefEngineData;
	EngineSetup.MOI = DefEngineData.mMOI;
	EngineSetup.MaxRPM = OmegaToRPM(DefEngineData.mMaxOmega);
	EngineSetup.DampingRateFullThrottle = DefEngineData.mDampingRateFullThrottle;
	EngineSetup.DampingRateZeroThrottleClutchEngaged = DefEngineData.mDampingRateZeroThrottleClutchEngaged;
	EngineSetup.DampingRateZeroThrottleClutchDisengaged = DefEngineData.mDampingRateZeroThrottleClutchDisengaged;

	// Convert from PhysX curve to ours.
	FRichCurve* TorqueCurveData = EngineSetup.TorqueCurve.GetRichCurve();
	for (PxU32 KeyIdx = 0; KeyIdx < DefEngineData.mTorqueCurve.getNbDataPairs(); KeyIdx++)
	{
		float Input = DefEngineData.mTorqueCurve.getX(KeyIdx) * EngineSetup.MaxRPM;
		float Output = DefEngineData.mTorqueCurve.getY(KeyIdx) * DefEngineData.mPeakTorque;
		TorqueCurveData->AddKey(Input, Output);
	}

	PxVehicleClutchData DefClutchData;
	TransmissionSetup.ClutchStrength = DefClutchData.mStrength;

	PxVehicleGearsData DefGearSetup;
	TransmissionSetup.GearSwitchTime = DefGearSetup.mSwitchTime;
	TransmissionSetup.ReverseGearRatio = DefGearSetup.mRatios[PxVehicleGearsData::eREVERSE];
	TransmissionSetup.FinalRatio = DefGearSetup.mFinalRatio;

	PxVehicleAutoBoxData DefAutoBoxSetup;
	TransmissionSetup.NeutralGearUpRatio = DefAutoBoxSetup.mUpRatios[PxVehicleGearsData::eNEUTRAL];
	TransmissionSetup.GearAutoBoxLatency = DefAutoBoxSetup.getLatency();
	TransmissionSetup.bUseGearAutoBox = true;

	for (uint32 i = PxVehicleGearsData::eFIRST; i < DefGearSetup.mNbRatios; i++)
	{
		FVehicleGearNWData GearData;
		GearData.DownRatio = DefAutoBoxSetup.mDownRatios[i];
		GearData.UpRatio = DefAutoBoxSetup.mUpRatios[i];
		GearData.Ratio = DefGearSetup.mRatios[i];
		TransmissionSetup.ForwardGears.Add(GearData);
	}

	// Init steering speed curve.
	FRichCurve* SteeringCurveData = SteeringCurve.GetRichCurve();
	SteeringCurveData->AddKey(0.f, 1.f);
	SteeringCurveData->AddKey(20.f, 0.9f);
	SteeringCurveData->AddKey(60.f, 0.8f);
	SteeringCurveData->AddKey(120.f, 0.7f);

	NumOfWheels = 4;

	// Initialize WheelSetups array with 4 wheels.
	WheelSetups.SetNum(NumOfWheels);

	// Grab default values from physx.
	PxVehicleDifferentialNWData DefDifferentialSetup;
	for (int32 WheelIdx = 0; WheelIdx < NumOfWheels; ++WheelIdx)
	{
		FDrivenWheelData DiffData;
		DiffData.IsDrivenWheel = DefDifferentialSetup.getIsDrivenWheel(WheelIdx);
		DiffData.DrivenWheelIndex = WheelIdx;
		DifferentialSetup.DWheelData.Add(DiffData);
	}
#endif // WITH_PHYSX_VEHICLES
}


//////////////////////////////////////////////////////////////////////////
///////////Modified Code from WheeledVehicleMovementComponent4W///////////

#if WITH_EDITOR
void UVehicleMovementComponentNW::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	const FName PropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if (PropertyName == TEXT("DownRatio"))
	{
		for (int32 GearIdx = 0; GearIdx < TransmissionSetup.ForwardGears.Num(); ++GearIdx)
		{
			FVehicleGearNWData & GearData = TransmissionSetup.ForwardGears[GearIdx];
			GearData.DownRatio = FMath::Min(GearData.DownRatio, GearData.UpRatio);
		}
	}
	else if (PropertyName == TEXT("UpRatio"))
	{
		for (int32 GearIdx = 0; GearIdx < TransmissionSetup.ForwardGears.Num(); ++GearIdx)
		{
			FVehicleGearNWData & GearData = TransmissionSetup.ForwardGears[GearIdx];
			GearData.UpRatio = FMath::Max(GearData.DownRatio, GearData.UpRatio);
		}
	}
	else if (PropertyName == TEXT("SteeringCurve"))
	{
		// Make sure values are capped between 0 and 1.
		TArray<FRichCurveKey> SteerKeys = SteeringCurve.GetRichCurve()->GetCopyOfKeys();
		for (int32 KeyIdx = 0; KeyIdx < SteerKeys.Num(); ++KeyIdx)
		{
			float NewValue = FMath::Clamp(SteerKeys[KeyIdx].Value, 0.f, 1.f);
			SteeringCurve.GetRichCurve()->UpdateOrAddKey(SteerKeys[KeyIdx].Time, NewValue);
		}
	}
}
#endif // WITH_EDITOR

#if WITH_PHYSX_VEHICLES
static void GetVehicleDifferentialNWSetup(const FVehicleDifferentialNWData& Setup, PxVehicleDifferentialNWData& PxSetup)
{
	for (int32 i = 0; i < Setup.DWheelData.Num(); i++)
	{
		PxSetup.setDrivenWheel(Setup.DWheelData[i].DrivenWheelIndex, Setup.DWheelData[i].IsDrivenWheel);
	}
}

float FVehicleEngineNWData::FindPeakTorque() const
{
	// Find max torque.
	float PeakTorque = 0.f;
	TArray<FRichCurveKey> TorqueKeys = TorqueCurve.GetRichCurveConst()->GetCopyOfKeys();
	for (int32 KeyIdx = 0; KeyIdx < TorqueKeys.Num(); KeyIdx++)
	{
		FRichCurveKey& Key = TorqueKeys[KeyIdx];
		PeakTorque = FMath::Max(PeakTorque, Key.Value);
	}
	return PeakTorque;
}

static void GetVehicleEngineSetup(const FVehicleEngineNWData& Setup, PxVehicleEngineData& PxSetup)
{
	PxSetup.mMOI = M2ToCm2(Setup.MOI);
	PxSetup.mMaxOmega = RPMToOmega(Setup.MaxRPM);
	PxSetup.mDampingRateFullThrottle = M2ToCm2(Setup.DampingRateFullThrottle);
	PxSetup.mDampingRateZeroThrottleClutchEngaged = M2ToCm2(Setup.DampingRateZeroThrottleClutchEngaged);
	PxSetup.mDampingRateZeroThrottleClutchDisengaged = M2ToCm2(Setup.DampingRateZeroThrottleClutchDisengaged);

	float PeakTorque = Setup.FindPeakTorque();  // In Nm.
	PxSetup.mPeakTorque = M2ToCm2(PeakTorque);	// Convert Nm to (kg cm^2/s^2).

												// Convert from our curve to PhysX.
	PxSetup.mTorqueCurve.clear();
	TArray<FRichCurveKey> TorqueKeys = Setup.TorqueCurve.GetRichCurveConst()->GetCopyOfKeys();
	int32 NumTorqueCurveKeys = FMath::Min<int32>(TorqueKeys.Num(), PxVehicleEngineData::eMAX_NB_ENGINE_TORQUE_CURVE_ENTRIES);
	for (int32 KeyIdx = 0; KeyIdx < NumTorqueCurveKeys; KeyIdx++)
	{
		FRichCurveKey& Key = TorqueKeys[KeyIdx];
		PxSetup.mTorqueCurve.addPair(FMath::Clamp(Key.Time / Setup.MaxRPM, 0.f, 1.f), Key.Value / PeakTorque); // Normalize torque to 0-1 range
	}
}

static void GetVehicleGearSetup(const FVehicleTransmissionNWData& Setup, PxVehicleGearsData& PxSetup)
{
	PxSetup.mSwitchTime = Setup.GearSwitchTime;
	PxSetup.mRatios[PxVehicleGearsData::eREVERSE] = Setup.ReverseGearRatio;
	for (int32 i = 0; i < Setup.ForwardGears.Num(); i++)
	{
		PxSetup.mRatios[i + PxVehicleGearsData::eFIRST] = Setup.ForwardGears[i].Ratio;
	}
	PxSetup.mFinalRatio = Setup.FinalRatio;
	PxSetup.mNbRatios = Setup.ForwardGears.Num() + PxVehicleGearsData::eFIRST;
}

static void GetVehicleAutoBoxSetup(const FVehicleTransmissionNWData& Setup, PxVehicleAutoBoxData& PxSetup)
{
	for (int32 i = 0; i < Setup.ForwardGears.Num(); i++)
	{
		const FVehicleGearNWData& GearData = Setup.ForwardGears[i];
		PxSetup.mUpRatios[i] = GearData.UpRatio;
		PxSetup.mDownRatios[i] = GearData.DownRatio;
	}
	PxSetup.mUpRatios[PxVehicleGearsData::eNEUTRAL] = Setup.NeutralGearUpRatio;
	PxSetup.setLatency(Setup.GearAutoBoxLatency);
}

void SetupDriveHelper(const UVehicleMovementComponentNW* VehicleData, const PxVehicleWheelsSimData* PWheelsSimData, PxVehicleDriveSimDataNW& DriveData)
{
	PxVehicleDifferentialNWData DifferentialSetup;
	GetVehicleDifferentialNWSetup(VehicleData->DifferentialSetup, DifferentialSetup);
	DriveData.setDiffData(DifferentialSetup);

	PxVehicleEngineData EngineSetup;
	GetVehicleEngineSetup(VehicleData->EngineSetup, EngineSetup);
	DriveData.setEngineData(EngineSetup);

	PxVehicleClutchData ClutchSetup;
	ClutchSetup.mStrength = M2ToCm2(VehicleData->TransmissionSetup.ClutchStrength);
	DriveData.setClutchData(ClutchSetup);

	PxVehicleGearsData GearSetup;
	GetVehicleGearSetup(VehicleData->TransmissionSetup, GearSetup);
	DriveData.setGearsData(GearSetup);

	PxVehicleAutoBoxData AutoBoxSetup;
	GetVehicleAutoBoxSetup(VehicleData->TransmissionSetup, AutoBoxSetup);
	DriveData.setAutoBoxData(AutoBoxSetup);
}

void UVehicleMovementComponentNW::SetupVehicle()
{
	if (!UpdatedPrimitive)
	{
		return;
	}

	if (WheelSetups.Num() > 20 || WheelSetups.Num() < 2)
	{
		PVehicle = nullptr;
		PVehicleDrive = nullptr;
		return;
	}

	for (int32 WheelIdx = 0; WheelIdx < WheelSetups.Num(); ++WheelIdx)
	{
		const FWheelSetup& WheelSetup = WheelSetups[WheelIdx];
		if (WheelSetup.BoneName == NAME_None)
		{
			return;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("OK SetupVehicle"));

	// Setup the chassis and wheel shapes.
	SetupVehicleShapes();

	// Setup mass properties.
	SetupVehicleMass();

	// Setup the wheels.
	PxVehicleWheelsSimData* PWheelsSimData = PxVehicleWheelsSimData::allocate(NumOfWheels);
	SetupWheels(PWheelsSimData);

	// Setup drive data.
	PxVehicleDriveSimDataNW DriveData;
	SetupDriveHelper(this, PWheelsSimData, DriveData);

	// Create the vehicle.
	PxVehicleDriveNW* PVehicleDriveNW = PxVehicleDriveNW::allocate(NumOfWheels);
	check(PVehicleDriveNW);

	FBodyInstance* BodyInstance = UpdatedPrimitive->GetBodyInstance();
	FPhysicsCommand::ExecuteWrite(BodyInstance->ActorHandle, [&] (const FPhysicsActorHandle &Actor) {
		PxRigidDynamic* PRigidDynamic = FPhysicsInterface::GetPxRigidDynamic_AssumesLocked(Actor);

		if (!PRigidDynamic) {
			return ;
		}

		PVehicleDriveNW->setup(GPhysXSDK, PRigidDynamic, *PWheelsSimData, DriveData, 0);
		PVehicleDriveNW->setToRestState();

		// Cleanup.
		PWheelsSimData->free();
	});

	PWheelsSimData = nullptr;

	// Cache values.
	PVehicle = PVehicleDriveNW;
	PVehicleDrive = PVehicleDriveNW;

	SetUseAutoGears(TransmissionSetup.bUseGearAutoBox);
}

void UVehicleMovementComponentNW::UpdateSimulation(float DeltaTime)
{
	if (PVehicleDrive == nullptr)
		return;

	FBodyInstance *BodyInstance = UpdatedPrimitive->GetBodyInstance();
	FPhysicsCommand::ExecuteWrite(BodyInstance->ActorHandle, [&] (const FPhysicsActorHandle &) {
		PxVehicleDriveNWRawInputData RawInputData;
		RawInputData.setAnalogAccel(ThrottleInput);
		RawInputData.setAnalogSteer(SteeringInput);
		RawInputData.setAnalogBrake(BrakeInput);
		RawInputData.setAnalogHandbrake(HandbrakeInput);

		if (!PVehicleDrive->mDriveDynData.getUseAutoGears())
		{
			RawInputData.setGearUp(bRawGearUpInput);
			RawInputData.setGearDown(bRawGearDownInput);
		}

		// Convert from our curve to PxFixedSizeLookupTable
		PxFixedSizeLookupTable<8> SpeedSteerLookup;
		TArray<FRichCurveKey> SteerKeys = SteeringCurve.GetRichCurve()->GetCopyOfKeys();
		const int32 MaxSteeringSamples = FMath::Min(8, SteerKeys.Num());
		for (int32 KeyIdx = 0; KeyIdx < MaxSteeringSamples; KeyIdx++)
		{
			FRichCurveKey& Key = SteerKeys[KeyIdx];
			SpeedSteerLookup.addPair(KmHToCmS(Key.Time), FMath::Clamp(Key.Value, 0.f, 1.f));
		}

		PxVehiclePadSmoothingData SmoothData =
		{
			{ ThrottleInputRate.RiseRate, BrakeInputRate.RiseRate, HandbrakeInputRate.RiseRate, SteeringInputRate.RiseRate, SteeringInputRate.RiseRate },
			{ ThrottleInputRate.FallRate, BrakeInputRate.FallRate, HandbrakeInputRate.FallRate, SteeringInputRate.FallRate, SteeringInputRate.FallRate }
		};

		PxVehicleDriveNW* PVehicleDriveNW = (PxVehicleDriveNW*)PVehicleDrive;
		PxVehicleDriveNWSmoothAnalogRawInputsAndSetAnalogInputs(SmoothData, SpeedSteerLookup, RawInputData, DeltaTime, false, *PVehicleDriveNW);
	});
}
#endif // WITH_PHYSX_VEHICLES

void UVehicleMovementComponentNW::UpdateEngineSetup(const FVehicleEngineNWData& NewEngineSetup)
{
#if WITH_PHYSX_VEHICLES
	if (PVehicleDrive)
	{
		PxVehicleEngineData EngineData;
		GetVehicleEngineSetup(NewEngineSetup, EngineData);

		PxVehicleDriveNW* PVehicleDriveNW = (PxVehicleDriveNW*)PVehicleDrive;
		PVehicleDriveNW->mDriveSimData.setEngineData(EngineData);
	}
#endif // WITH_PHYSX_VEHICLES
}

void UVehicleMovementComponentNW::UpdateDifferentialSetup(const FVehicleDifferentialNWData& NewDifferentialSetup)
{
#if WITH_PHYSX_VEHICLES
	if (PVehicleDrive)
	{
		PxVehicleDifferentialNWData DifferentialData;
		GetVehicleDifferentialNWSetup(NewDifferentialSetup, DifferentialData);

		PxVehicleDriveNW* PVehicleDriveNW = (PxVehicleDriveNW*)PVehicleDrive;
		PVehicleDriveNW->mDriveSimData.setDiffData(DifferentialData);
	}
#endif
}

void UVehicleMovementComponentNW::UpdateTransmissionSetup(const FVehicleTransmissionNWData& NewTransmissionSetup)
{
#if WITH_PHYSX_VEHICLES
	if (PVehicleDrive)
	{
		PxVehicleGearsData GearData;
		GetVehicleGearSetup(NewTransmissionSetup, GearData);

		PxVehicleAutoBoxData AutoBoxData;
		GetVehicleAutoBoxSetup(NewTransmissionSetup, AutoBoxData);

		PxVehicleDriveNW* PVehicleDriveNW = (PxVehicleDriveNW*)PVehicleDrive;
		PVehicleDriveNW->mDriveSimData.setGearsData(GearData);
		PVehicleDriveNW->mDriveSimData.setAutoBoxData(AutoBoxData);
	}
#endif
}

void BackwardsConvertCm2ToM2(float& val, float defaultValue)
{
	if (val != defaultValue)
	{
		val = Cm2ToM2(val);
	}
}

void UVehicleMovementComponentNW::Serialize(FArchive & Ar)
{
	Super::Serialize(Ar);
#if WITH_PHYSX_VEHICLES
	if (Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_VEHICLES_UNIT_CHANGE)
	{
		PxVehicleEngineData DefEngineData;
		float DefaultRPM = OmegaToRPM(DefEngineData.mMaxOmega);

		// We need to convert from old units to new. This backwards compatible code fails in the rare case that they were using very strange values that are the new defaults in the correct units.
		EngineSetup.MaxRPM = EngineSetup.MaxRPM != DefaultRPM ? OmegaToRPM(EngineSetup.MaxRPM) : DefaultRPM;	// Need to convert from rad/s to RPM.
	}

	if (Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_VEHICLES_UNIT_CHANGE2)
	{
		PxVehicleEngineData DefEngineData;
		PxVehicleClutchData DefClutchData;

		// We need to convert from old units to new. This backwards compatible code fails in the rare case that they were using very strange values that are the new defaults in the correct units.
		BackwardsConvertCm2ToM2(EngineSetup.DampingRateFullThrottle, DefEngineData.mDampingRateFullThrottle);
		BackwardsConvertCm2ToM2(EngineSetup.DampingRateZeroThrottleClutchDisengaged, DefEngineData.mDampingRateZeroThrottleClutchDisengaged);
		BackwardsConvertCm2ToM2(EngineSetup.DampingRateZeroThrottleClutchEngaged, DefEngineData.mDampingRateZeroThrottleClutchEngaged);
		BackwardsConvertCm2ToM2(EngineSetup.MOI, DefEngineData.mMOI);
		BackwardsConvertCm2ToM2(TransmissionSetup.ClutchStrength, DefClutchData.mStrength);
	}
#endif
}

void UVehicleMovementComponentNW::ComputeConstants()
{
	Super::ComputeConstants();
	MaxEngineRPM = EngineSetup.MaxRPM;
}