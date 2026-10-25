#include <ioj/array_utils.h>
#include <ioj/vector_value_math.h>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <vector>

TEST(NativeCoreArrayUtils, AssignsSplitVectorColumns) {
    std::array<std::int32_t, 3> const source_x{1, 2, 3};
    std::array<std::int32_t, 3> const source_y{4, 5, 6};
    std::array<std::int32_t, 3> const source_z{7, 8, 9};
    std::array<std::int32_t, 3> destination_x{};
    std::array<std::int32_t, 3> destination_y{};
    std::array<std::int32_t, 3> destination_z{};

    ioj::kernel::assign_from(destination_x.data(),
                             destination_y.data(),
                             destination_z.data(),
                             source_x.data(),
                             source_y.data(),
                             source_z.data(),
                             3);

    EXPECT_EQ(destination_x, source_x);
    EXPECT_EQ(destination_y, source_y);
    EXPECT_EQ(destination_z, source_z);
}

TEST(NativeCoreArrayUtils, FillsScalarAndSplitArrays) {
    std::array<std::int32_t, 3> values{};
    ioj::kernel::fill(values.data(), 7, 3);
    EXPECT_EQ(values, (std::array<std::int32_t, 3>{7, 7, 7}));

    std::array<float, 2> xs{};
    std::array<float, 2> ys{};
    std::array<float, 2> zs{};
    ioj::kernel::fill(xs.data(), ys.data(), zs.data(), 2.5f, 2);
    EXPECT_EQ(xs, (std::array<float, 2>{2.5f, 2.5f}));
    EXPECT_EQ(ys, xs);
    EXPECT_EQ(zs, xs);
}

struct ArrayComparisonCase {
    char const* name;
    ioj::Vector3f actual;
    bool expected;
};
using ArrayComparison = testing::TestWithParam<ArrayComparisonCase>;

TEST_P(ArrayComparison, ComparesWithinTolerance) {
    auto const& test{GetParam()};
    ioj::Vector3f const reference{1.f, 2.f, 3.f};
    EXPECT_EQ(ioj::kernel::almost_equal(reference.elements.data(), test.actual.elements.data(), 3),
              test.expected);
    EXPECT_EQ(ioj::almost_equal(reference, test.actual, 1.e-4f), test.expected);
}

constexpr ArrayComparisonCase array_comparison_cases[]{
    {"WithinTolerance", {1.f, 2.00005f, 3.f}, true},
    {"OutsideTolerance", {1.f, 2.001f, 3.f}, false}};
INSTANTIATE_TEST_SUITE_P(NativeCoreArrayUtils,
                         ArrayComparison,
                         testing::ValuesIn(array_comparison_cases),
                         [](auto const& info) { return info.param.name; });

TEST(NativeCoreArrayUtils, ComparesDoubleVectorsWithExplicitTolerance) {
    EXPECT_TRUE(ioj::almost_equal(ioj::Vector2d{1, 2}, ioj::Vector2d{1.5, 2}, 0.5));
    EXPECT_FALSE(ioj::almost_equal(ioj::Vector2d{1, 2}, ioj::Vector2d{1, 2.5}, 0.25));
}

TEST(NativeCoreArrayUtils, RejectsNanValues) {
    std::array<float, 1> const lhs{std::numeric_limits<float>::quiet_NaN()};
    std::array<float, 1> const rhs{std::numeric_limits<float>::quiet_NaN()};

    EXPECT_FALSE(ioj::kernel::almost_equal(lhs.data(), rhs.data(), 1));
    EXPECT_FALSE(ioj::almost_equal(ioj::Vector<float, 1>{lhs}, ioj::Vector<float, 1>{rhs}, 1.e-4f));
}

struct DescendingOrderCase {
    std::vector<std::int32_t> input;
    bool expected;
};
using DescendingOrder = testing::TestWithParam<DescendingOrderCase>;

TEST_P(DescendingOrder, RecognisesDescendingOrder) {
    auto const& test{GetParam()};
    EXPECT_EQ(ioj::kernel::is_sorted_desc(test.input.data(),
                                          static_cast<std::int32_t>(test.input.size())),
              test.expected);
}

DescendingOrderCase const descending_order_cases[]{
    {{}, true}, {{1}, true}, {{3, 2, 2, -1}, true}, {{-3, -2, -1}, false}};
INSTANTIATE_TEST_SUITE_P(NativeCoreArrayUtils,
                         DescendingOrder,
                         testing::ValuesIn(descending_order_cases));
