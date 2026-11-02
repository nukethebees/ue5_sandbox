#pragma once

#include <codegen/ast/expr.h>
#include <codegen/ast/indices.h>
#include <lispb/indices.h>

namespace lispb::codegen {

struct FunctionParameterView {
    CppTypeId type{};
    StringId name{};
    Expr default_value{};
};

}
