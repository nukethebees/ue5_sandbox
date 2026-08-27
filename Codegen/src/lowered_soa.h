#pragma once

#include <codegen/ast.h>

namespace codegen::detail {

struct LoweredSoa {
    Nodes header;
    Nodes source;
};

} // namespace codegen::detail
