#include "lowering.h"
#include "lowering_utils.h"

#include <algorithm>
#include <cctype>
#include <string_view>
#include <utility>

namespace codegen::detail {
namespace {

auto escaped_string(std::string_view const value) -> std::string {
    std::string result;
    result.reserve(value.size());
    for (auto const character : value) {
        switch (character) {
            case '\\':
                result += "\\\\";
                break;
            case '"':
                result += "\\\"";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\t':
                result += "\\t";
                break;
            default:
                result += character;
                break;
        }
    }
    return result;
}

auto text_literal(std::string_view const value) -> std::string {
    return "TEXT(\"" + escaped_string(value) + "\")";
}

auto qualified_enum_name(EnumModuleSchema const& module, EnumSchema const& schema) -> std::string {
    if (module.settings.namespace_name.has_value()) {
        return *module.settings.namespace_name + "::" + schema.name;
    }
    return schema.name;
}

auto snake_case_type_name(std::string_view const name) -> std::string {
    auto const begin{name.size() > 1 && name.front() == 'E' &&
                             std::isupper(static_cast<unsigned char>(name[1])) != 0
                         ? std::size_t{1}
                         : std::size_t{0}};
    std::string result;
    for (auto index{begin}; index < name.size(); ++index) {
        auto const character{static_cast<unsigned char>(name[index])};
        auto const upper{std::isupper(character) != 0};
        auto const previous_lower{index > begin &&
                                  std::islower(static_cast<unsigned char>(name[index - 1])) != 0};
        auto const next_lower{index + 1 < name.size() &&
                              std::islower(static_cast<unsigned char>(name[index + 1])) != 0};
        if (upper && !result.empty() && (previous_lower || next_lower)) {
            result += '_';
        }
        result += static_cast<char>(std::tolower(character));
    }
    return result;
}

auto internal_name(EnumSchema const& schema, std::string_view const suffix) -> std::string {
    return "get_" + snake_case_type_name(schema.name) + "_" + std::string{suffix};
}

auto enum_traits(EnumModuleSchema const& module, EnumSchema const& schema) -> Node {
    return raw("template <>\n"
               "struct TEnumTraits<" + qualified_enum_name(module, schema) + "> {\n"
               "    static constexpr int32 count{" + std::to_string(schema.values.size()) + "};\n"
               "};");
}

auto exact_lookup(EnumModuleSchema const& module, EnumSchema const& schema) -> FunctionSpec {
    auto const enum_name{qualified_enum_name(module, schema)};
    NodeListBuilder body;
    std::vector<std::string> cases;
    for (auto const& value : schema.values) {
        cases.push_back("case " + enum_name + "::" + value.name + ": {\n"
                        "    return " + text_literal(value.name) + ";\n"
                        "}");
    }
    body.add(raw("switch (value) {\n" + join(cases, "\n") + "\n}"), 2)
        .add(raw("ensureMsgf(false,\n"
                 "           TEXT(\"Unhandled " + schema.name + " value: %lld\"),\n"
                 "           static_cast<int64>(value));"),
             1)
        .add(ReturnStatement{text_literal("<invalid " + schema.name + ">")});
    return FunctionSpec{
        .name = internal_name(schema, "name"),
        .return_type = "auto",
        .parameters = {FunctionParameter{CppType{enum_name + " const"}, "value"}},
        .body = body.build(),
        .qualifiers = {.trailing_return_type = CppType{"TCHAR const*", "CoreMinimal.h"}},
    };
}

auto display_lookup(EnumModuleSchema const& module, EnumSchema const& schema) -> FunctionSpec {
    auto const enum_name{qualified_enum_name(module, schema)};
    std::vector<std::string> cases;
    for (auto const& value : schema.values) {
        if (!value.display_name.has_value()) {
            continue;
        }
        cases.push_back("case " + enum_name + "::" + value.name + ": {\n"
                        "    return " + text_literal(*value.display_name) + ";\n"
                        "}");
    }
    NodeListBuilder body;
    if (!cases.empty()) {
        body.add(raw("switch (value) {\n" + join(cases, "\n") +
                     "\ndefault: {\n    break;\n}\n}"),
                 2);
    }
    body.add(ReturnStatement{internal_name(schema, "name") + "(value)"});
    return FunctionSpec{
        .name = internal_name(schema, "display_name"),
        .return_type = "auto",
        .parameters = {FunctionParameter{CppType{enum_name + " const"}, "value"}},
        .body = body.build(),
        .qualifiers = {.trailing_return_type = CppType{"TCHAR const*", "CoreMinimal.h"}},
    };
}

auto conversion_spec(EnumModuleSchema const& module,
                     EnumSchema const& schema,
                     EnumConversion const conversion) -> FunctionSpec {
    auto const enum_name{qualified_enum_name(module, schema)};
    auto const display{conversion == EnumConversion::lex_to_display_string ||
                       conversion == EnumConversion::display_string_view ||
                       conversion == EnumConversion::display_string};
    auto const lexical{conversion == EnumConversion::lex_to_string ||
                       conversion == EnumConversion::lex_to_display_string};
    auto const helper_in_type_namespace{
        module.helper_namespace.value_or(module.settings.namespace_name.value_or("")) ==
        module.settings.namespace_name.value_or("")};
    auto const parameter_type{lexical || helper_in_type_namespace ? schema.name : enum_name};
    auto const lookup{internal_name(schema, display ? "display_name" : "name") + "(value)"};
    FunctionSpec result{
        .return_type = "auto",
        .parameters = {FunctionParameter{CppType{parameter_type + " const"}, "value"}},
        .body = {ReturnStatement{lookup}},
        .export_specifier = schema.export_specifier,
    };
    switch (conversion) {
        case EnumConversion::lex_to_string:
            result.name = "LexToString";
            result.qualifiers.trailing_return_type = CppType{"TCHAR const*", "CoreMinimal.h"};
            break;
        case EnumConversion::string_view:
            result.name = "to_string_view";
            result.qualifiers.trailing_return_type = CppType{"FStringView", "CoreMinimal.h"};
            result.body = {ReturnStatement{"FStringView{" + lookup + "}"}};
            break;
        case EnumConversion::string:
            result.name = "to_string";
            result.qualifiers.trailing_return_type = CppType{"FString", "CoreMinimal.h"};
            result.body = {ReturnStatement{"FString{" + lookup + "}"}};
            break;
        case EnumConversion::lex_to_display_string:
            result.name = "LexToDisplayString";
            result.qualifiers.trailing_return_type = CppType{"TCHAR const*", "CoreMinimal.h"};
            break;
        case EnumConversion::display_string_view:
            result.name = "to_display_string_view";
            result.qualifiers.trailing_return_type = CppType{"FStringView", "CoreMinimal.h"};
            result.body = {ReturnStatement{"FStringView{" + lookup + "}"}};
            break;
        case EnumConversion::display_string:
            result.name = "to_display_string";
            result.qualifiers.trailing_return_type = CppType{"FString", "CoreMinimal.h"};
            result.body = {ReturnStatement{"FString{" + lookup + "}"}};
            break;
    }
    return result;
}

auto annotation(EnumeratorSchema const& value, bool const reflected)
    -> std::optional<std::string> {
    if (!reflected || (!value.display_name.has_value() && !value.hidden)) {
        return std::nullopt;
    }
    std::vector<std::string> metadata;
    if (value.display_name.has_value()) {
        metadata.push_back("DisplayName = \"" + escaped_string(*value.display_name) + "\"");
    }
    if (value.hidden) {
        metadata.emplace_back("Hidden");
    }
    return "UMETA(" + join(metadata, ", ") + ")";
}

auto wrapped(std::optional<std::string> const& namespace_name, Nodes nodes) -> Nodes {
    if (!namespace_name.has_value() || nodes.empty()) {
        return nodes;
    }
    return {Namespace{*namespace_name, std::move(nodes)}};
}

} // namespace

auto lower_enum_module(EnumModuleSchema const& module,
                       std::map<std::string, CppType> const& types) -> Module {
    NodeListBuilder declarations;
    NodeListBuilder traits;
    bool has_reflected{};
    bool has_enum_arrays{};
    for (auto const& schema : module.enums) {
        auto const reflected{schema.reflection != EnumReflection::none};
        has_reflected = has_reflected || reflected;
        has_enum_arrays = has_enum_arrays || schema.enum_array;
        if (reflected) {
            declarations.add(raw(schema.reflection == EnumReflection::blueprint
                                     ? "UENUM(BlueprintType)"
                                     : "UENUM()",
                                 {TypeDependency{"UENUM", "CoreMinimal.h", {}}}),
                             1);
        }
        std::vector<Enumerator> values;
        for (auto const& value : schema.values) {
            values.push_back(Enumerator{
                .name = value.name,
                .initializer = value.initializer,
                .annotation = annotation(value, reflected),
            });
        }
        declarations.add(Enum{
                             .name = schema.name,
                             .underlying_type = resolve_type(schema.underlying_type, types),
                             .values = std::move(values),
                         },
                         2);
        if (schema.enum_array) {
            traits.add(enum_traits(module, schema), 2);
        }
    }

    NodeListBuilder lex_declarations;
    NodeListBuilder helper_declarations;
    for (auto const& schema : module.enums) {
        for (auto const conversion : schema.conversions) {
            auto spec{conversion_spec(module, schema, conversion)};
            auto const lexical{conversion == EnumConversion::lex_to_string ||
                               conversion == EnumConversion::lex_to_display_string};
            (lexical ? lex_declarations : helper_declarations).add(declaration(std::move(spec)), 2);
        }
    }

    NodeListBuilder header_nodes;
    header_nodes.add(IncludeDependencies{}, 2);
    if (has_enum_arrays) {
        header_nodes.add(Include{"SandboxGameShared/utilities/enum_array.h", false}, 2);
    }
    if (has_reflected) {
        header_nodes.add(Include{module.settings.header.stem().string() + ".generated.h", false}, 2);
    }
    if (!module.settings.prelude_lines.empty()) {
        header_nodes.add(raw(join_lines(module.settings.prelude_lines)), 2);
    }
    header_nodes.append(wrapped(module.settings.namespace_name, declarations.build()))
        .append(traits.build())
        .append(wrapped(module.settings.namespace_name, lex_declarations.build()))
        .append(wrapped(module.helper_namespace.has_value() ? module.helper_namespace
                                                            : module.settings.namespace_name,
                        helper_declarations.build()));

    Module result{
        .name = module.settings.name,
        .header = CppFile{
            .path = module.settings.header,
            .nodes = header_nodes.build(),
            .clang_format_off = true,
            .include_order = module.settings.include_order,
        },
    };
    if (!module.settings.source.has_value()) {
        return result;
    }

    NodeListBuilder internal_definitions;
    NodeListBuilder lex_definitions;
    NodeListBuilder helper_definitions;
    for (auto const& schema : module.enums) {
        if (schema.conversions.empty()) {
            continue;
        }
        internal_definitions.add(Function{exact_lookup(module, schema), std::nullopt, false, false},
                                 2)
            .add(Function{display_lookup(module, schema), std::nullopt, false, false}, 2);
        for (auto const conversion : schema.conversions) {
            auto spec{conversion_spec(module, schema, conversion)};
            auto const lexical{conversion == EnumConversion::lex_to_string ||
                               conversion == EnumConversion::lex_to_display_string};
            (lexical ? lex_definitions : helper_definitions)
                .add(Function{std::move(spec), std::nullopt, false, false}, 2);
        }
    }
    NodeListBuilder source_nodes;
    source_nodes.add(Include{source_include(module.settings), false}, 2)
        .add(Namespace{"", internal_definitions.build()}, 2)
        .append(wrapped(module.settings.namespace_name, lex_definitions.build()))
        .append(wrapped(module.helper_namespace.has_value() ? module.helper_namespace
                                                            : module.settings.namespace_name,
                        helper_definitions.build()));
    result.source = CppFile{
        .path = *module.settings.source,
        .nodes = source_nodes.build(),
        .pragma_once = false,
        .clang_format_off = true,
        .include_order = module.settings.include_order,
    };
    return result;
}

} // namespace codegen::detail
