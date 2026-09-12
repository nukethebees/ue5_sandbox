#include "sandbox/core/frame_memory_resource.h"

#include <algorithm>
#include <bit>
#include <exception>
#include <memory>
#include <new>
#include <stdexcept>

namespace ml {
namespace frame_memory_resource {
inline constexpr std::size_t backing_alignment{64};
}

FrameMemoryResource::FrameMemoryResource(std::size_t const capacity_bytes)
    : capacity_bytes_{capacity_bytes} {
    if (capacity_bytes_ == 0) {
        throw std::invalid_argument{"Frame memory capacity must be positive"};
    }

    backing_ = static_cast<std::byte*>(::operator new(
        capacity_bytes_, std::align_val_t{frame_memory_resource::backing_alignment}));
}

FrameMemoryResource::~FrameMemoryResource() {
    if (outstanding_allocation_count_.load(std::memory_order_relaxed) != 0) {
        std::terminate();
    }
    ::operator delete(backing_, std::align_val_t{frame_memory_resource::backing_alignment});
}

auto FrameMemoryResource::try_allocate(std::size_t const bytes,
                                       std::size_t const alignment) noexcept -> void* {
    if (!std::has_single_bit(alignment)) {
        record_overflow(bytes, alignment, claimed_bytes_.load(std::memory_order_relaxed));
        return nullptr;
    }

    auto const allocation_bytes{std::max<std::size_t>(bytes, 1)};
    auto current{claimed_bytes_.load(std::memory_order_relaxed)};
    for (;;) {
        auto* aligned_pointer{static_cast<void*>(backing_ + current)};
        auto remaining{capacity_bytes_ - current};
        if (std::align(alignment, allocation_bytes, aligned_pointer, remaining) == nullptr) {
            record_overflow(bytes, alignment, current);
            return nullptr;
        }

        auto* const aligned_bytes{static_cast<std::byte*>(aligned_pointer)};
        auto const aligned_offset{static_cast<std::size_t>(aligned_bytes - backing_)};
        auto const next{aligned_offset + allocation_bytes};
        if (claimed_bytes_.compare_exchange_weak(
                current, next, std::memory_order_relaxed, std::memory_order_relaxed)) {
            payload_bytes_.fetch_add(bytes, std::memory_order_relaxed);
            padding_bytes_.fetch_add(aligned_offset - current, std::memory_order_relaxed);
            root_claim_count_.fetch_add(1, std::memory_order_relaxed);
            outstanding_allocation_count_.fetch_add(1, std::memory_order_relaxed);
            auto frame_peak{frame_peak_claimed_bytes_.load(std::memory_order_relaxed)};
            while (frame_peak < next &&
                   !frame_peak_claimed_bytes_.compare_exchange_weak(
                       frame_peak, next, std::memory_order_relaxed, std::memory_order_relaxed)) {}
            update_peak(next);
            return aligned_pointer;
        }
    }
}

void FrameMemoryResource::reclaim() {
    if (outstanding_allocation_count_.load(std::memory_order_relaxed) != 0) {
        throw std::logic_error{"Frame memory cannot be reclaimed with outstanding allocations"};
    }

    claimed_bytes_.store(0, std::memory_order_relaxed);
}

void FrameMemoryResource::reset() {
    reclaim();

    last_frame_claimed_bytes_ = frame_peak_claimed_bytes_.load(std::memory_order_relaxed);
    last_frame_payload_bytes_ = payload_bytes_.load(std::memory_order_relaxed);
    last_frame_padding_bytes_ = padding_bytes_.load(std::memory_order_relaxed);
    last_frame_root_claim_count_ = root_claim_count_.load(std::memory_order_relaxed);

    payload_bytes_.store(0, std::memory_order_relaxed);
    padding_bytes_.store(0, std::memory_order_relaxed);
    root_claim_count_.store(0, std::memory_order_relaxed);
    frame_peak_claimed_bytes_.store(0, std::memory_order_relaxed);
}

auto FrameMemoryResource::get_stats() const noexcept -> FrameMemoryStats {
    return {
        .capacity_bytes = capacity_bytes_,
        .current_claimed_bytes = claimed_bytes_.load(std::memory_order_relaxed),
        .current_frame_peak_claimed_bytes =
            frame_peak_claimed_bytes_.load(std::memory_order_relaxed),
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

auto FrameMemoryResource::owns(void const* const pointer) const noexcept -> bool {
    auto const address{reinterpret_cast<std::uintptr_t>(pointer)};
    auto const begin{reinterpret_cast<std::uintptr_t>(backing_)};
    return address >= begin && address < begin + capacity_bytes_;
}

auto FrameMemoryResource::do_allocate(std::size_t const bytes, std::size_t const alignment)
    -> void* {
    if (auto* const allocation{try_allocate(bytes, alignment)}) {
        return allocation;
    }
    throw std::bad_alloc{};
}

void FrameMemoryResource::do_deallocate(void* const pointer, std::size_t const, std::size_t const) {
    if (!owns(pointer)) {
        std::terminate();
    }
    auto const previous{outstanding_allocation_count_.fetch_sub(1, std::memory_order_relaxed)};
    if (previous == 0) {
        std::terminate();
    }
}

auto FrameMemoryResource::do_is_equal(std::pmr::memory_resource const& other) const noexcept
    -> bool {
    return this == &other;
}

void FrameMemoryResource::update_peak(std::size_t const claimed_bytes) noexcept {
    auto peak{peak_claimed_bytes_.load(std::memory_order_relaxed)};
    while (peak < claimed_bytes &&
           !peak_claimed_bytes_.compare_exchange_weak(
               peak, claimed_bytes, std::memory_order_relaxed, std::memory_order_relaxed)) {}
}

void FrameMemoryResource::record_overflow(std::size_t const bytes,
                                          std::size_t const alignment,
                                          std::size_t const claimed_bytes) noexcept {
    overflow_count_.fetch_add(1, std::memory_order_relaxed);
    last_failure_requested_bytes_.store(bytes, std::memory_order_relaxed);
    last_failure_alignment_.store(alignment, std::memory_order_relaxed);
    last_failure_claimed_bytes_.store(claimed_bytes, std::memory_order_relaxed);
}
}
