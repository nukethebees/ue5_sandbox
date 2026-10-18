#pragma once

#include <lispb/indices.h>

#include <vector>

namespace lispb::codegen::ir {

struct StaticAssertions {
    std::vector<StringId> condition;
    std::vector<StringId> message;
};

} // namespace lispb::codegen::ir
