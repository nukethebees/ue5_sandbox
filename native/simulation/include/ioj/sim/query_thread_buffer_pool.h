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
    [[nodiscard]] auto reserve(std::uint32_t count) -> QueryThreadBufferReserveResult;
    [[nodiscard]] auto try_acquire() -> std::optional<std::uint32_t>;
    [[nodiscard]] auto release(std::uint32_t index) -> bool;

    [[nodiscard]] auto get(std::uint32_t index) -> QueryThreadBuffers&;
  private:
    std::mutex mutex_;
    std::vector<QueryThreadBuffers> buffers_;
    std::vector<std::uint32_t> free_indices_;
    std::uint32_t active_count_{};
};
} // namespace ioj::sim
