#include <SandboxCore/frame_memory_resource.h>

#include <SandboxCore/log_categories.h>
#include <SandboxCore/mimalloc_storage_allocator.h>

#include <Misc/AssertionMacros.h>

#include <algorithm>
#include <bit>
#include <cstdint>
#include <memory>

namespace ml {
namespace frame_memory_resource {
inline constexpr uint32 backing_alignment{64};

auto validate_local_chunk_bytes(SIZE_T const bytes) -> SIZE_T {
    if (bytes == 0) {
        UE_LOG(LogSandboxCore, Fatal, TEXT("Local frame memory chunk size must be positive."));
    }
    return bytes;
}
}

FFrameMemoryResource::FFrameMemoryResource(SIZE_T const capacity_bytes)
    : capacity_bytes_{capacity_bytes} {
    if (capacity_bytes_ == 0) {
        UE_LOG(LogSandboxCore, Fatal, TEXT("Frame memory capacity must be positive."));
    }

    backing_ = soa_storage::MimallocStorageAllocator::allocate(
        capacity_bytes_, frame_memory_resource::backing_alignment);
}

FFrameMemoryResource::~FFrameMemoryResource() {
    checkf(outstanding_allocation_count_.load(std::memory_order_relaxed) == 0,
           TEXT("Frame memory resource destroyed with outstanding allocations."));
    soa_storage::MimallocStorageAllocator::free(backing_);
}

auto FFrameMemoryResource::try_allocate(SIZE_T const bytes, SIZE_T const alignment) noexcept
    -> void* {
    check(std::has_single_bit(alignment));

    auto const allocation_bytes{std::max<SIZE_T>(bytes, 1)};
    auto current{claimed_bytes_.load(std::memory_order_relaxed)};
    for (;;) {
        auto* aligned_pointer{static_cast<void*>(backing_ + current)};
        auto remaining{capacity_bytes_ - current};
        if (std::align(alignment, allocation_bytes, aligned_pointer, remaining) == nullptr) {
            record_overflow(bytes, alignment, current);
            return nullptr;
        }

        auto* const aligned_bytes{static_cast<std::byte*>(aligned_pointer)};
        auto const aligned_offset{static_cast<SIZE_T>(aligned_bytes - backing_)};
        auto const next{aligned_offset + allocation_bytes};
        if (claimed_bytes_.compare_exchange_weak(
                current, next, std::memory_order_relaxed, std::memory_order_relaxed)) {
            payload_bytes_.fetch_add(bytes, std::memory_order_relaxed);
            padding_bytes_.fetch_add(aligned_offset - current, std::memory_order_relaxed);
            root_claim_count_.fetch_add(1, std::memory_order_relaxed);
            outstanding_allocation_count_.fetch_add(1, std::memory_order_relaxed);
            update_peak(next);
            return aligned_pointer;
        }
    }
}

void FFrameMemoryResource::reset() {
    auto const outstanding{outstanding_allocation_count_.load(std::memory_order_relaxed)};
    if (outstanding != 0) {
        UE_LOG(LogSandboxCore,
               Fatal,
               TEXT("Frame memory cannot be reset with %llu outstanding allocations."),
               outstanding);
    }

    last_frame_claimed_bytes_ = claimed_bytes_.load(std::memory_order_relaxed);
    last_frame_payload_bytes_ = payload_bytes_.load(std::memory_order_relaxed);
    last_frame_padding_bytes_ = padding_bytes_.load(std::memory_order_relaxed);
    last_frame_root_claim_count_ = root_claim_count_.load(std::memory_order_relaxed);

    payload_bytes_.store(0, std::memory_order_relaxed);
    padding_bytes_.store(0, std::memory_order_relaxed);
    root_claim_count_.store(0, std::memory_order_relaxed);
    claimed_bytes_.store(0, std::memory_order_relaxed);
}

auto FFrameMemoryResource::get_stats() const noexcept -> FFrameMemoryStats {
    return {
        .capacity_bytes = capacity_bytes_,
        .current_claimed_bytes = claimed_bytes_.load(std::memory_order_relaxed),
        .current_payload_bytes = payload_bytes_.load(std::memory_order_relaxed),
        .current_padding_bytes = padding_bytes_.load(std::memory_order_relaxed),
        .current_root_claim_count = root_claim_count_.load(std::memory_order_relaxed),
        .last_frame_claimed_bytes = last_frame_claimed_bytes_,
        .last_frame_payload_bytes = last_frame_payload_bytes_,
        .last_frame_padding_bytes = last_frame_padding_bytes_,
        .last_frame_root_claim_count = last_frame_root_claim_count_,
        .peak_claimed_bytes = peak_claimed_bytes_.load(std::memory_order_relaxed),
        .outstanding_allocation_count =
            outstanding_allocation_count_.load(std::memory_order_relaxed),
        .overflow_count = overflow_count_.load(std::memory_order_relaxed),
        .last_failure =
            {
                .requested_bytes = last_failure_requested_bytes_.load(std::memory_order_relaxed),
                .alignment = last_failure_alignment_.load(std::memory_order_relaxed),
                .claimed_bytes = last_failure_claimed_bytes_.load(std::memory_order_relaxed),
            },
    };
}

auto FFrameMemoryResource::owns(void const* const pointer) const noexcept -> bool {
    auto const address{reinterpret_cast<uintptr_t>(pointer)};
    auto const begin{reinterpret_cast<uintptr_t>(backing_)};
    return address >= begin && address < begin + capacity_bytes_;
}

auto FFrameMemoryResource::do_allocate(SIZE_T const bytes, SIZE_T const alignment) -> void* {
    if (auto* const allocation{try_allocate(bytes, alignment)}) {
        return allocation;
    }

    UE_LOG(LogSandboxCore,
           Fatal,
           TEXT("Frame memory exhausted: requested %llu bytes aligned to %llu with %llu of %llu "
                "bytes already claimed."),
           static_cast<uint64>(bytes),
           static_cast<uint64>(alignment),
           static_cast<uint64>(claimed_bytes_.load(std::memory_order_relaxed)),
           static_cast<uint64>(capacity_bytes_));
    return nullptr;
}

void FFrameMemoryResource::do_deallocate(void* const pointer, SIZE_T const, SIZE_T const) {
    check(owns(pointer));
    auto const previous{outstanding_allocation_count_.fetch_sub(1, std::memory_order_relaxed)};
    check(previous > 0);
}

auto FFrameMemoryResource::do_is_equal(std::pmr::memory_resource const& other) const noexcept
    -> bool {
    return this == &other;
}

void FFrameMemoryResource::update_peak(SIZE_T const claimed_bytes) noexcept {
    auto peak{peak_claimed_bytes_.load(std::memory_order_relaxed)};
    while (peak < claimed_bytes &&
           !peak_claimed_bytes_.compare_exchange_weak(
               peak, claimed_bytes, std::memory_order_relaxed, std::memory_order_relaxed)) {}
}

void FFrameMemoryResource::record_overflow(SIZE_T const bytes,
                                           SIZE_T const alignment,
                                           SIZE_T const claimed_bytes) noexcept {
    overflow_count_.fetch_add(1, std::memory_order_relaxed);
    last_failure_requested_bytes_.store(bytes, std::memory_order_relaxed);
    last_failure_alignment_.store(alignment, std::memory_order_relaxed);
    last_failure_claimed_bytes_.store(claimed_bytes, std::memory_order_relaxed);
}

FLocalFrameMemoryResource::FTrackingUpstreamResource::FTrackingUpstreamResource(
    std::pmr::memory_resource* const upstream)
    : upstream_{upstream} {
    check(upstream_ != nullptr);
}

auto FLocalFrameMemoryResource::FTrackingUpstreamResource::do_allocate(SIZE_T const bytes,
                                                                       SIZE_T const alignment)
    -> void* {
    auto* const allocation{upstream_->allocate(bytes, alignment)};
    claimed_bytes += bytes;
    ++claim_count;
    return allocation;
}

void FLocalFrameMemoryResource::FTrackingUpstreamResource::do_deallocate(void* const pointer,
                                                                         SIZE_T const bytes,
                                                                         SIZE_T const alignment) {
    upstream_->deallocate(pointer, bytes, alignment);
}

auto FLocalFrameMemoryResource::FTrackingUpstreamResource::do_is_equal(
    std::pmr::memory_resource const& other) const noexcept -> bool {
    return this == &other;
}

FLocalFrameMemoryResource::FLocalFrameMemoryResource(std::pmr::memory_resource* const upstream,
                                                     SIZE_T const initial_chunk_bytes)
    : tracking_upstream_{upstream}
    , initial_buffer_bytes_{frame_memory_resource::validate_local_chunk_bytes(initial_chunk_bytes)}
    , initial_buffer_{tracking_upstream_.allocate(initial_buffer_bytes_, alignof(std::max_align_t))}
    , monotonic_{initial_buffer_, initial_buffer_bytes_, &tracking_upstream_} {}
FLocalFrameMemoryResource::~FLocalFrameMemoryResource() {
    monotonic_.release();
    tracking_upstream_.deallocate(
        initial_buffer_, initial_buffer_bytes_, alignof(std::max_align_t));
}

auto FLocalFrameMemoryResource::get_stats() const noexcept -> FLocalFrameMemoryStats {
    return {
        .requested_bytes = requested_bytes_,
        .allocation_count = allocation_count_,
        .upstream_claimed_bytes = tracking_upstream_.claimed_bytes,
        .upstream_claim_count = tracking_upstream_.claim_count,
    };
}

auto FLocalFrameMemoryResource::do_allocate(SIZE_T const bytes, SIZE_T const alignment) -> void* {
    auto* const allocation{monotonic_.allocate(bytes, alignment)};
    requested_bytes_ += bytes;
    ++allocation_count_;
    return allocation;
}

void FLocalFrameMemoryResource::do_deallocate(void* const pointer,
                                              SIZE_T const bytes,
                                              SIZE_T const alignment) {
    monotonic_.deallocate(pointer, bytes, alignment);
}

auto FLocalFrameMemoryResource::do_is_equal(std::pmr::memory_resource const& other) const noexcept
    -> bool {
    return this == &other;
}
}
