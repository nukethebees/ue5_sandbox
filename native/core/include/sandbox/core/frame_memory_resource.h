#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <span>

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
    inline static constexpr std::size_t backing_alignment{64};

    explicit FrameMemoryResource(std::span<std::byte> backing);
    ~FrameMemoryResource() override;

    FrameMemoryResource(FrameMemoryResource const&) = delete;
    FrameMemoryResource(FrameMemoryResource&&) = delete;
    auto operator=(FrameMemoryResource const&) -> FrameMemoryResource& = delete;
    auto operator=(FrameMemoryResource&&) -> FrameMemoryResource& = delete;

    auto try_allocate(std::size_t bytes, std::size_t alignment) noexcept -> void*;

    auto try_reclaim() noexcept -> bool;
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
    void begin_epoch();
    void end_epoch() noexcept;

    friend class FrameScratchScope;

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
    bool epoch_active_{};
};

class FrameScratch final : public std::pmr::memory_resource {
  public:
    FrameScratch(FrameScratch const&) = delete;
    FrameScratch(FrameScratch&&) = delete;
    auto operator=(FrameScratch const&) -> FrameScratch& = delete;
    auto operator=(FrameScratch&&) -> FrameScratch& = delete;
  private:
    friend class FrameScratchScope;

    explicit FrameScratch(FrameMemoryResource& resource) noexcept
        : resource_{resource} {}

    auto do_allocate(std::size_t bytes, std::size_t alignment) -> void* override;
    void do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) override;
    auto do_is_equal(std::pmr::memory_resource const& other) const noexcept -> bool override;

    FrameMemoryResource& resource_;
};

class FrameScratchScope final {
  public:
    explicit FrameScratchScope(FrameMemoryResource& resource);
    ~FrameScratchScope() noexcept;

    FrameScratchScope(FrameScratchScope const&) = delete;
    FrameScratchScope(FrameScratchScope&&) = delete;
    auto operator=(FrameScratchScope const&) -> FrameScratchScope& = delete;
    auto operator=(FrameScratchScope&&) -> FrameScratchScope& = delete;

    auto scratch() noexcept -> FrameScratch& { return scratch_; }
  private:
    FrameMemoryResource& resource_;
    FrameScratch scratch_;
};
}
