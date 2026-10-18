#pragma once

#include <codegen/ast/expr.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/template_id.h>
#include <lispb/indices.h>

#include <cstdint>
#include <vector>

namespace lispb::codegen::ir {

struct ConstexprMembers {
    std::vector<CppTypeId> type;
    std::vector<IdentifierId> name;
    std::vector<Expr> initializer;
    std::vector<TemplateId> template_declaration;
    std::vector<std::uint8_t> is_inline;
};

}
