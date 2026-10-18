#pragma once

#include <cstdint>
#include <optional>

namespace ioj::layout {

struct RegionFootprint {
    std::optional<std::uint64_t> region_bytes{};
    std::optional<std::uint64_t> count{};
    std::optional<std::uint64_t> bytes{};
    std::optional<std::uint64_t> non_payload_bytes{};
    std::optional<std::uint64_t> read_count{};
    std::optional<std::uint64_t> read_bytes{};
    std::optional<std::uint64_t> write_count{};
    std::optional<std::uint64_t> write_bytes{};
    std::optional<std::uint64_t> straddling_elements{};
};

}
