#pragma once

#include <codegen/ast/indices.h>
#include <codegen/ast/return_syntax.h>
#include <lispb/indices.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace lispb::codegen::ir {

struct FunctionNodes {
    std::vector<FunctionId> function;
    std::vector<std::optional<StringId>> owner;
    std::vector<std::uint8_t> declaration;
    std::vector<std::uint8_t> is_header;
    std::vector<std::optional<ReturnSyntax>> return_syntax;
};

} // namespace lispb::codegen::ir
