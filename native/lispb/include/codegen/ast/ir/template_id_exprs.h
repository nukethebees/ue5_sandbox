#pragma once
#include <codegen/ast/ir/range.h>
#include <codegen/ast/syntax.h>

#include <vector>
namespace lispb::codegen::ir {
struct TemplateIdExprs {
    std::vector<Syntax> callee;
    std::vector<Range> arguments;
};
}
