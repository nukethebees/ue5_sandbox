#include <sandbox/core/frame_array.h>
#include <sandbox/core/frame_memory_resource.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <memory_resource>
#include <new>
#include <span>
#include <stdexcept>
#include <thread>
#include <vector>

namespace native_core_frame_memory_resource_tests {
struct alignas(256) OverAlignedValue {
    std::uint64_t value{};
};

struct TrackedValue {
    TrackedValue() { ++live_count; }
    TrackedValue(TrackedValue const&) { ++live_count; }
    TrackedValue(TrackedValue&&) noexcept { ++live_count; }
    ~TrackedValue() { --live_count; }

    inline static std::int32_t live_count{};
};

class FrameBacking {
  public:
    explicit FrameBacking(std::size_t const capacity)
        : data_{static_cast<std::byte*>(::operator new(
              capacity, std::align_val_t{ml::FrameMemoryResource::backing_alignment}))}
        , capacity_{capacity} {}

    ~FrameBacking() {
        ::operator delete(data_, std::align_val_t{ml::FrameMemoryResource::backing_alignment});
    }

    FrameBacking(FrameBacking const&) = delete;
    FrameBacking(FrameBacking&&) = delete;

    operator std::span<std::byte>() const noexcept { return {data_, capacity_}; }
  private:
    std::byte* data_{};
    std::size_t capacity_{};
};

class CountingResource final : public std::pmr::memory_resource {
  public:
    std::uint64_t allocation_count{};
  private:
    auto do_allocate(std::size_t const bytes, std::size_t const alignment) -> void* override {
        ++allocation_count;
        return std::pmr::new_delete_resource()->allocate(bytes, alignment);
    }

    void do_deallocate(void* const pointer,
                       std::size_t const bytes,
                       std::size_t const alignment) override {
        std::pmr::new_delete_resource()->deallocate(pointer, bytes, alignment);
    }

    auto do_is_equal(std::pmr::memory_resource const& other) const noexcept -> bool override {
        return this == &other;
    }
};
TEST(NativeCoreFrameMemoryResource, AlignsAllocationsIncludingZeroSizedRequests) {
    FrameBacking backing{4096};
    ml::FrameMemoryResource resource{backing};

    auto* const byte{resource.allocate(1, 1)};
    auto* const aligned{resource.allocate(sizeof(OverAlignedValue), alignof(OverAlignedValue))};
    auto* const zero_size{resource.allocate(0, 64)};

    EXPECT_TRUE(resource.owns(byte));
    EXPECT_TRUE(resource.owns(aligned));
    EXPECT_TRUE(resource.owns(zero_size));
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(aligned) % alignof(OverAlignedValue), 0);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(zero_size) % 64, 0);
    EXPECT_GT(resource.get_stats().current_padding_bytes, 0);

    resource.deallocate(zero_size, 0, 64);
    resource.deallocate(aligned, sizeof(OverAlignedValue), alignof(OverAlignedValue));
    resource.deallocate(byte, 1, 1);
}

TEST(NativeCoreFrameMemoryResource, RecordsExactBoundaryOverflowAndFrameStatistics) {
    FrameBacking backing{256};
    ml::FrameMemoryResource resource{backing};
    auto* const exact{resource.allocate(256, 1)};

    EXPECT_EQ(resource.try_allocate(1, 1), nullptr);
    auto const overflow{resource.get_stats()};
    EXPECT_EQ(overflow.overflow_count, 1);
    EXPECT_EQ(overflow.last_failure.requested_bytes, 1);
    EXPECT_EQ(overflow.last_failure.claimed_bytes, 256);

    resource.deallocate(exact, 256, 1);
    resource.reset();
    auto const reset{resource.get_stats()};
    EXPECT_EQ(reset.current_claimed_bytes, 0);
    EXPECT_EQ(reset.last_frame_claimed_bytes, 256);
    EXPECT_EQ(reset.last_frame_payload_bytes, 256);
    EXPECT_EQ(reset.last_frame_root_claim_count, 1);
}

TEST(NativeCoreFrameMemoryResource, ReclaimsPhasesWhilePreservingFrameTotals) {
    FrameBacking backing{512};
    ml::FrameMemoryResource resource{backing};

    auto* const first{resource.allocate(128, 16)};
    resource.deallocate(first, 128, 16);
    ASSERT_TRUE(resource.try_reclaim());

    auto* const second{resource.allocate(64, 16)};
    resource.deallocate(second, 64, 16);
    auto const current{resource.get_stats()};
    EXPECT_EQ(current.current_claimed_bytes, 64);
    EXPECT_EQ(current.current_frame_peak_claimed_bytes, 128);
    EXPECT_EQ(current.current_payload_bytes, 192);
    EXPECT_EQ(current.current_root_claim_count, 2);

    resource.reset();
    auto const reset{resource.get_stats()};
    EXPECT_EQ(reset.last_frame_claimed_bytes, 128);
    EXPECT_EQ(reset.last_frame_payload_bytes, 192);
    EXPECT_EQ(reset.last_frame_root_claim_count, 2);
}

TEST(NativeCoreFrameMemoryResource, MakesConcurrentNonOverlappingClaims) {
    constexpr std::int32_t thread_count{8};
    constexpr std::int32_t allocations_per_thread{64};
    constexpr std::size_t allocation_bytes{37};

    struct Allocation {
        std::byte* pointer{};
        std::size_t bytes{};
    };

    FrameBacking backing{thread_count * allocations_per_thread * 64};
    ml::FrameMemoryResource resource{backing};
    std::array<std::vector<Allocation>, thread_count> thread_allocations;
    std::vector<std::thread> threads;
    threads.reserve(thread_count);

    for (std::int32_t thread_index{}; thread_index < thread_count; ++thread_index) {
        threads.emplace_back([thread_index, &resource, &thread_allocations] {
            auto& allocations{thread_allocations[thread_index]};
            allocations.reserve(allocations_per_thread);
            for (std::int32_t allocation_index{}; allocation_index < allocations_per_thread;
                 ++allocation_index) {
                auto* const pointer{static_cast<std::byte*>(
                    resource.allocate(allocation_bytes, alignof(std::uint64_t)))};
                std::memset(pointer, thread_index + 1, allocation_bytes);
                allocations.push_back({pointer, allocation_bytes});
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }

    std::vector<Allocation> allocations;
    allocations.reserve(thread_count * allocations_per_thread);
    for (auto const& thread_values : thread_allocations) {
        allocations.insert(allocations.end(), thread_values.begin(), thread_values.end());
    }
    std::ranges::sort(allocations, {}, [](Allocation const& allocation) {
        return reinterpret_cast<std::uintptr_t>(allocation.pointer);
    });

    for (std::size_t index{}; index < allocations.size(); ++index) {
        EXPECT_TRUE(resource.owns(allocations[index].pointer));
        if (index > 0) {
            EXPECT_LE(allocations[index - 1].pointer + allocations[index - 1].bytes,
                      allocations[index].pointer);
        }
    }

    EXPECT_EQ(resource.get_stats().outstanding_allocation_count, allocations.size());
    for (Allocation const allocation : allocations) {
        resource.deallocate(allocation.pointer, allocation.bytes, alignof(std::uint64_t));
    }
    EXPECT_EQ(resource.get_stats().outstanding_allocation_count, 0);
}

TEST(NativeCoreFrameMemoryResource, TracksFrameArraysWithoutDefaultResourceFallback) {
    ASSERT_EQ(TrackedValue::live_count, 0);
    CountingResource fallback;
    auto* const previous_default{std::pmr::set_default_resource(&fallback)};
    {
        FrameBacking backing{4096};
        ml::FrameMemoryResource resource{backing};
        {
            ml::FrameArray<TrackedValue> values{&resource};
            values.reserve(4);
            values.emplace();
            values.emplace();
            EXPECT_EQ(TrackedValue::live_count, 2);
        }
        EXPECT_EQ(TrackedValue::live_count, 0);
        EXPECT_EQ(resource.get_stats().outstanding_allocation_count, 0);
        resource.reset();
    }
    std::pmr::set_default_resource(previous_default);

    EXPECT_EQ(fallback.allocation_count, 0);
}

TEST(NativeCoreFrameMemoryResource, ScratchScopesReclaimAndCleanUpDuringExceptions) {
    FrameBacking backing{4096};
    ml::FrameMemoryResource resource{backing};
    void* first{};
    {
        ml::FrameScratchScope scope{resource};
        first = scope.scratch().allocate(128, 16);
        scope.scratch().deallocate(first, 128, 16);
    }
    EXPECT_EQ(resource.get_stats().current_claimed_bytes, 0);

    {
        ml::FrameScratchScope scope{resource};
        auto* const reused{scope.scratch().allocate(64, 16)};
        EXPECT_EQ(reused, first);
        scope.scratch().deallocate(reused, 64, 16);
    }

    try {
        ml::FrameScratchScope scope{resource};
        ml::FrameArray<std::int32_t> values{&scope.scratch()};
        values.set_num(16);
        throw 7;
    } catch (std::int32_t const value) {
        EXPECT_EQ(value, 7);
    }

    auto const stats{resource.get_stats()};
    EXPECT_EQ(stats.current_claimed_bytes, 0);
    EXPECT_EQ(stats.outstanding_allocation_count, 0);
    EXPECT_GT(stats.current_frame_peak_claimed_bytes, 0);
}

TEST(NativeCoreFrameMemoryResource, RefusesReclaimWithOutstandingAllocationsAndValidatesBacking) {
    FrameBacking backing{4096};
    ml::FrameMemoryResource resource{backing};
    auto* const allocation{resource.allocate(64, 16)};

    EXPECT_FALSE(resource.try_reclaim());
    resource.deallocate(allocation, 64, 16);
    EXPECT_TRUE(resource.try_reclaim());

    EXPECT_THROW((ml::FrameMemoryResource{std::span<std::byte>{}}), std::invalid_argument);
    alignas(ml::FrameMemoryResource::backing_alignment) std::array<std::byte, 128> aligned{};
    EXPECT_THROW((ml::FrameMemoryResource{std::span<std::byte>{aligned}.subspan(1)}),
                 std::invalid_argument);
}
}
