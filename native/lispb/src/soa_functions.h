#pragma once

#include <codegen/ast/indices.h>
#include <lispb/indices.h>

#include <vector>

namespace lispb::codegen::detail {

struct SoaFunctions {
    std::vector<FunctionId> const_view;
    std::vector<FunctionId> mutable_view;
    std::vector<FunctionId> operations;
    std::vector<FunctionId> permutations;
};

}
