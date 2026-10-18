#pragma once

#include <lispb/indices.h>

#include <vector>

namespace lispb::codegen::ir {

struct LineComments {
    std::vector<StringId> text;
};

} // namespace lispb::codegen::ir
