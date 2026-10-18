#include "declaration_emission.h"
#include "lowering.h"
#include "lowering_context.h"
#include "lowering_utils.h"
#include "soa_internal.h"

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/expr.h>
#include <codegen/ast/function_receiver.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/ast/nodes.h>
#include <codegen/ast/return_syntax.h>
#include <codegen/schema.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/soa_backend.h>
#include <codegen/schema/soa_member_kind.h>
#include <codegen/schema/soa_member_schemas.h>
#include <codegen/schema/soa_schemas.h>
#include <codegen/schema/vector_soa_schemas.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <array>
#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lispb::codegen::detail {

auto lower_vector(LoweringContext& context, DeclarationHandle vector, SoaBackend const backend)
    -> DeclarationEmission {
    auto& ir{context.ir};
    auto& declarations{context.declarations};
    auto const components{
        declarations.items(context.declarations.vector_soas
                               .components[context.declarations.vector_soas.index(vector)])};
    std::vector<SoaMemberId> members;
    for (auto const component : components) {
        auto const member{declarations.soa_members.append()};
        auto const index{declarations.soa_members.index(member)};
        declarations.soa_members.name[index] = component;
        declarations.soa_members.kind[index] = SoaMemberKind::array;
        declarations.soa_members.type[index] =
            declarations.vector_soas.value_type[declarations.vector_soas.index(vector)];
        members.push_back(member);
    }
    auto const operations{all_storage_operations()};
    auto const schema{declarations.soas.append()};
    auto const schema_index{declarations.soas.index(schema)};
    auto const vector_index{declarations.vector_soas.index(vector)};
    declarations.soas.name[schema_index] = declarations.vector_soas.name[vector_index];
    declarations.soas.members[schema_index] = declarations.range(members);
    declarations.soas.operations[schema_index] = declarations.range(operations);
    declarations.soas.export_specifier[schema_index] =
        declarations.vector_soas.export_specifier[vector_index];
    declarations.soas.equivalent_type[schema_index] =
        declarations.vector_soas.equivalent_type[vector_index];
    declarations.soas.copy_element_memberwise[schema_index] = true;
    declarations.soas.fixed[schema_index] = declarations.vector_soas.fixed[vector_index];
    std::map<IdentifierId, DeclarationHandle> const schemas{
        {declarations.soas.name[schema_index], schema}};

    std::vector<std::string> equivalent_members;
    for (auto const name : declarations.items(
             context.declarations.vector_soas
                 .equivalent_members[context.declarations.vector_soas.index(vector)])) {
        equivalent_members.push_back(context.text(name));
    }
    if (backend == SoaBackend::standard_library) {
        auto lowered{lower_native_soa(
            context,
            schema,
            schemas,
            true,
            equivalent_members,
            context.declarations.vector_soas
                .equivalent_constructor[context.declarations.vector_soas.index(vector)]
                .value_or(StringId{}),
            context.declarations.vector_soas
                .equivalent_accessors[context.declarations.vector_soas.index(vector)])};
        return {.header = std::move(lowered.header), .format_generated = true};
    }
    if (equivalent_members.empty()) {
        equivalent_members = {"X", "Y", "Z"};
    }

    auto const value_type{
        context.resolve(context.declarations.vector_soas
                            .value_type[context.declarations.vector_soas.index(vector)])};
    auto const equivalent_type{
        context.resolve(context.declarations.vector_soas
                            .equivalent_type[context.declarations.vector_soas.index(vector)])};
    auto const index_type{ir.types.intern("int32 const")};
    std::vector<Expr> arguments;
    for (std::size_t index{}; index < components.size(); ++index) {
        auto value{ir.member_access(ir.named("value"), equivalent_members[index])};
        if (context.declarations.vector_soas
                .equivalent_accessors[context.declarations.vector_soas.index(vector)]) {
            value = ir.call(value);
        }
        arguments.push_back(value);
    }
    auto equivalent_set = [&](bool const is_const) {
        std::vector<Expr> set_arguments{ir.named("i")};
        set_arguments.insert(set_arguments.end(), arguments.begin(), arguments.end());
        auto const created_function{ir.function("set", "void")};
        ir.set_function_parameters(created_function,
                                   {ir.parameter(index_type, "i"),
                                    ir.parameter(qualify(ir, equivalent_type, " const"), "value")});
        ir.set_function_body(created_function,
                             {ir.expression_statement(ir.call(ir.named("set"), set_arguments))});
        ir.set_function_receiver(
            created_function, is_const ? FunctionReceiver::const_ : FunctionReceiver::unqualified);
        ir.function_flags(created_function).set_is_inline(true);
        ir.function_flags(created_function).set_define_in_header(true);
        return ir.header_function(created_function);
    };

    NodeListBuilder prelude{ir};
    for (bool const immutable : {false, true}) {
        NodeListBuilder fields{ir};
        std::vector<Expr> pointers;
        for (auto const component : components) {
            fields.add(ir.instance_member(qualify(ir, value_type, immutable ? " const*" : "*"),
                                          component));
            pointers.push_back(ir.call(ir.member_access(ir.named(component), "GetData")));
        }
        std::string const name{immutable ? "ConstData" : "Data"};
        prelude.add(ir.record(ir.strings.intern(name), fields.build(), RecordKind::struct_), 2);
        prelude.add(ir.header_function([&] {
            auto const created_function{ir.function("get_data", "auto")};
            ir.set_function_body(
                created_function,
                {ir.return_statement(ir.init_list(pointers, ir.types.intern(name)))});
            ir.set_function_return_type(
                created_function, ir.types.intern(name), ReturnSyntax::trailing);
            ir.set_function_receiver(created_function,
                                     immutable ? FunctionReceiver::const_
                                               : FunctionReceiver::unqualified);
            ir.function_flags(created_function).set_is_inline(true);
            ir.function_flags(created_function).set_define_in_header(true);
            return created_function;
        }()),
                    1);
    }
    prelude.add(ir.using_declaration(ir.strings.intern("value_type"), value_type), 1)
        .add(ir.using_declaration(ir.strings.intern("size_type"),
                                  ir.types.intern("TArray<value_type>::SizeType")),
             1)
        .add(ir.header_function([&] {
            auto const created_function{ir.function("add", "auto")};
            ir.set_function_parameters(
                created_function, {ir.parameter(qualify(ir, equivalent_type, " const&"), "value")});
            ir.set_function_body(created_function,
                                 {ir.return_statement(ir.call(ir.named("add"), arguments))});
            ir.set_function_return_type(
                created_function, ir.types.intern("size_type"), ReturnSyntax::trailing);
            ir.function_flags(created_function).set_is_inline(true);
            ir.function_flags(created_function).set_define_in_header(true);
            return created_function;
        }()),
             1)
        .add(equivalent_set(false), 1);

    for (auto const& [name, method] :
         std::array{std::pair{"empty", "Empty"},
                    std::pair{"set_num_uninitialised", "SetNumUninitialized"},
                    std::pair{"add_zeroed", "AddZeroed"}}) {
        bool const has_count{std::string_view{name} != "empty"};
        std::vector<Expr> call_arguments;
        std::vector<FunctionParameterId> parameters;
        if (has_count) {
            call_arguments.push_back(ir.named("count"));
            parameters.push_back(ir.parameter("size_type const", "count"));
        }
        NodeListBuilder body{ir};
        for (auto const component : components) {
            body.add(ir.expression_statement(
                ir.call(ir.member_access(ir.named(component), method), call_arguments)));
        }
        prelude.add(ir.header_function([&] {
            auto const created_function{ir.function(name, "void")};
            ir.set_function_parameters(created_function, std::move(parameters));
            ir.set_function_body(created_function, body.build());
            ir.function_flags(created_function).set_is_inline(true);
            ir.function_flags(created_function).set_define_in_header(true);
            return created_function;
        }()),
                    1);
    }
    auto const aliases{
        Nodes{ir.using_declaration(ir.strings.intern("value_type"), value_type),
              ir.using_declaration(ir.strings.intern("size_type"),
                                   ir.types.intern("TArray<value_type>::SizeType"))}};
    auto lowered{lower_soa(context, schema, prelude.build(), {equivalent_set(true)}, aliases)};
    if (declarations.soas.fixed[schema_index]) {
        NodeListBuilder header{ir};
        header.append(std::move(lowered.header))
            .new_lines(2)
            .append(lower_fixed_nodes(context, schema, schemas, equivalent_type));
        lowered.header = header.build();
    }
    return {.header = std::move(lowered.header), .source = std::move(lowered.source)};
}

} // namespace lispb::codegen::detail
