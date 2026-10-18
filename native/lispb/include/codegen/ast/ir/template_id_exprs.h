#pragma once
#include <codegen/ast/expr.h>
#include <codegen/ast/ir/range.h>

#include <vector>
namespace lispb::codegen::ir {
struct TemplateIdExprs {
    std::vector<Expr> callee;
    std::vector<Range> arguments;
};
}
