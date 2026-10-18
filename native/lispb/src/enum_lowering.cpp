#include "declaration_emission.h"
#include "lowering.h"
#include "lowering_context.h"
#include "lowering_utils.h"
#include "string_enum_parser.h"
#include <ioj/ascii.h>

#include <codegen/ast/binary_operator.h>
#include <codegen/ast/cpp_files.h>
#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/expr.h>
#include <codegen/ast/function_flags.h>
#include <codegen/ast/include_style.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/modules.h>
#include <codegen/ast/node_id.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/ast/nodes.h>
#include <codegen/ast/return_syntax.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/enum_conversion.h>
#include <codegen/schema/enum_reflection.h>
#include <codegen/schema/enum_schemas.h>
#include <codegen/schema/enum_unreal_projections.h>
#include <codegen/schema/enumerator_schemas.h>
#include <codegen/schema/module_settings_store.h>
#include <codegen/schema/native_enum_helper.h>
#include <lispb/indices.h>
#include <lispb/schema/enum_domains.h>
#include <lispb/schema/enum_storage_requirement.h>
#include <lispb/schema/type_graph.h>
#include <lispb/strings.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lispb::codegen::detail {
namespace {

auto text_literal(CppIR& ir, std::string_view const value) -> Expr {
    return ir.call(ir.named("TEXT", {ir.types.dependency("TEXT", "CoreMinimal.h")}),
                   {ir.string_literal(value)});
}

auto qualified_enum_name(LoweringContext& context,
                         ModuleSettingsId settings,
                         DeclarationHandle schema) -> std::string {
    if (context.declarations.module_settings_store
            .namespace_name[context.declarations.module_settings_store.index(settings)]
            .has_value()) {
        return context.text(*context.declarations.module_settings_store
                                 .namespace_name[context.declarations.module_settings_store.index(
                                     settings)]) +
               "::" +
               context.text(
                   context.declarations.enums.name[context.declarations.enums.index(schema)]);
    }
    return context.text(context.declarations.enums.name[context.declarations.enums.index(schema)]);
}

auto snake_case_type_name(std::string_view const name) -> std::string {
    auto const begin{name.size() > 1 && name.front() == 'E' && ioj::isupper(name[1])
                         ? std::size_t{1}
                         : std::size_t{0}};
    std::string result;
    for (auto index{begin}; index < name.size(); ++index) {
        auto const character{name[index]};
        auto const upper{ioj::isupper(character)};
        auto const previous_lower{index > begin && ioj::islower(name[index - 1])};
        auto const next_lower{index + 1 < name.size() && ioj::islower(name[index + 1])};
        if (upper && !result.empty() && (previous_lower || next_lower)) {
            result += '_';
        }
        result += ioj::tolower(character);
    }
    return result;
}

auto internal_name(LoweringContext& context,
                   DeclarationHandle schema,
                   std::string_view const suffix) -> std::string {
    return "get_" +
           snake_case_type_name(context.text(
               context.declarations.enums.name[context.declarations.enums.index(schema)])) +
           "_" + std::string{suffix};
}

auto enum_count_specialization(CppIR& ir, CppTypeId enum_type, std::size_t const count) -> NodeId {
    auto const function{
        ir.function(ir.template_id(ir.named("enum_count"), {ir.type_reference(enum_type)}),
                    ir.types.intern("auto"))};
    ir.set_function_template(function, ir.template_specialization());
    ir.set_function_return_type(
        function,
        ir.types.intern("std::size_t", {ir.types.dependency("std::size_t", "cstddef")}),
        ReturnSyntax::trailing);
    auto& flags{ir.function_flags(function)};
    flags.set_is_nodiscard(true);
    flags.set_is_constexpr(true);
    ir.set_function_noexcept(function);
    flags.set_define_in_header(true);
    ir.set_function_body(function, {ir.return_statement(ir.literal(std::to_string(count)))});
    return ir.header_function(function);
}

auto exact_lookup(LoweringContext& context, ModuleSettingsId settings, DeclarationHandle schema)
    -> FunctionId {
    auto& ir{context.ir};
    auto const enum_name{qualified_enum_name(context, settings, schema)};
    NodeListBuilder body{ir};
    std::vector<SwitchCaseId> cases;
    for (auto const value_id : context.declarations.items(
             context.declarations.enums.values[context.declarations.enums.index(schema)])) {
        auto const value{value_id};
        cases.push_back(ir.switch_case(
            ir.named(enum_name + "::" +
                     context.text(context.declarations.enumerators
                                      .name[context.declarations.enumerators.index(value)])),
            ir.block({ir.return_statement(text_literal(
                ir,
                context.text(context.declarations.enumerators
                                 .name[context.declarations.enumerators.index(value)])))})));
    }
    body.add(ir.switch_statement(ir.named("value"), std::move(cases)), 2)
        .add(ir.raw("ensureMsgf(false,\n"
                    "           TEXT(\"Unhandled " +
                    context.text(
                        context.declarations.enums.name[context.declarations.enums.index(schema)]) +
                    " value: %lld\"),\n"
                    "           static_cast<int64>(value));"),
             1)
        .add(ir.return_statement(text_literal(
            ir,
            "<invalid " +
                context.text(
                    context.declarations.enums.name[context.declarations.enums.index(schema)]) +
                ">")));
    auto const created_function{ir.function(
        ir.strings.intern(internal_name(context, schema, "name")), ir.types.intern("auto"))};
    ir.set_function_parameters(
        created_function,
        {ir.parameter(ir.types.intern(enum_name + " const"), ir.strings.intern("value"))});
    ir.set_function_body(created_function, body.build());
    ir.set_function_return_type(
        created_function,
        ir.types.intern("TCHAR const*", {ir.types.dependency("TCHAR const*", "CoreMinimal.h")}),
        ReturnSyntax::trailing);
    return created_function;
}

auto display_lookup(LoweringContext& context, ModuleSettingsId settings, DeclarationHandle schema)
    -> FunctionId {
    auto& ir{context.ir};
    auto const enum_name{qualified_enum_name(context, settings, schema)};
    std::vector<SwitchCaseId> cases;
    for (auto const value_id : context.declarations.items(
             context.declarations.enums.values[context.declarations.enums.index(schema)])) {
        auto const value{value_id};
        if (!context.declarations.enumerators
                 .display_name[context.declarations.enumerators.index(value)]
                 .has_value()) {
            continue;
        }
        cases.push_back(ir.switch_case(
            ir.named(enum_name + "::" +
                     context.text(context.declarations.enumerators
                                      .name[context.declarations.enumerators.index(value)])),
            ir.block({ir.return_statement(text_literal(
                ir,
                context.text(
                    *context.declarations.enumerators
                         .display_name[context.declarations.enumerators.index(value)])))})));
    }
    NodeListBuilder body{ir};
    if (!cases.empty()) {
        cases.push_back(ir.switch_case(std::nullopt, ir.block({ir.break_statement()})));
        body.add(ir.switch_statement(ir.named("value"), std::move(cases)), 2);
    }
    body.add(ir.return_statement(
        ir.call(ir.named(internal_name(context, schema, "name")), {ir.named("value")})));
    auto const created_function{
        ir.function(ir.strings.intern(internal_name(context, schema, "display_name")),
                    ir.types.intern("auto"))};
    ir.set_function_parameters(
        created_function,
        {ir.parameter(ir.types.intern(enum_name + " const"), ir.strings.intern("value"))});
    ir.set_function_body(created_function, body.build());
    ir.set_function_return_type(
        created_function,
        ir.types.intern("TCHAR const*", {ir.types.dependency("TCHAR const*", "CoreMinimal.h")}),
        ReturnSyntax::trailing);
    return created_function;
}

auto serialized_lookup(LoweringContext& context,
                       ModuleSettingsId settings,
                       DeclarationHandle schema) -> FunctionId {
    auto& ir{context.ir};
    auto const enum_name{qualified_enum_name(context, settings, schema)};
    NodeListBuilder body{ir};
    std::vector<SwitchCaseId> cases;
    for (auto const value_id : context.declarations.items(
             context.declarations.enums.values[context.declarations.enums.index(schema)])) {
        auto const value{value_id};
        if (!context.declarations.enumerators
                 .serialized_name[context.declarations.enumerators.index(value)]
                 .has_value()) {
            continue;
        }
        cases.push_back(ir.switch_case(
            ir.named(enum_name + "::" +
                     context.text(context.declarations.enumerators
                                      .name[context.declarations.enumerators.index(value)])),
            ir.block({ir.return_statement(text_literal(
                ir,
                context.text(
                    *context.declarations.enumerators
                         .serialized_name[context.declarations.enumerators.index(value)])))})));
    }
    body.add(ir.switch_statement(ir.named("value"), std::move(cases)), 2)
        .add(ir.raw("ensureMsgf(false,\n"
                    "           TEXT(\"Unhandled serialized " +
                    context.text(
                        context.declarations.enums.name[context.declarations.enums.index(schema)]) +
                    " value: %lld\"),\n"
                    "           static_cast<int64>(value));"),
             1)
        .add(ir.return_statement(text_literal(
            ir,
            "<invalid " +
                context.text(
                    context.declarations.enums.name[context.declarations.enums.index(schema)]) +
                ">")));
    auto const created_function{
        ir.function(ir.strings.intern(internal_name(context, schema, "serialized_name")),
                    ir.types.intern("auto"))};
    ir.set_function_parameters(
        created_function,
        {ir.parameter(ir.types.intern(enum_name + " const"), ir.strings.intern("value"))});
    ir.set_function_body(created_function, body.build());
    ir.set_function_return_type(
        created_function,
        ir.types.intern("TCHAR const*", {ir.types.dependency("TCHAR const*", "CoreMinimal.h")}),
        ReturnSyntax::trailing);
    return created_function;
}

auto serialized_parser(LoweringContext& context,
                       ModuleSettingsId settings,
                       DeclarationHandle schema) -> FunctionId {
    auto& ir{context.ir};
    auto const enum_name{qualified_enum_name(context, settings, schema)};
    NodeListBuilder body{ir};
    for (auto const value_id : context.declarations.items(
             context.declarations.enums.values[context.declarations.enums.index(schema)])) {
        auto const value{value_id};
        if (!context.declarations.enumerators
                 .serialized_name[context.declarations.enumerators.index(value)]
                 .has_value()) {
            continue;
        }
        body.add(
            ir.if_statement(
                ir.binary(
                    BinaryOperator::equal,
                    ir.named("value"),
                    text_literal(
                        ir,
                        context.text(
                            *context.declarations.enumerators
                                 .serialized_name[context.declarations.enumerators.index(value)]))),
                ir.block(
                    {ir.assignment(
                         ir.named("result"),
                         ir.named(enum_name + "::" +
                                  context.text(
                                      context.declarations.enumerators
                                          .name[context.declarations.enumerators.index(value)]))),
                     ir.return_statement(ir.literal("true"))})),
            1);
    }
    body.add(ir.return_statement(ir.literal("false")));
    auto const created_function{
        ir.function(ir.strings.intern("try_parse_serialized"), ir.types.intern("auto"))};
    ir.set_function_parameters(
        created_function,
        {ir.parameter(ir.types.intern("FStringView const",
                                      {ir.types.dependency("FStringView const", "CoreMinimal.h")}),
                      ir.strings.intern("value")),
         ir.parameter(ir.types.intern(enum_name + "&"), ir.strings.intern("result"))});
    ir.set_function_body(created_function, body.build());
    ir.set_function_return_type(created_function, ir.types.intern("bool"), ReturnSyntax::trailing);
    ir.set_function_export_specifier(
        created_function,
        context.declarations.enums.export_specifier[context.declarations.enums.index(schema)]);
    return created_function;
}

auto conversion_function(LoweringContext& context,
                         ModuleSettingsId settings,
                         DeclarationHandle schema,
                         EnumConversion const conversion,
                         std::optional<StringId> const helper_namespace) -> FunctionId {
    auto& ir{context.ir};
    if (conversion == EnumConversion::try_parse_serialized) {
        return serialized_parser(context, settings, schema);
    }
    auto const enum_name{qualified_enum_name(context, settings, schema)};
    auto const display{conversion == EnumConversion::lex_to_display_string ||
                       conversion == EnumConversion::display_string_view ||
                       conversion == EnumConversion::display_string};
    auto const serialized{conversion == EnumConversion::lex_to_serialized_string};
    auto const lexical{conversion == EnumConversion::lex_to_string ||
                       conversion == EnumConversion::lex_to_display_string || serialized};
    auto const helper_in_type_namespace{
        context.optional_text(helper_namespace)
            .value_or(
                context
                    .optional_text(context.declarations.module_settings_store.namespace_name
                                       [context.declarations.module_settings_store.index(settings)])
                    .value_or("")) ==
        context
            .optional_text(
                context.declarations.module_settings_store
                    .namespace_name[context.declarations.module_settings_store.index(settings)])
            .value_or("")};
    auto const parameter_type{
        lexical || helper_in_type_namespace
            ? context.text(
                  context.declarations.enums.name[context.declarations.enums.index(schema)])
            : enum_name};
    auto const lookup{ir.call(
        ir.named(internal_name(
            context, schema, serialized ? "serialized_name" : (display ? "display_name" : "name"))),
        {ir.named("value")})};
    auto const result{ir.function(IdentifierId{}, ir.types.intern("auto"))};
    ir.set_function_parameters(
        result,
        {ir.parameter(ir.types.intern(parameter_type + " const"), ir.strings.intern("value"))});
    ir.set_function_body(result, {ir.return_statement(lookup)});
    ir.set_function_export_specifier(
        result,
        context.declarations.enums.export_specifier[context.declarations.enums.index(schema)]);
    switch (conversion) {
        case EnumConversion::lex_to_string:
            ir.set_function_name(result, ir.strings.intern("LexToString"));
            ir.set_function_return_type(
                result,
                ir.types.intern("TCHAR const*",
                                {ir.types.dependency("TCHAR const*", "CoreMinimal.h")}),
                ReturnSyntax::trailing);
            break;
        case EnumConversion::string_view:
            ir.set_function_name(result, ir.strings.intern("to_string_view"));
            ir.set_function_return_type(
                result,
                ir.types.intern("FStringView",
                                {ir.types.dependency("FStringView", "CoreMinimal.h")}),
                ReturnSyntax::trailing);
            ir.set_function_body(
                result,
                {ir.return_statement(ir.init_list(
                    {lookup},
                    ir.types.intern("FStringView",
                                    {ir.types.dependency("FStringView", "CoreMinimal.h")})))});
            break;
        case EnumConversion::string:
            ir.set_function_name(result, ir.strings.intern("to_string"));
            ir.set_function_return_type(
                result,
                ir.types.intern("FString", {ir.types.dependency("FString", "CoreMinimal.h")}),
                ReturnSyntax::trailing);
            ir.set_function_body(
                result,
                {ir.return_statement(ir.init_list(
                    {lookup},
                    ir.types.intern("FString",
                                    {ir.types.dependency("FString", "CoreMinimal.h")})))});
            break;
        case EnumConversion::lex_to_display_string:
            ir.set_function_name(result, ir.strings.intern("LexToDisplayString"));
            ir.set_function_return_type(
                result,
                ir.types.intern("TCHAR const*",
                                {ir.types.dependency("TCHAR const*", "CoreMinimal.h")}),
                ReturnSyntax::trailing);
            break;
        case EnumConversion::display_string_view:
            ir.set_function_name(result, ir.strings.intern("to_display_string_view"));
            ir.set_function_return_type(
                result,
                ir.types.intern("FStringView",
                                {ir.types.dependency("FStringView", "CoreMinimal.h")}),
                ReturnSyntax::trailing);
            ir.set_function_body(
                result,
                {ir.return_statement(ir.init_list(
                    {lookup},
                    ir.types.intern("FStringView",
                                    {ir.types.dependency("FStringView", "CoreMinimal.h")})))});
            break;
        case EnumConversion::display_string:
            ir.set_function_name(result, ir.strings.intern("to_display_string"));
            ir.set_function_return_type(
                result,
                ir.types.intern("FString", {ir.types.dependency("FString", "CoreMinimal.h")}),
                ReturnSyntax::trailing);
            ir.set_function_body(
                result,
                {ir.return_statement(ir.init_list(
                    {lookup},
                    ir.types.intern("FString",
                                    {ir.types.dependency("FString", "CoreMinimal.h")})))});
            break;
        case EnumConversion::lex_to_serialized_string:
            ir.set_function_name(result, ir.strings.intern("LexToSerializedString"));
            ir.set_function_return_type(
                result,
                ir.types.intern("TCHAR const*",
                                {ir.types.dependency("TCHAR const*", "CoreMinimal.h")}),
                ReturnSyntax::trailing);
            break;
        case EnumConversion::try_parse_serialized:
            break;
    }
    return result;
}

auto annotation(LoweringContext& context, EnumeratorId value, bool const reflected)
    -> std::optional<std::string> {
    auto& ir{context.ir};
    if (!reflected ||
        (!context.declarations.enumerators
              .display_name[context.declarations.enumerators.index(value)]
              .has_value() &&
         !context.declarations.enumerators.hidden[context.declarations.enumerators.index(value)])) {
        return std::nullopt;
    }
    std::vector<std::string> metadata;
    if (context.declarations.enumerators.display_name[context.declarations.enumerators.index(value)]
            .has_value()) {
        metadata.push_back("DisplayName = " +
                           ir.render(ir.string_literal(
                               *context.declarations.enumerators
                                    .display_name[context.declarations.enumerators.index(value)])));
    }
    if (context.declarations.enumerators.hidden[context.declarations.enumerators.index(value)]) {
        metadata.emplace_back("Hidden");
    }
    return "UMETA(" + join(metadata, ", ") + ")";
}

auto native_string_literal(CppIR& ir, std::string_view const value) -> std::string {
    return ir.render(ir.string_literal(value));
}

auto native_enum_name(LoweringContext& context, ModuleSettingsId settings, DeclarationHandle schema)
    -> std::string {
    return context.declarations.module_settings_store
                   .namespace_name[context.declarations.module_settings_store.index(settings)]
                   .has_value()
             ? "::" +
                   context.text(*context.declarations.module_settings_store.namespace_name
                                     [context.declarations.module_settings_store.index(settings)]) +
                   "::" +
                   context.text(
                       context.declarations.enums.name[context.declarations.enums.index(schema)])
             : "::" +
                   context.text(
                       context.declarations.enums.name[context.declarations.enums.index(schema)]);
}

auto enum_underlying_type(LoweringContext& context, DeclarationHandle schema) -> CppTypeId {
    auto& ir{context.ir};
    if (context.declarations.enums.underlying_type[context.declarations.enums.index(schema)]
            .has_value()) {
        return context.resolve(
            *context.declarations.enums.underlying_type[context.declarations.enums.index(schema)]);
    }

    auto const& domains{context.semantic.enum_domains()};
    auto const domain{domains.find(schema)};
    auto const requirement{schema::derive_enum_storage_requirement(
        domains,
        domain,
        context.declarations.enums.bit_width[context.declarations.enums.index(schema)])};
    if (!requirement.has_value()) {
        throw std::invalid_argument{
            "Enum '" +
            context.text(
                context.declarations.enums.name[context.declarations.enums.index(schema)]) +
            "' has no derivable C++ backing storage"};
    }

    auto const prefix{requirement->signedness ? "int" : "uint"};
    auto const spelling{
        context.declarations.enums.native_api[context.declarations.enums.index(schema)]
            ? "std::" + std::string{prefix} + std::to_string(requirement->bit_width) + "_t"
            : std::string{prefix} + std::to_string(requirement->bit_width)};
    return ir.types.intern(
        spelling,
        {ir.types.dependency(
            spelling,
            context.declarations.enums.native_api[context.declarations.enums.index(schema)]
                ? "cstdint"
                : "CoreMinimal.h")});
}

auto enum_bit_width_comment(LoweringContext& context, DeclarationHandle schema) -> std::string {
    std::optional<std::uint8_t> const width{
        context.declarations.enums.bit_width[context.declarations.enums.index(schema)].has_value()
            ? std::optional<std::uint8_t>{*context.declarations.enums
                                               .bit_width[context.declarations.enums.index(schema)]}
            : context.semantic.enum_domains()
                  .minimum_required_bits[context.semantic.enum_domains().index(
                      context.semantic.enum_domains().find(schema))]};
    return width.has_value() ? "// Logical bit width: " + std::to_string(*width) + ".\n"
                             : "// Logical bit width depends on the enumerator expressions.\n";
}

auto native_enum_declaration(LoweringContext& context, DeclarationHandle schema) -> std::string {
    auto& ir{context.ir};
    auto const underlying{ir.types.spelling(enum_underlying_type(context, schema))};
    std::ostringstream output;
    output << "using "
           << context.text(
                  context.declarations.enums.name[context.declarations.enums.index(schema)])
           << "Underlying = " << underlying << ";\n\n";
    output << enum_bit_width_comment(context, schema);
    output
        << "enum class "
        << context.text(context.declarations.enums.name[context.declarations.enums.index(schema)])
        << " : "
        << context.text(context.declarations.enums.name[context.declarations.enums.index(schema)])
        << "Underlying {\n";
    for (auto const value_id : context.declarations.items(
             context.declarations.enums.values[context.declarations.enums.index(schema)])) {
        auto const value{value_id};
        output << "    "
               << context.text(context.declarations.enumerators
                                   .name[context.declarations.enumerators.index(value)]);
        if (context.declarations.enumerators
                .initializer[context.declarations.enumerators.index(value)]
                .has_value()) {
            output << " = "
                   << context.text(
                          *context.declarations.enumerators
                               .initializer[context.declarations.enumerators.index(value)]);
        }
        output << ",\n";
    }
    output << "};\n\n";

    if (std::ranges::contains(
            context.declarations.items(
                context.declarations.enums.helpers[context.declarations.enums.index(schema)]),
            NativeEnumHelper::to_string)) {
        output << "[[nodiscard]] constexpr auto to_string("
               << context.text(
                      context.declarations.enums.name[context.declarations.enums.index(schema)])
               << " const value) noexcept -> std::string_view {\n";
        output << "    switch (value) {\n";
        for (auto const value_id : context.declarations.items(
                 context.declarations.enums.values[context.declarations.enums.index(schema)])) {
            auto const value{value_id};
            output << "        case "
                   << context.text(
                          context.declarations.enums.name[context.declarations.enums.index(schema)])
                   << "::"
                   << context.text(context.declarations.enumerators
                                       .name[context.declarations.enumerators.index(value)])
                   << ": {\n";
            output << "            return "
                   << native_string_literal(
                          ir,
                          context.text(context.declarations.enumerators
                                           .name[context.declarations.enumerators.index(value)]))
                   << ";\n";
            output << "        }\n";
        }
        output << "    }\n\n";
        output << "    return "
               << native_string_literal(
                      ir,
                      "<invalid " +
                          context.text(context.declarations.enums
                                           .name[context.declarations.enums.index(schema)]) +
                          ">")
               << ";\n";
        output << "}\n\n";
    }

    if (std::ranges::contains(
            context.declarations.items(
                context.declarations.enums.helpers[context.declarations.enums.index(schema)]),
            NativeEnumHelper::to_display_string)) {
        output << "\n[[nodiscard]] constexpr auto to_display_string("
               << context.text(
                      context.declarations.enums.name[context.declarations.enums.index(schema)])
               << " const value) noexcept -> std::string_view {\n";
        output << "    switch (value) {\n";
        for (auto const value_id : context.declarations.items(
                 context.declarations.enums.values[context.declarations.enums.index(schema)])) {
            auto const value{value_id};
            output << "        case "
                   << context.text(
                          context.declarations.enums.name[context.declarations.enums.index(schema)])
                   << "::"
                   << context.text(context.declarations.enumerators
                                       .name[context.declarations.enumerators.index(value)])
                   << ": {\n";
            output << "            return "
                   << native_string_literal(
                          ir,
                          context
                              .optional_text(
                                  context.declarations.enumerators
                                      .display_name[context.declarations.enumerators.index(value)])
                              .value_or(context.text(
                                  context.declarations.enumerators
                                      .name[context.declarations.enumerators.index(value)])))
                   << ";\n";
            output << "        }\n";
        }
        output << "    }\n\n";
        output << "    return "
               << native_string_literal(
                      ir,
                      "<invalid " +
                          context.text(context.declarations.enums
                                           .name[context.declarations.enums.index(schema)]) +
                          ">")
               << ";\n}\n";
    }

    if (std::ranges::contains(
            context.declarations.items(
                context.declarations.enums.helpers[context.declarations.enums.index(schema)]),
            NativeEnumHelper::to_serialized_string)) {
        output << "\n[[nodiscard]] constexpr auto to_serialized_string("
               << context.text(
                      context.declarations.enums.name[context.declarations.enums.index(schema)])
               << " const value) noexcept -> std::string_view {\n";
        output << "    switch (value) {\n";
        for (auto const value_id : context.declarations.items(
                 context.declarations.enums.values[context.declarations.enums.index(schema)])) {
            auto const value{value_id};
            if (!context.declarations.enumerators
                     .serialized_name[context.declarations.enumerators.index(value)]
                     .has_value()) {
                continue;
            }
            output << "        case "
                   << context.text(
                          context.declarations.enums.name[context.declarations.enums.index(schema)])
                   << "::"
                   << context.text(context.declarations.enumerators
                                       .name[context.declarations.enumerators.index(value)])
                   << ": {\n";
            output << "            return "
                   << native_string_literal(
                          ir,
                          context.text(
                              *context.declarations.enumerators
                                   .serialized_name[context.declarations.enumerators.index(value)]))
                   << ";\n";
            output << "        }\n";
        }
        output << "        default: {\n";
        output << "            return "
               << native_string_literal(
                      ir,
                      "<invalid " +
                          context.text(context.declarations.enums
                                           .name[context.declarations.enums.index(schema)]) +
                          ">")
               << ";\n";
        output << "        }\n";
        output << "    }\n\n";
        output << "}\n\n";
    }
    return output.str();
}

auto native_enum_parsers(LoweringContext& context, DeclarationHandle const schema) -> Nodes {
    auto& ir{context.ir};
    auto const row{context.declarations.enums.index(schema)};
    auto const name{context.text(context.declarations.enums.name[row])};
    auto const snake_name{snake_case_type_name(name)};
    auto const helpers{context.declarations.items(context.declarations.enums.helpers[row])};
    NodeListBuilder parsers{ir};
    for (auto const helper :
         {NativeEnumHelper::try_parse, NativeEnumHelper::try_parse_serialized}) {
        if (!std::ranges::contains(helpers, helper)) {
            continue;
        }

        std::vector<StringEnumEntry> entries;
        for (auto const value :
             context.declarations.items(context.declarations.enums.values[row])) {
            auto const value_row{context.declarations.enumerators.index(value)};
            auto const value_name{context.text(context.declarations.enumerators.name[value_row])};
            std::string key;
            if (helper == NativeEnumHelper::try_parse_serialized) {
                auto const serialized{context.declarations.enumerators.serialized_name[value_row]};
                if (!serialized) {
                    continue;
                }
                key = context.text(*serialized);
            } else {
                key = value_name;
            }
            entries.push_back({std::move(key), name + "::" + value_name});
        }

        auto const prefix{helper == NativeEnumHelper::try_parse ? "try_parse_"
                                                                : "try_parse_serialized_"};
        parsers.add(lower_string_enum_parser(ir, prefix + snake_name, name, "value", entries), 2);
    }
    return parsers.build();
}

auto native_enum_traits(LoweringContext& context,
                        ModuleSettingsId settings,
                        DeclarationHandle schema) -> Nodes {
    auto& ir{context.ir};
    auto const qualified{native_enum_name(context, settings, schema)};
    auto const enum_type{ir.types.intern(qualified)};
    auto const schema_index{context.declarations.enums.index(schema)};
    auto const value_count{context.declarations.enums.values[schema_index].size() -
                           (context.declarations.enums.count[schema_index] ? 1 : 0)};
    NodeListBuilder result{ir};
    if (std::ranges::contains(
            context.declarations.items(context.declarations.enums.helpers[schema_index]),
            NativeEnumHelper::traits)) {
        std::vector<Expr> values;
        values.reserve(value_count);
        for (auto const value :
             context.declarations.items(context.declarations.enums.values[schema_index])) {
            auto const name{context.declarations.enumerators
                                .name[context.declarations.enumerators.index(value)]};
            if (context.declarations.enums.count[schema_index] &&
                name == *context.declarations.enums.count[schema_index]) {
                continue;
            }
            values.push_back(ir.named(qualified + "::" + context.text(name)));
        }
        auto const traits{ir.record(
            ir.template_id(ir.named("EnumTraits"), {ir.type_reference(enum_type)}),
            {ir.constexpr_member(
                ir.types.intern("std::array", {ir.types.dependency("std::array", "array")}),
                ir.strings.intern("values"),
                ir.init_list(values),
                true)})};
        ir.set_record_template(traits, ir.template_specialization());
        result.add(traits, 2);
    }

    if (context.declarations.enums.enum_array[schema_index]) {
        result.add(enum_count_specialization(ir, enum_type, value_count));
    }
    return result.build();
}

auto unreal_underlying_type(std::string_view const spelling) -> std::string {
    if (spelling == "std::uint8_t") {
        return "uint8";
    }
    if (spelling == "std::uint16_t") {
        return "uint16";
    }
    if (spelling == "std::uint32_t") {
        return "uint32";
    }
    if (spelling == "std::uint64_t") {
        return "uint64";
    }
    if (spelling == "std::int8_t") {
        return "int8";
    }
    if (spelling == "std::int16_t") {
        return "int16";
    }
    if (spelling == "std::int32_t") {
        return "int32";
    }
    if (spelling == "std::int64_t") {
        return "int64";
    }
    return std::string{spelling};
}

auto unreal_projection_header(LoweringContext& context,
                              DeclarationHandle schema,
                              EnumUnrealProjectionId projection) -> Nodes {
    auto& ir{context.ir};
    std::ostringstream output;
    auto const underlying{
        unreal_underlying_type(ir.types.spelling(enum_underlying_type(context, schema)))};
    output << enum_bit_width_comment(context, schema);
    output << (context.declarations.enum_unreal_projections
                           .reflection[context.declarations.enum_unreal_projections.index(
                               projection)] == EnumReflection::blueprint
                   ? "UENUM(BlueprintType)"
                   : "UENUM()")
           << "\nenum class "
           << context.text(
                  context.declarations.enum_unreal_projections
                      .name[context.declarations.enum_unreal_projections.index(projection)])
           << " : " << underlying << " {\n";
    for (auto const value_id : context.declarations.items(
             context.declarations.enums.values[context.declarations.enums.index(schema)])) {
        auto const value{value_id};
        output << "    "
               << context.text(context.declarations.enumerators
                                   .name[context.declarations.enumerators.index(value)]);
        if (context.declarations.enumerators
                .initializer[context.declarations.enumerators.index(value)]
                .has_value()) {
            output << " = "
                   << context.text(
                          *context.declarations.enumerators
                               .initializer[context.declarations.enumerators.index(value)]);
        }
        if (context.declarations.enumerators
                .display_name[context.declarations.enumerators.index(value)]
                .has_value() ||
            context.declarations.enumerators
                .hidden[context.declarations.enumerators.index(value)]) {
            output << " UMETA(";
            if (context.declarations.enumerators
                    .display_name[context.declarations.enumerators.index(value)]
                    .has_value()) {
                output
                    << "DisplayName = "
                    << native_string_literal(
                           ir,
                           context.text(
                               *context.declarations.enumerators
                                    .display_name[context.declarations.enumerators.index(value)]));
            }
            if (context.declarations.enumerators
                    .display_name[context.declarations.enumerators.index(value)]
                    .has_value() &&
                context.declarations.enumerators
                    .hidden[context.declarations.enumerators.index(value)]) {
                output << ", ";
            }
            if (context.declarations.enumerators
                    .hidden[context.declarations.enumerators.index(value)]) {
                output << "Hidden";
            }
            output << ")";
        }
        output << ",\n";
    }
    output << "};\n\nusing "
           << context.text(
                  context.declarations.enum_unreal_projections
                      .name[context.declarations.enum_unreal_projections.index(projection)])
           << "Underlying = " << underlying << ";";
    NodeListBuilder result{ir};
    result.add(ir.raw(output.str()), 2);
    if (context.declarations.enum_unreal_projections
            .enum_array[context.declarations.enum_unreal_projections.index(projection)]) {
        auto const enum_type{ir.types.intern(
            "::" + context.text(
                       context.declarations.enum_unreal_projections
                           .name[context.declarations.enum_unreal_projections.index(projection)]))};
        result.add(ir.namespace_node(
            ir.strings.intern("ioj"),
            {enum_count_specialization(
                ir,
                enum_type,
                context.declarations.enums.values[context.declarations.enums.index(schema)]
                    .size())}));
    }
    return result.build();
}

auto unreal_conversion_header(LoweringContext& context,
                              ModuleSettingsId settings,
                              DeclarationHandle schema,
                              EnumUnrealProjectionId projection) -> std::string {
    auto const native_name{native_enum_name(context, settings, schema)};
    std::ostringstream output;
    output << "namespace ioj {\n"
           << "[[nodiscard]] constexpr auto to_native("
           << context.text(
                  context.declarations.enum_unreal_projections
                      .name[context.declarations.enum_unreal_projections.index(projection)])
           << " const value) noexcept -> " << native_name << " {\n"
           << "    switch (value) {\n";
    for (auto const value_id : context.declarations.items(
             context.declarations.enums.values[context.declarations.enums.index(schema)])) {
        auto const value{value_id};
        output << "        case "
               << context.text(
                      context.declarations.enum_unreal_projections
                          .name[context.declarations.enum_unreal_projections.index(projection)])
               << "::"
               << context.text(context.declarations.enumerators
                                   .name[context.declarations.enumerators.index(value)])
               << ": {\n"
               << "            return " << native_name << "::"
               << context.text(context.declarations.enumerators
                                   .name[context.declarations.enumerators.index(value)])
               << ";\n"
               << "        }\n";
    }
    output << "    }\n\n"
           << "    return static_cast<" << native_name << ">(value);\n"
           << "}\n\n"
           << "[[nodiscard]] constexpr auto to_unreal(" << native_name
           << " const value) noexcept -> "
           << context.text(
                  context.declarations.enum_unreal_projections
                      .name[context.declarations.enum_unreal_projections.index(projection)])
           << " {\n"
           << "    switch (value) {\n";
    for (auto const value_id : context.declarations.items(
             context.declarations.enums.values[context.declarations.enums.index(schema)])) {
        auto const value{value_id};
        output << "        case " << native_name << "::"
               << context.text(context.declarations.enumerators
                                   .name[context.declarations.enumerators.index(value)])
               << ": {\n"
               << "            return "
               << context.text(
                      context.declarations.enum_unreal_projections
                          .name[context.declarations.enum_unreal_projections.index(projection)])
               << "::"
               << context.text(context.declarations.enumerators
                                   .name[context.declarations.enumerators.index(value)])
               << ";\n"
               << "        }\n";
    }
    output << "    }\n\n"
           << "    return static_cast<"
           << context.text(
                  context.declarations.enum_unreal_projections
                      .name[context.declarations.enum_unreal_projections.index(projection)])
           << ">(value);\n"
           << "}\n\n";
    for (auto const value_id : context.declarations.items(
             context.declarations.enums.values[context.declarations.enums.index(schema)])) {
        auto const value{value_id};
        output << "static_assert(static_cast<int>("
               << context.text(
                      context.declarations.enum_unreal_projections
                          .name[context.declarations.enum_unreal_projections.index(projection)])
               << "::"
               << context.text(context.declarations.enumerators
                                   .name[context.declarations.enumerators.index(value)])
               << ") == static_cast<int>(" << native_name << "::"
               << context.text(context.declarations.enumerators
                                   .name[context.declarations.enumerators.index(value)])
               << "));\n";
    }
    output << "} // namespace ioj\n";
    return output.str();
}

} // namespace

auto lower_enum(LoweringContext& context,
                DeclarationHandle schema,
                ModuleSettingsId settings,
                std::optional<StringId> const helper_namespace) -> DeclarationEmission {
    auto& ir{context.ir};
    DeclarationEmission emission;
    emission.source_dependencies = false;
    if (context.declarations.enums.native_api[context.declarations.enums.index(schema)]) {
        NodeListBuilder prefix{ir};
        auto const has = [&](NativeEnumHelper const helper) {
            return std::ranges::contains(
                context.declarations.items(
                    context.declarations.enums.helpers[context.declarations.enums.index(schema)]),
                helper);
        };
        prefix.add(ir.include(ir.store_text("cstdint"), IncludeStyle::system), 2);
        auto const parses_strings{has(NativeEnumHelper::try_parse) ||
                                  has(NativeEnumHelper::try_parse_serialized)};
        if (!parses_strings &&
            std::ranges::any_of(
                context.declarations.items(
                    context.declarations.enums.helpers[context.declarations.enums.index(schema)]),
                [](auto const helper) { return helper != NativeEnumHelper::traits; })) {
            prefix.add(ir.include(ir.store_text("string_view"), IncludeStyle::system), 2);
        }
        if (has(NativeEnumHelper::traits)) {
            prefix.add(ir.include(ir.store_text("array"), IncludeStyle::system), 2);
        }
        if (has(NativeEnumHelper::traits) ||
            context.declarations.enums.enum_array[context.declarations.enums.index(schema)]) {
            prefix.add(ir.include(ir.store_text("ioj/enum_traits.h"), IncludeStyle::quoted), 2);
        }
        if (context.declarations.enums.enum_array[context.declarations.enums.index(schema)]) {
            prefix.add(ir.include(ir.store_text("cstddef"), IncludeStyle::system), 2);
        }
        emission.header_prefix = prefix.build();
        NodeListBuilder declarations{ir};
        declarations.add(ir.raw(native_enum_declaration(context, schema)), 2);
        declarations.append(native_enum_parsers(context, schema));
        emission.header = declarations.build();
        if (has(NativeEnumHelper::traits) ||
            context.declarations.enums.enum_array[context.declarations.enums.index(schema)]) {
            emission.header_global = {ir.namespace_node(
                ir.strings.intern("ioj"), native_enum_traits(context, settings, schema))};
        }
        if (context.declarations.enums.unreal_projection[context.declarations.enums.index(schema)]
                .has_value()) {
            auto const projection{
                *context.declarations.enums
                     .unreal_projection[context.declarations.enums.index(schema)]};
            auto const generated_header{
                std::filesystem::path{
                    context.text(context.declarations.enum_unreal_projections
                                     .header[context.declarations.enum_unreal_projections.index(
                                         projection)])}
                    .stem()
                    .string() +
                ".generated.h"};
            auto projection_nodes{unreal_projection_header(context, schema, projection)};
            projection_nodes.insert(
                projection_nodes.begin(),
                {ir.include(ir.store_text("CoreMinimal.h"), IncludeStyle::quoted),
                 ir.include(ir.store_text(generated_header), IncludeStyle::quoted)});
            auto const& projections{context.declarations.enum_unreal_projections};
            auto const projection_row{projections.index(projection)};
            auto const module_name{
                context.text(
                    context.declarations.module_settings_store
                        .name[context.declarations.module_settings_store.index(settings)]) +
                "_" + context.text(projections.name[projection_row])};
            if (projections.enum_array[projection_row]) {
                projection_nodes.insert(
                    projection_nodes.begin() + 1,
                    ir.include(ir.store_text("ioj/enum_traits.h"), IncludeStyle::quoted));
            }
            auto const projection_file{
                ir.files.append(projections.header[projection_row], projection_nodes)};
            ir.files.flags[ir.files.index(projection_file)].set_clang_format_off(true);
            emission.additional_modules.push_back(
                ir.modules.append(ir.strings.intern(module_name), projection_file));
            auto const conversion_file{ir.files.append(
                projections.conversion_header[projection_row],
                {ir.include(projections.native_header_include[projection_row],
                            IncludeStyle::quoted),
                 ir.include(projections.header_include[projection_row], IncludeStyle::quoted),
                 ir.raw(unreal_conversion_header(context, settings, schema, projection))})};
            ir.files.flags[ir.files.index(conversion_file)].set_clang_format_off(true);
            emission.additional_modules.push_back(
                ir.modules.append(ir.strings.intern(module_name + "_conversion"), conversion_file));
        }
        return emission;
    }

    NodeListBuilder declarations{ir};
    auto const underlying{enum_underlying_type(context, schema)};
    auto const underlying_alias{
        context.text(context.declarations.enums.name[context.declarations.enums.index(schema)]) +
        "Underlying"};
    auto const reflected{
        context.declarations.enums.reflection[context.declarations.enums.index(schema)] !=
        EnumReflection::none};
    if (!reflected) {
        declarations.add(ir.using_declaration(ir.strings.intern(underlying_alias), underlying), 2);
    }
    declarations.add(ir.raw(enum_bit_width_comment(context, schema)), 1);
    if (reflected) {
        declarations.add(
            ir.raw(
                context.declarations.enums.reflection[context.declarations.enums.index(schema)] ==
                        EnumReflection::blueprint
                    ? "UENUM(BlueprintType)"
                    : "UENUM()",
                {ir.types.dependency("UENUM", "CoreMinimal.h")}),
            1);
        emission.header_generated_include = {ir.include(
            ir.store_text(
                std::filesystem::path{
                    context.text(
                        context.declarations.module_settings_store
                            .header[context.declarations.module_settings_store.index(settings)])}
                    .stem()
                    .string() +
                ".generated.h"),
            IncludeStyle::quoted)};
    }
    std::vector<EnumValueId> values;
    for (auto const value_id : context.declarations.items(
             context.declarations.enums.values[context.declarations.enums.index(schema)])) {
        auto const value{value_id};
        values.push_back(ir.enumerator(
            context.declarations.enumerators.name[context.declarations.enumerators.index(value)],
            context.declarations.enumerators
                .initializer[context.declarations.enumerators.index(value)],
            ir.store_optional_text(annotation(
                context,
                value,
                context.declarations.enums.reflection[context.declarations.enums.index(schema)] !=
                    EnumReflection::none))));
    }
    declarations.add(
        ir.enumeration(context.declarations.enums.name[context.declarations.enums.index(schema)],
                       reflected ? underlying : ir.types.intern(underlying_alias),
                       std::move(values)),
        2);
    if (reflected) {
        // Preserve the built-in spelling required by UnrealHeaderTool for reflected enums.
        declarations.add(ir.using_declaration(ir.strings.intern(underlying_alias), underlying), 2);
    }
    if (context.declarations.enums.enum_array[context.declarations.enums.index(schema)]) {
        emission.header_prefix.push_back(
            ir.include(ir.store_text("ioj/enum_traits.h"), IncludeStyle::quoted));
        emission.header_global.push_back(ir.namespace_node(
            ir.strings.intern("ioj"),
            {enum_count_specialization(
                ir,
                ir.types.intern("::" + qualified_enum_name(context, settings, schema)),
                context.declarations.enums.values[context.declarations.enums.index(schema)]
                    .size())}));
    }

    NodeListBuilder lex_declarations{ir};
    NodeListBuilder helper_declarations{ir};
    std::vector<FunctionId> conversions;
    conversions.reserve(
        context.declarations
            .items(context.declarations.enums.conversions[context.declarations.enums.index(schema)])
            .size());
    for (auto const conversion : context.declarations.items(
             context.declarations.enums.conversions[context.declarations.enums.index(schema)])) {
        auto const spec{
            conversion_function(context, settings, schema, conversion, helper_namespace)};
        conversions.push_back(spec);
        auto const lexical{conversion == EnumConversion::lex_to_string ||
                           conversion == EnumConversion::lex_to_display_string ||
                           conversion == EnumConversion::lex_to_serialized_string};
        (lexical ? lex_declarations : helper_declarations).add(ir.declaration(spec), 2);
    }
    emission.header = declarations.build();
    emission.header_after = lex_declarations.build();
    auto helper_nodes{helper_declarations.build()};
    if (!helper_nodes.empty()) {
        emission.header_tail = std::move(helper_nodes);
        emission.tail_namespace =
            context.optional_text(helper_namespace)
                .value_or(context
                              .optional_text(
                                  context.declarations.module_settings_store.namespace_name
                                      [context.declarations.module_settings_store.index(settings)])
                              .value_or(""));
    }

    if (!context.declarations
             .items(
                 context.declarations.enums.conversions[context.declarations.enums.index(schema)])
             .empty()) {
        NodeListBuilder internal{ir};
        NodeListBuilder lexical{ir};
        NodeListBuilder helpers{ir};
        auto const has_display{std::ranges::any_of(
            context.declarations.items(
                context.declarations.enums.conversions[context.declarations.enums.index(schema)]),
            [](auto const conversion) {
                return conversion == EnumConversion::lex_to_display_string ||
                       conversion == EnumConversion::display_string_view ||
                       conversion == EnumConversion::display_string;
            })};
        auto const has_name{
            has_display ||
            std::ranges::any_of(context.declarations.items(
                                    context.declarations.enums
                                        .conversions[context.declarations.enums.index(schema)]),
                                [](auto const conversion) {
                                    return conversion == EnumConversion::lex_to_string ||
                                           conversion == EnumConversion::string_view ||
                                           conversion == EnumConversion::string;
                                })};
        if (has_name) {
            internal.add(ir.definition(exact_lookup(context, settings, schema)), 2);
        }
        if (has_display) {
            internal.add(ir.definition(display_lookup(context, settings, schema)), 2);
        }
        if (std::ranges::find(context.declarations.items(
                                  context.declarations.enums
                                      .conversions[context.declarations.enums.index(schema)]),
                              EnumConversion::lex_to_serialized_string) !=
            context.declarations
                .items(context.declarations.enums
                           .conversions[context.declarations.enums.index(schema)])
                .end()) {
            internal.add(ir.definition(serialized_lookup(context, settings, schema)), 2);
        }
        auto const conversion_count{
            context.declarations
                .items(context.declarations.enums
                           .conversions[context.declarations.enums.index(schema)])
                .size()};
        for (std::size_t index{}; index < conversion_count; ++index) {
            auto const conversion{context.declarations.items(
                context.declarations.enums
                    .conversions[context.declarations.enums.index(schema)])[index]};
            auto const is_lexical{conversion == EnumConversion::lex_to_string ||
                                  conversion == EnumConversion::lex_to_display_string ||
                                  conversion == EnumConversion::lex_to_serialized_string};
            (is_lexical ? lexical : helpers).add(ir.definition(conversions[index]), 2);
        }
        emission.source = lexical.build();
        emission.source_global = internal.build();
        emission.source_tail = helpers.build();
        emission.tail_namespace =
            context.optional_text(helper_namespace)
                .value_or(context
                              .optional_text(
                                  context.declarations.module_settings_store.namespace_name
                                      [context.declarations.module_settings_store.index(settings)])
                              .value_or(""));
    }
    return emission;
}

} // namespace codegen::detail
