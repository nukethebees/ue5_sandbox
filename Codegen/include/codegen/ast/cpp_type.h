#pragma once

#include <codegen/ast/type_dependency.h>
#include <codegen/ast/type_operation.h>

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace codegen {

struct CppType {
    std::string spelling;
    std::vector<TypeDependency> dependencies;
    std::map<TypeOperation, std::string> member_operations;

    CppType() = default;
    CppType(char const* spelling);
    CppType(std::string spelling);
    CppType(std::string spelling, std::string header);
    CppType(std::string spelling, std::vector<TypeDependency> dependencies);

    auto operation(TypeOperation operation) const -> std::optional<std::string>;
};

} // namespace codegen
