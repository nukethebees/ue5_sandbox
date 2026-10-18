#pragma once
#include <codegen/ast/indices.h>
#include <codegen/ast/template_id.h>
#include <lispb/indices.h>

#include <vector>

namespace lispb::codegen::ir {

struct UsingDeclarations {
    std::vector<StringId> name;
    std::vector<CppTypeId> type;
    std::vector<TemplateId> template_declaration;
};

} // namespace lispb::codegen::ir
