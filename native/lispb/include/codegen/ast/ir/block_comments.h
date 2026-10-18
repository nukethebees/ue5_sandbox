#pragma once

#include <lispb/indices.h>

#include <vector>

namespace lispb::codegen::ir {

struct BlockComments {
    std::vector<StringId> text;
};

} // namespace lispb::codegen::ir
