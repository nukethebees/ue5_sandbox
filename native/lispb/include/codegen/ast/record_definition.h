#pragma once

#include <codegen/ast/indices.h>
#include <codegen/ast/node_id.h>
#include <codegen/ast/record_kind.h>
#include <codegen/ast/syntax.h>
#include <codegen/ast/template_id.h>
#include <lispb/indices.h>

#include <optional>
#include <vector>

namespace lispb::codegen {

struct RecordDefinition {
    Syntax name{};
    RecordKind kind{RecordKind::struct_};
    std::vector<NodeId> children;
    std::vector<CppTypeId> bases;
    TemplateId template_declaration{};
    Expr requires_clause{};
    StringId export_specifier{};
    std::vector<TypeDependencyId> dependencies;
    // An empty list declares complete binding information; absence leaves it unknown.
    std::optional<std::vector<StringId>> scope_bindings;
};

}
