#include "declaration_emission.h"
#include "lowering.h"
#include "lowering_context.h"
#include "lowering_utils.h"
#include "record_reader_lowering.h"

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/expr.h>
#include <codegen/ast/function_receiver.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/node_id.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/schema/child_occurrences.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/function_schemas.h>
#include <codegen/schema/record_comparison.h>
#include <codegen/schema/record_schemas.h>
#include <lispb/indices.h>
#include <lispb/schema/type_graph.h>
#include <lispb/strings.h>

#include <format>
#include <optional>
#include <string>
#include <vector>

namespace lispb::codegen::detail {
namespace {

auto record_visitor(LoweringContext& context, DeclarationHandle record, std::string const& name)
    -> Nodes {
    auto& ir{context.ir};
    auto const& records{context.declarations.records};
    auto const& members{context.declarations.child_occurrences};
    auto const fields{context.declarations.items(records.members[record])};
    std::vector<EnumValueId> values;
    std::vector<SwitchCaseId> cases;
    std::string accessors;
    for (auto const field : fields) {
        auto const member{members.name[members.index(field)]};
        values.push_back(ir.enumerator(member));
        auto const id{"Field::" + context.text(member)};
        accessors += std::format(
            "if constexpr (Selected == {}) {{ return (self.{}); }}\n", id, context.text(member));
        cases.push_back(ir.switch_case(
            ir.named(id),
            ir.block({ir.return_stmt(ir.raw_expr(
                std::format("invoke_field<{}>(self, std::forward<Visitor>(visitor))", id),
                {ir.types.dependency("std::forward", "utility")}))}),
            false));
    }

    NodeListBuilder result{ir};
    result.add(
        ir.enumeration(ir.strings.intern("Field"),
                       ir.types.intern(values.size() <= 256 ? "std::uint8_t" : "std::uint32_t",
                                       {ir.types.dependency("std::uint8_t", "cstdint")}),
                       values),
        2);
    auto const accessor{ir.function("get_field", "decltype(auto)")};
    ir.set_function_template(
        accessor,
        ir.template_declaration(
            {ir.value_template_parameter(ir.types.intern("Field"), ir.strings.intern("Selected")),
             ir.type_template_parameter(ir.strings.intern("Self"))}));
    ir.set_function_requires_clause(
        accessor,
        ir.raw_expr("(std::same_as<std::remove_const_t<Self>, " + name +
                        "> && std::to_underlying(Selected) < "
                        "ioj::enum_count<typename Self::Field>())",
                    {ir.types.dependency("std::same_as", "concepts"),
                     ir.types.dependency("std::to_underlying", "utility"),
                     ir.types.dependency("ioj::enum_count", "ioj/enum_traits.h"),
                     ir.types.dependency("std::remove_const_t", "type_traits")}));
    ir.set_function_receiver(accessor, FunctionReceiver::static_);
    ir.set_function_parameters(accessor, {ir.parameter("Self&", fields.empty() ? "" : "self")});
    ir.function_flags(accessor).set_is_constexpr(true);
    ir.function_flags(accessor).set_define_in_header(true);
    ir.set_function_body(accessor, fields.empty() ? Nodes{} : Nodes{ir.raw(accessors)});
    result.add(ir.header_function(accessor), 2);

    auto const invoke{ir.function("invoke_field", "void")};
    ir.set_function_template(
        invoke,
        ir.template_declaration(
            {ir.value_template_parameter(ir.types.intern("Field"), ir.strings.intern("Selected")),
             ir.type_template_parameter(ir.strings.intern("Self")),
             ir.type_template_parameter(ir.strings.intern("Visitor"))}));
    ir.set_function_receiver(invoke, FunctionReceiver::static_);
    ir.set_function_parameters(
        invoke, {ir.parameter("Self&", "self"), ir.parameter("Visitor&&", "visitor")});
    ir.function_flags(invoke).set_is_constexpr(true);
    ir.function_flags(invoke).set_define_in_header(true);
    ir.set_function_body(
        invoke,
        {ir.expr_stmt(ir.raw_expr("std::forward<Visitor>(visitor).template "
                                  "operator()<Selected>(get_field<Selected>(self))",
                                  {ir.types.dependency("std::forward", "utility")}))});
    result.add(ir.header_function(invoke), 2);

    auto const function{ir.function("visit_field", "void")};
    ir.set_function_template(
        function,
        ir.template_declaration({ir.type_template_parameter(ir.strings.intern("Self")),
                                 ir.type_template_parameter(ir.strings.intern("Visitor"))}));
    ir.set_function_requires_clause(
        function,
        ir.raw_expr("std::same_as<std::remove_const_t<Self>, " + name + ">",
                    {ir.types.dependency("std::same_as", "concepts"),
                     ir.types.dependency("std::remove_const_t", "type_traits")}));
    ir.set_function_receiver(function, FunctionReceiver::static_);
    ir.set_function_parameters(function,
                               {ir.parameter("Self&", fields.empty() ? "" : "self"),
                                ir.parameter("Field", "field"),
                                ir.parameter("Visitor&&", fields.empty() ? "" : "visitor")});
    ir.function_flags(function).set_is_constexpr(true);
    ir.function_flags(function).set_define_in_header(true);
    ir.set_function_body(
        function,
        {ir.switch_stmt(ir.named("field"), cases),
         ir.expr_stmt(ir.raw_expr("assert(false && \"Invalid record field\")",
                                  {ir.types.dependency("assert", "cassert")})),
         ir.expr_stmt(ir.raw_expr("std::unreachable()",
                                  {ir.types.dependency("std::unreachable", "utility")}))});
    result.add(ir.header_function(function), 2);
    return result.build();
}

auto record_node(LoweringContext& context, DeclarationHandle record) -> NodeId {
    auto& ir{context.ir};
    auto const& records{context.declarations.records};
    auto const& record_members{context.declarations.child_occurrences};
    auto const row{records.index(record)};
    auto const name{context.text(records.name[row]) +
                    (records.constraint_policy[row] ? "Value" : "")};
    NodeListBuilder members{ir};
    if (records.comparison[row] != RecordComparison::none) {
        auto const three_way{records.comparison[row] == RecordComparison::three_way};
        auto const comparison{ir.function(
            three_way ? "operator<=>" : "operator==",
            three_way ? ir.types.intern("auto", {ir.types.dependency("auto", "compare")})
                      : ir.types.intern("bool"))};
        ir.set_function_parameters(comparison, {ir.parameter(name + " const&", "")});
        ir.set_function_receiver(comparison, FunctionReceiver::const_);
        if (records.comparison_noexcept[row]) {
            ir.set_function_noexcept(comparison);
        }
        ir.set_function_defaulted(comparison);
        members.add(ir.header_function(comparison));
    }
    for (auto const function : context.declarations.items(records.functions[row])) {
        members.add(ir.header_function(lower_schema_function(context, function)));
    }
    if (records.comparison[row] != RecordComparison::none || !records.functions[row].empty()) {
        members.new_lines(2);
    }
    if (records.generate_visitor[row]) {
        members.append(record_visitor(context, record, name));
    }
    for (auto const member : context.declarations.items(records.members[row])) {
        auto const member_row{record_members.index(member)};
        auto const initializer{record_members.initializer[member_row]};
        members.add(ir.instance_member(lower_member_type(context, member),
                                       record_members.name[member_row],
                                       initializer
                                           ? std::optional<Expr>{ir.raw_expr(*initializer, {})}
                                           : std::nullopt),
                    1);
    }
    auto const created_record{
        ir.record(ir.strings.intern(name), members.build(), RecordKind::struct_)};
    ir.set_record_export_specifier(created_record, records.export_specifier[row]);
    return created_record;
}

} // namespace

auto lower_record(LoweringContext& context, DeclarationHandle schema) -> DeclarationEmission {
    auto result{DeclarationEmission{.header = {record_node(context, schema)}}};
    auto const& records{context.declarations.records};
    auto const row{records.index(schema)};
    if (records.generate_visitor[row]) {
        auto& ir{context.ir};
        auto const settings{context.declarations.module_settings(context.module)};
        auto const namespace_name{
            context.declarations.module_settings_store.namespace_name[settings]};
        auto const qualified{"::" + (namespace_name ? context.text(*namespace_name) + "::" : "") +
                             context.text(records.name[row]) +
                             (records.constraint_policy[row] ? "Value" : "") + "::Field"};
        auto const fields{context.declarations.items(records.members[row])};
        std::vector<std::string> names;
        for (auto const field : fields) {
            names.push_back(context.text(context.declarations.child_occurrences.name[field]));
        }
        result.header_global.push_back(ir.namespace_node(
            ir.strings.intern("ioj"),
            {enum_count_specialization(ir, ir.types.intern(qualified), fields.size())}));
        result.header_global.push_back(enum_strings_specialization(ir, qualified, "Name", names));
    }
    if (auto const policy{records.constraint_policy[row]}) {
        auto const name{context.text(records.name[row])};
        auto& ir{context.ir};
        auto const constrained{ir.types.intern(
            "ioj::ConstrainedValue<" + name + "Value, " + context.text(*policy) + ">",
            {ir.types.dependency("ioj::ConstrainedValue", "ioj/constrained_value.h")})};
        result.header.push_back(ir.using_declaration(records.name[row], constrained));
    }
    if (records.s7_tag[schema]) {
        lower_record_reader(context, schema, result);
    }
    return result;
}

} // namespace lispb::codegen::detail
