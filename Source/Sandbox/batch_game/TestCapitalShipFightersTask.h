#pragma once

#include <Sandbox/utilities/enums.h>

#include "CoreMinimal.h"

#include "TestCapitalShipFightersTask.generated.h"

UENUM()
enum class ETestCapitalShipFightersTask : uint8 {
    Standby,
    MoveToDestination,
    Attack,
    COUNT UMETA(DisplayName = "Count", Hidden),
};

inline auto LexToString(ETestCapitalShipFightersTask const task) -> FString {
    return ml::to_string_without_type_prefix(task);
}
