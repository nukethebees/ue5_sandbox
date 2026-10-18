#include <codegen/schema/module_settings_store.h>

#include <cassert>

namespace lispb::codegen {

auto ModuleSettingsStore::index(ModuleSettingsId id) const -> std::uint32_t {
    assert(id.value != 0);
    auto const row{id.value - 1};
    assert(row < name.size());
    return row;
}
auto ModuleSettingsStore::append() -> ModuleSettingsId {
    auto const index{static_cast<std::uint32_t>(name.size())};
    name.push_back({});
    header.push_back({});
    source.push_back({});
    header_include.push_back({});
    namespace_name.push_back({});
    include_order.push_back({});
    prelude_lines.push_back({});
    return ModuleSettingsId{index + 1};
}
auto ModuleSettingsStore::clone(ModuleSettingsId id) -> ModuleSettingsId {
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    name[destination] = name[source_index];
    header[destination] = header[source_index];
    source[destination] = source[source_index];
    header_include[destination] = header_include[source_index];
    namespace_name[destination] = namespace_name[source_index];
    include_order[destination] = include_order[source_index];
    prelude_lines[destination] = prelude_lines[source_index];
    return result;
}
void ModuleSettingsStore::clear() {
    name.clear();
    header.clear();
    source.clear();
    header_include.clear();
    namespace_name.clear();
    include_order.clear();
    prelude_lines.clear();
}

}
