#pragma once

#include <codegen/ast/forward_declaration.h>
#include <codegen/ast/function.h>
#include <codegen/ast/include.h>
#include <codegen/ast/include_dependencies.h>
#include <codegen/ast/member.h>
#include <codegen/ast/namespace.h>
#include <codegen/ast/new_lines.h>
#include <codegen/ast/raw.h>
#include <codegen/ast/struct.h>
#include <codegen/ast/using_declaration.h>

#include <concepts>
#include <utility>
#include <variant>

namespace codegen {

struct Node {
    using Value = std::variant<Raw,
                               NewLines,
                               Include,
                               IncludeDependencies,
                               ForwardDeclaration,
                               UsingDeclaration,
                               Member,
                               Function,
                               Struct,
                               Namespace>;

    Value value;

    template <typename T>
        requires std::constructible_from<Value, T>
    Node(T node) : value{std::move(node)} {}
};

} // namespace codegen
