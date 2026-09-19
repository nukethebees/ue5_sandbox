#include <codegen/ast/cpp_type.h>

#include <set>
#include <string_view>
#include <utility>

namespace codegen {
namespace {

auto default_parameter_passing(std::string_view const spelling) -> ParameterPassing {
    static std::set<std::string_view> const value_types{
        "bool",
        "char",
        "char8_t",
        "char16_t",
        "char32_t",
        "double",
        "float",
        "int",
        "int8",
        "int16",
        "int32",
        "int64",
        "long",
        "long double",
        "long int",
        "long long",
        "short",
        "short int",
        "signed char",
        "size_t",
        "std::ptrdiff_t",
        "std::size_t",
        "uint8",
        "uint16",
        "uint32",
        "uint64",
        "unsigned",
        "unsigned char",
        "unsigned int",
        "unsigned long",
        "unsigned long int",
        "unsigned long long",
        "unsigned short",
        "unsigned short int",
        "wchar_t",
    };
    return value_types.contains(spelling) || spelling.ends_with('*')
             ? ParameterPassing::value
             : ParameterPassing::const_reference;
}

} // namespace

CppType::CppType(char const* value)
    : spelling{value}
    , parameter_passing{default_parameter_passing(spelling)} {}
CppType::CppType(std::string value)
    : spelling{std::move(value)}
    , parameter_passing{default_parameter_passing(spelling)} {}
CppType::CppType(std::string value, std::string header)
    : spelling{std::move(value)}
    , dependencies{{TypeDependency{spelling, std::move(header), {}}}}
    , parameter_passing{default_parameter_passing(spelling)} {}
CppType::CppType(std::string value, std::vector<TypeDependency> type_dependencies)
    : spelling{std::move(value)}
    , dependencies{std::move(type_dependencies)}
    , parameter_passing{default_parameter_passing(spelling)} {}

auto CppType::operation(TypeOperation operation_name) const -> std::optional<std::string> {
    auto const found{member_operations.find(operation_name)};
    return found == member_operations.end() ? std::nullopt
                                            : std::optional<std::string>{found->second};
}

auto CppType::operation_parameter_passing(TypeOperation const operation_name) const
    -> ParameterPassing {
    auto const found{member_operation_parameter_passing.find(operation_name)};
    return found == member_operation_parameter_passing.end() ? ParameterPassing::const_reference
                                                             : found->second;
}

} // namespace codegen
