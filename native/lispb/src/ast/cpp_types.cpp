#include <codegen/ast/cpp_types.h>

#include <codegen/ast/cpp_type_dependency_range.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/parameter_passing.h>
#include <codegen/ast/type_operation.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace lispb::codegen {

namespace cpp_type_detail {

auto default_parameter_passing(std::string_view const spelling) -> ParameterPassing {
    static constexpr std::string_view value_types[]{
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
    return std::ranges::find(value_types, spelling) != std::end(value_types) ||
                   spelling.ends_with('*')
             ? ParameterPassing::value
             : ParameterPassing::const_reference;
}

} // namespace cpp_type_detail

CppTypes::CppTypes(Strings& strings)
    : strings_{strings} {}
auto CppTypes::strings() const -> Strings& {
    return strings_;
}
void CppTypes::clear() {
    spellings_.clear();
    dependencies_.clear();
    passing_.clear();
    for (auto& operations : operations_) {
        operations.clear();
    }
    for (auto& passing : operation_passing_) {
        passing.clear();
    }
    dependency_uses_.clear();
    dependency_lists_.clear();
    types_by_spelling_.clear();
    dependency_spellings_.clear();
    dependency_headers_.clear();
    dependency_has_headers_.clear();
    dependency_index_.clear();
}
auto CppTypes::append(StringId const spelling,
                      std::span<TypeDependencyId const> const dependencies,
                      ParameterPassing const passing) -> CppTypeId {
    assert(spellings_.size() < std::numeric_limits<std::uint32_t>::max());
    assert(dependency_uses_.size() + dependencies.size() <=
           std::numeric_limits<std::uint32_t>::max());

    CppTypeDependencyRange range{};
    if (!dependencies.empty()) {
        std::size_t hash{};
        for (auto const dependency : dependencies) {
            hash ^= dependency.value + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        }
        auto const [begin, end]{dependency_lists_.equal_range(hash)};
        for (auto current{begin}; current != end; ++current) {
            auto const candidate{current->second};
            if (std::ranges::equal(
                    dependencies,
                    std::span{dependency_uses_}.subspan(candidate.begin, candidate.count))) {
                range = candidate;
                break;
            }
        }
        if (range.count == 0) {
            range = {static_cast<std::uint32_t>(dependency_uses_.size()),
                     static_cast<std::uint32_t>(dependencies.size())};
            auto const begin{dependency_uses_.data()};
            auto const end{begin + dependency_uses_.size()};
            auto const within_store{
                !dependency_uses_.empty() &&
                !std::less<TypeDependencyId const*>{}(dependencies.data(), begin) &&
                std::less<TypeDependencyId const*>{}(dependencies.data(), end)};
            if (within_store) {
                auto const offset{static_cast<std::size_t>(dependencies.data() - begin)};
                dependency_uses_.resize(dependency_uses_.size() + dependencies.size());
                std::copy_n(dependency_uses_.data() + offset,
                            range.count,
                            dependency_uses_.data() + range.begin);
            } else {
                dependency_uses_.insert(
                    dependency_uses_.end(), dependencies.begin(), dependencies.end());
            }
            dependency_lists_.emplace(hash, range);
        }
    }

    auto const result{CppTypeId{static_cast<std::uint32_t>(spellings_.size() + 1)}};
    spellings_.push_back(spelling);
    dependencies_.push_back(range);
    passing_.push_back(passing);
    for (auto& operations : operations_) {
        operations.emplace_back();
    }
    for (auto& operation_passing : operation_passing_) {
        operation_passing.push_back(ParameterPassing::const_reference);
    }
    types_by_spelling_.emplace(spelling.value, result);
    return result;
}
auto CppTypes::find(StringId const spelling,
                    std::span<TypeDependencyId const> const dependencies,
                    ParameterPassing const passing,
                    std::optional<CppTypeId> const operation_source) const
    -> std::optional<CppTypeId> {
    auto const [begin, end]{types_by_spelling_.equal_range(spelling.value)};
    for (auto current{begin}; current != end; ++current) {
        auto const candidate{current->second};
        auto const index{candidate.value - 1};
        if (passing_[index] != passing ||
            !std::ranges::equal(this->dependencies(candidate), dependencies)) {
            continue;
        }

        bool same_operations{true};
        for (std::size_t operation{}; operation < operations_.size(); ++operation) {
            auto const name{operation_source ? operations_[operation][operation_source->value - 1]
                                             : IdentifierId{}};
            auto const policy{operation_source
                                  ? operation_passing_[operation][operation_source->value - 1]
                                  : ParameterPassing::const_reference};
            if (operations_[operation][index] != name ||
                operation_passing_[operation][index] != policy) {
                same_operations = false;
                break;
            }
        }
        if (same_operations) {
            return candidate;
        }
    }
    return std::nullopt;
}
auto CppTypes::add(std::string_view const spelling,
                   std::span<TypeDependencyId const> const dependencies,
                   std::optional<ParameterPassing> const passing) -> CppTypeId {
    auto const policy{passing.value_or(cpp_type_detail::default_parameter_passing(spelling))};
    auto const name{string_id(strings_.intern(spelling))};
    return append(name, dependencies, policy);
}
auto CppTypes::add(std::string_view const spelling,
                   std::initializer_list<TypeDependencyId> const dependencies,
                   std::optional<ParameterPassing> const passing) -> CppTypeId {
    return add(spelling, std::span{dependencies}, passing);
}
auto CppTypes::intern(std::string_view const spelling,
                      std::span<TypeDependencyId const> const dependencies,
                      std::optional<ParameterPassing> const passing) -> CppTypeId {
    auto const policy{passing.value_or(cpp_type_detail::default_parameter_passing(spelling))};
    auto const name{string_id(strings_.intern(spelling))};
    if (auto const found{find(name, dependencies, policy, std::nullopt)}) {
        return *found;
    }
    return append(name, dependencies, policy);
}
auto CppTypes::intern(std::string_view const spelling,
                      std::initializer_list<TypeDependencyId> const dependencies,
                      std::optional<ParameterPassing> const passing) -> CppTypeId {
    return intern(spelling, std::span{dependencies}, passing);
}
auto CppTypes::derive(CppTypeId const base, std::string_view const spelling) -> CppTypeId {
    return derive(base, spelling, dependencies(base));
}
auto CppTypes::derive(CppTypeId const base,
                      std::string_view const spelling,
                      std::span<TypeDependencyId const> const dependencies) -> CppTypeId {
    assert(base.value != 0 && base.value <= spellings_.size());
    auto const name{string_id(strings_.intern(spelling))};
    auto const policy{parameter_passing(base)};
    if (auto const found{find(name, dependencies, policy, base)}) {
        return *found;
    }

    auto const result{append(name, dependencies, policy)};
    for (std::size_t operation{}; operation < operations_.size(); ++operation) {
        operations_[operation][result.value - 1] = operations_[operation][base.value - 1];
        operation_passing_[operation][result.value - 1] =
            operation_passing_[operation][base.value - 1];
    }
    return result;
}
auto CppTypes::spelling(CppTypeId const type) const -> std::string_view {
    return strings_.view(spelling_id(type));
}
auto CppTypes::spelling_id(CppTypeId const type) const -> StringId {
    assert(type.value != 0 && type.value <= spellings_.size());
    return spellings_[type.value - 1];
}
auto CppTypes::dependencies(CppTypeId const type) const -> std::span<TypeDependencyId const> {
    assert(type.value != 0 && type.value <= spellings_.size());
    auto const range{dependencies_[type.value - 1]};
    return std::span{dependency_uses_}.subspan(range.begin, range.count);
}
auto CppTypes::parameter_passing(CppTypeId const type) const -> ParameterPassing {
    assert(type.value != 0 && type.value <= spellings_.size());
    return passing_[type.value - 1];
}
void CppTypes::set_parameter_passing(CppTypeId const type, ParameterPassing const passing) {
    assert(type.value != 0 && type.value <= spellings_.size());
    passing_[type.value - 1] = passing;
}
auto CppTypes::operation(CppTypeId const type, TypeOperation const operation) const
    -> std::optional<std::string_view> {
    assert(type.value != 0 && type.value <= spellings_.size());
    auto const name{operations_[static_cast<std::size_t>(operation)][type.value - 1]};
    return name.value == 0 ? std::nullopt : std::optional{strings_.view(name)};
}
auto CppTypes::operation_parameter_passing(CppTypeId const type,
                                           TypeOperation const operation) const
    -> ParameterPassing {
    assert(type.value != 0 && type.value <= spellings_.size());
    return operation_passing_[static_cast<std::size_t>(operation)][type.value - 1];
}
void CppTypes::set_operation(CppTypeId const type,
                             TypeOperation const operation,
                             std::string_view const spelling,
                             ParameterPassing const passing) {
    assert(type.value != 0 && type.value <= spellings_.size());
    auto const index{static_cast<std::size_t>(operation)};
    operations_[index][type.value - 1] = strings_.intern(spelling);
    operation_passing_[index][type.value - 1] = passing;
}
auto CppTypes::dependency(std::string_view const spelling,
                          std::optional<std::string_view> const header) -> TypeDependencyId {
    // Preserve the second input if interning the first grows shared character storage.
    auto const header_text{header ? std::string{*header} : std::string{}};
    auto const name{string_id(strings_.intern(spelling))};
    auto const include{string_id(strings_.intern(header_text))};
    auto const header_key{header ? include.value : std::numeric_limits<std::uint32_t>::max()};
    auto const key{static_cast<std::uint64_t>(name.value) << 32 | header_key};
    if (auto const found{dependency_index_.find(key)}; found != dependency_index_.end()) {
        return found->second;
    }

    assert(dependency_spellings_.size() < std::numeric_limits<std::uint32_t>::max());
    auto const result{
        TypeDependencyId{static_cast<std::uint32_t>(dependency_spellings_.size() + 1)}};
    dependency_spellings_.push_back(name);
    dependency_headers_.push_back(include);
    dependency_has_headers_.push_back(header.has_value());
    dependency_index_.emplace(key, result);
    return result;
}
auto CppTypes::dependency_spelling(TypeDependencyId const dependency) const -> std::string_view {
    assert(dependency.value != 0 && dependency.value <= dependency_spellings_.size());
    return strings_.view(dependency_spellings_[dependency.value - 1]);
}
auto CppTypes::dependency_header(TypeDependencyId const dependency) const
    -> std::optional<std::string_view> {
    assert(dependency.value != 0 && dependency.value <= dependency_spellings_.size());
    auto const header{dependency_headers_[dependency.value - 1]};
    return dependency_has_headers_[dependency.value - 1] == 0
             ? std::nullopt
             : std::optional{strings_.view(header)};
}

} // namespace lispb::codegen
