// Copyright (c) 2017 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#include "Carla.h"
#include "TireConfig.h"
#include "CarlaWheeledVehicle.h"
#include "VehicleWheel.h"

#include "Agent/VehicleAgentComponent.h"

#include "Components/BoxComponent.h"
#include "Engine/CollisionProfile.h"
// =============================================================================
// -- Constructor and destructor -----------------------------------------------
// =============================================================================

ACarlaWheeledVehicle::ACarlaWheeledVehicle(const FObjectInitializer& ObjectInitializer) :
  Super(ObjectInitializer)
{
  VehicleBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("VehicleBounds"));
  VehicleBounds->SetupAttachment(RootComponent);
  VehicleBounds->SetHiddenInGame(true);
  VehicleBounds->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

  VehicleAgentComponent = CreateDefaultSubobject<UVehicleAgentComponent>(TEXT("VehicleAgentComponent"));
  VehicleAgentComponent->SetupAttachment(RootComponent);

  GetVehicleMovementComponent()->bReverseAsBrake = false;
}

ACarlaWheeledVehicle::~ACarlaWheeledVehicle() {}

void ACarlaWheeledVehicle::BeginPlay()
{
  Super::BeginPlay();

  // Check if it overlaps with a Friction trigger
  float FrictionScale = 3.5f;
  TArray<AActor *> OverlapActors;
  GetOverlappingActors(OverlapActors, AFrictionTrigger::StaticClass());
  for (const auto &Actor : OverlapActors)
  {
    AFrictionTrigger *FrictionTrigger = Cast<AFrictionTrigger>(Actor);
    if (FrictionTrigger)
    {
      FrictionScale = FrictionTrigger->Friction;
    }
  }

  // Setup Tire Configs
  UWheeledVehicleMovementComponent4W *Vehicle4W = Cast<UWheeledVehicleMovementComponent4W>(
      GetVehicleMovementComponent());
  check(Vehicle4W != nullptr);

  // Wheels Setup
  TArray<FWheelSetup> NewWheelSetups = Vehicle4W->WheelSetups;

  for (const auto &WheelSetup : NewWheelSetups)
  {
    UVehicleWheel *Wheel = WheelSetup.WheelClass.GetDefaultObject();
    check(Wheel != nullptr);

    // Assigning new tire config
    Wheel->TireConfig = NewObject<UTireConfig>();

    // Setting a new value to friction
    Wheel->TireConfig->SetFrictionScale(FrictionScale);
  }

  Vehicle4W->WheelSetups = NewWheelSetups;
  Vehicle4W->RecreatePhysicsState();
}

// =============================================================================
// -- Get functions ------------------------------------------------------------
// =============================================================================

float ACarlaWheeledVehicle::GetVehicleForwardSpeed() const
{
  return GetVehicleMovementComponent()->GetForwardSpeed();
}

FVector ACarlaWheeledVehicle::GetVehicleOrientation() const
{
  return GetVehicleTransform().GetRotation().GetForwardVector();
}

int32 ACarlaWheeledVehicle::GetVehicleCurrentGear() const
{
  return GetVehicleMovementComponent()->GetCurrentGear();
}

FTransform ACarlaWheeledVehicle::GetVehicleBoundingBoxTransform() const
{
  return VehicleBounds->GetRelativeTransform();
}

FVector ACarlaWheeledVehicle::GetVehicleBoundingBoxExtent() const
{
  return VehicleBounds->GetScaledBoxExtent();
}

float ACarlaWheeledVehicle::GetMaximumSteerAngle() const
{
  const auto &Wheels = GetVehicleMovementComponent()->Wheels;
  check(Wheels.Num() > 0);
  const auto *FrontWheel = Wheels[0];
  check(FrontWheel != nullptr);
  return FrontWheel->SteerAngle;
}

// =============================================================================
// -- Set functions ------------------------------------------------------------
// =============================================================================

void ACarlaWheeledVehicle::FlushVehicleControl()
{
  auto *MovementComponent = GetVehicleMovementComponent();
  MovementComponent->SetThrottleInput(InputControl.Control.Throttle);
  MovementComponent->SetSteeringInput(InputControl.Control.Steer);
  MovementComponent->SetBrakeInput(InputControl.Control.Brake);
  MovementComponent->SetHandbrakeInput(InputControl.Control.bHandBrake);
  if (LastAppliedControl.bReverse != InputControl.Control.bReverse)
  {
    MovementComponent->SetUseAutoGears(!InputControl.Control.bReverse);
    MovementComponent->SetTargetGear(InputControl.Control.bReverse ? -1 : 1, true);
  }
  else
  {
    MovementComponent->SetUseAutoGears(!InputControl.Control.bManualGearShift);
    if (InputControl.Control.bManualGearShift)
    {
      MovementComponent->SetTargetGear(InputControl.Control.Gear, true);
    }
  }
  InputControl.Control.Gear = MovementComponent->GetCurrentGear();
  InputControl.Control.bReverse = InputControl.Control.Gear < 0;
  LastAppliedControl = InputControl.Control;
  InputControl.Priority = EVehicleInputPriority::INVALID;
}

void ACarlaWheeledVehicle::SetThrottleInput(const float Value)
{
  FVehicleControl Control = InputControl.Control;
  Control.Throttle = Value;
  ApplyVehicleControl(Control, EVehicleInputPriority::User);
}

void ACarlaWheeledVehicle::SetSteeringInput(const float Value)
{
  FVehicleControl Control = InputControl.Control;
  Control.Steer = Value;
  ApplyVehicleControl(Control, EVehicleInputPriority::User);
}

void ACarlaWheeledVehicle::SetBrakeInput(const float Value)
{
  FVehicleControl Control = InputControl.Control;
  Control.Brake = Value;
  ApplyVehicleControl(Control, EVehicleInputPriority::User);
}

void ACarlaWheeledVehicle::SetReverse(const bool Value)
{
  FVehicleControl Control = InputControl.Control;
  Control.bReverse = Value;
  ApplyVehicleControl(Control, EVehicleInputPriority::User);
}

void ACarlaWheeledVehicle::SetHandbrakeInput(const bool Value)
{
  FVehicleControl Control = InputControl.Control;
  Control.bHandBrake = Value;
  ApplyVehicleControl(Control, EVehicleInputPriority::User);
}

TArray<float> ACarlaWheeledVehicle::GetWheelsFrictionScale()
{
  UWheeledVehicleMovementComponent4W *Vehicle4W = Cast<UWheeledVehicleMovementComponent4W>(
      GetVehicleMovement());
  check(Vehicle4W != nullptr);

  TArray<float> WheelsFrictionScale;
  for (auto &Wheel : Vehicle4W->Wheels)
  {
    WheelsFrictionScale.Add(Wheel->TireConfig->GetFrictionScale());
  }
  return WheelsFrictionScale;
}

void ACarlaWheeledVehicle::SetWheelsFrictionScale(TArray<float> &WheelsFrictionScale)
{
  UWheeledVehicleMovementComponent4W *Vehicle4W = Cast<UWheeledVehicleMovementComponent4W>(
      GetVehicleMovement());
  check(Vehicle4W != nullptr);

  for (int32 i = 0; i < Vehicle4W->Wheels.Num(); ++i)
  {
    Vehicle4W->Wheels[i]->TireConfig->SetFrictionScale(WheelsFrictionScale[i]);
  }
}

FVehiclePhysicsControl ACarlaWheeledVehicle::GetVehiclePhysicsControl()
{
  UWheeledVehicleMovementComponent4W *Vehicle4W = Cast<UWheeledVehicleMovementComponent4W>(
      GetVehicleMovement());
  check(Vehicle4W != nullptr);

  FVehiclePhysicsControl PhysicsControl;

  // Engine Setup
  PhysicsControl.TorqueCurve = Vehicle4W->EngineSetup.TorqueCurve.EditorCurveData;
  PhysicsControl.MaxRPM = Vehicle4W->EngineSetup.MaxRPM;
  PhysicsControl.MOI = Vehicle4W->EngineSetup.MOI;
  PhysicsControl.DampingRateFullThrottle = Vehicle4W->EngineSetup.DampingRateFullThrottle;
  PhysicsControl.DampingRateZeroThrottleClutchEngaged =
      Vehicle4W->EngineSetup.DampingRateZeroThrottleClutchEngaged;
  PhysicsControl.DampingRateZeroThrottleClutchDisengaged =
      Vehicle4W->EngineSetup.DampingRateZeroThrottleClutchDisengaged;

  // Transmission Setup
  PhysicsControl.bUseGearAutoBox = Vehicle4W->TransmissionSetup.bUseGearAutoBox;
  PhysicsControl.GearSwitchTime = Vehicle4W->TransmissionSetup.GearSwitchTime;
  PhysicsControl.ClutchStrength = Vehicle4W->TransmissionSetup.ClutchStrength;

  // Vehicle Setup
  PhysicsControl.Mass = Vehicle4W->Mass;
  PhysicsControl.DragCoefficient = Vehicle4W->DragCoefficient;

  // Center of mass offset (Center of mass is always zero vector in local
  // position)
  UPrimitiveComponent *UpdatedPrimitive = Cast<UPrimitiveComponent>(Vehicle4W->UpdatedComponent);
  check(UpdatedPrimitive != nullptr);

  PhysicsControl.CenterOfMass = UpdatedPrimitive->BodyInstance.COMNudge;

  // Transmission Setup
  PhysicsControl.SteeringCurve = Vehicle4W->SteeringCurve.EditorCurveData;

  // Wheels Setup
  TArray<FWheelPhysicsControl> Wheels;

  for (int32 i = 0; i < Vehicle4W->WheelSetups.Num(); ++i)
  {
    FWheelPhysicsControl PhysicsWheel;

    UVehicleWheel *Wheel = Vehicle4W->WheelSetups[i].WheelClass.GetDefaultObject();
    check(Wheel != nullptr);

    PhysicsWheel.TireFriction = Wheel->TireConfig->GetFrictionScale();
    PhysicsWheel.DampingRate = Wheel->DampingRate;
    PhysicsWheel.MaxSteerAngle = Wheel->SteerAngle;
    PhysicsWheel.IsSteerable = !Vehicle4W->WheelSetups[i].bDisableSteering;

    Wheels.Add(PhysicsWheel);
  }

  PhysicsControl.Wheels = Wheels;

  return PhysicsControl;
}

void ACarlaWheeledVehicle::ApplyVehiclePhysicsControl(const FVehiclePhysicsControl &PhysicsControl)
{
  UWheeledVehicleMovementComponent4W *Vehicle4W = Cast<UWheeledVehicleMovementComponent4W>(
      GetVehicleMovement());
  check(Vehicle4W != nullptr);

  // Engine Setup
  Vehicle4W->EngineSetup.TorqueCurve.EditorCurveData = PhysicsControl.TorqueCurve;
  Vehicle4W->EngineSetup.MaxRPM = PhysicsControl.MaxRPM;

  Vehicle4W->EngineSetup.MOI = PhysicsControl.MOI;

  Vehicle4W->EngineSetup.DampingRateFullThrottle = PhysicsControl.DampingRateFullThrottle;
  Vehicle4W->EngineSetup.DampingRateZeroThrottleClutchEngaged =
      PhysicsControl.DampingRateZeroThrottleClutchEngaged;
  Vehicle4W->EngineSetup.DampingRateZeroThrottleClutchDisengaged =
      PhysicsControl.DampingRateZeroThrottleClutchDisengaged;

  // Transmission Setup
  Vehicle4W->TransmissionSetup.bUseGearAutoBox = PhysicsControl.bUseGearAutoBox;
  Vehicle4W->TransmissionSetup.GearSwitchTime = PhysicsControl.GearSwitchTime;
  Vehicle4W->TransmissionSetup.ClutchStrength = PhysicsControl.ClutchStrength;

  // Vehicle Setup
  Vehicle4W->Mass = PhysicsControl.Mass;
  Vehicle4W->DragCoefficient = PhysicsControl.DragCoefficient;

  // Center of mass
  UPrimitiveComponent *UpdatedPrimitive = Cast<UPrimitiveComponent>(Vehicle4W->UpdatedComponent);
  check(UpdatedPrimitive != nullptr);

  UpdatedPrimitive->BodyInstance.COMNudge = PhysicsControl.CenterOfMass;

  // Transmission Setup
  Vehicle4W->SteeringCurve.EditorCurveData = PhysicsControl.SteeringCurve;

  // Wheels Setup
  const int PhysicsWheelsNum = PhysicsControl.Wheels.Num();
  if (PhysicsWheelsNum != 4) {
    UE_LOG(LogCarla, Error, TEXT("Number of WheelPhysicsControl is not 4."));
    return;
  }

  TArray<FWheelSetup> NewWheelSetups;
  for (auto i = 0u; i < PhysicsWheelsNum; ++i)
  {
    FWheelSetup WheelSetup = Vehicle4W->WheelSetups[i];

    UVehicleWheel *Wheel = WheelSetup.WheelClass.GetDefaultObject();
    check(Wheel != nullptr);

    Wheel->DampingRate = PhysicsControl.Wheels[i].DampingRate;
    Wheel->SteerAngle = PhysicsControl.Wheels[i].MaxSteerAngle;
    Wheel->TireConfig->SetFrictionScale(PhysicsControl.Wheels[i].TireFriction);

    WheelSetup.bDisableSteering = !PhysicsControl.Wheels[i].IsSteerable;

    NewWheelSetups.Add(std::move(WheelSetup));
  }

  Vehicle4W->WheelSetups = NewWheelSetups;
  Vehicle4W->RecreatePhysicsState();
}
