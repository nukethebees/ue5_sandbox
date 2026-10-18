#pragma once
#include <codegen/ast/indices.h>

#include <vector>
namespace lispb::codegen::ir {
struct TypeReferenceExpressions {
    std::vector<CppTypeId> type;
};
}
