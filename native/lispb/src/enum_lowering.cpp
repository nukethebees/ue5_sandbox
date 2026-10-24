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
#include <codegen/cpp_string.h>
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
#include <format>
#include <optional>
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
    auto const& settings_store{context.declarations.module_settings_store};
    auto const namespace_name{settings_store.namespace_name[settings_store.index(settings)]};
    auto const& enums{context.declarations.enums};
    auto name{context.text(enums.name[enums.index(schema)])};
    if (namespace_name) {
        return context.text(*namespace_name) + "::" + name;
    }
    return name;
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
    ir.set_function_body(function, {ir.return_stmt(ir.literal(std::to_string(count)))});
    return ir.header_function(function);
}

auto lookup_function(LoweringContext& context,
                     DeclarationHandle schema,
                     std::string const& enum_name,
                     std::string_view suffix,
                     Nodes body) -> FunctionId {
    auto& ir{context.ir};
    auto const function{ir.function(internal_name(context, schema, suffix), "auto")};
    ir.set_function_parameters(function, {ir.parameter(enum_name + " const", "value")});
    ir.set_function_body(function, body);
    ir.set_function_return_type(
        function,
        ir.types.intern("TCHAR const*", {ir.types.dependency("TCHAR const*", "CoreMinimal.h")}),
        ReturnSyntax::trailing);
    return function;
}

auto exact_lookup(LoweringContext& context, ModuleSettingsId settings, DeclarationHandle schema)
    -> FunctionId {
    auto& ir{context.ir};
    auto const& enums{context.declarations.enums};
    auto const& enumerators{context.declarations.enumerators};
    auto const row{enums.index(schema)};
    auto const enum_name{qualified_enum_name(context, settings, schema)};
    auto const name{context.text(enums.name[row])};
    NodeListBuilder body{ir};
    std::vector<SwitchCaseId> cases;
    for (auto const value : context.declarations.items(enums.values[row])) {
        auto const value_name{context.text(enumerators.name[enumerators.index(value)])};
        cases.push_back(ir.switch_case(ir.named(enum_name + "::" + value_name),
                                       ir.block({ir.return_stmt(text_literal(ir, value_name))})));
    }
    body.add(ir.switch_stmt(ir.named("value"), std::move(cases)), 2)
        .add(ir.raw("ensureMsgf(false,\n"
                    "           TEXT(\"Unhandled " +
                    name +
                    " value: %lld\"),\n"
                    "           static_cast<int64>(value));"),
             1)
        .add(ir.return_stmt(text_literal(ir, "<invalid " + name + ">")));
    return lookup_function(context, schema, enum_name, "name", body.build());
}

auto display_lookup(LoweringContext& context, ModuleSettingsId settings, DeclarationHandle schema)
    -> FunctionId {
    auto& ir{context.ir};
    auto const& enums{context.declarations.enums};
    auto const& enumerators{context.declarations.enumerators};
    auto const row{enums.index(schema)};
    auto const enum_name{qualified_enum_name(context, settings, schema)};
    std::vector<SwitchCaseId> cases;
    for (auto const value : context.declarations.items(enums.values[row])) {
        auto const value_row{enumerators.index(value)};
        auto const display_name{enumerators.display_name[value_row]};
        if (!display_name) {
            continue;
        }
        cases.push_back(ir.switch_case(
            ir.named(enum_name + "::" + context.text(enumerators.name[value_row])),
            ir.block({ir.return_stmt(text_literal(ir, context.text(*display_name)))})));
    }
    NodeListBuilder body{ir};
    if (!cases.empty()) {
        cases.push_back(ir.switch_case(std::nullopt, ir.block({ir.break_stmt()})));
        body.add(ir.switch_stmt(ir.named("value"), std::move(cases)), 2);
    }
    body.add(ir.return_stmt(
        ir.call(ir.named(internal_name(context, schema, "name")), {ir.named("value")})));
    return lookup_function(context, schema, enum_name, "display_name", body.build());
}

auto serialized_lookup(LoweringContext& context,
                       ModuleSettingsId settings,
                       DeclarationHandle schema) -> FunctionId {
    auto& ir{context.ir};
    auto const& enums{context.declarations.enums};
    auto const& enumerators{context.declarations.enumerators};
    auto const row{enums.index(schema)};
    auto const enum_name{qualified_enum_name(context, settings, schema)};
    auto const name{context.text(enums.name[row])};
    NodeListBuilder body{ir};
    std::vector<SwitchCaseId> cases;
    for (auto const value : context.declarations.items(enums.values[row])) {
        auto const value_row{enumerators.index(value)};
        auto const serialized_name{enumerators.serialized_name[value_row]};
        if (!serialized_name) {
            continue;
        }
        cases.push_back(ir.switch_case(
            ir.named(enum_name + "::" + context.text(enumerators.name[value_row])),
            ir.block({ir.return_stmt(text_literal(ir, context.text(*serialized_name)))})));
    }
    body.add(ir.switch_stmt(ir.named("value"), std::move(cases)), 2)
        .add(ir.raw("ensureMsgf(false,\n"
                    "           TEXT(\"Unhandled serialized " +
                    name +
                    " value: %lld\"),\n"
                    "           static_cast<int64>(value));"),
             1)
        .add(ir.return_stmt(text_literal(ir, "<invalid " + name + ">")));
    return lookup_function(context, schema, enum_name, "serialized_name", body.build());
}

auto serialized_parser(LoweringContext& context,
                       ModuleSettingsId settings,
                       DeclarationHandle schema) -> FunctionId {
    auto& ir{context.ir};
    auto const& enums{context.declarations.enums};
    auto const& enumerators{context.declarations.enumerators};
    auto const row{enums.index(schema)};
    auto const enum_name{qualified_enum_name(context, settings, schema)};
    NodeListBuilder body{ir};
    for (auto const value : context.declarations.items(enums.values[row])) {
        auto const value_row{enumerators.index(value)};
        auto const serialized_name{enumerators.serialized_name[value_row]};
        if (!serialized_name) {
            continue;
        }
        body.add(
            ir.if_stmt(ir.binary(BinaryOperator::equal,
                                 ir.named("value"),
                                 text_literal(ir, context.text(*serialized_name))),
                       ir.block({ir.assignment(ir.named("result"),
                                               ir.named(enum_name + "::" +
                                                        context.text(enumerators.name[value_row]))),
                                 ir.return_stmt(ir.literal("true"))})),
            1);
    }
    body.add(ir.return_stmt(ir.literal("false")));
    auto const created_function{ir.function("try_parse_serialized", "auto")};
    ir.set_function_parameters(
        created_function,
        {ir.parameter(ir.types.intern("FStringView const",
                                      {ir.types.dependency("FStringView const", "CoreMinimal.h")}),
                      "value"),
         ir.parameter(enum_name + "&", "result")});
    ir.set_function_body(created_function, body.build());
    ir.set_function_return_type(created_function, ir.types.intern("bool"), ReturnSyntax::trailing);
    ir.set_function_export_specifier(created_function, enums.export_specifier[row]);
    return created_function;
}

auto conversion_function(LoweringContext& context,
                         ModuleSettingsId settings,
                         DeclarationHandle schema,
                         EnumConversion const conversion,
                         std::optional<StringId> const helper_namespace) -> FunctionId {
    auto& ir{context.ir};
    auto const& enums{context.declarations.enums};
    auto const& settings_store{context.declarations.module_settings_store};
    auto const row{enums.index(schema)};
    auto const settings_row{settings_store.index(settings)};
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
    auto const type_namespace{
        context.optional_text(settings_store.namespace_name[settings_row]).value_or("")};
    auto const helper_in_type_namespace{
        context.optional_text(helper_namespace).value_or(type_namespace) == type_namespace};
    auto const parameter_type{lexical || helper_in_type_namespace ? context.text(enums.name[row])
                                                                  : enum_name};
    auto const lookup{ir.call(
        ir.named(internal_name(
            context, schema, serialized ? "serialized_name" : (display ? "display_name" : "name"))),
        {ir.named("value")})};
    auto const result{ir.function(StringId{}, ir.types.intern("auto"))};
    ir.set_function_parameters(result, {ir.parameter(parameter_type + " const", "value")});
    ir.set_function_body(result, {ir.return_stmt(lookup)});
    ir.set_function_export_specifier(result, enums.export_specifier[row]);
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
                {ir.return_stmt(ir.init_list(
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
                {ir.return_stmt(ir.init_list(
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
                {ir.return_stmt(ir.init_list(
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
                {ir.return_stmt(ir.init_list(
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
    auto const& enumerators{context.declarations.enumerators};
    auto const row{enumerators.index(value)};
    if (!reflected || (!enumerators.display_name[row] && !enumerators.hidden[row])) {
        return std::nullopt;
    }
    std::vector<std::string> metadata;
    if (enumerators.display_name[row]) {
        metadata.push_back("DisplayName = \"" +
                           escape_cpp_string(context.text(*enumerators.display_name[row])) + "\"");
    }
    if (enumerators.hidden[row]) {
        metadata.emplace_back("Hidden");
    }
    return "UMETA(" + join(metadata, ", ") + ")";
}

auto native_enum_name(LoweringContext& context, ModuleSettingsId settings, DeclarationHandle schema)
    -> std::string {
    return "::" + qualified_enum_name(context, settings, schema);
}

auto enum_underlying_type(LoweringContext& context, DeclarationHandle schema) -> CppTypeId {
    auto& ir{context.ir};
    auto const& enums{context.declarations.enums};
    auto const row{enums.index(schema)};
    if (enums.underlying_type[row].has_value()) {
        return context.resolve(*enums.underlying_type[row]);
    }

    auto const& domains{context.semantic.enum_domains()};
    auto const domain{domains.find(schema)};
    auto const requirement{
        schema::derive_enum_storage_requirement(domains, domain, enums.bit_width[row])};
    if (!requirement.has_value()) {
        throw std::invalid_argument{"Enum '" + context.text(enums.name[row]) +
                                    "' has no derivable C++ backing storage"};
    }

    auto const prefix{requirement->signedness ? "int" : "uint"};
    auto const spelling{enums.native_api[row]
                            ? "std::" + std::string{prefix} +
                                  std::to_string(requirement->bit_width) + "_t"
                            : std::string{prefix} + std::to_string(requirement->bit_width)};
    return ir.types.intern(
        spelling,
        {ir.types.dependency(spelling, enums.native_api[row] ? "cstdint" : "CoreMinimal.h")});
}

auto enum_bit_width_comment(LoweringContext& context, DeclarationHandle const schema) -> NodeId {
    auto const& enums{context.declarations.enums};
    auto const row{enums.index(schema)};
    auto const& domains{context.semantic.enum_domains()};
    auto const width{enums.bit_width[row].has_value()
                         ? std::optional<std::uint8_t>{*enums.bit_width[row]}
                         : domains.minimum_required_bits[domains.index(domains.find(schema))]};
    return context.ir.line_comment(
        width ? std::format("Logical bit width: {}.", *width)
              : "Logical bit width depends on the enumerator expressions.");
}

auto native_enum_string_function(LoweringContext& context,
                                 DeclarationHandle const schema,
                                 NativeEnumHelper const helper) -> NodeId {
    auto& ir{context.ir};
    auto const& enums{context.declarations.enums};
    auto const row{enums.index(schema)};
    auto const name{context.text(enums.name[row])};
    auto const& enumerators{context.declarations.enumerators};
    auto const serialized{helper == NativeEnumHelper::to_serialized_string};
    auto const display{helper == NativeEnumHelper::to_display_string};
    auto const function_name{serialized ? "to_serialized_string"
                             : display  ? "to_display_string"
                                        : "to_string"};

    std::vector<SwitchCaseId> cases;
    for (auto const value : context.declarations.items(enums.values[row])) {
        auto const value_row{enumerators.index(value)};
        if (serialized && !enumerators.serialized_name[value_row]) {
            continue;
        }
        auto const value_name{context.text(enumerators.name[value_row])};
        auto const text{
            serialized ? context.text(*enumerators.serialized_name[value_row])
            : display
                ? context.optional_text(enumerators.display_name[value_row]).value_or(value_name)
                : value_name};
        cases.push_back(ir.switch_case(ir.named(name + "::" + value_name),
                                       ir.block({ir.return_stmt(ir.string_literal(text))})));
    }

    auto const invalid{ir.return_stmt(ir.string_literal("<invalid " + name + ">"))};
    if (serialized) {
        cases.push_back(ir.switch_case({}, ir.block({invalid})));
    }
    NodeListBuilder body{ir};
    body.add(ir.switch_stmt(ir.named("value"), cases), serialized ? 1 : 2);
    if (!serialized) {
        body.add(invalid);
    }

    auto const function{ir.function(function_name, "auto")};
    ir.set_function_return_type(
        function,
        ir.types.intern("std::string_view",
                        {ir.types.dependency("std::string_view", "string_view")}),
        ReturnSyntax::trailing);
    ir.set_function_parameters(function, {ir.parameter(name + " const", "value")});
    auto& flags{ir.function_flags(function)};
    flags.set_define_in_header(true);
    flags.set_is_constexpr(true);
    flags.set_is_nodiscard(true);
    ir.set_function_noexcept(function);
    ir.set_function_body(function, body.build());
    return ir.header_function(function);
}

auto native_enum_declaration(LoweringContext& context, DeclarationHandle const schema) -> Nodes {
    auto& ir{context.ir};
    auto const& enums{context.declarations.enums};
    auto const row{enums.index(schema)};
    auto const alias{context.text(enums.name[row]) + "Underlying"};
    auto const& enumerators{context.declarations.enumerators};
    std::vector<EnumValueId> values;
    for (auto const value : context.declarations.items(enums.values[row])) {
        auto const value_row{enumerators.index(value)};
        values.push_back(
            ir.enumerator(enumerators.name[value_row], enumerators.initializer[value_row]));
    }

    NodeListBuilder declarations{ir};
    declarations.add(
        ir.using_declaration(ir.strings.intern(alias), enum_underlying_type(context, schema)), 2);
    declarations.add(enum_bit_width_comment(context, schema), 1);
    declarations.add(ir.enumeration(enums.name[row], ir.types.intern(alias), values), 2);

    auto const helpers{context.declarations.items(enums.helpers[row])};
    for (auto const helper : {NativeEnumHelper::to_string,
                              NativeEnumHelper::to_display_string,
                              NativeEnumHelper::to_serialized_string}) {
        if (std::ranges::contains(helpers, helper)) {
            declarations.add(native_enum_string_function(context, schema, helper), 2);
        }
    }
    return declarations.build();
}

auto native_enum_parsers(LoweringContext& context, DeclarationHandle const schema) -> Nodes {
    auto& ir{context.ir};
    auto const& enums{context.declarations.enums};
    auto const& enumerators{context.declarations.enumerators};
    auto const row{enums.index(schema)};
    auto const name{context.text(enums.name[row])};
    auto const snake_name{snake_case_type_name(name)};
    auto const helpers{context.declarations.items(enums.helpers[row])};
    auto const& domains{context.semantic.enum_domains()};
    auto const domain{domains.find(schema)};
    NodeListBuilder parsers{ir};
    for (auto const helper :
         {NativeEnumHelper::try_parse, NativeEnumHelper::try_parse_serialized}) {
        if (!std::ranges::contains(helpers, helper)) {
            continue;
        }

        std::vector<StringEnumEntry> entries;
        bool indexed{true};
        std::size_t ordinal{};
        for (auto const value : context.declarations.items(enums.values[row])) {
            auto const code{domains.code(domain, ordinal++)};
            auto const value_row{enumerators.index(value)};
            auto const value_name{context.text(enumerators.name[value_row])};
            std::string key;
            if (helper == NativeEnumHelper::try_parse_serialized) {
                auto const serialized{enumerators.serialized_name[value_row]};
                if (!serialized) {
                    continue;
                }
                key = context.text(*serialized);
            } else {
                key = value_name;
            }
            indexed = indexed && code && !code->negative && code->magnitude == entries.size();
            entries.push_back({std::move(key), name + "::" + value_name});
        }

        auto const prefix{helper == NativeEnumHelper::try_parse ? "try_parse_"
                                                                : "try_parse_serialized_"};
        parsers.add(
            lower_string_enum_parser(ir, prefix + snake_name, name, "value", entries, indexed), 2);
    }
    return parsers.build();
}

auto native_enum_traits(LoweringContext& context,
                        ModuleSettingsId settings,
                        DeclarationHandle schema) -> Nodes {
    auto& ir{context.ir};
    auto const& enums{context.declarations.enums};
    auto const qualified{native_enum_name(context, settings, schema)};
    auto const enum_type{ir.types.intern(qualified)};
    auto const schema_index{enums.index(schema)};
    auto const value_count{enums.values[schema_index].size() - (enums.count[schema_index] ? 1 : 0)};
    NodeListBuilder result{ir};
    if (std::ranges::contains(context.declarations.items(enums.helpers[schema_index]),
                              NativeEnumHelper::traits)) {
        std::vector<Expr> values;
        values.reserve(value_count);
        for (auto const value : context.declarations.items(enums.values[schema_index])) {
            auto const name{context.declarations.enumerators
                                .name[context.declarations.enumerators.index(value)]};
            if (enums.count[schema_index] && name == *enums.count[schema_index]) {
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

    if (enums.enum_array[schema_index]) {
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
                              DeclarationHandle const schema,
                              EnumUnrealProjectionId const projection) -> Nodes {
    auto& ir{context.ir};
    auto const& projections{context.declarations.enum_unreal_projections};
    auto const row{projections.index(projection)};
    auto const name{context.text(projections.name[row])};
    auto const underlying{ir.types.intern(
        unreal_underlying_type(ir.types.spelling(enum_underlying_type(context, schema))))};
    auto const& enums{context.declarations.enums};
    auto const enum_row{enums.index(schema)};
    auto const& enumerators{context.declarations.enumerators};
    auto const enum_values{context.declarations.items(enums.values[enum_row])};
    std::vector<EnumValueId> values;
    values.reserve(enum_values.size());
    for (auto const value : enum_values) {
        auto const value_row{enumerators.index(value)};
        values.push_back(ir.enumerator(enumerators.name[value_row],
                                       enumerators.initializer[value_row],
                                       ir.store_optional_text(annotation(context, value, true))));
    }

    NodeListBuilder declarations{ir};
    declarations.add(enum_bit_width_comment(context, schema), 1);
    declarations.add(ir.raw(projections.reflection[row] == EnumReflection::blueprint
                                ? "UENUM(BlueprintType)"
                                : "UENUM()"),
                     1);
    declarations.add(ir.enumeration(projections.name[row], underlying, values), 2);
    declarations.add(ir.using_declaration(ir.strings.intern(name + "Underlying"), underlying), 2);
    if (projections.enum_array[row]) {
        declarations.add(ir.namespace_node(
            ir.strings.intern("ioj"),
            {enum_count_specialization(ir, ir.types.intern("::" + name), enum_values.size())}));
    }
    return declarations.build();
}

auto native_conversion_function(LoweringContext& context,
                                std::string_view const function_name,
                                std::string const& source,
                                std::string const& target,
                                std::span<EnumeratorId const> const values) -> NodeId {
    auto& ir{context.ir};
    auto const& enumerators{context.declarations.enumerators};
    std::vector<SwitchCaseId> cases;
    cases.reserve(values.size());
    for (auto const value : values) {
        auto const name{context.text(enumerators.name[enumerators.index(value)])};
        cases.push_back(ir.switch_case(ir.named(source + "::" + name),
                                       ir.block({ir.return_stmt(ir.named(target + "::" + name))})));
    }

    auto const target_type{ir.types.intern(target)};
    auto const function{ir.function(function_name, "auto")};
    ir.set_function_return_type(function, target_type, ReturnSyntax::trailing);
    ir.set_function_parameters(function, {ir.parameter(source + " const", "value")});
    auto& flags{ir.function_flags(function)};
    flags.set_define_in_header(true);
    flags.set_is_constexpr(true);
    flags.set_is_nodiscard(true);
    ir.set_function_noexcept(function);
    NodeListBuilder body{ir};
    body.add(ir.switch_stmt(ir.named("value"), cases), 2);
    body.add(ir.return_stmt(ir.static_cast_expr(target_type, ir.named("value"))));
    ir.set_function_body(function, body.build());
    return ir.header_function(function);
}

auto unreal_conversion_header(LoweringContext& context,
                              ModuleSettingsId const settings,
                              DeclarationHandle const schema,
                              EnumUnrealProjectionId const projection) -> NodeId {
    auto& ir{context.ir};
    auto const native_name{native_enum_name(context, settings, schema)};
    auto const& projections{context.declarations.enum_unreal_projections};
    auto const projection_name{context.text(projections.name[projections.index(projection)])};
    auto const& enums{context.declarations.enums};
    auto const values{context.declarations.items(enums.values[enums.index(schema)])};

    NodeListBuilder declarations{ir};
    declarations.add(
        native_conversion_function(context, "to_native", projection_name, native_name, values), 2);
    declarations.add(
        native_conversion_function(context, "to_unreal", native_name, projection_name, values), 2);
    auto const& enumerators{context.declarations.enumerators};
    for (auto const value : values) {
        auto const name{context.text(enumerators.name[enumerators.index(value)])};
        declarations.add(
            ir.static_assertion(
                ir.store_text(std::format("static_cast<int>({}::{}) == static_cast<int>({}::{})",
                                          projection_name,
                                          name,
                                          native_name,
                                          name)),
                {}),
            1);
    }
    return ir.namespace_node(ir.strings.intern("ioj"), declarations.build());
}

} // namespace

auto lower_enum(LoweringContext& context,
                DeclarationHandle schema,
                ModuleSettingsId settings,
                std::optional<StringId> const helper_namespace) -> DeclarationEmission {
    auto& ir{context.ir};
    auto const& enums{context.declarations.enums};
    auto const& enumerators{context.declarations.enumerators};
    auto const& settings_store{context.declarations.module_settings_store};
    auto const row{enums.index(schema)};
    auto const settings_row{settings_store.index(settings)};
    DeclarationEmission emission;
    emission.source_dependencies = false;
    if (enums.native_api[row]) {
        NodeListBuilder prefix{ir};
        auto const needs_traits{
            std::ranges::contains(context.declarations.items(enums.helpers[row]),
                                  NativeEnumHelper::traits) ||
            enums.enum_array[row]};
        if (needs_traits) {
            prefix.add(ir.include(ir.store_text("ioj/enum_traits.h"), IncludeStyle::quoted), 2);
        }
        emission.header_prefix = prefix.build();
        NodeListBuilder declarations{ir};
        declarations.append(
            {native_enum_declaration(context, schema), native_enum_parsers(context, schema)});
        emission.header = declarations.build();
        if (needs_traits) {
            emission.header_global = {ir.namespace_node(
                ir.strings.intern("ioj"), native_enum_traits(context, settings, schema))};
        }
        if (auto const projection_id{enums.unreal_projection[row]}) {
            auto const projection{*projection_id};
            auto const& projections{context.declarations.enum_unreal_projections};
            auto const projection_row{projections.index(projection)};
            auto const generated_header{
                std::filesystem::path{context.text(projections.header[projection_row])}
                    .stem()
                    .string() +
                ".generated.h"};
            auto projection_nodes{unreal_projection_header(context, schema, projection)};
            projection_nodes.insert(
                projection_nodes.begin(),
                {ir.include(ir.store_text("CoreMinimal.h"), IncludeStyle::quoted),
                 ir.include(ir.store_text(generated_header), IncludeStyle::quoted)});
            auto const module_name{context.text(settings_store.name[settings_row]) + "_" +
                                   context.text(projections.name[projection_row])};
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
                 unreal_conversion_header(context, settings, schema, projection)})};
            ir.files.flags[ir.files.index(conversion_file)].set_clang_format_off(true);
            emission.additional_modules.push_back(
                ir.modules.append(ir.strings.intern(module_name + "_conversion"), conversion_file));
        }
        return emission;
    }

    NodeListBuilder declarations{ir};
    auto const underlying{enum_underlying_type(context, schema)};
    auto const underlying_alias{context.text(enums.name[row]) + "Underlying"};
    auto const reflected{enums.reflection[row] != EnumReflection::none};
    if (!reflected) {
        declarations.add(ir.using_declaration(ir.strings.intern(underlying_alias), underlying), 2);
    }
    declarations.add(enum_bit_width_comment(context, schema), 1);
    if (reflected) {
        declarations.add(ir.raw(enums.reflection[row] == EnumReflection::blueprint
                                    ? "UENUM(BlueprintType)"
                                    : "UENUM()",
                                {ir.types.dependency("UENUM", "CoreMinimal.h")}),
                         1);
        emission.header_generated_include = {ir.include(
            ir.store_text(std::filesystem::path{context.text(settings_store.header[settings_row])}
                              .stem()
                              .string() +
                          ".generated.h"),
            IncludeStyle::quoted)};
    }
    std::vector<EnumValueId> values;
    for (auto const value : context.declarations.items(enums.values[row])) {
        auto const value_row{enumerators.index(value)};
        values.push_back(
            ir.enumerator(enumerators.name[value_row],
                          enumerators.initializer[value_row],
                          ir.store_optional_text(annotation(context, value, reflected))));
    }
    declarations.add(ir.enumeration(enums.name[row],
                                    reflected ? ir.types.intern(unreal_underlying_type(
                                                    ir.types.spelling(underlying)))
                                              : ir.types.intern(underlying_alias),
                                    std::move(values)),
                     2);
    if (reflected) {
        // Preserve the built-in spelling required by UnrealHeaderTool for reflected enums.
        declarations.add(ir.using_declaration(ir.strings.intern(underlying_alias), underlying), 2);
    }
    if (enums.enum_array[row]) {
        emission.header_prefix.push_back(
            ir.include(ir.store_text("ioj/enum_traits.h"), IncludeStyle::quoted));
        emission.header_global.push_back(ir.namespace_node(
            ir.strings.intern("ioj"),
            {enum_count_specialization(
                ir,
                ir.types.intern("::" + qualified_enum_name(context, settings, schema)),
                enums.values[row].size())}));
    }

    NodeListBuilder lex_declarations{ir};
    NodeListBuilder helper_declarations{ir};
    auto const conversion_kinds{context.declarations.items(enums.conversions[row])};
    std::vector<FunctionId> conversions;
    conversions.reserve(conversion_kinds.size());
    for (auto const conversion : conversion_kinds) {
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
    auto const tail_namespace{
        context.optional_text(helper_namespace)
            .value_or(
                context.optional_text(settings_store.namespace_name[settings_row]).value_or(""))};
    auto helper_nodes{helper_declarations.build()};
    if (!helper_nodes.empty()) {
        emission.header_tail = std::move(helper_nodes);
        emission.tail_namespace = tail_namespace;
    }

    if (!conversion_kinds.empty()) {
        NodeListBuilder internal{ir};
        NodeListBuilder lexical{ir};
        NodeListBuilder helpers{ir};
        auto const has_display{std::ranges::any_of(conversion_kinds, [](auto const conversion) {
            return conversion == EnumConversion::lex_to_display_string ||
                   conversion == EnumConversion::display_string_view ||
                   conversion == EnumConversion::display_string;
        })};
        auto const has_name{has_display ||
                            std::ranges::any_of(conversion_kinds, [](auto const conversion) {
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
        if (std::ranges::contains(conversion_kinds, EnumConversion::lex_to_serialized_string)) {
            internal.add(ir.definition(serialized_lookup(context, settings, schema)), 2);
        }
        auto const conversion_count{conversion_kinds.size()};
        for (std::size_t index{}; index < conversion_count; ++index) {
            auto const conversion{conversion_kinds[index]};
            auto const is_lexical{conversion == EnumConversion::lex_to_string ||
                                  conversion == EnumConversion::lex_to_display_string ||
                                  conversion == EnumConversion::lex_to_serialized_string};
            (is_lexical ? lexical : helpers).add(ir.definition(conversions[index]), 2);
        }
        emission.source = lexical.build();
        emission.source_global = internal.build();
        emission.source_tail = helpers.build();
        emission.tail_namespace = tail_namespace;
    }
    return emission;
}

} // namespace codegen::detail
