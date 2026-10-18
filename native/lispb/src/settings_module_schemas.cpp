#include <codegen/schema/settings_module_schemas.h>

#include <cassert>

namespace lispb::codegen {

auto SettingsModuleSchemas::index(ModuleHandle id) const -> std::uint32_t {
    assert(id.kind() == ModuleKind::settings);
    auto const row{id.index()};
    assert(row < settings.size());
    return row;
}
auto SettingsModuleSchemas::append() -> ModuleHandle {
    auto const index{static_cast<std::uint32_t>(settings.size())};
    assert(index < 0x00fffffe);
    settings.push_back({});
    api_name.push_back({});
    state_name.push_back({});
    export_specifier.push_back({});
    categories.push_back({});
    settings_list.push_back({});
    return ModuleHandle{(static_cast<std::uint32_t>(ModuleKind::settings) << 24) | (index + 1)};
}
auto SettingsModuleSchemas::clone(ModuleHandle id) -> ModuleHandle {
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    settings[destination] = settings[source_index];
    api_name[destination] = api_name[source_index];
    state_name[destination] = state_name[source_index];
    export_specifier[destination] = export_specifier[source_index];
    categories[destination] = categories[source_index];
    settings_list[destination] = settings_list[source_index];
    return result;
}
void SettingsModuleSchemas::clear() {
    settings.clear();
    api_name.clear();
    state_name.clear();
    export_specifier.clear();
    categories.clear();
    settings_list.clear();
}

}
