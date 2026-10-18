#include <codegen/schema/enum_unreal_projections.h>

#include <cassert>

namespace lispb::codegen {

auto EnumUnrealProjections::index(EnumUnrealProjectionId id) const -> std::uint32_t {
    assert(id.value != 0);
    auto const row{id.value - 1};
    assert(row < name.size());
    return row;
}
auto EnumUnrealProjections::append() -> EnumUnrealProjectionId {
    auto const index{static_cast<std::uint32_t>(name.size())};
    name.push_back({});
    header.push_back({});
    header_include.push_back({});
    conversion_header.push_back({});
    native_header_include.push_back({});
    reflection.push_back(EnumReflection::uenum);
    enum_array.push_back(false);
    return EnumUnrealProjectionId{index + 1};
}
auto EnumUnrealProjections::clone(EnumUnrealProjectionId id) -> EnumUnrealProjectionId {
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    name[destination] = name[source_index];
    header[destination] = header[source_index];
    header_include[destination] = header_include[source_index];
    conversion_header[destination] = conversion_header[source_index];
    native_header_include[destination] = native_header_include[source_index];
    reflection[destination] = reflection[source_index];
    enum_array[destination] = enum_array[source_index];
    return result;
}
void EnumUnrealProjections::clear() {
    name.clear();
    header.clear();
    header_include.clear();
    conversion_header.clear();
    native_header_include.clear();
    reflection.clear();
    enum_array.clear();
}

}
