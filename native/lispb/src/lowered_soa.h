#pragma once

#include <codegen/ast/nodes.h>
namespace lispb::codegen::detail {

struct LoweredSoa {
    Nodes header;
    Nodes source;
};

} // namespace lispb::codegen::detail
