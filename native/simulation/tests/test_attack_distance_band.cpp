#include "support/simulation_test_support.h"

TEST(AttackDistanceBand, ValuesAreValid) {

    ml::simulation::AttackDistanceBand band;
    ml::simulation_tests::expect_true(band.values_are_valid(),
                                      "Default attack distance band is valid");

    band.minimum_ratio = 0.f;
    band.desired_ratio = 0.f;
    band.maximum_ratio = 1.f;
    ml::simulation_tests::expect_true(band.values_are_valid(),
                                      "Equal values and range boundaries are valid");

    band = ml::simulation::AttackDistanceBand{};
    band.minimum_ratio = 0.6f;
    ml::simulation_tests::expect_false(band.values_are_valid(), "Minimum cannot exceed desired");

    band = ml::simulation::AttackDistanceBand{};
    band.maximum_ratio = 0.45f;
    ml::simulation_tests::expect_false(band.values_are_valid(), "Desired cannot exceed maximum");

    band = ml::simulation::AttackDistanceBand{};
    band.minimum_ratio = -0.1f;
    ml::simulation_tests::expect_false(band.values_are_valid(), "Minimum cannot be negative");

    band = ml::simulation::AttackDistanceBand{};
    band.maximum_ratio = 1.1f;
    ml::simulation_tests::expect_false(band.values_are_valid(), "Maximum cannot exceed one");
}
