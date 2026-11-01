#include "ioj/frame_memory_resource.h"

#include "ioj/diagnostics.h"

#include <algorithm>
#include <bit>
#include <exception>
#include <format>
#include <memory>
#include <new>
#include <stdexcept>

#if defined(IOJ_WITH_TRACY)
#include <sandbox/profiling/memory.h>
#endif

namespace ioj {
FrameMemoryResource::FrameMemoryResource(std::size_t const capacity_bytes,
                                         std::pmr::memory_resource& upstream)
    : upstream_{upstream}
    , backing_{nullptr, capacity_bytes, backing_alignment} {
    if (capacity_bytes == 0) {
        throw std::invalid_argument{"Frame memory capacity must be positive"};
    }
    backing_.data = static_cast<std::byte*>(upstream_.allocate(capacity_bytes, backing_alignment));
}

FrameMemoryResource::~FrameMemoryResource() {
    if (epoch_active_ || outstanding_allocation_count_.load(std::memory_order_relaxed) != 0) {
        std::terminate();
    }
    upstream_.deallocate(backing_.data, backing_.size_bytes, backing_.alignment);
}

auto FrameMemoryResource::try_allocate(std::size_t const bytes,
                                       std::size_t const alignment) noexcept -> void* {
    if (!std::has_single_bit(alignment)) {
        record_failure(bytes, alignment, claimed_bytes_.load(std::memory_order_relaxed));
        return nullptr;
    }

    auto const allocation_bytes{std::max<std::size_t>(bytes, 1)};
    auto current{claimed_bytes_.load(std::memory_order_relaxed)};
    for (;;) {
        auto* aligned_pointer{static_cast<void*>(backing_.data + current)};
        auto remaining{backing_.size_bytes - current};
        if (std::align(alignment, allocation_bytes, aligned_pointer, remaining) == nullptr) {
            overflow_count_.fetch_add(1, std::memory_order_relaxed);
            try {
                auto* const allocation{upstream_.allocate(bytes, alignment)};
                record_allocation(allocation, bytes);
                return allocation;
            } catch (std::bad_alloc const&) {
                record_failure(bytes, alignment, current);
                return nullptr;
            }
        }

        auto* const aligned_bytes{static_cast<std::byte*>(aligned_pointer)};
        auto const aligned_offset{static_cast<std::size_t>(aligned_bytes - backing_.data)};
        auto const next{aligned_offset + allocation_bytes};
        if (claimed_bytes_.compare_exchange_weak(
                current, next, std::memory_order_relaxed, std::memory_order_relaxed)) {
            padding_bytes_.fetch_add(aligned_offset - current, std::memory_order_relaxed);
            auto frame_peak{frame_peak_claimed_bytes_.load(std::memory_order_relaxed)};
            while (frame_peak < next &&
                   !frame_peak_claimed_bytes_.compare_exchange_weak(
                       frame_peak, next, std::memory_order_relaxed, std::memory_order_relaxed)) {}
            update_peak(next);
            record_allocation(aligned_pointer, bytes);
            return aligned_pointer;
        }
    }
}

auto FrameMemoryResource::try_reclaim() noexcept -> bool {
    if (outstanding_allocation_count_.load(std::memory_order_relaxed) != 0) {
        return false;
    }

    claimed_bytes_.store(0, std::memory_order_relaxed);
    return true;
}

void FrameMemoryResource::reset() {
    if (epoch_active_) {
        fatal_error("Frame memory cannot complete a tick while a scratch epoch is active");
    }
    if (!try_reclaim()) {
        fatal_error(
            std::format("Frame memory cannot complete a tick with {} outstanding allocations",
                        outstanding_allocation_count_.load(std::memory_order_relaxed)));
    }

    last_frame_claimed_bytes_ = frame_peak_claimed_bytes_.load(std::memory_order_relaxed);
    last_frame_payload_bytes_ = payload_bytes_.load(std::memory_order_relaxed);
    last_frame_padding_bytes_ = padding_bytes_.load(std::memory_order_relaxed);
    last_frame_root_claim_count_ = root_claim_count_.load(std::memory_order_relaxed);

    payload_bytes_.store(0, std::memory_order_relaxed);
    padding_bytes_.store(0, std::memory_order_relaxed);
    root_claim_count_.store(0, std::memory_order_relaxed);
    frame_peak_claimed_bytes_.store(0, std::memory_order_relaxed);
}

auto FrameMemoryResource::stats() const noexcept -> FrameMemoryStats {
    return {
        .capacity_bytes = backing_.size_bytes,
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
    auto const begin{reinterpret_cast<std::uintptr_t>(backing_.data)};
    return address >= begin && address < begin + backing_.size_bytes;
}

auto FrameMemoryResource::do_allocate(std::size_t const bytes, std::size_t const alignment)
    -> void* {
    if (auto* const allocation{try_allocate(bytes, alignment)}) {
        return allocation;
    }
    throw std::bad_alloc{};
}

void FrameMemoryResource::do_deallocate(void* const pointer,
                                        std::size_t const bytes,
                                        std::size_t const alignment) {
    if (!owns(pointer)) {
        upstream_.deallocate(pointer, bytes, alignment);
    }
    auto const previous{outstanding_allocation_count_.fetch_sub(1, std::memory_order_relaxed)};
    if (previous == 0) {
        std::terminate();
    }
#if defined(IOJ_WITH_TRACY)
    profiling::record_memory_free(profiling::MemoryDomain::FrameScratch, pointer);
#endif
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

void FrameMemoryResource::record_allocation(void* const pointer, std::size_t const bytes) noexcept {
    payload_bytes_.fetch_add(bytes, std::memory_order_relaxed);
    root_claim_count_.fetch_add(1, std::memory_order_relaxed);
    outstanding_allocation_count_.fetch_add(1, std::memory_order_relaxed);
#if defined(IOJ_WITH_TRACY)
    profiling::record_memory_allocation(profiling::MemoryDomain::FrameScratch, pointer, bytes);
#else
    static_cast<void>(pointer);
#endif
}

void FrameMemoryResource::record_failure(std::size_t const bytes,
                                         std::size_t const alignment,
                                         std::size_t const claimed_bytes) noexcept {
    last_failure_requested_bytes_.store(bytes, std::memory_order_relaxed);
    last_failure_alignment_.store(alignment, std::memory_order_relaxed);
    last_failure_claimed_bytes_.store(claimed_bytes, std::memory_order_relaxed);
}

void FrameMemoryResource::begin_epoch() {
    if (epoch_active_) {
        fatal_error("Frame scratch epochs cannot be nested");
    }
    if (outstanding_allocation_count_.load(std::memory_order_relaxed) != 0) {
        fatal_error("Frame scratch epoch started with outstanding allocations");
    }
    epoch_active_ = true;
}

void FrameMemoryResource::end_epoch() noexcept {
    if (!epoch_active_) {
        fatal_error("Frame scratch epoch ended without an active epoch");
    }
    if (!try_reclaim()) {
        auto const stats{this->stats()};
        fatal_error(std::format(
            "Frame scratch allocation escaped its epoch: {} allocations, {} claimed bytes",
            stats.outstanding_allocation_count,
            stats.current_claimed_bytes));
    }
    epoch_active_ = false;
}

FrameScratchScope::FrameScratchScope(FrameMemoryResource& resource)
    : resource_{resource} {
    resource_.begin_epoch();
}

FrameScratchScope::~FrameScratchScope() noexcept {
    resource_.end_epoch();
}
}
