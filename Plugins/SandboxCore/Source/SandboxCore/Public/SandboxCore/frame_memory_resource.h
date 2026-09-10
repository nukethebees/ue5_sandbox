#pragma once

#include <CoreTypes.h>

#include <atomic>
#include <cstddef>
#include <memory_resource>

namespace ml {
struct FFrameMemoryFailure {
    SIZE_T requested_bytes{};
    SIZE_T alignment{};
    SIZE_T claimed_bytes{};
};

struct FFrameMemoryStats {
    SIZE_T capacity_bytes{};

    SIZE_T current_claimed_bytes{};
    SIZE_T current_payload_bytes{};
    SIZE_T current_padding_bytes{};
    uint64 current_root_claim_count{};

    SIZE_T last_frame_claimed_bytes{};
    SIZE_T last_frame_payload_bytes{};
    SIZE_T last_frame_padding_bytes{};
    uint64 last_frame_root_claim_count{};

    SIZE_T peak_claimed_bytes{};
    uint64 outstanding_allocation_count{};
    uint64 overflow_count{};
    FFrameMemoryFailure last_failure{};
};

class SANDBOXCORE_API FFrameMemoryResource final : public std::pmr::memory_resource {
  public:
    explicit FFrameMemoryResource(SIZE_T capacity_bytes);
    ~FFrameMemoryResource() override;

    FFrameMemoryResource(FFrameMemoryResource const&) = delete;
    FFrameMemoryResource(FFrameMemoryResource&&) = delete;
    auto operator=(FFrameMemoryResource const&) -> FFrameMemoryResource& = delete;
    auto operator=(FFrameMemoryResource&&) -> FFrameMemoryResource& = delete;

    // This is the non-fatal form used for capacity probes and overflow tests. Normal PMR
    // allocation uses the same claim path but terminates on failure.
    auto try_allocate(SIZE_T bytes, SIZE_T alignment) noexcept -> void*;

    void reset();
    auto get_stats() const noexcept -> FFrameMemoryStats;
    auto owns(void const* pointer) const noexcept -> bool;
  private:
    auto do_allocate(SIZE_T bytes, SIZE_T alignment) -> void* override;
    void do_deallocate(void* pointer, SIZE_T bytes, SIZE_T alignment) override;
    auto do_is_equal(std::pmr::memory_resource const& other) const noexcept -> bool override;

    void update_peak(SIZE_T claimed_bytes) noexcept;
    void record_overflow(SIZE_T bytes, SIZE_T alignment, SIZE_T claimed_bytes) noexcept;

    std::byte* backing_{};
    SIZE_T capacity_bytes_{};

    std::atomic<SIZE_T> claimed_bytes_{};
    std::atomic<SIZE_T> payload_bytes_{};
    std::atomic<SIZE_T> padding_bytes_{};
    std::atomic<uint64> root_claim_count_{};
    std::atomic<uint64> outstanding_allocation_count_{};

    SIZE_T last_frame_claimed_bytes_{};
    SIZE_T last_frame_payload_bytes_{};
    SIZE_T last_frame_padding_bytes_{};
    uint64 last_frame_root_claim_count_{};

    std::atomic<SIZE_T> peak_claimed_bytes_{};
    std::atomic<uint64> overflow_count_{};
    std::atomic<SIZE_T> last_failure_requested_bytes_{};
    std::atomic<SIZE_T> last_failure_alignment_{};
    std::atomic<SIZE_T> last_failure_claimed_bytes_{};
};

struct FLocalFrameMemoryStats {
    SIZE_T requested_bytes{};
    uint64 allocation_count{};
    SIZE_T upstream_claimed_bytes{};
    uint64 upstream_claim_count{};
};

// Unsynchronised task/invocation-local bump allocator. Its upstream may be the concurrent frame
// resource or another local resource.
class SANDBOXCORE_API FLocalFrameMemoryResource final : public std::pmr::memory_resource {
  public:
    FLocalFrameMemoryResource(std::pmr::memory_resource* upstream, SIZE_T initial_chunk_bytes);
    ~FLocalFrameMemoryResource() override;

    FLocalFrameMemoryResource(FLocalFrameMemoryResource const&) = delete;
    FLocalFrameMemoryResource(FLocalFrameMemoryResource&&) = delete;
    auto operator=(FLocalFrameMemoryResource const&) -> FLocalFrameMemoryResource& = delete;
    auto operator=(FLocalFrameMemoryResource&&) -> FLocalFrameMemoryResource& = delete;

    auto get_stats() const noexcept -> FLocalFrameMemoryStats;
  private:
    class FTrackingUpstreamResource final : public std::pmr::memory_resource {
      public:
        explicit FTrackingUpstreamResource(std::pmr::memory_resource* upstream);

        SIZE_T claimed_bytes{};
        uint64 claim_count{};
      private:
        auto do_allocate(SIZE_T bytes, SIZE_T alignment) -> void* override;
        void do_deallocate(void* pointer, SIZE_T bytes, SIZE_T alignment) override;
        auto do_is_equal(std::pmr::memory_resource const& other) const noexcept -> bool override;

        std::pmr::memory_resource* upstream_{};
    };

    auto do_allocate(SIZE_T bytes, SIZE_T alignment) -> void* override;
    void do_deallocate(void* pointer, SIZE_T bytes, SIZE_T alignment) override;
    auto do_is_equal(std::pmr::memory_resource const& other) const noexcept -> bool override;

    FTrackingUpstreamResource tracking_upstream_;
    SIZE_T initial_buffer_bytes_{};
    void* initial_buffer_{};
    std::pmr::monotonic_buffer_resource monotonic_;
    SIZE_T requested_bytes_{};
    uint64 allocation_count_{};
};
}
