#include "support/simulation_test_support.h"

namespace ioj::sim::tests {

TEST(AttackDistanceBand, ValuesAreValid) {

    ioj::sim::AttackDistanceBand band;
    ioj::sim::tests::expect_true(band.values_are_valid(), "Default attack distance band is valid");

    band.minimum_ratio = 0.f;
    band.desired_ratio = 0.f;
    band.maximum_ratio = 1.f;
    ioj::sim::tests::expect_true(band.values_are_valid(),
                                 "Equal values and range boundaries are valid");

    band = ioj::sim::AttackDistanceBand{};
    band.minimum_ratio = 0.6f;
    ioj::sim::tests::expect_false(band.values_are_valid(), "Minimum cannot exceed desired");

    band = ioj::sim::AttackDistanceBand{};
    band.maximum_ratio = 0.45f;
    ioj::sim::tests::expect_false(band.values_are_valid(), "Desired cannot exceed maximum");

    band = ioj::sim::AttackDistanceBand{};
    band.minimum_ratio = -0.1f;
    ioj::sim::tests::expect_false(band.values_are_valid(), "Minimum cannot be negative");

    band = ioj::sim::AttackDistanceBand{};
    band.maximum_ratio = 1.1f;
    ioj::sim::tests::expect_false(band.values_are_valid(), "Maximum cannot exceed one");
}

} // namespace ioj::sim::tests
