#pragma once

#include <codegen/ast/syntax.h>
#include <lispb/indices.h>

#include <vector>

namespace lispb::codegen::ir {

struct DesignatedInitializers {
    std::vector<StringId> member;
    std::vector<Syntax> value;
};

}
