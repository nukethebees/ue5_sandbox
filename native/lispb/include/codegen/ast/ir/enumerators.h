#pragma once

#include <codegen/ast/syntax.h>
#include <lispb/indices.h>

#include <optional>
#include <vector>

namespace lispb::codegen::ir {

struct Enumerators {
    std::vector<StringId> name;
    std::vector<std::optional<Syntax>> initializer;
    std::vector<std::optional<StringId>> annotation;
};

} // namespace lispb::codegen::ir
