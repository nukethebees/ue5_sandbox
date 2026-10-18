#pragma once

#include <codegen/ast/block_id.h>
#include <codegen/ast/ir/range.h>

#include <vector>

namespace lispb::codegen::ir {

struct FunctionBodies {
    std::vector<BlockId> block;
    std::vector<Range> initializers;
};

}
