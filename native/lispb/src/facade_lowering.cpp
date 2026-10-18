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
#include <codegen/schema/declarations.h>
#include <codegen/schema/facade_method_schemas.h>
#include <codegen/schema/facade_schemas.h>
#include <codegen/schema/parameter_schemas.h>
#include <codegen/schema/type_registry.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <string>
#include <utility>
#include <vector>

namespace lispb::codegen::detail {
namespace {

auto lower_facade_impl(LoweringContext& context, DeclarationHandle facade) -> DeclarationEmission {
    auto& ir{context.ir};
    auto const& declarations{context.declarations};
    auto target_type{context.resolve(
        context.declarations.facades.target_type[context.declarations.facades.index(facade)])};
    auto const definitions_in_source{
        context.declarations.facades
            .definitions_in_source[context.declarations.facades.index(facade)]};
    auto const reference_target{
        declarations.facades.reference_target[declarations.facades.index(facade)]};
    auto const binding{
        reference_target
            ? ir.constructor(declarations.facades.name[declarations.facades.index(facade)])
            : ir.function("bind", "void")};
    ir.set_function_parameters(binding,
                               {ir.parameter(qualify(ir, target_type, "&"), "new_target")});
    ir.function_flags(binding).set_is_inline(!definitions_in_source);
    ir.function_flags(binding).set_define_in_header(!definitions_in_source);
    if (reference_target) {
        ir.set_function_body(
            binding,
            {},
            {ir.member_initializer(
                declarations.facades.target_member_name[declarations.facades.index(facade)],
                ir.strings.add("new_target"))});
    } else {
        ir.set_function_body(
            binding,
            {ir.assignment(
                ir.named(
                    declarations.facades.target_member_name[declarations.facades.index(facade)]),
                ir.raw_expression("&new_target"))});
    }
    std::vector<FunctionId> methods;
    for (auto const method_id : declarations.items(
             context.declarations.facades.methods[context.declarations.facades.index(facade)])) {
        auto const method{method_id};
        std::vector<FunctionParameterId> parameters;
        std::vector<Expr> arguments;
        for (auto const parameter_id :
             declarations.items(declarations.facade_methods
                                    .parameters[declarations.facade_methods.index(method)])) {
            auto const parameter{parameter_id};
            auto type{context.resolve(
                declarations.parameters.type[declarations.parameters.index(parameter)])};
            if (declarations.parameters.default_value[declarations.parameters.index(parameter)]
                    .has_value()) {
                parameters.push_back(ir.parameter(
                    type,
                    declarations.parameters.name[declarations.parameters.index(parameter)],
                    *declarations.parameters
                         .default_value[declarations.parameters.index(parameter)]));
            } else {
                parameters.push_back(ir.parameter(
                    type, declarations.parameters.name[declarations.parameters.index(parameter)]));
            }
            arguments.push_back(
                ir.named(declarations.parameters.name[declarations.parameters.index(parameter)]));
        }
        std::vector<TypeDependencyId> validation_dependencies;
        for (auto const key : declarations.items(
                 context.declarations.facades
                     .validation_dependencies[context.declarations.facades.index(facade)])) {
            append_type_dependencies(validation_dependencies, context.text(key), context.types);
        }
        NodeListBuilder body{ir};
        if (!context.declarations.facades
                 .validation_lines[context.declarations.facades.index(facade)]
                 .empty()) {
            body.add(ir.raw_lines(declarations.items(
                context.declarations.facades
                    .validation_lines[context.declarations.facades.index(facade)])));
        }
        auto const target{
            ir.named(context.declarations.facades
                         .target_member_name[context.declarations.facades.index(facade)])};
        auto const member{
            context.declarations.facades
                    .reference_target[context.declarations.facades.index(facade)]
                ? ir.member_access(
                      target,
                      declarations.facade_methods
                          .target_name[declarations.facade_methods.index(method)]
                          .value_or(declarations.facade_methods
                                        .name[declarations.facade_methods.index(method)]))
                : ir.pointer_member_access(
                      target,
                      declarations.facade_methods
                          .target_name[declarations.facade_methods.index(method)]
                          .value_or(declarations.facade_methods
                                        .name[declarations.facade_methods.index(method)]))};
        auto const call{ir.call(member, std::move(arguments))};
        auto const return_type{context.resolve(
            declarations.facade_methods.return_type[declarations.facade_methods.index(method)])};
        if (ir.types.spelling(return_type) == "void") {
            body.add(ir.expression_statement(call, std::move(validation_dependencies)));
        } else {
            body.add(ir.return_statement(call, std::move(validation_dependencies)));
        }
        methods.push_back([&] {
            auto const created_function{ir.function(
                declarations.facade_methods.name[declarations.facade_methods.index(method)],
                return_type)};
            ir.set_function_parameters(created_function, std::move(parameters));
            ir.set_function_body(created_function, body.build());
            ir.set_function_receiver(
                created_function,
                declarations.facade_methods.is_const[declarations.facade_methods.index(method)]
                    ? FunctionReceiver::const_
                    : FunctionReceiver::unqualified);
            if (declarations.facade_methods
                    .is_noexcept[declarations.facade_methods.index(method)]) {
                ir.set_function_noexcept(created_function);
            }
            ir.function_flags(created_function).set_is_inline(!definitions_in_source);
            ir.function_flags(created_function).set_define_in_header(!definitions_in_source);
            return created_function;
        }());
    }

    NodeListBuilder public_nodes{ir};
    NodeListBuilder private_nodes{ir};
    bool has_public_nodes{};
    auto add_method = [&](Access access, FunctionId spec) {
        auto& destination{access == Access::public_ ? public_nodes : private_nodes};
        destination.add(ir.header_function(spec), 2);
        has_public_nodes = has_public_nodes || access == Access::public_;
    };
    add_method(context.declarations.facades.bind_access[context.declarations.facades.index(facade)],
               binding);
    for (auto const& method : methods) {
        add_method(
            context.declarations.facades.method_access[context.declarations.facades.index(facade)],
            method);
    }
    for (auto const friend_name : declarations.items(
             context.declarations.facades.friends[context.declarations.facades.index(facade)])) {
        private_nodes.add(
            ir.friend_declaration(ir.strings.intern(context.text(friend_name)),
                                  context.declarations.facades
                                      .friend_kind[context.declarations.facades.index(facade)]),
            1);
    }
    private_nodes.add(
        context.declarations.facades.reference_target[context.declarations.facades.index(facade)]
            ? ir.instance_member(
                  qualify(ir, target_type, "&"),
                  context.declarations.facades
                      .target_member_name[context.declarations.facades.index(facade)])
            : ir.instance_member(
                  qualify(ir, target_type, "*"),
                  context.declarations.facades
                      .target_member_name[context.declarations.facades.index(facade)],
                  ir.raw_expression("nullptr")));

    NodeListBuilder class_nodes{ir};
    if (has_public_nodes) {
        class_nodes.add(ir.access(Access::public_, AccessIndentation::normal), 1)
            .append(public_nodes.build());
    }
    class_nodes.add(ir.access(Access::private_, AccessIndentation::normal), 1)
        .append(private_nodes.build());

    auto const declaration_node{[&] {
        auto const created_record{
            ir.record(context.declarations.facades.name[context.declarations.facades.index(facade)],
                      class_nodes.build(),
                      RecordKind::class_)};
        ir.set_record_export_specifier(
            created_record,
            context.declarations.facades
                .export_specifier[context.declarations.facades.index(facade)]);
        return created_record;
    }()};
    NodeListBuilder definitions{ir};
    if (context.declarations.facades
            .definitions_in_source[context.declarations.facades.index(facade)]) {
        definitions.add(ir.definition(
            binding,
            string_id(
                context.declarations.facades.name[context.declarations.facades.index(facade)])));
        for (auto const& method : methods) {
            definitions.new_lines(2).add(
                ir.definition(method,
                              string_id(context.declarations.facades
                                            .name[context.declarations.facades.index(facade)])));
        }
    }
    return {.header = {std::move(declaration_node)}, .source = definitions.build()};
}

} // namespace

auto lower_facade(LoweringContext& context, DeclarationHandle schema) -> DeclarationEmission {
    return lower_facade_impl(context, schema);
}

} // namespace lispb::codegen::detail
