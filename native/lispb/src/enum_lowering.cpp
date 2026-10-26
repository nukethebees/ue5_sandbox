#include "declaration_emission.h"
#include "lowering.h"
#include "lowering_context.h"
#include "lowering_utils.h"
#include "string_enum_parser.h"
#include <ioj/ascii.h>
#include <ioj/format_writer.h>

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
#include <limits>
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
    auto const namespace_name{settings_store.namespace_name[settings]};
    auto const& enums{context.declarations.enums};
    auto name{context.text(enums.name[schema])};
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
    return "get_" + snake_case_type_name(context.text(context.declarations.enums.name[schema])) +
           "_" + std::string{suffix};
}

} // namespace

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

namespace {

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

auto packed_enum_strings(LoweringContext& context,
                         ModuleSettingsId settings,
                         DeclarationHandle schema,
                         std::string_view category,
                         bool wide) -> NodeId {
    auto const& enums{context.declarations.enums};
    auto const& enumerators{context.declarations.enumerators};
    auto const& domains{context.semantic.enum_domains()};
    auto const domain{domains.find(schema)};
    auto const declared_values{context.declarations.items(enums.values[schema])};
    std::vector<EnumeratorId> values(declared_values.size());
    auto const value_count{declared_values.size()};
    for (std::size_t ordinal{}; ordinal < value_count; ++ordinal) {
        values[domains.code(domain, ordinal)->magnitude] = declared_values[ordinal];
    }
    auto const name{context.text(enums.name[schema])};
    auto const qualified{"::" + qualified_enum_name(context, settings, schema)};
    std::vector<std::string> names;
    names.reserve(value_count);
    for (auto const value : values) {
        auto const value_name{context.text(enumerators.name[value])};
        names.push_back(
            category == "Serialized" ? context.optional_text(enumerators.serialized_name[value])
                                           .value_or("<invalid " + name + ">")
            : category == "Display"
                ? context.optional_text(enumerators.display_name[value]).value_or(value_name)
                : value_name);
    }
    return enum_strings_specialization(context.ir, qualified, category, names, wide);
}

} // namespace

auto enum_strings_specialization(CppIR& ir,
                                 std::string_view qualified,
                                 std::string_view category,
                                 std::span<std::string const> names,
                                 bool wide) -> NodeId {
    std::string blob;
    std::vector<std::size_t> offsets{0};
    std::vector<std::size_t> wide_offsets{0};
    auto const offset_count{names.size() + 1};
    offsets.reserve(offset_count);
    wide_offsets.reserve(offset_count);
    for (std::string_view text : names) {
        auto next{offsets.back()};
        auto wide_next{wide_offsets.back()};
        // Preserve the old null-terminated literal conversion, including embedded nulls.
        text = text.substr(0, text.find('\0'));
        blob += text;
        blob += '\0';
        if (wide) {
            for (unsigned char const byte : text) {
                if ((byte & 0xc0) != 0x80) {
                    ++next;
                    wide_next += byte >= 0xf0 ? 2 : 1;
                }
            }
            ++next;
            ++wide_next;
        } else {
            next += text.size() + 1;
            wide_next = next;
        }
        offsets.push_back(next);
        wide_offsets.push_back(wide_next);
    }
    auto const maximum{wide_offsets.back()};
    if (maximum > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error{"Enum string table exceeds uint32_t offsets"};
    }
    auto const bits{maximum <= 255 ? 8 : maximum <= 65535 ? 16 : 32};
    if (!blob.empty()) {
        blob.pop_back();
    }
    auto const literal{[wide](std::string_view text) {
        auto result{std::format("\"{}\"", escape_cpp_string(text))};
        return wide ? std::format("TEXT({})", result) : result;
    }};
    std::string output;
    ioj::FormatWriter write{std::back_inserter(output)};
    write("template <>\ninline constexpr auto ioj::enum_strings<{}, ioj::{}>{{\n    "
          "ioj::PackedStrings{{\n        {},\n        std::array<std::uint{}_t, {}>{{",
          qualified,
          category,
          literal(blob),
          bits,
          offsets.size());
    auto const count{offsets.size()};
    for (std::size_t i{}; i < count; ++i) {
        if (i != 0) {
            output += ", ";
        }
        if (wide && offsets[i] != wide_offsets[i]) {
            write("(sizeof(TCHAR) == 2 ? {} : {})", wide_offsets[i], offsets[i]);
        } else {
            write("{}", offsets[i]);
        }
    }
    output += "}}};";
    return ir.raw(output, {ir.types.dependency("ioj::PackedStrings", "ioj/enum_strings.h")});
}

namespace {

auto string_lookup(LoweringContext& context,
                   ModuleSettingsId settings,
                   DeclarationHandle schema,
                   std::string_view category,
                   std::string_view suffix) -> FunctionId {
    auto const qualified{qualified_enum_name(context, settings, schema)};
    auto body{std::format("return ioj::enum_string<ioj::{}>(value).data();", category)};
    return lookup_function(context, schema, qualified, suffix, {context.ir.raw(body)});
}

auto serialized_parser(LoweringContext& context,
                       ModuleSettingsId settings,
                       DeclarationHandle schema) -> FunctionId {
    auto& ir{context.ir};
    auto const& enums{context.declarations.enums};
    auto const& enumerators{context.declarations.enumerators};
    auto const enum_name{qualified_enum_name(context, settings, schema)};
    NodeListBuilder body{ir};
    for (auto const value : context.declarations.items(enums.values[schema])) {
        auto const serialized_name{enumerators.serialized_name[value]};
        if (!serialized_name) {
            continue;
        }
        body.add(
            ir.if_stmt(ir.binary(BinaryOperator::equal,
                                 ir.named("value"),
                                 text_literal(ir, context.text(*serialized_name))),
                       ir.block({ir.assignment(ir.named("result"),
                                               ir.named(enum_name + "::" +
                                                        context.text(enumerators.name[value]))),
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
    ir.set_function_export_specifier(created_function, enums.export_specifier[schema]);
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
        context.optional_text(settings_store.namespace_name[settings]).value_or("")};
    auto const helper_in_type_namespace{
        context.optional_text(helper_namespace).value_or(type_namespace) == type_namespace};
    auto const parameter_type{lexical || helper_in_type_namespace ? context.text(enums.name[schema])
                                                                  : enum_name};
    auto const lookup{ir.call(
        ir.named(internal_name(
            context, schema, serialized ? "serialized_name" : (display ? "display_name" : "name"))),
        {ir.named("value")})};
    auto const result{ir.function(StringId{}, ir.types.intern("auto"))};
    ir.set_function_parameters(result, {ir.parameter(parameter_type + " const", "value")});
    ir.set_function_body(result, {ir.return_stmt(lookup)});
    ir.set_function_export_specifier(result, enums.export_specifier[schema]);
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
    if (!reflected || (!enumerators.display_name[value] && !enumerators.hidden[value])) {
        return std::nullopt;
    }
    std::vector<std::string> metadata;
    if (enumerators.display_name[value]) {
        metadata.push_back("DisplayName = \"" +
                           escape_cpp_string(context.text(*enumerators.display_name[value])) +
                           "\"");
    }
    if (enumerators.hidden[value]) {
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
    if (auto const& projection{enums.cpp_projection[schema]}; projection && projection->storage) {
        return context.resolve(*projection->storage);
    }
    if (enums.underlying_type[schema].has_value()) {
        return context.resolve(*enums.underlying_type[schema]);
    }

    auto const& domains{context.semantic.enum_domains()};
    auto const domain{domains.find(schema)};
    auto const requirement{
        schema::derive_enum_storage_requirement(domains, domain, enums.bit_width[schema])};
    if (!requirement.has_value()) {
        throw std::invalid_argument{"Enum '" + context.text(enums.name[schema]) +
                                    "' has no derivable C++ backing storage"};
    }

    auto const prefix{requirement->signedness ? "int" : "uint"};
    auto const spelling{"std::" + std::string{prefix} + std::to_string(requirement->bit_width) +
                        "_t"};
    return ir.types.intern(spelling, {ir.types.dependency(spelling, "cstdint")});
}

auto enum_bit_width_comment(LoweringContext& context, DeclarationHandle const schema) -> NodeId {
    auto const& enums{context.declarations.enums};
    auto const& domains{context.semantic.enum_domains()};
    auto const width{enums.bit_width[schema].has_value()
                         ? std::optional<std::uint8_t>{*enums.bit_width[schema]}
                         : domains.minimum_required_bits[domains.index(domains.find(schema))]};
    return context.ir.line_comment(
        width ? std::format("Logical bit width: {}.", *width)
              : "Logical bit width depends on the enumerator expressions.");
}

auto native_enum_declaration(LoweringContext& context, DeclarationHandle const schema) -> Nodes {
    auto& ir{context.ir};
    auto const& enums{context.declarations.enums};
    auto const alias{context.text(enums.name[schema]) + "Underlying"};
    auto const& enumerators{context.declarations.enumerators};
    std::vector<EnumValueId> values;
    for (auto const value : context.declarations.items(enums.values[schema])) {
        values.push_back(ir.enumerator(enumerators.name[value], enumerators.initializer[value]));
    }

    NodeListBuilder declarations{ir};
    declarations.add(
        ir.using_declaration(ir.strings.intern(alias), enum_underlying_type(context, schema)), 2);
    declarations.add(enum_bit_width_comment(context, schema), 1);
    declarations.add(ir.enumeration(enums.name[schema], ir.types.intern(alias), values), 2);

    auto const helpers{context.declarations.items(enums.helpers[schema])};
    for (auto const helper : {NativeEnumHelper::to_string,
                              NativeEnumHelper::to_display_string,
                              NativeEnumHelper::to_serialized_string}) {
        if (std::ranges::contains(helpers, helper)) {
            auto const name{helper == NativeEnumHelper::to_string ? "to_string"
                            : helper == NativeEnumHelper::to_display_string
                                ? "to_display_string"
                                : "to_serialized_string"};
            declarations.add(ir.using_import(ir.types.named(
                                 std::format("ioj::{}", name),
                                 {ir.types.dependency("ioj::enum_strings", "ioj/enum_strings.h")})),
                             1);
        }
    }
    return declarations.build();
}

auto native_enum_parsers(LoweringContext& context, DeclarationHandle const schema) -> Nodes {
    auto& ir{context.ir};
    auto const& enums{context.declarations.enums};
    auto const& enumerators{context.declarations.enumerators};
    auto const name{context.text(enums.name[schema])};
    auto const snake_name{snake_case_type_name(name)};
    auto const helpers{context.declarations.items(enums.helpers[schema])};
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
        for (auto const value : context.declarations.items(enums.values[schema])) {
            auto const code{domains.code(domain, ordinal++)};
            auto const value_name{context.text(enumerators.name[value])};
            std::string key;
            if (helper == NativeEnumHelper::try_parse_serialized) {
                auto const serialized{enumerators.serialized_name[value]};
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
    auto const value_count{enums.values[schema].size() - (enums.count[schema] ? 1 : 0)};
    NodeListBuilder result{ir};
    if (std::ranges::contains(context.declarations.items(enums.helpers[schema]),
                              NativeEnumHelper::traits)) {
        std::vector<Expr> values;
        values.reserve(value_count);
        for (auto const value : context.declarations.items(enums.values[schema])) {
            auto const name{context.declarations.enumerators.name[value]};
            if (enums.count[schema] && name == *enums.count[schema]) {
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

    if (enums.enum_array[schema]) {
        result.add(enum_count_specialization(ir, enum_type, value_count));
    }
    return result.build();
}

auto native_enum_formatter(LoweringContext& context,
                           ModuleSettingsId settings,
                           DeclarationHandle schema) -> NodeId {
    auto& ir{context.ir};
    auto const enum_type{ir.types.intern(native_enum_name(context, settings, schema))};
    auto const formatter{
        ir.record(ir.template_id(ir.named("formatter"), {ir.type_reference(enum_type)}), {})};
    ir.set_record_template(formatter, ir.template_specialization());
    ir.set_record_bases(
        formatter,
        {ir.types.template_type(
            ir.types.named("ioj::EnumFormatter",
                           {ir.types.dependency("ioj::EnumFormatter", "ioj/enum_formatter.h")}),
            {enum_type})});
    return ir.namespace_node(ir.strings.intern("std"), {formatter});
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
    auto const name{context.text(projections.name[projection])};
    auto const underlying{ir.types.intern(
        unreal_underlying_type(ir.types.spelling(enum_underlying_type(context, schema))))};
    auto const& enums{context.declarations.enums};
    auto const& enumerators{context.declarations.enumerators};
    auto const enum_values{context.declarations.items(enums.values[schema])};
    std::vector<EnumValueId> values;
    values.reserve(enum_values.size());
    for (auto const value : enum_values) {
        values.push_back(ir.enumerator(enumerators.name[value],
                                       enumerators.initializer[value],
                                       ir.store_optional_text(annotation(context, value, true))));
    }

    NodeListBuilder declarations{ir};
    declarations.add(enum_bit_width_comment(context, schema), 1);
    declarations.add(ir.raw(projections.reflection[projection] == EnumReflection::blueprint
                                ? "UENUM(BlueprintType)"
                                : "UENUM()"),
                     1);
    declarations.add(ir.enumeration(projections.name[projection], underlying, values), 2);
    declarations.add(ir.using_declaration(ir.strings.intern(name + "Underlying"), underlying), 2);
    if (projections.enum_array[projection]) {
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
        auto const name{context.text(enumerators.name[value])};
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
    auto const projection_name{context.text(projections.name[projection])};
    auto const& enums{context.declarations.enums};
    auto const values{context.declarations.items(enums.values[schema])};

    NodeListBuilder declarations{ir};
    declarations.add(
        native_conversion_function(context, "to_native", projection_name, native_name, values), 2);
    declarations.add(
        native_conversion_function(context, "to_unreal", native_name, projection_name, values), 2);
    auto const& enumerators{context.declarations.enumerators};
    for (auto const value : values) {
        auto const name{context.text(enumerators.name[value])};
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
    DeclarationEmission emission;
    if (!enums.cpp_projection[schema]) {
        return emission;
    }
    emission.source_dependencies = false;
    if (enums.native_api[schema]) {
        NodeListBuilder prefix{ir};
        auto const needs_traits{
            std::ranges::contains(context.declarations.items(enums.helpers[schema]),
                                  NativeEnumHelper::traits) ||
            enums.enum_array[schema]};
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
        auto const helpers{context.declarations.items(enums.helpers[schema])};
        for (auto const helper : {NativeEnumHelper::to_string,
                                  NativeEnumHelper::to_display_string,
                                  NativeEnumHelper::to_serialized_string}) {
            if (std::ranges::contains(helpers, helper)) {
                auto const category{helper == NativeEnumHelper::to_string           ? "Name"
                                    : helper == NativeEnumHelper::to_display_string ? "Display"
                                                                                    : "Serialized"};
                emission.header_global.push_back(
                    packed_enum_strings(context, settings, schema, category, false));
            }
        }
        if (std::ranges::contains(context.declarations.items(enums.helpers[schema]),
                                  NativeEnumHelper::formatter)) {
            emission.header_global.push_back(native_enum_formatter(context, settings, schema));
        }
        if (auto const projection_id{enums.unreal_projection[schema]}) {
            auto const projection{*projection_id};
            auto const& projections{context.declarations.enum_unreal_projections};
            auto const generated_header{
                std::filesystem::path{context.text(projections.header[projection])}
                    .stem()
                    .string() +
                ".generated.h"};
            auto projection_nodes{unreal_projection_header(context, schema, projection)};
            projection_nodes.insert(
                projection_nodes.begin(),
                {ir.include(ir.store_text("CoreMinimal.h"), IncludeStyle::quoted),
                 ir.include(ir.store_text(generated_header), IncludeStyle::quoted)});
            auto const module_name{context.text(settings_store.name[settings]) + "_" +
                                   context.text(projections.name[projection])};
            if (projections.enum_array[projection]) {
                projection_nodes.insert(
                    projection_nodes.begin() + 1,
                    ir.include(ir.store_text("ioj/enum_traits.h"), IncludeStyle::quoted));
            }
            auto const projection_file{
                ir.files.append(projections.header[projection], projection_nodes)};
            ir.files.flags[ir.files.index(projection_file)].set_clang_format_off(true);
            emission.additional_modules.push_back(
                ir.modules.append(ir.strings.intern(module_name), projection_file));
            auto const conversion_file{ir.files.append(
                projections.conversion_header[projection],
                {ir.include(projections.native_header_include[projection], IncludeStyle::quoted),
                 ir.include(projections.header_include[projection], IncludeStyle::quoted),
                 unreal_conversion_header(context, settings, schema, projection)})};
            ir.files.flags[ir.files.index(conversion_file)].set_clang_format_off(true);
            emission.additional_modules.push_back(
                ir.modules.append(ir.strings.intern(module_name + "_conversion"), conversion_file));
        }
        return emission;
    }

    NodeListBuilder declarations{ir};
    auto const underlying{enum_underlying_type(context, schema)};
    auto const underlying_alias{context.text(enums.name[schema]) + "Underlying"};
    auto const reflected{enums.reflection[schema] != EnumReflection::none};
    if (!reflected) {
        declarations.add(ir.using_declaration(ir.strings.intern(underlying_alias), underlying), 2);
    }
    declarations.add(enum_bit_width_comment(context, schema), 1);
    if (reflected) {
        declarations.add(ir.raw(enums.reflection[schema] == EnumReflection::blueprint
                                    ? "UENUM(BlueprintType)"
                                    : "UENUM()",
                                {ir.types.dependency("UENUM", "CoreMinimal.h")}),
                         1);
        emission.header_generated_include = {ir.include(
            ir.store_text(std::filesystem::path{context.text(settings_store.header[settings])}
                              .stem()
                              .string() +
                          ".generated.h"),
            IncludeStyle::quoted)};
    }
    std::vector<EnumValueId> values;
    for (auto const value : context.declarations.items(enums.values[schema])) {
        values.push_back(
            ir.enumerator(enumerators.name[value],
                          enumerators.initializer[value],
                          ir.store_optional_text(annotation(context, value, reflected))));
    }
    declarations.add(ir.enumeration(enums.name[schema],
                                    reflected ? ir.types.intern(unreal_underlying_type(
                                                    ir.types.spelling(underlying)))
                                              : ir.types.intern(underlying_alias),
                                    std::move(values)),
                     2);
    if (reflected) {
        // Preserve the built-in spelling required by UnrealHeaderTool for reflected enums.
        declarations.add(ir.using_declaration(ir.strings.intern(underlying_alias), underlying), 2);
    }
    if (enums.enum_array[schema]) {
        emission.header_prefix.push_back(
            ir.include(ir.store_text("ioj/enum_traits.h"), IncludeStyle::quoted));
        emission.header_global.push_back(ir.namespace_node(
            ir.strings.intern("ioj"),
            {enum_count_specialization(
                ir,
                ir.types.intern("::" + qualified_enum_name(context, settings, schema)),
                enums.values[schema].size())}));
    }

    NodeListBuilder lex_declarations{ir};
    NodeListBuilder helper_declarations{ir};
    auto const conversion_kinds{context.declarations.items(enums.conversions[schema])};
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
            .value_or(context.optional_text(settings_store.namespace_name[settings]).value_or(""))};
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
        auto const has_name{std::ranges::any_of(conversion_kinds, [](auto const conversion) {
            return conversion == EnumConversion::lex_to_string ||
                   conversion == EnumConversion::string_view ||
                   conversion == EnumConversion::string;
        })};
        if (has_name) {
            emission.header_global.push_back(
                packed_enum_strings(context, settings, schema, "Name", true));
            internal.add(ir.definition(string_lookup(context, settings, schema, "Name", "name")),
                         2);
        }
        if (has_display) {
            emission.header_global.push_back(
                packed_enum_strings(context, settings, schema, "Display", true));
            internal.add(
                ir.definition(string_lookup(context, settings, schema, "Display", "display_name")),
                2);
        }
        if (std::ranges::contains(conversion_kinds, EnumConversion::lex_to_serialized_string)) {
            emission.header_global.push_back(
                packed_enum_strings(context, settings, schema, "Serialized", true));
            internal.add(ir.definition(string_lookup(
                             context, settings, schema, "Serialized", "serialized_name")),
                         2);
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
