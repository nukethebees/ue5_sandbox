#include "ioj/sim/query_thread_buffer_pool.h"

#include <algorithm>
#include <cassert>
#include <cstddef>

namespace ioj::sim {
auto QueryThreadBufferPool::reserve(std::uint32_t const count) -> QueryThreadBufferReserveResult {
    if (count <= 0) {
        return QueryThreadBufferReserveResult::invalid_count;
    }

    std::scoped_lock const lock{mutex_};
    auto const required_count{static_cast<std::size_t>(count)};
    if (required_count <= buffers_.size()) {
        return QueryThreadBufferReserveResult::unchanged;
    }
    if (active_count_ != 0) {
        return QueryThreadBufferReserveResult::active_queries;
    }

    auto const previous_count{static_cast<std::uint32_t>(buffers_.size())};
    buffers_.reserve(required_count);
    free_indices_.reserve(required_count);
    for (auto i{previous_count}; i < count; ++i) {
        buffers_.emplace_back(buffer_resource_);
        free_indices_.push_back(i);
    }

    return QueryThreadBufferReserveResult::reserved;
}

auto QueryThreadBufferPool::set_buffer_resource(std::pmr::memory_resource* const resource) -> bool {
    assert(resource != nullptr);
    std::scoped_lock const lock{mutex_};
    if (active_count_ != 0) {
        return false;
    }

    // Destroy retained storage before changing allocators; PMR move assignment retains its
    // allocator.
    auto const count{buffers_.size()};
    buffers_.clear();
    buffer_resource_ = resource;
    for (std::size_t i{}; i < count; ++i) {
        buffers_.emplace_back(resource);
    }
    return true;
}

auto QueryThreadBufferPool::try_acquire() -> std::optional<std::uint32_t> {
    std::scoped_lock const lock{mutex_};
    if (free_indices_.empty()) {
        return std::nullopt;
    }

    auto const index{free_indices_.back()};
    free_indices_.pop_back();
    ++active_count_;
    return index;
}

auto QueryThreadBufferPool::release(std::uint32_t const index) -> bool {
    std::scoped_lock const lock{mutex_};
    auto const valid_index{static_cast<std::size_t>(index) < buffers_.size()};
    auto const already_free{std::ranges::find(free_indices_, index) != free_indices_.end()};
    if (!valid_index || already_free || active_count_ <= 0) {
        return false;
    }

    free_indices_.push_back(index);
    --active_count_;
    return true;
}

auto QueryThreadBufferPool::get(std::uint32_t const index) -> QueryThreadBuffers& {
    assert(static_cast<std::size_t>(index) < buffers_.size());
    return buffers_[static_cast<std::size_t>(index)];
}
} // namespace ioj::sim
