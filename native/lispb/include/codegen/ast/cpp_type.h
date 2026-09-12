#pragma once

#include <codegen/ast/type_dependency.h>
#include <codegen/ast/type_operation.h>

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace codegen {

enum class ParameterPassing {
    const_reference,
    value,
};

struct CppType {
    std::string spelling;
    std::vector<TypeDependency> dependencies;
    std::map<TypeOperation, std::string> member_operations;
    std::map<TypeOperation, ParameterPassing> member_operation_parameter_passing;
    ParameterPassing parameter_passing{ParameterPassing::const_reference};

    CppType() = default;
    CppType(char const* spelling);
    CppType(std::string spelling);
    CppType(std::string spelling, std::string header);
    CppType(std::string spelling, std::vector<TypeDependency> dependencies);

    auto operation(TypeOperation operation) const -> std::optional<std::string>;
    auto operation_parameter_passing(TypeOperation operation) const -> ParameterPassing;
};

} // namespace codegen
