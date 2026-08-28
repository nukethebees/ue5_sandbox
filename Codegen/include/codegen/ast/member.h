#pragma once

#include <codegen/ast/cpp_type.h>
#include <codegen/ast/member_qualifiers.h>

#include <optional>
#include <string>

namespace codegen {

struct Member {
    CppType type;
    std::string name;
    std::optional<std::string> initializer;
    MemberQualifiers qualifiers;

    Member(CppType type, std::string name);
    Member(CppType type, std::string name, std::string initializer);
    Member(CppType type,
           std::string name,
           std::optional<std::string> initializer,
           MemberQualifiers qualifiers);
};

} // namespace codegen
