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
    SIZE_T current_frame_peak_claimed_bytes{};
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

    // Rewinds storage after a synchronous phase while retaining this frame's aggregate stats.
    void reclaim();
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

    std::atomic<SIZE_T> frame_peak_claimed_bytes_{};

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

}
