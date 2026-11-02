#include "declaration_emission.h"
#include "lowering.h"
#include "lowering_context.h"
#include "lowering_utils.h"

#include <codegen/ast/access.h>
#include <codegen/ast/access_indentation.h>
#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/expr.h>
#include <codegen/ast/function_receiver.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/node_id.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/ast/nodes.h>
#include <codegen/ast/record_kind.h>
#include <codegen/ast/unary_operator.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/facade_method_schemas.h>
#include <codegen/schema/facade_schemas.h>
#include <codegen/schema/parameter_schemas.h>
#include <codegen/schema/type_registry.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace lispb::codegen::detail {

auto lower_facade(LoweringContext& ctx, DeclarationHandle facade) -> DeclarationEmission {
    auto& ir{ctx.ir};
    auto const& facades{ctx.declarations.facades};
    auto const& method_schemas{ctx.declarations.facade_methods};
    auto const& parameter_schemas{ctx.declarations.parameters};
    auto const row{facades.index(facade)};
    auto const& decls{ctx.declarations};
    auto const target_type{ctx.resolve(facades.target_type[row])};
    auto const definitions_in_source{facades.definitions_in_source[row]};
    auto const reference_target{facades.reference_target[row]};
    auto const target_name{facades.target_member_name[row]};
    auto const binding{reference_target ? ir.constructor(facades.name[row])
                                        : ir.function("bind", ir.types.std.void_)};
    ir.set_function_parameters(binding,
                               {ir.parameter(qualify(ir, target_type, "&"), "new_target")});
    ir.function_flags(binding).set_is_inline(!definitions_in_source);
    ir.function_flags(binding).set_define_in_header(!definitions_in_source);
    if (reference_target) {
        ir.set_function_body(
            binding, {}, {ir.member_initializer(target_name, ir.named("new_target"))});
    } else {
        ir.set_function_body(
            binding,
            {ir.assignment(ir.named(target_name),
                           ir.unary(UnaryOperator::address_of, ir.named("new_target")))});
    }
    std::vector<FunctionId> methods;
    for (auto const method : decls.items(facades.methods[row])) {
        auto const method_row{method_schemas.index(method)};
        std::vector<FunctionParameterId> parameters;
        std::vector<Expr> arguments;
        for (auto const parameter : decls.items(method_schemas.parameters[method_row])) {
            auto const parameter_row{parameter_schemas.index(parameter)};
            auto const type{ctx.resolve(parameter_schemas.type[parameter_row])};
            auto const name{parameter_schemas.name[parameter_row]};
            parameters.push_back(
                ir.parameter(type,
                             name,
                             parameter_schemas.default_value[parameter_row]
                                 ? ir.raw_expr(*parameter_schemas.default_value[parameter_row])
                                 : Expr{}));
            arguments.push_back(ir.named(name));
        }
        std::vector<TypeDependencyId> validation_dependencies;
        for (auto const key : decls.items(facades.validation_dependencies[row])) {
            append_type_dependencies(validation_dependencies, ctx.text(key), ctx.types);
        }
        NodeListBuilder body{ir};
        if (!facades.validation_lines[row].empty()) {
            body.add(ir.raw_lines(decls.items(facades.validation_lines[row])));
        }
        auto const target{ir.named(target_name)};
        auto const method_name{
            method_schemas.target_name[method_row].value_or(method_schemas.name[method_row])};
        auto const member{reference_target ? ir.member_access(target, method_name)
                                           : ir.pointer_member_access(target, method_name)};
        auto const call{ir.call(member, std::move(arguments))};
        auto const return_type{ctx.resolve(method_schemas.return_type[method_row])};
        if (ir.types.spelling(return_type) == "void") {
            body.add(ir.expr_stmt(call, std::move(validation_dependencies)));
        } else {
            body.add(ir.return_stmt(call, std::move(validation_dependencies)));
        }
        auto const function{ir.function(method_schemas.name[method_row], return_type)};
        ir.set_function_parameters(function, std::move(parameters));
        ir.set_function_body(function, body.build());
        ir.set_function_receiver(function,
                                 method_schemas.is_const[method_row]
                                     ? FunctionReceiver::const_
                                     : FunctionReceiver::unqualified);
        if (method_schemas.is_noexcept[method_row]) {
            ir.set_function_noexcept(function);
        }
        ir.function_flags(function).set_is_inline(!definitions_in_source);
        ir.function_flags(function).set_define_in_header(!definitions_in_source);
        methods.push_back(function);
    }

    NodeListBuilder public_nodes{ir};
    NodeListBuilder protected_nodes{ir};
    NodeListBuilder private_nodes{ir};
    auto section = [&](Access access) -> NodeListBuilder& {
        switch (access) {
            case Access::public_:
                return public_nodes;
            case Access::protected_:
                return protected_nodes;
            case Access::private_:
                return private_nodes;
        }
        std::unreachable();
    };
    auto add_method = [&](Access access, FunctionId spec) {
        section(access).add(ir.header_function(spec), 2);
    };
    add_method(facades.bind_access[row], binding);
    auto const schemas{decls.items(facades.methods[row])};
    auto const count{methods.size()};
    for (std::size_t i{}; i < count; ++i) {
        add_method(method_schemas.access[schemas[i]].value_or(facades.method_access[row]),
                   methods[i]);
    }
    for (auto const friend_name : decls.items(facades.friends[row])) {
        private_nodes.add(ir.friend_declaration(ir.strings.intern(ctx.text(friend_name)),
                                                facades.friend_kind[row]),
                          1);
    }
    NodeListBuilder class_nodes{ir};
    std::optional<Access> last_access;
    for (auto const access : {Access::public_, Access::protected_, Access::private_}) {
        auto const nodes{section(access).build()};
        if (!nodes.empty()) {
            class_nodes.add(ir.access(access, AccessIndentation::normal), 1).append(nodes);
            last_access = access;
        }
    }
    if (last_access != facades.target_access[row]) {
        class_nodes.add(ir.access(facades.target_access[row], AccessIndentation::normal), 1);
    }
    class_nodes.add(reference_target
                        ? ir.instance_member(qualify(ir, target_type, "&"), target_name)
                        : ir.instance_member(
                              qualify(ir, target_type, "*"), target_name, ir.literal("nullptr")));

    auto const declaration_node{
        ir.record(facades.name[row], class_nodes.build(), RecordKind::class_)};
    ir.set_record_export_specifier(declaration_node, facades.export_specifier[row]);
    NodeListBuilder definitions{ir};
    if (definitions_in_source) {
        auto const owner{ir.types.intern(ctx.text(facades.name[row]))};
        definitions.add(ir.definition(binding, owner));
        for (auto const method : methods) {
            definitions.new_lines(2).add(ir.definition(method, owner));
        }
    }
    return {.header = {std::move(declaration_node)}, .source = definitions.build()};
}

} // namespace lispb::codegen::detail
