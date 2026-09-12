#pragma once

#include <codegen/ast/expr.h>
#include <codegen/ast/member_qualifiers.h>

#include <optional>
#include <string>

namespace codegen {

struct Member {
    CppType type;
    std::string name;
    std::optional<Expr> initializer;
    MemberQualifiers qualifiers;
    std::optional<std::string> template_parameters;

    Member(CppType type, std::string name);
    Member(CppType type, std::string name, Expr initializer);
    Member(CppType type,
           std::string name,
           std::optional<Expr> initializer,
           MemberQualifiers qualifiers,
           std::optional<std::string> template_parameters = std::nullopt);
};

} // namespace codegen
