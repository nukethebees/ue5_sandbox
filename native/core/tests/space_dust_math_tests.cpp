#include <ioj/space_dust_math.h>

#include <gtest/gtest.h>

TEST(NativeCoreSpaceDustMath, NormalisesTuningIdempotently) {
    ioj::space_dust::Tuning tuning;
    tuning.particle_count = -7;
    tuning.volume_dimensions = ioj::Vector3f{-2.0f, 0.0f, -30.0f};
    tuning.particle_size = -1.0f;
    tuning.brightness = -2.0f;
    tuning.colour = ioj::Vector3f{-1.0f, -2.0f, -3.0f};
    tuning.minimum_visible_speed = -100.0f;
    tuning.full_visible_speed = -200.0f;
    tuning.streak_seconds = -0.1f;
    tuning.minimum_motion_pixels = -2.0f;
    tuning.full_motion_pixels = -4.0f;
    tuning.maximum_streak_pixels = -3.0f;
    tuning.volume_edge_fade_fraction = 1.0f;

    auto const normalised{ioj::space_dust::normalise_tuning(tuning)};
    auto const normalised_again{ioj::space_dust::normalise_tuning(normalised)};

    EXPECT_EQ(normalised.particle_count, 0);
    EXPECT_FLOAT_EQ(normalised.volume_dimensions.x(), 1.0f);
    EXPECT_FLOAT_EQ(normalised.volume_dimensions.y(), 1.0f);
    EXPECT_FLOAT_EQ(normalised.volume_dimensions.z(), 1.0f);
    EXPECT_FLOAT_EQ(normalised.brightness, 0.0f);
    EXPECT_GT(normalised.full_visible_speed, normalised.minimum_visible_speed);
    EXPECT_FLOAT_EQ(normalised.minimum_motion_pixels, 0.0f);
    EXPECT_GT(normalised.full_motion_pixels, normalised.minimum_motion_pixels);
    EXPECT_FLOAT_EQ(normalised.volume_edge_fade_fraction, 0.49f);
    EXPECT_EQ(normalised_again.particle_count, normalised.particle_count);
    EXPECT_EQ(normalised_again.volume_dimensions.x(), normalised.volume_dimensions.x());
    EXPECT_EQ(normalised_again.full_visible_speed, normalised.full_visible_speed);
    EXPECT_EQ(normalised_again.full_motion_pixels, normalised.full_motion_pixels);
}

TEST(NativeCoreSpaceDustMath, UsesSparseMotionDrivenDefaults) {
    ioj::space_dust::Tuning const tuning;

    EXPECT_EQ(tuning.particle_count, 96);
    EXPECT_FLOAT_EQ(tuning.minimum_visible_speed, 100.0f);
    EXPECT_FLOAT_EQ(tuning.full_visible_speed, 2000.0f);
    EXPECT_FLOAT_EQ(tuning.minimum_motion_pixels, 0.75f);
    EXPECT_FLOAT_EQ(tuning.full_motion_pixels, 4.0f);
}

TEST(NativeCoreSpaceDustMath, ClampsParticleCountToProductionMaximum) {
    ioj::space_dust::Tuning tuning;
    tuning.particle_count = 65536;

    auto const normalised{ioj::space_dust::normalise_tuning(tuning)};

    EXPECT_EQ(ioj::space_dust::maximum_particle_count, 2048);
    EXPECT_EQ(normalised.particle_count, ioj::space_dust::maximum_particle_count);
}

TEST(NativeCoreSpaceDustMath, OrdersMotionVisibilityThresholds) {
    ioj::space_dust::Tuning tuning;
    tuning.minimum_motion_pixels = 6.0f;
    tuning.full_motion_pixels = 2.0f;

    auto const normalised{ioj::space_dust::normalise_tuning(tuning)};

    EXPECT_FLOAT_EQ(normalised.minimum_motion_pixels, 6.0f);
    EXPECT_GT(normalised.full_motion_pixels, normalised.minimum_motion_pixels);
}

TEST(NativeCoreSpaceDustMath, WrapsLargeNegativeAndPeriodicLocations) {
    auto const dimensions{ioj::Vector3f{16000.0f, 12000.0f, 8000.0f}};
    auto const phase{ioj::space_dust::make_translation_phase(
        {16'000'000'125.0, -12'000'000'250.0, 8'000'000'999.0}, dimensions)};
    auto const repeated_phase{ioj::space_dust::make_translation_phase(
        {16'000'016'125.0, -12'000'012'250.0, 8'000'008'999.0}, dimensions)};

    EXPECT_FLOAT_EQ(phase.x(), 125.0f);
    EXPECT_FLOAT_EQ(phase.y(), 11750.0f);
    EXPECT_FLOAT_EQ(phase.z(), 999.0f);
    EXPECT_EQ(repeated_phase.x(), phase.x());
    EXPECT_EQ(repeated_phase.y(), phase.y());
    EXPECT_EQ(repeated_phase.z(), phase.z());
}

TEST(NativeCoreSpaceDustMath, ProducesStableSeededPositionsInsideTheUnitCube) {
    auto const first{ioj::space_dust::make_random_position(42u, 1337u)};
    auto const repeated{ioj::space_dust::make_random_position(42u, 1337u)};
    auto const different_seed{ioj::space_dust::make_random_position(42u, 1338u)};

    EXPECT_EQ(first.x(), repeated.x());
    EXPECT_EQ(first.y(), repeated.y());
    EXPECT_EQ(first.z(), repeated.z());
    EXPECT_NE(first.x(), different_seed.x());
    EXPECT_GE(first.x(), 0.0f);
    EXPECT_LT(first.x(), 1.0f);
    EXPECT_GE(first.y(), 0.0f);
    EXPECT_LT(first.y(), 1.0f);
    EXPECT_GE(first.z(), 0.0f);
    EXPECT_LT(first.z(), 1.0f);
}

TEST(NativeCoreSpaceDustMath, ProducesStableBoundedParticleVariation) {
    auto const first{ioj::space_dust::make_particle_variation(42u, 1337u)};
    auto const repeated{ioj::space_dust::make_particle_variation(42u, 1337u)};
    auto const different_particle{ioj::space_dust::make_particle_variation(43u, 1337u)};

    EXPECT_EQ(first.size_scale, repeated.size_scale);
    EXPECT_EQ(first.intensity_scale, repeated.intensity_scale);
    EXPECT_NE(first.size_scale, different_particle.size_scale);

    for (std::uint32_t instance_id{}; instance_id < 256u; ++instance_id) {
        auto const variation{ioj::space_dust::make_particle_variation(instance_id, 1337u)};
        EXPECT_GE(variation.size_scale, 0.8f);
        EXPECT_LE(variation.size_scale, 1.2f);
        EXPECT_GE(variation.intensity_scale, 0.65f);
        EXPECT_LE(variation.intensity_scale, 1.0f);
    }
}
