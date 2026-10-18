#pragma once

#include <cstdint>
#include <optional>

namespace ioj::layout {

struct PackedAccessWaste {
    std::optional<std::uint64_t> storage_bits{};
    std::optional<std::uint64_t> non_useful_bits{};
    std::optional<std::uint64_t> unselected_field_bits{};
    std::optional<std::uint64_t> reserved_bits{};
    std::optional<std::uint64_t> unused_bits{};
};

}
