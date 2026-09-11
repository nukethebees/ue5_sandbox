#pragma once

#include <cstddef>

namespace ml::memory {
struct Config {
    inline static constexpr std::size_t default_root_capacity_bytes{std::size_t{1} << 30};

    std::size_t root_capacity_bytes{default_root_capacity_bytes};
};
}
