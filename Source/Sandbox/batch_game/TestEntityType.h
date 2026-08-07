#pragma once

#include "CoreMinimal.h"

#include "TestEntityType.generated.h"

UENUM()
enum class ETestEntityType : uint8 {
    PlayerShip,
    Turret,
    CapitalShip,
    CapitalShipFighter,
    TubeSpinner,
    COUNT UMETA(Hidden),
};

namespace ml {
auto get_entity_display_name(ETestEntityType const type) -> FString const&;
auto get_entity_class_name(ETestEntityType const type) -> FString const&;
auto get_entity_short_name(ETestEntityType const type) -> FString const&;
}
