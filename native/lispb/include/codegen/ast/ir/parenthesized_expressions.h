#pragma once
#include <codegen/ast/expr.h>

#include <vector>
namespace lispb::codegen::ir {
struct ParenthesizedExpressions {
    std::vector<Expr> operand;
};
}
