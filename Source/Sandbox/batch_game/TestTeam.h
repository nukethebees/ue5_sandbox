#pragma once

#include "CoreMinimal.h"

#include "TestTeam.generated.h"

UENUM()
enum class ETestTeam : uint8 {
    neutral,
    foo,
    bar,
    baz,
    COUNT UMETA(DisplayName = "Count", Hidden),
};
