#include <codegen/schema/normal_module_schemas.h>

#include <cassert>

namespace lispb::codegen {

auto NormalModuleSchemas::index(ModuleHandle id) const -> std::uint32_t {
    assert(id.kind() == ModuleKind::normal);
    auto const row{id.index()};
    assert(row < settings.size());
    return row;
}
auto NormalModuleSchemas::append() -> ModuleHandle {
    auto const index{static_cast<std::uint32_t>(settings.size())};
    assert(index < 0x00fffffe);
    settings.push_back({});
    declarations.push_back({});
    enum_helper_namespace.push_back({});
    return ModuleHandle{(static_cast<std::uint32_t>(ModuleKind::normal) << 24) | (index + 1)};
}
auto NormalModuleSchemas::clone(ModuleHandle id) -> ModuleHandle {
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    settings[destination] = settings[source_index];
    declarations[destination] = declarations[source_index];
    enum_helper_namespace[destination] = enum_helper_namespace[source_index];
    return result;
}
void NormalModuleSchemas::clear() {
    settings.clear();
    declarations.clear();
    enum_helper_namespace.clear();
}

}
