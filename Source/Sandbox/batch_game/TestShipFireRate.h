#pragma once

#include <Sandbox/utilities/enums.h>

#include "CoreMinimal.h"

#include <utility>

#include "TestShipFireRate.generated.h"

UENUM()
enum class ETestShipFireRate : uint8 {
    Single,
    Burst3,
    FullAuto,
    COUNT UMETA(Hidden),
};

DECLARE_DELEGATE_OneParam(FOnShipFireRateChanged, ETestShipFireRate);
