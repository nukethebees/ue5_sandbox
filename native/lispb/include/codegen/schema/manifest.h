#pragma once

#include <codegen/schema/module_schema.h>
#include <codegen/schema/registered_type_schema.h>

#include <map>
#include <string>
#include <vector>

namespace codegen {

struct Manifest {
    int schema_version;
    TypeRegistry types;
    std::vector<ModuleSchema> modules;
};

} // namespace codegen
