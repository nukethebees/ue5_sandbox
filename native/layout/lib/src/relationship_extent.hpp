#pragma once

#include "relationship_extent_analysis.hpp"
#include "relationship_extent_requirement.hpp"
#include <ioj/layout/analyzer/relationship_target_facts.hpp>

#include <codegen/schema/declarations.h>
#include <codegen/schema/semantic_relation_kind.h>
#include <lispb/schema/type_graph.h>

#include <cstdint>
#include <optional>
#include <string_view>

namespace ioj::layout::analysis_detail {

auto relationship_extent_requirement(std::uint64_t const extent,
                                     bool const includes_terminal_count,
                                     std::uint64_t const sentinel_codes)
    -> RelationshipExtentRequirement;

auto relationship_code_space_extent_limit(std::uint32_t const width,
                                          bool const includes_terminal_count,
                                          std::uint64_t const sentinel_codes)
    -> std::optional<std::uint64_t>;

auto relationship_semantic_extent_limit(
    lispb::codegen::SemanticRelationKind const kind,
    std::optional<lispb::codegen::PackedIntegerValue> const minimum_value,
    std::optional<lispb::codegen::PackedIntegerValue> const maximum_value)
    -> std::optional<std::uint64_t>;

auto relationship_sentinel_extent_limit(
    lispb::codegen::Declarations const& declarations,
    lispb::codegen::SemanticRelationKind const kind,
    lispb::codegen::SchemaRange<lispb::codegen::PackedNamedCodeId> const named_codes)
    -> std::optional<std::uint64_t>;

auto analyze_unsigned_relationship_extent(
    lispb::codegen::Declarations const& declarations,
    std::string_view const context,
    lispb::codegen::SemanticRelationKind const kind,
    std::uint64_t const target_extent,
    lispb::codegen::SchemaRange<lispb::codegen::PackedNamedCodeId> const named_codes,
    std::uint32_t const width,
    std::optional<lispb::codegen::PackedIntegerValue> const minimum_value,
    std::optional<lispb::codegen::PackedIntegerValue> const maximum_value)
    -> RelationshipExtentAnalysis;

auto relationship_target_extent(lispb::schema::TypeGraph const& types,
                                lispb::schema::RelationshipId const relationship,
                                RelationshipTargetFacts const& target_facts)
    -> std::optional<std::uint64_t>;

}
