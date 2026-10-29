#include <codegen/schema/enum_schemas.h>

#include "schema_columns.h"

#include <cassert>

namespace lispb::codegen {

auto EnumSchemas::index(DeclarationHandle id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < name.size());
    return row;
}
auto EnumSchemas::append() -> DeclarationHandle {
    auto const index{static_cast<std::uint32_t>(name.size())};
    assert(index < 0x00fffffe);
    cpp_projection.push_back({});
    name.push_back({});
    underlying_type.push_back({});
    values.push_back({});
    count.push_back({});
    conversions.push_back({});
    export_specifier.push_back({});
    unreal_projection.push_back({});
    bit_width.push_back({});
    reflection.push_back(EnumReflection::none);
    signedness.push_back(schema::EnumSignedness::unspecified);
    enum_array.push_back(false);
    native_api.push_back(false);
    helpers.push_back({});
    return DeclarationHandle{(static_cast<std::uint32_t>(DeclarationKind::enumeration) << 24) |
                             (index + 1)};
}
auto EnumSchemas::clone(DeclarationHandle id) -> DeclarationHandle {
    return detail::clone_schema_row(*this,
                                    id,
                                    cpp_projection,
                                    name,
                                    underlying_type,
                                    values,
                                    count,
                                    conversions,
                                    export_specifier,
                                    unreal_projection,
                                    bit_width,
                                    reflection,
                                    signedness,
                                    enum_array,
                                    native_api,
                                    helpers);
}
void EnumSchemas::clear() {
    detail::clear_schema_columns(cpp_projection,
                                 name,
                                 underlying_type,
                                 values,
                                 count,
                                 conversions,
                                 export_specifier,
                                 unreal_projection,
                                 bit_width,
                                 reflection,
                                 signedness,
                                 enum_array,
                                 native_api,
                                 helpers);
}

}
