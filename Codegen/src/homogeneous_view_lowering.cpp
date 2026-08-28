#include "homogeneous_internal.h"
#include "lowering_utils.h"

#include <utility>

namespace codegen::detail {
namespace {

TypeDependency const std_forward{"std::forward", "utility", {}};
TypeDependency const std_remove_const{"std::remove_const_t", "type_traits", {}};
TypeDependency const tarray_view{"TArrayView", "Containers/ArrayView.h", {}};
TypeDependency const check_dependency{"check", "CoreMinimal.h", {}};

auto compact_function_formatting() -> FunctionFormatting {
    return FunctionFormatting{
        .body_layout = FunctionFormatting::BodyLayout::compact,
        .template_placement = FunctionFormatting::TemplatePlacement::same_line,
    };
}

auto homogeneous_function(std::string name,
                          CppType return_type,
                          std::vector<FunctionParameter> parameters,
                          std::string body,
                          FunctionQualifiers qualifiers = {},
                          std::optional<std::string> function_template = std::nullopt,
                          FunctionFormatting formatting = {},
                          std::vector<TypeDependency> dependencies = {}) -> Node {
    return header_function(FunctionSpec{
        .name = std::move(name),
        .return_type = std::move(return_type),
        .parameters = std::move(parameters),
        .body = {raw(std::move(body), std::move(dependencies))},
        .qualifiers = std::move(qualifiers),
        .is_inline = true,
        .template_parameters = std::move(function_template),
        .formatting = formatting,
    });
}

} // namespace

auto homogeneous_view_node(HomogeneousLayoutSchema const& layout, bool const has_equivalent_type)
    -> Node {
    auto const view_name{"T" + layout.name + "View"};
    auto const components{join(layout.components, ", ")};
    auto slice_values = [&](std::string const& operation) {
        std::vector<std::string> values;
        for (auto const& component : layout.components) {
            values.push_back(component + "." + operation);
        }
        return join(values, ", ");
    };
    NodeListBuilder children;
    children.add(UsingDeclaration{"size_type", CppType{"TArrayView<T>::SizeType"}}, 1)
        .add(UsingDeclaration{"value_type", CppType{"std::remove_const_t<T>"}}, 1)
        .add(UsingDeclaration{"View", CppType{view_name + "<T>"}}, 1)
        .add(UsingDeclaration{"ConstView", CppType{view_name + "<value_type const>"}},
             has_equivalent_type ? 1 : 2);
    if (has_equivalent_type) {
        children.add(UsingDeclaration{"equivalent_type",
                                      CppType{"typename T" + layout.name +
                                              "EquivalentType<value_type>::type"}},
                     2);
    }
    for (std::size_t index{0}; index < layout.components.size(); ++index) {
        children.add(Member{CppType{"TArrayView<T>"}, layout.components[index]},
                     index + 1 < layout.components.size() ? 1 : 2);
    }
    children
        .add(homogeneous_function("get_view",
                                  "auto",
                                  {},
                                  "return View{" + components + "};",
                                  {.trailing_return_type = CppType{"View"}},
                                  std::nullopt,
                                  compact_function_formatting()),
             1)
        .add(homogeneous_function("get_view",
                                  "auto",
                                  {FunctionParameter{"size_type const", "offset"},
                                   FunctionParameter{"size_type const", "count"}},
                                  "return get_view().slice(offset, count);",
                                  {.trailing_return_type = CppType{"View"}}),
             1)
        .add(homogeneous_function("get_view",
                                  "auto",
                                  {},
                                  "return ConstView{" + components + "};",
                                  {.trailing_return_type = CppType{"ConstView"}, .is_const = true},
                                  std::nullopt,
                                  compact_function_formatting()),
             1)
        .add(homogeneous_function("get_view",
                                  "auto",
                                  {FunctionParameter{"size_type const", "offset"},
                                   FunctionParameter{"size_type const", "count"}},
                                  "return get_view().slice(offset, count);",
                                  {.trailing_return_type = CppType{"ConstView"}, .is_const = true}),
             1)
        .add(homogeneous_function("get_const_view",
                                  "auto",
                                  {},
                                  "return ConstView{" + components + "};",
                                  {.trailing_return_type = CppType{"ConstView"}, .is_const = true},
                                  std::nullopt,
                                  compact_function_formatting()),
             1)
        .add(homogeneous_function("get_const_view",
                                  "auto",
                                  {FunctionParameter{"size_type const", "offset"},
                                   FunctionParameter{"size_type const", "count"}},
                                  "return get_const_view().slice(offset, count);",
                                  {.trailing_return_type = CppType{"ConstView"}, .is_const = true}),
             1)
        .add(homogeneous_function("apply_arrays",
                                  "auto",
                                  {FunctionParameter{"TFunc&&", "func"}},
                                  "return std::forward<TFunc>(func)(" + components + ");",
                                  {.trailing_return_type = CppType{"decltype(auto)"}},
                                  "typename TFunc",
                                  {},
                                  {std_forward}),
             1)
        .add(homogeneous_function(
                 "apply_arrays",
                 "auto",
                 {FunctionParameter{"TFunc&&", "func"}},
                 "return std::forward<TFunc>(func)(" + components + ");",
                 {.trailing_return_type = CppType{"decltype(auto)"}, .is_const = true},
                 "typename TFunc",
                 {},
                 {std_forward}),
             1)
        .add(homogeneous_function("num",
                                  "auto",
                                  {},
                                  "return " + layout.components.front() + ".Num();",
                                  {.trailing_return_type = CppType{"size_type"}, .is_const = true},
                                  std::nullopt,
                                  compact_function_formatting()),
             1)
        .add(homogeneous_function("is_empty",
                                  "auto",
                                  {},
                                  "return num() == 0;",
                                  {.trailing_return_type = CppType{"bool"}, .is_const = true},
                                  std::nullopt,
                                  compact_function_formatting()),
             1)
        .add(homogeneous_function("slice",
                                  "auto",
                                  {FunctionParameter{"size_type const", "offset"},
                                   FunctionParameter{"size_type const", "count"}},
                                  "return " + view_name + "{" +
                                      slice_values("Slice(offset, count)") + "};",
                                  {.trailing_return_type = CppType{view_name}, .is_const = true}),
             1)
        .add(homogeneous_function("left",
                                  "auto",
                                  {FunctionParameter{"size_type const", "count"}},
                                  "return " + view_name + "{" + slice_values("Left(count)") + "};",
                                  {.trailing_return_type = CppType{view_name}, .is_const = true}),
             1)
        .add(homogeneous_function("right",
                                  "auto",
                                  {FunctionParameter{"size_type const", "count"}},
                                  "return " + view_name + "{" + slice_values("Right(count)") + "};",
                                  {.trailing_return_type = CppType{view_name}, .is_const = true}));
    if (has_equivalent_type) {
        std::vector<std::string> values;
        for (auto const& component : layout.components) {
            values.push_back(component + ".GetData()[index]");
        }
        children.new_lines()
            .add(homogeneous_function(
                     "operator[]",
                     "auto",
                     {FunctionParameter{"size_type const", "index"}},
                     "return {" + join(values, ", ") + "};",
                     {.trailing_return_type = CppType{"equivalent_type"}, .is_const = true}),
                 1)
            .add(homogeneous_function(
                "at",
                "auto",
                {FunctionParameter{"size_type const", "index"}},
                "check(index >= 0);\ncheck(index < num());\n"
                "return (*this)[index];",
                {.trailing_return_type = CppType{"equivalent_type"}, .is_const = true},
                std::nullopt,
                {},
                {check_dependency}));
    }
    auto dependencies{std::vector<TypeDependency>{tarray_view, std_remove_const, std_forward}};
    if (has_equivalent_type) {
        dependencies.push_back(check_dependency);
    }
    return Struct{
        .name = view_name,
        .children = children.build(),
        .template_parameters = "typename T",
        .dependencies = std::move(dependencies),
    };
}

namespace {

auto equivalent_type_nodes(HomogeneousLayoutSchema const& layout,
                           std::map<std::string, CppType> const& types) -> Nodes {
    if (!layout.value_types.front().equivalent_type.has_value()) {
        return {};
    }

    auto const trait_name{"T" + layout.name + "EquivalentType"};
    NodeListBuilder result;
    result.add(raw("template <typename T>\nstruct " + trait_name + ";"), 2);
    for (std::size_t index{0}; index < layout.value_types.size(); ++index) {
        auto const& value{layout.value_types[index]};
        auto const value_type{resolve_type(value.type, types)};
        auto const equivalent_type{resolve_type(*value.equivalent_type, types)};
        result.add(Struct{
            .name = trait_name + "<" + value_type.spelling + ">",
            .children = {UsingDeclaration{"type", equivalent_type}},
            .template_parameters = "",
        });
        if (index + 1 < layout.value_types.size()) {
            result.new_lines(2);
        }
    }
    return result.build();
}

} // namespace

auto homogeneous_view_nodes(HomogeneousLayoutSchema const& layout,
                            std::map<std::string, CppType> const& types) -> Nodes {
    auto const has_equivalent_type{layout.value_types.front().equivalent_type.has_value()};
    NodeListBuilder result;
    result.append(equivalent_type_nodes(layout, types));
    if (has_equivalent_type) {
        result.new_lines(2);
    }
    return result.add(homogeneous_view_node(layout, has_equivalent_type)).build();
}

} // namespace codegen::detail
