#include <codegen/schema/enum_schemas.h>

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
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    name[destination] = name[source_index];
    underlying_type[destination] = underlying_type[source_index];
    values[destination] = values[source_index];
    count[destination] = count[source_index];
    conversions[destination] = conversions[source_index];
    export_specifier[destination] = export_specifier[source_index];
    unreal_projection[destination] = unreal_projection[source_index];
    bit_width[destination] = bit_width[source_index];
    reflection[destination] = reflection[source_index];
    signedness[destination] = signedness[source_index];
    enum_array[destination] = enum_array[source_index];
    native_api[destination] = native_api[source_index];
    helpers[destination] = helpers[source_index];
    return result;
}
void EnumSchemas::clear() {
    name.clear();
    underlying_type.clear();
    values.clear();
    count.clear();
    conversions.clear();
    export_specifier.clear();
    unreal_projection.clear();
    bit_width.clear();
    reflection.clear();
    signedness.clear();
    enum_array.clear();
    native_api.clear();
    helpers.clear();
}

}
