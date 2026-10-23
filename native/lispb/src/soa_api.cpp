#include "soa_api.h"

#include "lowered_soa.h"
#include "lowering_context.h"
#include "lowering_utils.h"
#include "soa_body.h"
#include "soa_receiver.h"
#include "soa_representation.h"

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/function_receiver.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/ast/return_syntax.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/function_schemas.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/physical_type_use.h>
#include <codegen/schema/schema_range.h>
#include <codegen/schema/soa_backend.h>
#include <codegen/schema/soa_member_kind.h>
#include <codegen/schema/soa_member_schemas.h>
#include <codegen/schema/soa_schemas.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace lispb::codegen::detail {
auto uses_native_vector_view(LoweringContext& context, DeclarationHandle schema) -> bool {
    if (context.declarations.soas.equivalent_type[context.declarations.soas.index(schema)]) {
        auto const spelling{context.ir.types.spelling(context.resolve(
            *context.declarations.soas.equivalent_type[context.declarations.soas.index(schema)]))};
        if (spelling != "Vector3f") {
            return false;
        }
    }
    if (context.declarations.soas.members[context.declarations.soas.index(schema)].size() != 3 ||
        !context.declarations.soas.const_view_functions[context.declarations.soas.index(schema)]
             .empty() ||
        !context.declarations.soas.mutable_view_functions[context.declarations.soas.index(schema)]
             .empty() ||
        !context.declarations.soas.using_declarations[context.declarations.soas.index(schema)]
             .empty() ||
        context.declarations.soas.export_specifier[context.declarations.soas.index(schema)].value !=
            0) {
        return false;
    }
    constexpr std::array<std::string_view, 3> components{"xs", "ys", "zs"};
    for (std::size_t index{}; index < components.size(); ++index) {
        auto const member{context.declarations.items(
            context.declarations.soas.members[context.declarations.soas.index(schema)])[index]};
        if (context.declarations.soa_members.kind[context.declarations.soa_members.index(member)] !=
                SoaMemberKind::array ||
            context.declarations.strings().view(
                context.declarations.soa_members
                    .name[context.declarations.soa_members.index(member)]) != components[index] ||
            native_spelling(std::string{context.ir.types.spelling(context.resolve(
                context.declarations.soa_members
                    .type[context.declarations.soa_members.index(member)]))}) != "float") {
            return false;
        }
    }
    return true;
}

auto logical_column_access(LoweringContext& context,
                           DeclarationHandle schema,
                           std::span<std::string const> const path,
                           SoaRepresentation const representation,
                           std::string receiver,
                           std::map<StringId, DeclarationHandle> const* schemas) -> std::string {
    if (path.empty()) {
        throw std::invalid_argument{"SOA column reference cannot be empty"};
    }
    auto const* current{&schema};
    for (std::size_t index{}; index < path.size(); ++index) {
        if (!receiver.empty() && !receiver.ends_with("->")) {
            receiver += '.';
        }
        auto const members{context.declarations.items(
            context.declarations.soas.members[context.declarations.soas.index(*current)])};
        auto const found{std::ranges::find(members, path[index], [&](auto id) {
            return context.declarations.strings().view(
                context.declarations.soa_members.name[context.declarations.soa_members.index(id)]);
        })};
        if (found == members.end()) {
            throw std::invalid_argument{
                "SOA '" +
                context.text(
                    context.declarations.soas.name[context.declarations.soas.index(*current)]) +
                "' has no column '" + path[index] + "'"};
        }
        auto const member{*found};
        bool const nested{
            context.declarations.soa_members.kind[context.declarations.soa_members.index(member)] ==
            SoaMemberKind::nested};
        receiver += representation == SoaRepresentation::compact && nested ? "view_" : "";
        receiver += path[index];
        if (representation == SoaRepresentation::compact ||
            (representation == SoaRepresentation::native_vector_view &&
             uses_native_vector_view(context, *current))) {
            receiver += "()";
        }
        if (index + 1 < path.size()) {
            if (!nested ||
                !context.declarations.soa_members
                     .nested_schema[context.declarations.soa_members.index(member)] ||
                !schemas) {
                throw std::invalid_argument{"SOA column path requires a resolved nested schema: " +
                                            path[index]};
            }
            current =
                &schemas->at(*context.declarations.soa_members
                                  .nested_schema[context.declarations.soa_members.index(member)]);
        }
    }
    return receiver;
}

namespace {
auto compact_row_api(LoweringContext& context,
                     DeclarationHandle schema,
                     SoaReceiver receiver,
                     std::map<StringId, DeclarationHandle> const& schemas) -> Nodes {
    auto& ir{context.ir};
    auto const& declarations{context.declarations};
    auto const& soas{declarations.soas};
    auto const& members{declarations.soa_members};
    NodeListBuilder result{ir};
    auto const owner{receiver == SoaReceiver::owner};
    auto const mutable_view{receiver == SoaReceiver::mutable_view};
    auto const functions{owner ? soas.functions[soas.index(schema)]
                               : soas.mutable_view_functions[soas.index(schema)]};
    auto custom = [&](std::string_view name) {
        return std::ranges::any_of(declarations.items(functions), [&](auto function) {
            return context.text(
                       declarations.functions.name[declarations.functions.index(function)]) == name;
        });
    };
    auto emit = [&](FunctionId function) {
        ir.function_flags(function).set_define_in_header(true);
        ir.function_flags(function).set_is_inline(true);
        result.add(ir.header_function(function)).new_lines(1);
    };

    if (owner) {
        for (auto member : declarations.items(soas.members[soas.index(schema)])) {
            auto const row{members.index(member)};
            auto const name{(members.kind[row] == SoaMemberKind::nested ? "view_" : "") +
                            context.text(members.name[row])};
            result
                .add(ir.raw("auto " + name + "(this auto& self) { return self.get_view()." + name +
                            "(); }"))
                .new_lines(1);
        }
    }
    if (!owner && !mutable_view) {
        return result.build();
    }

    std::vector<FunctionParameterId> parameters;
    std::vector<Expr> arguments{ir.named("index")};
    std::vector<Expr> equivalent_arguments;
    auto mapped{soas.equivalent_type[soas.index(schema)].has_value()};
    NodeListBuilder assignments{ir};
    assignments.add(ir.raw("assert(index < num());"));
    auto collect = [&](auto&& self,
                       DeclarationHandle current,
                       std::vector<std::string> path,
                       std::string value) -> void {
        for (auto member : declarations.items(soas.members[soas.index(current)])) {
            auto const row{members.index(member)};
            auto column{path};
            column.push_back(context.text(members.name[row]));
            auto component{value};
            if (members.equivalent_member[row]) {
                component += "." + context.text(*members.equivalent_member[row]);
                if (members.equivalent_accessor[row]) {
                    component += "()";
                }
            } else {
                mapped = false;
            }
            if (members.kind[row] == SoaMemberKind::nested) {
                self(self, schemas.at(*members.nested_schema[row]), column, component);
                continue;
            }
            auto const type{context.resolve(members.type[row])};
            auto const name{"new_" + join(column, "_")};
            parameters.push_back(ir.parameter(
                ir.types.derive(type,
                                native_spelling(std::string{ir.types.spelling(type)}) + " const"),
                name));
            arguments.push_back(ir.named(name));
            equivalent_arguments.push_back(ir.raw_expr(component));
            assignments.add(ir.raw(logical_column_access(context,
                                                         schema,
                                                         column,
                                                         SoaRepresentation::compact,
                                                         owner ? "get_view()" : "this->",
                                                         &schemas) +
                                   "[index] = " + name + ";"));
        }
    };
    collect(collect, schema, {}, "value");
    if (!custom("set")) {
        auto const setter{ir.function("set", "void")};
        auto setter_parameters{parameters};
        setter_parameters.insert(setter_parameters.begin(), ir.parameter("size_type", "index"));
        ir.set_function_parameters(setter, setter_parameters);
        ir.set_function_body(setter, assignments.build());
        ir.set_function_receiver(
            setter, mutable_view ? FunctionReceiver::const_ : FunctionReceiver::unqualified);
        emit(setter);
    }
    if (owner && !custom("add") && !custom("set")) {
        auto const add{ir.function("add", "size_type")};
        ir.set_function_parameters(add, parameters);
        ir.set_function_body(add,
                             {ir.raw("auto const index{num()};\nadd_uninitialised(1);"),
                              ir.expr_stmt(ir.call(ir.named("set"), arguments)),
                              ir.return_stmt(ir.named("index"))});
        emit(add);
    }
    if (mapped && !custom("set")) {
        auto const equivalent{context.resolve(*soas.equivalent_type[soas.index(schema)])};
        auto const value{ir.parameter(ir.types.decorate(equivalent, "", " const"), "value")};
        auto const setter{ir.function("set", "void")};
        ir.set_function_parameters(setter, {ir.parameter("size_type", "index"), value});
        auto set_arguments{equivalent_arguments};
        set_arguments.insert(set_arguments.begin(), ir.named("index"));
        ir.set_function_body(setter, {ir.expr_stmt(ir.call(ir.named("set"), set_arguments))});
        ir.set_function_receiver(
            setter, mutable_view ? FunctionReceiver::const_ : FunctionReceiver::unqualified);
        emit(setter);
        if (owner && !custom("add")) {
            auto const add{ir.function("add", "size_type")};
            ir.set_function_parameters(add, {value});
            ir.set_function_body(add,
                                 {ir.return_stmt(ir.call(ir.named("add"), equivalent_arguments))});
            emit(add);
        }
    }
    return result.build();
}

auto expand_columns(LoweringContext& context,
                    std::string_view const body,
                    DeclarationHandle schema,
                    SoaRepresentation const representation,
                    SoaReceiver const receiver,
                    std::map<StringId, DeclarationHandle> const* schemas) -> std::string {
    std::string result;
    result.reserve(body.size());
    std::size_t copied{};
    scan_soa_columns(body,
                     context.declarations.strings().view(
                         context.declarations.soas.name[context.declarations.soas.index(schema)]),
                     [&](std::size_t const begin,
                         std::size_t const end,
                         std::span<std::string const> const path) {
                         result.append(body.substr(copied, begin - copied));
                         auto const prefix{representation == SoaRepresentation::compact &&
                                                   receiver == SoaReceiver::owner
                                               ? "get_view()"
                                               : "this->"};
                         result += logical_column_access(
                             context, schema, path, representation, prefix, schemas);
                         copied = end;
                     });
    result.append(body.substr(copied));
    return result;
}
}

auto lower_soa_api(LoweringContext& context,
                   DeclarationHandle schema,
                   SoaRepresentation const representation,
                   SoaReceiver const receiver,
                   std::string const& type_name,
                   std::map<StringId, DeclarationHandle> const* schemas,
                   SoaBackend const backend,
                   StringId const equivalent_constructor) -> LoweredSoa {
    auto& ir{context.ir};
    NodeListBuilder header{ir};
    NodeListBuilder source{ir};
    if (representation == SoaRepresentation::compact && backend == SoaBackend::standard_library) {
        header.append(compact_row_api(context, schema, receiver, *schemas));
    }
    for (auto const alias : context.declarations.items(
             context.declarations.soas
                 .using_declarations[context.declarations.soas.index(schema)])) {
        header.add(ir.raw("using " + context.text(alias) + ";")).new_lines(1);
    }
    auto append = [&](SchemaRange<FunctionSchemaId> const functions) {
        for (auto const function_id : context.declarations.items(functions)) {
            auto const function{function_id};
            auto const body{expand_columns(
                context,
                join_lines(context,
                           context.declarations.functions
                               .body_lines[context.declarations.functions.index(function)]),
                schema,
                representation,
                receiver,
                schemas)};
            auto spec{lower_schema_function(context, function, body)};
            if (backend == SoaBackend::standard_library) {
                auto normalize = [&ir](CppTypeId& type) {
                    if (type.value == 0) {
                        return;
                    }
                    auto const physical{classify_physical_type_use(ir.types, type)};
                    auto const object{ir.strings.view(physical.object_spelling)};
                    auto const native{native_spelling(std::string{object})};
                    if (native != object) {
                        auto spelling{std::string{ir.types.spelling(type)}};
                        spelling.replace(spelling.find(object), object.size(), native);
                        auto const existing{ir.types.dependencies(type)};
                        std::vector<TypeDependencyId> dependencies{existing.begin(),
                                                                   existing.end()};
                        dependencies.push_back(ir.types.dependency(native, "cstdint"));
                        type = ir.types.derive(type, spelling, dependencies);
                    }
                };
                auto return_type{ir.function_return_type(spec)};
                normalize(return_type);
                ir.set_function_return_type(spec, return_type, ir.function_return_syntax(spec));
                for (auto const parameter : ir.function_parameters(spec)) {
                    auto type{ir.parameter_type(parameter)};
                    normalize(type);
                    ir.set_parameter_type(parameter, type);
                }
            }
            if (context.declarations.functions
                    .definition_in_source[context.declarations.functions.index(function)]) {
                auto const id{spec};
                auto const use_trailing_return{
                    ir.function_return_syntax(spec) == ReturnSyntax::direct &&
                    ir.types.spelling(ir.function_return_type(spec)) != "auto" &&
                    ir.types.spelling(ir.function_return_type(spec)) != "decltype(auto)"};
                header.add(ir.declaration(id)).new_lines(1);
                source
                    .add(ir.definition(id,
                                       ir.strings.intern(type_name),
                                       use_trailing_return ? ReturnSyntax::trailing
                                                           : ir.function_return_syntax(spec)))
                    .new_lines(2);
            } else {
                ir.function_flags(spec).set_define_in_header(
                    ir.function_flags(spec).define_in_header() ||
                    !context.declarations.functions
                         .body_lines[context.declarations.functions.index(function)]
                         .empty());
                ir.function_flags(spec).set_is_inline(ir.function_flags(spec).define_in_header());
                header.add(ir.header_function(spec)).new_lines(1);
            }
        }
    };
    if (receiver == SoaReceiver::owner) {
        append(context.declarations.soas.functions[context.declarations.soas.index(schema)]);
    } else {
        append(context.declarations.soas
                   .const_view_functions[context.declarations.soas.index(schema)]);
        if (receiver == SoaReceiver::mutable_view) {
            append(context.declarations.soas
                       .mutable_view_functions[context.declarations.soas.index(schema)]);
        }
    }
    if (representation == SoaRepresentation::compact &&
        context.declarations.soas.equivalent_type[context.declarations.soas.index(schema)]) {
        auto const equivalent{context.resolve(
            *context.declarations.soas.equivalent_type[context.declarations.soas.index(schema)])};
        header.add(ir.using_declaration(ir.strings.intern("equivalent_type"), equivalent))
            .new_lines(1);
        std::string expression;
        if (receiver == SoaReceiver::owner) {
            expression = "get_const_view()[index]";
        } else {
            expression = equivalent_constructor.value == 0
                           ? "equivalent_type{"
                           : std::string{ir.strings.view(equivalent_constructor)} + "(";
            bool first{true};
            for (auto const member_id : context.declarations.items(
                     context.declarations.soas.members[context.declarations.soas.index(schema)])) {
                auto const member{member_id};
                if (!first) {
                    expression += ", ";
                }
                first = false;
                std::vector<std::string> const path{
                    context.text(context.declarations.soa_members
                                     .name[context.declarations.soa_members.index(member)])};
                expression +=
                    logical_column_access(context, schema, path, representation) + "[index]";
            }
            expression += equivalent_constructor.value == 0 ? "}" : ")";
        }
        header
            .add(ir.header_function([&] {
                auto const created_function{ir.function("operator[]", "auto")};
                ir.set_function_parameters(
                    created_function,
                    {ir.parameter(backend == SoaBackend::standard_library ? "std::uint32_t"
                                                                          : "std::int32_t",
                                  "index")});
                ir.set_function_body(
                    created_function,
                    {ir.raw("assert(index < num());\nreturn " + expression + ";")});
                ir.set_function_return_type(
                    created_function, ir.types.intern("equivalent_type"), ReturnSyntax::trailing);
                ir.set_function_receiver(created_function, FunctionReceiver::const_);
                ir.function_flags(created_function).set_is_inline(true);
                ir.function_flags(created_function).set_define_in_header(true);
                return created_function;
            }()))
            .new_lines(1);
    }
    return {header.build(), source.build()};
}
}
