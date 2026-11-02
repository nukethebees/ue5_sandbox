#pragma once

#include <codegen/ast/expr.h>
#include <codegen/ast/indices.h>
#include <lispb/indices.h>

#include <vector>

namespace lispb::codegen::ir {

struct Parameters {
    std::vector<CppTypeId> type;
    std::vector<StringId> name;
    std::vector<Expr> default_value;
};

} // namespace lispb::codegen::ir
