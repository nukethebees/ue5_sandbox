#include "lowering.h"
#include "lowering_utils.h"

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
    auto const count{
        schema.count.has_value()
            ? static_cast_expr("int32",
                               named(qualified_enum_name(module, schema) + "::" + *schema.count))
            : literal(std::to_string(schema.values.size()))};
    return Struct{
        .name = "TEnumTraits<" + qualified_enum_name(module, schema) + ">",
        .children = {Member{"int32", "count", count, {.is_static = true, .is_constexpr = true}}},
        .template_parameters = "",
    };
}

auto exact_lookup(EnumModuleSchema const& module, EnumSchema const& schema) -> FunctionSpec {
    auto const enum_name{qualified_enum_name(module, schema)};
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

auto display_lookup(EnumModuleSchema const& module, EnumSchema const& schema) -> FunctionSpec {
    auto const enum_name{qualified_enum_name(module, schema)};
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

auto serialized_lookup(EnumModuleSchema const& module, EnumSchema const& schema) -> FunctionSpec {
    auto const enum_name{qualified_enum_name(module, schema)};
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

auto serialized_parser(EnumModuleSchema const& module, EnumSchema const& schema) -> FunctionSpec {
    auto const enum_name{qualified_enum_name(module, schema)};
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

auto conversion_spec(EnumModuleSchema const& module,
                     EnumSchema const& schema,
                     EnumConversion const conversion) -> FunctionSpec {
    if (conversion == EnumConversion::try_parse_serialized) {
        return serialized_parser(module, schema);
    }
    auto const enum_name{qualified_enum_name(module, schema)};
    auto const display{conversion == EnumConversion::lex_to_display_string ||
                       conversion == EnumConversion::display_string_view ||
                       conversion == EnumConversion::display_string};
    auto const serialized{conversion == EnumConversion::lex_to_serialized_string};
    auto const lexical{conversion == EnumConversion::lex_to_string ||
                       conversion == EnumConversion::lex_to_display_string || serialized};
    auto const helper_in_type_namespace{
        module.helper_namespace.value_or(module.settings.namespace_name.value_or("")) ==
        module.settings.namespace_name.value_or("")};
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

auto wrapped(std::optional<std::string> const& namespace_name, Nodes nodes) -> Nodes {
    if (!namespace_name.has_value() || nodes.empty()) {
        return nodes;
    }
    return {Namespace{*namespace_name, std::move(nodes)}};
}

auto native_string_literal(std::string_view const value) -> std::string {
    return render(string_literal(value));
}

auto native_enum_name(EnumModuleSchema const& module, EnumSchema const& schema) -> std::string {
    return module.settings.namespace_name.has_value()
             ? "::" + *module.settings.namespace_name + "::" + schema.name
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

auto native_enum_declaration(EnumSchema const& schema, std::map<std::string, CppType> const& types)
    -> std::string {
    auto const underlying{resolve_type(schema.underlying_type, types).spelling};
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

auto native_enum_traits(EnumModuleSchema const& module, EnumSchema const& schema) -> std::string {
    auto const qualified{native_enum_name(module, schema)};
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
    return native_type.spelling;
}

auto unreal_projection_header(EnumSchema const& schema,
                              EnumUnrealProjection const& projection,
                              std::map<std::string, CppType> const& types) -> std::string {
    std::ostringstream output;
    output << (projection.reflection == EnumReflection::blueprint ? "UENUM(BlueprintType)"
                                                                  : "UENUM()")
           << "\nenum class " << projection.name << " : "
           << unreal_underlying_type(resolve_type(schema.underlying_type, types)) << " {\n";
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

auto unreal_conversion_header(EnumModuleSchema const& module,
                              EnumSchema const& schema,
                              EnumUnrealProjection const& projection) -> std::string {
    auto const native_name{native_enum_name(module, schema)};
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

auto lower_native_enum_module(EnumModuleSchema const& module,
                              std::map<std::string, CppType> const& types) -> std::vector<Module> {
    NodeListBuilder native_nodes;
    native_nodes.add(Include{"array", true}, 2)
        .add(Include{"cstdint", true}, 2)
        .add(Include{"cstddef", true}, 2)
        .add(Include{"optional", true}, 2)
        .add(Include{"string_view", true}, 2)
        .add(Include{"sandbox/core/enum_traits.h", false}, 2);

    NodeListBuilder enum_nodes;
    for (auto const& schema : module.enums) {
        enum_nodes.add(raw(native_enum_declaration(schema, types)), 2);
    }
    native_nodes.append(wrapped(module.settings.namespace_name, enum_nodes.build()));

    NodeListBuilder trait_nodes;
    for (auto const& schema : module.enums) {
        trait_nodes.add(raw(native_enum_traits(module, schema)), 2);
    }
    native_nodes.add(Namespace{"ml", trait_nodes.build()}, 2);

    std::vector<Module> result;
    result.push_back(Module{
        .name = module.settings.name,
        .header =
            CppFile{
                .path = module.settings.header,
                .nodes = native_nodes.build(),
                .clang_format_off = true,
                .include_order = module.settings.include_order,
            },
    });

    for (auto const& schema : module.enums) {
        if (!schema.unreal_projection.has_value()) {
            continue;
        }
        auto const& projection{*schema.unreal_projection};
        auto const generated_header{projection.header.stem().string() + ".generated.h"};
        result.push_back(Module{
            .name = module.settings.name + "_" + projection.name,
            .header =
                CppFile{
                    .path = projection.header,
                    .nodes = {Include{"CoreMinimal.h", false},
                              Include{generated_header, false},
                              raw(unreal_projection_header(schema, projection, types))},
                    .clang_format_off = true,
                },
        });
        result.push_back(Module{
            .name = module.settings.name + "_" + projection.name + "_conversion",
            .header =
                CppFile{
                    .path = projection.conversion_header,
                    .nodes = {Include{projection.native_header_include, false},
                              Include{projection.header_include, false},
                              raw(unreal_conversion_header(module, schema, projection))},
                    .clang_format_off = true,
                },
        });
    }
    return result;
}

} // namespace

auto lower_enum_module(EnumModuleSchema const& module, std::map<std::string, CppType> const& types)
    -> std::vector<Module> {
    if (std::ranges::any_of(module.enums,
                            [](EnumSchema const& schema) { return schema.native_api; })) {
        return lower_native_enum_module(module, types);
    }
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
        declarations.add(
            Enum{
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
                               conversion == EnumConversion::lex_to_display_string ||
                               conversion == EnumConversion::lex_to_serialized_string};
            (lexical ? lex_declarations : helper_declarations).add(declaration(std::move(spec)), 2);
        }
    }

    NodeListBuilder header_nodes;
    header_nodes.add(IncludeDependencies{}, 2);
    if (has_enum_arrays) {
        header_nodes.add(Include{"SandboxCore/enum_array.h", false}, 2);
    }
    if (has_reflected) {
        header_nodes.add(Include{module.settings.header.stem().string() + ".generated.h", false},
                         2);
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
        .header =
            CppFile{
                .path = module.settings.header,
                .nodes = header_nodes.build(),
                .clang_format_off = true,
                .include_order = module.settings.include_order,
            },
    };
    if (!module.settings.source.has_value()) {
        return {std::move(result)};
    }

    NodeListBuilder internal_definitions;
    NodeListBuilder lex_definitions;
    NodeListBuilder helper_definitions;
    for (auto const& schema : module.enums) {
        if (schema.conversions.empty()) {
            continue;
        }
        auto const has_display_conversion{
            std::ranges::any_of(schema.conversions, [](EnumConversion const conversion) {
                return conversion == EnumConversion::lex_to_display_string ||
                       conversion == EnumConversion::display_string_view ||
                       conversion == EnumConversion::display_string;
            })};
        auto const has_name_conversion{
            has_display_conversion ||
            std::ranges::any_of(schema.conversions, [](EnumConversion const conversion) {
                return conversion == EnumConversion::lex_to_string ||
                       conversion == EnumConversion::string_view ||
                       conversion == EnumConversion::string;
            })};
        if (has_name_conversion) {
            internal_definitions.add(
                Function{exact_lookup(module, schema), std::nullopt, false, false}, 2);
        }
        if (has_display_conversion) {
            internal_definitions.add(
                Function{display_lookup(module, schema), std::nullopt, false, false}, 2);
        }
        if (std::ranges::find(schema.conversions, EnumConversion::lex_to_serialized_string) !=
            schema.conversions.end()) {
            internal_definitions.add(
                Function{serialized_lookup(module, schema), std::nullopt, false, false}, 2);
        }
        for (auto const conversion : schema.conversions) {
            auto spec{conversion_spec(module, schema, conversion)};
            auto const lexical{conversion == EnumConversion::lex_to_string ||
                               conversion == EnumConversion::lex_to_display_string ||
                               conversion == EnumConversion::lex_to_serialized_string};
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
    return {std::move(result)};
}

} // namespace codegen::detail
