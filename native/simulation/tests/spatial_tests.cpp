#include "sandbox/simulation/spatial.h"

#include <gtest/gtest.h>

#include <cmath>

TEST(Spatial, SamplesSphericalShellSquaredDistanceUniformly) {
    auto const minimum{ml::simulation::sample_spherical_shell_point(
        {1.0, 2.0, 3.0}, {1.0, 0.0, 0.0}, 10.0f, 20.0f, 0.0f)};
    auto const maximum{ml::simulation::sample_spherical_shell_point(
        {1.0, 2.0, 3.0}, {1.0, 0.0, 0.0}, 10.0f, 20.0f, 1.0f)};
    auto const midpoint{
        ml::simulation::sample_spherical_shell_point({}, {1.0, 0.0, 0.0}, 10.0f, 20.0f, 0.5f)};

    EXPECT_DOUBLE_EQ(minimum.x, 11.0);
    EXPECT_DOUBLE_EQ(maximum.x, 21.0);
    EXPECT_DOUBLE_EQ(minimum.y, 2.0);
    EXPECT_NEAR(midpoint.x, std::sqrt(250.0), 1e-6);
}

TEST(Spatial, ClampsUnitSample) {
    auto const below{
        ml::simulation::sample_spherical_shell_point({}, {0.0, 1.0, 0.0}, 2.0f, 4.0f, -1.0f)};
    auto const above{
        ml::simulation::sample_spherical_shell_point({}, {0.0, 1.0, 0.0}, 2.0f, 4.0f, 2.0f)};

    EXPECT_DOUBLE_EQ(below.y, 2.0);
    EXPECT_DOUBLE_EQ(above.y, 4.0);
}
