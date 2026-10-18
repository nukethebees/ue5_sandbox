#include "lowering_context.h"
#include "lowering_utils.h"
#include "soa_api.h"
#include "soa_functions.h"
#include "soa_internal.h"
#include "soa_receiver.h"
#include "soa_representation.h"

#include <codegen/ast/binary_operator.h>
#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/expr.h>
#include <codegen/ast/function_receiver.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/node_id.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/ast/nodes.h>
#include <codegen/ast/parameter_passing.h>
#include <codegen/ast/return_syntax.h>
#include <codegen/ast/type_operation.h>
#include <codegen/ast/unary_operator.h>
#include <codegen/lowering/indices.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/function_schemas.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/schema_range.h>
#include <codegen/schema/soa_member_kind.h>
#include <codegen/schema/soa_member_schemas.h>
#include <codegen/schema/soa_schemas.h>
#include <codegen/schema/storage_operation.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <algorithm>
#include <cstddef>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lispb::codegen::detail {
namespace {

auto container_function(LoweringContext& context, std::string spelling) -> Expr {
    auto& ir{context.ir};
    auto const dependency{ir.types.dependency(spelling, "SandboxCore/container_ops.h")};
    return ir.named(std::move(spelling), {std::move(dependency)});
}

auto has_custom_function(LoweringContext& context,
                         SchemaRange<FunctionSchemaId> functions,
                         std::string_view const name) -> bool {
    auto const identifier{context.ir.strings.intern(name)};
    return std::ranges::any_of(
        context.declarations.items(functions), [&](FunctionSchemaId const function) {
            return context.declarations.functions
                       .name[context.declarations.functions.index(function)] == identifier;
        });
}

auto parameter_type(LoweringContext& context, CppTypeId type, ParameterPassing const passing)
    -> CppTypeId {
    auto& ir{context.ir};
    return qualify(ir, std::move(type), passing == ParameterPassing::value ? " const" : " const&");
}

auto storage_operation_body(LoweringContext& context,
                            std::string const& operation,
                            std::vector<std::string> arguments = {}) -> Nodes {
    auto& ir{context.ir};
    std::vector<Expr> values{ir.unary(UnaryOperator::dereference, ir.named("this"))};
    for (auto const& argument : arguments) {
        values.push_back(ir.named(argument));
    }
    return {ir.expression_statement(
        ir.call(ir.named("ioj::soa_ops::" + operation,
                         {ir.types.dependency("ioj::soa_ops", "SandboxCore/soa_storage_ops.h")}),
                values))};
}

auto has_custom_remove_at_swap(LoweringContext& context, std::span<ResolvedMemberId const> members)
    -> bool {
    auto& resolved_store{context.ir.lowering.resolved_members};
    auto& ir{context.ir};
    return std::ranges::any_of(members, [&ir, &resolved_store](ResolvedMemberId member) {
        std::optional<std::string> const operation{
            ir.types.operation(resolved_store.container_type[resolved_store.index(member)],
                               TypeOperation::remove_at_swap)};
        return operation.has_value() && *operation != "RemoveAtSwap" &&
             *operation != "remove_at_swap";
    });
}

auto explicit_remove_at_swap_body(LoweringContext& context,
                                  std::span<ResolvedMemberId const> members) -> Nodes {
    auto& resolved_store{context.ir.lowering.resolved_members};
    auto& ir{context.ir};
    NodeListBuilder calls{ir};
    calls.add(ir.expression_statement(ir.call(ir.named("validate_array_sizes"))));
    for (auto const member : members) {
        std::optional<std::string> const operation{
            ir.types.operation(resolved_store.container_type[resolved_store.index(member)],
                               TypeOperation::remove_at_swap)};
        auto const invocation{
            operation.has_value()
                ? ir.call(ir.member_access(ir.named(context.text(
                                               resolved_store.name[resolved_store.index(member)])),
                                           *operation),
                          {ir.named("index"), ir.named("count"), ir.named("allow_shrinking")})
                : ir.call(
                      container_function(context, "ioj::remove_at_swap"),
                      {ir.named(context.text(resolved_store.name[resolved_store.index(member)])),
                       ir.named("index"),
                       ir.named("count"),
                       ir.named("allow_shrinking")})};
        calls.add(ir.expression_statement(invocation));
    }
    return calls.build();
}

} // namespace

auto soa_set_function(LoweringContext& context,
                      DeclarationHandle schema,
                      std::span<ResolvedMemberId const> members,
                      bool const is_const) -> std::optional<FunctionId> {
    auto& resolved_store{context.ir.lowering.resolved_members};
    auto& ir{context.ir};
    if (has_custom_function(
            context,
            context.declarations.soas.functions[context.declarations.soas.index(schema)],
            "set") ||
        has_custom_function(context,
                            context.declarations.soas
                                .mutable_view_functions[context.declarations.soas.index(schema)],
                            "set")) {
        return std::nullopt;
    }

    std::vector<FunctionParameterId> parameters{ir.parameter("int32 const", "index")};
    NodeListBuilder body{ir};
    for (auto const member : members) {
        auto const argument{"new_" +
                            context.text(resolved_store.name[resolved_store.index(member)])};
        if (resolved_store.kind[resolved_store.index(member)] == SoaMemberKind::array) {
            parameters.push_back(ir.parameter(
                parameter_type(context,
                               resolved_store.element_type[resolved_store.index(member)],
                               ir.types.parameter_passing(
                                   resolved_store.element_type[resolved_store.index(member)])),
                argument));
            body.add(ir.assignment(
                ir.subscript(
                    ir.named(context.text(resolved_store.name[resolved_store.index(member)])),
                    ir.named("index")),
                ir.named(argument)));
            continue;
        }

        std::optional<std::string> const operation{
            ir.types.operation(resolved_store.container_type[resolved_store.index(member)],
                               TypeOperation::set_element)};
        if (!operation.has_value()) {
            return std::nullopt;
        }
        auto equivalent_type{qualify(
            ir, resolved_store.element_type[resolved_store.index(member)], "::equivalent_type")};
        parameters.push_back(ir.parameter(
            parameter_type(context,
                           std::move(equivalent_type),
                           ir.types.operation_parameter_passing(
                               resolved_store.container_type[resolved_store.index(member)],
                               TypeOperation::set_element)),
            argument));
        body.add(ir.expression_statement(
            ir.call(ir.member_access(
                        ir.named(context.text(resolved_store.name[resolved_store.index(member)])),
                        *operation),
                    {ir.named("index"), ir.named(argument)})));
    }
    auto const created_function{ir.function("set", "void")};
    ir.set_function_parameters(created_function, std::move(parameters));
    ir.set_function_body(created_function, body.build());
    ir.set_function_receiver(created_function,
                             is_const ? FunctionReceiver::const_ : FunctionReceiver::unqualified);
    ir.function_flags(created_function).set_is_inline(true);
    ir.function_flags(created_function).set_define_in_header(true);
    return created_function;
}

auto soa_add_function(LoweringContext& context,
                      DeclarationHandle schema,
                      std::span<ResolvedMemberId const> members) -> std::optional<FunctionId> {
    auto& resolved_store{context.ir.lowering.resolved_members};
    auto& ir{context.ir};
    if (has_custom_function(
            context,
            context.declarations.soas.functions[context.declarations.soas.index(schema)],
            "add")) {
        return std::nullopt;
    }

    std::vector<FunctionParameterId> parameters;
    NodeListBuilder body{ir};
    body.add(ir.variable("auto const", "index", ir.call(ir.named("num"))));
    for (auto const member : members) {
        auto const argument{"new_" +
                            context.text(resolved_store.name[resolved_store.index(member)])};
        if (resolved_store.kind[resolved_store.index(member)] == SoaMemberKind::array) {
            parameters.push_back(ir.parameter(
                parameter_type(context,
                               resolved_store.element_type[resolved_store.index(member)],
                               ir.types.parameter_passing(
                                   resolved_store.element_type[resolved_store.index(member)])),
                argument));
            body.add(ir.expression_statement(ir.call(
                ir.member_access(
                    ir.named(context.text(resolved_store.name[resolved_store.index(member)])),
                    "Add"),
                {ir.named(argument)})));
            continue;
        }

        std::optional<std::string> const operation{
            ir.types.operation(resolved_store.container_type[resolved_store.index(member)],
                               TypeOperation::add_element)};
        if (!operation.has_value()) {
            return std::nullopt;
        }
        auto equivalent_type{qualify(
            ir, resolved_store.element_type[resolved_store.index(member)], "::equivalent_type")};
        parameters.push_back(ir.parameter(
            parameter_type(context,
                           std::move(equivalent_type),
                           ir.types.operation_parameter_passing(
                               resolved_store.container_type[resolved_store.index(member)],
                               TypeOperation::add_element)),
            argument));
        body.add(ir.expression_statement(
            ir.call(ir.member_access(
                        ir.named(context.text(resolved_store.name[resolved_store.index(member)])),
                        *operation),
                    {ir.named(argument)})));
    }
    body.add(ir.return_statement(ir.named("index")));
    auto const created_function{ir.function("add", "auto")};
    ir.set_function_parameters(created_function, std::move(parameters));
    ir.set_function_body(created_function, body.build());
    ir.set_function_return_type(created_function, ir.types.intern("int32"), ReturnSyntax::trailing);
    ir.function_flags(created_function).set_is_inline(true);
    ir.function_flags(created_function).set_define_in_header(true);
    return created_function;
}

auto soa_storage_functions(LoweringContext& context,
                           DeclarationHandle schema,
                           std::span<ResolvedMemberId const> members) -> std::vector<FunctionId> {
    auto& resolved_store{context.ir.lowering.resolved_members};
    auto& ir{context.ir};
    auto const count{ir.parameter("int32 const", "count")};
    auto const other{ir.parameter("Other const&", "other")};
    auto const destination_index{ir.parameter("int32 const", "dst_i")};
    auto const source_index{ir.parameter("int32 const", "src_i")};
    auto const allow_shrinking{ir.parameter(
        ir.types.intern("EAllowShrinking const",
                        {ir.types.dependency("EAllowShrinking", "Containers/AllowShrinking.h")}),
        "allow_shrinking")};

    std::vector<FunctionId> result;
    auto contains = [&](StorageOperation operation) {
        auto const operations{context.declarations.items(
            context.declarations.soas.operations[context.declarations.soas.index(schema)])};
        return std::ranges::find(operations, operation) != operations.end();
    };
    if (contains(StorageOperation::reset)) {
        result.push_back([&] {
            auto const created_function{ir.function("reset", "void")};
            ir.set_function_body(created_function, storage_operation_body(context, "reset"));
            return created_function;
        }());
    }
    if (contains(StorageOperation::reserve)) {
        result.push_back([&] {
            auto const created_function{ir.function("reserve", "void")};
            ir.set_function_parameters(created_function, {count});
            ir.set_function_body(created_function,
                                 storage_operation_body(context, "reserve", {"count"}));
            return created_function;
        }());
    }
    if (contains(StorageOperation::add_uninitialised)) {
        result.push_back([&] {
            auto const created_function{ir.function("add_uninitialised", "void")};
            ir.set_function_parameters(created_function, {count});
            ir.set_function_body(created_function,
                                 storage_operation_body(context, "add_uninitialised", {"count"}));
            return created_function;
        }());
    }
    if (contains(StorageOperation::add_defaulted)) {
        result.push_back([&] {
            auto const created_function{ir.function("add_defaulted", "void")};
            ir.set_function_parameters(created_function, {count});
            ir.set_function_body(created_function,
                                 storage_operation_body(context, "add_defaulted", {"count"}));
            return created_function;
        }());
    }
    if (contains(StorageOperation::remove_at_swap)) {
        auto body{has_custom_remove_at_swap(context, members)
                      ? explicit_remove_at_swap_body(context, members)
                      : storage_operation_body(
                            context, "remove_at_swap", {"index", "count", "allow_shrinking"})};
        result.push_back([&] {
            auto const created_function{ir.function("remove_at_swap", "void")};
            ir.set_function_parameters(
                created_function, {ir.parameter("int32 const", "index"), count, allow_shrinking});
            ir.set_function_body(created_function, std::move(body));
            ir.function_flags(created_function).set_is_inline(true);
            ir.function_flags(created_function).set_define_in_header(true);
            return created_function;
        }());
    }
    if (contains(StorageOperation::set_num)) {
        result.push_back([&] {
            auto const created_function{ir.function("set_num", "void")};
            ir.set_function_parameters(created_function, {count, allow_shrinking});
            ir.set_function_body(
                created_function,
                storage_operation_body(context, "set_num", {"count", "allow_shrinking"}));
            return created_function;
        }());
    }
    if (contains(StorageOperation::copy_element)) {
        NodeListBuilder copy_one{ir};
        NodeListBuilder copy_range{ir};
        for (auto const member : members) {
            auto const destination{
                ir.named(context.text(resolved_store.name[resolved_store.index(member)]))};
            auto const source{
                ir.member_access(ir.named("other"),
                                 context.text(resolved_store.name[resolved_store.index(member)]))};
            if (context.declarations.soas
                    .copy_element_memberwise[context.declarations.soas.index(schema)]) {
                copy_one.add(ir.assignment(ir.subscript(destination, ir.named("dst_i")),
                                           ir.subscript(source, ir.named("src_i"))));
            } else {
                copy_one.add(ir.expression_statement(
                    ir.call(container_function(context, "ioj::copy_element"),
                            {destination, ir.named("dst_i"), source, ir.named("src_i")})));
            }
            copy_range.add(ir.expression_statement(ir.call(
                container_function(context, "ioj::copy_elements"),
                {destination, ir.named("dst_i"), source, ir.named("src_i"), ir.named("count")})));
        }
        result.push_back([&] {
            auto const created_function{ir.function("copy_element", "void")};
            ir.set_function_parameters(created_function, {destination_index, other, source_index});
            ir.set_function_body(created_function, copy_one.build());
            ir.function_flags(created_function).set_is_inline(true);
            ir.function_flags(created_function).set_define_in_header(true);
            ir.set_function_template(
                created_function,
                ir.template_declaration({ir.type_template_parameter(ir.strings.intern("Other"))}));
            return created_function;
        }());
        result.push_back([&] {
            auto const created_function{ir.function("copy_elements", "void")};
            ir.set_function_parameters(created_function,
                                       {destination_index, other, source_index, count});
            ir.set_function_body(created_function, copy_range.build());
            ir.function_flags(created_function).set_is_inline(true);
            ir.function_flags(created_function).set_define_in_header(true);
            ir.set_function_template(
                created_function,
                ir.template_declaration({ir.type_template_parameter(ir.strings.intern("Other"))}));
            return created_function;
        }());
        result.push_back([&] {
            auto const created_function{ir.function("copy_to_tail", "void")};
            ir.set_function_parameters(created_function, {other});
            ir.set_function_body(
                created_function,
                {
                    ir.variable(
                        "auto const", "count", ir.call(ir.member_access(ir.named("other"), "num"))),
                    ir.expression_statement(
                        ir.call(ir.named("check", {ir.types.dependency("check", "CoreMinimal.h")}),
                                {ir.binary(BinaryOperator::greater_equal,
                                           ir.call(ir.named("num")),
                                           ir.named("count"))})),
                    ir.expression_statement(ir.call(ir.named("copy_elements"),
                                                    {ir.binary(BinaryOperator::subtract,
                                                               ir.call(ir.named("num")),
                                                               ir.named("count")),
                                                     ir.named("other"),
                                                     ir.literal("0"),
                                                     ir.named("count")})),
                });
            ir.function_flags(created_function).set_is_inline(true);
            ir.function_flags(created_function).set_define_in_header(true);
            ir.set_function_template(
                created_function,
                ir.template_declaration({ir.type_template_parameter(ir.strings.intern("Other"))}));
            return created_function;
        }());
    }
    if (contains(StorageOperation::append_from)) {
        NodeListBuilder calls{ir};
        auto const member_count{members.size()};
        for (std::size_t index{}; index < member_count; ++index) {
            auto const member{members[index]};
            auto const destination{
                ir.named(context.text(resolved_store.name[resolved_store.index(member)]))};
            auto const source{
                ir.member_access(ir.named("other"),
                                 context.text(resolved_store.name[resolved_store.index(member)]))};
            auto const invocation{
                context.declarations.soa_members
                        .nested_schema[context.declarations.soa_members.index(
                            context.declarations.items(
                                context.declarations.soas
                                    .members[context.declarations.soas.index(schema)])[index])]
                    ? ir.call(ir.member_access(destination, "append_from"), {source})
                    : ir.call(container_function(context, "ioj::append_from"),
                              {destination, source})};
            calls.add(ir.expression_statement(invocation));
        }
        result.push_back([&] {
            auto const created_function{ir.function("append_from", "void")};
            ir.set_function_parameters(created_function, {other});
            ir.set_function_body(created_function, calls.build());
            ir.function_flags(created_function).set_is_inline(true);
            ir.function_flags(created_function).set_define_in_header(true);
            ir.set_function_template(
                created_function,
                ir.template_declaration({ir.type_template_parameter(ir.strings.intern("Other"))}));
            ir.set_function_requires_clause(
                created_function,
                ir.template_id(
                    ir.named("ioj::SupportsApplyArrayPairsWith",
                             {ir.types.dependency("ioj::SupportsApplyArrayPairsWith",
                                                  "ioj/soa_concepts.h")}),
                    {ir.type_reference(ir.types.intern(context.text(
                         context.declarations.soas.name[context.declarations.soas.index(schema)]))),
                     ir.type_reference(ir.types.intern("Other"))}));
            return created_function;
        }());
    }
    return result;
}

auto soa_permutation_functions(LoweringContext& context, std::span<ResolvedMemberId const>)
    -> std::vector<FunctionId> {
    auto& ir{context.ir};
    auto const scratch_indices{
        ir.parameter(ir.types.intern("TArrayView<int32>",
                                     {ir.types.dependency("TArrayView", "Containers/ArrayView.h")}),
                     "scratch_indices")};
    auto const apply_body{storage_operation_body(context, "apply_permutation", {"indices"})};
    return {
        [&] {
            auto const created_function{ir.function("apply_permutation", "void")};
            ir.set_function_parameters(
                created_function,
                {ir.parameter(
                    ir.types.intern("TArrayView<int32>",
                                    {ir.types.dependency("TArrayView", "Containers/ArrayView.h")}),
                    "indices")});
            ir.set_function_body(created_function, apply_body);
            return created_function;
        }(),
        [&] {
            auto const created_function{ir.function("sort", "void")};
            ir.set_function_parameters(created_function,
                                       {ir.parameter("Compare&&", "compare"), scratch_indices});
            ir.set_function_body(
                created_function,
                {ir.expression_statement(ir.call(
                    ir.named(
                        "ioj::soa_ops::sort",
                        {ir.types.dependency("ioj::soa_ops", "SandboxCore/soa_storage_ops.h")}),
                    {ir.unary(UnaryOperator::dereference, ir.named("this")),
                     ir.call(
                         ir.template_id(ir.named("std::forward",
                                                 {ir.types.dependency("std::forward", "utility")}),
                                        {ir.type_reference(ir.types.intern("Compare"))}),
                         {ir.named("compare")}),
                     ir.named("scratch_indices")}))});
            ir.function_flags(created_function).set_is_inline(true);
            ir.function_flags(created_function).set_define_in_header(true);
            ir.set_function_template(created_function,
                                     ir.template_declaration({ir.type_template_parameter(
                                         ir.strings.intern("Compare"))}));
            return created_function;
        }(),
        [&] {
            auto const created_function{ir.function("sort", "void")};
            ir.set_function_parameters(created_function, {scratch_indices});
            ir.set_function_body(
                created_function,
                {ir.expression_statement(ir.call(
                    ir.template_id(ir.named("ioj::soa_ops::sort",
                                            {ir.types.dependency("ioj::soa_ops",
                                                                 "SandboxCore/soa_storage_ops.h")}),
                                   {ir.named("Compare")}),
                    {ir.unary(UnaryOperator::dereference, ir.named("this")),
                     ir.named("scratch_indices")}))});
            ir.function_flags(created_function).set_is_inline(true);
            ir.function_flags(created_function).set_define_in_header(true);
            ir.set_function_template(created_function,
                                     ir.template_declaration({ir.value_template_parameter(
                                         ir.types.intern("auto"), ir.strings.intern("Compare"))}));
            return created_function;
        }(),
    };
}

auto soa_storage_node(LoweringContext& context,
                      DeclarationHandle schema,
                      std::span<ResolvedMemberId const> members,
                      std::string const& view_name,
                      std::string const& const_view_name,
                      SoaFunctions const& functions,
                      Nodes& custom_source,
                      Nodes storage_prelude,
                      std::map<IdentifierId, DeclarationHandle> const* schemas) -> NodeId {
    auto& resolved_store{context.ir.lowering.resolved_members};
    auto& ir{context.ir};
    NodeListBuilder nodes{ir};
    nodes.add(ir.using_declaration(ir.strings.intern("View"), ir.types.intern(view_name)), 1)
        .add(ir.using_declaration(ir.strings.intern("ConstView"), ir.types.intern(const_view_name)),
             2);
    nodes.append(std::move(storage_prelude));
    if (context.declarations.soas.equivalent_type[context.declarations.soas.index(schema)]
            .has_value()) {
        nodes
            .append(soa_equivalent_nodes(
                context,
                *context.declarations.soas.equivalent_type[context.declarations.soas.index(schema)],
                members))
            .new_lines(2);
    }
    auto api{lower_soa_api(
        context,
        schema,
        SoaRepresentation::vector,
        SoaReceiver::owner,
        context.text(context.declarations.soas.name[context.declarations.soas.index(schema)]),
        schemas)};
    nodes.append(std::move(api.header)).new_lines(1);
    custom_source = std::move(api.source);
    if (auto set{soa_set_function(context, schema, members, false)}; set.has_value()) {
        nodes.add(ir.header_function(*set), 2);
    }
    if (auto add{soa_add_function(context, schema, members)}; add.has_value()) {
        nodes.add(ir.header_function(*add), 2);
    }
    for (auto const spec : functions.operations) {
        nodes.add(ir.function_flags(spec).define_in_header() ? ir.header_function(spec)
                                                             : ir.declaration(spec),
                  2);
    }
    for (auto const spec : functions.permutations) {
        nodes.add(ir.function_flags(spec).define_in_header() ? ir.header_function(spec)
                                                             : ir.declaration(spec),
                  2);
    }
    nodes.append(soa_storage_view_nodes(context, members, functions.mutable_view)).new_lines(2);
    for (std::size_t index{0}; index < members.size(); ++index) {
        nodes.add(
            ir.instance_member(resolved_store.container_type[resolved_store.index(members[index])],
                               resolved_store.name[resolved_store.index(members[index])]));
        if (index + 1 < members.size()) {
            nodes.new_lines();
        }
    }
    auto const created_record{
        ir.record(context.declarations.soas.name[context.declarations.soas.index(schema)],
                  nodes.build(),
                  RecordKind::struct_)};
    ir.set_record_export_specifier(
        created_record,
        context.declarations.soas.export_specifier[context.declarations.soas.index(schema)]);
    return created_record;
}

} // namespace lispb::codegen::detail
