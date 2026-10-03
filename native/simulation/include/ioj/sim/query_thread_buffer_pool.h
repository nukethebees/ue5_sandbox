#pragma once

#include "ioj/sim/query_thread_buffers.h"

#include <cstdint>
#include <mutex>
#include <optional>
#include <vector>

namespace ioj::sim {
enum class QueryThreadBufferReserveResult {
    reserved,
    unchanged,
    invalid_count,
    active_queries,
};

class QueryThreadBufferPool {
  public:
    explicit QueryThreadBufferPool(
        std::pmr::memory_resource* resource = std::pmr::get_default_resource())
        : QueryThreadBufferPool{resource, resource} {}
    // Use a concurrent resource for buffer contents; reserve bookkeeping before workers start.
    QueryThreadBufferPool(std::pmr::memory_resource* storage_resource,
                          std::pmr::memory_resource* buffer_resource)
        : buffers_{storage_resource}
        , free_indices_{storage_resource}
        , persistent_buffer_resource_{buffer_resource}
        , buffer_resource_{buffer_resource} {}

    [[nodiscard]] auto reserve(std::uint32_t count) -> QueryThreadBufferReserveResult;
    // Discard retained contents only after every lease has returned.
    [[nodiscard]] auto set_buffer_resource(std::pmr::memory_resource* resource) -> bool;
    [[nodiscard]] auto get_memory_resource() const noexcept -> std::pmr::memory_resource* {
        return buffers_.get_allocator().resource();
    }
    [[nodiscard]] auto get_buffer_memory_resource() const noexcept -> std::pmr::memory_resource* {
        return persistent_buffer_resource_;
    }
    [[nodiscard]] auto try_acquire() -> std::optional<std::uint32_t>;
    [[nodiscard]] auto release(std::uint32_t index) -> bool;

    [[nodiscard]] auto get(std::uint32_t index) -> QueryThreadBuffers&;
  private:
    std::mutex mutex_;
    std::pmr::vector<QueryThreadBuffers> buffers_;
    std::pmr::vector<std::uint32_t> free_indices_;
    std::pmr::memory_resource* persistent_buffer_resource_;
    std::pmr::memory_resource* buffer_resource_;
    std::uint32_t active_count_{};
};
} // namespace ioj::sim
