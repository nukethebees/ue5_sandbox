#include <codegen/schema/manifest.h>

#include <codegen/schema/declarations.h>
#include <codegen/schema/module_handle.h>
#include <codegen/schema/type_registry.h>

#include <cassert>
#include <utility>
#include <vector>

namespace lispb::codegen {

Manifest::Manifest(int version,
                   TypeRegistry registry,
                   Declarations& store,
                   std::vector<ModuleHandle> ordered_modules)
    : schema_version{version}
    , types{std::move(registry)}
    , declarations{store}
    , modules{std::move(ordered_modules)} {}
auto Manifest::operator=(Manifest const& other) -> Manifest& {
    assert(&declarations == &other.declarations);
    schema_version = other.schema_version;
    types = other.types;
    modules = other.modules;
    return *this;
}
auto Manifest::operator=(Manifest&& other) noexcept -> Manifest& {
    assert(&declarations == &other.declarations);
    schema_version = other.schema_version;
    types = std::move(other.types);
    modules = std::move(other.modules);
    return *this;
}

} // namespace lispb::codegen
