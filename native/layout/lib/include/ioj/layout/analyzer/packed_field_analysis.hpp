#pragma once

#include <ioj/layout/analyzer/exact_code_count.hpp>
#include <ioj/layout/analyzer/fixed_point_analysis.hpp>
#include <ioj/layout/analyzer/linear_quantized_analysis.hpp>
#include <ioj/layout/analyzer/mini_float_analysis.hpp>
#include <ioj/layout/analyzer/packed_named_code_analysis.hpp>

#include <codegen/schema/packed_field_kind.h>
#include <codegen/schema/packed_integer_value.h>
#include <codegen/schema/semantic_relation_kind.h>
#include <codegen/schema/semantic_relation_unit.h>
#include <lispb/schema/indices.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ioj::layout {

struct PackedFieldAnalysis {
    std::string name{};
    std::optional<lispb::schema::TypeId> semantic_type{};
    std::string logical_type{};
    lispb::codegen::PackedFieldKind kind{lispb::codegen::PackedFieldKind::unsigned_integer};
    bool reserved{};
    std::uint32_t schema_bit_width{};
    bool schema_bit_width_auto{};
    std::uint32_t bit_width{};
    bool overridden{};
    std::uint64_t least_significant_bit{};
    std::optional<std::uint64_t> most_significant_bit{};
    std::optional<std::uint64_t> maximum_unsigned_value{};
    std::optional<std::int64_t> minimum_signed_value{};
    std::optional<std::int64_t> maximum_signed_value{};
    std::optional<lispb::codegen::PackedIntegerValue> minimum_semantic_value{};
    std::optional<lispb::codegen::PackedIntegerValue> maximum_semantic_value{};
    std::optional<std::uint64_t> semantic_value_count{};
    std::uint64_t sentinel_code_count{};
    std::optional<std::uint64_t> required_code_count{};
    std::optional<std::uint32_t> minimum_required_bits{};
    std::optional<std::uint64_t> unused_codes{};
    std::vector<PackedNamedCodeAnalysis> named_codes{};
    std::optional<LinearQuantizedAnalysis> linear_quantized{};
    std::optional<FixedPointAnalysis> fixed_point{};
    std::optional<MiniFloatAnalysis> mini_float{};
    std::optional<lispb::codegen::SemanticRelationKind> relationship_kind{};
    std::optional<lispb::codegen::SemanticRelationUnit> relationship_unit{};
    std::optional<std::string> relationship_target{};
    std::optional<std::uint64_t> relationship_target_extent{};
    std::optional<ExactCodeCount> relationship_live_value_count{};
    std::optional<ExactCodeCount> relationship_required_code_count{};
    std::optional<std::uint32_t> relationship_minimum_required_bits{};
    std::optional<bool> relationship_width_sufficient{};
    std::optional<std::uint64_t> relationship_code_space_capacity_limit{};
    std::optional<std::uint64_t> relationship_capacity_headroom{};
    std::optional<std::uint64_t> relationship_semantic_capacity_limit{};
    std::optional<std::uint64_t> relationship_sentinel_capacity_limit{};
    std::optional<std::uint64_t> relationship_effective_capacity_limit{};
    std::optional<std::uint64_t> relationship_effective_capacity_headroom{};
};

}
