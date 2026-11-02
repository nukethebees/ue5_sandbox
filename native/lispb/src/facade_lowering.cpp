#include "declaration_emission.h"
#include "lowering.h"
#include "lowering_context.h"
#include "lowering_utils.h"

#include <codegen/ast/access.h>
#include <codegen/ast/access_indentation.h>
#include <codegen/ast/cpp_ir_builder.h>
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
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace lispb::codegen::detail {
namespace {

class FacadeLowering : public CppIRBuilder {
  public:
    explicit FacadeLowering(CppIR& ir)
        : CppIRBuilder{ir} {}
    auto lower_facade(LoweringContext& ctx, DeclarationHandle facade) -> DeclarationEmission {
        auto const& facades{ctx.declarations.facades};
        auto const& method_schemas{ctx.declarations.facade_methods};
        auto const& parameter_schemas{ctx.declarations.parameters};
        auto const row{facades.index(facade)};
        auto const& decls{ctx.declarations};
        auto const target_type{ctx.resolve(facades.target_type[row])};
        auto const definitions_in_source{facades.definitions_in_source[row]};
        auto const reference_target{facades.reference_target[row]};
        auto const target_name{facades.target_member_name[row]};
        FunctionDefinition const binding{
            .name = named(reference_target ? facades.name[row] : store_text("bind")),
            .return_type = reference_target ? CppTypeId{} : std_types().void_,
            .kind = reference_target ? FunctionKind::constructor : FunctionKind::ordinary,
            .parameters = {parameter(decorate_type(target_type, "", "&"), "new_target")},
            .implementation =
                reference_target
                    ? FunctionImplementation{.kind = FunctionImplementationKind::body,
                                             .initializers = {member_initializer(
                                                 target_name, named("new_target"))}}
                    : FunctionImplementation{.kind = FunctionImplementationKind::body,
                                             .body = {assignment(named(target_name),
                                                                 unary(UnaryOperator::address_of,
                                                                       named("new_target")))}},
            .flags = {.is_inline = !definitions_in_source,
                      .define_in_header = !definitions_in_source}};
        std::vector<FunctionId> methods;
        for (auto const method : decls.items(facades.methods[row])) {
            auto const method_row{method_schemas.index(method)};
            std::vector<FunctionParameterId> parameters;
            std::vector<Expr> arguments;
            for (auto const parameter_id : decls.items(method_schemas.parameters[method_row])) {
                auto const parameter_row{parameter_schemas.index(parameter_id)};
                auto type{ctx.resolve(parameter_schemas.type[parameter_row])};
                auto const is_pack{parameter_schemas.is_pack[parameter_row]};
                if (is_pack) {
                    type = decorate_type(type, "", "...");
                }
                auto const name{parameter_schemas.name[parameter_row]};
                parameters.push_back(
                    parameter(type,
                              name,
                              parameter_schemas.default_value[parameter_row]
                                  ? raw_expr(*parameter_schemas.default_value[parameter_row])
                                  : Expr{}));
                auto const argument{named(name)};
                arguments.push_back(is_pack ? unary(UnaryOperator::pack_expansion, argument)
                                            : argument);
            }
            std::vector<TypeDependencyId> validation_dependencies;
            for (auto const key : decls.items(facades.validation_dependencies[row])) {
                append_type_dependencies(validation_dependencies, ctx.text(key), ctx.types);
            }
            auto body{node_list()};
            if (!facades.validation_lines[row].empty()) {
                body.add(raw_lines(decls.items(facades.validation_lines[row])));
            }
            auto const target{named(target_name)};
            auto const method_name{
                method_schemas.target_name[method_row].value_or(method_schemas.name[method_row])};
            auto member{target};
            bool pointer{!reference_target};
            for (auto const component : ctx.text(method_name) | std::views::split('.')) {
                auto const name{std::string_view{component.begin(), component.end()}};
                member =
                    pointer ? pointer_member_access(member, name) : member_access(member, name);
                pointer = false;
            }
            auto const call_expr{call(member, std::move(arguments))};
            auto const return_type{ctx.resolve(method_schemas.return_type[method_row])};
            if (type_spelling(return_type) == "void") {
                body.add(expr_stmt(call_expr, std::move(validation_dependencies)));
            } else {
                body.add(return_stmt(call_expr, std::move(validation_dependencies)));
            }
            FunctionDefinition const function{
                .name = named(method_schemas.name[method_row]),
                .return_type = return_type,
                .parameters = std::move(parameters),
                .implementation = {.kind = FunctionImplementationKind::body, .body = body.build()},
                .exception_specification = {.kind =
                                                method_schemas.is_noexcept[method_row]
                                                    ? FunctionExceptionKind::noexcept_
                                                    : FunctionExceptionKind::potentially_throwing},
                .receiver = method_schemas.is_const[method_row] ? FunctionReceiver::const_
                                                                : FunctionReceiver::unqualified,
                .flags = {.is_inline = !definitions_in_source,
                          .define_in_header = !definitions_in_source}};
            methods.push_back(add_function(function));
        }

        auto const binding_id{add_function(binding)};
        auto public_nodes{node_list()};
        auto protected_nodes{node_list()};
        auto private_nodes{node_list()};
        auto section = [&](Access visibility) -> NodeListBuilder& {
            switch (visibility) {
                case Access::public_:
                    return public_nodes;
                case Access::protected_:
                    return protected_nodes;
                case Access::private_:
                    return private_nodes;
            }
            std::unreachable();
        };
        auto add_method = [&](Access visibility, FunctionId spec) {
            section(visibility).add(header_function(spec), 2);
        };
        add_method(facades.bind_access[row], binding_id);
        auto const schemas{decls.items(facades.methods[row])};
        auto const count{methods.size()};
        for (std::size_t i{}; i < count; ++i) {
            add_method(method_schemas.access[schemas[i]].value_or(facades.method_access[row]),
                       methods[i]);
        }
        for (auto const friend_name : decls.items(facades.friends[row])) {
            private_nodes.add(
                friend_declaration(store_text(ctx.text(friend_name)), facades.friend_kind[row]), 1);
        }
        auto class_nodes{node_list()};
        std::optional<Access> last_access;
        for (auto const visibility : {Access::public_, Access::protected_, Access::private_}) {
            auto const nodes{section(visibility).build()};
            if (!nodes.empty()) {
                class_nodes.add(access(visibility, AccessIndentation::normal), 1).append(nodes);
                last_access = visibility;
            }
        }
        if (last_access != facades.target_access[row]) {
            class_nodes.add(access(facades.target_access[row], AccessIndentation::normal), 1);
        }
        class_nodes.add(reference_target
                            ? instance_member(decorate_type(target_type, "", "&"), target_name)
                            : instance_member(decorate_type(target_type, "", "*"),
                                              target_name,
                                              literal("nullptr")));

        auto const declaration_node{
            record(facades.name[row], class_nodes.build(), RecordKind::class_)};
        set_record_export_specifier(declaration_node, facades.export_specifier[row]);
        auto definitions{node_list()};
        if (definitions_in_source) {
            auto const owner{intern_type(ctx.text(facades.name[row]))};
            definitions.add(definition(binding_id, owner));
            for (auto const method : methods) {
                definitions.new_lines(2).add(definition(method, owner));
            }
        }
        return {.header = {std::move(declaration_node)}, .source = definitions.build()};
    }
};

} // namespace

auto lower_facade(LoweringContext& ctx, DeclarationHandle facade) -> DeclarationEmission {
    return FacadeLowering{ctx.ir}.lower_facade(ctx, facade);
}

} // namespace lispb::codegen::detail
