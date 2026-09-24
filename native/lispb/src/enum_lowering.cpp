#include "lowering.h"
#include "lowering_utils.h"

#include <lispb/schema/enum_domain.h>

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string_view>
#include <utility>

namespace codegen::detail {
namespace {

auto text_literal(std::string_view const value) -> Expr {
    return call(named("TEXT", {TypeDependency{"TEXT", "CoreMinimal.h", {}}}),
                {string_literal(value)});
}

auto qualified_enum_name(ModuleSettings const& settings, EnumSchema const& schema) -> std::string {
    if (settings.namespace_name.has_value()) {
        return *settings.namespace_name + "::" + schema.name;
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

auto enum_traits(ModuleSettings const& settings, EnumSchema const& schema) -> Node {
    auto const count{
        schema.count.has_value()
            ? static_cast_expr("int32",
                               named(qualified_enum_name(settings, schema) + "::" + *schema.count))
            : literal(std::to_string(schema.values.size()))};
    return Struct{
        .name = "TEnumTraits<" + qualified_enum_name(settings, schema) + ">",
        .children = {Member{"int32", "count", count, {.is_static = true, .is_constexpr = true}}},
        .template_parameters = "",
    };
}

auto exact_lookup(ModuleSettings const& settings, EnumSchema const& schema) -> FunctionSpec {
    auto const enum_name{qualified_enum_name(settings, schema)};
    NodeListBuilder body;
    std::vector<SwitchCase> cases;
    for (auto const& value : schema.values) {
        cases.push_back(
            {named(enum_name + "::" + value.name), Block{{ReturnStmt{text_literal(value.name)}}}});
    }
    body.add(SwitchStmt{named("value"), std::move(cases)}, 2)
        .add(raw("ensureMsgf(false,\n"
                 "           TEXT(\"Unhandled " +
                 schema.name +
                 " value: %lld\"),\n"
                 "           static_cast<int64>(value));"),
             1)
        .add(ReturnStmt{text_literal("<invalid " + schema.name + ">")});
    return FunctionSpec{
        .name = internal_name(schema, "name"),
        .return_type = "auto",
        .parameters = {FunctionParameter{CppType{enum_name + " const"}, "value"}},
        .body = body.build(),
        .qualifiers = {.trailing_return_type = CppType{"TCHAR const*", "CoreMinimal.h"}},
    };
}

auto display_lookup(ModuleSettings const& settings, EnumSchema const& schema) -> FunctionSpec {
    auto const enum_name{qualified_enum_name(settings, schema)};
    std::vector<SwitchCase> cases;
    for (auto const& value : schema.values) {
        if (!value.display_name.has_value()) {
            continue;
        }
        cases.push_back({named(enum_name + "::" + value.name),
                         Block{{ReturnStmt{text_literal(*value.display_name)}}}});
    }
    NodeListBuilder body;
    if (!cases.empty()) {
        cases.push_back({std::nullopt, Block{{BreakStmt{}}}});
        body.add(SwitchStmt{named("value"), std::move(cases)}, 2);
    }
    body.add(ReturnStmt{call(named(internal_name(schema, "name")), {named("value")})});
    return FunctionSpec{
        .name = internal_name(schema, "display_name"),
        .return_type = "auto",
        .parameters = {FunctionParameter{CppType{enum_name + " const"}, "value"}},
        .body = body.build(),
        .qualifiers = {.trailing_return_type = CppType{"TCHAR const*", "CoreMinimal.h"}},
    };
}

auto serialized_lookup(ModuleSettings const& settings, EnumSchema const& schema) -> FunctionSpec {
    auto const enum_name{qualified_enum_name(settings, schema)};
    NodeListBuilder body;
    std::vector<SwitchCase> cases;
    for (auto const& value : schema.values) {
        if (!value.serialized_name.has_value()) {
            continue;
        }
        cases.push_back({named(enum_name + "::" + value.name),
                         Block{{ReturnStmt{text_literal(*value.serialized_name)}}}});
    }
    body.add(SwitchStmt{named("value"), std::move(cases)}, 2)
        .add(raw("ensureMsgf(false,\n"
                 "           TEXT(\"Unhandled serialized " +
                 schema.name +
                 " value: %lld\"),\n"
                 "           static_cast<int64>(value));"),
             1)
        .add(ReturnStmt{text_literal("<invalid " + schema.name + ">")});
    return FunctionSpec{
        .name = internal_name(schema, "serialized_name"),
        .return_type = "auto",
        .parameters = {FunctionParameter{CppType{enum_name + " const"}, "value"}},
        .body = body.build(),
        .qualifiers = {.trailing_return_type = CppType{"TCHAR const*", "CoreMinimal.h"}},
    };
}

auto serialized_parser(ModuleSettings const& settings, EnumSchema const& schema) -> FunctionSpec {
    auto const enum_name{qualified_enum_name(settings, schema)};
    NodeListBuilder body;
    for (auto const& value : schema.values) {
        if (!value.serialized_name.has_value()) {
            continue;
        }
        body.add(
            IfStmt{
                binary(BinaryOperator::equal, named("value"), text_literal(*value.serialized_name)),
                Block{{AssignmentStmt{named("result"), named(enum_name + "::" + value.name)},
                       ReturnStmt{literal("true")}}}},
            1);
    }
    body.add(ReturnStmt{literal("false")});
    return FunctionSpec{
        .name = "try_parse_serialized",
        .return_type = "auto",
        .parameters =
            {
                FunctionParameter{CppType{"FStringView const", "CoreMinimal.h"}, "value"},
                FunctionParameter{CppType{enum_name + "&"}, "result"},
            },
        .body = body.build(),
        .qualifiers = {.trailing_return_type = CppType{"bool"}},
        .export_specifier = schema.export_specifier,
    };
}

auto conversion_spec(ModuleSettings const& settings,
                     EnumSchema const& schema,
                     EnumConversion const conversion,
                     std::optional<std::string> const& helper_namespace) -> FunctionSpec {
    if (conversion == EnumConversion::try_parse_serialized) {
        return serialized_parser(settings, schema);
    }
    auto const enum_name{qualified_enum_name(settings, schema)};
    auto const display{conversion == EnumConversion::lex_to_display_string ||
                       conversion == EnumConversion::display_string_view ||
                       conversion == EnumConversion::display_string};
    auto const serialized{conversion == EnumConversion::lex_to_serialized_string};
    auto const lexical{conversion == EnumConversion::lex_to_string ||
                       conversion == EnumConversion::lex_to_display_string || serialized};
    auto const helper_in_type_namespace{helper_namespace.value_or(settings.namespace_name.value_or(
                                            "")) == settings.namespace_name.value_or("")};
    auto const parameter_type{lexical || helper_in_type_namespace ? schema.name : enum_name};
    auto const lookup{
        call(named(internal_name(
                 schema, serialized ? "serialized_name" : (display ? "display_name" : "name"))),
             {named("value")})};
    FunctionSpec result{
        .return_type = "auto",
        .parameters = {FunctionParameter{CppType{parameter_type + " const"}, "value"}},
        .body = {ReturnStmt{lookup}},
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
            result.body = {
                ReturnStmt{init_list({lookup}, CppType{"FStringView", "CoreMinimal.h"})}};
            break;
        case EnumConversion::string:
            result.name = "to_string";
            result.qualifiers.trailing_return_type = CppType{"FString", "CoreMinimal.h"};
            result.body = {ReturnStmt{init_list({lookup}, CppType{"FString", "CoreMinimal.h"})}};
            break;
        case EnumConversion::lex_to_display_string:
            result.name = "LexToDisplayString";
            result.qualifiers.trailing_return_type = CppType{"TCHAR const*", "CoreMinimal.h"};
            break;
        case EnumConversion::display_string_view:
            result.name = "to_display_string_view";
            result.qualifiers.trailing_return_type = CppType{"FStringView", "CoreMinimal.h"};
            result.body = {
                ReturnStmt{init_list({lookup}, CppType{"FStringView", "CoreMinimal.h"})}};
            break;
        case EnumConversion::display_string:
            result.name = "to_display_string";
            result.qualifiers.trailing_return_type = CppType{"FString", "CoreMinimal.h"};
            result.body = {ReturnStmt{init_list({lookup}, CppType{"FString", "CoreMinimal.h"})}};
            break;
        case EnumConversion::lex_to_serialized_string:
            result.name = "LexToSerializedString";
            result.qualifiers.trailing_return_type = CppType{"TCHAR const*", "CoreMinimal.h"};
            break;
        case EnumConversion::try_parse_serialized:
            break;
    }
    return result;
}

auto annotation(EnumeratorSchema const& value, bool const reflected) -> std::optional<std::string> {
    if (!reflected || (!value.display_name.has_value() && !value.hidden)) {
        return std::nullopt;
    }
    std::vector<std::string> metadata;
    if (value.display_name.has_value()) {
        metadata.push_back("DisplayName = " + render(string_literal(*value.display_name)));
    }
    if (value.hidden) {
        metadata.emplace_back("Hidden");
    }
    return "UMETA(" + join(metadata, ", ") + ")";
}

auto native_string_literal(std::string_view const value) -> std::string {
    return render(string_literal(value));
}

auto native_enum_name(ModuleSettings const& settings, EnumSchema const& schema) -> std::string {
    return settings.namespace_name.has_value()
             ? "::" + *settings.namespace_name + "::" + schema.name
             : "::" + schema.name;
}

auto enum_values(EnumSchema const& schema) -> std::vector<EnumeratorSchema const*> {
    std::vector<EnumeratorSchema const*> result;
    for (auto const& value : schema.values) {
        if (!schema.count.has_value() || value.name != *schema.count) {
            result.push_back(&value);
        }
    }
    return result;
}

auto enum_underlying_type(EnumSchema const& schema, TypeRegistry const& types) -> CppType {
    if (schema.underlying_type.has_value()) {
        return resolve_type(*schema.underlying_type, types);
    }

    auto const domain{lispb::schema::analyze_enum_domain(schema)};
    auto const requirement{
        lispb::schema::derive_enum_storage_requirement(domain, schema.bit_width)};
    if (!requirement.has_value()) {
        throw std::invalid_argument{"Enum '" + schema.name +
                                    "' has no derivable C++ backing storage"};
    }

    auto const prefix{requirement->signedness ? "int" : "uint"};
    auto const spelling{schema.native_api
                            ? "std::" + std::string{prefix} +
                                  std::to_string(requirement->bit_width) + "_t"
                            : std::string{prefix} + std::to_string(requirement->bit_width)};
    return CppType{spelling, schema.native_api ? "cstdint" : "CoreMinimal.h"};
}

auto native_enum_declaration(EnumSchema const& schema, TypeRegistry const& types) -> std::string {
    auto const underlying{enum_underlying_type(schema, types).spelling};
    std::ostringstream output;
    output << "enum class " << schema.name << " : " << underlying << " {\n";
    for (auto const& value : schema.values) {
        output << "    " << value.name;
        if (value.initializer.has_value()) {
            output << " = " << *value.initializer;
        }
        output << ",\n";
    }
    output << "};\n\n";

    auto const snake_name{snake_case_type_name(schema.name)};

    output << "[[nodiscard]] constexpr auto to_string(" << schema.name
           << " const value) noexcept -> std::string_view {\n";
    output << "    switch (value) {\n";
    for (auto const& value : schema.values) {
        output << "        case " << schema.name << "::" << value.name << ": {\n";
        output << "            return " << native_string_literal(value.name) << ";\n";
        output << "        }\n";
    }
    output << "    }\n\n";
    output << "    return " << native_string_literal("<invalid " + schema.name + ">") << ";\n";
    output << "}\n\n";

    output << "[[nodiscard]] constexpr auto try_parse_" << snake_name
           << "(std::string_view const value) noexcept -> std::optional<" << schema.name << "> {\n";
    for (auto const& value : schema.values) {
        output << "    if (value == " << native_string_literal(value.name) << ") {\n";
        output << "        return " << schema.name << "::" << value.name << ";\n";
        output << "    }\n";
    }
    output << "\n    return std::nullopt;\n}\n";

    auto const has_display{std::ranges::any_of(
        schema.values, [](auto const& value) { return value.display_name.has_value(); })};
    if (has_display) {
        output << "\n[[nodiscard]] constexpr auto to_display_string(" << schema.name
               << " const value) noexcept -> std::string_view {\n";
        output << "    switch (value) {\n";
        for (auto const& value : schema.values) {
            output << "        case " << schema.name << "::" << value.name << ": {\n";
            output << "            return "
                   << native_string_literal(value.display_name.value_or(value.name)) << ";\n";
            output << "        }\n";
        }
        output << "    }\n\n";
        output << "    return " << native_string_literal("<invalid " + schema.name + ">")
               << ";\n}\n";
    }

    auto const values{enum_values(schema)};
    auto const has_serialized{std::ranges::all_of(
        values, [](auto const* value) { return value->serialized_name.has_value(); })};
    if (has_serialized) {
        output << "\n[[nodiscard]] constexpr auto to_serialized_string(" << schema.name
               << " const value) noexcept -> std::string_view {\n";
        output << "    switch (value) {\n";
        for (auto const& value : schema.values) {
            if (!value.serialized_name.has_value()) {
                continue;
            }
            output << "        case " << schema.name << "::" << value.name << ": {\n";
            output << "            return " << native_string_literal(*value.serialized_name)
                   << ";\n";
            output << "        }\n";
        }
        output << "        default: {\n";
        output << "            return " << native_string_literal("<invalid " + schema.name + ">")
               << ";\n";
        output << "        }\n";
        output << "    }\n\n";
        output << "}\n\n";
        output << "[[nodiscard]] constexpr auto try_parse_serialized_" << snake_name
               << "(std::string_view const value) noexcept -> std::optional<" << schema.name
               << "> {\n";
        for (auto const& value : schema.values) {
            if (!value.serialized_name.has_value()) {
                continue;
            }
            output << "    if (value == " << native_string_literal(*value.serialized_name)
                   << ") {\n";
            output << "        return " << schema.name << "::" << value.name << ";\n";
            output << "    }\n";
        }
        output << "\n    return std::nullopt;\n}\n";
    }
    return output.str();
}

auto native_enum_traits(ModuleSettings const& settings, EnumSchema const& schema) -> std::string {
    auto const qualified{native_enum_name(settings, schema)};
    auto const values{enum_values(schema)};
    std::ostringstream output;
    output << "template <>\nstruct EnumTraits<" << qualified << "> {\n";
    output << "    inline static constexpr std::array values{";
    for (auto const* value : values) {
        output << qualified << "::" << value->name << ", ";
    }
    output << "};\n";
    output << "    inline static constexpr std::array names{";
    for (auto const* value : values) {
        output << "std::string_view{" << native_string_literal(value->name) << "}, ";
    }
    output << "};\n";
    output << "    inline static constexpr std::size_t count{values.size()};\n";
    output << "};\n";
    return output.str();
}

auto unreal_underlying_type(CppType const& native_type) -> std::string {
    if (native_type.spelling == "std::uint8_t") {
        return "uint8";
    }
    if (native_type.spelling == "std::uint16_t") {
        return "uint16";
    }
    if (native_type.spelling == "std::uint32_t") {
        return "uint32";
    }
    if (native_type.spelling == "std::uint64_t") {
        return "uint64";
    }
    if (native_type.spelling == "std::int8_t") {
        return "int8";
    }
    if (native_type.spelling == "std::int16_t") {
        return "int16";
    }
    if (native_type.spelling == "std::int32_t") {
        return "int32";
    }
    if (native_type.spelling == "std::int64_t") {
        return "int64";
    }
    return native_type.spelling;
}

auto unreal_projection_header(EnumSchema const& schema,
                              EnumUnrealProjection const& projection,
                              TypeRegistry const& types) -> std::string {
    std::ostringstream output;
    output << (projection.reflection == EnumReflection::blueprint ? "UENUM(BlueprintType)"
                                                                  : "UENUM()")
           << "\nenum class " << projection.name << " : "
           << unreal_underlying_type(enum_underlying_type(schema, types)) << " {\n";
    for (auto const& value : schema.values) {
        output << "    " << value.name;
        if (value.initializer.has_value()) {
            output << " = " << *value.initializer;
        }
        if (value.display_name.has_value() || value.hidden) {
            output << " UMETA(";
            if (value.display_name.has_value()) {
                output << "DisplayName = " << native_string_literal(*value.display_name);
            }
            if (value.display_name.has_value() && value.hidden) {
                output << ", ";
            }
            if (value.hidden) {
                output << "Hidden";
            }
            output << ")";
        }
        output << ",\n";
    }
    output << "};";
    return output.str();
}

auto unreal_conversion_header(ModuleSettings const& settings,
                              EnumSchema const& schema,
                              EnumUnrealProjection const& projection) -> std::string {
    auto const native_name{native_enum_name(settings, schema)};
    std::ostringstream output;
    output << "namespace ml {\n"
           << "[[nodiscard]] constexpr auto to_native(" << projection.name
           << " const value) noexcept -> " << native_name << " {\n"
           << "    switch (value) {\n";
    for (auto const& value : schema.values) {
        output << "        case " << projection.name << "::" << value.name << ": {\n"
               << "            return " << native_name << "::" << value.name << ";\n"
               << "        }\n";
    }
    output << "    }\n\n"
           << "    return static_cast<" << native_name << ">(value);\n"
           << "}\n\n"
           << "[[nodiscard]] constexpr auto to_unreal(" << native_name
           << " const value) noexcept -> " << projection.name << " {\n"
           << "    switch (value) {\n";
    for (auto const& value : schema.values) {
        output << "        case " << native_name << "::" << value.name << ": {\n"
               << "            return " << projection.name << "::" << value.name << ";\n"
               << "        }\n";
    }
    output << "    }\n\n"
           << "    return static_cast<" << projection.name << ">(value);\n"
           << "}\n\n";
    for (auto const& value : schema.values) {
        output << "static_assert(static_cast<int>(" << projection.name << "::" << value.name
               << ") == static_cast<int>(" << native_name << "::" << value.name << "));\n";
    }
    output << "} // namespace ml\n";
    return output.str();
}

} // namespace

auto lower_enum(EnumSchema const& schema,
                ModuleSettings const& settings,
                std::optional<std::string> const& helper_namespace,
                TypeRegistry const& types) -> DeclarationEmission {
    DeclarationEmission emission;
    emission.source_dependencies = false;
    if (schema.native_api) {
        NodeListBuilder prefix;
        prefix.add(Include{"array", true}, 2)
            .add(Include{"cstdint", true}, 2)
            .add(Include{"cstddef", true}, 2)
            .add(Include{"optional", true}, 2)
            .add(Include{"string_view", true}, 2)
            .add(Include{"sandbox/core/enum_traits.h", false}, 2);
        emission.header_prefix = prefix.build();
        emission.header = {raw(native_enum_declaration(schema, types))};
        emission.header_global = {Namespace{"ml", {raw(native_enum_traits(settings, schema))}}};
        if (schema.unreal_projection.has_value()) {
            auto const& projection{*schema.unreal_projection};
            auto const generated_header{projection.header.stem().string() + ".generated.h"};
            emission.additional_modules.push_back(Module{
                .name = settings.name + "_" + projection.name,
                .header =
                    CppFile{.path = projection.header,
                            .nodes = {Include{"CoreMinimal.h", false},
                                      Include{generated_header, false},
                                      raw(unreal_projection_header(schema, projection, types))},
                            .clang_format_off = true},
            });
            emission.additional_modules.push_back(Module{
                .name = settings.name + "_" + projection.name + "_conversion",
                .header =
                    CppFile{.path = projection.conversion_header,
                            .nodes = {Include{projection.native_header_include, false},
                                      Include{projection.header_include, false},
                                      raw(unreal_conversion_header(settings, schema, projection))},
                            .clang_format_off = true},
            });
        }
        return emission;
    }

    NodeListBuilder declarations;
    if (schema.reflection != EnumReflection::none) {
        declarations.add(
            raw(schema.reflection == EnumReflection::blueprint ? "UENUM(BlueprintType)" : "UENUM()",
                {TypeDependency{"UENUM", "CoreMinimal.h", {}}}),
            1);
        emission.header_generated_include = {
            Include{settings.header.stem().string() + ".generated.h", false}};
    }
    std::vector<Enumerator> values;
    for (auto const& value : schema.values) {
        values.push_back(
            Enumerator{.name = value.name,
                       .initializer = value.initializer,
                       .annotation = annotation(value, schema.reflection != EnumReflection::none)});
    }
    declarations.add(Enum{.name = schema.name,
                          .underlying_type = enum_underlying_type(schema, types),
                          .values = std::move(values)},
                     2);
    if (schema.enum_array) {
        emission.header_prefix.push_back(Include{"SandboxCore/enum_array.h", false});
        emission.header_global.push_back(enum_traits(settings, schema));
    }

    NodeListBuilder lex_declarations;
    NodeListBuilder helper_declarations;
    for (auto const conversion : schema.conversions) {
        auto spec{conversion_spec(settings, schema, conversion, helper_namespace)};
        auto const lexical{conversion == EnumConversion::lex_to_string ||
                           conversion == EnumConversion::lex_to_display_string ||
                           conversion == EnumConversion::lex_to_serialized_string};
        (lexical ? lex_declarations : helper_declarations).add(declaration(std::move(spec)), 2);
    }
    emission.header = declarations.build();
    emission.header_after = lex_declarations.build();
    auto helper_nodes{helper_declarations.build()};
    if (!helper_nodes.empty()) {
        emission.header_tail = std::move(helper_nodes);
        emission.tail_namespace = helper_namespace.value_or(settings.namespace_name.value_or(""));
    }

    if (!schema.conversions.empty()) {
        NodeListBuilder internal;
        NodeListBuilder lexical;
        NodeListBuilder helpers;
        auto const has_display{std::ranges::any_of(schema.conversions, [](auto const conversion) {
            return conversion == EnumConversion::lex_to_display_string ||
                   conversion == EnumConversion::display_string_view ||
                   conversion == EnumConversion::display_string;
        })};
        auto const has_name{has_display ||
                            std::ranges::any_of(schema.conversions, [](auto const conversion) {
                                return conversion == EnumConversion::lex_to_string ||
                                       conversion == EnumConversion::string_view ||
                                       conversion == EnumConversion::string;
                            })};
        if (has_name) {
            internal.add(Function{exact_lookup(settings, schema), std::nullopt, false, false}, 2);
        }
        if (has_display) {
            internal.add(Function{display_lookup(settings, schema), std::nullopt, false, false}, 2);
        }
        if (std::ranges::find(schema.conversions, EnumConversion::lex_to_serialized_string) !=
            schema.conversions.end()) {
            internal.add(Function{serialized_lookup(settings, schema), std::nullopt, false, false},
                         2);
        }
        for (auto const conversion : schema.conversions) {
            auto spec{conversion_spec(settings, schema, conversion, helper_namespace)};
            auto const is_lexical{conversion == EnumConversion::lex_to_string ||
                                  conversion == EnumConversion::lex_to_display_string ||
                                  conversion == EnumConversion::lex_to_serialized_string};
            (is_lexical ? lexical : helpers)
                .add(Function{std::move(spec), std::nullopt, false, false}, 2);
        }
        emission.source = lexical.build();
        emission.source_global = internal.build();
        emission.source_tail = helpers.build();
        emission.tail_namespace = helper_namespace.value_or(settings.namespace_name.value_or(""));
    }
    return emission;
}

} // namespace codegen::detail
