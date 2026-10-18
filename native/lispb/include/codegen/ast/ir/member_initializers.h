#pragma once

#include <lispb/indices.h>

#include <vector>

namespace lispb::codegen::ir {

struct MemberInitializers {
    std::vector<StringId> member;
    std::vector<StringId> value;
};

} // namespace lispb::codegen::ir
