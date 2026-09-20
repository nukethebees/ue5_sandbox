#include <sandbox/core/array_math.h>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>

namespace {
TEST(NativeCoreArrayMath, CollectsOrderedRelativeIndices) {
    std::array const values{4, -2, 0, 7, -1};
    std::array<std::int32_t, values.size()> indices{};

    auto const count{
        ml::kernel::collect_indices_less_equal(values.data() + 1, 3, 0, indices.data())};

    ASSERT_EQ(count, 2);
    EXPECT_EQ(indices[0], 0);
    EXPECT_EQ(indices[1], 1);
}

TEST(NativeCoreArrayMath, DoesNotWriteIndicesWhenNothingMatches) {
    std::array const values{4, 7, 1};
    std::array<std::int32_t, values.size()> indices{99, 98, 97};

    EXPECT_EQ(ml::kernel::collect_indices_less_equal(values.data(), 3, 0, indices.data()), 0);
    EXPECT_EQ(indices, (std::array<std::int32_t, 3>{99, 98, 97}));
}

TEST(NativeCoreArrayMath, SumsEmptyAndSubranges) {
    std::array<std::int32_t, 0> const empty{};
    std::array const values{10, -3, 7, 99};

    EXPECT_EQ(ml::kernel::sum(empty.data(), 0), 0);
    EXPECT_EQ(ml::kernel::sum(values.data() + 1, 2), 4);
}
}
