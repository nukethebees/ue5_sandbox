#include "ioj/sim/player/flight_model_config.h"

#include <array>
#include <cassert>

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

[[nodiscard]] auto validate_response(ResponseConfig const& config) noexcept
    -> std::expected<void, FlightModelConfigError> {
    if (!valid_enum(config.mode)) {
        return std::unexpected{FlightModelConfigError::InvalidEnumValue};
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
    if (automatic) {
        if (config.input_source != TranslationInputSource::Axis) {
            return std::unexpected{FlightModelConfigError::InvalidAutomaticChannel};
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

[[nodiscard]] auto validate_axis(TranslationAxisConfig const& config) noexcept
    -> std::expected<void, FlightModelConfigError> {
    if (auto const result{validate_channel(config.manual, false)}; !result) {
        return result;
    }
    if (auto const result{validate_channel(config.automatic, true)}; !result) {
        return result;
    }
    if (is_velocity_target(config.manual.semantic) &&
        is_velocity_target(config.automatic.semantic)) {
        return std::unexpected{FlightModelConfigError::AmbiguousTargetChannels};
    }
    if (!valid_enum(config.passive_drag_reference_frame) ||
        !valid_enum(config.active_stabilization_reference_frame)) {
        return std::unexpected{FlightModelConfigError::InvalidEnumValue};
    }
    return {};
}

[[nodiscard]] auto validate_axis(RotationAxisConfig const& config) noexcept
    -> std::expected<void, FlightModelConfigError> {
    if (!valid_enum(config.manual_semantic)) {
        return std::unexpected{FlightModelConfigError::InvalidEnumValue};
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
    result.rate_limited = {NonNegativeFinite<float>::make(increasing_rate),
                           NonNegativeFinite<float>::make(decreasing_rate)};
    return result;
}

void configure_common_rotation(FlightModelConfig& config) {
    config.rotation.pitch.manual_semantic = RotationSemantic::TargetAngularVelocity;
    config.rotation.pitch.maximum_rate = NonNegativeFinite<float>{60.f};
    config.rotation.yaw.manual_semantic = RotationSemantic::TargetAngularVelocity;
    config.rotation.yaw.maximum_rate = NonNegativeFinite<float>{60.f};

    auto& roll{config.rotation.roll};
    roll.stabilization.enabled = true;
    roll.stabilization.delay = NonNegativeFinite<float>{1.f};
    roll.stabilization.response = make_rate_limited_response(10.f, 10.f);
}

void configure_common_actions(FlightModelConfig& config) {
    config.boost.available = true;
    config.boost.energy_drain_per_second = NonNegativeFinite<float>{1.f / 4.f};
    config.brake.available = true;
    config.emergency_brake.available = true;
    config.emergency_brake.deceleration = NonNegativeFinite<float>{32000.f};
    config.emergency_brake.energy_drain_per_second = NonNegativeFinite<float>{1.f / 6.f};
    config.energy_recharge_per_second = NonNegativeFinite<float>{1.f / 7.f};
}

[[nodiscard]] auto make_starfox_config() -> FlightModelConfig {
    FlightModelConfig result;
    auto& forward{result.translation.forward};
    forward.automatic.semantic = TranslationSemantic::TargetVelocity;
    forward.automatic.automatic_value = Bounded<float, -1, 1>{1.f};
    forward.automatic.response = make_second_order_response();
    forward.normal.positive_target_speed = NonNegativeFinite<float>{12000.f};
    forward.normal.positive_speed_limit = NonNegativeFinite<float>{12000.f};
    forward.normal.negative_speed_limit = NonNegativeFinite<float>{0.f};
    forward.boosted.positive_target_speed = NonNegativeFinite<float>{30000.f};
    forward.boosted.positive_speed_limit = NonNegativeFinite<float>{30000.f};
    forward.boosted.negative_speed_limit = NonNegativeFinite<float>{0.f};

    result.facing_velocity.mode = FacingVelocityCoupling::LockedToFacing;
    result.boost.accelerator_activates_boost = true;
    result.boost.response = make_second_order_response();
    result.brake.target_speed = NonNegativeFinite<float>{1000.f};
    result.brake.deceleration = NonNegativeFinite<float>{14000.f};
    result.brake.response = make_second_order_response();
    result.emergency_brake.target_speed = NonNegativeFinite<float>{1000.f};
    result.emergency_brake.response = make_second_order_response();
    result.maximum_resultant_speed = NonNegativeFinite<float>{12000.f};
    result.boosted_maximum_resultant_speed = NonNegativeFinite<float>{30000.f};
    configure_common_actions(result);
    result.brake.energy_drain_per_second = NonNegativeFinite<float>{1.f / 6.f};
    configure_common_rotation(result);
    return result;
}

[[nodiscard]] auto make_fighter_config() -> FlightModelConfig {
    FlightModelConfig result;
    auto& forward{result.translation.forward};
    forward.manual.semantic = TranslationSemantic::Acceleration;
    forward.manual.input_source = TranslationInputSource::Accelerator;
    forward.normal.positive_speed_limit = NonNegativeFinite<float>{8000.f};
    forward.normal.negative_speed_limit = NonNegativeFinite<float>{0.f};
    forward.normal.positive_acceleration = NonNegativeFinite<float>{10000.f};
    forward.normal.negative_acceleration = NonNegativeFinite<float>{10000.f};
    forward.boosted.positive_speed_limit = NonNegativeFinite<float>{16000.f};
    forward.boosted.negative_speed_limit = NonNegativeFinite<float>{0.f};
    forward.boosted.positive_acceleration = NonNegativeFinite<float>{20000.f};
    forward.boosted.negative_acceleration = NonNegativeFinite<float>{20000.f};
    forward.passive_drag = NonNegativeFinite<float>{2000.f};

    result.facing_velocity.mode = FacingVelocityCoupling::LockedToFacing;
    result.brake.deceleration = NonNegativeFinite<float>{14000.f};
    result.emergency_brake.deceleration = NonNegativeFinite<float>{32000.f};
    result.maximum_resultant_speed = NonNegativeFinite<float>{8000.f};
    result.boosted_maximum_resultant_speed = NonNegativeFinite<float>{16000.f};
    configure_common_actions(result);
    configure_common_rotation(result);
    result.rotation.roll.manual_semantic = RotationSemantic::TargetAngularVelocity;
    result.rotation.roll.maximum_rate = NonNegativeFinite<float>{90.f};
    return result;
}

[[nodiscard]] auto make_skater_config() -> FlightModelConfig {
    auto result{make_fighter_config()};
    result.translation.forward.passive_drag = NonNegativeFinite<float>{0.f};
    result.facing_velocity.mode = FacingVelocityCoupling::Independent;
    result.maximum_resultant_speed = NonNegativeFinite<float>{effectively_unlimited_speed};
    result.boosted_maximum_resultant_speed = NonNegativeFinite<float>{effectively_unlimited_speed};
    return result;
}

[[nodiscard]] auto make_gunship_config() -> FlightModelConfig {
    FlightModelConfig result;
    auto configure_axis = [](TranslationAxisConfig& axis, NonNegativeFinite<float> const speed) {
        auto const boosted_speed{NonNegativeFinite<float>::make(speed * 2.f)};
        axis.manual.semantic = TranslationSemantic::TargetVelocity;
        axis.manual.response = make_rate_limited_response(10000.f, 14000.f);
        axis.normal.positive_target_speed = speed;
        axis.normal.negative_target_speed = speed;
        axis.normal.positive_speed_limit = speed;
        axis.normal.negative_speed_limit = speed;
        axis.boosted.positive_target_speed = boosted_speed;
        axis.boosted.negative_target_speed = boosted_speed;
        axis.boosted.positive_speed_limit = boosted_speed;
        axis.boosted.negative_speed_limit = boosted_speed;
    };
    configure_axis(result.translation.forward, NonNegativeFinite<float>{8000.f});
    configure_axis(result.translation.right, NonNegativeFinite<float>{3000.f});
    configure_axis(result.translation.up, NonNegativeFinite<float>{3000.f});
    result.boost.response = make_rate_limited_response(10000.f, 14000.f);

    result.facing_velocity.mode = FacingVelocityCoupling::Independent;
    result.brake.deceleration = NonNegativeFinite<float>{14000.f};
    result.emergency_brake.deceleration = NonNegativeFinite<float>{32000.f};
    result.maximum_resultant_speed = NonNegativeFinite<float>{9000.f};
    result.boosted_maximum_resultant_speed = NonNegativeFinite<float>{18000.f};
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
    if (auto const result{validate_response(config.facing_velocity.response)}; !result) {
        return result;
    }
    if (auto const result{validate_response(config.boost.response)}; !result) {
        return result;
    }
    for (auto const* const brake : std::array{&config.brake, &config.emergency_brake}) {
        if (auto const result{validate_response(brake->response)}; !result) {
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

auto matches_authored_flight_model_topology(FlightModelConfig const& config,
                                            FlightModelPreset const preset) -> bool {
    auto const& authored{make_flight_model_profile(preset).config};
    auto const same_channel = [](TranslationChannelConfig const& edited,
                                 TranslationChannelConfig const& original) {
        return edited.semantic == original.semantic &&
               edited.reference_frame == original.reference_frame &&
               edited.input_source == original.input_source;
    };
    auto const same_translation = [&](TranslationAxisConfig const& edited,
                                      TranslationAxisConfig const& original) {
        return same_channel(edited.manual, original.manual) &&
               same_channel(edited.automatic, original.automatic);
    };
    auto const same_rotation = [](RotationAxisConfig const& edited,
                                  RotationAxisConfig const& original) {
        return edited.manual_semantic == original.manual_semantic;
    };
    return same_translation(config.translation.forward, authored.translation.forward) &&
           same_translation(config.translation.right, authored.translation.right) &&
           same_translation(config.translation.up, authored.translation.up) &&
           same_rotation(config.rotation.pitch, authored.rotation.pitch) &&
           same_rotation(config.rotation.yaw, authored.rotation.yaw) &&
           same_rotation(config.rotation.roll, authored.rotation.roll);
}

auto make_default_flight_model_loadout() -> FlightModelLoadout {
    return {
        .profiles = {make_flight_model_profile(FlightModelPreset::Starfox),
                     make_flight_model_profile(FlightModelPreset::Fighter),
                     make_flight_model_profile(FlightModelPreset::Skater),
                     make_flight_model_profile(FlightModelPreset::Gunship)},
        .initial_slot = FlightModelSlot::First,
    };
}

auto flight_model_profile(FlightModelLoadout& loadout, FlightModelSlot const slot) noexcept
    -> FlightModelProfile& {
    auto const index{static_cast<std::size_t>(slot)};
    assert(index < loadout.profiles.size());
    return loadout.profiles[index];
}

auto flight_model_profile(FlightModelLoadout const& loadout, FlightModelSlot const slot) noexcept
    -> FlightModelProfile const& {
    auto const index{static_cast<std::size_t>(slot)};
    assert(index < loadout.profiles.size());
    return loadout.profiles[index];
}

auto try_set_flight_model_profile(FlightModelLoadout& loadout,
                                  FlightModelSlot const slot,
                                  FlightModelProfile profile) noexcept -> bool {
    if (!validate_flight_model_config(profile.config)) {
        return false;
    }
    flight_model_profile(loadout, slot) = profile;
    return true;
}
}
