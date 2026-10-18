#pragma once

#include <codegen/ast/cpp_type_dependency_range.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/parameter_passing.h>
#include <codegen/ast/type_operation.h>
#include <lispb/indices.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace lispb {
class Strings;
}

namespace lispb::codegen {

class CppTypes {
  public:
    explicit CppTypes(Strings& strings);
    CppTypes(CppTypes const&) = delete;
    auto operator=(CppTypes const&) -> CppTypes& = delete;

    auto strings() const -> Strings&;
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

    auto spelling(CppTypeId type) const -> std::string_view;
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
    auto append(StringId spelling,
                std::span<TypeDependencyId const> dependencies,
                ParameterPassing passing) -> CppTypeId;
    auto find(StringId spelling,
              std::span<TypeDependencyId const> dependencies,
              ParameterPassing passing,
              std::optional<CppTypeId> operation_source) const -> std::optional<CppTypeId>;

    Strings& strings_;
    std::vector<StringId> spellings_;
    std::vector<CppTypeDependencyRange> dependencies_;
    std::vector<ParameterPassing> passing_;
    std::array<std::vector<IdentifierId>, 3> operations_;
    std::array<std::vector<ParameterPassing>, 3> operation_passing_;
    std::vector<TypeDependencyId> dependency_uses_;
    std::unordered_multimap<std::size_t, CppTypeDependencyRange> dependency_lists_;
    std::unordered_multimap<std::uint32_t, CppTypeId> types_by_spelling_;
    std::vector<StringId> dependency_spellings_;
    std::vector<StringId> dependency_headers_;
    std::vector<std::uint8_t> dependency_has_headers_;
    std::unordered_map<std::uint64_t, TypeDependencyId> dependency_index_;
};

} // namespace lispb::codegen
