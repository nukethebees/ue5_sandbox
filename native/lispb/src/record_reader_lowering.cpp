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
    if (members.settings_name[row]) {
        return context.text(*members.settings_name[row]);
    }
    auto name{context.text(members.name[row])};
    std::ranges::replace(name, '_', '-');
    return name;
}

auto reader_fields(LoweringContext& context, DeclarationHandle record)
    -> std::vector<RecordMemberId> {
    auto const& declarations{context.declarations};
    auto const& records{declarations.records};
    std::vector<RecordMemberId> result;
    for (auto field : declarations.items(records.members[records.index(record)])) {
        if (!records.generate_settings[records.index(record)] ||
            !declarations.record_members
                 .settings_exclude[declarations.record_members.index(field)]) {
            result.push_back(field);
        }
    }
    return result;
}

auto constraint_arguments(LoweringContext& context, RecordMemberId field) -> std::string {
    auto const& members{context.declarations.record_members};
    auto const& options{members.s7[members.index(field)]};
    std::string checks;
    auto const constraint{
        [&](std::string_view check) { checks += std::format(", constraints::{}", check); }};
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
        constraint(std::format("minimum({})", bound_literal(context, *options.minimum)));
    }
    if (options.maximum) {
        constraint(std::format("maximum({})", bound_literal(context, *options.maximum)));
    }
    return checks;
}

auto fields_body(LoweringContext& context, DeclarationHandle const record, bool const constructing)
    -> Nodes {
    auto& ir{context.ir};
    auto const& records{context.declarations.records};
    auto const row{records.index(record)};
    auto const& members{context.declarations.record_members};
    auto const fields{reader_fields(context, record)};
    auto const positional{records.s7_positional[row] != 0};
    NodeListBuilder body{ir};
    if (constructing) {
        for (auto const field : fields) {
            auto const name{context.text(members.name[members.index(field)])};
            body.add(ir.variable("std::optional<decltype(value_type::" + name + ")>",
                                 "value_" + name,
                                 ir.literal("")));
        }
    }

    auto const read_method{constructing ? ".construct" : ""};
    auto const patch{records.generate_overrides[row] && !constructing};
    if (positional) {
        body.add(ir.variable("auto const",
                             "arguments",
                             ir.raw_expr(std::format("positional_arguments(context.reader, "
                                                     "context.node, \"{}\", context.path, {})",
                                                     context.text(*records.s7_tag[row]),
                                                     fields.size()),
                                         {})),
                 2);
        body.add(ir.if_stmt(
            ir.raw_expr("!arguments", {}),
            ir.block(
                {ir.expr_stmt(ir.raw_expr("append_errors(context.errors, arguments.error())", {})),
                 constructing
                     ? ir.return_stmt(ir.raw_expr("std::unexpected{std::move(context.errors)}", {}))
                     : ir.return_stmt()})));
        std::size_t index{};
        for (auto const field : fields) {
            auto const name{context.text(members.name[members.index(field)])};
            auto const destination{(constructing ? "value_" : "result.") + name};
            auto const path{
                std::format("std::string{{context.path}} + \".{}\"", field_name(context, field))};
            body.add(ir.raw(std::format("FieldReader{}{{{{context.reader, (*arguments)[{}], {}, "
                                        "context.mode, context.errors}}}}"
                                        "{}({}{});\n",
                                        patch ? "<is_patch>" : "",
                                        index++,
                                        path,
                                        read_method,
                                        destination,
                                        constraint_arguments(context, field)),
                            {}));
        }
    } else {
        // The IR has no lambda node; keep only the streaming callback's syntax textual.
        std::string visitor{"[&](Field const field, auto const& read) {\nswitch (field) {\n"};
        for (auto const field : fields) {
            auto const name{context.text(members.name[members.index(field)])};
            auto const destination{(constructing ? "value_" : "result.") + name};
            visitor += std::format("case Field::{}: read{}({}{}); break;\n",
                                   name,
                                   read_method,
                                   destination,
                                   constraint_arguments(context, field));
        }
        visitor += "case Field::Count: std::unreachable();\n}\n}";
        if (fields.empty()) {
            visitor = "[](Field, auto const&) {}";
        }
        std::vector<Expr> arguments{ir.named("context"),
                                    ir.string_literal(context.text(*records.s7_tag[row])),
                                    ir.named("field_names"),
                                    ir.raw_expr(visitor, {})};
        if (std::ranges::any_of(fields, [&](auto const field) {
                return members.s7[members.index(field)].required;
            })) {
            arguments.push_back(ir.named("required"));
        }
        body.add(ir.expr_stmt(
            ir.call(ir.named(patch ? "decode_record<Field, is_patch>" : "decode_record<Field>"),
                    arguments)));
    }
    if (!constructing) {
        return body.build();
    }
    auto const return_errors{ir.if_stmt(
        ir.raw_expr("!context.errors.empty()", {}),
        ir.block({ir.return_stmt(ir.raw_expr("std::unexpected{std::move(context.errors)}", {}))}))};
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

}

void lower_record_reader(LoweringContext& context,
                         DeclarationHandle const record,
                         DeclarationEmission& emission) {
    auto& ir{context.ir};
    auto const& records{context.declarations.records};
    auto const row{records.index(record)};
    auto const fields{record_reader::reader_fields(context, record)};
    auto const empty{fields.empty()};
    auto const paired{records.generate_overrides[row] != 0};
    auto const type{context.semantic.find_declared(context.module, records.name[row]).value()};
    auto const name{context.text(context.semantic.cpp_spelling(type))};
    auto const config_type{ir.types.named(name + (records.constraint_policy[row] ? "Value" : ""))};
    auto const value_type{paired ? ir.types.named("T") : config_type};
    auto const patch_type{ir.types.named(name + "Overrides")};
    auto const same_type{[&](CppTypeId other) {
        return ir.template_id(
            ir.named("std::same_as", {ir.types.dependency("std::same_as", "concepts")}),
            {ir.type_reference(value_type), ir.type_reference(other)});
    }};
    auto const& members{context.declarations.record_members};
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
    }
    NodeListBuilder declarations{ir};
    if (paired) {
        declarations.add(ir.using_declaration(ir.strings.intern("value_type"), value_type), 2);
    }
    auto const includes{context.declarations.items(records.s7_reader_includes[row])};
    emission.reader_includes.insert(
        emission.reader_includes.end(), includes.begin(), includes.end());
    auto const add_function{[&](std::string const& method,
                                std::string const& return_type,
                                std::vector<FunctionParameterId> const& parameters,
                                Nodes body) {
        auto const function{ir.function(method, return_type)};
        ir.set_function_parameters(function, parameters);
        ir.set_function_receiver(function, FunctionReceiver::static_);
        ir.set_function_body(function, body);
        ir.function_flags(function).set_define_in_header(true);
        declarations.add(ir.header_function(function), 2);
    }};
    std::vector<FunctionParameterId> const read_parameters{
        ir.parameter("DecodeContext", "context")};
    if (std::ranges::any_of(fields, [&](auto field) {
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
    add_function(
        "decode", "void", decode_parameters, record_reader::fields_body(context, record, false));
    if (paired || !records.s7_positional[row]) {
        declarations.add(ir.access(Access::private_), 1);
    }
    if (!records.s7_positional[row]) {
        values.push_back(ir.enumerator(ir.strings.intern("Count")));
        declarations.add(
            ir.enumeration(ir.strings.intern("Field"),
                           ir.types.intern(values.size() <= 256 ? "std::uint8_t" : "std::uint32_t"),
                           values),
            2);
        std::vector<Expr> names;
        std::vector<Expr> required;
        for (auto const field : fields) {
            auto const index{members.index(field)};
            names.push_back(ir.string_literal(record_reader::field_name(context, field)));
            if (members.s7[index].required) {
                required.push_back(ir.named("Field::" + context.text(members.name[index])));
            }
        }
        auto const array_type{ir.types.template_type(
            ir.types.named("std::array", {ir.types.dependency("std::array", "array")}),
            {ir.types.named("std::string_view",
                            {ir.types.dependency("std::string_view", "string_view")}),
             ir.types.intern(std::to_string(fields.size()))})};
        declarations.add(
            ir.constexpr_member(array_type, ir.strings.intern("field_names"), ir.init_list(names)),
            1);
        if (!required.empty()) {
            declarations.add(ir.constexpr_member(ir.types.intern("auto"),
                                                 ir.strings.intern("required"),
                                                 ir.call(ir.named("required_fields<Field>"),
                                                         {ir.init_list(required)})),
                             1);
        }
    }
    if (paired) {
        declarations.add(ir.constexpr_member(ir.types.intern("bool"),
                                             ir.strings.intern("is_patch"),
                                             same_type(patch_type)),
                         1);
    }
    auto const specialization{
        ir.record(ir.template_id(ir.named("TypeReader"), {ir.type_reference(value_type)}),
                  declarations.build())};
    if (paired) {
        ir.set_record_template(
            specialization,
            ir.template_declaration({ir.type_template_parameter(ir.strings.intern("T"))}));
        ir.set_record_requires_clause(
            specialization,
            ir.template_id(ir.named("ioj::SameAsAny",
                                    {ir.types.dependency("ioj::SameAsAny", "ioj/type_traits.h")}),
                           {ir.type_reference(value_type),
                            ir.type_reference(config_type),
                            ir.type_reference(patch_type)}));
    } else {
        ir.set_record_template(specialization, ir.template_specialization());
    }
    std::vector<CppTypeId> base_arguments{value_type};
    if (paired) {
        base_arguments.push_back(patch_type);
    }
    ir.set_record_bases(
        specialization,
        {ir.types.template_type(ir.types.named("ioj::s7::RecordReader",
                                               {ir.types.dependency("ioj::s7::RecordReader",
                                                                    "ioj/s7/record_reader.h")}),
                                base_arguments)});
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
