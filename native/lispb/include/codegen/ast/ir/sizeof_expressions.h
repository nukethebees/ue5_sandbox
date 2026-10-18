#pragma once

#include <codegen/ast/indices.h>

#include <vector>

namespace lispb::codegen::ir {

struct SizeofExpressions {
    std::vector<CppTypeId> type;
};

} // namespace lispb::codegen::ir
