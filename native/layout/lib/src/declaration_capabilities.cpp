#include <ioj/layout/declaration_capabilities.hpp>

#include <utility>

namespace ioj::layout {
auto declaration_capabilities(lispb::schema::TypeGraph const& types,
                              lispb::schema::TypeId const node) -> DeclarationCapabilities {
    using enum lispb::schema::TypeDefinitionKind;
    auto const physical{[](DeclarationKind kind) {
        return DeclarationCapabilities{.kind = kind,
                                       .inspectable = true,
                                       .editable = true,
                                       .physical_analysis_available = true};
    }};
    switch (types.definition(node).kind()) {
        case external: {
            return {.kind = DeclarationKind::external, .inspectable = true};
        }
        case enumeration: {
            return physical(DeclarationKind::enumeration);
        }
        case integer_scalar: {
            return physical(DeclarationKind::integer_scalar);
        }
        case linear_quantized: {
            return physical(DeclarationKind::linear_quantized);
        }
        case integer_varint: {
            return physical(DeclarationKind::integer_varint);
        }
        case fixed_point: {
            return physical(DeclarationKind::fixed_point);
        }
        case mini_float: {
            return physical(DeclarationKind::mini_float);
        }
        case optional_sentinel: {
            return physical(DeclarationKind::optional_sentinel);
        }
        case optional_presence_bit: {
            return physical(DeclarationKind::optional_presence_bit);
        }
        case packed: {
            return {.kind = DeclarationKind::packed,
                    .inspectable = true,
                    .editable = true,
                    .physical_analysis_available = true,
                    .supports_variant_overrides = true};
        }
        case record: {
            return physical(DeclarationKind::record);
        }
        case union_type: {
            return physical(DeclarationKind::union_);
        }
        case tagged_union: {
            return physical(DeclarationKind::tagged_union);
        }
        case static_table: {
            return {.kind = DeclarationKind::static_table, .inspectable = true, .editable = true};
        }
        case facade: {
            return {.kind = DeclarationKind::facade, .inspectable = true, .editable = true};
        }
        case soa: {
            return {.kind = DeclarationKind::soa,
                    .inspectable = true,
                    .editable = true,
                    .physical_analysis_available = true,
                    .supports_variant_overrides = true};
        }
        default:
            std::unreachable();
    }
}

auto declaration_capabilities(lispb::codegen::DeclarationHandle const declaration)
    -> DeclarationCapabilities {
    switch (declaration.kind()) {
        case lispb::codegen::DeclarationKind::static_table:
            return {.kind = DeclarationKind::static_table, .inspectable = true, .editable = true};
        case lispb::codegen::DeclarationKind::facade:
            return {.kind = DeclarationKind::facade, .inspectable = true, .editable = true};
        default:
            return {};
    }
}
}
