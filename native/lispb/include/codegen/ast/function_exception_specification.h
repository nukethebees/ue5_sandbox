#pragma once

#include <codegen/ast/expr.h>
#include <codegen/ast/function_exception_kind.h>

namespace lispb::codegen {

struct FunctionExceptionSpecification {
    FunctionExceptionKind kind{FunctionExceptionKind::potentially_throwing};
    Expr condition{};
};

}
