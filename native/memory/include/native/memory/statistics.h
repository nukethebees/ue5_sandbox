#pragma once

#include <cstddef>
#include <cstdint>

namespace ml::memory {
struct AllocationFailure {
    std::size_t requested_bytes{};
    std::size_t alignment{};
    std::size_t claimed_bytes{};
    std::size_t total_capacity_bytes{};
};

struct Statistics {
    std::size_t total_capacity_bytes{};
    std::size_t claimed_bytes{};
    std::size_t live_block_bytes{};
    std::int32_t live_block_count{};
    std::int32_t peak_live_block_count{};
    std::int32_t reusable_range_count{};
};
}
