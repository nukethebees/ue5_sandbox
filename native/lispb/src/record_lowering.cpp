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
                store_text("ioj"),
                {enum_count_specialization(
                    ctx, named_type(qualified, NameOrigin::global), fields.size())}));
            result.header_global.push_back(
                enum_strings_specialization(ctx, qualified, "Name", names));
        }
        if (auto const policy{records.constraint_policy[row]}) {
            auto const name{ctx.text(records.name[row])};
            auto const constrained{template_type(
                named_type("ioj::ConstrainedValue",
                           NameOrigin::global,
                           {dependency("ioj::ConstrainedValue", "ioj/constrained_value.h")}),
                {named_type(name + "Value"), reference_type(intern_type(ctx.text(*policy)))})};
            result.header.push_back(using_declaration(records.name[row], constrained));
        }
        if (records.s7_tag[schema]) {
            lower_record_reader(ctx, schema, result);
        }
        return result;
    }
  private:
    auto record_type_matches(std::string const& name) -> Expr {
        return type_reference(template_type(
            std_types().same_as,
            {template_type(std_types().remove_const_t, {named_type("Self")}), named_type(name)}));
    }
    auto record_visitor(LoweringContext& ctx, DeclarationHandle record_id, std::string const& name)
        -> Nodes {
        auto const& records{ctx.declarations.records};
        auto const& members{ctx.declarations.child_occurrences};
        auto const fields{ctx.declarations.items(records.members[record_id])};
        std::vector<EnumValueId> values;
        std::vector<SwitchCaseId> cases;
        Nodes accessors;
        auto const forwarded_visitor{template_call(type_reference(std_types().forward),
                                                   {type_reference(named_type("Visitor"))},
                                                   {named("visitor")})};
        for (auto const field : fields) {
            auto const member{members.name[members.index(field)]};
            values.push_back(enumerator(member));
            auto const field_value{scope_member(named_type("Field"), member)};
            accessors.push_back(
                if_stmt(binary(BinaryOperator::equal, named("Selected"), field_value),
                        block({return_stmt(parenthesized(member_access(named("self"), member)))}),
                        {},
                        true));
            cases.push_back(switch_case(
                field_value,
                block({return_stmt(template_call(
                    named("invoke_field"), {field_value}, {named("self"), forwarded_visitor}))}),
                false));
        }

        auto result{node_list()};
        result.add(enumeration(store_text("Field"),
                               values.size() <= 256 ? std_types().u8 : std_types().u32,
                               values),
                   2);
        FunctionDefinition const accessor{
            .name = named("get_field"),
            .return_type = intern_type("decltype(auto)"),
            .parameters = {parameter("Self&", fields.empty() ? "" : "self")},
            .implementation = {.kind = FunctionImplementationKind::body, .body = accessors},
            .receiver = FunctionReceiver::static_,
            .flags = {.is_constexpr = true, .define_in_header = true},
            .template_declaration = template_declaration(
                {value_template_parameter(intern_type("Field"), store_text("Selected")),
                 type_template_parameter(store_text("Self"))}),
            .requires_clause = parenthesized(binary(
                BinaryOperator::logical_and,
                record_type_matches(name),
                binary(BinaryOperator::less,
                       call(std_types().to_underlying, {named("Selected")}),
                       template_call(
                           type_reference(
                               named_type("ioj::enum_count",
                                          NameOrigin::global,
                                          {dependency("ioj::enum_count", "ioj/enum_traits.h")})),
                           {type_reference(decorate_type(
                               nested_type(named_type("Self"), "Field"), "typename ", ""))}))))};
        result.add(header_function(add_function(accessor)), 2);

        FunctionDefinition const invoke{
            .name = named("invoke_field"),
            .return_type = std_types().void_,
            .parameters = {parameter("Self&", "self"), parameter("Visitor&&", "visitor")},
            .implementation =
                {.kind = FunctionImplementationKind::body,
                 .body = {expr_stmt(template_call(
                     member_access(forwarded_visitor, "template operator()"),
                     {named("Selected")},
                     {template_call(named("get_field"), {named("Selected")}, {named("self")})}))}},
            .receiver = FunctionReceiver::static_,
            .flags = {.is_constexpr = true, .define_in_header = true},
            .template_declaration = template_declaration(
                {value_template_parameter(intern_type("Field"), store_text("Selected")),
                 type_template_parameter(store_text("Self")),
                 type_template_parameter(store_text("Visitor"))})};
        result.add(header_function(add_function(invoke)), 2);

        FunctionDefinition const function{
            .name = named("visit_field"),
            .return_type = std_types().void_,
            .parameters = {parameter("Self&", fields.empty() ? "" : "self"),
                           parameter("Field", "field"),
                           parameter("Visitor&&", fields.empty() ? "" : "visitor")},
            .implementation = {.kind = FunctionImplementationKind::body,
                               .body = {switch_stmt(named("field"), cases),
                                        assertion(binary(BinaryOperator::logical_and,
                                                         literal(false),
                                                         string_literal("Invalid record field"))),
                                        expr_stmt(call(std_types().unreachable))}},
            .receiver = FunctionReceiver::static_,
            .flags = {.is_constexpr = true, .define_in_header = true},
            .template_declaration =
                template_declaration({type_template_parameter(store_text("Self")),
                                      type_template_parameter(store_text("Visitor"))}),
            .requires_clause = record_type_matches(name)};
        result.add(header_function(add_function(function)), 2);
        return result.build();
    }
    auto record_node(LoweringContext& ctx, DeclarationHandle record_id) -> NodeId {
        auto const& records{ctx.declarations.records};
        auto const& record_members{ctx.declarations.child_occurrences};
        auto const row{records.index(record_id)};
        auto const name{ctx.text(records.name[row]) +
                        (records.constraint_policy[row] ? "Value" : "")};
        auto members{node_list()};
        if (records.comparison[row] != RecordComparison::none) {
            auto const three_way{records.comparison[row] == RecordComparison::three_way};
            FunctionDefinition const comparison{
                .name = named(three_way ? "operator<=>" : "operator=="),
                .return_type = three_way ? intern_type("auto", {dependency("auto", "compare")})
                                         : std_types().bool_,
                .parameters = {parameter(name + " const&", "")},
                .implementation = {.kind = FunctionImplementationKind::defaulted},
                .exception_specification = {.kind =
                                                records.comparison_noexcept[row]
                                                    ? FunctionExceptionKind::noexcept_
                                                    : FunctionExceptionKind::potentially_throwing},
                .receiver = FunctionReceiver::const_};
            members.add(header_function(add_function(comparison)));
        }
        for (auto function_id : ctx.declarations.items(records.functions[row])) {
            members.add(header_function(add_function(lower_schema_function(ctx, function_id))));
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
        auto const created_record{add_record({.name = named(store_text(name)),
                                              .children = members.build(),
                                              .export_specifier = records.export_specifier[row]})};
        return created_record;
    }
};

} // namespace

auto lower_record(LoweringContext& ctx, DeclarationHandle schema) -> DeclarationEmission {
    return RecordLowering{ctx.ir}.lower_record(ctx, schema);
}

} // namespace lispb::codegen::detail
