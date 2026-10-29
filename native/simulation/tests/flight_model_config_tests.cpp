#include <ioj/sim/player/flight_model_config.h>

#include <gtest/gtest.h>

#include <array>
#include <limits>

namespace ioj::sim::player::tests {
using enum FlightModelConfigError;
TEST(FlightModelConfig, DefaultLoadoutContainsFourValidDistinctPresets) {
    auto const loadout{make_default_flight_model_loadout()};
    EXPECT_EQ(loadout.initial_slot, FlightModelSlot::First);

    std::array const expected{
        FlightModelPreset::Starfox,
        FlightModelPreset::Fighter,
        FlightModelPreset::Skater,
        FlightModelPreset::Gunship,
    };
    std::array const slots{
        FlightModelSlot::First,
        FlightModelSlot::Second,
        FlightModelSlot::Third,
        FlightModelSlot::Fourth,
    };
    auto const slot_count{slots.size()};
    for (std::size_t index{}; index < slot_count; ++index) {
        auto const& profile{flight_model_profile(loadout, slots[index])};
        EXPECT_EQ(profile.base_preset, expected[index]);
        EXPECT_FALSE(profile.customized);
        EXPECT_TRUE(validate_flight_model_config(profile.config).has_value());
    }
}

TEST(FlightModelConfig, PresetsExpressDifferentVelocitySemantics) {
    auto const starfox{make_flight_model_profile(FlightModelPreset::Starfox)};
    auto const fighter{make_flight_model_profile(FlightModelPreset::Fighter)};
    auto const skater{make_flight_model_profile(FlightModelPreset::Skater)};
    auto const gunship{make_flight_model_profile(FlightModelPreset::Gunship)};
    auto const& starfox_forward{starfox.config.translation.forward};
    auto const& fighter_forward{fighter.config.translation.forward};
    auto const& skater_forward{skater.config.translation.forward};
    auto const& gunship_translation{gunship.config.translation};

    EXPECT_EQ(starfox_forward.automatic.semantic, TranslationSemantic::TargetVelocity);
    EXPECT_EQ(starfox_forward.automatic.response.mode, ResponseMode::SecondOrder);
    EXPECT_FLOAT_EQ(starfox_forward.automatic.automatic_value, 1.f);
    EXPECT_FLOAT_EQ(starfox_forward.normal.positive_target_speed, 12000.f);
    EXPECT_EQ(fighter_forward.manual.semantic, TranslationSemantic::Acceleration);
    EXPECT_EQ(fighter_forward.manual.input_source, TranslationInputSource::Accelerator);
    EXPECT_GT(fighter_forward.passive_drag, 0.f);
    EXPECT_EQ(fighter.config.facing_velocity.mode, FacingVelocityCoupling::LockedToFacing);
    EXPECT_EQ(skater_forward.manual.semantic, TranslationSemantic::Acceleration);
    EXPECT_EQ(skater_forward.passive_drag, 0.f);
    EXPECT_EQ(skater.config.facing_velocity.mode, FacingVelocityCoupling::Independent);
    EXPECT_EQ(skater.config.maximum_resultant_speed, effectively_unlimited_speed);
    EXPECT_EQ(skater.config.boosted_maximum_resultant_speed, effectively_unlimited_speed);
    EXPECT_EQ(gunship_translation.right.manual.semantic, TranslationSemantic::TargetVelocity);
    EXPECT_EQ(gunship_translation.forward.manual.input_source, TranslationInputSource::Axis);
    EXPECT_EQ(gunship_translation.up.manual.semantic, TranslationSemantic::TargetVelocity);
}

TEST(FlightModelConfig, ValidationRejectsInvalidEnumsAndIgnoredChannelFields) {
    auto config{make_flight_model_profile(FlightModelPreset::Gunship).config};
    config.translation.forward.manual.semantic = static_cast<TranslationSemantic>(255);
    EXPECT_EQ(validate_flight_model_config(config).error(), InvalidEnumValue);

    config = make_flight_model_profile(FlightModelPreset::Gunship).config;
    config.translation.forward.manual.automatic_value = 0.5f;
    EXPECT_EQ(validate_flight_model_config(config).error(), InvalidManualChannel);

    config = make_flight_model_profile(FlightModelPreset::Skater).config;
    config.translation.forward.manual.semantic = TranslationSemantic::TargetSpeed;
    EXPECT_EQ(validate_flight_model_config(config).error(), InvalidManualChannel);

    config = make_flight_model_profile(FlightModelPreset::Starfox).config;
    config.translation.forward.automatic.input_source = TranslationInputSource::Accelerator;
    EXPECT_EQ(validate_flight_model_config(config).error(), InvalidAutomaticChannel);

    config = make_flight_model_profile(FlightModelPreset::Starfox).config;
    config.translation.forward.automatic.automatic_value = 1.01f;
    EXPECT_EQ(validate_flight_model_config(config).error(), InputValueOutOfRange);

    config = make_flight_model_profile(FlightModelPreset::Fighter).config;
    config.translation.forward.passive_drag_reference_frame = static_cast<ReferenceFrame>(255);
    EXPECT_EQ(validate_flight_model_config(config).error(), InvalidEnumValue);

    config = make_flight_model_profile(FlightModelPreset::Fighter).config;
    config.translation.forward.active_stabilization_reference_frame =
        static_cast<ReferenceFrame>(255);
    EXPECT_EQ(validate_flight_model_config(config).error(), InvalidEnumValue);
}

TEST(FlightModelConfig, ValidationDefinesManualAndAutomaticComposition) {
    auto config{make_flight_model_profile(FlightModelPreset::Gunship).config};
    config.translation.forward.automatic.semantic = TranslationSemantic::TargetSpeed;
    EXPECT_EQ(validate_flight_model_config(config).error(), AmbiguousTargetChannels);

    config.translation.forward.automatic.semantic = TranslationSemantic::Acceleration;
    config.translation.forward.automatic.automatic_value = 0.5f;
    EXPECT_TRUE(validate_flight_model_config(config).has_value());

    config.translation.forward.manual.semantic = TranslationSemantic::Acceleration;
    EXPECT_TRUE(validate_flight_model_config(config).has_value());
}

TEST(FlightModelConfig, AcceleratorInputSourceIsValidOnEveryManualAxis) {
    auto config{make_flight_model_profile(FlightModelPreset::Skater).config};
    for (auto* const axis : std::array{
             &config.translation.forward, &config.translation.right, &config.translation.up}) {
        axis->manual.semantic = TranslationSemantic::Acceleration;
        axis->manual.input_source = TranslationInputSource::Accelerator;
        axis->normal.positive_acceleration = 100.f;
    }
    EXPECT_TRUE(validate_flight_model_config(config).has_value());
}

TEST(FlightModelConfig, ValidationRejectsUnsafeSecondOrderAndNumericValues) {
    auto config{make_flight_model_profile(FlightModelPreset::Starfox).config};
    config.translation.forward.automatic.response.second_order.settling_time = 0.f;
    EXPECT_EQ(validate_flight_model_config(config).error(), InvalidSecondOrderSettlingTime);

    config = make_flight_model_profile(FlightModelPreset::Starfox).config;
    config.translation.forward.automatic.response.second_order.damping_ratio = 1.f;
    EXPECT_EQ(validate_flight_model_config(config).error(), InvalidSecondOrderDampingRatio);

    config = make_flight_model_profile(FlightModelPreset::Skater).config;
    config.translation.forward.normal.positive_acceleration = -1.f;
    EXPECT_EQ(validate_flight_model_config(config).error(), NegativeValue);

    config = make_flight_model_profile(FlightModelPreset::Gunship).config;
    config.maximum_resultant_speed = std::numeric_limits<float>::infinity();
    EXPECT_EQ(validate_flight_model_config(config).error(), NonFiniteValue);
}
}
