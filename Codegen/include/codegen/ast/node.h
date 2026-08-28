#pragma once

#include <codegen/ast/access_specifier.h>
#include <codegen/ast/assignment_statement.h>
#include <codegen/ast/expression_statement.h>
#include <codegen/ast/forward_declaration.h>
#include <codegen/ast/friend_declaration.h>
#include <codegen/ast/function.h>
#include <codegen/ast/include.h>
#include <codegen/ast/include_dependencies.h>
#include <codegen/ast/member.h>
#include <codegen/ast/namespace.h>
#include <codegen/ast/new_lines.h>
#include <codegen/ast/raw.h>
#include <codegen/ast/return_statement.h>
#include <codegen/ast/struct.h>
#include <codegen/ast/using_declaration.h>
#include <codegen/ast/variable_declaration_statement.h>

#include <variant>

namespace codegen {

using NodeValue = std::variant<Raw,
                               ExpressionStatement,
                               ReturnStatement,
                               AssignmentStatement,
                               VariableDeclarationStatement,
                               NewLines,
                               AccessSpecifier,
                               Include,
                               IncludeDependencies,
                               ForwardDeclaration,
                               FriendDeclaration,
                               UsingDeclaration,
                               Member,
                               Function,
                               Struct,
                               Namespace>;

struct Node : NodeValue {
    using NodeValue::NodeValue;
    using NodeValue::operator=;

    template <typename T>
    auto get_if() noexcept -> T* {
        return std::get_if<T>(static_cast<NodeValue*>(this));
    }

    template <typename T>
    auto get_if() const noexcept -> T const* {
        return std::get_if<T>(static_cast<NodeValue const*>(this));
    }

    template <typename T>
    auto is() const noexcept -> bool {
        return std::holds_alternative<T>(static_cast<NodeValue const&>(*this));
    }
};

} // namespace codegen
