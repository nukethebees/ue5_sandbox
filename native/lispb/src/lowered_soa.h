#pragma once

#include <codegen/ast/nodes.h>
namespace lispb::codegen::detail {

struct LoweredSoa {
    Nodes header;
    Nodes source;
    Nodes resolved;
    Nodes compact;
};

} // namespace lispb::codegen::detail
