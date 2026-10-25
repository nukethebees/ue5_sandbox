#include <ioj/array_utils.h>
#include <ioj/vector_value_math.h>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>

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

TEST(NativeCoreArrayUtils, ComparesWithinTolerance) {
    struct Case {
        char const* name;
        ioj::Vector3f actual;
        bool expected;
    };
    ioj::Vector3f const reference{1.f, 2.f, 3.f};
    for (auto const& test : {Case{"within tolerance", {1.f, 2.00005f, 3.f}, true},
                             Case{"outside tolerance", {1.f, 2.001f, 3.f}, false}}) {
        SCOPED_TRACE(test.name);
        EXPECT_EQ(
            ioj::kernel::almost_equal(reference.elements.data(), test.actual.elements.data(), 3),
            test.expected);
        EXPECT_EQ(ioj::almost_equal(reference, test.actual, 1.e-4f), test.expected);
    }

    EXPECT_TRUE(ioj::almost_equal(ioj::Vector2d{1, 2}, ioj::Vector2d{1.5, 2}, 0.5));
    EXPECT_FALSE(ioj::almost_equal(ioj::Vector2d{1, 2}, ioj::Vector2d{1, 2.5}, 0.25));
}

TEST(NativeCoreArrayUtils, RejectsNanValues) {
    std::array<float, 1> const lhs{std::numeric_limits<float>::quiet_NaN()};
    std::array<float, 1> const rhs{std::numeric_limits<float>::quiet_NaN()};

    EXPECT_FALSE(ioj::kernel::almost_equal(lhs.data(), rhs.data(), 1));
    EXPECT_FALSE(ioj::almost_equal(ioj::Vector<float, 1>{lhs}, ioj::Vector<float, 1>{rhs}, 1.e-4f));
}

TEST(NativeCoreArrayUtils, RecognisesDescendingOrder) {
    std::array<std::int32_t, 0> const empty{};
    std::array<std::int32_t, 1> const single{1};
    std::array<std::int32_t, 4> const descending{3, 2, 2, -1};
    std::array<std::int32_t, 3> const ascending{-3, -2, -1};

    EXPECT_TRUE(ioj::kernel::is_sorted_desc(empty.data(), 0));
    EXPECT_TRUE(ioj::kernel::is_sorted_desc(single.data(), 1));
    EXPECT_TRUE(ioj::kernel::is_sorted_desc(descending.data(), 4));
    EXPECT_FALSE(ioj::kernel::is_sorted_desc(ascending.data(), 3));
}
