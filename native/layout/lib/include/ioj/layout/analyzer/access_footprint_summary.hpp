#pragma once

#include <cstdint>
#include <optional>

namespace ioj::layout {

struct AccessFootprintSummary {
    std::optional<std::uint64_t> useful_bytes{};
    std::optional<std::uint64_t> read_useful_bytes{};
    std::optional<std::uint64_t> write_useful_bytes{};
    std::optional<std::uint64_t> logical_read_useful_bytes{};
    std::optional<std::uint64_t> logical_write_useful_bytes{};
    std::optional<std::uint64_t> cache_lines{};
    std::optional<std::uint64_t> cache_bytes{};
    std::optional<std::uint64_t> read_cache_lines{};
    std::optional<std::uint64_t> read_cache_bytes{};
    std::optional<std::uint64_t> write_cache_lines{};
    std::optional<std::uint64_t> write_cache_bytes{};
    std::optional<std::uint64_t> pages{};
    std::optional<std::uint64_t> page_bytes{};
    std::optional<std::uint64_t> read_pages{};
    std::optional<std::uint64_t> read_page_bytes{};
    std::optional<std::uint64_t> write_pages{};
    std::optional<std::uint64_t> write_page_bytes{};
};

}
