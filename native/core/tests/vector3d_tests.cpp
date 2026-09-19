#include <sandbox/core/vector3d.h>

#include <gtest/gtest.h>

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
