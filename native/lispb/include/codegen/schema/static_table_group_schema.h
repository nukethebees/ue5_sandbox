#pragma once

#include <codegen/schema/type_ref.h>

#include <string>
#include <vector>

namespace codegen {

struct StaticTableGroupSchema {
    std::string name;
    TypeRef type;
    std::vector<std::string> columns;
};

} // namespace codegen
