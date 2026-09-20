#include "ioj/sim/player/flight_model_evaluator.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace ioj::sim::player::flight_model_evaluator_detail {
struct AxisRuntime {
    TranslationAxisConfig const& config;
    ScalarResponse& manual_response;
    ScalarResponse& automatic_response;
    float& persistent_target_speed;
    float input;
    ml::Vector3d ship_axis;
    ml::Vector3d world_axis;
};

[[nodiscard]] auto axis_vector(AxisRuntime const& axis,
                               ReferenceFrame const frame) noexcept -> ml::Vector3d {
    return frame == ReferenceFrame::Ship ? axis.ship_axis : axis.world_axis;
}

[[nodiscard]] auto selected_drive(AxisRuntime const& axis,
                                  bool const boosting) noexcept -> TranslationDriveConfig const& {
    return boosting ? axis.config.boosted : axis.config.normal;
}

[[nodiscard]] auto limit_for(float const input,
                             TranslationDriveConfig const& drive) noexcept -> float {
    return input >= 0.f ? drive.positive_speed_limit : drive.negative_speed_limit;
}

[[nodiscard]] auto acceleration_for(float const input,
                                    TranslationDriveConfig const& drive) noexcept -> float {
    return input >= 0.f ? drive.positive_acceleration : drive.negative_acceleration;
}

[[nodiscard]] auto automatic_target(TranslationChannelConfig const& channel,
                                    TranslationDriveConfig const& drive,
                                    bool const boosting) noexcept -> float {
    if (channel.automatic_value == 0.f) {
        return 0.f;
    }
    auto const limit{limit_for(channel.automatic_value, drive)};
    auto const magnitude{boosting ? limit : std::min(std::abs(channel.automatic_value), limit)};
    return std::copysign(magnitude,
                         channel.automatic_value);
}

void apply_target_channel(float const dt,
                          TranslationSemantic const semantic,
                          TranslationChannelConfig const& channel,
                          TranslationDriveConfig const& drive,
                          float const input,
                          float const persistent_target,
                          ScalarResponse& response,
                          ml::Vector3d const axis,
                          ml::Vector3d& velocity) noexcept {
    if (semantic != TranslationSemantic::TargetVelocity &&
        semantic != TranslationSemantic::TargetSpeed) {
        return;
    }

    auto target{input * limit_for(input, drive)};
    if (semantic == TranslationSemantic::TargetSpeed) {
        target = persistent_target;
    }
    auto const current{static_cast<float>(ml::dot(velocity, axis))};
    if (!std::isfinite(target)) {
        target = current;
    }
    auto const controlled{response.update(dt, target, channel.response)};
    velocity += axis * (controlled - current);
}

void apply_acceleration_channel(float const dt,
                                TranslationChannelConfig const& channel,
                                TranslationDriveConfig const& drive,
                                float const input,
                                ScalarResponse& response,
                                ml::Vector3d const axis,
                                ml::Vector3d& velocity) noexcept {
    if (channel.semantic != TranslationSemantic::Acceleration) {
        return;
    }
    if (input == 0.f) {
        auto const unused{response.update(dt, 0.f, channel.response)};
        static_cast<void>(unused);
        return;
    }

    auto const acceleration{input * acceleration_for(input, drive)};
    auto const smoothed{response.update(dt, acceleration, channel.response)};
    auto const component{ml::dot(velocity, axis)};
    auto const limit{limit_for(input, drive)};
    if (std::isfinite(limit) && std::abs(component) >= limit && component * smoothed > 0.0) {
        return;
    }
    velocity += axis * (smoothed * dt);
}

void integrate_axis(float const dt,
                    bool const boosting,
                    AxisRuntime& axis,
                    ml::Vector3d& velocity) noexcept {
    auto const& drive{selected_drive(axis, boosting)};
    auto const manual_axis{axis_vector(axis, axis.config.manual.reference_frame)};
    auto const automatic_axis{axis_vector(axis, axis.config.automatic.reference_frame)};
    auto const automatic_input{axis.config.automatic.automatic_value};

    apply_target_channel(dt,
                         axis.config.manual.semantic,
                         axis.config.manual,
                         drive,
                         axis.input,
                         axis.persistent_target_speed,
                         axis.manual_response,
                         manual_axis,
                         velocity);
    apply_target_channel(dt,
                         axis.config.automatic.semantic,
                         axis.config.automatic,
                         drive,
                         automatic_target(axis.config.automatic, drive, boosting) /
                             std::max(limit_for(automatic_input, drive), 1.f),
                         axis.persistent_target_speed,
                         axis.automatic_response,
                         automatic_axis,
                         velocity);
    apply_acceleration_channel(dt,
                               axis.config.manual,
                               drive,
                               axis.input,
                               axis.manual_response,
                               manual_axis,
                               velocity);
    apply_acceleration_channel(dt,
                               axis.config.automatic,
                               drive,
                               automatic_input,
                               axis.automatic_response,
                               automatic_axis,
                               velocity);

    if (axis.input == 0.f && axis.config.manual.semantic == TranslationSemantic::Acceleration &&
        axis.config.passive_drag > 0.f) {
        auto const component{ml::dot(velocity, manual_axis)};
        auto const maximum_change{static_cast<double>(axis.config.passive_drag * dt)};
        auto const new_component{std::clamp(0.0,
                                            component - maximum_change,
                                            component + maximum_change)};
        velocity += manual_axis * (new_component - component);
    }
}

void apply_brake(float const dt,
                 BrakeConfig const& brake,
                 PlayerSimulationState& state) noexcept {
    auto& velocity{state.physical.velocity};
    auto const speed{static_cast<float>(velocity.size())};
    if (speed <= brake.target_speed || speed <= 1.e-8f) {
        return;
    }

    auto target_speed{brake.target_speed};
    if (brake.response.mode == ResponseMode::Direct) {
        target_speed = std::max(brake.target_speed, speed - brake.deceleration * dt);
    } else if (brake.response.mode == ResponseMode::RateLimited) {
        auto response{brake.response};
        response.rate_limited.decreasing_rate = brake.deceleration;
        target_speed = state.controller.action_speed_response.update(dt, brake.target_speed, response);
    } else {
        target_speed =
            state.controller.action_speed_response.update(dt, brake.target_speed, brake.response);
    }
    target_speed = std::clamp(target_speed, brake.target_speed, speed);
    velocity = velocity * (target_speed / speed);
}

void apply_facing_coupling(float const dt,
                           FacingVelocityConfig const& config,
                           PhysicalMovementState& physical) noexcept {
    auto const speed{physical.velocity.size()};
    if (speed <= 1.e-8 || config.mode == FacingVelocityCoupling::Independent) {
        return;
    }

    auto const target{physical.transform.forward() * speed};
    if (config.mode == FacingVelocityCoupling::LockedToFacing) {
        physical.velocity = target;
        return;
    }
    physical.velocity =
        ml::move_towards(physical.velocity, target, config.alignment_rate * dt);
}

void apply_resultant_limit(float const limit, ml::Vector3d& velocity) noexcept {
    auto const speed{velocity.size()};
    if (!std::isfinite(limit) || speed <= limit || speed <= 1.e-8) {
        return;
    }
    velocity = velocity * (limit / speed);
}

[[nodiscard]] auto integrate_rotation_axis(float const dt,
                                           float const input,
                                           RotationAxisConfig const& config,
                                           ScalarResponse& response,
                                           double& angular_velocity) noexcept -> double {
    switch (config.manual_semantic) {
        case RotationSemantic::Disabled:
            angular_velocity = 0.f;
            return 0.f;
        case RotationSemantic::TargetAngularVelocity:
            angular_velocity = response.update(dt, input * config.maximum_rate, config.response);
            break;
        case RotationSemantic::AngularAcceleration:
            angular_velocity +=
                response.update(dt, input * config.acceleration, config.response) * dt;
            angular_velocity =
                std::clamp(angular_velocity,
                           -static_cast<double>(config.maximum_rate),
                           static_cast<double>(config.maximum_rate));
            break;
    }
    return angular_velocity * dt;
}
}

namespace ioj::sim::player {
void seed_flight_model_responses(PlayerSimulationState& state) noexcept {
    auto& controller{state.controller};
    auto const& physical{state.physical};
    auto const local_velocity{
        physical.transform.inverse_transform_vector_no_scale(physical.velocity)};

    controller.forward_manual_response.reset(static_cast<float>(local_velocity.x));
    controller.forward_automatic_response.reset(static_cast<float>(local_velocity.x));
    controller.right_manual_response.reset(static_cast<float>(local_velocity.y));
    controller.right_automatic_response.reset(static_cast<float>(local_velocity.y));
    controller.up_manual_response.reset(static_cast<float>(local_velocity.z));
    controller.up_automatic_response.reset(static_cast<float>(local_velocity.z));
    controller.pitch_response.reset(0.f);
    controller.yaw_response.reset(0.f);
    controller.roll_response.reset(0.f);
    controller.action_speed_response.reset(static_cast<float>(physical.velocity.size()));
}

void reset_flight_model_controller(PlayerSimulationState& state,
                                   FlightModelConfig const&) noexcept {
    seed_flight_model_responses(state);

    auto& controller{state.controller};
    controller.angular_velocity = {};
    controller.persistent_forward_target_speed = 0.f;
    controller.persistent_right_target_speed = 0.f;
    controller.persistent_up_target_speed = 0.f;
    controller.effective_action = BoostBrakeState::None;
}

void integrate_flight_model(float const dt,
                            FlightModelConfig const& config,
                            PlayerFlightIntent const& intent,
                            PlayerSimulationState& state) noexcept {
    using namespace flight_model_evaluator_detail;

    auto& physical{state.physical};
    auto& controller{state.controller};
    auto const pitch{integrate_rotation_axis(dt,
                                             static_cast<float>(intent.rotation.x),
                                             config.rotation.pitch,
                                             controller.pitch_response,
                                             controller.angular_velocity.x)};
    auto const yaw{integrate_rotation_axis(dt,
                                           static_cast<float>(intent.rotation.y),
                                           config.rotation.yaw,
                                           controller.yaw_response,
                                           controller.angular_velocity.y)};
    auto const roll{integrate_rotation_axis(dt,
                                            static_cast<float>(intent.rotation.z),
                                            config.rotation.roll,
                                            controller.roll_response,
                                            controller.angular_velocity.z)};
    physical.transform.rotation =
        physical.transform.rotation * to_quaternion(Rotator3d{pitch, yaw, roll});
    physical.transform.rotation.normalize();

    auto const boosting{controller.effective_action == BoostBrakeState::Boost};
    std::array axes{
        AxisRuntime{config.translation.forward,
                    controller.forward_manual_response,
                    controller.forward_automatic_response,
                    controller.persistent_forward_target_speed,
                    static_cast<float>(intent.translation.x),
                    physical.transform.forward(),
                    ml::Vector3d{1.0, 0.0, 0.0}},
        AxisRuntime{config.translation.right,
                    controller.right_manual_response,
                    controller.right_automatic_response,
                    controller.persistent_right_target_speed,
                    static_cast<float>(intent.translation.y),
                    physical.transform.right(),
                    ml::Vector3d{0.0, 1.0, 0.0}},
        AxisRuntime{config.translation.up,
                    controller.up_manual_response,
                    controller.up_automatic_response,
                    controller.persistent_up_target_speed,
                    static_cast<float>(intent.translation.z),
                    physical.transform.transform_vector_no_scale({0.0, 0.0, 1.0}),
                    ml::Vector3d{0.0, 0.0, 1.0}},
    };
    auto const braking{controller.effective_action == BoostBrakeState::Brake ||
                       controller.effective_action == BoostBrakeState::EmergencyBrake};
    if (!braking) {
        for (auto& axis : axes) {
            integrate_axis(dt, boosting, axis, physical.velocity);
        }
    }

    apply_facing_coupling(dt, config.facing_velocity, physical);
    if (controller.effective_action == BoostBrakeState::EmergencyBrake) {
        apply_brake(dt, config.emergency_brake, state);
    } else if (controller.effective_action == BoostBrakeState::Brake) {
        apply_brake(dt, config.brake, state);
    }

    auto const resultant_limit{boosting ? config.boosted_maximum_resultant_speed
                                        : config.maximum_resultant_speed};
    apply_resultant_limit(resultant_limit, physical.velocity);
    physical.transform.location += physical.velocity * dt;
}
}
