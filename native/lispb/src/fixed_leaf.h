#pragma once

#include <codegen/ast/cpp_type.h>

#include <string>
#include <vector>

namespace codegen::detail {

struct FixedLeaf {
    std::vector<std::string> path;
    CppType type;
};

} // namespace codegen::detail
