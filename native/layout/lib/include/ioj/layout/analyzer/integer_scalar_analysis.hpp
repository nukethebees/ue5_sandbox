#pragma once

#include <ioj/layout/analyzer/exact_code_count.hpp>
#include <ioj/layout/analyzer/packed_named_code_analysis.hpp>
#include <ioj/layout/diagnostic.hpp>

#include <codegen/schema/packed_integer_value.h>
#include <codegen/schema/semantic_relation_kind.h>
#include <codegen/schema/semantic_relation_unit.h>
#include <lispb/schema/indices.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ioj::layout {

struct IntegerScalarAnalysis {
    lispb::schema::TypeId type{};
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
    std::vector<PackedNamedCodeAnalysis> named_codes{};
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
    std::vector<Diagnostic> diagnostics{};
};

}
