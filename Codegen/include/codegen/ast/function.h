#pragma once

#include <codegen/ast/function_spec.h>

#include <optional>
#include <string>

namespace codegen {

struct Function {
    FunctionSpec spec;
    std::optional<std::string> owner;
    bool declaration{false};
    bool is_header{false};
};

} // namespace codegen
