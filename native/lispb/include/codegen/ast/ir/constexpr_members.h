#pragma once

#include <codegen/ast/indices.h>
#include <codegen/ast/syntax.h>
#include <codegen/ast/template_id.h>
#include <lispb/indices.h>

#include <vector>

namespace lispb::codegen::ir {

struct ConstexprMembers {
    std::vector<CppTypeId> type;
    std::vector<StringId> name;
    std::vector<Syntax> initializer;
    std::vector<TemplateId> template_declaration;
};

}
