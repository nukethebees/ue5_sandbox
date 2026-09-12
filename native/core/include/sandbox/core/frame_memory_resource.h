#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory_resource>

namespace ml {
struct FrameMemoryFailure {
    std::size_t requested_bytes{};
    std::size_t alignment{};
    std::size_t claimed_bytes{};
};

struct FrameMemoryStats {
    std::size_t capacity_bytes{};

    std::size_t current_claimed_bytes{};
    std::size_t current_frame_peak_claimed_bytes{};
    std::size_t current_payload_bytes{};
    std::size_t current_padding_bytes{};
    std::uint64_t current_root_claim_count{};

    std::size_t last_frame_claimed_bytes{};
    std::size_t last_frame_payload_bytes{};
    std::size_t last_frame_padding_bytes{};
    std::uint64_t last_frame_root_claim_count{};

    std::size_t peak_claimed_bytes{};
    std::uint64_t outstanding_allocation_count{};
    std::uint64_t overflow_count{};
    FrameMemoryFailure last_failure{};
};

class FrameMemoryResource final : public std::pmr::memory_resource {
  public:
    explicit FrameMemoryResource(std::size_t capacity_bytes);
    ~FrameMemoryResource() override;

    FrameMemoryResource(FrameMemoryResource const&) = delete;
    FrameMemoryResource(FrameMemoryResource&&) = delete;
    auto operator=(FrameMemoryResource const&) -> FrameMemoryResource& = delete;
    auto operator=(FrameMemoryResource&&) -> FrameMemoryResource& = delete;

    auto try_allocate(std::size_t bytes, std::size_t alignment) noexcept -> void*;

    void reclaim();
    void reset();
    auto get_stats() const noexcept -> FrameMemoryStats;
    auto owns(void const* pointer) const noexcept -> bool;
  private:
    auto do_allocate(std::size_t bytes, std::size_t alignment) -> void* override;
    void do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) override;
    auto do_is_equal(std::pmr::memory_resource const& other) const noexcept -> bool override;

    void update_peak(std::size_t claimed_bytes) noexcept;
    void record_overflow(std::size_t bytes,
                         std::size_t alignment,
                         std::size_t claimed_bytes) noexcept;

    std::byte* backing_{};
    std::size_t capacity_bytes_{};

    std::atomic<std::size_t> claimed_bytes_{};
    std::atomic<std::size_t> payload_bytes_{};
    std::atomic<std::size_t> padding_bytes_{};
    std::atomic<std::uint64_t> root_claim_count_{};
    std::atomic<std::uint64_t> outstanding_allocation_count_{};

    std::atomic<std::size_t> frame_peak_claimed_bytes_{};

    std::size_t last_frame_claimed_bytes_{};
    std::size_t last_frame_payload_bytes_{};
    std::size_t last_frame_padding_bytes_{};
    std::uint64_t last_frame_root_claim_count_{};

    std::atomic<std::size_t> peak_claimed_bytes_{};
    std::atomic<std::uint64_t> overflow_count_{};
    std::atomic<std::size_t> last_failure_requested_bytes_{};
    std::atomic<std::size_t> last_failure_alignment_{};
    std::atomic<std::size_t> last_failure_claimed_bytes_{};
};
}
