#include "record_reader_lowering.h"

#include "declaration_emission.h"
#include "lowering_context.h"
#include "lowering_utils.h"

#include <codegen/ast/cpp_ir.h>
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

namespace record_reader {
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

auto reader_fields(LoweringContext& ctx, DeclarationHandle record)
    -> std::vector<ChildOccurrenceId> {
    auto const& decls{ctx.declarations};
    auto const& records{decls.records};
    std::vector<ChildOccurrenceId> result;
    for (auto field : decls.items(records.members[record])) {
        if (!decls.child_occurrences.s7[field].exclude) {
            result.push_back(field);
        }
    }
    return result;
}

auto constraint_arguments(LoweringContext& ctx, ChildOccurrenceId field) -> std::vector<Expr> {
    auto& ir{ctx.ir};
    auto const& members{ctx.declarations.child_occurrences};
    auto const& options{members.s7[members.index(field)]};
    std::vector<Expr> checks;
    auto reference = [&](std::string_view name) {
        return ir.type_reference(
            ir.types.named("ioj::s7::constraints::" + std::string{name}, NameOrigin::global));
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
            ir.call(reference("minimum"), {ir.literal(bound_literal(ctx, *options.minimum))}));
    }
    if (options.maximum) {
        checks.push_back(
            ir.call(reference("maximum"), {ir.literal(bound_literal(ctx, *options.maximum))}));
    }
    return checks;
}

auto read_field(LoweringContext& ctx,
                ChildOccurrenceId field,
                Expr destination,
                bool constructing = false) -> NodeId {
    auto& ir{ctx.ir};
    auto arguments{constraint_arguments(ctx, field)};
    arguments.insert(arguments.begin(), destination);
    auto const reader{ir.named("read")};
    return ir.expr_stmt(
        ir.call(constructing ? ir.member_access(reader, "construct") : reader, arguments));
}

auto field_value(CppIR& ir, StringId name) -> Expr {
    return ir.type_reference(
        ir.types.decorate(ir.types.named("Field"), "", "::" + std::string{ir.strings.view(name)}));
}

auto member_reader(LoweringContext& ctx, DeclarationHandle record) -> Expr {
    auto& ir{ctx.ir};
    auto const& records{ctx.declarations.records};
    auto const& members{ctx.declarations.child_occurrences};
    struct Group {
        S7FieldOptions options;
        Expr condition;
        NodeId action;
    };
    std::vector<Group> groups;
    for (auto const field : ctx.declarations.items(records.members[record])) {
        auto const& options{members.s7[field]};
        auto const group{std::ranges::find_if(groups, [&](Group const& candidate) {
            auto const& other{candidate.options};
            return options.exclude == other.exclude &&
                   (options.exclude ||
                    (options.minimum == other.minimum && options.maximum == other.maximum &&
                     options.nonzero == other.nonzero && options.positive == other.positive &&
                     options.nonnegative == other.nonnegative && options.finite == other.finite &&
                     options.nonempty == other.nonempty));
        })};
        auto const condition{ir.binary(
            BinaryOperator::equal, ir.named("field"), field_value(ir, members.name[field]))};
        if (group != groups.end()) {
            group->condition = ir.binary(BinaryOperator::logical_or, group->condition, condition);
        } else {
            auto const action{options.exclude
                                  ? ir.expr_stmt(ir.call(ir.type_reference(
                                        ir.types.named("std::unreachable", NameOrigin::global))))
                                  : read_field(ctx, field, ir.named("member"))};
            groups.push_back({options, condition, action});
        }
    }
    Nodes body;
    if (groups.size() == 1) {
        body.push_back(groups.front().action);
    } else {
        for (auto const& group : groups) {
            body.push_back(ir.if_stmt(group.condition, ir.block({group.action}), {}, true));
        }
    }
    return ir.lambda({ir.unary(UnaryOperator::address_of, ir.named("read"))},
                     {ir.parameter("auto&", "member")},
                     ir.block(body),
                     ir.template_declaration({ir.value_template_parameter(
                         ir.types.named("Field"), ir.strings.intern("field"))}));
}

auto fields_body(LoweringContext& ctx, DeclarationHandle const record, bool const constructing)
    -> Nodes {
    auto& ir{ctx.ir};
    auto const& records{ctx.declarations.records};
    auto const row{records.index(record)};
    auto const& members{ctx.declarations.child_occurrences};
    auto const fields{reader_fields(ctx, record)};
    auto const positional{records.s7_positional[row] != 0};
    auto const decode_ctx{ir.named("ctx")};
    auto const errors{ir.member_access(decode_ctx, "errors")};
    auto move = [&](Expr value) {
        return ir.call(ir.type_reference(ir.types.named("std::move", NameOrigin::global)), {value});
    };
    auto const unexpected{ir.init_list({move(errors)}, ir.types.std.unexpected)};
    NodeListBuilder body{ir};
    if (constructing) {
        for (auto const field : fields) {
            auto const name{ctx.text(members.name[members.index(field)])};
            body.add(ir.variable(
                ir.types.template_type(
                    ir.types.std.optional,
                    {ir.types.decorate(
                        ir.types.decorate(ir.types.named("value_type"), "", "::" + name),
                        "decltype(",
                        ")")}),
                "value_" + name,
                ir.literal("")));
        }
    }

    if (positional) {
        body.add(ir.variable("auto const",
                             "arguments",
                             ir.call(ir.type_reference(ir.types.named(
                                         "ioj::s7::positional_arguments", NameOrigin::global)),
                                     {ir.member_access(decode_ctx, "reader"),
                                      ir.member_access(decode_ctx, "node"),
                                      ir.string_literal(ctx.text(*records.s7_tag[row])),
                                      ir.member_access(decode_ctx, "path"),
                                      ir.literal(std::to_string(fields.size()))})),
                 2);
        body.add(ir.if_stmt(
            ir.unary(UnaryOperator::logical_not, ir.named("arguments")),
            ir.block({ir.expr_stmt(ir.call(
                          ir.type_reference(
                              ir.types.named("ioj::s7::append_errors", NameOrigin::global)),
                          {errors, ir.call(ir.member_access(ir.named("arguments"), "error"))})),
                      constructing ? ir.return_stmt(unexpected) : ir.return_stmt()})));
        std::size_t index{};
        for (auto const field : fields) {
            auto const name{ctx.text(members.name[members.index(field)])};
            auto const destination{constructing ? ir.named("value_" + name)
                                                : ir.member_access(ir.named("result"), name)};
            auto const path{
                ir.binary(BinaryOperator::add,
                          ir.init_list({ir.member_access(decode_ctx, "path")}, ir.types.std.string),
                          ir.string_literal("." + field_name(ctx, field)))};
            auto const reader{ir.init_list(
                {ir.init_list(
                    {ir.member_access(decode_ctx, "reader"),
                     ir.subscript(ir.unary(UnaryOperator::dereference, ir.named("arguments")),
                                  ir.literal(std::to_string(index++))),
                     path,
                     ir.member_access(decode_ctx, "mode"),
                     errors})},
                ir.types.named("ioj::s7::FieldReader", NameOrigin::global))};
            auto arguments{constraint_arguments(ctx, field)};
            arguments.insert(arguments.begin(), destination);
            body.add(ir.expr_stmt(
                ir.call(constructing ? ir.member_access(reader, "construct") : reader, arguments)));
        }
    } else {
        Expr visitor;
        if (fields.empty()) {
            visitor =
                ir.lambda({}, {ir.parameter("Field"), ir.parameter("auto const&")}, ir.block({}));
        } else if (records.generate_visitor[row] && !constructing) {
            visitor = ir.lambda(
                {ir.unary(UnaryOperator::address_of, ir.named("result"))},
                {ir.parameter("Field", "selected"), ir.parameter("auto const&", "read")},
                ir.block({ir.expr_stmt(ir.call(
                    ir.type_reference(
                        ir.types.decorate(ir.types.named("value_type"), "", "::visit_field")),
                    {ir.named("result"), ir.named("selected"), member_reader(ctx, record)}))}));
        } else {
            std::vector<SwitchCaseId> cases;
            std::vector<Expr> captures;
            if (!constructing) {
                captures.push_back(ir.unary(UnaryOperator::address_of, ir.named("result")));
            }
            for (auto const field : fields) {
                auto const name{ctx.text(members.name[members.index(field)])};
                auto const destination{constructing ? ir.named("value_" + name)
                                                    : ir.member_access(ir.named("result"), name)};
                if (constructing) {
                    captures.push_back(ir.unary(UnaryOperator::address_of, destination));
                }
                cases.push_back(ir.switch_case(
                    field_value(ir, members.name[field]),
                    ir.block({read_field(ctx, field, destination, constructing), ir.break_stmt()}),
                    false));
            }
            if (records.generate_visitor[row]) {
                // Stage construction before an object exists; excluded fields cannot be looked up.
                cases.push_back(
                    ir.switch_case({},
                                   ir.block({ir.expr_stmt(ir.call(ir.type_reference(
                                       ir.types.named("std::unreachable", NameOrigin::global))))}),
                                   false));
            }
            std::array const parameters{ir.parameter("Field const", "field"),
                                        ir.parameter("auto const&", "read")};
            visitor = ir.lambda(
                captures, parameters, ir.block({ir.switch_stmt(ir.named("field"), cases)}));
        }
        std::vector<Expr> arguments{ir.named("ctx"),
                                    ir.string_literal(ctx.text(*records.s7_tag[row])),
                                    ir.named("field_names"),
                                    visitor};
        if (std::ranges::any_of(fields, [&](auto const field) {
                return members.s7[members.index(field)].required;
            })) {
            arguments.push_back(ir.named("required"));
        }
        body.add(ir.expr_stmt(ir.call(
            ir.template_id(ir.named("decode_record"), {ir.type_reference(ir.types.named("Field"))}),
            arguments)));
    }
    if (!constructing) {
        return body.build();
    }
    auto const return_errors{
        ir.if_stmt(ir.unary(UnaryOperator::logical_not, ir.call(ir.member_access(errors, "empty"))),
                   ir.block({ir.return_stmt(unexpected)}))};
    body.add(return_errors, 2);
    std::vector<Expr> initializers;
    for (auto const field : fields) {
        auto const index{members.index(field)};
        if (positional || members.s7[index].required) {
            auto const name{ctx.text(members.name[index])};
            initializers.push_back(ir.designated_initializer(
                members.name[index],
                move(ir.unary(UnaryOperator::dereference, ir.named("value_" + name)))));
        }
    }
    body.add(ir.variable("value_type", "result", ir.init_list(initializers)), 2);
    for (auto const field : fields) {
        auto const index{members.index(field)};
        auto const name{ctx.text(members.name[index])};
        auto const& options{members.s7[index]};
        if (!positional && !options.required) {
            body.add(ir.if_stmt(
                ir.named("value_" + name),
                ir.block({ir.assignment(
                    ir.member_access(ir.named("result"), name),
                    move(ir.unary(UnaryOperator::dereference, ir.named("value_" + name))))})));
        }
    }
    body.add(ir.return_stmt(ir.named("result")));
    return body.build();
}

}

void lower_record_reader(LoweringContext& ctx,
                         DeclarationHandle const record,
                         DeclarationEmission& emission) {
    auto& ir{ctx.ir};
    auto const& records{ctx.declarations.records};
    auto const row{records.index(record)};
    auto const fields{record_reader::reader_fields(ctx, record)};
    auto const empty{fields.empty()};
    auto const type{ctx.semantic.find_declared(ctx.module, records.name[row]).value()};
    auto const name{ctx.text(ctx.semantic.cpp_spelling(type))};
    auto const config_type{
        ir.types.named(name + (records.constraint_policy[row] ? "Value" : ""), NameOrigin::global)};
    auto const value_type{config_type};
    auto const& members{ctx.declarations.child_occurrences};
    if (std::ranges::any_of(fields, [&](auto field) {
            auto const& options{members.s7[members.index(field)]};
            return options.nonzero || options.positive || options.nonnegative || options.finite ||
                   options.nonempty || options.minimum || options.maximum;
        })) {
        emission.reader_includes.push_back(ir.strings.intern("ioj/s7/field_constraints.h"));
    }
    std::vector<EnumValueId> values;
    std::set<std::string> keys;
    for (auto const field : fields) {
        auto const member{members.name[members.index(field)]};
        auto const key{record_reader::field_name(ctx, field)};
        if (key.empty() || !std::ranges::all_of(key, [](unsigned char c) {
                return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
            })) {
            throw std::invalid_argument{
                "S7 field names must contain lowercase letters, digits or hyphens"};
        }
        if (!keys.insert(key).second) {
            throw std::invalid_argument{"Duplicate S7 field name: " + key};
        }
        values.push_back(ir.enumerator(member));
    }
    NodeListBuilder decls{ir};
    auto const includes{ctx.declarations.items(records.s7_reader_includes[row])};
    emission.reader_includes.insert(
        emission.reader_includes.end(), includes.begin(), includes.end());
    auto const add_function{[&](std::string const& method,
                                CppTypeId const return_type,
                                std::vector<FunctionParameterId> const& parameters,
                                Nodes body) {
        auto const function{ir.function(method, return_type)};
        ir.set_function_parameters(function, parameters);
        ir.set_function_receiver(function, FunctionReceiver::static_);
        ir.set_function_body(function, body);
        ir.function_flags(function).set_define_in_header(true);
        decls.add(ir.header_function(function), 2);
    }};
    std::vector<FunctionParameterId> const read_parameters{ir.parameter("DecodeContext", "ctx")};
    if (std::ranges::any_of(fields, [&](auto field) {
            auto const index{members.index(field)};
            return !members.initializer[index].has_value();
        })) {
        add_function(
            "construct",
            ir.types.template_type(ir.types.named("ioj::s7::ReadResult", NameOrigin::global),
                                   {ir.types.named("value_type")}),
            read_parameters,
            record_reader::fields_body(ctx, record, true));
    }
    auto decode_parameters{read_parameters};
    decode_parameters.push_back(ir.parameter("value_type&", empty ? "" : "result"));
    add_function("decode",
                 ir.types.std.void_,
                 decode_parameters,
                 record_reader::fields_body(ctx, record, false));
    if (!records.s7_positional[row]) {
        decls.add(ir.access(Access::private_), 1);
    }
    if (!records.s7_positional[row]) {
        if (records.generate_visitor[row]) {
            decls.add(ir.using_declaration(
                          ir.strings.intern("Field"),
                          ir.types.decorate(ir.types.named("value_type"), "", "::Field")),
                      2);
        } else {
            decls.add(ir.enumeration(ir.strings.intern("Field"),
                                     values.size() <= 256 ? ir.types.std.u8 : ir.types.std.u32,
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
                names.push_back(ir.string_literal(""));
                continue;
            }
            names.push_back(ir.string_literal(record_reader::field_name(ctx, field)));
            if (members.s7[index].required) {
                required.push_back(ir.type_reference(ir.types.decorate(
                    ir.types.named("Field"), "", "::" + ctx.text(members.name[index]))));
            }
        }
        auto const array_type{ir.types.template_type(
            ir.types.std.array,
            {ir.types.std.string_view, ir.types.intern(std::to_string(named_fields.size()))})};
        decls.add(
            ir.constexpr_member(array_type, ir.strings.intern("field_names"), ir.init_list(names)),
            1);
        if (!required.empty()) {
            decls.add(ir.constexpr_member(
                          ir.types.intern("auto"),
                          ir.strings.intern("required"),
                          ir.call(ir.template_id(
                                      ir.named("required_fields"),
                                      {ir.type_reference(ir.types.named("Field")),
                                       ir.call(ir.member_access(ir.named("field_names"), "size"))}),
                                  {ir.init_list(required)})),
                      1);
        }
    }
    auto const specialization{ir.record(
        ir.template_id(ir.named("TypeReader"), {ir.type_reference(value_type)}), decls.build())};
    ir.set_record_template(specialization, ir.template_specialization());
    std::array const inherited_names{ir.strings.intern("value_type"), ir.strings.intern("read")};
    ir.set_scope_bindings(specialization, inherited_names);
    ir.set_record_bases(
        specialization,
        {ir.types.template_type(ir.types.named("ioj::s7::RecordReader",
                                               NameOrigin::global,
                                               {ir.types.dependency("ioj::s7::RecordReader",
                                                                    "ioj/s7/record_reader.h")}),
                                {value_type})});
    emission.reader_declarations.push_back(specialization);

    for (auto const dependency : ctx.semantic.dependencies_of(type)) {
        auto const declaration{ctx.semantic.source_declaration(dependency)};
        auto const module{ctx.semantic.module(dependency)};
        if (declaration.value != 0 && declaration.kind() == DeclarationKind::record &&
            records.s7_tag[declaration] && module != ctx.module) {
            emission.reader_includes.push_back(ir.strings.intern(
                reader_header_path(source_include(ctx, ctx.declarations.module_settings(module)))));
        }
    }
}
}
