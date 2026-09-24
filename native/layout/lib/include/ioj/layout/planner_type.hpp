#pragma once

#include <ioj/layout/abi_profile.hpp>
#include <ioj/layout/analyzer.hpp>
#include <ioj/layout/workspace.hpp>

#include <lispb/schema/type_graph.h>

#include <cstdint>
#include <span>

namespace ioj::layout {

enum class DeclarationKind {
    external,
    enumeration,
    integer_scalar,
    linear_quantized,
    integer_varint,
    fixed_point,
    mini_float,
    optional_sentinel,
    optional_presence_bit,
    packed,
    record,
    union_,
    tagged_union,
    soa,
    vector_soa,
    homogeneous_layout,
    static_table,
    facade,
};

struct DeclarationCapabilities {
    DeclarationKind kind{DeclarationKind::external};
    bool inspectable{};
    bool editable{};
    bool physical_analysis_available{};
    bool supports_variant_overrides{};
};

enum class LayoutStatus { available, unknown, error };

auto integer_source_reference(lispb::schema::TypeNode const& node) -> std::string;

auto declaration_capabilities(lispb::schema::TypeNode const& node) -> DeclarationCapabilities;
auto declaration_capabilities(codegen::DeclarationSchema const& declaration)
    -> DeclarationCapabilities;
auto declaration_kind_label(DeclarationKind kind) -> char const*;
auto declaration_status(lispb::schema::TypeGraph const& types,
                        lispb::schema::TypeId type,
                        Variant const& variant,
                        AbiProfile const& abi,
                        std::uint64_t default_capacity) -> LayoutStatus;
auto declaration_status(lispb::schema::TypeGraph const& types,
                        lispb::schema::TypeId type,
                        Variant const& variant,
                        AbiProfile const& abi,
                        std::uint64_t default_capacity,
                        std::uint64_t element_count,
                        SoaAllocationStrategy allocation_strategy,
                        std::span<RelationshipTargetFacts const> relationship_targets)
    -> LayoutStatus;

} // namespace ioj::layout
