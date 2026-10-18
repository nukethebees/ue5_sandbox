#pragma once
#include <ioj/layout/analysis/row_range.hpp>
#include <ioj/layout/analyzer/exact_code_count.hpp>

#include <codegen/schema/packed_integer_value.h>
#include <codegen/schema/semantic_relation_kind.h>
#include <codegen/schema/semantic_relation_unit.h>
#include <lispb/schema/indices.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ioj::layout {

struct IntegerDomain {
    bool signedness{};
    lispb::codegen::PackedIntegerValue minimum_value{};
    lispb::codegen::PackedIntegerValue maximum_value{};
    std::optional<std::uint64_t> live_value_count{};
    std::uint64_t sentinel_code_count{};
    std::optional<std::uint64_t> required_code_count{};
    std::uint32_t minimum_required_bits{};
    std::optional<std::uint32_t> declared_bit_width{};
    std::uint32_t effective_bit_width{};
    std::optional<std::uint64_t> unused_codes{};
    RowRange codes{};
};

}
