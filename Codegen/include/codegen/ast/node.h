#pragma once

#include <codegen/ast/access_specifier.h>
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

#include <variant>

namespace codegen {

using NodeValue = std::variant<Raw,
                               NewLines,
                               AccessSpecifier,
                               Include,
                               IncludeDependencies,
                               ForwardDeclaration,
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
