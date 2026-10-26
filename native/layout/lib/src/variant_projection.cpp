#include "variant_projection.hpp"

#include <ioj/layout/analyzer/physical_type_spelling.hpp>

#include <codegen/ast/cpp_types.h>
#include <lispb/schema/soa_columns.h>
#include <lispb/strings.h>

#include <cstdint>
#include <stdexcept>
#include <string>

namespace ioj::layout::analysis_detail {

auto effective_bit_layout(lispb::schema::TypeGraph const& types,
                          lispb::schema::TypeId type,
                          ResolvedVariant const& variant) -> lispb::schema::BitLayout {
    lispb::schema::BitLayoutOverrides overrides;
    for (auto const& [field, bits] : variant.overrides.packed_field_widths) {
        if (field.type == type) {
            overrides.field_bits.emplace(std::string{types.strings().view(field.field_name)}, bits);
        }
    }
    if (auto const found{variant.overrides.packed_storage_types.find(type)};
        found != variant.overrides.packed_storage_types.end()) {
        overrides.storage_bits = lispb::schema::packed_storage_bits(found->second);
        if (!overrides.storage_bits) {
            throw std::invalid_argument{
                "Packed storage override must be an unsigned fixed-width C++ integer."};
        }
    }
    return resolve_bit_layout(types, type, overrides);
}

auto effective_storage_type(lispb::schema::TypeGraph const& types,
                            lispb::schema::TypeId const type,
                            lispb::schema::TypeDefinitionId const&,
                            ResolvedVariant const& variant) -> std::string {
    auto const bits{effective_bit_layout(types, type, variant).storage_bits};
    return "std::uint" + std::to_string(bits) + "_t";
}

auto effective_column_type(lispb::schema::TypeGraph const& types,
                           lispb::schema::TypeId const type,
                           lispb::schema::SoaColumnId const& column,
                           ResolvedVariant const& variant) -> std::string {
    auto const found{variant.overrides.soa_column_types.find(FieldOverrideId{
        .type = type,
        .field_name =
            types.definitions().soa_columns.name[types.definitions().soa_columns.index(column)]})};
    if (found != variant.overrides.soa_column_types.end()) {
        return found->second;
    }
    return physical_type_spelling(
               types,
               types.definitions()
                   .soa_columns.semantic_type[types.definitions().soa_columns.index(column)])
        .value_or(std::string{
            types.cpp_types().spelling(types.occurrences().cpp_type[types.occurrences().index(
                types.definitions()
                    .soa_columns.semantic_type[types.definitions().soa_columns.index(column)])])});
}

}
