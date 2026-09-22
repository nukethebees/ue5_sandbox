#pragma once

#include <ioj/layout/abi_profile.hpp>
#include <ioj/layout/workspace.hpp>

#include <lispb/schema/type_graph.h>

#include <cstdint>

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
};

struct DeclarationCapabilities {
    DeclarationKind kind{DeclarationKind::external};
    bool visible{};
    bool editable{};
    bool has_physical_layout{};
    bool supports_variants{};
    bool supports_access{};
};

enum class LayoutStatus { available, unknown, error };

auto declaration_capabilities(lispb::schema::TypeNode const& node) -> DeclarationCapabilities;
auto declaration_kind_label(DeclarationKind kind) -> char const*;
auto declaration_status(lispb::schema::TypeGraph const& types,
                        lispb::schema::TypeId type,
                        Variant const& baseline,
                        AbiProfile const& abi,
                        std::uint64_t default_capacity) -> LayoutStatus;

} // namespace ioj::layout
