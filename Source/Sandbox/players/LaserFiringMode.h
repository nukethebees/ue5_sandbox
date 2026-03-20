#pragma once

#include "CoreMinimal.h"

#include "LaserFiringMode.generated.h"

UENUM()
enum class ELaserFiringMode : uint8 { idle, burst, lock_on_transition, lock_on };
