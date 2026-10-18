#include "variant_projection.hpp"

#include <ioj/layout/analyzer/physical_type_spelling.hpp>

#include <codegen/ast/cpp_types.h>
#include <lispb/schema/packed_fields.h>
#include <lispb/schema/packed_types.h>
#include <lispb/schema/soa_columns.h>
#include <lispb/strings.h>

#include <cstdint>
#include <string>

namespace ioj::layout::analysis_detail {

auto effective_field_width(lispb::schema::TypeGraph const& types,
                           lispb::schema::TypeId const type,
                           lispb::schema::PackedSegmentId const& field,
                           ResolvedVariant const& variant) -> std::uint32_t {
    auto const found{variant.overrides.packed_field_widths.find(FieldOverrideId{
        .type = type,
        .field_name = types.definitions()
                          .packed_fields.name[types.definitions().packed_fields.index(field)]})};
    return found == variant.overrides.packed_field_widths.end()
             ? types.definitions()
                   .packed_fields.bit_width[types.definitions().packed_fields.index(field)]
             : found->second;
}

auto effective_storage_type(lispb::schema::TypeGraph const& types,
                            lispb::schema::TypeId const type,
                            lispb::schema::TypeDefinitionId const& packed,
                            ResolvedVariant const& variant) -> std::string {
    auto const found{variant.overrides.packed_storage_types.find(type)};
    if (found != variant.overrides.packed_storage_types.end()) {
        return found->second;
    }
    return physical_type_spelling(
               types,
               types.definitions().packed.storage_type[types.definitions().packed.index(packed)])
        .value_or(std::string{
            types.cpp_types().spelling(types.occurrences().cpp_type[types.occurrences().index(
                types.definitions()
                    .packed.storage_type[types.definitions().packed.index(packed)])])});
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
