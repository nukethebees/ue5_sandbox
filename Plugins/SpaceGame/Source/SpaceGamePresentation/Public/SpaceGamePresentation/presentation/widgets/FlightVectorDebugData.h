#pragma once

#include <CoreMinimal.h>

namespace ml::ship_hud {

struct FFlightVectorDebugData {
    bool operator==(FFlightVectorDebugData const& other) const noexcept = default;

    FVector2D turn_input{};
    FVector2D move_input{};
    FVector2D target_velocity{};
    FVector2D local_velocity{};
};

} // namespace ml::ship_hud
