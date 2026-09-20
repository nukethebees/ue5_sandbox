#pragma once

#include <cstdint>

#include <ioj/sim/player/scalar_response.h>
#include <ioj/sim/player/space_ship_common.h>
#include <ioj/sim/transform3d.h>
#include <sandbox/core/vector2d.h>

namespace ioj::sim::player {
struct PhysicalMovementState {
    Transform3d transform{};
    ml::Vector3d velocity{};
};

struct PlayerFlightIntent {
    ml::Vector3d translation{};
    ml::Vector3d rotation{};
    float accelerator{};
    bool boost_held{};
    bool brake_held{};
    bool emergency_brake_held{};
};

struct FlightModelControllerState {
    ScalarResponse forward_manual_response{};
    ScalarResponse forward_automatic_response{};
    ScalarResponse right_manual_response{};
    ScalarResponse right_automatic_response{};
    ScalarResponse up_manual_response{};
    ScalarResponse up_automatic_response{};
    ScalarResponse pitch_response{};
    ScalarResponse yaw_response{};
    ScalarResponse roll_response{};
    ScalarResponse pitch_stabilization_response{};
    ScalarResponse yaw_stabilization_response{};
    ScalarResponse roll_stabilization_response{};
    ScalarResponse facing_alignment_response{};
    ScalarResponse action_speed_response{};
    ml::Vector3d angular_velocity{};
    float persistent_forward_target_speed{};
    float persistent_right_target_speed{};
    float persistent_up_target_speed{};
    float time_since_rotation_input{100.f};
    BoostBrakeState effective_action{};
};

struct PlayerResourceState {
    float thrust_energy{1.f};
    float thrust_change_rate{};
};

struct PlayerPresentationState {
    Transform3d body_transform{};
    std::uint64_t boost_start_sequence{};
};

struct PlayerSimulationState {
    PhysicalMovementState physical{};
    FlightModelControllerState controller{};
    PlayerResourceState resources{};
    PlayerPresentationState presentation{};
};
}
