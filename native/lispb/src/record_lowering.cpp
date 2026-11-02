#include "declaration_emission.h"
#include "lowering.h"
#include "lowering_context.h"
#include "lowering_utils.h"
#include "record_reader_lowering.h"

#include <codegen/ast/cpp_ir_builder.h>
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

class RecordLowering : public CppIRBuilder {
  public:
    explicit RecordLowering(CppIR& ir)
        : CppIRBuilder{ir} {}
    auto lower_record(LoweringContext& ctx, DeclarationHandle schema) -> DeclarationEmission {
        auto result{DeclarationEmission{.header = {record_node(ctx, schema)}}};
        auto const& records{ctx.declarations.records};
        auto const row{records.index(schema)};
        if (records.generate_visitor[row]) {
            auto const settings{ctx.declarations.module_settings(ctx.module)};
            auto const namespace_name{
                ctx.declarations.module_settings_store.namespace_name[settings]};
            auto const qualified{(namespace_name ? ctx.text(*namespace_name) + "::" : "::") +
                                 ctx.text(records.name[row]) +
                                 (records.constraint_policy[row] ? "Value" : "") + "::Field"};
            auto const fields{ctx.declarations.items(records.members[row])};
            std::vector<std::string> names;
            for (auto const field : fields) {
                names.push_back(ctx.text(ctx.declarations.child_occurrences.name[field]));
            }
            result.header_global.push_back(namespace_node(
                ir_.strings.intern("ioj"),
                {enum_count_specialization(
                    ir_, ir_.types.named(qualified, NameOrigin::global), fields.size())}));
            result.header_global.push_back(
                enum_strings_specialization(ir_, qualified, "Name", names));
        }
        if (auto const policy{records.constraint_policy[row]}) {
            auto const name{ctx.text(records.name[row])};
            auto const constrained{ir_.types.template_type(
                ir_.types.named(
                    "ioj::ConstrainedValue",
                    NameOrigin::global,
                    {ir_.types.dependency("ioj::ConstrainedValue", "ioj/constrained_value.h")}),
                {ir_.types.named(name + "Value"),
                 ir_.types.reference(ir_.types.intern(ctx.text(*policy)))})};
            result.header.push_back(using_declaration(records.name[row], constrained));
        }
        if (records.s7_tag[schema]) {
            lower_record_reader(ctx, schema, result);
        }
        return result;
    }
  private:
    auto record_type_matches(std::string const& name) -> Expr {
        return type_reference(ir_.types.template_type(
            ir_.types.std.same_as,
            {ir_.types.template_type(ir_.types.std.remove_const_t, {ir_.types.named("Self")}),
             ir_.types.named(name)}));
    }
    auto record_visitor(LoweringContext& ctx, DeclarationHandle record_id, std::string const& name)
        -> Nodes {
        auto const& records{ctx.declarations.records};
        auto const& members{ctx.declarations.child_occurrences};
        auto const fields{ctx.declarations.items(records.members[record_id])};
        std::vector<EnumValueId> values;
        std::vector<SwitchCaseId> cases;
        Nodes accessors;
        auto const forwarded_visitor{call(
            template_id(
                type_reference(ir_.types.named("std::forward",
                                               NameOrigin::global,
                                               {ir_.types.dependency("std::forward", "utility")})),
                {type_reference(ir_.types.named("Visitor"))}),
            {named("visitor")})};
        for (auto const field : fields) {
            auto const member{members.name[members.index(field)]};
            values.push_back(enumerator(member));
            auto const field_value{type_reference(
                ir_.types.decorate(ir_.types.named("Field"), "", "::" + ctx.text(member)))};
            accessors.push_back(
                if_stmt(binary(BinaryOperator::equal, named("Selected"), field_value),
                        block({return_stmt(parenthesized(member_access(named("self"), member)))}),
                        {},
                        true));
            cases.push_back(switch_case(
                field_value,
                block({return_stmt(call(template_id(named("invoke_field"), {field_value}),
                                        {named("self"), forwarded_visitor}))}),
                false));
        }

        NodeListBuilder result{ir_};
        result.add(enumeration(ir_.strings.intern("Field"),
                               values.size() <= 256 ? ir_.types.std.u8 : ir_.types.std.u32,
                               values),
                   2);
        auto const accessor{function("get_field", "decltype(auto)")};
        set_function_template(
            accessor,
            template_declaration({value_template_parameter(ir_.types.intern("Field"),
                                                           ir_.strings.intern("Selected")),
                                  type_template_parameter(ir_.strings.intern("Self"))}));
        set_function_requires_clause(
            accessor,
            parenthesized(binary(
                BinaryOperator::logical_and,
                record_type_matches(name),
                binary(BinaryOperator::less,
                       call(type_reference(ir_.types.named(
                                "std::to_underlying",
                                NameOrigin::global,
                                {ir_.types.dependency("std::to_underlying", "utility")})),
                            {named("Selected")}),
                       call(template_id(
                           type_reference(ir_.types.named(
                               "ioj::enum_count",
                               NameOrigin::global,
                               {ir_.types.dependency("ioj::enum_count", "ioj/enum_traits.h")})),
                           {type_reference(ir_.types.decorate(
                               ir_.types.named("Self"), "typename ", "::Field"))}))))));
        set_function_receiver(accessor, FunctionReceiver::static_);
        set_function_parameters(accessor, {parameter("Self&", fields.empty() ? "" : "self")});
        function_flags(accessor).set_is_constexpr(true);
        function_flags(accessor).set_define_in_header(true);
        set_function_body(accessor, accessors);
        result.add(header_function(accessor), 2);

        auto const invoke{function("invoke_field", ir_.types.std.void_)};
        set_function_template(
            invoke,
            template_declaration({value_template_parameter(ir_.types.intern("Field"),
                                                           ir_.strings.intern("Selected")),
                                  type_template_parameter(ir_.strings.intern("Self")),
                                  type_template_parameter(ir_.strings.intern("Visitor"))}));
        set_function_receiver(invoke, FunctionReceiver::static_);
        set_function_parameters(invoke,
                                {parameter("Self&", "self"), parameter("Visitor&&", "visitor")});
        function_flags(invoke).set_is_constexpr(true);
        function_flags(invoke).set_define_in_header(true);
        set_function_body(
            invoke,
            {expr_stmt(call(
                template_id(member_access(forwarded_visitor, "template operator()"),
                            {named("Selected")}),
                {call(template_id(named("get_field"), {named("Selected")}), {named("self")})}))});
        result.add(header_function(invoke), 2);

        auto const function_id{function("visit_field", ir_.types.std.void_)};
        set_function_template(
            function_id,
            template_declaration({type_template_parameter(ir_.strings.intern("Self")),
                                  type_template_parameter(ir_.strings.intern("Visitor"))}));
        set_function_requires_clause(function_id, record_type_matches(name));
        set_function_receiver(function_id, FunctionReceiver::static_);
        set_function_parameters(function_id,
                                {parameter("Self&", fields.empty() ? "" : "self"),
                                 parameter("Field", "field"),
                                 parameter("Visitor&&", fields.empty() ? "" : "visitor")});
        function_flags(function_id).set_is_constexpr(true);
        function_flags(function_id).set_define_in_header(true);
        set_function_body(
            function_id,
            {switch_stmt(named("field"), cases),
             expr_stmt(call(named("assert", {ir_.types.dependency("assert", "cassert")}),
                            {binary(BinaryOperator::logical_and,
                                    literal("false"),
                                    string_literal("Invalid record field"))})),
             expr_stmt(call(type_reference(
                 ir_.types.named("std::unreachable",
                                 NameOrigin::global,
                                 {ir_.types.dependency("std::unreachable", "utility")}))))});
        result.add(header_function(function_id), 2);
        return result.build();
    }
    auto record_node(LoweringContext& ctx, DeclarationHandle record_id) -> NodeId {
        auto const& records{ctx.declarations.records};
        auto const& record_members{ctx.declarations.child_occurrences};
        auto const row{records.index(record_id)};
        auto const name{ctx.text(records.name[row]) +
                        (records.constraint_policy[row] ? "Value" : "")};
        NodeListBuilder members{ir_};
        if (records.comparison[row] != RecordComparison::none) {
            auto const three_way{records.comparison[row] == RecordComparison::three_way};
            auto const comparison{function(
                three_way ? "operator<=>" : "operator==",
                three_way ? ir_.types.intern("auto", {ir_.types.dependency("auto", "compare")})
                          : ir_.types.std.bool_)};
            set_function_parameters(comparison, {parameter(name + " const&", "")});
            set_function_receiver(comparison, FunctionReceiver::const_);
            if (records.comparison_noexcept[row]) {
                set_function_noexcept(comparison);
            }
            set_function_defaulted(comparison);
            members.add(header_function(comparison));
        }
        for (auto const function_id : ctx.declarations.items(records.functions[row])) {
            members.add(header_function(lower_schema_function(ctx, function_id)));
        }
        if (records.comparison[row] != RecordComparison::none || !records.functions[row].empty()) {
            members.new_lines(2);
        }
        if (records.generate_visitor[row]) {
            members.append(record_visitor(ctx, record_id, name));
        }
        for (auto const member : ctx.declarations.items(records.members[row])) {
            auto const member_row{record_members.index(member)};
            auto const initializer{record_members.initializer[member_row]};
            members.add(instance_member(lower_member_type(ctx, member),
                                        record_members.name[member_row],
                                        initializer
                                            ? std::optional<Expr>{raw_expr(*initializer, {})}
                                            : std::nullopt),
                        1);
        }
        auto const created_record{
            record(ir_.strings.intern(name), members.build(), RecordKind::struct_)};
        set_record_export_specifier(created_record, records.export_specifier[row]);
        return created_record;
    }
};

} // namespace

auto lower_record(LoweringContext& ctx, DeclarationHandle schema) -> DeclarationEmission {
    return RecordLowering{ctx.ir}.lower_record(ctx, schema);
}

} // namespace lispb::codegen::detail
