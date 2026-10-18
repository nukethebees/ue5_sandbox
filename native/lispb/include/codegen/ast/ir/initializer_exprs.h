#pragma once

#include <codegen/ast/indices.h>
#include <codegen/ast/ir/range.h>

#include <optional>
#include <vector>

namespace lispb::codegen::ir {

struct InitializerExprs {
    std::vector<std::optional<CppTypeId>> type;
    std::vector<Range> elements;
};

} // namespace lispb::codegen::ir
