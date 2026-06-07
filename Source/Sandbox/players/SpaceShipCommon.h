#pragma once

#include "Sandbox/players/LaserFiringMode.h"

#include <CoreMinimal.h>

#include <span>

#include "SpaceShipCommon.generated.h"

class AActor;

UENUM()
enum class EBoostBrakeState : uint8 { None, Boost, Brake };

DECLARE_DELEGATE_OneParam(FOnShipSpeedChanged, float);
DECLARE_DELEGATE_OneParam(FOnShipEnergyChanged, float);
DECLARE_DELEGATE_OneParam(FOnShipBombsChanged, int32);
DECLARE_DELEGATE_OneParam(FOnShipGoldRingsChanged, int32);
DECLARE_DELEGATE_OneParam(FOnShipPointsChanged, int32);
DECLARE_DELEGATE_OneParam(FOnLivesChanged, int32);
DECLARE_DELEGATE_TwoParams(FOnSpeedSampled, std::span<FVector2d>, int32);
DECLARE_DELEGATE_OneParam(FOnLaserModeChanged, ELaserFiringMode);
DECLARE_DELEGATE_OneParam(FOnLockOnAcquired, AActor*);
