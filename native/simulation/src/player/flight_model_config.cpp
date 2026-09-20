#include "ioj/sim/player/flight_model_config.h"

#include <array>
#include <cmath>

namespace ioj::sim::player::flight_model_config_detail {
[[nodiscard]] auto valid_enum(TranslationSemantic const value) noexcept -> bool {
    switch (value) {
        case TranslationSemantic::Disabled:
        case TranslationSemantic::TargetSpeed:
        case TranslationSemantic::TargetVelocity:
        case TranslationSemantic::Acceleration:
            return true;
    }
    return false;
}

[[nodiscard]] auto valid_enum(RotationSemantic const value) noexcept -> bool {
    switch (value) {
        case RotationSemantic::Disabled:
        case RotationSemantic::TargetAngularVelocity:
        case RotationSemantic::AngularAcceleration:
            return true;
    }
    return false;
}

[[nodiscard]] auto valid_enum(ReferenceFrame const value) noexcept -> bool {
    switch (value) {
        case ReferenceFrame::Ship:
        case ReferenceFrame::World:
            return true;
    }
    return false;
}

[[nodiscard]] auto valid_enum(TranslationInputSource const value) noexcept -> bool {
    switch (value) {
        case TranslationInputSource::Axis:
        case TranslationInputSource::Accelerator:
            return true;
    }
    return false;
}

[[nodiscard]] auto valid_enum(ResponseMode const value) noexcept -> bool {
    switch (value) {
        case ResponseMode::Direct:
        case ResponseMode::RateLimited:
        case ResponseMode::SecondOrder:
            return true;
    }
    return false;
}

[[nodiscard]] auto valid_enum(FacingVelocityCoupling const value) noexcept -> bool {
    switch (value) {
        case FacingVelocityCoupling::Independent:
        case FacingVelocityCoupling::AlignToFacing:
        case FacingVelocityCoupling::LockedToFacing:
            return true;
    }
    return false;
}

[[nodiscard]] auto targets_velocity(TranslationSemantic const semantic) noexcept -> bool {
    return semantic == TranslationSemantic::TargetSpeed ||
           semantic == TranslationSemantic::TargetVelocity;
}

[[nodiscard]] auto validate_non_negative(float const value) noexcept
    -> std::expected<void, FlightModelConfigError> {
    if (!std::isfinite(value)) {
        return std::unexpected{FlightModelConfigError::NonFiniteValue};
    }
    if (value < 0.f) {
        return std::unexpected{FlightModelConfigError::NegativeValue};
    }
    return {};
}

[[nodiscard]] auto validate_finite(float const value) noexcept
    -> std::expected<void, FlightModelConfigError> {
    if (!std::isfinite(value)) {
        return std::unexpected{FlightModelConfigError::NonFiniteValue};
    }
    return {};
}

[[nodiscard]] auto validate_response(ResponseConfig const& config) noexcept
    -> std::expected<void, FlightModelConfigError> {
    if (!valid_enum(config.mode)) {
        return std::unexpected{FlightModelConfigError::InvalidEnumValue};
    }

    for (auto const value :
         {config.rate_limited.increasing_rate, config.rate_limited.decreasing_rate}) {
        if (auto const result{validate_non_negative(value)}; !result) {
            return result;
        }
    }

    if (!std::isfinite(config.second_order.settling_time)) {
        return std::unexpected{FlightModelConfigError::NonFiniteValue};
    }
    if (config.second_order.settling_time <= 0.f) {
        return std::unexpected{FlightModelConfigError::InvalidSecondOrderSettlingTime};
    }
    if (!std::isfinite(config.second_order.damping_ratio)) {
        return std::unexpected{FlightModelConfigError::NonFiniteValue};
    }
    if (config.second_order.damping_ratio <= 0.f || config.second_order.damping_ratio >= 1.f) {
        return std::unexpected{FlightModelConfigError::InvalidSecondOrderDampingRatio};
    }
    return {};
}

[[nodiscard]] auto validate_channel(TranslationChannelConfig const& config,
                                    bool const automatic) noexcept
    -> std::expected<void, FlightModelConfigError> {
    if (!valid_enum(config.semantic) || !valid_enum(config.reference_frame) ||
        !valid_enum(config.input_source)) {
        return std::unexpected{FlightModelConfigError::InvalidEnumValue};
    }
    if (auto const result{validate_finite(config.automatic_value)}; !result) {
        return result;
    }
    if (automatic) {
        if (config.input_source != TranslationInputSource::Axis) {
            return std::unexpected{FlightModelConfigError::InvalidAutomaticChannel};
        }
        if (std::abs(config.automatic_value) > 1.f) {
            return std::unexpected{FlightModelConfigError::InputValueOutOfRange};
        }
    } else {
        if (config.automatic_value != 0.f ||
            (config.semantic == TranslationSemantic::TargetSpeed &&
             config.input_source != TranslationInputSource::Axis)) {
            return std::unexpected{FlightModelConfigError::InvalidManualChannel};
        }
    }
    return validate_response(config.response);
}

[[nodiscard]] auto validate_drive(TranslationDriveConfig const& config) noexcept
    -> std::expected<void, FlightModelConfigError> {
    for (auto const value : {config.positive_target_speed,
                             config.negative_target_speed,
                             config.positive_speed_limit,
                             config.negative_speed_limit,
                             config.positive_acceleration,
                             config.negative_acceleration}) {
        if (auto const result{validate_non_negative(value)}; !result) {
            return result;
        }
    }
    return {};
}

[[nodiscard]] auto validate_axis(TranslationAxisConfig const& config) noexcept
    -> std::expected<void, FlightModelConfigError> {
    if (auto const result{validate_channel(config.manual, false)}; !result) {
        return result;
    }
    if (auto const result{validate_channel(config.automatic, true)}; !result) {
        return result;
    }
    if (targets_velocity(config.manual.semantic) && targets_velocity(config.automatic.semantic)) {
        return std::unexpected{FlightModelConfigError::AmbiguousTargetChannels};
    }
    if (auto const result{validate_drive(config.normal)}; !result) {
        return result;
    }
    if (auto const result{validate_drive(config.boosted)}; !result) {
        return result;
    }
    for (auto const value : {config.passive_drag, config.active_stabilization_rate}) {
        if (auto const result{validate_non_negative(value)}; !result) {
            return result;
        }
    }
    return {};
}

[[nodiscard]] auto validate_axis(RotationAxisConfig const& config) noexcept
    -> std::expected<void, FlightModelConfigError> {
    if (!valid_enum(config.manual_semantic)) {
        return std::unexpected{FlightModelConfigError::InvalidEnumValue};
    }
    for (auto const value :
         {config.maximum_rate, config.acceleration, config.stabilization.delay}) {
        if (auto const result{validate_non_negative(value)}; !result) {
            return result;
        }
    }
    if (auto const result{validate_finite(config.stabilization.target_angle)}; !result) {
        return result;
    }
    if (auto const result{validate_response(config.response)}; !result) {
        return result;
    }
    return validate_response(config.stabilization.response);
}

[[nodiscard]] auto make_second_order_response() -> ResponseConfig {
    ResponseConfig result;
    result.mode = ResponseMode::SecondOrder;
    return result;
}

[[nodiscard]] auto make_rate_limited_response(float const increasing_rate,
                                              float const decreasing_rate) -> ResponseConfig {
    ResponseConfig result;
    result.mode = ResponseMode::RateLimited;
    result.rate_limited = {increasing_rate, decreasing_rate};
    return result;
}

void configure_common_rotation(FlightModelConfig& config) {
    config.rotation.pitch.manual_semantic = RotationSemantic::TargetAngularVelocity;
    config.rotation.pitch.maximum_rate = 60.f;
    config.rotation.yaw.manual_semantic = RotationSemantic::TargetAngularVelocity;
    config.rotation.yaw.maximum_rate = 60.f;

    auto& roll{config.rotation.roll};
    roll.stabilization.enabled = true;
    roll.stabilization.delay = 1.f;
    roll.stabilization.response = make_rate_limited_response(10.f, 10.f);
}

void configure_common_actions(FlightModelConfig& config) {
    config.boost.available = true;
    config.boost.energy_drain_per_second = 1.f / 4.f;
    config.brake.available = true;
    config.emergency_brake.available = true;
    config.emergency_brake.deceleration = 32000.f;
    config.emergency_brake.energy_drain_per_second = 1.f / 6.f;
    config.energy_recharge_per_second = 1.f / 7.f;
}

[[nodiscard]] auto make_starfox_config() -> FlightModelConfig {
    FlightModelConfig result;
    auto& forward{result.translation.forward};
    forward.automatic.semantic = TranslationSemantic::TargetVelocity;
    forward.automatic.automatic_value = 1.f;
    forward.automatic.response = make_second_order_response();
    forward.normal.positive_target_speed = 12000.f;
    forward.normal.positive_speed_limit = 12000.f;
    forward.normal.negative_speed_limit = 0.f;
    forward.boosted.positive_target_speed = 30000.f;
    forward.boosted.positive_speed_limit = 30000.f;
    forward.boosted.negative_speed_limit = 0.f;

    result.facing_velocity.mode = FacingVelocityCoupling::LockedToFacing;
    result.boost.response = make_second_order_response();
    result.brake.target_speed = 1000.f;
    result.brake.deceleration = 14000.f;
    result.brake.response = make_second_order_response();
    result.emergency_brake.target_speed = 1000.f;
    result.emergency_brake.response = make_second_order_response();
    result.maximum_resultant_speed = 12000.f;
    result.boosted_maximum_resultant_speed = 30000.f;
    configure_common_actions(result);
    result.brake.energy_drain_per_second = 1.f / 6.f;
    configure_common_rotation(result);
    return result;
}

[[nodiscard]] auto make_fighter_config() -> FlightModelConfig {
    FlightModelConfig result;
    auto& forward{result.translation.forward};
    forward.manual.semantic = TranslationSemantic::Acceleration;
    forward.manual.input_source = TranslationInputSource::Accelerator;
    forward.normal.positive_speed_limit = 8000.f;
    forward.normal.negative_speed_limit = 0.f;
    forward.normal.positive_acceleration = 10000.f;
    forward.normal.negative_acceleration = 10000.f;
    forward.boosted.positive_speed_limit = 16000.f;
    forward.boosted.negative_speed_limit = 0.f;
    forward.boosted.positive_acceleration = 20000.f;
    forward.boosted.negative_acceleration = 20000.f;
    forward.passive_drag = 2000.f;

    result.facing_velocity.mode = FacingVelocityCoupling::LockedToFacing;
    result.brake.deceleration = 14000.f;
    result.emergency_brake.deceleration = 32000.f;
    result.maximum_resultant_speed = 8000.f;
    result.boosted_maximum_resultant_speed = 16000.f;
    configure_common_actions(result);
    configure_common_rotation(result);
    return result;
}

[[nodiscard]] auto make_skater_config() -> FlightModelConfig {
    auto result{make_fighter_config()};
    result.translation.forward.passive_drag = 0.f;
    result.facing_velocity.mode = FacingVelocityCoupling::Independent;
    result.maximum_resultant_speed = effectively_unlimited_speed;
    result.boosted_maximum_resultant_speed = effectively_unlimited_speed;
    return result;
}

[[nodiscard]] auto make_gunship_config() -> FlightModelConfig {
    FlightModelConfig result;
    auto configure_axis = [](TranslationAxisConfig& axis, float const speed) {
        axis.manual.semantic = TranslationSemantic::TargetVelocity;
        axis.manual.response = make_rate_limited_response(10000.f, 14000.f);
        axis.normal.positive_target_speed = speed;
        axis.normal.negative_target_speed = speed;
        axis.normal.positive_speed_limit = speed;
        axis.normal.negative_speed_limit = speed;
        axis.boosted.positive_target_speed = speed * 2.f;
        axis.boosted.negative_target_speed = speed * 2.f;
        axis.boosted.positive_speed_limit = speed * 2.f;
        axis.boosted.negative_speed_limit = speed * 2.f;
    };
    configure_axis(result.translation.forward, 8000.f);
    configure_axis(result.translation.right, 3000.f);
    configure_axis(result.translation.up, 3000.f);
    result.boost.response = make_rate_limited_response(10000.f, 14000.f);

    result.facing_velocity.mode = FacingVelocityCoupling::Independent;
    result.brake.deceleration = 14000.f;
    result.emergency_brake.deceleration = 32000.f;
    result.maximum_resultant_speed = 9000.f;
    result.boosted_maximum_resultant_speed = 18000.f;
    configure_common_actions(result);
    configure_common_rotation(result);
    return result;
}
}

namespace ioj::sim::player {
auto validate_flight_model_config(FlightModelConfig const& config) noexcept
    -> std::expected<void, FlightModelConfigError> {
    using namespace flight_model_config_detail;

    if (!valid_enum(config.facing_velocity.mode)) {
        return std::unexpected{FlightModelConfigError::InvalidEnumValue};
    }

    for (auto const* const axis : std::array{
             &config.translation.forward, &config.translation.right, &config.translation.up}) {
        if (auto const result{validate_axis(*axis)}; !result) {
            return result;
        }
    }
    for (auto const* const axis :
         std::array{&config.rotation.pitch, &config.rotation.yaw, &config.rotation.roll}) {
        if (auto const result{validate_axis(*axis)}; !result) {
            return result;
        }
    }
    if (auto const result{validate_non_negative(config.facing_velocity.alignment_rate)}; !result) {
        return result;
    }
    if (auto const result{validate_response(config.facing_velocity.response)}; !result) {
        return result;
    }
    if (auto const result{validate_non_negative(config.boost.energy_drain_per_second)}; !result) {
        return result;
    }
    if (auto const result{validate_response(config.boost.response)}; !result) {
        return result;
    }
    for (auto const* const brake : std::array{&config.brake, &config.emergency_brake}) {
        for (auto const value :
             {brake->target_speed, brake->deceleration, brake->energy_drain_per_second}) {
            if (auto const result{validate_non_negative(value)}; !result) {
                return result;
            }
        }
        if (auto const result{validate_response(brake->response)}; !result) {
            return result;
        }
    }
    for (auto const value : {config.energy_recharge_per_second,
                             config.maximum_resultant_speed,
                             config.boosted_maximum_resultant_speed}) {
        if (auto const result{validate_non_negative(value)}; !result) {
            return result;
        }
    }
    return {};
}

auto make_flight_model_profile(FlightModelPreset const preset) -> FlightModelProfile {
    using namespace flight_model_config_detail;

    FlightModelConfig config;
    switch (preset) {
        case FlightModelPreset::Starfox:
            config = make_starfox_config();
            break;
        case FlightModelPreset::Fighter:
            config = make_fighter_config();
            break;
        case FlightModelPreset::Skater:
            config = make_skater_config();
            break;
        case FlightModelPreset::Gunship:
            config = make_gunship_config();
            break;
    }
    return {preset, false, config};
}

auto make_default_flight_model_loadout() -> FlightModelLoadout {
    return {
        .up = make_flight_model_profile(FlightModelPreset::Starfox),
        .right = make_flight_model_profile(FlightModelPreset::Fighter),
        .down = make_flight_model_profile(FlightModelPreset::Skater),
        .left = make_flight_model_profile(FlightModelPreset::Gunship),
        .initial_slot = FlightModelSlot::Up,
    };
}

auto flight_model_profile(FlightModelLoadout& loadout, FlightModelSlot const slot) noexcept
    -> FlightModelProfile& {
    return const_cast<FlightModelProfile&>(
        flight_model_profile(static_cast<FlightModelLoadout const&>(loadout), slot));
}

auto flight_model_profile(FlightModelLoadout const& loadout, FlightModelSlot const slot) noexcept
    -> FlightModelProfile const& {
    switch (slot) {
        case FlightModelSlot::Up:
            return loadout.up;
        case FlightModelSlot::Right:
            return loadout.right;
        case FlightModelSlot::Down:
            return loadout.down;
        case FlightModelSlot::Left:
            return loadout.left;
    }
    return loadout.up;
}
}
