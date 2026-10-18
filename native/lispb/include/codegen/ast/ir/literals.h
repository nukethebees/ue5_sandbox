#pragma once

#include <lispb/indices.h>

#include <vector>

namespace lispb::codegen::ir {

struct Literals {
    std::vector<StringId> spelling;
};

} // namespace lispb::codegen::ir
