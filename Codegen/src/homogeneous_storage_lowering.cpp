#include "homogeneous_internal.h"
#include "lowering_utils.h"

#include <utility>

namespace codegen::detail {
namespace {

TypeDependency const std_forward{"std::forward", "utility", {}};
TypeDependency const tarray{"TArray", "Containers/Array.h", {}};
TypeDependency const tarray_view{"TArrayView", "Containers/ArrayView.h", {}};
TypeDependency const allow_shrinking{"EAllowShrinking", "Containers/AllowShrinking.h", {}};
TypeDependency const fill_indices{"ml::fill_indices", "SandboxCore/array_utils.h", {}};
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
                          Nodes body,
                          std::string suffix = {},
                          std::optional<std::string> function_template = std::nullopt,
                          FunctionFormatting formatting = {}) -> Node {
    return header_function(FunctionSpec{
        .name = std::move(name),
        .return_type = std::move(return_type),
        .parameters = std::move(parameters),
        .body = std::move(body),
        .suffix = std::move(suffix),
        .is_inline = true,
        .template_parameters = std::move(function_template),
        .formatting = formatting,
    });
}

auto homogeneous_function(std::string name,
                          CppType return_type,
                          std::vector<FunctionParameter> parameters,
                          std::string body,
                          std::string suffix = {},
                          std::optional<std::string> function_template = std::nullopt,
                          FunctionFormatting formatting = {},
                          std::vector<TypeDependency> dependencies = {}) -> Node {
    return homogeneous_function(std::move(name),
                                std::move(return_type),
                                std::move(parameters),
                                {raw(std::move(body), std::move(dependencies))},
                                std::move(suffix),
                                std::move(function_template),
                                formatting);
}

auto same_line_template_formatting() -> FunctionFormatting {
    return FunctionFormatting{
        .template_placement = FunctionFormatting::TemplatePlacement::same_line,
    };
}

auto substitute_component(std::string value, std::string const& component) -> std::string {
    auto marker{value.find("{}")};
    while (marker != std::string::npos) {
        value.replace(marker, 2, component);
        marker = value.find("{}", marker + component.size());
    }
    return value;
}

auto component_lines(HomogeneousLayoutSchema const& layout,
                     std::string const& line_template) -> std::string {
    std::vector<std::string> lines;
    for (auto const& component : layout.components) {
        lines.push_back(substitute_component(line_template, component));
    }
    return join_lines(lines);
}

auto component_statements(HomogeneousLayoutSchema const& layout,
                          std::string const& expression_template) -> Nodes {
    NodeListBuilder result;
    for (auto const& component : layout.components) {
        result.add(ExpressionStatement{substitute_component(expression_template, component)});
    }
    return result.build();
}

auto component_assignments(HomogeneousLayoutSchema const& layout,
                           std::string const& target_template,
                           std::string const& value_template) -> Nodes {
    NodeListBuilder result;
    for (auto const& component : layout.components) {
        result.add(AssignmentStatement{substitute_component(target_template, component),
                                       substitute_component(value_template, component)});
    }
    return result.build();
}

void add_compact_function(
    NodeListBuilder& nodes,
    std::string name,
    CppType return_type,
    std::vector<FunctionParameter> parameters,
    std::string body,
    std::string suffix = {},
    std::optional<std::string> function_template = std::nullopt) {
    nodes.add(homogeneous_function(std::move(name),
                                   std::move(return_type),
                                   std::move(parameters),
                                   std::move(body),
                                   std::move(suffix),
                                   std::move(function_template),
                                   compact_function_formatting()),
              1);
}

auto homogeneous_pointer_struct(HomogeneousLayoutSchema const& layout,
                                std::string name,
                                std::string const& pointer_type) -> Node {
    NodeListBuilder members;
    for (std::size_t index{0}; index < layout.components.size(); ++index) {
        members.add(Member{CppType{pointer_type}, layout.components[index]});
        if (index + 1 < layout.components.size()) {
            members.new_lines();
        }
    }
    return Struct{.name = std::move(name), .children = members.build()};
}

auto homogeneous_storage_prelude_nodes(HomogeneousLayoutSchema const& layout,
                                       CppType const& value_type,
                                       std::string const& view_name) -> Nodes {
    NodeListBuilder result;
    result.add(UsingDeclaration{"value_type", value_type}, 1)
        .add(UsingDeclaration{"size_type", CppType{"TArray<value_type>::SizeType"}}, 1)
        .add(UsingDeclaration{"View", CppType{view_name + "<value_type>"}}, 1)
        .add(UsingDeclaration{"ConstView", CppType{view_name + "<value_type const>"}}, 2)
        .add(homogeneous_pointer_struct(layout, "Data", "value_type*"), 2)
        .add(homogeneous_pointer_struct(layout, "ConstData", "value_type const*"), 2);
    for (std::size_t index{0}; index < layout.components.size(); ++index) {
        result.add(Member{CppType{"TArray<value_type>"}, layout.components[index]},
                   index + 1 < layout.components.size() ? 1 : 2);
    }
    return result.build();
}

auto homogeneous_storage_access_nodes(HomogeneousLayoutSchema const& layout) -> Nodes {
    std::vector<std::string> pointers;
    for (auto const& component : layout.components) {
        pointers.push_back(component + ".GetData()");
    }
    auto const components{join(layout.components, ", ")};
    std::vector<std::string> validation;
    for (std::size_t index{1}; index < layout.components.size(); ++index) {
        validation.push_back("check(" + layout.components[index] +
                             ".Num() == " + layout.components.front() + ".Num());");
    }

    NodeListBuilder result;
    add_compact_function(
        result, "get_data", "auto", {}, "return Data{" + join(pointers, ", ") + "};", " -> Data");
    add_compact_function(result,
                         "get_data",
                         "auto",
                         {},
                         "return ConstData{" + join(pointers, ", ") + "};",
                         " const -> ConstData");
    add_compact_function(
        result, "get_view", "auto", {}, "return View{" + components + "};", " -> View");
    add_compact_function(result,
                         "get_view",
                         "auto",
                         {FunctionParameter{"size_type const", "offset"},
                          FunctionParameter{"size_type const", "count"}},
                         "return get_view().slice(offset, count);",
                         " -> View");
    add_compact_function(result,
                         "get_view",
                         "auto",
                         {},
                         "return ConstView{" + components + "};",
                         " const -> ConstView");
    add_compact_function(result,
                         "get_view",
                         "auto",
                         {FunctionParameter{"size_type const", "offset"},
                          FunctionParameter{"size_type const", "count"}},
                         "return get_view().slice(offset, count);",
                         " const -> ConstView");
    add_compact_function(result,
                         "get_const_view",
                         "auto",
                         {},
                         "return ConstView{" + components + "};",
                         " const -> ConstView");
    add_compact_function(result,
                         "get_const_view",
                         "auto",
                         {FunctionParameter{"size_type const", "offset"},
                          FunctionParameter{"size_type const", "count"}},
                         "return get_const_view().slice(offset, count);",
                         " const -> ConstView");
    add_compact_function(result,
                         "apply_arrays",
                         "auto",
                         {FunctionParameter{"TFunc&&", "func"}},
                         "return std::forward<TFunc>(func)(" + components + ");",
                         " -> decltype(auto)",
                         "typename TFunc");
    add_compact_function(result,
                         "apply_arrays",
                         "auto",
                         {FunctionParameter{"TFunc&&", "func"}},
                         "return std::forward<TFunc>(func)(" + components + ");",
                         " const -> decltype(auto)",
                         "typename TFunc");
    add_compact_function(result,
                         "num",
                         "auto",
                         {},
                         "return " + layout.components.front() + ".Num();",
                         " const -> size_type");
    result.add(homogeneous_function("validate_array_sizes",
                                    "void",
                                    {},
                                    join_lines(validation),
                                    " const",
                                    std::nullopt,
                                    {},
                                    {check_dependency}),
               1);
    add_compact_function(
        result, "is_empty", "auto", {}, "return num() == 0;", " const -> bool");
    return result.build();
}

auto homogeneous_storage_copy_nodes(HomogeneousLayoutSchema const& layout) -> Nodes {
    NodeListBuilder result;
    result.add(homogeneous_function("copy_element",
                                  "auto",
                                  {FunctionParameter{"size_type const", "dst_i"},
                                   FunctionParameter{"Other const&", "src"},
                                   FunctionParameter{"size_type const", "src_i"}},
                                  component_assignments(
                                      layout, "{}[dst_i]", "src.{}[src_i]"),
                                  " -> void",
                                  "typename Other",
                                  same_line_template_formatting()),
               1);
    result.add(homogeneous_function(
                 "copy_elements",
                 "auto",
                 {FunctionParameter{"size_type const", "dst_i"},
                  FunctionParameter{"Other const&", "src"},
                  FunctionParameter{"size_type const", "src_i"},
                  FunctionParameter{"size_type const", "count"}},
                 "for (auto i{0}; i < count; ++i) {\n" +
                     component_lines(layout, "    {}[dst_i + i] = src.{}[src_i + i];") +
                     "\n}",
                 " -> void",
                 "typename Other",
                 same_line_template_formatting()),
               1);
    result.add(homogeneous_function("copy_to_tail",
                                  "auto",
                                  {FunctionParameter{"Other const&", "src"}},
                                  Nodes{
                                      VariableDeclarationStatement{
                                          "auto const", "count", "src.num()"},
                                      ExpressionStatement{
                                          "check(num() >= count)", {check_dependency}},
                                      ExpressionStatement{
                                          "copy_elements(num() - count, src, 0, count)"},
                                  },
                                  " -> void",
                                  "typename Other",
                                  same_line_template_formatting()),
               1);
    result.add(homogeneous_function("append_from",
                                  "void",
                                  {FunctionParameter{"Other const&", "other"}},
                                  component_statements(layout, "{}.Append(other.{})"),
                                  {},
                                  "typename Other",
                                  same_line_template_formatting()),
               1);
    return result.build();
}

auto homogeneous_storage_sort_nodes() -> Nodes {
    NodeListBuilder result;
    result.add(declaration(
                 FunctionSpec{.name = "apply_permutation",
                              .return_type = "void",
                              .parameters = {FunctionParameter{"TArrayView<int32>", "indices"}}}),
               1);
    auto sort_body = [](std::string sort_expression) {
        NodeListBuilder body;
        return body.add(ExpressionStatement{"validate_array_sizes()"})
            .add(VariableDeclarationStatement{"auto const", "n", "num()"})
            .add(ExpressionStatement{"check(scratch_indices.Num() == n)", {check_dependency}})
            .add(ExpressionStatement{"ml::fill_indices(scratch_indices)", {fill_indices}})
            .add(raw(std::move(sort_expression)))
            .add(ExpressionStatement{"apply_permutation(scratch_indices)"})
            .build();
    };
    result.add(homogeneous_function(
                 "sort",
                 "void",
                 {FunctionParameter{"Compare&&", "compare"},
                  FunctionParameter{"TArrayView<int32>", "scratch_indices"}},
                 sort_body(
                     "scratch_indices.Sort([this, &compare](int32 const lhs, int32 const rhs) { "
                     "return compare(*this, lhs, rhs); });"),
                 {},
                 "typename Compare",
                 same_line_template_formatting()),
               1);
    result.add(homogeneous_function(
                 "sort",
                 "void",
                 {FunctionParameter{"TArrayView<int32>", "scratch_indices"}},
                 sort_body("scratch_indices.Sort([this](int32 const lhs, int32 const rhs) { return "
                           "Compare(*this, lhs, rhs); });"),
                 {},
                 "auto Compare",
                 same_line_template_formatting()),
               1);
    return result.build();
}

auto homogeneous_storage_array_operation_nodes(HomogeneousLayoutSchema const& layout) -> Nodes {
    NodeListBuilder result;
    auto add_each = [&](std::string name,
                        std::vector<FunctionParameter> parameters,
                        std::string expression) {
        result.add(homogeneous_function(std::move(name),
                                      "auto",
                                      std::move(parameters),
                                      component_statements(layout, expression),
                                      " -> void"),
                   1);
    };
    add_each("reset", {}, "{}.Reset()");
    add_each("empty", {}, "{}.Empty()");
    add_each("reserve", {FunctionParameter{"size_type const", "count"}}, "{}.Reserve(count)");
    add_each("set_num",
             {FunctionParameter{"size_type const", "count"},
              FunctionParameter{"EAllowShrinking const", "allow_shrinking"}},
             "{}.SetNum(count, allow_shrinking)");
    add_each("set_num_uninitialised",
             {FunctionParameter{"size_type const", "count"}},
             "{}.SetNumUninitialized(count)");
    add_each("add_uninitialised",
             {FunctionParameter{"size_type const", "count"}},
             "{}.AddUninitialized(count)");
    add_each("remove_at_swap",
             {FunctionParameter{"size_type const", "index"},
              FunctionParameter{"size_type const", "count"},
              FunctionParameter{"EAllowShrinking const", "allow_shrinking"}},
             "{}.RemoveAtSwap(index, count, allow_shrinking)");
    add_each(
        "add_zeroed", {FunctionParameter{"size_type const", "count"}}, "{}.AddZeroed(count)");
    result.add(homogeneous_function("add_defaulted",
                                    "auto",
                                    {FunctionParameter{"size_type const", "count"}},
                                    component_statements(layout, "{}.AddDefaulted(count)"),
                                    " -> void"));
    return result.build();
}

} // namespace

auto homogeneous_storage_node(HomogeneousLayoutSchema const& layout,
                              HomogeneousValueSchema const& value,
                              std::map<std::string, CppType> const& types) -> Node {
    auto const value_type{resolve_type(value.type, types)};
    auto const view_name{"T" + layout.name + "View"};
    auto const storage_name{"F" + layout.name + value.suffix};
    NodeListBuilder children;
    children.append(homogeneous_storage_prelude_nodes(layout, value_type, view_name))
        .append(homogeneous_storage_access_nodes(layout))
        .append(homogeneous_storage_copy_nodes(layout))
        .append(homogeneous_storage_sort_nodes())
        .append(homogeneous_storage_array_operation_nodes(layout));

    std::vector<TypeDependency> dependencies{
        tarray, tarray_view, allow_shrinking, check_dependency, fill_indices, std_forward};
    dependencies.insert(
        dependencies.end(), value_type.dependencies.begin(), value_type.dependencies.end());
    return Struct{
        .name = storage_name,
        .children = children.build(),
        .export_specifier = layout.export_specifier,
        .dependencies = std::move(dependencies),
    };
}

} // namespace codegen::detail
