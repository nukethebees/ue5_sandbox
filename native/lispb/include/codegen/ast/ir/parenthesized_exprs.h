#pragma once
#include <codegen/ast/expr.h>

#include <vector>
namespace lispb::codegen::ir {
struct ParenthesizedExprs {
    std::vector<Expr> operand;
};
}
