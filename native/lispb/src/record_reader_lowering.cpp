#include "record_reader_lowering.h"

#include "declaration_emission.h"
#include "lowering_context.h"
#include "lowering_utils.h"
#include <ioj/ascii.h>

#include <codegen/ast/cpp_ir_builder.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/schema/declarations.h>
#include <lispb/schema/type_graph.h>
#include <lispb/strings.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <format>
#include <iterator>
#include <set>
#include <stdexcept>

namespace lispb::codegen::detail {
auto reader_header_path(std::string_view const header) -> std::string {
    auto const path{std::filesystem::path{header}};
    return (path.parent_path() / (path.stem().string() + "_reader" + path.extension().string()))
        .generic_string();
}

namespace {

class RecordReaderLowering : public CppIRBuilder {
  public:
    explicit RecordReaderLowering(CppIR& ir)
        : CppIRBuilder{ir} {}

    /* **************************************** */
    // Lowering
    /* **************************************** */
    void lower_record_reader(LoweringContext& ctx,
                             DeclarationHandle const record_id,
                             DeclarationEmission& emission) {
        auto const& records{ctx.declarations.records};
        auto const row{records.index(record_id)};
        auto const fields{reader_fields(ctx, record_id)};
        auto const empty{fields.empty()};
        auto const type{ctx.semantic.find_declared(ctx.module, records.name[row]).value()};
        auto const name{ctx.text(ctx.semantic.cpp_spelling(type))};
        auto const config_type{
            named_type(name + (records.constraint_policy[row] ? "Value" : ""), NameOrigin::global)};
        auto const value_type{config_type};
        auto const& members{ctx.declarations.child_occurrences};
        if (std::ranges::any_of(fields, [&](auto field) {
                auto const& options{members.s7[members.index(field)]};
                return options.nonzero || options.positive || options.nonnegative ||
                       options.finite || options.nonempty || options.minimum || options.maximum;
            })) {
            emission.reader_includes.push_back(store_text("ioj/s7/field_constraints.h"));
        }
        std::vector<EnumValueId> values;
        std::set<std::string> keys;
        for (auto const field : fields) {
            auto const member{members.name[members.index(field)]};
            auto const key{field_name(ctx, field)};
            if (key.empty() || !std::ranges::all_of(key, ioj::is_lowercase_symbol_character)) {
                throw std::invalid_argument{
                    "S7 field names must contain lowercase letters, digits or hyphens"};
            }
            if (!keys.insert(key).second) {
                throw std::invalid_argument{"Duplicate S7 field name: " + key};
            }
            values.push_back(enumerator(member));
        }
        auto decls{node_list()};
        auto const includes{ctx.declarations.items(records.s7_reader_includes[row])};
        emission.reader_includes.insert(
            emission.reader_includes.end(), includes.begin(), includes.end());
        auto const emit_method{[&](std::string const& method,
                                   CppTypeId const return_type,
                                   std::vector<FunctionParameterId> const& parameters,
                                   Nodes body) {
            FunctionDefinition const function{
                .name = named(method),
                .return_type = return_type,
                .parameters = {parameters.begin(), parameters.end()},
                .implementation = {.kind = FunctionImplementationKind::body, .body = body},
                .receiver = FunctionReceiver::static_,
                .flags = {.define_in_header = true}};
            decls.add(header_function(add_function(function)), 2);
        }};
        std::vector<FunctionParameterId> const read_parameters{parameter("DecodeContext", "ctx")};
        if (std::ranges::any_of(fields, [&](auto field) {
                auto const index{members.index(field)};
                return !members.initializer[index].has_value();
            })) {
            emit_method("construct",
                        template_type(named_type("ioj::s7::ReadResult", NameOrigin::global),
                                      {named_type("value_type")}),
                        read_parameters,
                        fields_body(ctx, record_id, true));
        }
        auto decode_parameters{read_parameters};
        decode_parameters.push_back(parameter("value_type&", empty ? "" : "result"));
        emit_method(
            "decode", std_types().void_, decode_parameters, fields_body(ctx, record_id, false));
        if (!records.s7_positional[row]) {
            decls.add(access(Access::private_), 1);
        }
        if (!records.s7_positional[row]) {
            if (records.generate_visitor[row]) {
                decls.add(using_declaration(store_text("Field"),
                                            decorate_type(named_type("value_type"), "", "::Field")),
                          2);
            } else {
                decls.add(enumeration(store_text("Field"),
                                      values.size() <= 256 ? std_types().u8 : std_types().u32,
                                      values),
                          2);
            }
            std::vector<Expr> names;
            std::vector<Expr> required;
            auto const named_fields{records.generate_visitor[row]
                                        ? ctx.declarations.items(records.members[row])
                                        : std::span<ChildOccurrenceId const>{fields}};
            for (auto const field : named_fields) {
                auto const index{members.index(field)};
                if (std::ranges::find(fields, field) == fields.end()) {
                    names.push_back(string_literal(""));
                    continue;
                }
                names.push_back(string_literal(field_name(ctx, field)));
                if (members.s7[index].required) {
                    required.push_back(type_reference(decorate_type(
                        named_type("Field"), "", "::" + ctx.text(members.name[index]))));
                }
            }
            auto const array_type{template_type(
                std_types().array,
                {std_types().string_view, intern_type(std::to_string(named_fields.size()))})};
            decls.add(constexpr_member(array_type, store_text("field_names"), init_list(names)), 1);
            if (!required.empty()) {
                decls.add(
                    constexpr_member(intern_type("auto"),
                                     store_text("required"),
                                     template_call(named("required_fields"),
                                                   {type_reference(named_type("Field")),
                                                    member_call(named("field_names"), "size")},
                                                   {init_list(required)})),
                    1);
            }
        }
        std::array const inherited_names{store_text("value_type"), store_text("read")};
        auto const specialization{add_record(
            {.name = template_id(named("TypeReader"), {type_reference(value_type)}),
             .children = decls.build(),
             .bases = {template_type(
                 named_type("ioj::s7::RecordReader",
                            NameOrigin::global,
                            {dependency("ioj::s7::RecordReader", "ioj/s7/record_reader.h")}),
                 {value_type})},
             .template_declaration = template_specialization(),
             .scope_bindings =
                 std::vector<StringId>{inherited_names.begin(), inherited_names.end()}})};
        emission.reader_declarations.push_back(specialization);

        for (auto const dependency : ctx.semantic.dependencies_of(type)) {
            auto const declaration_id{ctx.semantic.source_declaration(dependency)};
            auto const module{ctx.semantic.module(dependency)};
            if (declaration_id.value != 0 && declaration_id.kind() == DeclarationKind::record &&
                records.s7_tag[declaration_id] && module != ctx.module) {
                emission.reader_includes.push_back(store_text(reader_header_path(
                    source_include(ctx, ctx.declarations.module_settings(module)))));
            }
        }
    }
  private:
    /* **************************************** */
    // Construction helpers
    /* **************************************** */
    auto bound_literal(LoweringContext& ctx, StringId const bound) -> std::string {
        auto text{ctx.text(bound)};
        if (text.contains('.')) {
            return text;
        }
        if (text == "-9223372036854775808") {
            return "(-9223372036854775807LL - 1LL)";
        }
        return text + (text.starts_with('-') ? "LL" : "ULL");
    }
    auto field_name(LoweringContext& ctx, ChildOccurrenceId const member) -> std::string {
        auto const& members{ctx.declarations.child_occurrences};
        auto const row{members.index(member)};
        if (members.s7[row].name) {
            return ctx.text(*members.s7[row].name);
        }
        auto name{ctx.text(members.name[row])};
        std::ranges::replace(name, '_', '-');
        return name;
    }
    auto reader_fields(LoweringContext& ctx, DeclarationHandle record_id)
        -> std::vector<ChildOccurrenceId> {
        auto const& decls{ctx.declarations};
        auto const& records{decls.records};
        std::vector<ChildOccurrenceId> result;
        for (auto field : decls.items(records.members[record_id])) {
            if (!decls.child_occurrences.s7[field].exclude) {
                result.push_back(field);
            }
        }
        return result;
    }
    auto constraint_arguments(LoweringContext& ctx, ChildOccurrenceId field) -> std::vector<Expr> {
        auto const& members{ctx.declarations.child_occurrences};
        auto const& options{members.s7[members.index(field)]};
        std::vector<Expr> checks;
        auto reference = [&](std::string_view name) {
            return type_reference(
                named_type("ioj::s7::constraints::" + std::string{name}, NameOrigin::global));
        };
        auto const constraint{[&](std::string_view check) { checks.push_back(reference(check)); }};
        if (options.nonzero) {
            constraint("nonzero");
        }
        if (options.positive) {
            constraint("positive");
        }
        if (options.nonnegative) {
            constraint("nonnegative");
        }
        if (options.finite) {
            constraint("finite");
        }
        if (options.nonempty) {
            constraint("nonempty");
        }
        if (options.minimum) {
            checks.push_back(
                call(reference("minimum"), {literal(bound_literal(ctx, *options.minimum))}));
        }
        if (options.maximum) {
            checks.push_back(
                call(reference("maximum"), {literal(bound_literal(ctx, *options.maximum))}));
        }
        return checks;
    }
    auto read_field(LoweringContext& ctx,
                    ChildOccurrenceId field,
                    Expr destination,
                    bool constructing = false) -> NodeId {
        auto arguments{constraint_arguments(ctx, field)};
        arguments.insert(arguments.begin(), destination);
        auto const reader{named("read")};
        return expr_stmt(
            call(constructing ? member_access(reader, "construct") : reader, arguments));
    }
    auto field_value(StringId name) -> Expr {
        return type_reference(
            decorate_type(named_type("Field"), "", "::" + std::string{text_view(name)}));
    }
    auto member_reader(LoweringContext& ctx, DeclarationHandle record_id) -> Expr {
        auto const& records{ctx.declarations.records};
        auto const& members{ctx.declarations.child_occurrences};
        struct Group {
            S7FieldOptions options;
            Expr condition;
            NodeId action;
        };
        std::vector<Group> groups;
        for (auto const field : ctx.declarations.items(records.members[record_id])) {
            auto const& options{members.s7[field]};
            auto const group{std::ranges::find_if(groups, [&](Group const& candidate) {
                auto const& other{candidate.options};
                return options.exclude == other.exclude &&
                       (options.exclude ||
                        (options.minimum == other.minimum && options.maximum == other.maximum &&
                         options.nonzero == other.nonzero && options.positive == other.positive &&
                         options.nonnegative == other.nonnegative &&
                         options.finite == other.finite && options.nonempty == other.nonempty));
            })};
            auto const condition{
                binary(BinaryOperator::equal, named("field"), field_value(members.name[field]))};
            if (group != groups.end()) {
                group->condition = binary(BinaryOperator::logical_or, group->condition, condition);
            } else {
                auto const action{options.exclude ? expr_stmt(call(std_types().unreachable))
                                                  : read_field(ctx, field, named("member"))};
                groups.push_back({options, condition, action});
            }
        }
        Nodes body;
        if (groups.size() == 1) {
            body.push_back(groups.front().action);
        } else {
            for (auto const& group : groups) {
                body.push_back(if_stmt(group.condition, block({group.action}), {}, true));
            }
        }
        return lambda({unary(UnaryOperator::address_of, named("read"))},
                      {parameter("auto&", "member")},
                      block(body),
                      template_declaration(
                          {value_template_parameter(named_type("Field"), store_text("field"))}));
    }
    auto fields_body(LoweringContext& ctx,
                     DeclarationHandle const record_id,
                     bool const constructing) -> Nodes {
        auto const& records{ctx.declarations.records};
        auto const row{records.index(record_id)};
        auto const& members{ctx.declarations.child_occurrences};
        auto const fields{reader_fields(ctx, record_id)};
        auto const positional{records.s7_positional[row] != 0};
        auto const decode_ctx{named("ctx")};
        auto const errors{member_access(decode_ctx, "errors")};
        auto move = [&](Expr value) { return call(std_types().move, {value}); };
        auto const unexpected{init_list({move(errors)}, std_types().unexpected)};
        auto body{node_list()};
        if (constructing) {
            for (auto const field : fields) {
                auto const name{ctx.text(members.name[members.index(field)])};
                body.add(variable(template_type(std_types().optional,
                                                {decltype_type(type_reference(
                                                    named_type("value_type::" + name)))}),
                                  "value_" + name,
                                  literal("")));
            }
        }

        if (positional) {
            body.add(variable("auto const",
                              "arguments",
                              call(named_type("ioj::s7::positional_arguments", NameOrigin::global),
                                   {member_access(decode_ctx, "reader"),
                                    member_access(decode_ctx, "node"),
                                    string_literal(ctx.text(*records.s7_tag[row])),
                                    member_access(decode_ctx, "path"),
                                    literal(std::to_string(fields.size()))})),
                     2);
            body.add(if_stmt(
                unary(UnaryOperator::logical_not, named("arguments")),
                block({expr_stmt(call(named_type("ioj::s7::append_errors", NameOrigin::global),
                                      {errors, member_call(named("arguments"), "error")})),
                       constructing ? return_stmt(unexpected) : return_stmt()})));
            std::size_t index{};
            for (auto const field : fields) {
                auto const name{ctx.text(members.name[members.index(field)])};
                auto const destination{constructing ? named("value_" + name)
                                                    : member_access(named("result"), name)};
                auto const path{
                    binary(BinaryOperator::add,
                           init_list({member_access(decode_ctx, "path")}, std_types().string),
                           string_literal("." + field_name(ctx, field)))};
                auto const reader{init_list(
                    {init_list({member_access(decode_ctx, "reader"),
                                subscript(unary(UnaryOperator::dereference, named("arguments")),
                                          literal(std::to_string(index++))),
                                path,
                                member_access(decode_ctx, "mode"),
                                errors})},
                    named_type("ioj::s7::FieldReader", NameOrigin::global))};
                auto arguments{constraint_arguments(ctx, field)};
                arguments.insert(arguments.begin(), destination);
                body.add(expr_stmt(
                    call(constructing ? member_access(reader, "construct") : reader, arguments)));
            }
        } else {
            Expr visitor;
            if (fields.empty()) {
                visitor = lambda({}, {parameter("Field"), parameter("auto const&")}, block({}));
            } else if (records.generate_visitor[row] && !constructing) {
                visitor = lambda(
                    {unary(UnaryOperator::address_of, named("result"))},
                    {parameter("Field", "selected"), parameter("auto const&", "read")},
                    block({expr_stmt(call(
                        decorate_type(named_type("value_type"), "", "::visit_field"),
                        {named("result"), named("selected"), member_reader(ctx, record_id)}))}));
            } else {
                std::vector<SwitchCaseId> cases;
                std::vector<Expr> captures;
                if (!constructing) {
                    captures.push_back(unary(UnaryOperator::address_of, named("result")));
                }
                for (auto const field : fields) {
                    auto const name{ctx.text(members.name[members.index(field)])};
                    auto const destination{constructing ? named("value_" + name)
                                                        : member_access(named("result"), name)};
                    if (constructing) {
                        captures.push_back(unary(UnaryOperator::address_of, destination));
                    }
                    cases.push_back(switch_case(
                        field_value(members.name[field]),
                        block({read_field(ctx, field, destination, constructing), break_stmt()}),
                        false));
                }
                if (records.generate_visitor[row]) {
                    // Stage construction before an object exists; excluded fields cannot be looked
                    // up.
                    cases.push_back(
                        switch_case({}, block({expr_stmt(call(std_types().unreachable))}), false));
                }
                std::array const parameters{parameter("Field const", "field"),
                                            parameter("auto const&", "read")};
                visitor = lambda(captures, parameters, block({switch_stmt(named("field"), cases)}));
            }
            std::vector<Expr> arguments{named("ctx"),
                                        string_literal(ctx.text(*records.s7_tag[row])),
                                        named("field_names"),
                                        visitor};
            if (std::ranges::any_of(fields, [&](auto const field) {
                    return members.s7[members.index(field)].required;
                })) {
                arguments.push_back(named("required"));
            }
            body.add(expr_stmt(template_call(
                named("decode_record"), {type_reference(named_type("Field"))}, arguments)));
        }
        if (!constructing) {
            return body.build();
        }
        auto const return_errors{
            if_stmt(unary(UnaryOperator::logical_not, member_call(errors, "empty")),
                    block({return_stmt(unexpected)}))};
        body.add(return_errors, 2);
        std::vector<Expr> initializers;
        for (auto const field : fields) {
            auto const index{members.index(field)};
            if (positional || members.s7[index].required) {
                auto const name{ctx.text(members.name[index])};
                initializers.push_back(designated_initializer(
                    members.name[index],
                    move(unary(UnaryOperator::dereference, named("value_" + name)))));
            }
        }
        body.add(variable("value_type", "result", init_list(initializers)), 2);
        for (auto const field : fields) {
            auto const index{members.index(field)};
            auto const name{ctx.text(members.name[index])};
            auto const& options{members.s7[index]};
            if (!positional && !options.required) {
                body.add(if_stmt(named("value_" + name),
                                 block({assignment(member_access(named("result"), name),
                                                   move(unary(UnaryOperator::dereference,
                                                              named("value_" + name))))})));
            }
        }
        body.add(return_stmt(named("result")));
        return body.build();
    }
};

} // namespace

void lower_record_reader(LoweringContext& ctx,
                         DeclarationHandle const record,
                         DeclarationEmission& emission) {
    RecordReaderLowering{ctx.ir}.lower_record_reader(ctx, record, emission);
}

} // namespace lispb::codegen::detail
