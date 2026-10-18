#pragma once
#include <codegen/ast/ir/range.h>
#include <lispb/indices.h>

#include <vector>
namespace lispb::codegen::ir {
struct Templates {
    std::vector<Range> parameters;
    std::vector<StringId> authored_text;
};
}
