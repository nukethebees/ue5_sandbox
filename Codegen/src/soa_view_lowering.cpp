#include "lowering_utils.h"
#include "soa_internal.h"

#include <utility>

namespace codegen::detail {
namespace {

TypeDependency const std_forward{"std::forward", "utility", {}};
TypeDependency const array_checks{
    "ml::fatal_if_nums_not_equal", "SandboxCore/array_checks.h", {}};
TypeDependency const container_ops{"ml::num", "SandboxCore/container_ops.h", {}};
TypeDependency const check_dependency{"check", "CoreMinimal.h", {}};

auto view_expression(ResolvedMember const& member, bool const_view) -> std::string {
    if (member.kind == SoaMemberKind::nested) {
        return member.name + (const_view ? ".get_const_view(offset, count)"
                                         : ".get_view(offset, count)");
    }
    auto const& type{const_view ? member.const_view_type : member.view_type};
    return type.spelling + "{" + member.name + "}.Slice(offset, count)";
}

auto equivalent_expression(ResolvedMember const& member, std::string const& index)
    -> std::string {
    if (member.kind == SoaMemberKind::array) {
        return member.name + ".GetData()[" + index + "]";
    }
    return member.name + "[" + index + "]";
}

auto apply_arrays_function(std::vector<ResolvedMember> const& members) -> Node {
    std::vector<std::string> body{"return std::forward<TFunc>(func)("};
    auto const count{members.size()};
    for (std::size_t index{0}; index < count; ++index) {
        body.push_back("    self." + members[index].name + (index + 1 < count ? "," : ""));
    }
    body.emplace_back(");");
    return header_function(FunctionSpec{
        .name = "apply_arrays",
        .return_type = "auto",
        .parameters = {FunctionParameter{"this auto&&", "self"},
                       FunctionParameter{"TFunc&&", "func"}},
        .body = {raw(join_lines(body), {std_forward})},
        .suffix = " -> decltype(auto)",
        .is_inline = true,
        .template_parameters = "typename TFunc",
    });
}

auto apply_array_pairs_function(std::vector<ResolvedMember> const& members) -> Node {
    std::vector<std::string> body{"return std::forward<TFunc>(func)("};
    auto const count{members.size()};
    for (std::size_t index{0}; index < count; ++index) {
        auto const& name{members[index].name};
        body.push_back("    self." + name + ", other." + name +
                       (index + 1 < count ? "," : ""));
    }
    body.emplace_back(");");
    return header_function(FunctionSpec{
        .name = "apply_array_pairs",
        .return_type = "auto",
        .parameters = {FunctionParameter{"this Self&&", "self"},
                       FunctionParameter{"Other&&", "other"},
                       FunctionParameter{"TFunc&&", "func"}},
        .body = {raw(join_lines(body), {std_forward})},
        .suffix = "\n    -> decltype(auto)",
        .is_inline = true,
        .template_parameters = "typename Self, typename Other, typename TFunc",
    });
}

auto declaration_nodes(std::vector<FunctionSpec> const& specs) -> Nodes {
    NodeListBuilder result;
    auto const count{specs.size()};
    for (std::size_t index{0}; index < count; ++index) {
        result.add(declaration(specs[index]));
        if (index + 1 < count) {
            result.new_lines();
        }
    }
    return result.build();
}

auto view_struct(std::string name,
                 std::string const& view_name,
                 std::string const& const_view_name,
                 std::vector<ResolvedMember> const& members,
                 std::optional<TypeRef> const& equivalent,
                 std::map<std::string, CppType> const& types,
                 std::optional<std::string> const& export_specifier,
                 bool const_only) -> Struct {
    NodeListBuilder nodes;
    nodes.add(UsingDeclaration{"View", CppType{view_name}}, 1)
        .add(UsingDeclaration{"ConstView", CppType{const_view_name}}, 2);
    if (equivalent.has_value()) {
        nodes.append(soa_equivalent_nodes(*equivalent, members, types)).new_lines(2);
    }
    nodes.add(apply_arrays_function(members), 2)
        .append(declaration_nodes(soa_view_specs(members, const_only)))
        .new_lines(2);
    for (std::size_t index{0}; index < members.size(); ++index) {
        nodes.add(Member{const_only ? members[index].const_view_type : members[index].view_type,
                         members[index].name});
        if (index + 1 < members.size()) {
            nodes.new_lines();
        }
    }
    return Struct{
        .name = std::move(name),
        .children = nodes.build(),
        .export_specifier = export_specifier,
    };
}

} // namespace

auto soa_equivalent_nodes(TypeRef const& equivalent_reference,
                          std::vector<ResolvedMember> const& members,
                          std::map<std::string, CppType> const& types) -> Nodes {
    auto const equivalent{resolve_type(equivalent_reference, types)};
    std::vector<std::string> values;
    for (auto const& member : members) {
        values.push_back(equivalent_expression(member, "index"));
    }
    NodeListBuilder result;
    result.add(UsingDeclaration{"equivalent_type", equivalent}, 2)
        .add(header_function(FunctionSpec{
            .name = "operator[]",
            .return_type = "auto",
            .parameters = {FunctionParameter{"int32 const", "index"}},
            .body = {ReturnStatement{"{" + join(values, ", ") + "}"}},
            .suffix = " const -> " + equivalent.spelling,
            .is_inline = true,
        }), 1)
        .add(header_function(FunctionSpec{
            .name = "at",
            .return_type = "auto",
            .parameters = {FunctionParameter{"int32 const", "index"}},
            .body = {
                ExpressionStatement{"validate_array_sizes()"},
                ExpressionStatement{"check(index >= 0)", {check_dependency}},
                ExpressionStatement{"check(index < num())"},
                ReturnStatement{"(*this)[index]"},
            },
            .suffix = " const -> " + equivalent.spelling,
            .is_inline = true,
        }));
    return result.build();
}

auto soa_view_specs(std::vector<ResolvedMember> const& members,
                    bool const_only) -> std::vector<FunctionSpec> {
    std::vector<FunctionSpec> result;
    auto add = [&](std::string name,
                   CppType return_type,
                   std::vector<FunctionParameter> parameters,
                   std::string body,
                   std::string suffix,
                   std::vector<TypeDependency> dependencies = {}) {
        result.push_back(FunctionSpec{
            .name = std::move(name),
            .return_type = std::move(return_type),
            .parameters = std::move(parameters),
            .body = {raw(std::move(body), std::move(dependencies))},
            .suffix = std::move(suffix),
        });
    };

    if (!const_only) {
        add("get_view", "auto", {}, "return get_view(0, num());", " -> View");
        std::vector<std::string> values;
        for (auto const& member : members) {
            values.push_back("    " + view_expression(member, false) + ",");
        }
        add("get_view",
            "auto",
            {FunctionParameter{"int32 const", "offset"},
             FunctionParameter{"int32 const", "count"}},
            "return View{\n" + join_lines(values) + "\n};",
            " -> View");
    }

    add("get_view", "auto", {}, "return get_view(0, num());", " const -> ConstView");
    std::vector<std::string> const_values;
    for (auto const& member : members) {
        const_values.push_back("    " + view_expression(member, true) + ",");
    }
    add("get_view",
        "auto",
        {FunctionParameter{"int32 const", "offset"},
         FunctionParameter{"int32 const", "count"}},
        "return ConstView{\n" + join_lines(const_values) + "\n};",
        " const -> ConstView");
    add("get_const_view", "auto", {}, "return get_const_view(0, num());", " const -> ConstView");
    add("get_const_view",
        "auto",
        {FunctionParameter{"int32 const", "offset"},
         FunctionParameter{"int32 const", "count"}},
        "return ConstView{\n" + join_lines(const_values) + "\n};",
        " const -> ConstView");
    add("num",
        "auto",
        {},
        "return ml::num(" + members.front().name + ");",
        " const noexcept -> int32",
        {container_ops});
    add("is_empty", "auto", {}, "return num() == 0;", " const noexcept -> bool");
    std::vector<std::string> nums;
    for (auto const& member : members) {
        nums.push_back("    ml::num(" + member.name + "),");
    }
    add("validate_array_sizes",
        "void",
        {},
        "ml::fatal_if_nums_not_equal({\n" + join_lines(nums) + "\n});",
        " const",
        {array_checks, container_ops});
    if (!const_only) {
        add("slice",
            "auto",
            {FunctionParameter{"int32 const", "offset"},
             FunctionParameter{"int32 const", "count"}},
            "return get_view(offset, count);",
            " -> View");
        add("left",
            "auto",
            {FunctionParameter{"int32 const", "count"}},
            "return slice(0, count);",
            " -> View");
        add("right",
            "auto",
            {FunctionParameter{"int32 const", "count"}},
            "return slice(num() - count, count);",
            " -> View");
    }
    add("slice",
        "auto",
        {FunctionParameter{"int32 const", "offset"},
         FunctionParameter{"int32 const", "count"}},
        "return get_view(offset, count);",
        " const -> ConstView");
    add("left",
        "auto",
        {FunctionParameter{"int32 const", "count"}},
        "return slice(0, count);",
        " const -> ConstView");
    add("right",
        "auto",
        {FunctionParameter{"int32 const", "count"}},
        "return slice(num() - count, count);",
        " const -> ConstView");
    return result;
}

auto soa_view_struct_nodes(SoaSchema const& schema,
                           std::vector<ResolvedMember> const& members,
                           std::map<std::string, CppType> const& types,
                           std::string const& view_name,
                           std::string const& const_view_name) -> Nodes {
    NodeListBuilder result;
    return result.add(ForwardDeclaration{view_name}, 1)
        .add(ForwardDeclaration{const_view_name}, 2)
        .add(view_struct(const_view_name,
                         view_name,
                         const_view_name,
                         members,
                         schema.equivalent_type,
                         types,
                         schema.export_specifier,
                         true),
             2)
        .add(view_struct(view_name,
                         view_name,
                         const_view_name,
                         members,
                         schema.equivalent_type,
                         types,
                         schema.export_specifier,
                         false),
             2)
        .build();
}

auto soa_storage_view_nodes(std::vector<ResolvedMember> const& members) -> Nodes {
    NodeListBuilder result;
    return result.add(apply_arrays_function(members), 2)
        .add(apply_array_pairs_function(members), 2)
        .append(declaration_nodes(soa_view_specs(members, false)))
        .build();
}

} // namespace codegen::detail
