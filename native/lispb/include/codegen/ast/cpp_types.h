#pragma once

#include <ioj/enum_array.h>

#include <codegen/ast/cpp_type_dependency_range.h>
#include <codegen/ast/cpp_type_part.h>
#include <codegen/ast/cpp_type_shape.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/integer_type.h>
#include <codegen/ast/ir/range.h>
#include <codegen/ast/parameter_passing.h>
#include <codegen/ast/standard_types.h>
#include <codegen/ast/type_operation.h>
#include <lispb/indices.h>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace lispb {
class Strings;
}

namespace lispb::codegen {

class CppIR;
class CppRenderer;

class CppTypes {
  public:
    explicit CppTypes(Strings& strings);
    CppTypes(CppTypes const&) = delete;
    auto operator=(CppTypes const&) -> CppTypes& = delete;

    auto strings() const -> Strings&;
    auto standard_types() const -> StandardTypes const& { return std; }
    void clear();

    // Add a distinct registration whose policies may still be configured.
    auto add(std::string_view spelling,
             std::span<TypeDependencyId const> dependencies = {},
             std::optional<ParameterPassing> passing = std::nullopt) -> CppTypeId;
    auto add(std::string_view spelling,
             std::initializer_list<TypeDependencyId> dependencies,
             std::optional<ParameterPassing> passing = std::nullopt) -> CppTypeId;
    // Reuse a complete metadata match; equal spellings alone never identify types.
    auto intern(std::string_view spelling,
                std::span<TypeDependencyId const> dependencies = {},
                std::optional<ParameterPassing> passing = std::nullopt) -> CppTypeId;
    auto intern(std::string_view spelling,
                std::initializer_list<TypeDependencyId> dependencies,
                std::optional<ParameterPassing> passing = std::nullopt) -> CppTypeId;
    auto derive(CppTypeId base, std::string_view spelling) -> CppTypeId;
    auto derive(CppTypeId base,
                std::string_view spelling,
                std::span<TypeDependencyId const> dependencies) -> CppTypeId;

    // Resolve names from the enclosing scope unless a global origin is supplied.
    auto named(std::string_view spelling,
               std::span<TypeDependencyId const> dependencies = {},
               std::optional<ParameterPassing> passing = std::nullopt) -> CppTypeId;
    auto named(std::string_view spelling,
               std::initializer_list<TypeDependencyId> dependencies,
               std::optional<ParameterPassing> passing = std::nullopt) -> CppTypeId {
        return named(spelling, std::span<TypeDependencyId const>{dependencies}, passing);
    }
    auto named(std::string_view spelling,
               NameOrigin origin,
               std::span<TypeDependencyId const> dependencies = {},
               std::optional<ParameterPassing> passing = std::nullopt) -> CppTypeId;
    auto named(std::string_view spelling,
               NameOrigin origin,
               std::initializer_list<TypeDependencyId> dependencies,
               std::optional<ParameterPassing> passing = std::nullopt) -> CppTypeId {
        return named(spelling, origin, std::span<TypeDependencyId const>{dependencies}, passing);
    }
    // Structure a simple qualified identifier at an authored type boundary; otherwise retain it
    // raw.
    auto reference(CppTypeId type) -> CppTypeId;
    // Require at most one of text/type in each part. Empty text is permitted.
    // Inherit policies from policy_source, if set.
    auto compose(std::span<CppTypePart const> parts, std::optional<CppTypeId> policy_source = {})
        -> CppTypeId;
    auto compose(std::initializer_list<CppTypePart> parts,
                 std::optional<CppTypeId> policy_source = {}) -> CppTypeId {
        return compose(std::span<CppTypePart const>{parts}, policy_source);
    }
    auto decorate(CppTypeId base, std::string_view prefix, std::string_view suffix) -> CppTypeId;
    auto nested_type(CppTypeId owner, std::string_view member) -> CppTypeId;
    auto nested_type(CppTypeId owner, StringId member) -> CppTypeId;
    auto template_type(CppTypeId primary, std::span<CppTypeId const> arguments) -> CppTypeId;
    auto template_type(CppTypeId primary, std::initializer_list<CppTypeId> arguments) -> CppTypeId {
        return template_type(primary, std::span<CppTypeId const>{arguments});
    }
    auto with_dependencies(CppTypeId base, std::span<TypeDependencyId const> dependencies)
        -> CppTypeId;
    // Return an unpublished copy whose policies can be configured independently.
    auto clone(CppTypeId base) -> CppTypeId;

    // Expression-dependent types have no spelling until rendered in their enclosing scope.
    auto spelling(CppTypeId type) const -> std::string_view;
    auto unqualified_type(CppTypeId type) const -> CppTypeId;
    auto integer_type(CppTypeId type) const -> IntegerType;
    auto spelling_id(CppTypeId type) const -> StringId;
    auto dependencies(CppTypeId type) const -> std::span<TypeDependencyId const>;
    auto parameter_passing(CppTypeId type) const -> ParameterPassing;
    // Configure only an unpublished type returned by add(), never a shared derived type.
    void set_parameter_passing(CppTypeId type, ParameterPassing passing);
    auto operation(CppTypeId type, TypeOperation operation) const
        -> std::optional<std::string_view>;
    auto operation_parameter_passing(CppTypeId type, TypeOperation operation) const
        -> ParameterPassing;
    void set_operation(CppTypeId type,
                       TypeOperation operation,
                       std::string_view spelling,
                       ParameterPassing passing = ParameterPassing::const_reference);

    auto dependency(std::string_view spelling,
                    std::optional<std::string_view> header = std::nullopt) -> TypeDependencyId;
    auto dependency_spelling(TypeDependencyId dependency) const -> std::string_view;
    auto dependency_header(TypeDependencyId dependency) const -> std::optional<std::string_view>;
  private:
    friend class CppIR;
    friend class CppRenderer;
    auto decltype_type(Syntax operand, std::span<TypeDependencyId const> dependencies) -> CppTypeId;
    auto closure_type(Expr lambda, std::span<TypeDependencyId const> dependencies) -> CppTypeId;
    auto template_type(Syntax expression, std::span<TypeDependencyId const> dependencies)
        -> CppTypeId;
    void initialise_standard_types();
    auto compose(std::span<CppTypePart const> parts,
                 std::optional<CppTypeId> policy_source,
                 CppTypeId initializer_element_type,
                 CppTypeId unqualified_type = {}) -> CppTypeId;
    auto append(StringId spelling,
                std::span<TypeDependencyId const> dependencies,
                ParameterPassing passing,
                CppTypeShape shape) -> CppTypeId;
    auto find(StringId spelling,
              std::span<TypeDependencyId const> dependencies,
              ParameterPassing passing,
              std::optional<CppTypeId> operation_source,
              CppTypeShape shape) const -> std::optional<CppTypeId>;
    auto intern_shape(std::string_view spelling,
                      std::span<TypeDependencyId const> dependencies,
                      ParameterPassing passing,
                      std::optional<CppTypeId> operation_source,
                      CppTypeShape shape) -> CppTypeId;

    Strings& strings_;
    std::vector<StringId> spellings_;
    std::vector<CppTypeShape> shapes_;
    std::vector<StringId> name_components_;
    std::vector<CppTypePart> parts_;
    std::vector<CppTypeDependencyRange> dependencies_;
    std::vector<ParameterPassing> passing_;
    ioj::EnumArray<std::vector<StringId>, TypeOperation> operations_;
    ioj::EnumArray<std::vector<ParameterPassing>, TypeOperation> operation_passing_;
    std::vector<TypeDependencyId> dependency_uses_;
    std::unordered_multimap<std::size_t, CppTypeDependencyRange> dependency_lists_;
    std::unordered_multimap<std::uint32_t, CppTypeId> types_by_spelling_;
    std::vector<StringId> dependency_spellings_;
    std::vector<StringId> dependency_headers_;
    std::vector<std::uint8_t> dependency_has_headers_;
    std::unordered_map<std::uint64_t, TypeDependencyId> dependency_index_;

  public:
    StandardTypes std;
};

} // namespace lispb::codegen
