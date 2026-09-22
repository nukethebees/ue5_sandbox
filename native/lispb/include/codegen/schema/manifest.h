#pragma once

#include <codegen/ast/cpp_type.h>
#include <codegen/schema/module_schema.h>

#include <map>
#include <string>
#include <vector>

namespace codegen {

struct Manifest {
    int schema_version;
    std::map<std::string, CppType> types;
    std::vector<ModuleSchema> modules;
};

} // namespace codegen
