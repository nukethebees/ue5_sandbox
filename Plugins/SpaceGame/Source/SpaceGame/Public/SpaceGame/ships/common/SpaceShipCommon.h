#pragma once

#include "SpaceGame/ships/common/LaserFiringState.h"

#include <CoreMinimal.h>

#include "SpaceShipCommon.generated.h"

class AActor;

UENUM()
enum class EBoostBrakeState : uint8 { None, Boost, Brake };

DECLARE_DELEGATE_OneParam(FOnShipSpeedChanged, float);
DECLARE_DELEGATE_OneParam(FOnShipTargetSpeedChanged, float);
DECLARE_DELEGATE_OneParam(FOnShipEnergyChanged, float);
DECLARE_DELEGATE_OneParam(FOnShipGoldRingsChanged, int32);
DECLARE_DELEGATE_OneParam(FOnShipPointsChanged, int32);
DECLARE_DELEGATE_OneParam(FOnLivesChanged, int32);
DECLARE_DELEGATE_TwoParams(FOnSpeedSampled, TConstArrayView<FVector2d>, int32);
DECLARE_DELEGATE_OneParam(FOnLaserModeChanged, ELaserFiringState);
DECLARE_DELEGATE_OneParam(FOnLockOnAcquired, AActor*);
