#pragma once

#include <ioj/layout/declaration_kind.hpp>

#include <codegen/schema/declaration_handle.h>
#include <lispb/schema/type_graph.h>

namespace ioj::layout {
struct DeclarationCapabilities {
    auto operator==(DeclarationCapabilities const&) const -> bool = default;

    DeclarationKind kind{DeclarationKind::external};
    bool inspectable{};
    bool editable{};
    bool physical_analysis_available{};
    bool supports_variant_overrides{};
};
auto declaration_capabilities(lispb::schema::TypeGraph const& types,
                              lispb::schema::TypeId const node) -> DeclarationCapabilities;
auto declaration_capabilities(lispb::codegen::DeclarationHandle declaration)
    -> DeclarationCapabilities;
}
