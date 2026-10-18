#pragma once

#include <lispb/indices.h>

#include <cstdint>
#include <vector>

namespace lispb::codegen::ir {

struct LineDirectives {
    std::vector<std::uint32_t> line;
    std::vector<StringId> path;
};

} // namespace lispb::codegen::ir
