#include <ioj/container_ops.h>
#include <ioj/multi_buffer.h>
#include <ioj/soa_permutation.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <memory_resource>
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

TEST(NativeCoreContainerOps, RemovesSingleElementsFromBeginningMiddleAndEnd) {
    std::vector<int> values{0, 1, 2, 3, 4};
    ioj::remove_at_swap(values, 0);
    EXPECT_EQ(values, (std::vector<int>{4, 1, 2, 3}));

    values = {0, 1, 2, 3, 4};
    ioj::remove_at_swap(values, 2);
    EXPECT_EQ(values, (std::vector<int>{0, 1, 4, 3}));

    values = {0, 1, 2, 3, 4};
    ioj::remove_at_swap(values, 4);
    EXPECT_EQ(values, (std::vector<int>{0, 1, 2, 3}));
}

TEST(NativeCoreContainerOps, RemovesRangesOverlappingTheTail) {
    std::vector<int> values{0, 1, 2, 3, 4, 5, 6};
    ioj::remove_at_swap(values, 3, 3);
    EXPECT_EQ(values, (std::vector<int>{0, 1, 2, 6}));

    values = {0, 1, 2, 3, 4, 5, 6};
    ioj::remove_at_swap(values, 1, 4);
    EXPECT_EQ(values, (std::vector<int>{0, 5, 6}));

    values = {0, 1, 2, 3, 4, 5};
    ioj::remove_at_swap(values, 3, 3);
    EXPECT_EQ(values, (std::vector<int>{0, 1, 2}));

    ioj::remove_at_swap(values, 0, values.size());
    EXPECT_TRUE(values.empty());
}

TEST(NativeCoreContainerOps, ZeroLengthRemovalPreservesValues) {
    std::vector<int> values{0, 1, 2};
    for (std::size_t index{}; index <= values.size(); ++index) {
        ioj::remove_at_swap(values, index, 0);
        EXPECT_EQ(values, (std::vector<int>{0, 1, 2}));
    }

    values.clear();
    ioj::remove_at_swap(values, 0, 0);
    EXPECT_TRUE(values.empty());
}

#ifndef NDEBUG
TEST(NativeCoreContainerOps, RejectsRemovalOutsideTheVector) {
    std::vector<int> values{0, 1, 2};
    EXPECT_DEATH(ioj::remove_at_swap(values, 4, 0), "");
    EXPECT_DEATH(ioj::remove_at_swap(values, 3), "");
    EXPECT_DEATH(ioj::remove_at_swap(values, 1, 3), "");
}
#endif

TEST(NativeCoreContainerOps, PredicateRemovalWithNoMatchesPreservesValues) {
    std::vector<int> values{1, 3, 5};
    EXPECT_EQ(ioj::remove_at_swap_if(values, [](int value) { return value % 2 == 0; }), 0U);
    EXPECT_EQ(values, (std::vector<int>{1, 3, 5}));
}

TEST(NativeCoreContainerOps, PredicateRemovalReturnsTheNumberRemoved) {
    std::vector<int> values{0, 1, 2, 3, 4, 5};
    EXPECT_EQ(ioj::remove_at_swap_if(values, [](int value) { return value % 2 == 0; }), 3U);
    std::ranges::sort(values);
    EXPECT_EQ(values, (std::vector<int>{1, 3, 5}));
}

TEST(NativeCoreContainerOps, PredicateRemovalChecksEachElementOnceWithConsecutiveMatches) {
    std::vector<int> values{2, 4, 1, 6, 8, 3, 10, 12};
    auto const original{values};
    std::array<unsigned, 13> visits{};

    auto const removed{ioj::remove_at_swap_if(values, [&](int value) {
        ++visits[static_cast<std::size_t>(value)];
        return value % 2 == 0;
    })};

    EXPECT_EQ(removed, 6U);
    std::ranges::sort(values);
    EXPECT_EQ(values, (std::vector<int>{1, 3}));
    for (auto const value : original) {
        EXPECT_EQ(visits[static_cast<std::size_t>(value)], 1U);
    }
}

TEST(NativeCoreContainerOps, PredicateRemovalHandlesAllMatchesAndEmptyVectors) {
    std::vector<int> values{2, 4, 6};
    EXPECT_EQ(ioj::remove_at_swap_if(values, [](int value) { return value % 2 == 0; }), 3U);
    EXPECT_TRUE(values.empty());

    EXPECT_EQ(ioj::remove_at_swap_if(values,
                                     [](int) {
                                         ADD_FAILURE()
                                             << "Do not evaluate the predicate for an empty vector";
                                         return true;
                                     }),
              0U);
}

struct MoveOnlyElement {
    explicit MoveOnlyElement(int number)
        : value{std::make_unique<int>(number)} {}

    std::unique_ptr<int> value;
};

TEST(NativeCoreContainerOps, RemovesMoveOnlyNonDefaultConstructibleElements) {
    std::vector<MoveOnlyElement> values;
    for (int number{}; number < 6; ++number) {
        values.emplace_back(number);
    }

    ioj::remove_at_swap(values, 1, 2);
    ASSERT_EQ(values.size(), 4U);
    EXPECT_EQ(*values[0].value, 0);
    EXPECT_EQ(*values[1].value, 4);
    EXPECT_EQ(*values[2].value, 5);
    EXPECT_EQ(*values[3].value, 3);

    EXPECT_EQ(ioj::remove_at_swap_if(
                  values, [](MoveOnlyElement const& value) { return *value.value % 2 != 0; }),
              2U);
    ASSERT_EQ(values.size(), 2U);
    EXPECT_EQ(*values[0].value, 0);
    EXPECT_EQ(*values[1].value, 4);

    ioj::remove_at_swap(values, 1);
    ASSERT_EQ(values.size(), 1U);
    EXPECT_EQ(*values[0].value, 0);
    ioj::remove_at_swap(values, 0);
    EXPECT_TRUE(values.empty());
}

TEST(NativeCoreContainerOps, RemovesFromPmrVectorsWithoutAllocating) {
    std::array<std::byte, 256> buffer;
    std::pmr::monotonic_buffer_resource resource{
        buffer.data(), buffer.size(), std::pmr::null_memory_resource()};
    std::pmr::vector<int> values{{0, 1, 2, 3, 4, 5}, &resource};
    auto* const data{values.data()};
    auto const capacity{values.capacity()};

    ioj::remove_at_swap(values, 1, 2);
    EXPECT_TRUE(std::ranges::equal(values, std::array{0, 4, 5, 3}));
    EXPECT_EQ(ioj::remove_at_swap_if(values, [](int value) { return value % 2 != 0; }), 2U);
    EXPECT_TRUE(std::ranges::equal(values, std::array{0, 4}));
    EXPECT_EQ(values.data(), data);
    EXPECT_EQ(values.capacity(), capacity);
    EXPECT_EQ(values.get_allocator().resource(), &resource);
}
}
