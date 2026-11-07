#include <ioj/constrained_numbers.h>
#include <ioj/constrained_strings.h>
#include <ioj/constrained_vectors.h>

#include <gtest/gtest.h>

#include <limits>
#include <type_traits>
#include <utility>

namespace ioj::tests {
static_assert(
    std::is_same_v<decltype(std::declval<FiniteVector<double, 3> const&>().get()), Vector3d>);
static_assert(sizeof(FiniteVector<double, 3>) == sizeof(Vector3d));
static_assert(FiniteVector<double, 3>{Vector3d{1, 2, 3}}.get() == Vector3d{1, 2, 3});
static_assert(NonNegativeFinite<double>{0}.get() == 0);
static_assert(!std::default_initializable<Positive<int>>);
static_assert(!std::default_initializable<NonEmptyString>);
static_assert(!noexcept(std::declval<NonEmptyString const&>().get()));
static_assert(std::is_trivially_copyable_v<Positive<int>>);
static_assert(Bounded<std::uint32_t, 1, 64>{64}.get() == 64);
static_assert(!std::default_initializable<OpenInterval<float, 0, 1>>);

TEST(ConstrainedValue, OwnsNonEmptyStringsAndSupportsFormatting) {
    EXPECT_FALSE(NonEmptyString::try_make(""));
    std::string source(100, 'x');
    auto const value{NonEmptyString::make(source)};
    source.clear();
    EXPECT_EQ(value.ref(), std::string(100, 'x'));
    EXPECT_EQ(std::format("{}", value), value.ref());
}

TEST(ConstrainedValue, MovingAnOwnerPreservesBothInvariants) {
    auto source{NonEmptyString::make(std::string(100, 'x'))};
    auto destination{std::move(source)};
    EXPECT_EQ(source.ref(), destination.ref());

    auto assigned{NonEmptyString::make("original")};
    assigned = std::move(destination);
    EXPECT_EQ(assigned.ref(), source.ref());
    EXPECT_EQ(destination.ref(), source.ref());
}

TEST(ConstrainedValue, AppliesScalarConstraintToEveryVectorComponent) {
    using Dimensions = PositiveFiniteVector<double, 3>;
    EXPECT_TRUE(Dimensions::try_make({1, 2, 3}));
    for (std::size_t component{}; component < 3; ++component) {
        Vector3d value{1, 2, 3};
        value[component] = 0;
        EXPECT_FALSE(Dimensions::try_make(value));
    }
}

TEST(ConstrainedValue, PreservesNumericPoliciesAndFormatting) {
    EXPECT_FALSE(NonNegativeFinite<double>::try_make(-1));
    EXPECT_FALSE(NonNegativeFinite<double>::try_make(std::numeric_limits<double>::infinity()));
    EXPECT_FALSE(NonNegative<double>::try_make(std::numeric_limits<double>::quiet_NaN()));
    EXPECT_TRUE(NonNegative<double>::try_make(std::numeric_limits<double>::infinity()));
    EXPECT_FALSE(Positive<int>::try_make(0));
    EXPECT_TRUE(NonNegative<int>::try_make(0));
    EXPECT_EQ(std::format("{}", Positive<int>::make(7)), "7");
}

TEST(ConstrainedValue, ChecksClosedOpenAndFiniteDomains) {
    using Width = Bounded<std::uint32_t, 1, 64>;
    EXPECT_FALSE(Width::try_make(0));
    EXPECT_TRUE(Width::try_make(1));
    EXPECT_TRUE(Width::try_make(64));
    EXPECT_FALSE(Width::try_make(65));
    using Damping = OpenInterval<float, 0, 1>;
    EXPECT_FALSE(Damping::try_make(0));
    EXPECT_TRUE(Damping::try_make(0.5f));
    EXPECT_FALSE(Damping::try_make(1));
    EXPECT_FALSE(Damping::try_make(std::numeric_limits<float>::quiet_NaN()));
    EXPECT_FALSE(Finite<float>::try_make(std::numeric_limits<float>::infinity()));
    EXPECT_TRUE(Finite<float>::try_make(-1.f));
}
}
