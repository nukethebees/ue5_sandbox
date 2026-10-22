#include <codegen/ast/cpp_types.h>

#include <codegen/ast/cpp_type_dependency_range.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/parameter_passing.h>
#include <codegen/ast/render_context.h>
#include <codegen/ast/type_operation.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <algorithm>
#include <cassert>
#include <cctype>
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
    shapes_.clear();
    name_components_.clear();
    parts_.clear();
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
                      ParameterPassing const passing,
                      Shape const shape) -> CppTypeId {
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
    shapes_.push_back(shape);
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
                    std::optional<CppTypeId> const operation_source,
                    Shape const shape) const -> std::optional<CppTypeId> {
    auto const [begin, end]{types_by_spelling_.equal_range(spelling.value)};
    for (auto current{begin}; current != end; ++current) {
        auto const candidate{current->second};
        auto const index{candidate.value - 1};
        auto const candidate_shape{shapes_[index]};
        if (candidate_shape.form != shape.form ||
            (shape.form == Form::composed &&
             !std::ranges::equal(std::span{parts_}.subspan(shape.parts.begin, shape.parts.count),
                                 std::span{parts_}.subspan(candidate_shape.parts.begin,
                                                           candidate_shape.parts.count))) ||
            passing_[index] != passing ||
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
    return append(name, dependencies, policy, {});
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
    return intern_shape(spelling, dependencies, policy, std::nullopt, {});
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
    auto const policy{parameter_passing(base)};
    auto const shape{spelling == this->spelling(base) ? shapes_[base.value - 1] : Shape{}};
    return intern_shape(spelling, dependencies, policy, base, shape);
}

auto CppTypes::intern_shape(std::string_view const spelling,
                            std::span<TypeDependencyId const> const dependencies,
                            ParameterPassing const passing,
                            std::optional<CppTypeId> const operation_source,
                            Shape const shape) -> CppTypeId {
    auto const name{string_id(strings_.intern(spelling))};
    if (auto const found{find(name, dependencies, passing, operation_source, shape)}) {
        return *found;
    }

    auto const result{append(name, dependencies, passing, shape)};
    if (operation_source) {
        for (std::size_t operation{}; operation < operations_.size(); ++operation) {
            operations_[operation][result.value - 1] =
                operations_[operation][operation_source->value - 1];
            operation_passing_[operation][result.value - 1] =
                operation_passing_[operation][operation_source->value - 1];
        }
    }
    return result;
}

auto CppTypes::named(std::string_view const spelling,
                     std::span<TypeDependencyId const> const dependencies,
                     std::optional<ParameterPassing> const passing) -> CppTypeId {
    // Keep explicitly global references explicitly global.
    if (spelling.starts_with("::")) {
        return intern(spelling, dependencies, passing);
    }
    std::string const name{spelling};
    auto const policy{passing.value_or(cpp_type_detail::default_parameter_passing(name))};
    Shape shape{.form = Form::named,
                .parts = {static_cast<std::uint32_t>(name_components_.size()), 0}};
    std::size_t begin{};
    do {
        auto const separator{name.find("::", begin)};
        auto const end{separator == std::string::npos ? name.size() : separator};
        auto const component{std::string_view{name}.substr(begin, end - begin)};
        assert(!component.empty());
        assert(std::isalpha(static_cast<unsigned char>(component.front())) != 0 ||
               component.front() == '_');
        assert(std::ranges::all_of(component, [](unsigned char character) {
            return std::isalnum(character) != 0 || character == '_';
        }));
        name_components_.push_back(string_id(strings_.intern(component)));
        ++shape.parts.count;
        if (separator == std::string::npos) {
            break;
        }
        begin = separator + 2;
    } while (true);
    auto const result{intern_shape(name, dependencies, policy, {}, shape)};
    if (shapes_[result.value - 1].parts.begin != shape.parts.begin) {
        name_components_.resize(shape.parts.begin);
    }
    return result;
}

auto CppTypes::compose(std::span<CppTypePart const> const parts,
                       std::optional<CppTypeId> const policy_source) -> CppTypeId {
    assert(!parts.empty());
    std::string spelling;
    std::vector<TypeDependencyId> dependencies;
    for (auto const part : parts) {
        assert(part.text.value == 0 || part.type.value == 0);
        if (part.type.value != 0) {
            spelling += this->spelling(part.type);
            for (auto const dependency : this->dependencies(part.type)) {
                if (std::ranges::find(dependencies, dependency) == dependencies.end()) {
                    dependencies.push_back(dependency);
                }
            }
        } else {
            spelling += strings_.view(part.text);
        }
    }

    Shape const shape{.form = Form::composed,
                      .parts = {static_cast<std::uint32_t>(parts_.size()),
                                static_cast<std::uint32_t>(parts.size())}};
    parts_.insert(parts_.end(), parts.begin(), parts.end());
    auto const passing{policy_source ? parameter_passing(*policy_source)
                                     : cpp_type_detail::default_parameter_passing(spelling)};
    auto const result{intern_shape(spelling, dependencies, passing, policy_source, shape)};
    if (shapes_[result.value - 1].parts.begin != shape.parts.begin) {
        parts_.resize(shape.parts.begin);
    }
    return result;
}

auto CppTypes::reference(CppTypeId const type) -> CppTypeId {
    if (shapes_[type.value - 1].form != Form::raw) {
        return type;
    }
    std::string const name{spelling(type)};
    if (name.find("::") == std::string::npos || name.starts_with("::") || name.ends_with("::")) {
        return type;
    }
    bool component_start{true};
    for (std::size_t index{}; index < name.size(); ++index) {
        auto const character{static_cast<unsigned char>(name[index])};
        if (character == ':' && !component_start && index + 1 < name.size() &&
            name[index + 1] == ':') {
            ++index;
            component_start = true;
        } else if ((std::isalpha(character) != 0 || character == '_') ||
                   (!component_start && std::isdigit(character) != 0)) {
            component_start = false;
        } else {
            return type;
        }
    }
    auto const structured{named(name)};
    return intern_shape(
        name, dependencies(type), parameter_passing(type), type, shapes_[structured.value - 1]);
}

auto CppTypes::decorate(CppTypeId const base,
                        std::string_view const prefix,
                        std::string_view const suffix) -> CppTypeId {
    // Intern both pieces before publishing views into the shared string buffer.
    std::string const suffix_text{suffix};
    auto const before{string_id(strings_.intern(prefix))};
    auto const after{string_id(strings_.intern(suffix_text))};
    return compose({{.text = before}, {.type = base}, {.text = after}}, base);
}

auto CppTypes::template_type(CppTypeId const primary, std::span<CppTypeId const> const arguments)
    -> CppTypeId {
    std::vector<CppTypePart> parts{{.type = primary}, {.text = string_id(strings_.intern("<"))}};
    for (auto const argument : arguments) {
        if (parts.size() != 2) {
            parts.push_back({.text = string_id(strings_.intern(", "))});
        }
        parts.push_back({.type = argument});
    }
    parts.push_back({.text = string_id(strings_.intern(">"))});
    return compose(parts);
}

auto CppTypes::with_dependencies(CppTypeId const base,
                                 std::span<TypeDependencyId const> const dependencies)
    -> CppTypeId {
    return intern_shape(
        spelling(base), dependencies, parameter_passing(base), base, shapes_[base.value - 1]);
}
auto CppTypes::clone(CppTypeId const base) -> CppTypeId {
    auto const result{append(
        spelling_id(base), dependencies(base), parameter_passing(base), shapes_[base.value - 1])};
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
