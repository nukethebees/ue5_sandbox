#include <ioj/container_ops.h>
#include <ioj/multi_buffer.h>
#include <ioj/soa_permutation.h>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace {
TEST(NativeCoreMultiBuffer, CyclesPreviousCurrentAndNextRoles) {
    ioj::MultiBuffer<int, 3> buffers{{10, 20, 30}};

    auto* const first{&buffers.current()};
    auto* const second{&buffers.next()};
    EXPECT_EQ(buffers.previous(), 10);
    EXPECT_EQ(buffers.current(), 10);
    EXPECT_EQ(buffers.next(), 20);

    buffers.cycle();
    auto* const third{&buffers.next()};
    EXPECT_EQ(buffers.previous(), 10);
    EXPECT_EQ(buffers.current(), 20);
    EXPECT_EQ(buffers.next(), 30);

    buffers.cycle();
    EXPECT_EQ(buffers.current(), 30);
    EXPECT_EQ(buffers.next(), 10);
    EXPECT_EQ(&buffers.next(), first);
    EXPECT_NE(first, second);
    EXPECT_NE(first, third);
    EXPECT_NE(second, third);
}

TEST(NativeCorePermutation, AppliesCyclesAndRestoresIndices) {
    std::vector<std::string> values{"zero", "one", "two", "three"};
    std::array<std::int32_t, 4> indices{2, 0, 3, 1};
    auto const original{indices};

    ioj::apply_permutation(std::span{values}, std::span{indices});

    EXPECT_EQ(values, (std::vector<std::string>{"two", "zero", "three", "one"}));
    EXPECT_EQ(indices, original);
}

TEST(NativeCorePermutation, UnsignedCyclesRestoreIndicesForTheNextColumn) {
    std::vector<std::string> values{"zero", "one", "two", "three", "four", "five"};
    std::vector<int> numbers{0, 1, 2, 3, 4, 5};
    std::array<std::uint32_t, 6> indices{1, 0, 2, 5, 3, 4};
    auto const original{indices};

    ioj::apply_permutation(std::span{values}, std::span{indices});

    EXPECT_EQ(values, (std::vector<std::string>{"one", "zero", "two", "five", "three", "four"}));
    EXPECT_EQ(indices, original);

    ioj::apply_permutation(std::span{numbers}, std::span{indices});

    EXPECT_EQ(numbers, (std::vector<int>{1, 0, 2, 5, 3, 4}));
    EXPECT_EQ(indices, original);
}

TEST(NativeCorePermutation, UnsignedEmptyAndIdentityPermutationsPreserveValues) {
    std::vector<int> values;
    std::span<std::uint32_t> empty_indices;
    ioj::apply_permutation(std::span{values}, empty_indices);
    EXPECT_TRUE(values.empty());

    values = {10, 20, 30};
    std::array<std::uint32_t, 3> indices{0, 1, 2};
    auto const original{indices};
    ioj::apply_permutation(std::span{values}, std::span{indices});
    EXPECT_EQ(values, (std::vector<int>{10, 20, 30}));
    EXPECT_EQ(indices, original);
}

#ifndef NDEBUG
TEST(NativeCorePermutation, RejectsInvalidUnsignedIndices) {
    std::vector<int> values{10, 20, 30};
    std::array<std::uint32_t, 3> duplicate{0, 0, 2};
    std::array<std::uint32_t, 3> out_of_bounds{0, 3, 2};
    std::array<std::uint32_t, 3> reserved_bit{0, 0x80000000u, 2};

    EXPECT_DEATH(ioj::apply_permutation(std::span{values}, std::span{duplicate}), "");
    EXPECT_DEATH(ioj::apply_permutation(std::span{values}, std::span{out_of_bounds}), "");
    EXPECT_DEATH(ioj::apply_permutation(std::span{values}, std::span{reserved_bit}), "");
}
#endif

TEST(NativeCoreContainerOps, RemovesRangeByMovingTailIntoRemovedRange) {
    std::vector<int> values{0, 1, 2, 3, 4, 5};

    ioj::remove_at_swap(values, 1, 2);

    EXPECT_EQ(values, (std::vector<int>{0, 4, 5, 3}));
}
}
