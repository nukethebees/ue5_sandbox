#pragma once

#include <codegen/ast/block_id.h>
#include <codegen/ast/syntax.h>

#include <optional>
#include <vector>

namespace lispb::codegen::ir {

struct DeclarationMacros {
    std::vector<Syntax> invocation;
    std::vector<std::optional<BlockId>> body;
    std::vector<bool> statement;
};

} // namespace lispb::codegen::ir
