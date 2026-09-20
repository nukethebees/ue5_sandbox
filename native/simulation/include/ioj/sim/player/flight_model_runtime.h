#pragma once

#include <cstdint>

#include <ioj/sim/player/scalar_response.h>
#include <ioj/sim/player/space_ship_common.h>
#include <ioj/sim/ship_flight_model.h>
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
    ScalarResponse action_speed_response{};
    ml::Vector3d angular_velocity{};
    float persistent_forward_target_speed{};
    float persistent_right_target_speed{};
    float persistent_up_target_speed{};
    BoostBrakeState effective_action{};

    // Legacy response state retained only while the old evaluator is migrated preset by preset.
    ml::Vector3d planar_velocity{};
    float planar_boost_speed{};
    float target_speed{};
    ShipFlightModel<float> forward_flight_model{};
    ShipFlightModel<ml::Vector3d> planar_flight_model{};
    ShipFlightModel<float> planar_boost_flight_model{};
};

struct PlayerResourceState {
    float thrust_energy{1.f};
    float thrust_change_rate{};
};

struct PlayerPresentationState {
    Transform3d body_transform{};
    float time_since_rotation_input{100.f};
    std::uint64_t boost_start_sequence{};
};

struct PlayerSimulationState {
    PhysicalMovementState physical{};
    FlightModelControllerState controller{};
    PlayerResourceState resources{};
    PlayerPresentationState presentation{};
};
}
