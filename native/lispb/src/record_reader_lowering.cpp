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

auto read_function(CppIR& ir, std::string const& name, std::string const& type) -> FunctionId {
    auto const function{ir.function(name, "ReadResult<" + type + ">")};
    ir.set_function_parameters(function,
                               {ir.parameter("Reader const&", "reader"),
                                ir.parameter("NodeIndex const", "node"),
                                ir.parameter("std::string_view const", "path")});
    return function;
}

auto read_body(LoweringContext& context,
               DeclarationHandle const record,
               DeclarationEmission& emission) -> Nodes {
    auto& ir{context.ir};
    auto const& records{context.declarations.records};
    auto const row{records.index(record)};
    auto const& members{context.declarations.record_members};
    auto const fields{context.declarations.items(records.members[row])};
    NodeListBuilder body{ir};
    body.add(ir.variable("AstDiagnostics", "errors", ir.literal("")));
    for (auto const field : fields) {
        auto const name{context.text(members.name[members.index(field)])};
        body.add(ir.variable(
            "std::optional<decltype(Value::" + name + ")>", "value_" + name, ir.literal("")));
    }

    // The IR has no lambda node; keep only the streaming callback's syntax textual.
    std::string visitor{"[&](Field const field, NodeIndex const value, std::string_view const "
                        "location) {\nswitch (field) {\n"};
    for (auto const field : fields) {
        auto const name{context.text(members.name[members.index(field)])};
        visitor += std::format(
            "case Field::{}: read_field(reader, value, location, value_{}, errors); break;\n",
            name,
            name);
    }
    visitor += "}\n}";
    if (fields.empty()) {
        visitor = "[](Field, NodeIndex, std::string_view) {}";
    }
    body.add(ir.variable("auto const",
                         "seen",
                         ir.call(ir.named(std::format("read_record<{}>", fields.size())),
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
                  ir.return_stmt(ir.raw_expr("std::unexpected{std::move(errors)}", {}))})));

    std::vector<std::string> initializers;
    std::size_t ordinal{};
    for (auto const field : fields) {
        auto const index{members.index(field)};
        auto const name{context.text(members.name[index])};
        if (members.s7[index].required) {
            body.add(ir.if_stmt(
                ir.raw_expr(std::format("!seen->test({})", ordinal), {}),
                ir.block({ir.expr_stmt(ir.call(
                    ir.named("errors.push_back"),
                    {ir.raw_expr(
                        std::format(
                            "AstDiagnostic{{AstErrorCode::MissingProperty, std::string{{path}} + "
                            "\".{}\", \"Missing required property ':{}'\"}}",
                            field_name(context, field),
                            field_name(context, field)),
                        {})}))})));
            initializers.push_back("." + name + " = std::move(*value_" + name + ")");
        }
        ++ordinal;
    }
    auto const return_errors{ir.if_stmt(
        ir.raw_expr("!errors.empty()", {}),
        ir.block({ir.return_stmt(ir.raw_expr("std::unexpected{std::move(errors)}", {}))}))};
    body.add(return_errors, 2);
    body.add(ir.variable("Value", "result", ir.raw_expr(join(initializers, ",\n"), {})), 2);
    bool has_constraints{};
    for (auto const field : fields) {
        auto const index{members.index(field)};
        auto const name{context.text(members.name[index])};
        auto const& options{members.s7[index]};
        if (!options.required) {
            body.add(ir.if_stmt(
                ir.named("value_" + name),
                ir.block({ir.assignment(ir.named("result." + name),
                                        ir.raw_expr("std::move(*value_" + name + ")", {}))})));
        }
        auto const constraint =
            [&](std::string const& check, std::string const& message, bool const numeric = true) {
                has_constraints = true;
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
            constraint("nonzero(" + value + ")", "Expected a nonzero value");
        }
        if (options.positive) {
            constraint("positive(" + value + ")", "Expected a positive value");
        }
        if (options.nonnegative) {
            constraint("nonnegative(" + value + ")", "Expected a nonnegative value");
        }
        if (options.finite) {
            constraint("finite(" + value + ")", "Expected a finite value");
        }
        if (options.nonempty) {
            constraint("nonempty(" + value + ")", "Expected a nonempty string", false);
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
    if (has_constraints) {
        emission.reader_includes.push_back(ir.strings.intern("ioj/s7/field_constraints.h"));
        body.add(return_errors, 2);
    }
    body.add(ir.return_stmt(ir.named("result")));
    return body.build();
}
}

void lower_record_reader(LoweringContext& context,
                         DeclarationHandle const record,
                         DeclarationEmission& emission) {
    auto& ir{context.ir};
    auto const& records{context.declarations.records};
    auto const row{records.index(record)};
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
    declarations.add(ir.using_declaration(ir.strings.intern("Value"), ir.types.intern(name)), 2);
    auto const function{record_reader::read_function(ir, "read", "Value")};
    ir.set_function_receiver(function, FunctionReceiver::static_);
    declarations.add(ir.header_function(function), 2);
    declarations.add(ir.access(Access::private_), 1);
    declarations.add(
        ir.enumeration(ir.strings.intern("Field"), ir.types.intern("std::size_t"), values), 2);
    declarations.add(lower_string_enum_parser(ir, "parse_field", "Field", "name", entries, true));
    auto const specialization{ir.record(ir.strings.intern(reader_name), declarations.build())};
    ir.set_record_template(specialization, ir.template_specialization());
    emission.reader_declarations.push_back(specialization);

    auto const definition{record_reader::read_function(ir, reader_name + "::read", name)};
    ir.function_flags(definition).set_is_inline(true);
    ir.function_flags(definition).set_define_in_header(true);
    auto const includes{context.declarations.items(records.s7_reader_includes[row])};
    emission.reader_includes.assign(includes.begin(), includes.end());
    ir.set_function_body(definition, record_reader::read_body(context, record, emission));
    emission.reader_definitions.push_back(ir.header_function(definition));
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
