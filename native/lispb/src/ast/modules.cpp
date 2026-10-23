#include <codegen/ast/modules.h>

#include <cassert>
#include <cstdint>

namespace lispb::codegen {

auto Modules::append(StringId module_name, CppFileId module_header, CppFileId module_source)
    -> ModuleId {
    name.push_back(module_name);
    header.push_back(module_header);
    source.push_back(module_source);
    return ModuleId{static_cast<std::uint32_t>(name.size())};
}
auto Modules::index(ModuleId module) const -> std::size_t {
    assert(module.value != 0 && module.value <= name.size());
    return module.value - 1;
}
void Modules::clear() {
    name.clear();
    header.clear();
    source.clear();
}

}
