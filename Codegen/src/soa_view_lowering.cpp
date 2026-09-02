#include "lowering_utils.h"
#include "soa_internal.h"

#include <utility>

namespace codegen::detail {
namespace {

TypeDependency const array_checks{"ml::fatal_if_nums_not_equal", "SandboxCore/array_checks.h", {}};
TypeDependency const container_ops{"ml::num", "SandboxCore/container_ops.h", {}};
TypeDependency const check_dependency{"check", "CoreMinimal.h", {}};

auto view_expression(ResolvedMember const& member, bool const_view) -> std::string {
    if (member.kind == SoaMemberKind::nested) {
        return member.name +
               (const_view ? ".get_const_view(offset, count)" : ".get_view(offset, count)");
    }
    auto const& type{const_view ? member.const_view_type : member.view_type};
    return type.spelling + "{" + member.name + "}.Slice(offset, count)";
}

auto equivalent_expression(ResolvedMember const& member, std::string const& index) -> std::string {
    if (member.kind == SoaMemberKind::array) {
        return member.name + ".GetData()[" + index + "]";
    }
    return member.name + "[" + index + "]";
}

auto column_names(std::vector<ResolvedMember> const& members) -> std::vector<std::string> {
    std::vector<std::string> result;
    result.reserve(members.size());
    for (auto const& member : members) {
        result.push_back(member.name);
    }
    return result;
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
                 std::vector<FunctionSchema> const& mutable_view_functions,
                 std::map<std::string, CppType> const& types,
                 std::optional<std::string> const& export_specifier,
                 bool const_only) -> Struct {
    NodeListBuilder nodes;
    nodes.add(UsingDeclaration{"View", CppType{view_name}}, 1)
        .add(UsingDeclaration{"ConstView", CppType{const_view_name}}, 2);
    if (equivalent.has_value()) {
        nodes.append(soa_equivalent_nodes(*equivalent, members, types)).new_lines(2);
    }
    if (!const_only) {
        for (auto const& function : mutable_view_functions) {
            nodes.add(header_function(soa_function_spec(function, types)), 2);
        }
    }
    nodes.add(column_apply_arrays_function(column_names(members)), 2)
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
                 .qualifiers = {.trailing_return_type = equivalent, .is_const = true},
                 .is_inline = true,
             }),
             1)
        .add(header_function(FunctionSpec{
            .name = "at",
            .return_type = "auto",
            .parameters = {FunctionParameter{"int32 const", "index"}},
            .body =
                {
                    ExpressionStatement{"validate_array_sizes()"},
                    ExpressionStatement{"check(index >= 0)", {check_dependency}},
                    ExpressionStatement{"check(index < num())"},
                    ReturnStatement{"(*this)[index]"},
                },
            .qualifiers = {.trailing_return_type = equivalent, .is_const = true},
            .is_inline = true,
        }));
    return result.build();
}

auto soa_view_specs(std::vector<ResolvedMember> const& members, bool const_only)
    -> std::vector<FunctionSpec> {
    std::vector<FunctionSpec> result;
    auto add = [&](std::string name,
                   CppType return_type,
                   std::vector<FunctionParameter> parameters,
                   std::string body,
                   CppType trailing_return_type,
                   bool const is_const = false,
                   bool const is_noexcept = false,
                   std::vector<TypeDependency> dependencies = {}) {
        result.push_back(FunctionSpec{
            .name = std::move(name),
            .return_type = std::move(return_type),
            .parameters = std::move(parameters),
            .body = {raw(std::move(body), std::move(dependencies))},
            .qualifiers =
                {
                    .trailing_return_type =
                        trailing_return_type.spelling.empty()
                            ? std::nullopt
                            : std::optional<CppType>{std::move(trailing_return_type)},
                    .is_const = is_const,
                    .is_noexcept = is_noexcept,
                },
        });
    };

    if (!const_only) {
        add("get_view", "auto", {}, "return get_view(0, num());", "View");
        std::vector<std::string> values;
        for (auto const& member : members) {
            values.push_back("    " + view_expression(member, false) + ",");
        }
        add("get_view",
            "auto",
            {FunctionParameter{"int32 const", "offset"}, FunctionParameter{"int32 const", "count"}},
            "return View{\n" + join_lines(values) + "\n};",
            "View");
    }

    add("get_view", "auto", {}, "return get_view(0, num());", "ConstView", true);
    std::vector<std::string> const_values;
    for (auto const& member : members) {
        const_values.push_back("    " + view_expression(member, true) + ",");
    }
    add("get_view",
        "auto",
        {FunctionParameter{"int32 const", "offset"}, FunctionParameter{"int32 const", "count"}},
        "return ConstView{\n" + join_lines(const_values) + "\n};",
        "ConstView",
        true);
    add("get_const_view", "auto", {}, "return get_const_view(0, num());", "ConstView", true);
    add("get_const_view",
        "auto",
        {FunctionParameter{"int32 const", "offset"}, FunctionParameter{"int32 const", "count"}},
        "return ConstView{\n" + join_lines(const_values) + "\n};",
        "ConstView",
        true);
    add("num",
        "auto",
        {},
        "return ml::num(" + members.front().name + ");",
        "int32",
        true,
        true,
        {container_ops});
    add("is_empty", "auto", {}, "return num() == 0;", "bool", true, true);
    std::vector<std::string> nums;
    for (auto const& member : members) {
        nums.push_back("    ml::num(" + member.name + "),");
    }
    add("validate_array_sizes",
        "void",
        {},
        "ml::fatal_if_nums_not_equal({\n" + join_lines(nums) + "\n});",
        {},
        true,
        false,
        {array_checks, container_ops});
    if (!const_only) {
        add("slice",
            "auto",
            {FunctionParameter{"int32 const", "offset"}, FunctionParameter{"int32 const", "count"}},
            "return get_view(offset, count);",
            "View");
        add("left",
            "auto",
            {FunctionParameter{"int32 const", "count"}},
            "return slice(0, count);",
            "View");
        add("right",
            "auto",
            {FunctionParameter{"int32 const", "count"}},
            "return slice(num() - count, count);",
            "View");
    }
    add("slice",
        "auto",
        {FunctionParameter{"int32 const", "offset"}, FunctionParameter{"int32 const", "count"}},
        "return get_view(offset, count);",
        "ConstView",
        true);
    add("left",
        "auto",
        {FunctionParameter{"int32 const", "count"}},
        "return slice(0, count);",
        "ConstView",
        true);
    add("right",
        "auto",
        {FunctionParameter{"int32 const", "count"}},
        "return slice(num() - count, count);",
        "ConstView",
        true);
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
                         schema.mutable_view_functions,
                         types,
                         schema.export_specifier,
                         true),
             2)
        .add(view_struct(view_name,
                         view_name,
                         const_view_name,
                         members,
                         schema.equivalent_type,
                         schema.mutable_view_functions,
                         types,
                         schema.export_specifier,
                         false),
             2)
        .build();
}

auto soa_storage_view_nodes(std::vector<ResolvedMember> const& members) -> Nodes {
    NodeListBuilder result;
    auto const columns{column_names(members)};
    return result.add(column_apply_arrays_function(columns), 2)
        .add(column_apply_array_pairs_function(columns), 2)
        .append(declaration_nodes(soa_view_specs(members, false)))
        .build();
}

} // namespace codegen::detail
