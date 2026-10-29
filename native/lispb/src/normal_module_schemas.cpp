#include <codegen/schema/normal_module_schemas.h>

#include "schema_columns.h"

#include <cassert>

namespace lispb::codegen {

auto NormalModuleSchemas::index(ModuleHandle id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < settings.size());
    return row;
}
auto NormalModuleSchemas::append() -> ModuleHandle {
    auto const index{static_cast<std::uint32_t>(settings.size())};
    assert(index < 0x00fffffe);
    settings.push_back({});
    declarations.push_back({});
    enum_helper_namespace.push_back({});
    s7_constructors.push_back(false);
    return ModuleHandle{(static_cast<std::uint32_t>(ModuleKind::normal) << 24) | (index + 1)};
}
auto NormalModuleSchemas::clone(ModuleHandle id) -> ModuleHandle {
    return detail::clone_schema_row(
        *this, id, settings, declarations, enum_helper_namespace, s7_constructors);
}
void NormalModuleSchemas::clear() {
    detail::clear_schema_columns(settings, declarations, enum_helper_namespace, s7_constructors);
}

}
