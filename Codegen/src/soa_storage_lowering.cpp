#include "lowering_utils.h"
#include "soa_internal.h"

#include <algorithm>
#include <utility>

namespace codegen::detail {
namespace {

TypeDependency const tarray_view{"TArrayView", "Containers/ArrayView.h", {}};
TypeDependency const allow_shrinking{"EAllowShrinking", "Containers/AllowShrinking.h", {}};
TypeDependency const container_ops{"ml::num", "SandboxCore/container_ops.h", {}};
TypeDependency const soa_concepts{
    "ml::SupportsApplyArrayPairsWith", "SandboxCore/soa_concepts.h", {}};
TypeDependency const soa_permutation{"ml::apply_permutation", "SandboxCore/soa_permutation.h", {}};
TypeDependency const fill_indices{"ml::fill_indices", "SandboxCore/array_utils.h", {}};
TypeDependency const check_dependency{"check", "CoreMinimal.h", {}};

auto custom_function_spec(FunctionSchema const& schema, std::map<std::string, CppType> const& types)
    -> FunctionSpec {
    std::vector<FunctionParameter> parameters;
    for (auto const& parameter : schema.parameters) {
        auto resolved{resolve_type(parameter.type, types)};
        if (parameter.default_value.has_value()) {
            parameters.emplace_back(std::move(resolved), parameter.name, *parameter.default_value);
        } else {
            parameters.emplace_back(std::move(resolved), parameter.name);
        }
    }
    std::vector<TypeDependency> dependencies;
    for (auto const& key : schema.dependencies) {
        dependencies.push_back(dependency_for_key(key, types));
    }
    return FunctionSpec{
        .name = schema.name,
        .return_type = resolve_type(schema.return_type, types),
        .parameters = std::move(parameters),
        .body = {raw(join_lines(schema.body_lines), std::move(dependencies))},
        .qualifiers =
            {
                .trailing_return_type =
                    schema.trailing_return_type.has_value()
                        ? std::optional<CppType>{resolve_type(*schema.trailing_return_type, types)}
                        : std::nullopt,
                .is_const = schema.is_const,
                .is_noexcept = schema.is_noexcept,
            },
        .is_static = schema.is_static,
        .is_inline = schema.is_inline,
        .template_parameters = schema.template_parameters,
        .requires_clause = schema.requires_clause,
    };
}

} // namespace

auto soa_storage_operation_specs(SoaSchema const& schema,
                                 std::vector<ResolvedMember> const& members)
    -> std::vector<FunctionSpec> {
    std::vector<FunctionSpec> result;
    auto member_calls = [&](std::string const& function,
                            std::vector<std::string> const& arguments) {
        NodeListBuilder calls;
        for (auto const& member : members) {
            auto values{std::vector<std::string>{member.name}};
            values.insert(values.end(), arguments.begin(), arguments.end());
            calls.add(
                ExpressionStatement{function + "(" + join(values, ", ") + ")", {container_ops}});
        }
        return calls.build();
    };
    auto contains = [&](StorageOperation operation) {
        return std::ranges::find(schema.operations, operation) != schema.operations.end();
    };
    if (contains(StorageOperation::reset)) {
        result.push_back(FunctionSpec{
            .name = "reset", .return_type = "void", .body = member_calls("ml::reset", {})});
    }
    if (contains(StorageOperation::reserve)) {
        result.push_back(FunctionSpec{
            .name = "reserve",
            .return_type = "void",
            .parameters = {FunctionParameter{"int32 const", "count"}},
            .body = member_calls("ml::reserve", {"count"}),
        });
    }
    if (contains(StorageOperation::add_uninitialised)) {
        result.push_back(FunctionSpec{
            .name = "add_uninitialised",
            .return_type = "void",
            .parameters = {FunctionParameter{"int32 const", "count"}},
            .body = member_calls("ml::add_uninitialised", {"count"}),
        });
    }
    if (contains(StorageOperation::add_defaulted)) {
        result.push_back(FunctionSpec{
            .name = "add_defaulted",
            .return_type = "void",
            .parameters = {FunctionParameter{"int32 const", "count"}},
            .body = member_calls("ml::add_defaulted", {"count"}),
        });
    }
    if (contains(StorageOperation::remove_at_swap)) {
        NodeListBuilder calls;
        for (auto const& member : members) {
            auto const operation{member.container_type.operation(TypeOperation::remove_at_swap)};
            calls.add(ExpressionStatement{
                operation.has_value()
                    ? member.name + "." + *operation + "(index, count, allow_shrinking)"
                    : "ml::remove_at_swap(" + member.name + ", index, count, allow_shrinking)",
                {container_ops}});
        }
        result.push_back(FunctionSpec{
            .name = "remove_at_swap",
            .return_type = "void",
            .parameters = {FunctionParameter{"int32 const", "index"},
                           FunctionParameter{"int32 const", "count"},
                           FunctionParameter{CppType{"EAllowShrinking const", {allow_shrinking}},
                                             "allow_shrinking"}},
            .body = calls.build(),
            .is_inline = true,
        });
    }
    if (contains(StorageOperation::set_num)) {
        result.push_back(FunctionSpec{
            .name = "set_num",
            .return_type = "void",
            .parameters = {FunctionParameter{"int32 const", "count"},
                           FunctionParameter{CppType{"EAllowShrinking const", {allow_shrinking}},
                                             "allow_shrinking"}},
            .body = member_calls("ml::set_num", {"count", "allow_shrinking"}),
        });
    }
    if (contains(StorageOperation::copy_element)) {
        NodeListBuilder copy_one;
        NodeListBuilder copy_range;
        for (auto const& member : members) {
            if (schema.copy_element_memberwise) {
                copy_one.add(AssignmentStatement{
                    member.name + "[dst_i]", "other." + member.name + "[src_i]", {container_ops}});
            } else {
                copy_one.add(ExpressionStatement{"ml::copy_element(" + member.name +
                                                     ", dst_i, other." + member.name + ", src_i)",
                                                 {container_ops}});
            }
            copy_range.add(ExpressionStatement{"ml::copy_elements(" + member.name +
                                                   ", dst_i, other." + member.name +
                                                   ", src_i, count)",
                                               {container_ops}});
        }
        result.push_back(FunctionSpec{
            .name = "copy_element",
            .return_type = "void",
            .parameters = {FunctionParameter{"int32 const", "dst_i"},
                           FunctionParameter{"Other const&", "other"},
                           FunctionParameter{"int32 const", "src_i"}},
            .body = copy_one.build(),
            .is_inline = true,
            .template_parameters = "typename Other",
        });
        result.push_back(FunctionSpec{
            .name = "copy_elements",
            .return_type = "void",
            .parameters = {FunctionParameter{"int32 const", "dst_i"},
                           FunctionParameter{"Other const&", "other"},
                           FunctionParameter{"int32 const", "src_i"},
                           FunctionParameter{"int32 const", "count"}},
            .body = copy_range.build(),
            .is_inline = true,
            .template_parameters = "typename Other",
        });
        result.push_back(FunctionSpec{
            .name = "copy_to_tail",
            .return_type = "void",
            .parameters = {FunctionParameter{"Other const&", "other"}},
            .body =
                {
                    VariableDeclarationStatement{"auto const", "count", "other.num()"},
                    ExpressionStatement{"check(num() >= count)", {check_dependency}},
                    ExpressionStatement{"copy_elements(num() - count, other, 0, count)"},
                },
            .is_inline = true,
            .template_parameters = "typename Other",
        });
    }
    if (contains(StorageOperation::append_from)) {
        NodeListBuilder calls;
        for (auto const& member : members) {
            calls.add(ExpressionStatement{"ml::append_from(" + member.name + ", other." +
                                              member.name + ")",
                                          {container_ops, soa_concepts}});
        }
        result.push_back(FunctionSpec{
            .name = "append_from",
            .return_type = "void",
            .parameters = {FunctionParameter{"Other const&", "other"}},
            .body = calls.build(),
            .is_inline = true,
            .template_parameters = "typename Other",
            .requires_clause = "ml::SupportsApplyArrayPairsWith<" + schema.name + ", Other>",
        });
    }
    return result;
}

auto soa_permutation_specs(std::vector<ResolvedMember> const& members)
    -> std::vector<FunctionSpec> {
    NodeListBuilder apply;
    apply.add(ExpressionStatement{"validate_array_sizes()"})
        .add(ExpressionStatement{"check(indices.Num() == num())", {check_dependency}});
    for (auto const& member : members) {
        apply.add(ExpressionStatement{"ml::apply_permutation(" + member.name + ", indices)",
                                      {soa_permutation}});
    }
    auto sort_body = [](std::string sort_expression) {
        NodeListBuilder result;
        return result.add(ExpressionStatement{"validate_array_sizes()"})
            .add(VariableDeclarationStatement{"auto const", "n", "num()"})
            .add(ExpressionStatement{"check(scratch_indices.Num() == n)", {check_dependency}})
            .add(ExpressionStatement{"ml::fill_indices(scratch_indices)", {fill_indices}})
            .add(raw("// indices[new_index] is the old row index that belongs at new_index.\n" +
                     std::move(sort_expression)))
            .add(ExpressionStatement{"apply_permutation(scratch_indices)"})
            .build();
    };
    return {
        FunctionSpec{
            .name = "apply_permutation",
            .return_type = "void",
            .parameters = {FunctionParameter{CppType{"TArrayView<int32>", {tarray_view}},
                                             "indices"}},
            .body = apply.build(),
        },
        FunctionSpec{
            .name = "sort",
            .return_type = "void",
            .parameters = {FunctionParameter{"Compare&&", "compare"},
                           FunctionParameter{CppType{"TArrayView<int32>", {tarray_view}},
                                             "scratch_indices"}},
            .body = sort_body(
                "scratch_indices.Sort([this, &compare](int32 const lhs, int32 const rhs) {\n"
                "    return compare(*this, lhs, rhs);\n"
                "});"),
            .is_inline = true,
            .template_parameters = "typename Compare",
        },
        FunctionSpec{
            .name = "sort",
            .return_type = "void",
            .parameters = {FunctionParameter{CppType{"TArrayView<int32>", {tarray_view}},
                                             "scratch_indices"}},
            .body = sort_body("scratch_indices.Sort([this](int32 const lhs, int32 const rhs) {\n"
                              "    return Compare(*this, lhs, rhs);\n"
                              "});"),
            .is_inline = true,
            .template_parameters = "auto Compare",
        },
    };
}

auto soa_storage_node(SoaSchema const& schema,
                      std::vector<ResolvedMember> const& members,
                      std::string const& view_name,
                      std::string const& const_view_name,
                      std::map<std::string, CppType> const& types,
                      std::vector<FunctionSpec>& custom_source,
                      Nodes storage_prelude) -> Node {
    NodeListBuilder nodes;
    nodes.add(UsingDeclaration{"View", CppType{view_name}}, 1)
        .add(UsingDeclaration{"ConstView", CppType{const_view_name}}, 2);
    nodes.append(std::move(storage_prelude));
    if (schema.equivalent_type.has_value()) {
        nodes.append(soa_equivalent_nodes(*schema.equivalent_type, members, types)).new_lines(2);
    }
    for (auto const& declaration_text : schema.using_declarations) {
        nodes.add(raw("using " + declaration_text + ";"), 2);
    }
    for (auto const& function : schema.functions) {
        auto spec{custom_function_spec(function, types)};
        if (function.definition_in_source) {
            custom_source.push_back(spec);
            nodes.add(declaration(spec), 2);
        } else {
            nodes.add(header_function(spec), 2);
        }
    }
    auto operations{soa_storage_operation_specs(schema, members)};
    for (auto const& spec : operations) {
        nodes.add(spec.is_inline ? header_function(spec) : declaration(spec), 2);
    }
    auto permutations{soa_permutation_specs(members)};
    for (auto const& spec : permutations) {
        nodes.add(spec.is_inline ? header_function(spec) : declaration(spec), 2);
    }
    nodes.append(soa_storage_view_nodes(members)).new_lines(2);
    for (std::size_t index{0}; index < members.size(); ++index) {
        nodes.add(Member{members[index].container_type, members[index].name});
        if (index + 1 < members.size()) {
            nodes.new_lines();
        }
    }
    return Struct{
        .name = schema.name,
        .children = nodes.build(),
        .export_specifier = schema.export_specifier,
    };
}

} // namespace codegen::detail
