#pragma once

#include <ioj/presentation/flight_vector_debug_data.h>
#include <ioj/sim/player/space_ship_common.h>
#include <ioj/vector.h>

#include <string>

namespace ioj::presentation {
struct PlayerFlightHudData {
    auto operator==(PlayerFlightHudData const&) const -> bool = default;

    FlightVectorDebugData flight_vector_debug{};
    Vector3d ship_velocity{};
    Vector3d target_velocity{};
    float throttle{};
    sim::player::BoostBrakeState boost_brake_state{};
    Vector3d crosshair_origin{};
    Vector3d crosshair_direction{};
};
}
