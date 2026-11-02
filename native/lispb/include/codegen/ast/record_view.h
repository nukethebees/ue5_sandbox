#pragma once

#include <codegen/ast/record_definition.h>

#include <span>

namespace lispb::codegen {

struct RecordView {
    Expr name{};
    RecordKind kind{RecordKind::struct_};
    std::span<NodeId const> children;
    std::span<CppTypeId const> bases;
    TemplateId template_declaration{};
    Expr requires_clause{};
    StringId export_specifier{};
    std::span<TypeDependencyId const> dependencies;
    std::optional<std::span<StringId const>> scope_bindings;
};

}
