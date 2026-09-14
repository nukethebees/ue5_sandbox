#pragma once

#include <cstdint>
#include <ioj/sim/player/space_ship_common.h>

namespace ioj::sim::player {
enum class SpeedResponseKind : std::uint8_t { AcceleratingToCruise, SlowingToCruise, Boost, Brake };

struct ThrustSettings {
    float cruise_speed;
    float boost_speed;
    float brake_speed;
    float thrust_recharge_time;
    float boost_depletion_time;
    float brake_depletion_time;
    float boost_forward_speed_addition_multiplier;
};

struct ThrustTransition {
    float target_speed;
    float energy_change_rate;
    float planar_boost_target;
    SpeedResponseKind speed_response;
};

[[nodiscard]] auto make_thrust_transition(ThrustSettings settings,
                                          BoostBrakeState state,
                                          float current_speed) noexcept -> ThrustTransition;
}
