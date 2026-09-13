#pragma once
#include <algorithm>
#include <cstdint>
#include <optional>
#include <sandbox/simulation/ships/common/LaserFiringState.h>
#include <sandbox/simulation/ships/common/SpaceShipCommon.h>
#include <sandbox/simulation/transform3d.h>

struct FPlayerReadView {
    ml::simulation::Transform3d transform;
    ml::simulation::Transform3d body_transform;
    ml::simulation::Transform3d middle_socket;
    ml::Vector3d velocity;
    ml::simulation::player::BoostBrakeState boost_brake_state{};
    ml::simulation::LaserFiringState laser_firing_mode{};
    std::uint64_t boost_start_sequence{};
};
