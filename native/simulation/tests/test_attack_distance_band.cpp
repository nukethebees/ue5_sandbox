#include "support/simulation_test_support.h"

namespace ioj::sim::tests {

TEST(AttackDistanceBand, ValuesAreValid) {

    AttackDistanceBand band;
    tests::expect_true(band.values_are_valid(), "Default attack distance band is valid");

    band.minimum_ratio = 0.f;
    band.desired_ratio = 0.f;
    band.maximum_ratio = 1.f;
    tests::expect_true(band.values_are_valid(), "Equal values and range boundaries are valid");

    band = AttackDistanceBand{};
    band.minimum_ratio = 0.6f;
    tests::expect_false(band.values_are_valid(), "Minimum cannot exceed desired");

    band = AttackDistanceBand{};
    band.maximum_ratio = 0.45f;
    tests::expect_false(band.values_are_valid(), "Desired cannot exceed maximum");

    band = AttackDistanceBand{};
    band.minimum_ratio = -0.1f;
    tests::expect_false(band.values_are_valid(), "Minimum cannot be negative");

    band = AttackDistanceBand{};
    band.maximum_ratio = 1.1f;
    tests::expect_false(band.values_are_valid(), "Maximum cannot exceed one");
}

} // namespace tests
