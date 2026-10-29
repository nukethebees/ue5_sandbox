#include <codegen/schema/settings_module_schemas.h>

#include "schema_columns.h"

#include <cassert>

namespace lispb::codegen {

auto SettingsModuleSchemas::index(ModuleHandle id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
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
    return detail::clone_schema_row(
        *this, id, settings, api_name, state_name, export_specifier, categories, settings_list);
}
void SettingsModuleSchemas::clear() {
    detail::clear_schema_columns(
        settings, api_name, state_name, export_specifier, categories, settings_list);
}

}
