#pragma once
#include <codegen/ast/indices.h>

#include <vector>
namespace lispb::codegen::ir {
struct TypeReferenceExprs {
    std::vector<CppTypeId> type;
};
}
