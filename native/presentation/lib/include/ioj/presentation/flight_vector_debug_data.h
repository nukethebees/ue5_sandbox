#pragma once

#include <ioj/vector.h>

namespace ioj::presentation {
struct FlightVectorDebugData {
    auto operator==(FlightVectorDebugData const&) const -> bool = default;

    Vector2f turn_input{};
    Vector2f local_velocity{};
};
}
