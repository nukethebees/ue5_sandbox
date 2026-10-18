#pragma once

#include <codegen/schema/module_handle.h>
#include <codegen/schema/type_registry.h>

#include <vector>

namespace lispb::codegen {

class Declarations;

struct Manifest {
    Manifest(int version,
             TypeRegistry registry,
             Declarations& store,
             std::vector<ModuleHandle> ordered_modules);
    Manifest(Manifest const&) = default;
    Manifest(Manifest&&) = default;
    auto operator=(Manifest const& other) -> Manifest&;
    auto operator=(Manifest&& other) noexcept -> Manifest&;

    int schema_version;
    TypeRegistry types;
    Declarations& declarations;
    std::vector<ModuleHandle> modules;
};

} // namespace lispb::codegen
