#include <ioj/vector3d.h>

#include <gtest/gtest.h>

#include <limits>

template <typename T>
concept HasZComponent = requires(T value) { value.z(); };
template <typename T>
concept HasWComponent = requires(T value) { value.w(); };
static_assert(!HasZComponent<ioj::Vector<float, 2>>);
static_assert(HasZComponent<ioj::Vector<float, 3>>);
static_assert(!HasWComponent<ioj::Vector<float, 3>>);
static_assert(HasWComponent<ioj::Vector<float, 4>>);
static_assert(std::is_aggregate_v<ioj::Vector3d>);
static_assert(std::is_trivially_copyable_v<ioj::Vector3d>);
static_assert(sizeof(ioj::Vector3d) == sizeof(double) * 3);
static_assert(ioj::Vector<int, 3>{1, 2, 3} < ioj::Vector<int, 3>{1, 2, 4});
static_assert(std::same_as<decltype(std::declval<ioj::Vector3d&>().x()), double&>);
static_assert(std::same_as<decltype(std::declval<ioj::Vector3d const&>().x()), double const&>);

TEST(NativeCoreVector3d, MovesTowardsTargetWithoutOvershoot) {
    auto const current{ioj::Vector3d{3.0, 4.0, 0.0}};
    auto const target{ioj::Vector3d{0.0, 0.0, 0.0}};

    auto const stepped{ioj::move_towards(current, target, 2.0)};
    EXPECT_NEAR(stepped.x(), 1.8, 1.e-12);
    EXPECT_NEAR(stepped.y(), 2.4, 1.e-12);
    EXPECT_DOUBLE_EQ(stepped.z(), 0.0);
    EXPECT_EQ(ioj::move_towards(current, target, 5.0), target);
    EXPECT_EQ(ioj::move_towards(current, target, 6.0), target);
    EXPECT_EQ(ioj::move_towards(current, target, 0.0), current);
}

TEST(NativeCoreVector3d, ChecksFinitePositiveDimensions) {
    EXPECT_TRUE(ioj::is_finite(ioj::Vector3d{-1.0, 0.0, 3.0}));
    EXPECT_TRUE(ioj::is_positive_finite(ioj::Vector3d{1.0, 2.0, 3.0}));
    EXPECT_FALSE(ioj::is_positive_finite(ioj::Vector3d{0.0, 2.0, 3.0}));
    EXPECT_FALSE(ioj::is_positive_finite(ioj::Vector3d{1.0, -2.0, 3.0}));
    auto const infinity{std::numeric_limits<double>::infinity()};
    auto const nan{std::numeric_limits<double>::quiet_NaN()};
    ioj::Vector3d const unordered{nan, 0.0, 0.0};
    EXPECT_EQ(unordered <=> unordered, std::partial_ordering::unordered);
    for (auto const value : {ioj::Vector3d{infinity, 2.0, 3.0},
                             ioj::Vector3d{1.0, nan, 3.0},
                             ioj::Vector3d{1.0, 2.0, -infinity}}) {
        EXPECT_FALSE(ioj::is_finite(value));
        EXPECT_FALSE(ioj::is_positive_finite(value));
    }
}
