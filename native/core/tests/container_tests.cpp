#include <ioj/container_ops.h>
#include <ioj/fixed_array.h>
#include <ioj/frame_array.h>
#include <ioj/frame_memory_resource.h>
#include <ioj/multi_buffer.h>
#include <ioj/soa_permutation.h>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <memory_resource>
#include <string>
#include <vector>

namespace {
TEST(NativeCoreFixedArray, ManagesCapacityAndElementLifetime) {
    ioj::FixedArray<std::string, 3> values{"alpha", "beta"};

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
    ioj::FrameArray<int> values{&resource};

    values.add(10);
    values.add(20);
    values.add(30);
    values.remove_at_swap(1);

    ASSERT_EQ(values.num(), 2);
    EXPECT_EQ(values[0], 10);
    EXPECT_EQ(values[1], 30);
}

TEST(NativeCoreFrameMemoryResource, AlignsTracksAndReclaimsAllocations) {
    alignas(ioj::FrameMemoryResource::backing_alignment) std::array<std::byte, 512> backing{};
    ioj::FrameMemoryResource resource{backing};

    auto* const first{resource.allocate(7, 1)};
    auto* const aligned{resource.allocate(32, 32)};

    EXPECT_TRUE(resource.owns(first));
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(aligned) % 32, 0);
    EXPECT_EQ(resource.get_stats().outstanding_allocation_count, 2);

    resource.deallocate(aligned, 32, 32);
    resource.deallocate(first, 7, 1);
    resource.reset();

    auto const stats{resource.get_stats()};
    EXPECT_EQ(stats.current_claimed_bytes, 0);
    EXPECT_EQ(stats.last_frame_payload_bytes, 39);
    EXPECT_EQ(stats.last_frame_root_claim_count, 2);
}

TEST(NativeCoreFrameMemoryResource, RecordsOverflowWithoutClaimingMemory) {
    alignas(ioj::FrameMemoryResource::backing_alignment) std::array<std::byte, 64> backing{};
    ioj::FrameMemoryResource resource{backing};

    EXPECT_EQ(resource.try_allocate(128, 16), nullptr);

    auto const stats{resource.get_stats()};
    EXPECT_EQ(stats.overflow_count, 1);
    EXPECT_EQ(stats.last_failure.requested_bytes, 128);
    EXPECT_EQ(stats.current_claimed_bytes, 0);
}

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
