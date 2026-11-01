#pragma once

#include <cstddef>

namespace ioj::memory {
struct Allocation {
    std::byte* data{};
    std::size_t size_bytes{};
    std::size_t alignment{};
};
}
