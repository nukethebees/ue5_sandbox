#include <SandboxCore/frame_array.h>
#include <SandboxCore/frame_memory_resource.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory_resource>
#include <new>
#include <span>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
struct alignas(256) FOverAlignedValue {
    uint64 value{};
};

struct FTrackedValue {
    FTrackedValue() { ++live_count; }
    FTrackedValue(FTrackedValue const&) { ++live_count; }
    FTrackedValue(FTrackedValue&&) noexcept { ++live_count; }
    ~FTrackedValue() { --live_count; }

    inline static int32 live_count{};
};

class FFrameBacking {
  public:
    explicit FFrameBacking(size_t const capacity)
        : data_{static_cast<std::byte*>(::operator new(capacity, std::align_val_t{ml::FFrameMemoryResource::backing_alignment}))}
        , capacity_{capacity} {}
    ~FFrameBacking() { ::operator delete(data_, std::align_val_t{ml::FFrameMemoryResource::backing_alignment}); }

    FFrameBacking(FFrameBacking const&) = delete;
    FFrameBacking(FFrameBacking&&) = delete;

    operator std::span<std::byte>() const noexcept { return {data_, capacity_}; }
  private:
    std::byte* data_{};
    size_t capacity_{};
};

class FCountingResource final : public std::pmr::memory_resource {
  public:
    uint64 allocation_count{};
  private:
    auto do_allocate(size_t const bytes, size_t const alignment) -> void* override {
        ++allocation_count;
        return std::pmr::new_delete_resource()->allocate(bytes, alignment);
    }

    void do_deallocate(void* const pointer, size_t const bytes, size_t const alignment) override {
        std::pmr::new_delete_resource()->deallocate(pointer, bytes, alignment);
    }

    auto do_is_equal(std::pmr::memory_resource const& other) const noexcept -> bool override { return this == &other; }
};
}

TEST_CASE("SandboxCore.FFrameMemoryResource aligns arbitrary PMR allocations") {
    FFrameBacking backing{4096};
    ml::FFrameMemoryResource resource{backing};

    auto* const byte{resource.allocate(1, 1)};
    auto* const aligned{resource.allocate(sizeof(FOverAlignedValue), alignof(FOverAlignedValue))};
    auto* const zero_size{resource.allocate(0, 64)};

    CHECK(resource.owns(byte));
    CHECK(resource.owns(aligned));
    CHECK(resource.owns(zero_size));
    CHECK(reinterpret_cast<uintptr_t>(aligned) % alignof(FOverAlignedValue) == 0);
    CHECK(reinterpret_cast<uintptr_t>(zero_size) % 64 == 0);
    CHECK(resource.get_stats().current_padding_bytes > 0);

    resource.deallocate(zero_size, 0, 64);
    resource.deallocate(aligned, sizeof(FOverAlignedValue), alignof(FOverAlignedValue));
    resource.deallocate(byte, 1, 1);
}

TEST_CASE("SandboxCore.FFrameMemoryResource supports exact-boundary allocation and overflow") {
    FFrameBacking backing{256};
    ml::FFrameMemoryResource resource{backing};
    auto* const exact{resource.allocate(256, 1)};

    CHECK(resource.get_stats().current_claimed_bytes == 256);
    CHECK(resource.try_allocate(1, 1) == nullptr);

    auto const stats{resource.get_stats()};
    CHECK(stats.overflow_count == 1);
    CHECK(stats.last_failure.requested_bytes == 1);
    CHECK(stats.last_failure.alignment == 1);
    CHECK(stats.last_failure.claimed_bytes == 256);

    resource.deallocate(exact, 256, 1);
}

TEST_CASE("SandboxCore.FFrameMemoryResource reclaims only on explicit reset") {
    FFrameBacking backing{512};
    ml::FFrameMemoryResource resource{backing};
    auto* const first{resource.allocate(128, 16)};
    resource.deallocate(first, 128, 16);

    CHECK(resource.get_stats().current_claimed_bytes == 128);

    resource.reset();
    auto const reset_stats{resource.get_stats()};
    CHECK(reset_stats.current_claimed_bytes == 0);
    CHECK(reset_stats.last_frame_claimed_bytes == 128);
    CHECK(reset_stats.last_frame_payload_bytes == 128);
    CHECK(reset_stats.last_frame_root_claim_count == 1);
    CHECK(reset_stats.peak_claimed_bytes == 128);

    auto* const reused{resource.allocate(128, 16)};
    CHECK(reused == first);
    resource.deallocate(reused, 128, 16);
}

TEST_CASE("SandboxCore.FFrameMemoryResource reclaims phases while preserving frame totals") {
    FFrameBacking backing{512};
    ml::FFrameMemoryResource resource{backing};

    auto* first{resource.allocate(128, 16)};
    resource.deallocate(first, 128, 16);
    CHECK(resource.try_reclaim());

    auto* second{resource.allocate(64, 16)};
    resource.deallocate(second, 64, 16);
    auto const current{resource.get_stats()};
    CHECK(current.current_claimed_bytes == 64);
    CHECK(current.current_frame_peak_claimed_bytes == 128);
    CHECK(current.current_payload_bytes == 192);
    CHECK(current.current_root_claim_count == 2);

    resource.reset();
    auto const reset{resource.get_stats()};
    CHECK(reset.last_frame_claimed_bytes == 128);
    CHECK(reset.last_frame_payload_bytes == 192);
    CHECK(reset.last_frame_root_claim_count == 2);
    CHECK(reset.current_claimed_bytes == 0);
    CHECK(reset.current_frame_peak_claimed_bytes == 0);
}

TEST_CASE("SandboxCore.FFrameMemoryResource makes concurrent non-overlapping claims") {
    constexpr int32 thread_count{16};
    constexpr int32 allocations_per_thread{128};
    constexpr SIZE_T allocation_bytes{37};
    constexpr SIZE_T capacity{thread_count * allocations_per_thread * 64};

    struct FAllocation {
        std::byte* pointer{};
        SIZE_T bytes{};
    };

    FFrameBacking backing{capacity};
    ml::FFrameMemoryResource resource{backing};
    std::array<std::vector<FAllocation>, thread_count> thread_allocations;
    std::vector<std::thread> threads;
    threads.reserve(thread_count);

    for (int32 thread_index{}; thread_index < thread_count; ++thread_index) {
        threads.emplace_back([thread_index, &resource, &thread_allocations] {
            auto& allocations{thread_allocations[thread_index]};
            allocations.reserve(allocations_per_thread);
            for (int32 allocation_index{}; allocation_index < allocations_per_thread; ++allocation_index) {
                auto* const pointer{static_cast<std::byte*>(resource.allocate(allocation_bytes, alignof(uint64)))};
                std::memset(pointer, thread_index + 1, allocation_bytes);
                allocations.push_back({pointer, allocation_bytes});
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }

    std::vector<FAllocation> allocations;
    allocations.reserve(thread_count * allocations_per_thread);
    for (auto const& thread_values : thread_allocations) {
        allocations.insert(allocations.end(), thread_values.begin(), thread_values.end());
    }
    std::ranges::sort(allocations, {}, &FAllocation::pointer);

    auto const allocation_count{static_cast<int32>(allocations.size())};
    for (int32 i{}; i < allocation_count; ++i) {
        CHECK(resource.owns(allocations[i].pointer));
        if (i > 0) {
            CHECK(allocations[i - 1].pointer + allocations[i - 1].bytes <= allocations[i].pointer);
        }
    }

    auto const stats{resource.get_stats()};
    CHECK(stats.current_root_claim_count == thread_count * allocations_per_thread);
    CHECK(stats.current_payload_bytes == thread_count * allocations_per_thread * allocation_bytes);
    CHECK(stats.overflow_count == 0);

    for (auto const allocation : allocations) {
        resource.deallocate(allocation.pointer, allocation.bytes, alignof(uint64));
    }
    CHECK(resource.get_stats().outstanding_allocation_count == 0);
}

TEST_CASE("SandboxCore frame-backed arrays destroy non-trivial elements before reset") {
    CHECK(FTrackedValue::live_count == 0);
    FFrameBacking backing{4096};
    ml::FFrameMemoryResource root{backing};
    {
        ml::TFrameArray<FTrackedValue> values{&root};
        values.reserve(4);
        for (int32 i{}; i < 4; ++i) {
            values.emplace();
        }
        CHECK(FTrackedValue::live_count == 4);
    }
    CHECK(FTrackedValue::live_count == 0);
    CHECK(root.get_stats().outstanding_allocation_count == 0);
    root.reset();
}

TEST_CASE("SandboxCore explicit frame resource never consults the default PMR resource") {
    FCountingResource fallback;
    auto* const previous_default{std::pmr::set_default_resource(&fallback)};
    {
        FFrameBacking backing{4096};
        ml::FFrameMemoryResource root{backing};
        {
            ml::TFrameArray<int32> values{&root};
            values.reserve(64);
            for (int32 i{}; i < 64; ++i) {
                values.add(i);
            }
        }
        root.reset();
    }
    std::pmr::set_default_resource(previous_default);

    CHECK(fallback.allocation_count == 0);
}

TEST_CASE("SandboxCore frame scratch scopes reclaim epochs and preserve tick totals") {
    FFrameBacking backing{4096};
    ml::FFrameMemoryResource resource{backing};

    void* first{};
    {
        ml::FrameScratchScope scope{resource};
        first = scope.scratch().allocate(128, 16);
        scope.scratch().deallocate(first, 128, 16);
    }
    CHECK(resource.get_stats().current_claimed_bytes == 0);

    {
        ml::FrameScratchScope scope{resource};
        auto* const second{scope.scratch().allocate(64, 16)};
        CHECK(second == first);
        scope.scratch().deallocate(second, 64, 16);
    }

    resource.reset();
    auto const stats{resource.get_stats()};
    CHECK(stats.last_frame_claimed_bytes == 128);
    CHECK(stats.last_frame_payload_bytes == 192);
    CHECK(stats.last_frame_root_claim_count == 2);
}

TEST_CASE("SandboxCore frame memory refuses reclaim with an outstanding allocation") {
    FFrameBacking backing{4096};
    ml::FFrameMemoryResource resource{backing};
    auto* const allocation{resource.allocate(64, 16)};

    CHECK_FALSE(resource.try_reclaim());
    CHECK(resource.get_stats().current_claimed_bytes == 64);

    resource.deallocate(allocation, 64, 16);
    CHECK(resource.try_reclaim());
}

TEST_CASE("SandboxCore frame memory validates externally supplied backing") {
    CHECK_THROWS_AS(ml::FFrameMemoryResource{std::span<std::byte>{}}, std::invalid_argument);

    alignas(ml::FFrameMemoryResource::backing_alignment) std::array<std::byte, 128> backing;
    CHECK_THROWS_AS(ml::FFrameMemoryResource{std::span<std::byte>{backing}.subspan(1)}, std::invalid_argument);
}

TEST_CASE("SandboxCore frame scratch scope reclaims during exception unwinding") {
    FFrameBacking backing{4096};
    ml::FFrameMemoryResource resource{backing};

    try {
        ml::FrameScratchScope scope{resource};
        ml::TFrameArray<int32> values{&scope.scratch()};
        values.set_num(16);
        throw 7;
    } catch (int32 const value) {
        CHECK(value == 7);
    }

    auto const stats{resource.get_stats()};
    CHECK(stats.current_claimed_bytes == 0);
    CHECK(stats.outstanding_allocation_count == 0);
    CHECK(stats.current_frame_peak_claimed_bytes > 0);
}
