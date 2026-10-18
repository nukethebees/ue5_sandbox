#pragma once

#include <codegen/ast/ir/range.h>
#include <lispb/indices.h>

#include <vector>

namespace lispb::codegen::ir {

struct Namespaces {
    std::vector<StringId> name;
    std::vector<Range> children;
};

} // namespace lispb::codegen::ir
