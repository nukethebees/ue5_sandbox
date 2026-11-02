#include "declaration_emission.h"
#include "lowering.h"
#include "lowering_context.h"
#include "lowering_utils.h"
#include "string_enum_parser.h"

#include <codegen/ast/binary_operator.h>
#include <codegen/ast/cpp_files.h>
#include <codegen/ast/cpp_ir_builder.h>
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
#include <codegen/fixed_width_integer_types.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/enum_conversion.h>
#include <codegen/schema/enum_reflection.h>
#include <codegen/schema/enum_schemas.h>
#include <codegen/schema/enum_unreal_projections.h>
#include <codegen/schema/enumerator_schemas.h>
#include <codegen/schema/module_settings_store.h>
#include <codegen/schema/native_enum_helper.h>
#include <codegen/schema/packed_integer_value.h>
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

class EnumLowering : public CppIRBuilder {
  public:
    explicit EnumLowering(CppIR& ir)
        : CppIRBuilder{ir} {}

    /* **************************************** */
    // Lowering
    /* **************************************** */
    auto enum_count_specialization(CppTypeId enum_type, std::size_t const count) -> NodeId {
        auto const function_id{
            function(template_id(named("enum_count"), {type_reference(enum_type)}),
                     ir_.types.intern("auto"))};
        set_function_template(function_id, template_specialization());
        set_function_return_type(function_id, ir_.types.std.size_t, ReturnSyntax::trailing);
        auto& flags{function_flags(function_id)};
        flags.set_is_nodiscard(true);
        flags.set_is_constexpr(true);
        set_function_noexcept(function_id);
        flags.set_define_in_header(true);
        set_function_body(function_id, {return_stmt(literal(std::to_string(count)))});
        return header_function(function_id);
    }
    auto enum_strings_specialization(std::string_view qualified,
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
        auto const literal_value{wide ? text_literal(blob) : string_literal(blob)};
        std::vector<Expr> offset_values;
        auto const count{offsets.size()};
        for (std::size_t i{}; i < count; ++i) {
            if (wide && offsets[i] != wide_offsets[i]) {
                offset_values.push_back(conditional(binary(BinaryOperator::equal,
                                                           sizeof_type(ir_.types.named("TCHAR")),
                                                           literal("2")),
                                                    literal(std::to_string(wide_offsets[i])),
                                                    literal(std::to_string(offsets[i]))));
            } else {
                offset_values.push_back(literal(std::to_string(offsets[i])));
            }
        }
        auto const offset_type{ir_.types.std.integer(bits)};
        auto const offsets_type{ir_.types.template_type(
            ir_.types.std.array, {offset_type, ir_.types.intern(std::to_string(count))})};
        auto const initializer{init_list(
            {literal_value, init_list(offset_values, offsets_type)},
            ir_.types.named("ioj::PackedStrings",
                            NameOrigin::global,
                            {ir_.types.dependency("ioj::PackedStrings", "ioj/enum_strings.h")}))};
        auto const variable_id{
            variable(ir_.types.intern("auto"),
                     template_id(named("enum_strings"),
                                 {type_reference(ir_.types.named(qualified, NameOrigin::global)),
                                  type_reference(ir_.types.named("ioj::" + std::string{category},
                                                                 NameOrigin::global))}),
                     initializer,
                     {.template_declaration = template_specialization(),
                      .is_inline = true,
                      .is_constexpr = true})};
        return namespace_node(store_text("ioj"), {variable_id});
    }
    auto lower_enum(LoweringContext& ctx,
                    DeclarationHandle schema,
                    ModuleSettingsId settings,
                    std::optional<StringId> const helper_namespace) -> DeclarationEmission {
        auto const& enums{ctx.declarations.enums};
        auto const& settings_store{ctx.declarations.module_settings_store};
        DeclarationEmission emission;
        if (!enums.cpp_projection[schema]) {
            return emission;
        }
        auto const qualified{qualified_enum_name(ctx, settings, schema)};
        auto const enum_type{ir_.types.named(
            qualified.contains("::") ? qualified : "::" + qualified, NameOrigin::global)};
        emission.source_dependencies = false;
        if (enums.native_api[schema]) {
            NodeListBuilder prefix{ir_};
            auto const needs_traits{
                std::ranges::contains(ctx.declarations.items(enums.helpers[schema]),
                                      NativeEnumHelper::traits) ||
                enums.enum_array[schema]};
            prefix.add(include(store_text("ioj/enum_traits.h"), IncludeStyle::quoted), 2);
            emission.header_prefix = prefix.build();
            NodeListBuilder decls{ir_};
            decls.append({native_enum_declaration(ctx, schema), native_enum_parsers(ctx, schema)});
            emission.header = decls.build();
            emission.header_immediate_global = enum_range_declaration(ctx, schema, enum_type);
            if (needs_traits) {
                emission.header_global = {namespace_node(
                    ir_.strings.intern("ioj"), native_enum_traits(ctx, schema, enum_type))};
            }
            auto const helpers{ctx.declarations.items(enums.helpers[schema])};
            for (auto const helper : {NativeEnumHelper::to_string,
                                      NativeEnumHelper::to_display_string,
                                      NativeEnumHelper::to_serialized_string}) {
                if (std::ranges::contains(helpers, helper)) {
                    auto const category{helper == NativeEnumHelper::to_string ? "Name"
                                        : helper == NativeEnumHelper::to_display_string
                                            ? "Display"
                                            : "Serialized"};
                    emission.header_global.push_back(
                        packed_enum_strings(ctx, settings, schema, category, false));
                }
            }
            if (std::ranges::contains(ctx.declarations.items(enums.helpers[schema]),
                                      NativeEnumHelper::formatter)) {
                emission.header_global.push_back(native_enum_formatter(ctx, settings, schema));
            }
            if (auto const projection_id{enums.unreal_projection[schema]}) {
                auto const projection{*projection_id};
                auto const& projections{ctx.declarations.enum_unreal_projections};
                auto const generated_header{
                    std::filesystem::path{ctx.text(projections.header[projection])}
                        .stem()
                        .string() +
                    ".generated.h"};
                auto projection_nodes{unreal_projection_header(ctx, schema, projection)};
                projection_nodes.insert(
                    projection_nodes.begin(),
                    {include(store_text("CoreMinimal.h"), IncludeStyle::quoted),
                     include(store_text("ioj/enum_traits.h"), IncludeStyle::quoted),
                     include(store_text(generated_header), IncludeStyle::quoted)});
                auto const module_name{ctx.text(settings_store.name[settings]) + "_" +
                                       ctx.text(projections.name[projection])};
                auto const projection_file{
                    ir_.files.append(projections.header[projection], projection_nodes)};
                ir_.files.flags[ir_.files.index(projection_file)].set_clang_format_off(true);
                emission.additional_modules.push_back(
                    ir_.modules.append(ir_.strings.intern(module_name), projection_file));
                auto const conversion_file{ir_.files.append(
                    projections.conversion_header[projection],
                    {include(projections.native_header_include[projection], IncludeStyle::quoted),
                     include(projections.header_include[projection], IncludeStyle::quoted),
                     unreal_conversion_header(ctx, settings, schema, projection)})};
                ir_.files.flags[ir_.files.index(conversion_file)].set_clang_format_off(true);
                emission.additional_modules.push_back(ir_.modules.append(
                    ir_.strings.intern(module_name + "_conversion"), conversion_file));
            }
            return emission;
        }

        NodeListBuilder decls{ir_};
        auto const underlying{enum_underlying_type(ctx, schema)};
        auto const underlying_alias{ctx.text(enums.name[schema]) + "Underlying"};
        auto const reflected{enums.reflection[schema] != EnumReflection::none};
        if (!reflected) {
            decls.add(using_declaration(ir_.strings.intern(underlying_alias), underlying), 2);
        }
        decls.add(enum_bit_width_comment(ctx, schema), 1);
        if (reflected) {
            decls.add(reflection_annotation(enums.reflection[schema]), 1);
            emission.header_generated_include = {
                include(store_text(std::filesystem::path{ctx.text(settings_store.header[settings])}
                                       .stem()
                                       .string() +
                                   ".generated.h"),
                        IncludeStyle::quoted)};
        }
        decls.add(enumeration(enums.name[schema],
                              reflected ? ir_.types.intern(unreal_underlying_type(
                                              ir_.types.spelling(underlying)))
                                        : ir_.types.intern(underlying_alias),
                              enum_values(ctx, schema, reflected)),
                  2);
        if (reflected) {
            // Preserve the built-in spelling required by UnrealHeaderTool for reflected enums.
            decls.add(using_declaration(ir_.strings.intern(underlying_alias), underlying), 2);
        }
        emission.header_immediate_global = enum_range_declaration(ctx, schema, enum_type);
        emission.header_prefix.push_back(
            include(store_text("ioj/enum_traits.h"), IncludeStyle::quoted));
        if (enums.enum_array[schema]) {
            emission.header_global.push_back(namespace_node(
                ir_.strings.intern("ioj"),
                {enum_count_specialization(enum_type, enums.values[schema].size())}));
        }

        NodeListBuilder lex_declarations{ir_};
        NodeListBuilder helper_declarations{ir_};
        auto const conversion_kinds{ctx.declarations.items(enums.conversions[schema])};
        std::vector<FunctionId> conversions;
        conversions.reserve(conversion_kinds.size());
        for (auto const conversion : conversion_kinds) {
            auto const spec{
                conversion_function(ctx, settings, schema, conversion, helper_namespace)};
            conversions.push_back(spec);
            auto const lexical{conversion == EnumConversion::lex_to_string ||
                               conversion == EnumConversion::lex_to_display_string ||
                               conversion == EnumConversion::lex_to_serialized_string};
            (lexical ? lex_declarations : helper_declarations).add(declaration(spec), 2);
        }
        emission.header = decls.build();
        emission.header_after = lex_declarations.build();
        auto const tail_namespace{
            ctx.optional_text(helper_namespace)
                .value_or(ctx.optional_text(settings_store.namespace_name[settings]).value_or(""))};
        auto helper_nodes{helper_declarations.build()};
        if (!helper_nodes.empty()) {
            emission.header_tail = std::move(helper_nodes);
            emission.tail_namespace = tail_namespace;
        }

        if (!conversion_kinds.empty()) {
            NodeListBuilder internal{ir_};
            NodeListBuilder lexical{ir_};
            NodeListBuilder helpers{ir_};
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
                    packed_enum_strings(ctx, settings, schema, "Name", true));
                internal.add(definition(string_lookup(ctx, settings, schema, "Name", "name")), 2);
            }
            if (has_display) {
                emission.header_global.push_back(
                    packed_enum_strings(ctx, settings, schema, "Display", true));
                internal.add(
                    definition(string_lookup(ctx, settings, schema, "Display", "display_name")), 2);
            }
            if (std::ranges::contains(conversion_kinds, EnumConversion::lex_to_serialized_string)) {
                emission.header_global.push_back(
                    packed_enum_strings(ctx, settings, schema, "Serialized", true));
                internal.add(definition(string_lookup(
                                 ctx, settings, schema, "Serialized", "serialized_name")),
                             2);
            }
            auto const conversion_count{conversion_kinds.size()};
            for (std::size_t index{}; index < conversion_count; ++index) {
                auto const conversion{conversion_kinds[index]};
                auto const is_lexical{conversion == EnumConversion::lex_to_string ||
                                      conversion == EnumConversion::lex_to_display_string ||
                                      conversion == EnumConversion::lex_to_serialized_string};
                (is_lexical ? lexical : helpers).add(definition(conversions[index]), 2);
            }
            emission.source = lexical.build();
            emission.source_global = internal.build();
            emission.source_tail = helpers.build();
            emission.tail_namespace = tail_namespace;
        }
        return emission;
    }
  private:
    /* **************************************** */
    // Construction helpers
    /* **************************************** */
    auto text_literal(std::string_view const value) -> Expr {
        return call(named("TEXT", {ir_.types.dependency("TEXT", "CoreMinimal.h")}),
                    {string_literal(value)});
    }
    auto reflection_annotation(EnumReflection const reflection) -> NodeId {
        auto const macro{named("UENUM", {ir_.types.dependency("UENUM", "CoreMinimal.h")})};
        return declaration_macro(reflection == EnumReflection::blueprint
                                     ? call(macro, {named("BlueprintType")})
                                     : call(macro));
    }
    auto qualified_enum_name(LoweringContext& ctx,
                             ModuleSettingsId settings,
                             DeclarationHandle schema) -> std::string {
        auto const& settings_store{ctx.declarations.module_settings_store};
        auto const namespace_name{settings_store.namespace_name[settings]};
        auto const& enums{ctx.declarations.enums};
        auto name{ctx.text(enums.name[schema])};
        if (namespace_name) {
            return ctx.text(*namespace_name) + "::" + name;
        }
        return name;
    }
    auto internal_name(LoweringContext& ctx,
                       DeclarationHandle schema,
                       std::string_view const suffix) -> std::string {
        return "get_" + snake_case_type_name(ctx.text(ctx.declarations.enums.name[schema])) + "_" +
               std::string{suffix};
    }
    auto lookup_function(LoweringContext& ctx,
                         DeclarationHandle schema,
                         std::string const& enum_name,
                         std::string_view suffix,
                         Nodes body) -> FunctionId {
        auto const function_id{function(internal_name(ctx, schema, suffix), "auto")};
        set_function_parameters(
            function_id,
            {parameter(ir_.types.decorate(ir_.types.named(enum_name), "", " const"), "value")});
        set_function_body(function_id, body);
        set_function_return_type(
            function_id,
            ir_.types.decorate(
                ir_.types.named("TCHAR", {ir_.types.dependency("TCHAR", "CoreMinimal.h")}),
                "",
                " const*"),
            ReturnSyntax::trailing);
        return function_id;
    }
    auto packed_enum_strings(LoweringContext& ctx,
                             ModuleSettingsId settings,
                             DeclarationHandle schema,
                             std::string_view category,
                             bool wide) -> NodeId {
        auto const& enums{ctx.declarations.enums};
        auto const& enumerators{ctx.declarations.enumerators};
        auto const& domains{ctx.semantic.enum_domains()};
        auto const domain{domains.find(schema)};
        auto const declared_values{ctx.declarations.items(enums.values[schema])};
        std::vector<EnumeratorId> values(declared_values.size());
        auto const value_count{declared_values.size()};
        for (std::size_t ordinal{}; ordinal < value_count; ++ordinal) {
            values[domains.code(domain, ordinal)->magnitude] = declared_values[ordinal];
        }
        auto const name{ctx.text(enums.name[schema])};
        auto const qualified{qualified_enum_name(ctx, settings, schema)};
        std::vector<std::string> names;
        names.reserve(value_count);
        for (auto const value : values) {
            auto const value_name{ctx.text(enumerators.name[value])};
            names.push_back(
                category == "Serialized" ? ctx.optional_text(enumerators.serialized_name[value])
                                               .value_or("<invalid " + name + ">")
                : category == "Display"
                    ? ctx.optional_text(enumerators.display_name[value]).value_or(value_name)
                    : value_name);
        }
        return enum_strings_specialization(qualified, category, names, wide);
    }
    auto string_lookup(LoweringContext& ctx,
                       ModuleSettingsId settings,
                       DeclarationHandle schema,
                       std::string_view category,
                       std::string_view suffix) -> FunctionId {
        auto const qualified{qualified_enum_name(ctx, settings, schema)};
        auto const lookup{call(
            template_id(type_reference(ir_.types.named("ioj::enum_string", NameOrigin::global)),
                        {type_reference(
                            ir_.types.named("ioj::" + std::string{category}, NameOrigin::global))}),
            {named("value")})};
        return lookup_function(
            ctx, schema, qualified, suffix, {return_stmt(call(member_access(lookup, "data")))});
    }
    auto serialized_parser(LoweringContext& ctx,
                           ModuleSettingsId settings,
                           DeclarationHandle schema) -> FunctionId {
        auto const& enums{ctx.declarations.enums};
        auto const& enumerators{ctx.declarations.enumerators};
        auto const enum_name{qualified_enum_name(ctx, settings, schema)};
        NodeListBuilder body{ir_};
        for (auto const value : ctx.declarations.items(enums.values[schema])) {
            auto const serialized_name{enumerators.serialized_name[value]};
            if (!serialized_name) {
                continue;
            }
            body.add(if_stmt(binary(BinaryOperator::equal,
                                    named("value"),
                                    text_literal(ctx.text(*serialized_name))),
                             block({assignment(
                                        named("result"),
                                        type_reference(ir_.types.named(
                                            enum_name + "::" + ctx.text(enumerators.name[value])))),
                                    return_stmt(literal("true"))})),
                     1);
        }
        body.add(return_stmt(literal("false")));
        auto const created_function{function("try_parse_serialized", "auto")};
        set_function_parameters(
            created_function,
            {parameter(ir_.types.decorate(
                           ir_.types.named("FStringView",
                                           {ir_.types.dependency("FStringView", "CoreMinimal.h")}),
                           "",
                           " const"),
                       "value"),
             parameter(ir_.types.decorate(ir_.types.named(enum_name), "", "&"), "result")});
        set_function_body(created_function, body.build());
        set_function_return_type(created_function, ir_.types.std.bool_, ReturnSyntax::trailing);
        set_function_export_specifier(created_function, enums.export_specifier[schema]);
        return created_function;
    }
    auto conversion_function(LoweringContext& ctx,
                             ModuleSettingsId settings,
                             DeclarationHandle schema,
                             EnumConversion const conversion,
                             std::optional<StringId> const helper_namespace) -> FunctionId {
        auto const& enums{ctx.declarations.enums};
        auto const& settings_store{ctx.declarations.module_settings_store};
        if (conversion == EnumConversion::try_parse_serialized) {
            return serialized_parser(ctx, settings, schema);
        }
        auto const enum_name{qualified_enum_name(ctx, settings, schema)};
        auto const display{conversion == EnumConversion::lex_to_display_string ||
                           conversion == EnumConversion::display_string_view ||
                           conversion == EnumConversion::display_string};
        auto const serialized{conversion == EnumConversion::lex_to_serialized_string};
        auto const lexical{conversion == EnumConversion::lex_to_string ||
                           conversion == EnumConversion::lex_to_display_string || serialized};
        auto const type_namespace{
            ctx.optional_text(settings_store.namespace_name[settings]).value_or("")};
        auto const helper_in_type_namespace{
            ctx.optional_text(helper_namespace).value_or(type_namespace) == type_namespace};
        auto const parameter_type_value{
            lexical || helper_in_type_namespace ? ctx.text(enums.name[schema]) : enum_name};
        auto const lookup{call(
            named(internal_name(
                ctx, schema, serialized ? "serialized_name" : (display ? "display_name" : "name"))),
            {named("value")})};
        auto const result{function(StringId{}, ir_.types.intern("auto"))};
        set_function_parameters(
            result,
            {parameter(ir_.types.decorate(ir_.types.named(parameter_type_value), "", " const"),
                       "value")});
        set_function_body(result, {return_stmt(lookup)});
        set_function_export_specifier(result, enums.export_specifier[schema]);
        auto configure_result = [&](std::string_view name, CppTypeId type, bool construct = false) {
            set_function_name(result, ir_.strings.intern(name));
            set_function_return_type(result, type, ReturnSyntax::trailing);
            if (construct) {
                set_function_body(result, {return_stmt(init_list({lookup}, type))});
            }
        };
        auto unreal_type = [&](std::string_view name) {
            return ir_.types.named(name, {ir_.types.dependency(name, "CoreMinimal.h")});
        };
        auto const text_pointer{ir_.types.decorate(unreal_type("TCHAR"), "", " const*")};
        switch (conversion) {
            case EnumConversion::lex_to_string:
                configure_result("LexToString", text_pointer);
                break;
            case EnumConversion::string_view:
                configure_result("to_string_view", unreal_type("FStringView"), true);
                break;
            case EnumConversion::string:
                configure_result("to_string", unreal_type("FString"), true);
                break;
            case EnumConversion::lex_to_display_string:
                configure_result("LexToDisplayString", text_pointer);
                break;
            case EnumConversion::display_string_view:
                configure_result("to_display_string_view", unreal_type("FStringView"), true);
                break;
            case EnumConversion::display_string:
                configure_result("to_display_string", unreal_type("FString"), true);
                break;
            case EnumConversion::lex_to_serialized_string:
                configure_result("LexToSerializedString", text_pointer);
                break;
            case EnumConversion::try_parse_serialized:
                break;
        }
        return result;
    }
    auto annotation(LoweringContext& ctx, EnumeratorId value, bool const reflected)
        -> std::optional<std::string> {
        auto const& enumerators{ctx.declarations.enumerators};
        if (!reflected || (!enumerators.display_name[value] && !enumerators.hidden[value])) {
            return std::nullopt;
        }
        std::vector<std::string> metadata;
        if (enumerators.display_name[value]) {
            metadata.push_back("DisplayName = \"" +
                               escape_cpp_string(ctx.text(*enumerators.display_name[value])) +
                               "\"");
        }
        if (enumerators.hidden[value]) {
            metadata.emplace_back("Hidden");
        }
        return "UMETA(" + join(metadata, ", ") + ")";
    }
    auto enum_values(LoweringContext& ctx, DeclarationHandle schema, bool reflected)
        -> std::vector<EnumValueId> {
        auto const& decls{ctx.declarations};
        auto const& enumerators{decls.enumerators};
        std::vector<EnumValueId> values;
        values.reserve(decls.enums.values[schema].size());
        for (auto const value : decls.items(decls.enums.values[schema])) {
            values.push_back(enumerator(enumerators.name[value],
                                        enumerators.initializer[value],
                                        store_optional_text(annotation(ctx, value, reflected))));
        }
        return values;
    }
    auto native_enum_name(LoweringContext& ctx, ModuleSettingsId settings, DeclarationHandle schema)
        -> std::string {
        return "::" + qualified_enum_name(ctx, settings, schema);
    }
    auto enum_underlying_type(LoweringContext& ctx, DeclarationHandle schema) -> CppTypeId {
        auto const& enums{ctx.declarations.enums};
        if (auto const& projection{enums.cpp_projection[schema]};
            projection && projection->storage) {
            return ctx.resolve(*projection->storage);
        }
        if (enums.underlying_type[schema].has_value()) {
            return ctx.resolve(*enums.underlying_type[schema]);
        }

        auto const& domains{ctx.semantic.enum_domains()};
        auto const domain{domains.find(schema)};
        auto const requirement{
            schema::derive_enum_storage_requirement(domains, domain, enums.bit_width[schema])};
        if (!requirement.has_value()) {
            throw std::invalid_argument{"Enum '" + ctx.text(enums.name[schema]) +
                                        "' has no derivable C++ backing storage"};
        }

        return ir_.types.std.integer(requirement->bit_width, requirement->signedness);
    }
    auto enum_bit_width_comment(LoweringContext& ctx, DeclarationHandle const schema) -> NodeId {
        auto const& enums{ctx.declarations.enums};
        auto const& domains{ctx.semantic.enum_domains()};
        auto const width{enums.bit_width[schema].has_value()
                             ? std::optional<std::uint8_t>{*enums.bit_width[schema]}
                             : domains.minimum_required_bits[domains.index(domains.find(schema))]};
        return line_comment(width ? std::format("Logical bit width: {}.", *width)
                                  : "Logical bit width depends on the enumerator expressions.");
    }
    auto enum_range_declaration(LoweringContext& ctx,
                                DeclarationHandle const schema,
                                CppTypeId const enum_type) -> Nodes {
        auto const& domains{ctx.semantic.enum_domains()};
        auto const domain{domains.find(schema)};
        auto const values{domains.values(domain)};
        auto const count{values.size()};
        std::optional<PackedIntegerValue> minimum;
        std::optional<PackedIntegerValue> maximum;
        EnumeratorId first{};
        EnumeratorId last{};
        for (std::size_t ordinal{}; ordinal < count; ++ordinal) {
            auto const value{values[ordinal]};
            if (value == domains.count_value[domain]) {
                continue;
            }
            auto const code{domains.code(domain, ordinal)};
            if (!code) {
                return {};
            }
            auto const number{PackedIntegerValue::from_parts(code->negative, code->magnitude)};
            if (!minimum || packed_integer_less(number, *minimum)) {
                minimum = number;
                first = value;
            }
            if (!maximum || packed_integer_less(*maximum, number)) {
                maximum = number;
                last = value;
            }
        }
        if (!minimum) {
            return {};
        }

        auto const range_type{ir_.types.template_type(
            ir_.types.named("ioj::MinMax",
                            NameOrigin::global,
                            {ir_.types.dependency("ioj::MinMax", "ioj/min_max.h")}),
            {enum_type})};
        auto enumerator_value = [&](EnumeratorId const value) {
            return type_reference(
                ir_.types.named(std::string{ir_.types.spelling(enum_type)} +
                                    "::" + ctx.text(ctx.declarations.enumerators.name[value]),
                                NameOrigin::global));
        };
        auto const variable_id{
            variable(range_type,
                     template_id(named("enum_range"), {type_reference(enum_type)}),
                     init_list({enumerator_value(first), enumerator_value(last)}),
                     {.template_declaration = template_specialization(),
                      .is_inline = true,
                      .is_constexpr = true})};
        return {namespace_node(ir_.strings.intern("ioj"), {variable_id})};
    }
    auto native_enum_declaration(LoweringContext& ctx, DeclarationHandle const schema) -> Nodes {
        auto const& enums{ctx.declarations.enums};
        auto const alias{ctx.text(enums.name[schema]) + "Underlying"};
        NodeListBuilder decls{ir_};
        decls.add(using_declaration(ir_.strings.intern(alias), enum_underlying_type(ctx, schema)),
                  2);
        decls.add(enum_bit_width_comment(ctx, schema), 1);
        decls.add(enumeration(
                      enums.name[schema], ir_.types.intern(alias), enum_values(ctx, schema, false)),
                  2);
        auto const helpers{ctx.declarations.items(enums.helpers[schema])};
        for (auto const helper : {NativeEnumHelper::to_string,
                                  NativeEnumHelper::to_display_string,
                                  NativeEnumHelper::to_serialized_string}) {
            if (std::ranges::contains(helpers, helper)) {
                auto const name{helper == NativeEnumHelper::to_string ? "to_string"
                                : helper == NativeEnumHelper::to_display_string
                                    ? "to_display_string"
                                    : "to_serialized_string"};
                decls.add(using_import(ir_.types.named(
                              std::format("ioj::{}", name),
                              {ir_.types.dependency("ioj::enum_strings", "ioj/enum_strings.h")})),
                          1);
            }
        }
        return decls.build();
    }
    auto native_enum_parsers(LoweringContext& ctx, DeclarationHandle const schema) -> Nodes {
        auto const& enums{ctx.declarations.enums};
        auto const& enumerators{ctx.declarations.enumerators};
        auto const name{ctx.text(enums.name[schema])};
        auto const snake_name{snake_case_type_name(name)};
        auto const helpers{ctx.declarations.items(enums.helpers[schema])};
        auto const& domains{ctx.semantic.enum_domains()};
        auto const domain{domains.find(schema)};
        NodeListBuilder parsers{ir_};
        for (auto const helper :
             {NativeEnumHelper::try_parse, NativeEnumHelper::try_parse_serialized}) {
            if (!std::ranges::contains(helpers, helper)) {
                continue;
            }

            std::vector<StringEnumEntry> entries;
            bool indexed{true};
            std::size_t ordinal{};
            for (auto const value : ctx.declarations.items(enums.values[schema])) {
                auto const code{domains.code(domain, ordinal++)};
                auto const value_name{ctx.text(enumerators.name[value])};
                std::string key;
                if (helper == NativeEnumHelper::try_parse_serialized) {
                    auto const serialized{enumerators.serialized_name[value]};
                    if (!serialized) {
                        continue;
                    }
                    key = ctx.text(*serialized);
                } else {
                    key = value_name;
                }
                indexed = indexed && code && !code->negative && code->magnitude == entries.size();
                entries.push_back({std::move(key),
                                   type_reference(ir_.types.decorate(
                                       ir_.types.named(name), "", "::" + value_name))});
            }

            auto const prefix{helper == NativeEnumHelper::try_parse ? "try_parse_"
                                                                    : "try_parse_serialized_"};
            parsers.add(
                lower_string_enum_parser(ir_, prefix + snake_name, name, "value", entries, indexed),
                2);
        }
        return parsers.build();
    }
    auto native_enum_traits(LoweringContext& ctx,
                            DeclarationHandle schema,
                            CppTypeId const enum_type) -> Nodes {
        auto const& enums{ctx.declarations.enums};
        auto const value_count{enums.values[schema].size() - (enums.count[schema] ? 1 : 0)};
        NodeListBuilder result{ir_};
        if (std::ranges::contains(ctx.declarations.items(enums.helpers[schema]),
                                  NativeEnumHelper::traits)) {
            std::vector<Expr> values;
            values.reserve(value_count);
            for (auto const value : ctx.declarations.items(enums.values[schema])) {
                auto const name{ctx.declarations.enumerators.name[value]};
                if (enums.count[schema] && name == *enums.count[schema]) {
                    continue;
                }
                values.push_back(type_reference(ir_.types.named(
                    std::string{ir_.types.spelling(enum_type)} + "::" + ctx.text(name),
                    NameOrigin::global)));
            }
            auto const traits{
                record(template_id(named("EnumTraits"), {type_reference(enum_type)}),
                       {constexpr_member(
                           ir_.types.std.array, ir_.strings.intern("values"), init_list(values))})};
            set_record_template(traits, template_specialization());
            result.add(traits, 2);
        }

        if (enums.enum_array[schema]) {
            result.add(enum_count_specialization(enum_type, value_count));
        }
        return result.build();
    }
    auto native_enum_formatter(LoweringContext& ctx,
                               ModuleSettingsId settings,
                               DeclarationHandle schema) -> NodeId {
        auto const enum_type{
            ir_.types.named(native_enum_name(ctx, settings, schema), NameOrigin::global)};
        auto const formatter{
            record(template_id(named("formatter"), {type_reference(enum_type)}), {})};
        set_record_template(formatter, template_specialization());
        set_record_bases(formatter,
                         {ir_.types.template_type(
                             ir_.types.named("ioj::EnumFormatter",
                                             NameOrigin::global,
                                             {ir_.types.dependency("ioj::EnumFormatter",
                                                                   "ioj/enum_formatter.h")}),
                             {enum_type})});
        return namespace_node(ir_.strings.intern("std"), {formatter});
    }
    auto unreal_underlying_type(std::string_view const spelling) -> std::string {
        for (auto const& type : fixed_width_integer_types) {
            if (spelling == type.native_name) {
                return std::string{type.name};
            }
        }
        return std::string{spelling};
    }
    auto unreal_projection_header(LoweringContext& ctx,
                                  DeclarationHandle const schema,
                                  EnumUnrealProjectionId const projection) -> Nodes {
        auto const& projections{ctx.declarations.enum_unreal_projections};
        auto const name{ctx.text(projections.name[projection])};
        auto const underlying{ir_.types.intern(
            unreal_underlying_type(ir_.types.spelling(enum_underlying_type(ctx, schema))))};
        auto const& enums{ctx.declarations.enums};

        NodeListBuilder decls{ir_};
        decls.add(enum_bit_width_comment(ctx, schema), 1);
        decls.add(reflection_annotation(projections.reflection[projection]), 1);
        decls.add(
            enumeration(projections.name[projection], underlying, enum_values(ctx, schema, true)),
            2);
        decls.add(using_declaration(ir_.strings.intern(name + "Underlying"), underlying), 2);
        decls.append(enum_range_declaration(ctx, schema, ir_.types.named("::" + name)));
        decls.new_lines(2);
        if (projections.enum_array[projection]) {
            decls.add(namespace_node(ir_.strings.intern("ioj"),
                                     {enum_count_specialization(ir_.types.named("::" + name),
                                                                enums.values[schema].size())}));
        }
        return decls.build();
    }
    auto native_conversion_function(LoweringContext& ctx,
                                    std::string_view const function_name,
                                    std::string const& source,
                                    std::string const& target,
                                    std::span<EnumeratorId const> const values) -> NodeId {
        auto const& enumerators{ctx.declarations.enumerators};
        std::vector<SwitchCaseId> cases;
        cases.reserve(values.size());
        for (auto const value : values) {
            auto const name{ctx.text(enumerators.name[value])};
            cases.push_back(switch_case(
                type_reference(ir_.types.decorate(ir_.types.named(source), "", "::" + name)),
                block({return_stmt(type_reference(
                    ir_.types.decorate(ir_.types.named(target), "", "::" + name)))})));
        }

        auto const target_type{ir_.types.named(target)};
        auto const function_id{function(function_name, "auto")};
        set_function_return_type(function_id, target_type, ReturnSyntax::trailing);
        set_function_parameters(
            function_id,
            {parameter(ir_.types.decorate(ir_.types.named(source), "", " const"), "value")});
        auto& flags{function_flags(function_id)};
        flags.set_define_in_header(true);
        flags.set_is_constexpr(true);
        flags.set_is_nodiscard(true);
        set_function_noexcept(function_id);
        NodeListBuilder body{ir_};
        body.add(switch_stmt(named("value"), cases), 2);
        body.add(return_stmt(static_cast_expr(target_type, named("value"))));
        set_function_body(function_id, body.build());
        return header_function(function_id);
    }
    auto unreal_conversion_header(LoweringContext& ctx,
                                  ModuleSettingsId const settings,
                                  DeclarationHandle const schema,
                                  EnumUnrealProjectionId const projection) -> NodeId {
        auto const native_name{native_enum_name(ctx, settings, schema)};
        auto const& projections{ctx.declarations.enum_unreal_projections};
        auto const projection_name{ctx.text(projections.name[projection])};
        auto const& enums{ctx.declarations.enums};
        auto const values{ctx.declarations.items(enums.values[schema])};

        NodeListBuilder decls{ir_};
        decls.add(
            native_conversion_function(ctx, "to_native", projection_name, native_name, values), 2);
        decls.add(
            native_conversion_function(ctx, "to_unreal", native_name, projection_name, values), 2);
        auto const& enumerators{ctx.declarations.enumerators};
        for (auto const value : values) {
            auto const name{ctx.text(enumerators.name[value])};
            decls.add(static_assertion(binary(
                          BinaryOperator::equal,
                          static_cast_expr(ir_.types.std.int_,
                                           type_reference(ir_.types.decorate(
                                               ir_.types.named(projection_name), "", "::" + name))),
                          static_cast_expr(ir_.types.std.int_,
                                           type_reference(ir_.types.decorate(
                                               ir_.types.named(native_name), "", "::" + name))))),
                      1);
        }
        return namespace_node(ir_.strings.intern("ioj"), decls.build());
    }
};

} // namespace

auto enum_count_specialization(CppIR& ir, CppTypeId enum_type, std::size_t const count) -> NodeId {
    return EnumLowering{ir}.enum_count_specialization(enum_type, count);
}
auto enum_strings_specialization(CppIR& ir,
                                 std::string_view qualified,
                                 std::string_view category,
                                 std::span<std::string const> names,
                                 bool wide) -> NodeId {
    return EnumLowering{ir}.enum_strings_specialization(qualified, category, names, wide);
}
auto lower_enum(LoweringContext& ctx,
                DeclarationHandle schema,
                ModuleSettingsId settings,
                std::optional<StringId> const helper_namespace) -> DeclarationEmission {
    return EnumLowering{ctx.ir}.lower_enum(ctx, schema, settings, helper_namespace);
}

} // namespace lispb::codegen::detail
