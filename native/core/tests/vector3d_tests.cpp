#include <sandbox/core/vector3d.h>

#include <gtest/gtest.h>

#include <limits>

TEST(NativeCoreVector3d, MovesTowardsTargetWithoutOvershoot) {
    auto const current{ml::Vector3d{3.0, 4.0, 0.0}};
    auto const target{ml::Vector3d{0.0, 0.0, 0.0}};

    auto const stepped{ml::move_towards(current, target, 2.0)};
    EXPECT_NEAR(stepped.x, 1.8, 1.e-12);
    EXPECT_NEAR(stepped.y, 2.4, 1.e-12);
    EXPECT_DOUBLE_EQ(stepped.z, 0.0);
    EXPECT_EQ(ml::move_towards(current, target, 5.0), target);
    EXPECT_EQ(ml::move_towards(current, target, 6.0), target);
    EXPECT_EQ(ml::move_towards(current, target, 0.0), current);
}

TEST(NativeCoreVector3d, ChecksFinitePositiveDimensions) {
    EXPECT_TRUE(ml::is_finite({-1.0, 0.0, 3.0}));
    EXPECT_TRUE(ml::is_positive_finite({1.0, 2.0, 3.0}));
    EXPECT_FALSE(ml::is_positive_finite({0.0, 2.0, 3.0}));
    EXPECT_FALSE(ml::is_positive_finite({1.0, -2.0, 3.0}));
    auto const infinity{std::numeric_limits<double>::infinity()};
    auto const nan{std::numeric_limits<double>::quiet_NaN()};
    for (auto const value : {ml::Vector3d{infinity, 2.0, 3.0},
                             ml::Vector3d{1.0, nan, 3.0},
                             ml::Vector3d{1.0, 2.0, -infinity}}) {
        EXPECT_FALSE(ml::is_finite(value));
        EXPECT_FALSE(ml::is_positive_finite(value));
    }
}
