#pragma once
#include <codegen/ast/record_kind.h>
#include <codegen/ast/template_id.h>
#include <lispb/indices.h>

#include <vector>

namespace lispb::codegen::ir {

struct ForwardDeclarations {
    std::vector<StringId> name;
    std::vector<RecordKind> kind;
    std::vector<TemplateId> template_declaration;
    std::vector<StringId> export_specifier;
};

} // namespace lispb::codegen::ir
