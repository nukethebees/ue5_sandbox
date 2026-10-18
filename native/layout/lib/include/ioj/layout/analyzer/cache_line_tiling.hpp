#pragma once

#include <cstdint>
#include <optional>

namespace ioj::layout {

struct CacheLineTiling {
    std::uint64_t cache_line_bytes{};
    std::uint64_t element_bytes{};
    std::optional<std::uint64_t> exact_elements_per_cache_line{};
    std::uint64_t complete_elements_from_line_start{};
    std::uint64_t boundary_fragment_bytes{};
    std::uint64_t minimum_cache_lines_per_element{};
};

}
