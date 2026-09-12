#pragma once

#include <codegen/ast/node_fwd.h>

#include <string>

namespace codegen {

struct Namespace {
    std::string name;
    Nodes children;
};

} // namespace codegen
