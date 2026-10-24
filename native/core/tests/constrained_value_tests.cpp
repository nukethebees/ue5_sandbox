#include <ioj/constrained_numbers.h>
#include <ioj/constrained_strings.h>
#include <ioj/constrained_vectors.h>

#include <gtest/gtest.h>

#include <limits>

namespace ioj::tests {
static_assert(sizeof(FiniteVector<double, 3>) == sizeof(Vector3d));
static_assert(FiniteVector<double, 3>{Vector3d{1, 2, 3}}.get() == Vector3d{1, 2, 3});
static_assert(NonNegativeFinite<double>{0}.get() == 0);
static_assert(!std::default_initializable<Positive<int>>);
static_assert(!std::default_initializable<NonEmptyString>);
static_assert(!noexcept(std::declval<NonEmptyString const&>().get()));

TEST(ConstrainedValue, OwnsNonEmptyStringsAndSupportsIndependentCopies) {
    EXPECT_FALSE(NonEmptyString::try_make(""));
    std::string source(100, 'x');
    auto const value{NonEmptyString::make(source)};
    source.clear();
    auto copy{value.get()};
    copy.clear();
    EXPECT_EQ(value.get_ref(), std::string(100, 'x'));
    EXPECT_EQ(std::format("{}", value), value.get_ref());
}

TEST(ConstrainedValue, VectorGetReturnsAnIndependentValue) {
    auto const constrained{FiniteVector<double, 3>::make({1, 2, 3})};
    auto copy{constrained.get()};
    copy.x() = 9;
    EXPECT_EQ(constrained.get(), (Vector3d{1, 2, 3}));
    EXPECT_EQ(copy.x(), 9);
}

TEST(ConstrainedValue, RejectsNonFiniteVectorComponents) {
    using FinitePosition = FiniteVector<double, 3>;
    for (auto const invalid : {std::numeric_limits<double>::infinity(),
                               -std::numeric_limits<double>::infinity(),
                               std::numeric_limits<double>::quiet_NaN()}) {
        for (std::size_t component{}; component < 3; ++component) {
            Vector3d value{1, 2, 3};
            value[component] = invalid;
            EXPECT_FALSE(FinitePosition::try_make(value));
        }
    }
    EXPECT_TRUE(FinitePosition::try_make(
        {std::numeric_limits<double>::lowest(), 0, std::numeric_limits<double>::max()}));
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
}
