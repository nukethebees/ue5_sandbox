#include "lowering.h"
#include "lowering_utils.h"

#include <utility>

namespace codegen::detail {
namespace {

TypeDependency const std_forward{"std::forward", "utility", {}};
TypeDependency const std_remove_const{"std::remove_const_t", "type_traits", {}};
TypeDependency const tarray{"TArray", "Containers/Array.h", {}};
TypeDependency const tarray_view{"TArrayView", "Containers/ArrayView.h", {}};
TypeDependency const allow_shrinking{"EAllowShrinking", "Containers/AllowShrinking.h", {}};
TypeDependency const soa_permutation{
    "ml::apply_permutation", "SandboxCore/soa_permutation.h", {}};
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
    Nodes children{
        UsingDeclaration{"size_type", CppType{"TArrayView<T>::SizeType"}},
        lines(1),
        UsingDeclaration{"value_type", CppType{"std::remove_const_t<T>"}},
        lines(1),
        UsingDeclaration{"View", CppType{view_name + "<T>"}},
        lines(1),
        UsingDeclaration{"ConstView", CppType{view_name + "<value_type const>"}},
        lines(2),
    };
    for (std::size_t index{0}; index < layout.components.size(); ++index) {
        children.push_back(Member{CppType{"TArrayView<T>"}, layout.components[index]});
        children.push_back(lines(index + 1 < layout.components.size() ? 1 : 2));
    }
    auto add = [&](Node node, int newlines = 1) {
        children.push_back(std::move(node));
        children.push_back(lines(newlines));
    };
    add(homogeneous_function("get_view",
                             "auto",
                             {},
                             "return View{" + components + "};",
                             " -> View",
                             std::nullopt,
                             compact_function_formatting()));
    add(homogeneous_function("get_view",
                             "auto",
                             {FunctionParameter{"size_type const", "offset"},
                              FunctionParameter{"size_type const", "count"}},
                             "return get_view().slice(offset, count);",
                             " -> View"));
    add(homogeneous_function("get_view",
                             "auto",
                             {},
                             "return ConstView{" + components + "};",
                             " const -> ConstView",
                             std::nullopt,
                             compact_function_formatting()));
    add(homogeneous_function("get_view",
                             "auto",
                             {FunctionParameter{"size_type const", "offset"},
                              FunctionParameter{"size_type const", "count"}},
                             "return get_view().slice(offset, count);",
                             " const -> ConstView"));
    add(homogeneous_function("get_const_view",
                             "auto",
                             {},
                             "return ConstView{" + components + "};",
                             " const -> ConstView",
                             std::nullopt,
                             compact_function_formatting()));
    add(homogeneous_function("get_const_view",
                             "auto",
                             {FunctionParameter{"size_type const", "offset"},
                              FunctionParameter{"size_type const", "count"}},
                             "return get_const_view().slice(offset, count);",
                             " const -> ConstView"));
    add(homogeneous_function("apply_arrays",
                             "auto",
                             {FunctionParameter{"TFunc&&", "func"}},
                             "return std::forward<TFunc>(func)(" + components + ");",
                             " -> decltype(auto)",
                             "typename TFunc",
                             {},
                             {std_forward}));
    add(homogeneous_function("apply_arrays",
                             "auto",
                             {FunctionParameter{"TFunc&&", "func"}},
                             "return std::forward<TFunc>(func)(" + components + ");",
                             " const -> decltype(auto)",
                             "typename TFunc",
                             {},
                             {std_forward}));
    add(homogeneous_function("num",
                             "auto",
                             {},
                             "return " + layout.components.front() + ".Num();",
                             " const -> size_type",
                             std::nullopt,
                             compact_function_formatting()));
    add(homogeneous_function("is_empty",
                             "auto",
                             {},
                             "return num() == 0;",
                             " const -> bool",
                             std::nullopt,
                             compact_function_formatting()));
    add(homogeneous_function("slice",
                             "auto",
                             {FunctionParameter{"size_type const", "offset"},
                              FunctionParameter{"size_type const", "count"}},
                             "return " + view_name + "{" +
                                 slice_values("Slice(offset, count)") + "};",
                             " const -> " + view_name));
    add(homogeneous_function("left",
                             "auto",
                             {FunctionParameter{"size_type const", "count"}},
                             "return " + view_name + "{" + slice_values("Left(count)") +
                                 "};",
                             " const -> " + view_name));
    children.push_back(homogeneous_function(
        "right",
        "auto",
        {FunctionParameter{"size_type const", "count"}},
        "return " + view_name + "{" + slice_values("Right(count)") + "};",
        " const -> " + view_name));
    return Struct{
        .name = view_name,
        .children = std::move(children),
        .template_parameters = "typename T",
        .dependencies = {tarray_view, std_remove_const, std_forward},
    };
}

auto same_line_template_formatting() -> FunctionFormatting {
    return FunctionFormatting{
        .template_placement = FunctionFormatting::TemplatePlacement::same_line,
    };
}

auto component_statements(HomogeneousLayoutSchema const& layout,
                          std::string const& expression) -> std::string {
    std::vector<std::string> result;
    for (auto const& component : layout.components) {
        auto line{expression};
        auto marker{line.find("{}")};
        while (marker != std::string::npos) {
            line.replace(marker, 2, component);
            marker = line.find("{}", marker + component.size());
        }
        result.push_back(std::move(line));
    }
    return join_lines(result);
}

void append_nodes(Nodes& destination, Nodes source) {
    for (auto& node : source) {
        destination.push_back(std::move(node));
    }
}

void add_node(Nodes& nodes, Node node, int newlines = 1) {
    nodes.push_back(std::move(node));
    nodes.push_back(lines(newlines));
}

void add_compact_function(
    Nodes& nodes,
    std::string name,
    CppType return_type,
    std::vector<FunctionParameter> parameters,
    std::string body,
    std::string suffix = {},
    std::optional<std::string> function_template = std::nullopt) {
    add_node(nodes,
             homogeneous_function(std::move(name),
                                  std::move(return_type),
                                  std::move(parameters),
                                  std::move(body),
                                  std::move(suffix),
                                  std::move(function_template),
                                  compact_function_formatting()));
}

auto homogeneous_pointer_struct(HomogeneousLayoutSchema const& layout,
                                std::string name,
                                std::string const& pointer_type) -> Node {
    Nodes members;
    for (std::size_t index{0}; index < layout.components.size(); ++index) {
        members.push_back(Member{CppType{pointer_type}, layout.components[index]});
        if (index + 1 < layout.components.size()) {
            members.push_back(lines(1));
        }
    }
    return Struct{.name = std::move(name), .children = std::move(members)};
}

auto homogeneous_storage_prelude_nodes(HomogeneousLayoutSchema const& layout,
                                       CppType const& value_type,
                                       std::string const& view_name) -> Nodes {
    Nodes result{
        UsingDeclaration{"value_type", value_type},
        lines(1),
        UsingDeclaration{"size_type", CppType{"TArray<value_type>::SizeType"}},
        lines(1),
        UsingDeclaration{"View", CppType{view_name + "<value_type>"}},
        lines(1),
        UsingDeclaration{"ConstView", CppType{view_name + "<value_type const>"}},
        lines(2),
        homogeneous_pointer_struct(layout, "Data", "value_type*"),
        lines(2),
        homogeneous_pointer_struct(layout, "ConstData", "value_type const*"),
        lines(2),
    };
    for (std::size_t index{0}; index < layout.components.size(); ++index) {
        result.push_back(Member{CppType{"TArray<value_type>"}, layout.components[index]});
        result.push_back(lines(index + 1 < layout.components.size() ? 1 : 2));
    }
    return result;
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

    Nodes result;
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
    add_node(result,
             homogeneous_function("validate_array_sizes",
                                  "void",
                                  {},
                                  join_lines(validation),
                                  " const",
                                  std::nullopt,
                                  {},
                                  {check_dependency}));
    add_compact_function(
        result, "is_empty", "auto", {}, "return num() == 0;", " const -> bool");
    return result;
}

auto homogeneous_storage_copy_nodes(HomogeneousLayoutSchema const& layout) -> Nodes {
    Nodes result;
    add_node(result,
             homogeneous_function("copy_element",
                                  "auto",
                                  {FunctionParameter{"size_type const", "dst_i"},
                                   FunctionParameter{"Other const&", "src"},
                                   FunctionParameter{"size_type const", "src_i"}},
                                  component_statements(layout, "{}[dst_i] = src.{}[src_i];"),
                                  " -> void",
                                  "typename Other",
                                  same_line_template_formatting()));
    add_node(result,
             homogeneous_function(
                 "copy_elements",
                 "auto",
                 {FunctionParameter{"size_type const", "dst_i"},
                  FunctionParameter{"Other const&", "src"},
                  FunctionParameter{"size_type const", "src_i"},
                  FunctionParameter{"size_type const", "count"}},
                 "for (auto i{0}; i < count; ++i) {\n" +
                     component_statements(layout, "    {}[dst_i + i] = src.{}[src_i + i];") +
                     "\n}",
                 " -> void",
                 "typename Other",
                 same_line_template_formatting()));
    add_node(result,
             homogeneous_function("copy_to_tail",
                                  "auto",
                                  {FunctionParameter{"Other const&", "src"}},
                                  "auto const count{src.num()};\ncheck(num() >= "
                                  "count);\ncopy_elements(num() - count, src, 0, count);",
                                  " -> void",
                                  "typename Other",
                                  same_line_template_formatting(),
                                  {check_dependency}));
    add_node(result,
             homogeneous_function("append_from",
                                  "void",
                                  {FunctionParameter{"Other const&", "other"}},
                                  component_statements(layout, "{}.Append(other.{});"),
                                  {},
                                  "typename Other",
                                  same_line_template_formatting()));
    return result;
}

auto homogeneous_storage_sort_nodes() -> Nodes {
    Nodes result;
    add_node(result,
             declaration(
                 FunctionSpec{.name = "apply_permutation",
                              .return_type = "void",
                              .parameters = {FunctionParameter{"TArrayView<int32>", "indices"}}}));
    std::string const sort_common{
        "validate_array_sizes();\nauto const n{num()};\ncheck(scratch_indices.Num() == "
        "n);\nml::fill_indices(scratch_indices);\n"};
    add_node(result,
             homogeneous_function(
                 "sort",
                 "void",
                 {FunctionParameter{"Compare&&", "compare"},
                  FunctionParameter{"TArrayView<int32>", "scratch_indices"}},
                 sort_common +
                     "scratch_indices.Sort([this, &compare](int32 const lhs, int32 const rhs) { "
                     "return compare(*this, lhs, rhs); });\napply_permutation(scratch_indices);",
                 {},
                 "typename Compare",
                 same_line_template_formatting(),
                 {check_dependency, fill_indices}));
    add_node(result,
             homogeneous_function(
                 "sort",
                 "void",
                 {FunctionParameter{"TArrayView<int32>", "scratch_indices"}},
                 sort_common +
                     "scratch_indices.Sort([this](int32 const lhs, int32 const rhs) { return "
                     "Compare(*this, lhs, rhs); });\napply_permutation(scratch_indices);",
                 {},
                 "auto Compare",
                 same_line_template_formatting(),
                 {check_dependency, fill_indices}));
    return result;
}

auto homogeneous_storage_array_operation_nodes(HomogeneousLayoutSchema const& layout) -> Nodes {
    Nodes result;
    auto add_each = [&](std::string name,
                        std::vector<FunctionParameter> parameters,
                        std::string expression) {
        add_node(result,
                 homogeneous_function(std::move(name),
                                      "auto",
                                      std::move(parameters),
                                      component_statements(layout, expression),
                                      " -> void"));
    };
    add_each("reset", {}, "{}.Reset();");
    add_each("empty", {}, "{}.Empty();");
    add_each("reserve", {FunctionParameter{"size_type const", "count"}}, "{}.Reserve(count);");
    add_each("set_num",
             {FunctionParameter{"size_type const", "count"},
              FunctionParameter{"EAllowShrinking const", "allow_shrinking"}},
             "{}.SetNum(count, allow_shrinking);");
    add_each("set_num_uninitialised",
             {FunctionParameter{"size_type const", "count"}},
             "{}.SetNumUninitialized(count);");
    add_each("add_uninitialised",
             {FunctionParameter{"size_type const", "count"}},
             "{}.AddUninitialized(count);");
    add_each("remove_at_swap",
             {FunctionParameter{"size_type const", "index"},
              FunctionParameter{"size_type const", "count"},
              FunctionParameter{"EAllowShrinking const", "allow_shrinking"}},
             "{}.RemoveAtSwap(index, count, allow_shrinking);");
    add_each(
        "add_zeroed", {FunctionParameter{"size_type const", "count"}}, "{}.AddZeroed(count);");
    result.push_back(homogeneous_function("add_defaulted",
                                          "auto",
                                          {FunctionParameter{"size_type const", "count"}},
                                          component_statements(layout, "{}.AddDefaulted(count);"),
                                          " -> void"));
    return result;
}

auto homogeneous_storage_node(HomogeneousLayoutSchema const& layout,
                              HomogeneousValueSchema const& value,
                              std::map<std::string, CppType> const& types) -> Node {
    auto const value_type{resolve_type(value.type, types)};
    auto const view_name{"T" + layout.name + "View"};
    auto const storage_name{"F" + layout.name + value.suffix};
    Nodes children;
    append_nodes(children, homogeneous_storage_prelude_nodes(layout, value_type, view_name));
    append_nodes(children, homogeneous_storage_access_nodes(layout));
    append_nodes(children, homogeneous_storage_copy_nodes(layout));
    append_nodes(children, homogeneous_storage_sort_nodes());
    append_nodes(children, homogeneous_storage_array_operation_nodes(layout));

    std::vector<TypeDependency> dependencies{
        tarray, tarray_view, allow_shrinking, check_dependency, fill_indices, std_forward};
    dependencies.insert(
        dependencies.end(), value_type.dependencies.begin(), value_type.dependencies.end());
    return Struct{
        .name = storage_name,
        .children = std::move(children),
        .export_specifier = layout.export_specifier,
        .dependencies = std::move(dependencies),
    };
}

auto lower_homogeneous_module_impl(HomogeneousModuleSchema const& module,
                                   std::map<std::string, CppType> const& types) -> Module {
    Nodes header_nodes{IncludeDependencies{}, lines(2)};
    Nodes source_nodes{Include{source_include(module.settings), false},
                       lines(2),
                       IncludeDependencies{},
                       lines(2)};
    for (std::size_t layout_index{0}; layout_index < module.layouts.size(); ++layout_index) {
        auto const& layout{module.layouts[layout_index]};
        header_nodes.push_back(homogeneous_view_node(layout));
        header_nodes.push_back(lines(2));
        for (std::size_t value_index{0}; value_index < layout.value_types.size(); ++value_index) {
            auto const& value{layout.value_types[value_index]};
            header_nodes.push_back(homogeneous_storage_node(layout, value, types));
            if (value_index + 1 < layout.value_types.size()) {
                header_nodes.push_back(lines(2));
            }

            auto const storage_name{"F" + layout.name + value.suffix};
            std::vector<std::string> body{
                "validate_array_sizes();", "check(indices.Num() == num());"};
            for (auto const& component : layout.components) {
                body.push_back("ml::apply_permutation(" + component + ", indices);");
            }
            source_nodes.push_back(definition(
                FunctionSpec{
                    .name = "apply_permutation",
                    .return_type = "void",
                    .parameters = {FunctionParameter{CppType{"TArrayView<int32>", {tarray_view}},
                                                     "indices"}},
                    .body = {raw(join_lines(body), {check_dependency, soa_permutation})},
                },
                storage_name));
            if (layout_index + 1 < module.layouts.size() ||
                value_index + 1 < layout.value_types.size()) {
                source_nodes.push_back(lines(2));
            }
        }
    }
    return Module{
        .name = module.settings.name,
        .header = CppFile{
            .path = module.settings.header,
            .nodes = std::move(header_nodes),
            .clang_format_off = true,
            .include_order = module.settings.include_order,
        },
        .source = module.settings.source.has_value()
                      ? std::optional<CppFile>{CppFile{
                            .path = *module.settings.source,
                            .nodes = std::move(source_nodes),
                            .pragma_once = false,
                            .clang_format_off = true,
                            .include_order = module.settings.include_order,
                        }}
                      : std::nullopt,
    };
}

} // namespace

auto lower_homogeneous_module(HomogeneousModuleSchema const& module,
                              std::map<std::string, CppType> const& types) -> Module {
    return lower_homogeneous_module_impl(module, types);
}

} // namespace codegen::detail
