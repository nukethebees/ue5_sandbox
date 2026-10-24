#include <ioj/quaternion.h>
#include <ioj/rotator_types.h>
#include <ioj/vector_value_math.h>

#include <gtest/gtest.h>

#include <limits>

namespace numeric_conversion_test {
struct Fields {
    double x{1.25};
    double y{2.5};
    double z{3.75};
    double w{4.5};
};
struct UpperFields {
    double X{1.25};
    double Y{2.5};
    double Z{3.75};
    double W{4.5};
};
struct Accessors {
    constexpr auto x() const { return 1.25; }
    constexpr auto y() const { return 2.5; }
    constexpr auto z() const { return 3.75; }
    constexpr auto w() const { return 4.5; }
};
struct UpperAccessors {
    constexpr auto X() const { return 1.25; }
    constexpr auto Y() const { return 2.5; }
    constexpr auto Z() const { return 3.75; }
    constexpr auto W() const { return 4.5; }
};
struct EulerFields {
    double pitch{1.25};
    double yaw{2.5};
    double roll{3.75};
};
struct UpperEulerFields {
    double Pitch{1.25};
    double Yaw{2.5};
    double Roll{3.75};
};
struct EulerAccessors {
    constexpr auto pitch() const { return 1.25; }
    constexpr auto yaw() const { return 2.5; }
    constexpr auto roll() const { return 3.75; }
};
struct UpperEulerAccessors {
    constexpr auto Pitch() const { return 1.25; }
    constexpr auto Yaw() const { return 2.5; }
    constexpr auto Roll() const { return 3.75; }
};
}

TEST(NativeCoreVector3d, ExplicitStructuralConversionsPreserveComponentsAndDefaults) {
    using namespace ioj;
    using namespace numeric_conversion_test;
    auto const check{[]<typename Source>(Source const source) {
        static_assert(!std::is_convertible_v<Source, Vector3f>);
        static_assert(!std::is_convertible_v<Source, Quaternion4f>);
        EXPECT_EQ((Vector<float, 1>{source}), (Vector<float, 1>{1.25f}));
        EXPECT_EQ(Vector2f{source}, (Vector2f{1.25f, 2.5f}));
        EXPECT_EQ(Vector3f{source}, (Vector3f{1.25f, 2.5f, 3.75f}));
        EXPECT_EQ(Quaternion4f{source}, (Quaternion4f{1.25f, 2.5f, 3.75f, 4.5f}));
    }};
    check(Fields{});
    check(UpperFields{});
    check(Accessors{});
    check(UpperAccessors{});
    check(Quaternion4d{1.25, 2.5, 3.75, 4.5});
    EXPECT_EQ(Vector3f{}, (Vector3f{0.f, 0.f, 0.f}));
    EXPECT_EQ(Quaternion4f{}, (Quaternion4f{0.f, 0.f, 0.f, 1.f}));
    static_assert(!std::is_constructible_v<Vector3f, Vector2f>);
    static_assert(std::is_trivially_copyable_v<Quaternion4f>);
    static_assert(sizeof(Quaternion4f) == 16 && alignof(Quaternion4f) == 16);
    static_assert(Vector3f{UpperFields{}}.z() == 3.75f);
}

TEST(NativeCoreVector3d, ExplicitEulerConversionsDelegateToComponentConstruction) {
    using namespace ioj;
    using namespace numeric_conversion_test;
    auto const check{[]<typename Source>(Source const source) {
        static_assert(!std::is_convertible_v<Source, Rotator3f>);
        EXPECT_EQ(Rotator3f{source}, (Rotator3f{1.25f, 2.5f, 3.75f}));
    }};
    check(EulerFields{});
    check(UpperEulerFields{});
    check(numeric_conversion_test::EulerAccessors{});
    check(UpperEulerAccessors{});
    check(Rotator3d{1.25, 2.5, 3.75});
    EXPECT_EQ(Rotator3f{}, (Rotator3f{0, 0, 0}));
    static_assert(std::is_trivially_copyable_v<Rotator3f>);
    static_assert(sizeof(Rotator3f) == 3 * sizeof(float));
    static_assert(Rotator3f{UpperEulerFields{}}.yaw == 2.5f);
}

template <typename T>
concept HasZComponent = requires(T value) { value.z(); };
template <typename T>
concept HasWComponent = requires(T value) { value.w(); };
static_assert(!HasZComponent<ioj::Vector<float, 2>>);
static_assert(HasZComponent<ioj::Vector<float, 3>>);
static_assert(!HasWComponent<ioj::Vector<float, 3>>);
static_assert(HasWComponent<ioj::Vector<float, 4>>);
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
