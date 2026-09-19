#include <sandbox/core/space_dust_math.h>

#include <gtest/gtest.h>

TEST(NativeCoreSpaceDustMath, NormalisesTuningIdempotently) {
    ml::space_dust::Tuning tuning;
    tuning.particle_count = -7;
    tuning.volume_dimensions = ml::make_vector3f(-2.0f, 0.0f, -30.0f);
    tuning.particle_size = -1.0f;
    tuning.brightness = -2.0f;
    tuning.colour = ml::make_vector3f(-1.0f, -2.0f, -3.0f);
    tuning.minimum_visible_speed = -100.0f;
    tuning.full_visible_speed = -200.0f;
    tuning.streak_seconds = -0.1f;
    tuning.minimum_motion_pixels = -2.0f;
    tuning.full_motion_pixels = -4.0f;
    tuning.maximum_streak_pixels = -3.0f;
    tuning.volume_edge_fade_fraction = 1.0f;

    auto const normalised{ml::space_dust::normalise_tuning(tuning)};
    auto const normalised_again{ml::space_dust::normalise_tuning(normalised)};

    EXPECT_EQ(normalised.particle_count, 0);
    EXPECT_FLOAT_EQ(normalised.volume_dimensions.X, 1.0f);
    EXPECT_FLOAT_EQ(normalised.volume_dimensions.Y, 1.0f);
    EXPECT_FLOAT_EQ(normalised.volume_dimensions.Z, 1.0f);
    EXPECT_FLOAT_EQ(normalised.brightness, 0.0f);
    EXPECT_GT(normalised.full_visible_speed, normalised.minimum_visible_speed);
    EXPECT_FLOAT_EQ(normalised.minimum_motion_pixels, 0.0f);
    EXPECT_GT(normalised.full_motion_pixels, normalised.minimum_motion_pixels);
    EXPECT_FLOAT_EQ(normalised.volume_edge_fade_fraction, 0.49f);
    EXPECT_EQ(normalised_again.particle_count, normalised.particle_count);
    EXPECT_EQ(normalised_again.volume_dimensions.X, normalised.volume_dimensions.X);
    EXPECT_EQ(normalised_again.full_visible_speed, normalised.full_visible_speed);
    EXPECT_EQ(normalised_again.full_motion_pixels, normalised.full_motion_pixels);
}

TEST(NativeCoreSpaceDustMath, UsesSparseMotionDrivenDefaults) {
    ml::space_dust::Tuning const tuning;

    EXPECT_EQ(tuning.particle_count, 96);
    EXPECT_FLOAT_EQ(tuning.minimum_motion_pixels, 0.75f);
    EXPECT_FLOAT_EQ(tuning.full_motion_pixels, 4.0f);
}

TEST(NativeCoreSpaceDustMath, OrdersMotionVisibilityThresholds) {
    ml::space_dust::Tuning tuning;
    tuning.minimum_motion_pixels = 6.0f;
    tuning.full_motion_pixels = 2.0f;

    auto const normalised{ml::space_dust::normalise_tuning(tuning)};

    EXPECT_FLOAT_EQ(normalised.minimum_motion_pixels, 6.0f);
    EXPECT_GT(normalised.full_motion_pixels, normalised.minimum_motion_pixels);
}

TEST(NativeCoreSpaceDustMath, WrapsLargeNegativeAndPeriodicLocations) {
    auto const dimensions{ml::make_vector3f(16000.0f, 12000.0f, 8000.0f)};
    auto const phase{ml::space_dust::make_translation_phase(
        {.x = 16'000'000'125.0, .y = -12'000'000'250.0, .z = 8'000'000'999.0}, dimensions)};
    auto const repeated_phase{ml::space_dust::make_translation_phase(
        {.x = 16'000'016'125.0, .y = -12'000'012'250.0, .z = 8'000'008'999.0}, dimensions)};

    EXPECT_FLOAT_EQ(phase.X, 125.0f);
    EXPECT_FLOAT_EQ(phase.Y, 11750.0f);
    EXPECT_FLOAT_EQ(phase.Z, 999.0f);
    EXPECT_EQ(repeated_phase.X, phase.X);
    EXPECT_EQ(repeated_phase.Y, phase.Y);
    EXPECT_EQ(repeated_phase.Z, phase.Z);
}

TEST(NativeCoreSpaceDustMath, ProducesStableSeededPositionsInsideTheUnitCube) {
    auto const first{ml::space_dust::make_random_position(42u, 1337u)};
    auto const repeated{ml::space_dust::make_random_position(42u, 1337u)};
    auto const different_seed{ml::space_dust::make_random_position(42u, 1338u)};

    EXPECT_EQ(first.X, repeated.X);
    EXPECT_EQ(first.Y, repeated.Y);
    EXPECT_EQ(first.Z, repeated.Z);
    EXPECT_NE(first.X, different_seed.X);
    EXPECT_GE(first.X, 0.0f);
    EXPECT_LT(first.X, 1.0f);
    EXPECT_GE(first.Y, 0.0f);
    EXPECT_LT(first.Y, 1.0f);
    EXPECT_GE(first.Z, 0.0f);
    EXPECT_LT(first.Z, 1.0f);
}
