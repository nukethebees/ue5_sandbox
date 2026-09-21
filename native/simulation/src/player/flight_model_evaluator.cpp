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
    float axis_input;
    float accelerator_input;
    ml::Vector3d ship_axis;
    ml::Vector3d world_axis;
};

[[nodiscard]] auto axis_vector(AxisRuntime const& axis, ReferenceFrame const frame) noexcept
    -> ml::Vector3d {
    return frame == ReferenceFrame::Ship ? axis.ship_axis : axis.world_axis;
}

[[nodiscard]] auto channel_input(AxisRuntime const& axis,
                                 TranslationChannelConfig const& channel) noexcept -> float {
    return channel.input_source == TranslationInputSource::Accelerator ? axis.accelerator_input
                                                                       : axis.axis_input;
}

void seed_axis_responses(ml::Vector3d const& velocity,
                         TranslationAxisConfig const& config,
                         ml::Vector3d const& ship_axis,
                         ml::Vector3d const& world_axis,
                         bool const reset_acceleration,
                         ScalarResponse& manual,
                         ScalarResponse& automatic) noexcept {
    auto seed = [&](TranslationChannelConfig const& channel, ScalarResponse& response) {
        auto const targets_velocity{channel.semantic == TranslationSemantic::TargetSpeed ||
                                    channel.semantic == TranslationSemantic::TargetVelocity};
        if (!targets_velocity && !reset_acceleration) {
            return;
        }
        auto const direction{channel.reference_frame == ReferenceFrame::Ship ? ship_axis
                                                                             : world_axis};
        response.reset(targets_velocity ? static_cast<float>(ml::dot(velocity, direction)) : 0.f);
    };
    seed(config.manual, manual);
    seed(config.automatic, automatic);
}

void seed_translation_responses(PlayerSimulationState& state,
                                FlightModelConfig const& config,
                                bool const reset_acceleration) noexcept {
    auto& controller{state.controller};
    auto const& physical{state.physical};
    seed_axis_responses(physical.velocity,
                        config.translation.forward,
                        physical.transform.forward(),
                        {1.0, 0.0, 0.0},
                        reset_acceleration,
                        controller.forward_manual_response,
                        controller.forward_automatic_response);
    seed_axis_responses(physical.velocity,
                        config.translation.right,
                        physical.transform.right(),
                        {0.0, 1.0, 0.0},
                        reset_acceleration,
                        controller.right_manual_response,
                        controller.right_automatic_response);
    seed_axis_responses(physical.velocity,
                        config.translation.up,
                        physical.transform.transform_vector_no_scale({0.0, 0.0, 1.0}),
                        {0.0, 0.0, 1.0},
                        reset_acceleration,
                        controller.up_manual_response,
                        controller.up_automatic_response);
}

[[nodiscard]] auto selected_drive(AxisRuntime const& axis, bool const boosting) noexcept
    -> TranslationDriveConfig const& {
    return boosting ? axis.config.boosted : axis.config.normal;
}

[[nodiscard]] auto limit_for(float const input, TranslationDriveConfig const& drive) noexcept
    -> float {
    return input >= 0.f ? drive.positive_speed_limit : drive.negative_speed_limit;
}

[[nodiscard]] auto target_speed_for(float const input, TranslationDriveConfig const& drive) noexcept
    -> float {
    auto const requested{input >= 0.f ? drive.positive_target_speed : drive.negative_target_speed};
    return std::min(requested, limit_for(input, drive));
}

[[nodiscard]] auto clamp_target_speed(float const target,
                                      TranslationDriveConfig const& drive) noexcept -> float {
    return std::clamp(target, -target_speed_for(-1.f, drive), target_speed_for(1.f, drive));
}

[[nodiscard]] auto acceleration_for(float const input, TranslationDriveConfig const& drive) noexcept
    -> float {
    return input >= 0.f ? drive.positive_acceleration : drive.negative_acceleration;
}

void apply_target_channel(float const dt,
                          TranslationSemantic const semantic,
                          bool const automatic,
                          ResponseConfig const& response_config,
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

    auto target{input * target_speed_for(input, drive)};
    if (semantic == TranslationSemantic::TargetSpeed && !automatic) {
        target = clamp_target_speed(persistent_target, drive);
    }
    auto const current{static_cast<float>(ml::dot(velocity, axis))};
    auto const controlled{std::clamp(response.update(dt, target, response_config),
                                     -drive.negative_speed_limit,
                                     drive.positive_speed_limit)};
    velocity += axis * (controlled - current);
}

void apply_acceleration_channel(float const dt,
                                TranslationChannelConfig const& channel,
                                ResponseConfig const& response_config,
                                TranslationDriveConfig const& drive,
                                float const input,
                                ScalarResponse& response,
                                ml::Vector3d const axis,
                                ml::Vector3d& velocity) noexcept {
    if (channel.semantic != TranslationSemantic::Acceleration) {
        return;
    }
    if (input == 0.f) {
        auto const unused{response.update(dt, 0.f, response_config)};
        static_cast<void>(unused);
        return;
    }

    auto const acceleration{input * acceleration_for(input, drive)};
    auto const smoothed{response.update(dt, acceleration, response_config)};
    auto const component{ml::dot(velocity, axis)};
    auto const limit{limit_for(input, drive)};
    if (std::isfinite(limit) && std::abs(component) >= limit && component * smoothed > 0.0) {
        return;
    }
    velocity += axis * (smoothed * dt);
    auto const updated_component{ml::dot(velocity, axis)};
    auto const limited_component{std::clamp(updated_component,
                                            -static_cast<double>(drive.negative_speed_limit),
                                            static_cast<double>(drive.positive_speed_limit))};
    velocity += axis * (limited_component - updated_component);
}

void integrate_axis(float const dt,
                    bool const boosting,
                    BoostConfig const& boost,
                    AxisRuntime& axis,
                    ml::Vector3d& velocity) noexcept {
    auto const& drive{selected_drive(axis, boosting)};
    auto const manual_input{channel_input(axis, axis.config.manual)};
    auto const manual_axis{axis_vector(axis, axis.config.manual.reference_frame)};
    auto const automatic_axis{axis_vector(axis, axis.config.automatic.reference_frame)};
    auto const automatic_input{axis.config.automatic.automatic_value};
    auto const& manual_response_config{boosting ? boost.response : axis.config.manual.response};
    auto const& automatic_response_config{boosting ? boost.response
                                                   : axis.config.automatic.response};
    axis.persistent_target_speed = clamp_target_speed(axis.persistent_target_speed, drive);

    apply_target_channel(dt,
                         axis.config.manual.semantic,
                         false,
                         manual_response_config,
                         drive,
                         manual_input,
                         axis.persistent_target_speed,
                         axis.manual_response,
                         manual_axis,
                         velocity);
    apply_target_channel(dt,
                         axis.config.automatic.semantic,
                         true,
                         automatic_response_config,
                         drive,
                         automatic_input,
                         axis.persistent_target_speed,
                         axis.automatic_response,
                         automatic_axis,
                         velocity);
    apply_acceleration_channel(dt,
                               axis.config.manual,
                               manual_response_config,
                               drive,
                               manual_input,
                               axis.manual_response,
                               manual_axis,
                               velocity);
    apply_acceleration_channel(dt,
                               axis.config.automatic,
                               automatic_response_config,
                               drive,
                               automatic_input,
                               axis.automatic_response,
                               automatic_axis,
                               velocity);

    auto move_component_toward_zero = [&](ml::Vector3d const& direction, float const rate) {
        auto const component{ml::dot(velocity, direction)};
        auto const maximum_change{static_cast<double>(rate * dt)};
        auto const new_component{
            std::clamp(0.0, component - maximum_change, component + maximum_change)};
        velocity += direction * (new_component - component);
    };

    if (axis.config.passive_drag > 0.f) {
        move_component_toward_zero(axis_vector(axis, axis.config.passive_drag_reference_frame),
                                   axis.config.passive_drag);
    }
    auto const manual_command_active{
        axis.config.manual.semantic == TranslationSemantic::TargetSpeed
            ? axis.persistent_target_speed != 0.f
            : axis.config.manual.semantic != TranslationSemantic::Disabled && manual_input != 0.f};
    auto const automatic_command_active{
        axis.config.automatic.semantic != TranslationSemantic::Disabled && automatic_input != 0.f};
    if (!manual_command_active && !automatic_command_active &&
        axis.config.active_stabilization_rate > 0.f) {
        move_component_toward_zero(
            axis_vector(axis, axis.config.active_stabilization_reference_frame),
            axis.config.active_stabilization_rate);
    }
}

void apply_brake(float const dt, BrakeConfig const& brake, PlayerSimulationState& state) noexcept {
    auto& velocity{state.physical.velocity};
    auto const speed{static_cast<float>(velocity.size())};
    if (speed <= brake.target_speed || speed <= 1.e-8f) {
        return;
    }

    auto const engagement{std::clamp(
        state.controller.brake_engagement_response.update(dt, 1.f, brake.response), 0.f, 1.f)};
    auto const target_speed{
        std::max(brake.target_speed, speed - brake.deceleration * engagement * dt)};
    velocity = velocity * (target_speed / speed);
}

void apply_facing_coupling(float const dt,
                           FacingVelocityConfig const& config,
                           ScalarResponse& response,
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
    auto const alignment_rate{
        std::max(0.f, response.update(dt, config.alignment_rate, config.response))};
    physical.velocity = ml::move_towards(physical.velocity, target, alignment_rate * dt);
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
            angular_velocity = std::clamp(angular_velocity,
                                          -static_cast<double>(config.maximum_rate),
                                          static_cast<double>(config.maximum_rate));
            break;
    }
    return angular_velocity * dt;
}

[[nodiscard]] auto stabilized_angle(float const dt,
                                    float const input,
                                    float const time_since_input,
                                    double const current,
                                    RotationAxisConfig const& config,
                                    ScalarResponse& response) noexcept -> double {
    if (!config.stabilization.enabled || std::abs(input) > 1.e-8f ||
        time_since_input < config.stabilization.delay) {
        response.reset(static_cast<float>(current));
        return current;
    }
    return response.update(dt, config.stabilization.target_angle, config.stabilization.response);
}
}

namespace ioj::sim::player {
void seed_flight_model_responses(PlayerSimulationState& state,
                                 FlightModelConfig const& config) noexcept {
    auto& controller{state.controller};
    auto const& physical{state.physical};
    flight_model_evaluator_detail::seed_translation_responses(state, config, true);
    controller.pitch_response.reset(0.f);
    controller.yaw_response.reset(0.f);
    controller.roll_response.reset(0.f);
    auto const rotation{physical.transform.rotator()};
    controller.pitch_stabilization_response.reset(static_cast<float>(rotation.pitch));
    controller.yaw_stabilization_response.reset(static_cast<float>(rotation.yaw));
    controller.roll_stabilization_response.reset(static_cast<float>(rotation.roll));
    controller.facing_alignment_response.reset(0.f);
    controller.brake_engagement_response.reset(0.f);
}

void prepare_flight_model_action_transition(PlayerSimulationState& state,
                                            FlightModelConfig const& config) noexcept {
    flight_model_evaluator_detail::seed_translation_responses(state, config, false);
    state.controller.brake_engagement_response.reset(0.f);
}

void clamp_flight_model_persistent_targets(PlayerSimulationState& state,
                                           FlightModelConfig const& config) noexcept {
    using namespace flight_model_evaluator_detail;

    auto const boosting{state.controller.effective_action == BoostBrakeState::Boost};
    auto clamp_axis = [boosting](float& target, TranslationAxisConfig const& axis) {
        auto const& drive{boosting ? axis.boosted : axis.normal};
        target = clamp_target_speed(target, drive);
    };
    clamp_axis(state.controller.persistent_forward_target_speed, config.translation.forward);
    clamp_axis(state.controller.persistent_right_target_speed, config.translation.right);
    clamp_axis(state.controller.persistent_up_target_speed, config.translation.up);
}

void reset_flight_model_controller(PlayerSimulationState& state,
                                   FlightModelConfig const& config,
                                   bool const clear_persistent_targets) noexcept {
    seed_flight_model_responses(state, config);

    auto& controller{state.controller};
    controller.angular_velocity = {};
    controller.time_since_rotation_input = {};
    if (clear_persistent_targets) {
        controller.persistent_forward_target_speed = 0.f;
        controller.persistent_right_target_speed = 0.f;
        controller.persistent_up_target_speed = 0.f;
        controller.effective_action = BoostBrakeState::None;
    } else {
        clamp_flight_model_persistent_targets(state, config);
    }
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

    if (std::abs(intent.rotation.x) > 1.e-8) {
        controller.time_since_rotation_input.x = 0.0;
    }
    if (std::abs(intent.rotation.y) > 1.e-8) {
        controller.time_since_rotation_input.y = 0.0;
    }
    if (std::abs(intent.rotation.z) > 1.e-8) {
        controller.time_since_rotation_input.z = 0.0;
    }
    auto const current_rotation{physical.transform.rotator()};
    auto const stabilized_pitch{
        stabilized_angle(dt,
                         static_cast<float>(intent.rotation.x),
                         static_cast<float>(controller.time_since_rotation_input.x),
                         current_rotation.pitch,
                         config.rotation.pitch,
                         controller.pitch_stabilization_response)};
    auto const stabilized_yaw{
        stabilized_angle(dt,
                         static_cast<float>(intent.rotation.y),
                         static_cast<float>(controller.time_since_rotation_input.y),
                         current_rotation.yaw,
                         config.rotation.yaw,
                         controller.yaw_stabilization_response)};
    auto const stabilized_roll{
        stabilized_angle(dt,
                         static_cast<float>(intent.rotation.z),
                         static_cast<float>(controller.time_since_rotation_input.z),
                         current_rotation.roll,
                         config.rotation.roll,
                         controller.roll_stabilization_response)};
    physical.transform.rotation =
        to_quaternion(Rotator3d{stabilized_pitch, stabilized_yaw, stabilized_roll});

    auto const boosting{controller.effective_action == BoostBrakeState::Boost};
    std::array axes{
        AxisRuntime{config.translation.forward,
                    controller.forward_manual_response,
                    controller.forward_automatic_response,
                    controller.persistent_forward_target_speed,
                    static_cast<float>(intent.translation.x),
                    intent.accelerator,
                    physical.transform.forward(),
                    ml::Vector3d{1.0, 0.0, 0.0}},
        AxisRuntime{config.translation.right,
                    controller.right_manual_response,
                    controller.right_automatic_response,
                    controller.persistent_right_target_speed,
                    static_cast<float>(intent.translation.y),
                    intent.accelerator,
                    physical.transform.right(),
                    ml::Vector3d{0.0, 1.0, 0.0}},
        AxisRuntime{config.translation.up,
                    controller.up_manual_response,
                    controller.up_automatic_response,
                    controller.persistent_up_target_speed,
                    static_cast<float>(intent.translation.z),
                    intent.accelerator,
                    physical.transform.transform_vector_no_scale({0.0, 0.0, 1.0}),
                    ml::Vector3d{0.0, 0.0, 1.0}},
    };
    auto const braking{controller.effective_action == BoostBrakeState::Brake ||
                       controller.effective_action == BoostBrakeState::EmergencyBrake};
    if (!braking) {
        for (auto& axis : axes) {
            integrate_axis(dt, boosting, config.boost, axis, physical.velocity);
        }
    }

    apply_facing_coupling(
        dt, config.facing_velocity, controller.facing_alignment_response, physical);
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
