#include "lowering_utils.h"
#include "soa_internal.h"

#include <algorithm>
#include <string_view>
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

auto function_spec(FunctionSchema const& schema, std::map<std::string, CppType> const& types)
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

auto has_custom_function(std::vector<FunctionSchema> const& functions, std::string_view const name)
    -> bool {
    return std::ranges::any_of(
        functions, [name](FunctionSchema const& function) { return function.name == name; });
}

auto parameter_type(CppType type, ParameterPassing const passing) -> CppType {
    return qualify(std::move(type), passing == ParameterPassing::value ? " const" : " const&");
}

} // namespace

auto soa_function_spec(FunctionSchema const& schema, std::map<std::string, CppType> const& types)
    -> FunctionSpec {
    return function_spec(schema, types);
}

auto soa_set_spec(SoaSchema const& schema,
                  std::vector<ResolvedMember> const& members,
                  bool const is_const) -> std::optional<FunctionSpec> {
    if (has_custom_function(schema.functions, "set") ||
        has_custom_function(schema.mutable_view_functions, "set")) {
        return std::nullopt;
    }

    std::vector<FunctionParameter> parameters{FunctionParameter{"int32 const", "index"}};
    NodeListBuilder body;
    for (auto const& member : members) {
        auto const argument{"new_" + member.name};
        if (member.kind == SoaMemberKind::array) {
            parameters.emplace_back(
                parameter_type(member.element_type, member.element_type.parameter_passing),
                argument);
            body.add(AssignmentStmt{RawExpr{member.name + "[index]"}, RawExpr{argument}});
            continue;
        }

        auto const operation{member.container_type.operation(TypeOperation::set_element)};
        if (!operation.has_value()) {
            return std::nullopt;
        }
        auto equivalent_type{qualify(member.element_type, "::equivalent_type")};
        parameters.emplace_back(parameter_type(std::move(equivalent_type),
                                               member.container_type.operation_parameter_passing(
                                                   TypeOperation::set_element)),
                                argument);
        body.add(
            ExpressionStmt{RawExpr{member.name + "." + *operation + "(index, " + argument + ")"}});
    }
    return FunctionSpec{
        .name = "set",
        .return_type = "void",
        .parameters = std::move(parameters),
        .body = body.build(),
        .qualifiers = {.is_const = is_const},
        .is_inline = true,
    };
}

auto soa_add_spec(SoaSchema const& schema, std::vector<ResolvedMember> const& members)
    -> std::optional<FunctionSpec> {
    if (has_custom_function(schema.functions, "add")) {
        return std::nullopt;
    }

    std::vector<FunctionParameter> parameters;
    NodeListBuilder body;
    body.add(VariableDeclarationStmt{"auto const", "index", RawExpr{"num()"}});
    for (auto const& member : members) {
        auto const argument{"new_" + member.name};
        if (member.kind == SoaMemberKind::array) {
            parameters.emplace_back(
                parameter_type(member.element_type, member.element_type.parameter_passing),
                argument);
            body.add(ExpressionStmt{RawExpr{member.name + ".Add(" + argument + ")"}});
            continue;
        }

        auto const operation{member.container_type.operation(TypeOperation::add_element)};
        if (!operation.has_value()) {
            return std::nullopt;
        }
        auto equivalent_type{qualify(member.element_type, "::equivalent_type")};
        parameters.emplace_back(parameter_type(std::move(equivalent_type),
                                               member.container_type.operation_parameter_passing(
                                                   TypeOperation::add_element)),
                                argument);
        body.add(ExpressionStmt{RawExpr{member.name + "." + *operation + "(" + argument + ")"}});
    }
    body.add(ReturnStmt{RawExpr{"index"}});
    return FunctionSpec{
        .name = "add",
        .return_type = "auto",
        .parameters = std::move(parameters),
        .body = body.build(),
        .qualifiers = {.trailing_return_type = CppType{"int32"}},
        .is_inline = true,
    };
}

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
            calls.add(ExpressionStmt{RawExpr{function + "(" + join(values, ", ") + ")"},
                                     {container_ops}});
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
            calls.add(ExpressionStmt{
                RawExpr{operation.has_value()
                            ? member.name + "." + *operation + "(index, count, allow_shrinking)"
                            : "ml::remove_at_swap(" + member.name +
                                  ", index, count, allow_shrinking)"},
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
                copy_one.add(AssignmentStmt{RawExpr{member.name + "[dst_i]"},
                                            RawExpr{"other." + member.name + "[src_i]"},
                                            {container_ops}});
            } else {
                copy_one.add(ExpressionStmt{RawExpr{"ml::copy_element(" + member.name +
                                                    ", dst_i, other." + member.name + ", src_i)"},
                                            {container_ops}});
            }
            copy_range.add(
                ExpressionStmt{RawExpr{"ml::copy_elements(" + member.name + ", dst_i, other." +
                                       member.name + ", src_i, count)"},
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
                    VariableDeclarationStmt{"auto const", "count", RawExpr{"other.num()"}},
                    ExpressionStmt{RawExpr{"check(num() >= count)"}, {check_dependency}},
                    ExpressionStmt{RawExpr{"copy_elements(num() - count, other, 0, count)"}},
                },
            .is_inline = true,
            .template_parameters = "typename Other",
        });
    }
    if (contains(StorageOperation::append_from)) {
        NodeListBuilder calls;
        auto const member_count{members.size()};
        for (std::size_t index{}; index < member_count; ++index) {
            auto const& member{members[index]};
            auto const call{schema.members[index].nested_schema
                                ? member.name + ".append_from(other." + member.name + ")"
                                : "ml::append_from(" + member.name + ", other." + member.name +
                                      ")"};
            calls.add(ExpressionStmt{RawExpr{call}, {container_ops, soa_concepts}});
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
    apply.add(ExpressionStmt{RawExpr{"validate_array_sizes()"}})
        .add(ExpressionStmt{RawExpr{"check(indices.Num() == num())"}, {check_dependency}});
    for (auto const& member : members) {
        apply.add(ExpressionStmt{RawExpr{"ml::apply_permutation(" + member.name + ", indices)"},
                                 {soa_permutation}});
    }
    auto sort_body = [](std::string sort_expression) {
        NodeListBuilder result;
        return result.add(ExpressionStmt{RawExpr{"validate_array_sizes()"}})
            .add(VariableDeclarationStmt{"auto const", "n", RawExpr{"num()"}})
            .add(ExpressionStmt{RawExpr{"check(scratch_indices.Num() == n)"}, {check_dependency}})
            .add(ExpressionStmt{RawExpr{"ml::fill_indices(scratch_indices)"}, {fill_indices}})
            .add(raw("// indices[new_index] is the old row index that belongs at new_index.\n" +
                     std::move(sort_expression)))
            .add(ExpressionStmt{RawExpr{"apply_permutation(scratch_indices)"}})
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
        auto spec{soa_function_spec(function, types)};
        if (function.definition_in_source) {
            custom_source.push_back(spec);
            nodes.add(declaration(spec), 2);
        } else {
            nodes.add(header_function(spec), 2);
        }
    }
    if (auto set{soa_set_spec(schema, members, false)}; set.has_value()) {
        nodes.add(header_function(*set), 2);
    }
    if (auto add{soa_add_spec(schema, members)}; add.has_value()) {
        nodes.add(header_function(*add), 2);
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
