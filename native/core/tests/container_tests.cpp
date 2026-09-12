#include <sandbox/core/fixed_array.h>
#include <sandbox/core/frame_array.h>
#include <sandbox/core/multi_buffer.h>
#include <sandbox/core/soa_permutation.h>

#include <gtest/gtest.h>

#include <array>
#include <memory_resource>
#include <string>
#include <vector>

namespace {
TEST(NativeCoreFixedArray, ManagesCapacityAndElementLifetime) {
    ml::FixedArray<std::string, 3> values{"alpha", "beta"};

    EXPECT_EQ(values.num(), 2);
    EXPECT_EQ(values.last(), "beta");

    values.emplace_back("gamma");
    EXPECT_TRUE(values.is_full());

    values.pop();
    EXPECT_EQ(values.num(), 2);
    values.reset();
    EXPECT_TRUE(values.is_empty());
}

TEST(NativeCoreFrameArray, UsesBorrowedPmrStorageAndRemovesBySwap) {
    std::array<std::byte, 1024> storage{};
    std::pmr::monotonic_buffer_resource resource{storage.data(), storage.size()};
    ml::FrameArray<int> values{&resource};

    values.add(10);
    values.add(20);
    values.add(30);
    values.remove_at_swap(1);

    ASSERT_EQ(values.num(), 2);
    EXPECT_EQ(values[0], 10);
    EXPECT_EQ(values[1], 30);
}

TEST(NativeCoreMultiBuffer, CyclesPreviousCurrentAndNextRoles) {
    ml::MultiBuffer<int, 3> buffers{{10, 20, 30}};

    EXPECT_EQ(buffers.previous(), 10);
    EXPECT_EQ(buffers.current(), 10);
    EXPECT_EQ(buffers.next(), 20);

    buffers.cycle();
    EXPECT_EQ(buffers.previous(), 10);
    EXPECT_EQ(buffers.current(), 20);
    EXPECT_EQ(buffers.next(), 30);
}

TEST(NativeCorePermutation, AppliesCyclesAndRestoresIndices) {
    std::vector<std::string> values{"zero", "one", "two", "three"};
    std::array<std::int32_t, 4> indices{2, 0, 3, 1};
    auto const original{indices};

    ml::apply_permutation(std::span{values}, std::span{indices});

    EXPECT_EQ(values, (std::vector<std::string>{"two", "zero", "three", "one"}));
    EXPECT_EQ(indices, original);
}
}
