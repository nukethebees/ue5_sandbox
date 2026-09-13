#include "sandbox/simulation/query_thread_buffer_pool.h"

#include <algorithm>
#include <cassert>
#include <cstddef>

namespace ml::simulation {
auto QueryThreadBufferPool::reserve(std::int32_t const count) -> QueryThreadBufferReserveResult {
    if (count <= 0) {
        return QueryThreadBufferReserveResult::invalid_count;
    }

    std::lock_guard const lock{mutex_};
    auto const required_count{static_cast<std::size_t>(count)};
    if (required_count <= buffers_.size()) {
        return QueryThreadBufferReserveResult::unchanged;
    }
    if (active_count_ != 0) {
        return QueryThreadBufferReserveResult::active_queries;
    }

    auto const previous_count{static_cast<std::int32_t>(buffers_.size())};
    buffers_.resize(required_count);
    free_indices_.reserve(required_count);
    for (auto i{previous_count}; i < count; ++i) {
        free_indices_.push_back(i);
    }

    return QueryThreadBufferReserveResult::reserved;
}

auto QueryThreadBufferPool::try_acquire() -> std::optional<std::int32_t> {
    std::lock_guard const lock{mutex_};
    if (free_indices_.empty()) {
        return std::nullopt;
    }

    auto const index{free_indices_.back()};
    free_indices_.pop_back();
    ++active_count_;
    return index;
}

auto QueryThreadBufferPool::release(std::int32_t const index) -> bool {
    std::lock_guard const lock{mutex_};
    auto const valid_index{index >= 0 && static_cast<std::size_t>(index) < buffers_.size()};
    auto const already_free{std::ranges::find(free_indices_, index) != free_indices_.end()};
    if (!valid_index || already_free || active_count_ <= 0) {
        return false;
    }

    free_indices_.push_back(index);
    --active_count_;
    return true;
}

auto QueryThreadBufferPool::get(std::int32_t const index) -> QueryThreadBuffers& {
    assert(index >= 0 && static_cast<std::size_t>(index) < buffers_.size());
    return buffers_[static_cast<std::size_t>(index)];
}
} // namespace ml::simulation
