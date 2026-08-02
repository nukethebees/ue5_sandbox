#pragma once

#include "CoreMinimal.h"

#include "LaserFiringState.generated.h"

UENUM()
enum class ELaserFiringState : uint8 {
    idle,
    burst,
    lock_on_transition,
    lock_on_searching,
    lock_on_acquired
};
