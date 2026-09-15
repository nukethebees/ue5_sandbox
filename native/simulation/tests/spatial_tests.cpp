#include "ioj/sim/spatial.h"

#include <gtest/gtest.h>

#include <cmath>

namespace ioj::sim::tests {

TEST(Spatial, SamplesSphericalShellSquaredDistanceUniformly) {
    auto const minimum{
        sample_spherical_shell_point({1.0, 2.0, 3.0}, {1.0, 0.0, 0.0}, 10.0f, 20.0f, 0.0f)};
    auto const maximum{
        sample_spherical_shell_point({1.0, 2.0, 3.0}, {1.0, 0.0, 0.0}, 10.0f, 20.0f, 1.0f)};
    auto const midpoint{sample_spherical_shell_point({}, {1.0, 0.0, 0.0}, 10.0f, 20.0f, 0.5f)};

    EXPECT_DOUBLE_EQ(minimum.x, 11.0);
    EXPECT_DOUBLE_EQ(maximum.x, 21.0);
    EXPECT_DOUBLE_EQ(minimum.y, 2.0);
    EXPECT_NEAR(midpoint.x, std::sqrt(250.0), 1e-6);
}

TEST(Spatial, ClampsUnitSample) {
    auto const below{sample_spherical_shell_point({}, {0.0, 1.0, 0.0}, 2.0f, 4.0f, -1.0f)};
    auto const above{sample_spherical_shell_point({}, {0.0, 1.0, 0.0}, 2.0f, 4.0f, 2.0f)};

    EXPECT_DOUBLE_EQ(below.y, 2.0);
    EXPECT_DOUBLE_EQ(above.y, 4.0);
}

} // namespace tests
