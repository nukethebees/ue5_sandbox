#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace ioj::layout {

struct MemoryFacts {
    auto operator==(MemoryFacts const&) const -> bool = default;

    std::optional<std::uint64_t> cache_line_bytes{};
    std::optional<std::uint64_t> page_bytes{};
    std::optional<std::uint64_t> l1_data_cache_bytes{};
    std::optional<std::uint64_t> l2_cache_bytes{};
    std::optional<std::uint64_t> l3_cache_bytes{};
    std::string provenance{};
};

} // namespace ioj::layout
