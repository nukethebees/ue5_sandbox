#pragma once
#include <codegen/ast/indices.h>
#include <codegen/ast/record_kind.h>
#include <codegen/ast/template_id.h>
#include <lispb/indices.h>

#include <vector>

namespace lispb::codegen::ir {

struct FriendDeclarations {
    std::vector<StringId> name;
    std::vector<RecordKind> kind;
    std::vector<TemplateId> template_declaration;
    std::vector<CppTypeId> type;
};

} // namespace lispb::codegen::ir
