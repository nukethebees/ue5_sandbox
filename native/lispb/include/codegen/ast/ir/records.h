#pragma once
#include <codegen/ast/expr.h>
#include <codegen/ast/ir/range.h>
#include <codegen/ast/record_kind.h>
#include <codegen/ast/template_id.h>
#include <lispb/indices.h>

#include <vector>

namespace lispb::codegen::ir {

struct Records {
    std::vector<Expr> name;
    std::vector<Range> children;
    std::vector<Range> bases;
    std::vector<TemplateId> template_declaration;
    std::vector<Expr> requires_clause;
    std::vector<IdentifierId> export_specifier;
    std::vector<RecordKind> record_kind;
    std::vector<Range> dependencies;
};

} // namespace lispb::codegen::ir
