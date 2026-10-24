#include "record_reader_lowering.h"

#include "declaration_emission.h"
#include "lowering_context.h"
#include "lowering_utils.h"
#include "string_enum_parser.h"

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/schema/declarations.h>
#include <lispb/schema/type_graph.h>
#include <lispb/strings.h>

#include <algorithm>
#include <filesystem>
#include <format>
#include <set>
#include <stdexcept>

namespace lispb::codegen::detail {
auto reader_header_path(std::string_view const header) -> std::string {
    auto const path{std::filesystem::path{header}};
    return (path.parent_path() / (path.stem().string() + "_reader" + path.extension().string()))
        .generic_string();
}

namespace record_reader {
auto bound_literal(LoweringContext& context, StringId const bound) -> std::string {
    auto const text{context.text(bound)};
    if (text.find('.') != std::string::npos) {
        return text;
    }
    if (text == "-9223372036854775808") {
        return "(-9223372036854775807LL - 1LL)";
    }
    return text + (text.starts_with('-') ? "LL" : "ULL");
}

auto field_name(LoweringContext& context, RecordMemberId const member) -> std::string {
    auto const& members{context.declarations.record_members};
    auto const row{members.index(member)};
    if (members.s7[row].name) {
        return context.text(*members.s7[row].name);
    }
    auto name{context.text(members.name[row])};
    std::ranges::replace(name, '_', '-');
    return name;
}

auto fields_body(LoweringContext& context, DeclarationHandle const record, bool const constructing)
    -> Nodes {
    auto& ir{context.ir};
    auto const& records{context.declarations.records};
    auto const row{records.index(record)};
    auto const& members{context.declarations.record_members};
    auto const fields{context.declarations.items(records.members[row])};
    auto const positional{records.s7_positional[row] != 0};
    NodeListBuilder body{ir};
    if (constructing) {
        body.add(ir.variable("AstDiagnostics", "errors", ir.literal("")));
        for (auto const field : fields) {
            auto const name{context.text(members.name[members.index(field)])};
            body.add(ir.variable("std::optional<decltype(value_type::" + name + ")>",
                                 "value_" + name,
                                 ir.literal("")));
        }
    }

    auto const read_value{constructing
                              ? "read_field(reader, value, location, destination, errors)"
                              : "decode_field(reader, value, location, destination, mode, errors)"};
    if (positional) {
        body.add(ir.variable(
                     "auto const",
                     "arguments",
                     ir.raw_expr(std::format("positional_arguments(reader, node, \"{}\", path, {})",
                                             context.text(*records.s7_tag[row]),
                                             fields.size()),
                                 {})),
                 2);
        body.add(ir.if_stmt(
            ir.raw_expr("!arguments", {}),
            ir.block({ir.expr_stmt(ir.raw_expr("append_errors(errors, arguments.error())", {})),
                      constructing
                          ? ir.return_stmt(ir.raw_expr("std::unexpected{std::move(errors)}", {}))
                          : ir.return_stmt()})));
        std::size_t index{};
        for (auto const field : fields) {
            auto const name{context.text(members.name[members.index(field)])};
            body.add(ir.expr_stmt(ir.raw_expr(
                std::format(
                    "{}(reader, (*arguments)[{}], std::string{{path}} + \".{}\", {}{}, {}errors)",
                    constructing ? "read_field" : "decode_field",
                    index++,
                    field_name(context, field),
                    constructing ? "value_" : "result.",
                    name,
                    constructing ? "" : "mode, "),
                {})));
        }
    } else {
        // The IR has no lambda node; keep only the streaming callback's syntax textual.
        std::string visitor{"[&](Field const field, NodeIndex const value, std::string_view const "
                            "location) {\nauto const read = [&](auto& destination) {\n"};
        visitor += std::string{read_value} + ";\n};\nswitch (field) {\n";
        for (auto const field : fields) {
            auto const name{context.text(members.name[members.index(field)])};
            visitor += std::format("case Field::{}: read({}{}); break;\n",
                                   name,
                                   constructing ? "value_" : "result.",
                                   name);
        }
        visitor += "case Field::Count: std::unreachable();\n}\n}";
        if (fields.empty()) {
            visitor = "[](Field, NodeIndex, std::string_view) {}";
        }
        body.add(ir.variable("auto const",
                             "seen",
                             ir.call(ir.named("read_record<Field>"),
                                     {ir.named("reader"),
                                      ir.named("node"),
                                      ir.string_literal(context.text(*records.s7_tag[row])),
                                      ir.named("path"),
                                      ir.named("parse_field"),
                                      ir.raw_expr(visitor, {})})),
                 2);
        body.add(ir.if_stmt(
            ir.raw_expr("!seen", {}),
            ir.block({ir.expr_stmt(ir.raw_expr("append_errors(errors, seen.error())", {})),
                      constructing
                          ? ir.return_stmt(ir.raw_expr("std::unexpected{std::move(errors)}", {}))
                          : ir.return_stmt()})));

        std::vector<std::string> required_names;
        bool has_required{};
        for (auto const field : fields) {
            auto const index{members.index(field)};
            auto const name{context.text(members.name[index])};
            auto const required{members.s7[index].required};
            required_names.push_back(
                std::format("\"{}\"", required ? field_name(context, field) : ""));
            if (required) {
                has_required = true;
            }
        }
        if (has_required) {
            body.add(ir.variable(
                         std::format("std::array<std::string_view, {}> constexpr", fields.size()),
                         "required_names",
                         ir.raw_expr(join(required_names, ", "), {})),
                     2);
            body.add(ir.variable("auto constexpr",
                                 "required",
                                 ir.raw_expr("required_fields<Field>(required_names)", {})));
            auto const check{ir.expr_stmt(ir.raw_expr(
                "require_fields<Field>(*seen, required, required_names, path, errors)", {}))};
            body.add(constructing ? check
                                  : ir.if_stmt(ir.raw_expr("mode == ReadMode::Create", {}),
                                               ir.block({check})));
        }
    }
    if (!constructing) {
        return body.build();
    }
    auto const return_errors{ir.if_stmt(
        ir.raw_expr("!errors.empty()", {}),
        ir.block({ir.return_stmt(ir.raw_expr("std::unexpected{std::move(errors)}", {}))}))};
    body.add(return_errors, 2);
    std::vector<std::string> initializers;
    for (auto const field : fields) {
        auto const index{members.index(field)};
        if (positional || members.s7[index].required) {
            auto const name{context.text(members.name[index])};
            initializers.push_back("." + name + " = std::move(*value_" + name + ")");
        }
    }
    body.add(ir.variable("value_type", "result", ir.raw_expr(join(initializers, ",\n"), {})), 2);
    for (auto const field : fields) {
        auto const index{members.index(field)};
        auto const name{context.text(members.name[index])};
        auto const& options{members.s7[index]};
        if (!positional && !options.required) {
            body.add(ir.if_stmt(
                ir.named("value_" + name),
                ir.block({ir.assignment(ir.named("result." + name),
                                        ir.raw_expr("std::move(*value_" + name + ")", {}))})));
        }
    }
    body.add(ir.return_stmt(ir.named("result")));
    return body.build();
}

auto validation_body(LoweringContext& context,
                     DeclarationHandle record,
                     DeclarationEmission& emission) -> Nodes {
    auto& ir{context.ir};
    auto const& records{context.declarations.records};
    auto const& members{context.declarations.record_members};
    NodeListBuilder body{ir};
    for (auto const field : context.declarations.items(records.members[records.index(record)])) {
        auto const index{members.index(field)};
        auto const name{context.text(members.name[index])};
        auto const& options{members.s7[index]};
        body.add(
            ir.expr_stmt(ir.raw_expr(std::format("validate_member(result.{}, path, \"{}\", errors)",
                                                 name,
                                                 field_name(context, field)),
                                     {})));
        auto const constraint =
            [&](std::string const& check, std::string const& message, bool const numeric = true) {
                emission.reader_includes.push_back(ir.strings.intern("ioj/s7/field_constraints.h"));
                body.add(ir.if_stmt(
                    ir.raw_expr("!constraints::" + check, {}),
                    ir.block({ir.expr_stmt(
                        ir.call(ir.named("errors.push_back"),
                                {ir.raw_expr(std::format("AstDiagnostic{{AstErrorCode::{}, "
                                                         "std::string{{path}} + \".{}\", \"{}\"}}",
                                                         numeric ? "InvalidNumber" : "InvalidType",
                                                         field_name(context, field),
                                                         message),
                                             {})}))})));
            };
        auto const value{"result." + name};
        if (options.nonzero) {
            constraint(std::format("nonzero({})", value), "Expected a nonzero value");
        }
        if (options.positive) {
            constraint(std::format("positive({})", value), "Expected a positive value");
        }
        if (options.nonnegative) {
            constraint(std::format("nonnegative({})", value), "Expected a nonnegative value");
        }
        if (options.finite) {
            constraint(std::format("finite({})", value), "Expected a finite value");
        }
        if (options.nonempty) {
            constraint(std::format("nonempty({})", value), "Expected a nonempty string", false);
        }
        if (options.minimum) {
            constraint(
                std::format("minimum({}, {})", value, bound_literal(context, *options.minimum)),
                "Value is below the minimum");
        }
        if (options.maximum) {
            constraint(
                std::format("maximum({}, {})", value, bound_literal(context, *options.maximum)),
                "Value is above the maximum");
        }
    }
    return body.build();
}
}

void lower_record_reader(LoweringContext& context,
                         DeclarationHandle const record,
                         DeclarationEmission& emission) {
    auto& ir{context.ir};
    auto const& records{context.declarations.records};
    auto const row{records.index(record)};
    auto const empty{records.members[row].empty()};
    auto const type{context.semantic.find_declared(context.module, records.name[row]).value()};
    auto const name{"::" + context.text(context.semantic.cpp_spelling(type))};
    auto const reader_name{"TypeReader<" + name + ">"};
    auto const& members{context.declarations.record_members};
    std::vector<EnumValueId> values;
    std::vector<StringEnumEntry> entries;
    std::set<std::string> keys;
    for (auto const field : context.declarations.items(records.members[row])) {
        auto const member{members.name[members.index(field)]};
        auto const key{record_reader::field_name(context, field)};
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
        entries.push_back({key, "Field::" + context.text(member)});
    }
    NodeListBuilder declarations{ir};
    declarations.add(ir.using_declaration(ir.strings.intern("value_type"), ir.types.intern(name)),
                     2);
    auto const includes{context.declarations.items(records.s7_reader_includes[row])};
    emission.reader_includes.assign(includes.begin(), includes.end());
    auto const add_function{[&](std::string const& method,
                                std::string const& return_type,
                                std::vector<FunctionParameterId> const& parameters,
                                Nodes body) {
        auto const function{ir.function(method, return_type)};
        ir.set_function_parameters(function, parameters);
        ir.set_function_receiver(function, FunctionReceiver::static_);
        declarations.add(ir.header_function(function), 2);
        auto const definition{ir.function(
            reader_name + "::" + method,
            return_type == "ReadResult<value_type>" ? "ReadResult<" + name + ">" : return_type)};
        ir.set_function_parameters(definition, parameters);
        ir.function_flags(definition).set_is_inline(true);
        ir.function_flags(definition).set_define_in_header(true);
        ir.set_function_body(definition, body);
        emission.reader_definitions.push_back(ir.header_function(definition));
    }};
    std::vector<FunctionParameterId> const read_parameters{
        ir.parameter("Reader const&", "reader"),
        ir.parameter("NodeIndex const", "node"),
        ir.parameter("std::string_view const", "path")};
    add_function(
        "read",
        "ReadResult<value_type>",
        read_parameters,
        {ir.return_stmt(ir.raw_expr("read_generated<value_type>(reader, node, path)", {}))});
    if (std::ranges::any_of(context.declarations.items(records.members[row]), [&](auto field) {
            auto const index{members.index(field)};
            return !members.initializer[index].has_value();
        })) {
        add_function("construct",
                     "ReadResult<value_type>",
                     read_parameters,
                     record_reader::fields_body(context, record, true));
    }
    auto decode_parameters{read_parameters};
    decode_parameters.push_back(ir.parameter("value_type&", empty ? "" : "result"));
    decode_parameters.push_back(ir.parameter("ReadMode", empty ? "" : "mode"));
    decode_parameters.push_back(ir.parameter("AstDiagnostics&", "errors"));
    add_function(
        "decode", "void", decode_parameters, record_reader::fields_body(context, record, false));
    add_function("validate",
                 "void",
                 {ir.parameter("value_type const&", empty ? "" : "result"),
                  ir.parameter("std::string_view", empty ? "" : "path"),
                  ir.parameter("AstDiagnostics&", empty ? "" : "errors")},
                 record_reader::validation_body(context, record, emission));
    if (!records.s7_positional[row]) {
        declarations.add(ir.access(Access::private_), 1);
        values.push_back(ir.enumerator(ir.strings.intern("Count")));
        declarations.add(
            ir.enumeration(ir.strings.intern("Field"),
                           ir.types.intern(values.size() <= 256 ? "std::uint8_t" : "std::uint32_t"),
                           values),
            2);
        declarations.add(
            lower_string_enum_parser(ir, "parse_field", "Field", "name", entries, true));
    }
    auto const specialization{ir.record(ir.strings.intern(reader_name), declarations.build())};
    ir.set_record_template(specialization, ir.template_specialization());
    emission.reader_declarations.push_back(specialization);

    for (auto const dependency : context.semantic.dependencies_of(type)) {
        auto const declaration{context.semantic.source_declaration(dependency)};
        auto const module{context.semantic.module(dependency)};
        if (declaration.value != 0 && declaration.kind() == DeclarationKind::record &&
            records.s7_tag[records.index(declaration)] && module != context.module) {
            emission.reader_includes.push_back(ir.strings.intern(reader_header_path(
                source_include(context, context.declarations.module_settings(module)))));
        }
    }
}
}
