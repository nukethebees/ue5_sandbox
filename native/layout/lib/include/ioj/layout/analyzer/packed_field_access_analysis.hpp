#pragma once

#include <ioj/layout/analyzer/access_operation.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace ioj::layout {

struct PackedFieldAccessAnalysis {
    std::string name{};
    AccessOperation operation{AccessOperation::read};
    std::uint32_t bit_width{};
    std::optional<std::uint64_t> useful_bits{};
    std::optional<std::uint64_t> read_useful_bits{};
    std::optional<std::uint64_t> write_useful_bits{};
    std::optional<std::uint64_t> logical_read_useful_bits{};
    std::optional<std::uint64_t> logical_write_useful_bits{};
};

}
