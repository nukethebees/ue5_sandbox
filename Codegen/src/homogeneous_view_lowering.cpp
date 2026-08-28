#include "homogeneous_internal.h"
#include "lowering_utils.h"

#include <utility>

namespace codegen::detail {
namespace {

TypeDependency const std_forward{"std::forward", "utility", {}};
TypeDependency const std_remove_const{"std::remove_const_t", "type_traits", {}};
TypeDependency const tarray_view{"TArrayView", "Containers/ArrayView.h", {}};

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
                          std::string suffix = {},
                          std::optional<std::string> function_template = std::nullopt,
                          FunctionFormatting formatting = {},
                          std::vector<TypeDependency> dependencies = {}) -> Node {
    return header_function(FunctionSpec{
        .name = std::move(name),
        .return_type = std::move(return_type),
        .parameters = std::move(parameters),
        .body = {raw(std::move(body), std::move(dependencies))},
        .suffix = std::move(suffix),
        .is_inline = true,
        .template_parameters = std::move(function_template),
        .formatting = formatting,
    });
}

} // namespace

auto homogeneous_view_node(HomogeneousLayoutSchema const& layout) -> Node {
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
        .add(UsingDeclaration{"ConstView", CppType{view_name + "<value_type const>"}}, 2);
    for (std::size_t index{0}; index < layout.components.size(); ++index) {
        children.add(Member{CppType{"TArrayView<T>"}, layout.components[index]},
                     index + 1 < layout.components.size() ? 1 : 2);
    }
    children.add(homogeneous_function("get_view",
                                      "auto",
                                      {},
                                      "return View{" + components + "};",
                                      " -> View",
                                      std::nullopt,
                                      compact_function_formatting()),
                 1)
        .add(homogeneous_function("get_view",
                                  "auto",
                                  {FunctionParameter{"size_type const", "offset"},
                                   FunctionParameter{"size_type const", "count"}},
                                  "return get_view().slice(offset, count);",
                                  " -> View"),
             1)
        .add(homogeneous_function("get_view",
                                  "auto",
                                  {},
                                  "return ConstView{" + components + "};",
                                  " const -> ConstView",
                                  std::nullopt,
                                  compact_function_formatting()),
             1)
        .add(homogeneous_function("get_view",
                                  "auto",
                                  {FunctionParameter{"size_type const", "offset"},
                                   FunctionParameter{"size_type const", "count"}},
                                  "return get_view().slice(offset, count);",
                                  " const -> ConstView"),
             1)
        .add(homogeneous_function("get_const_view",
                                  "auto",
                                  {},
                                  "return ConstView{" + components + "};",
                                  " const -> ConstView",
                                  std::nullopt,
                                  compact_function_formatting()),
             1)
        .add(homogeneous_function("get_const_view",
                                  "auto",
                                  {FunctionParameter{"size_type const", "offset"},
                                   FunctionParameter{"size_type const", "count"}},
                                  "return get_const_view().slice(offset, count);",
                                  " const -> ConstView"),
             1)
        .add(homogeneous_function("apply_arrays",
                                  "auto",
                                  {FunctionParameter{"TFunc&&", "func"}},
                                  "return std::forward<TFunc>(func)(" + components + ");",
                                  " -> decltype(auto)",
                                  "typename TFunc",
                                  {},
                                  {std_forward}),
             1)
        .add(homogeneous_function("apply_arrays",
                                  "auto",
                                  {FunctionParameter{"TFunc&&", "func"}},
                                  "return std::forward<TFunc>(func)(" + components + ");",
                                  " const -> decltype(auto)",
                                  "typename TFunc",
                                  {},
                                  {std_forward}),
             1)
        .add(homogeneous_function("num",
                                  "auto",
                                  {},
                                  "return " + layout.components.front() + ".Num();",
                                  " const -> size_type",
                                  std::nullopt,
                                  compact_function_formatting()),
             1)
        .add(homogeneous_function("is_empty",
                                  "auto",
                                  {},
                                  "return num() == 0;",
                                  " const -> bool",
                                  std::nullopt,
                                  compact_function_formatting()),
             1)
        .add(homogeneous_function("slice",
                                  "auto",
                                  {FunctionParameter{"size_type const", "offset"},
                                   FunctionParameter{"size_type const", "count"}},
                                  "return " + view_name + "{" +
                                      slice_values("Slice(offset, count)") + "};",
                                  " const -> " + view_name),
             1)
        .add(homogeneous_function("left",
                                  "auto",
                                  {FunctionParameter{"size_type const", "count"}},
                                  "return " + view_name + "{" + slice_values("Left(count)") +
                                      "};",
                                  " const -> " + view_name),
             1)
        .add(homogeneous_function(
        "right",
        "auto",
        {FunctionParameter{"size_type const", "count"}},
        "return " + view_name + "{" + slice_values("Right(count)") + "};",
        " const -> " + view_name));
    return Struct{
        .name = view_name,
        .children = children.build(),
        .template_parameters = "typename T",
        .dependencies = {tarray_view, std_remove_const, std_forward},
    };
}

} // namespace codegen::detail
