#include <codegen/schema/umbrella_module_schemas.h>

#include "schema_columns.h"

#include <cassert>

namespace lispb::codegen {

auto UmbrellaModuleSchemas::index(ModuleHandle id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < settings.size());
    return row;
}
auto UmbrellaModuleSchemas::append() -> ModuleHandle {
    auto const index{static_cast<std::uint32_t>(settings.size())};
    assert(index < 0x00fffffe);
    settings.push_back({});
    headers.push_back({});
    return ModuleHandle{(static_cast<std::uint32_t>(ModuleKind::umbrella) << 24) | (index + 1)};
}
auto UmbrellaModuleSchemas::clone(ModuleHandle id) -> ModuleHandle {
    return detail::clone_schema_row(*this, id, settings, headers);
}
void UmbrellaModuleSchemas::clear() {
    detail::clear_schema_columns(settings, headers);
}

}
