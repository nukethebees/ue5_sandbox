#pragma once

#include <codegen/ast/cpp_type.h>

#include <optional>
#include <string>

namespace codegen {

struct Member {
    CppType type;
    std::string name;
    std::optional<std::string> initializer;

    Member(CppType type, std::string name);
    Member(CppType type, std::string name, std::string initializer);
};

} // namespace codegen
