#pragma once

#include <codegen/ast/access.h>
#include <codegen/ast/access_indentation.h>

#include <vector>

namespace lispb::codegen::ir {

struct AccessSpecifiers {
    std::vector<Access> access;
    std::vector<AccessIndentation> indentation;
};

} // namespace lispb::codegen::ir
