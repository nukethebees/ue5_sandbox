#pragma once

#include <CoreMinimal.h>

#include "TestDeathReason.generated.h"

UENUM()
enum class ETestDeathReason : uint8 {
    Unset,
    Unknown,
    Combat,
};
