#pragma once

#include <cstddef>

namespace ioj::memory {
struct AllocationFailure {
    std::size_t requested_bytes{};
    std::size_t alignment{};
    std::size_t claimed_bytes{};
    std::size_t total_capacity_bytes{};
};

}
