#pragma once

#include <codegen/ast/ir/range.h>
#include <codegen/ast/template_id.h>

#include <vector>

namespace lispb::codegen::ir {

struct LambdaExprs {
    std::vector<Range> captures;
    std::vector<Range> parameters;
    std::vector<Range> body;
    std::vector<TemplateId> template_declaration;
};

} // namespace lispb::codegen::ir
