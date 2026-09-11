#include "native/memory/root_arena.h"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstdint>
#include <new>
#include <optional>
#include <span>
#include <vector>

namespace ml::memory {
struct RootArena::Impl {
    struct ReusableRange {
        std::byte* data{};
        std::size_t size_bytes{};
    };

    class MemoryResource final : public std::pmr::memory_resource {
      public:
        explicit MemoryResource(Impl& owner) noexcept
            : owner_{owner} {}
      private:
        auto do_allocate(std::size_t bytes, std::size_t alignment) -> void* override;
        void do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) override;
        auto do_is_equal(std::pmr::memory_resource const& other) const noexcept -> bool override {
            return this == &other;
        }

        Impl& owner_;
    };

    Impl(std::byte* backing, std::size_t capacity_bytes);

    auto try_acquire_range(std::size_t size_bytes, std::size_t alignment) -> std::byte*;
    void release_range(std::byte* data, std::size_t size_bytes);

    std::span<std::byte> backing_{};
    MemoryResource resource_{*this};
    std::vector<ReusableRange> reusable_ranges_{};
    std::optional<AllocationFailure> last_allocation_failure_{};
    std::size_t claimed_bytes_{};
    std::size_t live_block_bytes_{};
    std::int32_t live_block_count_{};
    std::int32_t peak_live_block_count_{};
};

RootArena::Impl::Impl(std::byte* const backing, std::size_t const capacity_bytes)
    : backing_{backing, capacity_bytes} {
    assert(backing != nullptr);
    assert(!backing_.empty());
    reusable_ranges_.reserve(64);
}

auto RootArena::Impl::try_acquire_range(std::size_t const size_bytes, std::size_t const alignment)
    -> std::byte* {
    if (size_bytes == 0 || !std::has_single_bit(alignment)) {
        last_allocation_failure_ = {.requested_bytes = size_bytes,
                                    .alignment = alignment,
                                    .claimed_bytes = claimed_bytes_,
                                    .total_capacity_bytes = backing_.size_bytes()};
        return nullptr;
    }

    auto const reusable_count{reusable_ranges_.size()};
    for (std::size_t index{}; index < reusable_count; ++index) {
        auto const range{reusable_ranges_[index]};
        auto const begin{reinterpret_cast<std::uintptr_t>(range.data)};
        auto const remainder{begin & (alignment - 1)};
        auto const padding{remainder == 0 ? std::size_t{} : alignment - remainder};
        if (padding > range.size_bytes || size_bytes > range.size_bytes - padding) {
            continue;
        }

        reusable_ranges_[index] = reusable_ranges_.back();
        reusable_ranges_.pop_back();
        if (padding > 0) {
            reusable_ranges_.push_back({.data = range.data, .size_bytes = padding});
        }

        auto* const aligned_data{reinterpret_cast<std::byte*>(begin + padding)};
        auto const remaining{range.size_bytes - padding - size_bytes};
        if (remaining > 0) {
            reusable_ranges_.push_back(
                {.data = aligned_data + size_bytes, .size_bytes = remaining});
        }

        last_allocation_failure_.reset();
        return aligned_data;
    }

    auto const current_address{reinterpret_cast<std::uintptr_t>(backing_.data()) + claimed_bytes_};
    auto const remainder{current_address & (alignment - 1)};
    auto const padding{remainder == 0 ? std::size_t{} : alignment - remainder};
    auto const available{backing_.size_bytes() - claimed_bytes_};
    if (padding > available || size_bytes > available - padding) {
        last_allocation_failure_ = {.requested_bytes = size_bytes,
                                    .alignment = alignment,
                                    .claimed_bytes = claimed_bytes_,
                                    .total_capacity_bytes = backing_.size_bytes()};
        return nullptr;
    }

    claimed_bytes_ += padding + size_bytes;
    last_allocation_failure_.reset();
    return reinterpret_cast<std::byte*>(current_address + padding);
}

void RootArena::Impl::release_range(std::byte* const data, std::size_t const size_bytes) {
    reusable_ranges_.push_back({.data = data, .size_bytes = size_bytes});
}

auto RootArena::Impl::MemoryResource::do_allocate(std::size_t const bytes,
                                                  std::size_t const alignment) -> void* {
    auto* const data{owner_.try_acquire_range(bytes, alignment)};
    if (data == nullptr) {
        throw std::bad_alloc{};
    }
    return data;
}

void RootArena::Impl::MemoryResource::do_deallocate(void* const pointer,
                                                    std::size_t const bytes,
                                                    std::size_t const) {
    owner_.release_range(static_cast<std::byte*>(pointer), bytes);
}

RootArena::RootArena(std::byte* const backing, std::size_t const capacity_bytes)
    : impl_{std::make_unique<Impl>(backing, capacity_bytes)} {}

RootArena::~RootArena() {
    assert(impl_->live_block_count_ == 0);
}

auto RootArena::try_acquire_block(std::size_t const size_bytes, std::size_t const alignment)
    -> Block {
    auto* const data{impl_->try_acquire_range(size_bytes, alignment)};
    if (data == nullptr) {
        return {};
    }

    impl_->live_block_bytes_ += size_bytes;
    ++impl_->live_block_count_;
    impl_->peak_live_block_count_ =
        std::max(impl_->peak_live_block_count_, impl_->live_block_count_);
    return Block{*this, data, size_bytes, alignment};
}

auto RootArena::memory_resource() noexcept -> std::pmr::memory_resource& {
    return impl_->resource_;
}

auto RootArena::get_statistics() const noexcept -> Statistics {
    return {.total_capacity_bytes = impl_->backing_.size_bytes(),
            .claimed_bytes = impl_->claimed_bytes_,
            .live_block_bytes = impl_->live_block_bytes_,
            .live_block_count = impl_->live_block_count_,
            .peak_live_block_count = impl_->peak_live_block_count_,
            .reusable_range_count = static_cast<std::int32_t>(impl_->reusable_ranges_.size())};
}

auto RootArena::get_last_allocation_failure() const noexcept -> AllocationFailure const* {
    return impl_->last_allocation_failure_ ? &*impl_->last_allocation_failure_ : nullptr;
}

auto RootArena::backing_address() const noexcept -> std::byte* {
    return impl_->backing_.data();
}

void RootArena::release_block(std::byte* const data, std::size_t const size_bytes) {
    assert(impl_->live_block_count_ > 0);
    assert(impl_->live_block_bytes_ >= size_bytes);
    impl_->live_block_bytes_ -= size_bytes;
    --impl_->live_block_count_;
    impl_->release_range(data, size_bytes);
}
}
