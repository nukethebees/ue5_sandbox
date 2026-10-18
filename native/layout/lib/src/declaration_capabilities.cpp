#include <ioj/layout/declaration_capabilities.hpp>

#include <codegen/schema/soa_backend.h>
#include <lispb/schema/soa_source_kind.h>
#include <lispb/schema/soa_types.h>

#include <utility>

namespace ioj::layout {
auto declaration_capabilities(lispb::schema::TypeGraph const& types,
                              lispb::schema::TypeId const node) -> DeclarationCapabilities {
    switch ((types.definition(node)).kind()) {
        case lispb::schema::TypeDefinitionKind::external: {
            return {.kind = DeclarationKind::external, .inspectable = true};
        }
        case lispb::schema::TypeDefinitionKind::enumeration: {
            return {.kind = DeclarationKind::enumeration,
                    .inspectable = true,
                    .editable = true,
                    .physical_analysis_available = true};
        }
        case lispb::schema::TypeDefinitionKind::integer_scalar: {
            return {.kind = DeclarationKind::integer_scalar,
                    .inspectable = true,
                    .editable = true,
                    .physical_analysis_available = true};
        }
        case lispb::schema::TypeDefinitionKind::linear_quantized: {
            return {.kind = DeclarationKind::linear_quantized,
                    .inspectable = true,
                    .editable = true,
                    .physical_analysis_available = true};
        }
        case lispb::schema::TypeDefinitionKind::integer_varint: {
            return {.kind = DeclarationKind::integer_varint,
                    .inspectable = true,
                    .editable = true,
                    .physical_analysis_available = true};
        }
        case lispb::schema::TypeDefinitionKind::fixed_point: {
            return {.kind = DeclarationKind::fixed_point,
                    .inspectable = true,
                    .editable = true,
                    .physical_analysis_available = true};
        }
        case lispb::schema::TypeDefinitionKind::mini_float: {
            return {.kind = DeclarationKind::mini_float,
                    .inspectable = true,
                    .editable = true,
                    .physical_analysis_available = true};
        }
        case lispb::schema::TypeDefinitionKind::optional_sentinel: {
            return {.kind = DeclarationKind::optional_sentinel,
                    .inspectable = true,
                    .editable = true,
                    .physical_analysis_available = true};
        }
        case lispb::schema::TypeDefinitionKind::optional_presence_bit: {
            return {.kind = DeclarationKind::optional_presence_bit,
                    .inspectable = true,
                    .editable = true,
                    .physical_analysis_available = true};
        }
        case lispb::schema::TypeDefinitionKind::packed: {
            return {.kind = DeclarationKind::packed,
                    .inspectable = true,
                    .editable = true,
                    .physical_analysis_available = true,
                    .supports_variant_overrides = true};
        }
        case lispb::schema::TypeDefinitionKind::record: {
            return {.kind = DeclarationKind::record,
                    .inspectable = true,
                    .editable = true,
                    .physical_analysis_available = true};
        }
        case lispb::schema::TypeDefinitionKind::union_type: {
            return {.kind = DeclarationKind::union_,
                    .inspectable = true,
                    .editable = true,
                    .physical_analysis_available = true};
        }
        case lispb::schema::TypeDefinitionKind::tagged_union: {
            return {.kind = DeclarationKind::tagged_union,
                    .inspectable = true,
                    .editable = true,
                    .physical_analysis_available = true};
        }
        case lispb::schema::TypeDefinitionKind::static_table: {
            return {.kind = DeclarationKind::static_table, .inspectable = true, .editable = true};
        }
        case lispb::schema::TypeDefinitionKind::facade: {
            return {.kind = DeclarationKind::facade, .inspectable = true, .editable = true};
        }
        case lispb::schema::TypeDefinitionKind::homogeneous_storage: {
            return {
                .kind = DeclarationKind::homogeneous_layout, .inspectable = true, .editable = true};
        }
        case lispb::schema::TypeDefinitionKind::soa: {
            auto const supported{
                types.definitions()
                    .soas.backend[types.definitions().soas.index(types.definition(node))] ==
                lispb::codegen::SoaBackend::standard_library};
            auto const vector{types.definitions().soas.source_kind[types.definitions().soas.index(
                                  types.definition(node))] == lispb::schema::SoaSourceKind::vector};
            return {.kind = vector ? DeclarationKind::vector_soa : DeclarationKind::soa,
                    .inspectable = true,
                    .editable = supported && !vector,
                    .physical_analysis_available = supported,
                    .supports_variant_overrides = supported};
        }
        default:
            std::unreachable();
    }
}

auto declaration_capabilities(lispb::codegen::DeclarationHandle const declaration)
    -> DeclarationCapabilities {
    switch (declaration.kind()) {
        case lispb::codegen::DeclarationKind::homogeneous_layout:
            return {
                .kind = DeclarationKind::homogeneous_layout, .inspectable = true, .editable = true};
        case lispb::codegen::DeclarationKind::static_table:
            return {.kind = DeclarationKind::static_table, .inspectable = true, .editable = true};
        case lispb::codegen::DeclarationKind::facade:
            return {.kind = DeclarationKind::facade, .inspectable = true, .editable = true};
        default:
            return {};
    }
}
}
