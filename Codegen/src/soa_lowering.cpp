#include "lowering.h"
#include "lowering_utils.h"
#include "soa_internal.h"

#include <algorithm>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace codegen::detail {
namespace {

TypeDependency const std_forward{"std::forward", "utility", {}};
TypeDependency const std_remove_const{"std::remove_const_t", "type_traits", {}};
TypeDependency const tarray{"TArray", "Containers/Array.h", {}};
TypeDependency const tarray_view{"TArrayView", "Containers/ArrayView.h", {}};
TypeDependency const allow_shrinking{"EAllowShrinking", "Containers/AllowShrinking.h", {}};
TypeDependency const array_checks{
    "ml::fatal_if_nums_not_equal", "SandboxCore/array_checks.h", {}};
TypeDependency const container_ops{"ml::num", "SandboxCore/container_ops.h", {}};
TypeDependency const soa_concepts{
    "ml::SupportsApplyArrayPairsWith", "SandboxCore/soa_concepts.h", {}};
TypeDependency const soa_permutation{
    "ml::apply_permutation", "SandboxCore/soa_permutation.h", {}};
TypeDependency const fill_indices{"ml::fill_indices", "SandboxCore/array_utils.h", {}};
TypeDependency const check_dependency{"check", "CoreMinimal.h", {}};

auto composed(std::string spelling,
              TypeDependency outer,
              CppType const& contained) -> CppType {
    outer.spelling = spelling;
    outer.dependencies = contained.dependencies;
    return CppType{std::move(spelling), std::vector<TypeDependency>{std::move(outer)}};
}

auto resolve_members_impl(SoaSchema const& schema,
                          std::map<std::string, CppType> const& types)
    -> std::vector<ResolvedMember> {
    std::vector<ResolvedMember> result;
    for (auto const& member : schema.members) {
        auto element{resolve_type(member.type, types)};
        if (member.kind == SoaMemberKind::array) {
            auto container{composed("TArray<" + element.spelling + ">", tarray, element)};
            container.member_operations.emplace(TypeOperation::remove_at_swap, "RemoveAtSwap");
            result.push_back(ResolvedMember{
                .name = member.name,
                .kind = member.kind,
                .element_type = element,
                .container_type = std::move(container),
                .view_type = composed("TArrayView<" + element.spelling + ">", tarray_view, element),
                .const_view_type =
                    composed("TConstArrayView<" + element.spelling + ">", tarray_view, element),
            });
        } else {
            result.push_back(ResolvedMember{
                .name = member.name,
                .kind = member.kind,
                .element_type = element,
                .container_type = element,
                .view_type = CppType{element.spelling + "::View", element.dependencies},
                .const_view_type = CppType{element.spelling + "::ConstView", element.dependencies},
            });
        }
    }
    return result;
}

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

auto equivalent_nodes(TypeRef const& equivalent_reference,
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

auto view_specs(std::vector<ResolvedMember> const& members,
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
        nodes.append(equivalent_nodes(*equivalent, members, types)).new_lines(2);
    }
    nodes.add(apply_arrays_function(members), 2)
        .append(declaration_nodes(view_specs(members, const_only)))
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

auto storage_operation_specs(SoaSchema const& schema,
                             std::vector<ResolvedMember> const& members)
    -> std::vector<FunctionSpec> {
    std::vector<FunctionSpec> result;
    auto member_calls = [&](std::string const& function,
                            std::vector<std::string> const& arguments) {
        NodeListBuilder calls;
        for (auto const& member : members) {
            auto values{std::vector<std::string>{member.name}};
            values.insert(values.end(), arguments.begin(), arguments.end());
            calls.add(ExpressionStatement{function + "(" + join(values, ", ") + ")",
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
            calls.add(ExpressionStatement{
                operation.has_value()
                    ? member.name + "." + *operation + "(index, count, allow_shrinking)"
                    : "ml::remove_at_swap(" + member.name +
                          ", index, count, allow_shrinking)",
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
                copy_one.add(AssignmentStatement{member.name + "[dst_i]",
                                                 "other." + member.name + "[src_i]",
                                                 {container_ops}});
            } else {
                copy_one.add(ExpressionStatement{"ml::copy_element(" + member.name +
                                                       ", dst_i, other." + member.name +
                                                       ", src_i)",
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
            .body = {raw("auto const count{other.num()};\ncheck(num() >= count);\n"
                         "copy_elements(num() - count, other, 0, count);",
                         {check_dependency})},
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

auto permutation_specs(std::vector<ResolvedMember> const& members) -> std::vector<FunctionSpec> {
    NodeListBuilder apply;
    apply.add(ExpressionStatement{"validate_array_sizes()"})
        .add(ExpressionStatement{"check(indices.Num() == num())", {check_dependency}});
    for (auto const& member : members) {
        apply.add(ExpressionStatement{"ml::apply_permutation(" + member.name + ", indices)",
                                      {soa_permutation}});
    }
    std::string const common{
        "validate_array_sizes();\n"
        "auto const n{num()};\n"
        "check(scratch_indices.Num() == n);\n"
        "ml::fill_indices(scratch_indices);\n"
        "// indices[new_index] is the old row index that belongs at new_index.\n"};
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
            .body = {raw(common +
                             "scratch_indices.Sort([this, &compare](int32 const lhs, int32 const rhs) {\n"
                             "    return compare(*this, lhs, rhs);\n"
                             "});\n"
                             "apply_permutation(scratch_indices);",
                         {fill_indices, check_dependency})},
            .is_inline = true,
            .template_parameters = "typename Compare",
        },
        FunctionSpec{
            .name = "sort",
            .return_type = "void",
            .parameters = {FunctionParameter{CppType{"TArrayView<int32>", {tarray_view}},
                                             "scratch_indices"}},
            .body = {raw(common +
                             "scratch_indices.Sort([this](int32 const lhs, int32 const rhs) {\n"
                             "    return Compare(*this, lhs, rhs);\n"
                             "});\n"
                             "apply_permutation(scratch_indices);",
                         {fill_indices, check_dependency})},
            .is_inline = true,
            .template_parameters = "auto Compare",
        },
    };
}

auto custom_function_spec(FunctionSchema const& schema,
                          std::map<std::string, CppType> const& types) -> FunctionSpec {
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
        .suffix = schema.suffix,
        .is_static = schema.is_static,
        .is_inline = schema.is_inline,
        .template_parameters = schema.template_parameters,
        .requires_clause = schema.requires_clause,
    };
}

auto storage_struct(SoaSchema const& schema,
                    std::vector<ResolvedMember> const& members,
                    std::string const& view_name,
                    std::string const& const_view_name,
                    std::map<std::string, CppType> const& types,
                    std::vector<FunctionSpec>& custom_source,
                    Nodes storage_prelude) -> Struct {
    NodeListBuilder nodes;
    nodes.add(UsingDeclaration{"View", CppType{view_name}}, 1)
        .add(UsingDeclaration{"ConstView", CppType{const_view_name}}, 2);
    nodes.append(std::move(storage_prelude));
    if (schema.equivalent_type.has_value()) {
        nodes.append(equivalent_nodes(*schema.equivalent_type, members, types)).new_lines(2);
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
    auto operations{storage_operation_specs(schema, members)};
    for (auto const& spec : operations) {
        nodes.add(spec.is_inline ? header_function(spec) : declaration(spec), 2);
    }
    auto permutations{permutation_specs(members)};
    for (auto const& spec : permutations) {
        nodes.add(spec.is_inline ? header_function(spec) : declaration(spec), 2);
    }
    nodes.add(apply_arrays_function(members), 2)
        .add(apply_array_pairs_function(members), 2)
        .append(declaration_nodes(view_specs(members, false)))
        .new_lines(2);
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

auto lower_soa_impl(SoaSchema const& schema,
                    std::map<std::string, CppType> const& types,
                    Nodes storage_prelude) -> LoweredSoa {
    auto const members{resolve_members_impl(schema, types)};
    auto const view_name{schema.view_name.value_or(schema.name + "View")};
    auto const const_view_name{schema.const_view_name.value_or(schema.name + "ConstView")};
    std::vector<FunctionSpec> custom_source;
    auto storage{storage_struct(
        schema, members, view_name, const_view_name, types, custom_source, std::move(storage_prelude))};
    NodeListBuilder header;
    header.add(ForwardDeclaration{view_name}, 1)
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
        .add(std::move(storage));

    NodeListBuilder source;
    bool has_source_definition{};
    auto add_definition = [&](FunctionSpec const& spec, std::string const& owner) {
        if (has_source_definition) {
            source.new_lines(2);
        }
        source.add(definition(spec, owner));
        has_source_definition = true;
    };
    for (auto const& spec : custom_source) {
        add_definition(spec, schema.name);
    }
    auto append_definitions = [&](std::vector<FunctionSpec> const& specs, std::string const& owner) {
        for (auto const& spec : specs) {
            add_definition(spec, owner);
        }
    };
    append_definitions(view_specs(members, true), const_view_name);
    append_definitions(view_specs(members, false), view_name);
    for (auto const& spec : storage_operation_specs(schema, members)) {
        if (!spec.is_inline) {
            add_definition(spec, schema.name);
        }
    }
    auto const permutations{permutation_specs(members)};
    add_definition(permutations.front(), schema.name);
    append_definitions(view_specs(members, false), schema.name);
    return LoweredSoa{header.build(), source.build()};
}

auto lower_soa_module_impl(SoaModuleSchema const& module,
                           std::map<std::string, CppType> const& types) -> Module {
    std::map<std::string, SoaSchema const*> schemas;
    for (auto const& schema : module.structs) {
        schemas.emplace(schema.name, &schema);
    }

    std::vector<LoweredSoa> lowered_structs;
    lowered_structs.reserve(module.structs.size());
    for (auto const& schema : module.structs) {
        auto lowered{lower_soa_impl(schema, types, {})};
        if (schema.fixed.has_value()) {
            NodeListBuilder header;
            header.append(std::move(lowered.header))
                .new_lines(2)
                .append(lower_fixed_nodes(schema, schemas, types));
            lowered.header = header.build();
        }
        lowered_structs.push_back(std::move(lowered));
    }

    NodeListBuilder header_nodes;
    header_nodes.add(IncludeDependencies{}, 2);
    if (!module.settings.prelude_lines.empty()) {
        header_nodes.add(raw(join_lines(module.settings.prelude_lines)), 2);
    }
    NodeListBuilder definitions;
    for (std::size_t index{0}; index < lowered_structs.size(); ++index) {
        if (index > 0) {
            definitions.new_lines(2);
        }
        definitions.append(std::move(lowered_structs[index].header));
    }
    auto definition_nodes{definitions.build()};
    if (module.settings.namespace_name.has_value()) {
        header_nodes.add(Namespace{*module.settings.namespace_name, std::move(definition_nodes)});
    } else {
        header_nodes.append(std::move(definition_nodes));
    }
    Module result{
        .name = module.settings.name,
        .header = CppFile{
            .path = module.settings.header,
            .nodes = header_nodes.build(),
            .clang_format_off = true,
            .include_order = module.settings.include_order,
        },
    };
    if (module.settings.source.has_value()) {
        NodeListBuilder source_definitions;
        for (std::size_t index{0}; index < lowered_structs.size(); ++index) {
            if (index > 0) {
                source_definitions.new_lines(2);
            }
            source_definitions.append(std::move(lowered_structs[index].source));
        }
        auto source_definition_nodes{source_definitions.build()};
        if (module.settings.namespace_name.has_value()) {
            source_definition_nodes = {
                Namespace{*module.settings.namespace_name, std::move(source_definition_nodes)}};
        }
        NodeListBuilder source_nodes;
        source_nodes.add(Include{source_include(module.settings), false}, 2)
            .add(IncludeDependencies{}, 2)
            .append(std::move(source_definition_nodes));
        result.source = CppFile{
            .path = *module.settings.source,
            .nodes = source_nodes.build(),
            .pragma_once = false,
            .clang_format_off = true,
            .include_order = module.settings.include_order,
        };
    }
    return result;
}

} // namespace

auto resolve_members(SoaSchema const& schema,
                     std::map<std::string, CppType> const& types)
    -> std::vector<ResolvedMember> {
    return resolve_members_impl(schema, types);
}

auto lower_soa(SoaSchema const& schema,
               std::map<std::string, CppType> const& types,
               Nodes storage_prelude) -> LoweredSoa {
    return lower_soa_impl(schema, types, std::move(storage_prelude));
}

auto lower_soa_module(SoaModuleSchema const& module,
                      std::map<std::string, CppType> const& types) -> Module {
    return lower_soa_module_impl(module, types);
}

} // namespace codegen::detail
