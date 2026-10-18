#pragma once
#include <ioj/layout/analysis/row_range.hpp>
#include <ioj/layout/analyzer/enum_code_value.hpp>
#include <ioj/layout/type_facts.hpp>

#include <lispb/schema/indices.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ioj::layout {

struct EnumDomain {
    std::uint64_t live_value_count{};
    std::uint64_t reserved_value_count{};
    std::optional<EnumCodeValue> minimum_value{};
    std::optional<EnumCodeValue> maximum_value{};
    std::optional<bool> signed_domain{};
    std::optional<bool> declared_signedness{};
    std::optional<std::uint32_t> minimum_required_bits{};
    std::optional<std::uint32_t> declared_bit_width{};
    std::optional<std::uint32_t> effective_bit_width{};
    std::optional<bool> semantic_width_can_represent_domain{};
    std::optional<std::uint64_t> unused_semantic_codes{};
    RowRange codes{};
};

}
