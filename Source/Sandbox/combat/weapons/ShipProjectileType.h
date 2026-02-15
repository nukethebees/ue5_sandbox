#pragma once

#include "CoreMinimal.h"

#include "ShipProjectileType.generated.h"

UENUM(BlueprintType)
enum class EShipProjectileType : uint8 { laser, homing_laser, bomb };
