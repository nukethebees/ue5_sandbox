#pragma once

#include "CoreMinimal.h"

#include "TestShipFireRate.generated.h"

UENUM()
enum class ETestShipFireRate : uint8 {
    Single,
    Burst3,
    FullAuto,
};
