// Copyright Unreal Engine Community.

#pragma once

#include "CoreMinimal.h"
#include "WheeledVehicleMovementComponent.h"
#include "Curves/CurveFloat.h"
#include "VehicleMovementComponentNW.generated.h"

#if WITH_PHYSX_VEHICLES
namespace physx
{
	class PxVehicleDriveNW;
}
#endif // WITH_PHYSX_VEHICLES

USTRUCT()
struct FDrivenWheelData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = Setup)
	int32 DrivenWheelIndex;

	UPROPERTY(EditAnywhere, Category = Setup)
		bool IsDrivenWheel;
};

USTRUCT()
struct FVehicleDifferentialNWData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = Setup, AdvancedDisplay)
	TArray<FDrivenWheelData> DWheelData;
};

USTRUCT()
struct FVehicleEngineNWData
{
	GENERATED_USTRUCT_BODY()

	// Torque (Nm) at a given RPM.
	UPROPERTY(EditAnywhere, Category = Setup)
	FRuntimeFloatCurve TorqueCurve;

	// Maximum revolutions per minute of the engine.
	UPROPERTY(EditAnywhere, Category = Setup, meta = (ClampMin = "0.01", UIMin = "0.01"))
		float MaxRPM;

	// Moment of inertia of the engine around the axis of rotation (Kgm^2). 
	UPROPERTY(EditAnywhere, Category = Setup, meta = (ClampMin = "0.01", UIMin = "0.01"))
		float MOI;

	// Damping rate of engine when full throttle is applied (Kgm^2/s).
	UPROPERTY(EditAnywhere, Category = Setup, AdvancedDisplay, meta = (ClampMin = "0.0", UIMin = "0.0"))
		float DampingRateFullThrottle;

	// Damping rate of engine in at zero throttle when the clutch is engaged (Kgm^2/s).
	UPROPERTY(EditAnywhere, Category = Setup, AdvancedDisplay, meta = (ClampMin = "0.0", UIMin = "0.0"))
		float DampingRateZeroThrottleClutchEngaged;

	// Damping rate of engine in at zero throttle when the clutch is disengaged (in neutral gear) (Kgm^2/s).
	UPROPERTY(EditAnywhere, Category = Setup, AdvancedDisplay, meta = (ClampMin = "0.0", UIMin = "0.0"))
		float DampingRateZeroThrottleClutchDisengaged;

	// Find the peak torque produced by the TorqueCurve.
	float FindPeakTorque() const;
};

USTRUCT()
struct FVehicleGearNWData
{
	GENERATED_USTRUCT_BODY()

		// Determines the amount of torque multiplication.
	UPROPERTY(EditAnywhere, Category = Setup)
    	float Ratio;

	// Value of engineRevs/maxEngineRevs that is low enough to gear down.
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "1.0", UIMax = "1.0"), Category = Setup)
		float DownRatio;

	// Value of engineRevs/maxEngineRevs that is high enough to gear up.
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "1.0", UIMax = "1.0"), Category = Setup)
		float UpRatio;
};

USTRUCT()
struct FVehicleTransmissionNWData
{
	GENERATED_USTRUCT_BODY()

	// Whether to use automatic transmission.
	UPROPERTY(EditAnywhere, Category = VehicleSetup, meta = (DisplayName = "Automatic Transmission"))
		bool bUseGearAutoBox;

	// Time it takes to switch gears (seconds).
	UPROPERTY(EditAnywhere, Category = Setup, meta = (ClampMin = "0.0", UIMin = "0.0"))
		float GearSwitchTime;

	// Minimum time it takes the automatic transmission to initiate a gear change (seconds).
	UPROPERTY(EditAnywhere, Category = Setup, meta = (editcondition = "bUseGearAutoBox", ClampMin = "0.0", UIMin = "0.0"))
		float GearAutoBoxLatency;

	// The final gear ratio multiplies the transmission gear ratios.
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Setup)
		float FinalRatio;

	// Forward gear ratios (up to 30).
	UPROPERTY(EditAnywhere, Category = Setup, AdvancedDisplay)
		TArray<FVehicleGearNWData> ForwardGears;

	// Reverse gear ratio.
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Setup)
		float ReverseGearRatio;

	// Value of engineRevs/maxEngineRevs that is high enough to increment gear.
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Setup, meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "1.0", UIMax = "1.0"))
		float NeutralGearUpRatio;

	// Strength of clutch (Kgm^2/s).
	UPROPERTY(EditAnywhere, Category = Setup, AdvancedDisplay, meta = (ClampMin = "0.0", UIMin = "0.0"))
		float ClutchStrength;
};

UCLASS(ClassGroup = (Physics), meta = (BlueprintSpawnableComponent), hidecategories = (PlanarMovement, "Components|Movement|Planar", Activation, "Components|Activation"))
class MYVEHICLEPROJECT_API UVehicleMovementComponentNW : public UWheeledVehicleMovementComponent
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = Vehicle)
		int32 NumOfWheels;

	// Engine.
	UPROPERTY(EditAnywhere, Category = MechanicalSetup)
		FVehicleEngineNWData EngineSetup;

	// Differential.
	UPROPERTY(EditAnywhere, Category = MechanicalSetup)
		FVehicleDifferentialNWData DifferentialSetup;

	//UPROPERTY(EditAnywhere, Category = MechanicalSetup).
	FDrivenWheelData DrivenWheelSetup;

	// Transmission Data.
	UPROPERTY(EditAnywhere, Category = MechanicalSetup)
		FVehicleTransmissionNWData TransmissionSetup;

	// Maximum steering versus forward speed (Km/h).
	UPROPERTY(EditAnywhere, Category = SteeringSetup)
		FRuntimeFloatCurve SteeringCurve;

	virtual void Serialize(FArchive & Ar) override;
	virtual void ComputeConstants() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:

#if WITH_PHYSX_VEHICLES

	// Allocate and setup the PhysX vehicle.
	virtual void SetupVehicle() override;
	virtual void UpdateSimulation(float DeltaTime) override;


#endif // WITH_PHYSX_VEHICLES

	// Update simulation data: engine.
	void UpdateEngineSetup(const FVehicleEngineNWData& NewEngineSetup);

	// Update Simulation data: differential.
	void UpdateDifferentialSetup(const FVehicleDifferentialNWData& NewDifferentialSetup);

	// Update simulation data: transmission.
	void UpdateTransmissionSetup(const FVehicleTransmissionNWData& NewGearSetup);
};