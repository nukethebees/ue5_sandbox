#pragma once

#include <cstddef>

namespace ioj::memory {

struct Statistics {
    std::size_t live_bytes{};
    std::size_t peak_live_bytes{};
    std::size_t live_allocation_count{};
    std::size_t peak_live_allocation_count{};
};
}
