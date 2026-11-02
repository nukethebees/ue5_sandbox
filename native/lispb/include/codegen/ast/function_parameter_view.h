#pragma once

#include <codegen/ast/indices.h>
#include <codegen/ast/syntax.h>
#include <lispb/indices.h>

namespace lispb::codegen {

struct FunctionParameterView {
    CppTypeId type{};
    StringId name{};
    Syntax default_value{};
};

}
