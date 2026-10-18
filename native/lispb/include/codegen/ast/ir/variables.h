#pragma once

#include <codegen/ast/expr.h>
#include <codegen/ast/indices.h>
#include <lispb/indices.h>

#include <optional>
#include <vector>

namespace lispb::codegen::ir {

struct Variables {
    std::vector<CppTypeId> type;
    std::vector<StringId> name;
    std::vector<std::optional<Expr>> initializer;
};

} // namespace lispb::codegen::ir
