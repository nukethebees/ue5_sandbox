#include <sandbox/core/array_utils.h>

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

    ml::kernel::assign_from(destination_x.data(),
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
    ml::kernel::fill(values.data(), 7, 3);
    EXPECT_EQ(values, (std::array<std::int32_t, 3>{7, 7, 7}));

    std::array<float, 2> xs{};
    std::array<float, 2> ys{};
    std::array<float, 2> zs{};
    ml::kernel::fill(xs.data(), ys.data(), zs.data(), 2.5f, 2);
    EXPECT_EQ(xs, (std::array<float, 2>{2.5f, 2.5f}));
    EXPECT_EQ(ys, xs);
    EXPECT_EQ(zs, xs);
}

TEST(NativeCoreArrayUtils, ComparesWithinTolerance) {
    std::array<float, 3> const lhs{1.0f, 2.0f, 3.0f};
    std::array<float, 3> const close{1.0f, 2.00005f, 3.0f};
    std::array<float, 3> const far{1.0f, 2.001f, 3.0f};

    EXPECT_TRUE(ml::kernel::almost_equal(lhs.data(), close.data(), 3));
    EXPECT_FALSE(ml::kernel::almost_equal(lhs.data(), far.data(), 3));
}

TEST(NativeCoreArrayUtils, RejectsNanValues) {
    std::array<float, 1> const lhs{std::numeric_limits<float>::quiet_NaN()};
    std::array<float, 1> const rhs{std::numeric_limits<float>::quiet_NaN()};

    EXPECT_FALSE(ml::kernel::almost_equal(lhs.data(), rhs.data(), 1));
}

TEST(NativeCoreArrayUtils, RecognisesDescendingOrder) {
    std::array<std::int32_t, 0> const empty{};
    std::array<std::int32_t, 1> const single{1};
    std::array<std::int32_t, 4> const descending{3, 2, 2, -1};
    std::array<std::int32_t, 3> const ascending{-3, -2, -1};

    EXPECT_TRUE(ml::kernel::is_sorted_desc(empty.data(), 0));
    EXPECT_TRUE(ml::kernel::is_sorted_desc(single.data(), 1));
    EXPECT_TRUE(ml::kernel::is_sorted_desc(descending.data(), 4));
    EXPECT_FALSE(ml::kernel::is_sorted_desc(ascending.data(), 3));
}
