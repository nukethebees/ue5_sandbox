#pragma once
#include <algorithm>
#include <cstdint>
#include <ioj/sim/player/laser_firing_state.h>
#include <ioj/sim/player/space_ship_common.h>
#include <ioj/sim/transform3d.h>
#include <optional>

namespace ioj::sim {

struct PlayerReadView {
    ioj::sim::Transform3d transform;
    ioj::sim::Transform3d body_transform;
    ioj::sim::Transform3d middle_socket;
    ml::Vector3d velocity;
    ioj::sim::player::BoostBrakeState boost_brake_state{};
    ioj::sim::LaserFiringState laser_firing_mode{};
    std::uint64_t boost_start_sequence{};
};
} // namespace ioj::sim
