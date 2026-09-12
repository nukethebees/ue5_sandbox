#pragma once

#include <codegen/schema/type_ref.h>

#include <string>

namespace codegen {

struct StaticTableColumnSchema {
    std::string name;
    TypeRef type;
};

} // namespace codegen
