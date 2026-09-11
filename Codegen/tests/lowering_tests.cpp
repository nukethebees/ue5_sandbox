#include <codegen/generator.h>

#include <gtest/gtest.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace codegen {
namespace {

struct RenderedModule {
    std::string header;
    std::string source;
};

auto render_soa(SoaSchema schema, std::map<std::string, CppType> types = {}) -> RenderedModule {
    auto const files{render_modules(lower_modules(Manifest{
        .schema_version = manifest_schema_version,
        .types = std::move(types),
        .modules = {SoaModuleSchema{
            .settings =
                ModuleSettings{
                    .name = "test",
                    .header = "Generated.h",
                    .source = "Generated.cpp",
                    .header_include = "Project/Generated.h",
                },
            .structs = {std::move(schema)},
        }},
    }))};
    EXPECT_EQ(files.size(), 2);
    return RenderedModule{files[0].content, files[1].content};
}

auto render_enum(EnumModuleSchema module) -> RenderedModule {
    auto const files{render_modules(lower_modules(Manifest{
        .schema_version = manifest_schema_version,
        .modules = {std::move(module)},
    }))};
    EXPECT_EQ(files.size(), 2);
    return RenderedModule{files[0].content, files[1].content};
}

auto render_settings(SettingsModuleSchema module) -> RenderedModule {
    auto const files{render_modules(lower_modules(Manifest{
        .schema_version = manifest_schema_version,
        .modules = {std::move(module)},
    }))};
    EXPECT_EQ(files.size(), 2);
    return RenderedModule{files[0].content, files[1].content};
}

auto basic_schema() -> SoaSchema {
    return SoaSchema{
        .name = "FData",
        .members =
            {
                SoaMemberSchema{"ids", SoaMemberKind::array, TypeRef{"int32"}},
                SoaMemberSchema{"weights", SoaMemberKind::array, TypeRef{"float"}},
            },
    };
}

auto occurrences(std::string const& text, std::string const& value) -> std::size_t {
    std::size_t count{};
    auto position{text.find(value)};
    while (position != std::string::npos) {
        ++count;
        position = text.find(value, position + value.size());
    }
    return count;
}

TEST(Lowering, EmitsOnlyRequestedStorageOperations) {
    auto schema{basic_schema()};
    schema.operations = all_storage_operations();

    auto const output{render_soa(std::move(schema))};

    EXPECT_NE(output.source.find("void FData::reset()"), std::string::npos);
    EXPECT_NE(output.source.find("void FData::reserve(int32 const count)"), std::string::npos);
    EXPECT_NE(output.source.find("void FData::add_uninitialised(int32 const count)"),
              std::string::npos);
    EXPECT_NE(output.source.find("void FData::add_defaulted(int32 const count)"),
              std::string::npos);
    EXPECT_NE(output.source.find("void FData::set_num("), std::string::npos);
    EXPECT_NE(output.header.find("void remove_at_swap("), std::string::npos);
    EXPECT_NE(output.header.find("void copy_element("), std::string::npos);
    EXPECT_NE(output.header.find("void copy_elements("), std::string::npos);
    EXPECT_NE(output.header.find("void copy_to_tail("), std::string::npos);
    EXPECT_NE(output.header.find("void append_from("), std::string::npos);

    auto no_operations{basic_schema()};
    auto const minimal{render_soa(std::move(no_operations))};
    EXPECT_EQ(minimal.header.find("void remove_at_swap("), std::string::npos);
    EXPECT_EQ(minimal.header.find("void copy_element("), std::string::npos);
    EXPECT_EQ(minimal.header.find("void append_from("), std::string::npos);
    EXPECT_EQ(minimal.source.find("void FData::reset()"), std::string::npos);
}

TEST(Lowering, EmitsReflectedEnumsAndSelectableOutOfLineConversions) {
    auto const output{render_enum(EnumModuleSchema{
        .settings =
            ModuleSettings{
                .name = "modes",
                .header = "Modes.h",
                .source = "Modes.cpp",
                .header_include = "Project/Modes.h",
            },
        .helper_namespace = "project",
        .enums = {EnumSchema{
            .name = "EMode",
            .underlying_type = TypeRef{"uint8"},
            .reflection = EnumReflection::blueprint,
            .values =
                {
                    EnumeratorSchema{"First", std::nullopt, std::nullopt, false, "first"},
                    EnumeratorSchema{"Readable", "7", "Readable Value", false, "readable"},
                    EnumeratorSchema{"COUNT", std::nullopt, std::nullopt, true, "count"},
                },
            .conversions =
                {
                    EnumConversion::lex_to_string,
                    EnumConversion::string_view,
                    EnumConversion::display_string_view,
                    EnumConversion::lex_to_serialized_string,
                    EnumConversion::try_parse_serialized,
                },
            .export_specifier = "PROJECT_API",
        }},
    })};

    EXPECT_NE(output.header.find("#include \"Modes.generated.h\""), std::string::npos);
    EXPECT_NE(output.header.find("UENUM(BlueprintType)\nenum class EMode : uint8"),
              std::string::npos);
    EXPECT_NE(output.header.find("Readable = 7 UMETA(DisplayName = \"Readable Value\")"),
              std::string::npos);
    EXPECT_NE(output.header.find("COUNT UMETA(Hidden)"), std::string::npos);
    EXPECT_NE(
        output.header.find("PROJECT_API auto LexToString(EMode const value) -> TCHAR const*;"),
        std::string::npos);
    EXPECT_NE(output.header.find("namespace project {\nPROJECT_API auto to_string_view"),
              std::string::npos);
    EXPECT_NE(output.header.find(
                  "PROJECT_API auto LexToSerializedString(EMode const value) -> TCHAR const*;"),
              std::string::npos);
    EXPECT_NE(
        output.header.find(
            "PROJECT_API auto try_parse_serialized(FStringView const value, EMode& result) -> "
            "bool;"),
        std::string::npos);
    EXPECT_EQ(output.header.find("auto to_string(EMode"), std::string::npos);

    EXPECT_NE(output.source.find("#include \"Project/Modes.h\""), std::string::npos);
    EXPECT_NE(output.source.find("switch (value)"), std::string::npos);
    EXPECT_NE(output.source.find("TEXT(\"<invalid EMode>\")"), std::string::npos);
    EXPECT_NE(output.source.find("return TEXT(\"Readable Value\");"), std::string::npos);
    EXPECT_NE(output.source.find("return TEXT(\"readable\");"), std::string::npos);
    EXPECT_NE(output.source.find("result = EMode::Readable;"), std::string::npos);
    EXPECT_NE(output.source.find("auto LexToString(EMode const value) -> TCHAR const*"),
              std::string::npos);
}

TEST(Lowering, EmitsPlainEnumsInTheirNamespace) {
    auto const output{render_enum(EnumModuleSchema{
        .settings =
            ModuleSettings{
                .name = "states",
                .header = "States.h",
                .source = "States.cpp",
                .namespace_name = "project::states",
            },
        .enums = {EnumSchema{
            .name = "EState",
            .underlying_type = TypeRef{"int"},
            .values = {EnumeratorSchema{"Ready"}},
            .conversions = {EnumConversion::string},
        }},
    })};

    EXPECT_NE(output.header.find("namespace project::states {\nenum class EState : int"),
              std::string::npos);
    EXPECT_NE(output.header.find("namespace project::states {\nauto to_string(EState const value)"),
              std::string::npos);
    EXPECT_EQ(output.header.find("UENUM"), std::string::npos);
    EXPECT_NE(output.source.find("project::states::EState::Ready"), std::string::npos);
}

TEST(Lowering, EmitsTraitsForEnumArrayEnums) {
    auto const output{render_enum(EnumModuleSchema{
        .settings =
            ModuleSettings{
                .name = "modes",
                .header = "Modes.h",
                .source = "Modes.cpp",
            },
        .enums = {EnumSchema{
            .name = "EMode",
            .underlying_type = TypeRef{"uint8"},
            .values = {EnumeratorSchema{"First"}, EnumeratorSchema{"Second"}},
            .enum_array = true,
        }},
    })};

    EXPECT_NE(output.header.find("#include \"SandboxCore/enum_array.h\""), std::string::npos);
    EXPECT_NE(
        output.header.find("struct TEnumTraits<EMode> {\n    static constexpr int32 count{2};\n};"),
        std::string::npos);
    EXPECT_EQ(output.header.find("COUNT"), std::string::npos);
}

TEST(Lowering, EmitsTraitsForEnumArrayEnumsWithCountSentinels) {
    auto const output{render_enum(EnumModuleSchema{
        .settings =
            ModuleSettings{
                .name = "modes",
                .header = "Modes.h",
                .source = "Modes.cpp",
            },
        .enums = {EnumSchema{
            .name = "EMode",
            .underlying_type = TypeRef{"uint8"},
            .reflection = EnumReflection::uenum,
            .values =
                {
                    EnumeratorSchema{"First"},
                    EnumeratorSchema{"Second"},
                    EnumeratorSchema{"COUNT", std::nullopt, std::nullopt, true},
                },
            .enum_array = true,
            .count = "COUNT",
        }},
    })};

    EXPECT_NE(output.header.find("#include \"SandboxCore/enum_array.h\""), std::string::npos);
    EXPECT_NE(output.header.find("struct TEnumTraits<EMode> {\n    static constexpr int32 "
                                 "count{static_cast<int32>(EMode::COUNT)};\n};"),
              std::string::npos);
}

TEST(Lowering, EnumDisplayLookupWithoutOverridesFallsBackDirectly) {
    auto const output{render_enum(EnumModuleSchema{
        .settings = {.name = "states", .header = "States.h", .source = "States.cpp"},
        .enums = {EnumSchema{
            .name = "EState",
            .underlying_type = TypeRef{"int"},
            .values = {EnumeratorSchema{"Ready"}},
            .conversions = {EnumConversion::display_string},
        }},
    })};

    EXPECT_NE(
        output.source.find("auto get_state_display_name(EState const value) -> TCHAR const* {\n"
                           "    return get_state_name(value);\n"
                           "}"),
        std::string::npos);
    EXPECT_EQ(occurrences(output.source, "switch (value)"), 1);
    EXPECT_NE(output.source.find("return FString{get_state_display_name(value)};"),
              std::string::npos);
}

TEST(Lowering, EnumEscapingAndSparseDisplayCasesPreserveFallbacks) {
    auto const output{render_enum(EnumModuleSchema{
        .settings = {.name = "states", .header = "States.h", .source = "States.cpp"},
        .enums = {EnumSchema{
            .name = "EState",
            .underlying_type = TypeRef{"int"},
            .reflection = EnumReflection::uenum,
            .values = {EnumeratorSchema{"Ready", std::nullopt, std::nullopt, false, "ready"},
                       EnumeratorSchema{
                           "Special", std::nullopt, "Quote\" Slash\\\n\t", false, "a\"b\\c\n"}},
            .conversions = {EnumConversion::display_string,
                            EnumConversion::lex_to_serialized_string,
                            EnumConversion::try_parse_serialized},
        }},
    })};

    EXPECT_NE(output.header.find("UMETA(DisplayName = \"Quote\\\" Slash\\\\\\n\\t\")"),
              std::string::npos);
    EXPECT_NE(
        output.source.find("auto get_state_display_name(EState const value) -> TCHAR const* {\n"
                           "    switch (value) {\n"
                           "    case EState::Special: {\n"
                           "        return TEXT(\"Quote\\\" Slash\\\\\\n\\t\");\n"
                           "    }\n"
                           "    default: {\n"
                           "        break;\n"
                           "    }\n"
                           "    }\n\n"
                           "    return get_state_name(value);\n"
                           "}"),
        std::string::npos);
    EXPECT_NE(output.source.find("if (value == TEXT(\"a\\\"b\\\\c\\n\")) {\n"
                                 "        result = EState::Special;\n"
                                 "        return true;\n"
                                 "    }\n"
                                 "    return false;"),
              std::string::npos);
}

TEST(Lowering, EnumTraitsRemainGlobalForQualifiedHeaderOnlyEnums) {
    auto const files{render_modules(lower_modules(Manifest{
        .schema_version = manifest_schema_version,
        .modules = {EnumModuleSchema{
            .settings =
                {.name = "states", .header = "States.h", .namespace_name = "project::states"},
            .enums = {EnumSchema{
                .name = "EState",
                .underlying_type = TypeRef{"int"},
                .values = {EnumeratorSchema{"Ready"}, EnumeratorSchema{"COUNT"}},
                .enum_array = true,
                .count = "COUNT",
            }},
        }},
    }))};
    ASSERT_EQ(files.size(), 1);
    EXPECT_NE(
        files.front().content.find("} // namespace project::states\n"
                                   "template <>\n"
                                   "struct TEnumTraits<project::states::EState> {\n"
                                   "    static constexpr int32 "
                                   "count{static_cast<int32>(project::states::EState::COUNT)};\n"
                                   "};"),
        std::string::npos);
}

TEST(Lowering, AppliesEveryStorageOperationToEveryMember) {
    auto schema{basic_schema()};
    schema.operations = all_storage_operations();
    auto const output{render_soa(std::move(schema))};

    std::vector<std::pair<std::string const*, std::string>> const expectations{
        {&output.source, "ml::reset(ids);"},
        {&output.source, "ml::reset(weights);"},
        {&output.source, "ml::reserve(ids, count);"},
        {&output.source, "ml::reserve(weights, count);"},
        {&output.source, "ml::add_uninitialised(ids, count);"},
        {&output.source, "ml::add_uninitialised(weights, count);"},
        {&output.source, "ml::add_defaulted(ids, count);"},
        {&output.source, "ml::add_defaulted(weights, count);"},
        {&output.header, "ids.RemoveAtSwap(index, count, allow_shrinking);"},
        {&output.header, "weights.RemoveAtSwap(index, count, allow_shrinking);"},
        {&output.source, "ml::set_num(ids, count, allow_shrinking);"},
        {&output.source, "ml::set_num(weights, count, allow_shrinking);"},
        {&output.header, "ml::copy_element(ids, dst_i, other.ids, src_i);"},
        {&output.header, "ml::copy_element(weights, dst_i, other.weights, src_i);"},
        {&output.header, "ml::append_from(ids, other.ids);"},
        {&output.header, "ml::append_from(weights, other.weights);"},
        {&output.source, "ml::apply_permutation(ids, indices);"},
        {&output.source, "ml::apply_permutation(weights, indices);"},
    };

    for (auto const& [text, expected] : expectations) {
        SCOPED_TRACE(expected);
        EXPECT_NE(text->find(expected), std::string::npos);
    }
}

TEST(Lowering, UsesRegisteredAndGenericNestedRemovalOperations) {
    CppType registered{"FRegistered", "Project/Registered.h"};
    registered.member_operations.emplace(TypeOperation::remove_at_swap, "erase_swap");
    auto schema{SoaSchema{
        .name = "FData",
        .members =
            {
                SoaMemberSchema{"values", SoaMemberKind::array, TypeRef{"int32"}},
                SoaMemberSchema{"registered", SoaMemberKind::nested, TypeRef{"@registered"}},
                SoaMemberSchema{"generic", SoaMemberKind::nested, TypeRef{"@generic"}},
            },
        .operations = {StorageOperation::remove_at_swap},
    }};

    auto const output{
        render_soa(std::move(schema),
                   {{"registered", std::move(registered)}, {"generic", CppType{"FGeneric"}}})};

    EXPECT_NE(output.header.find("values.RemoveAtSwap(index, count, allow_shrinking);"),
              std::string::npos);
    EXPECT_NE(output.header.find("registered.erase_swap(index, count, allow_shrinking);"),
              std::string::npos);
    EXPECT_NE(output.header.find("ml::remove_at_swap(generic, index, count, allow_shrinking);"),
              std::string::npos);
}

TEST(Lowering, EmitsLogicalElementSettersAndAddForArraysAndSupportedNestedMembers) {
    CppType vectors{"FVectors3f", "Project/Vectors.h"};
    vectors.member_operations.emplace(TypeOperation::add_element, "add");
    vectors.member_operation_parameter_passing.emplace(TypeOperation::add_element,
                                                       ParameterPassing::value);
    vectors.member_operations.emplace(TypeOperation::set_element, "set");
    vectors.member_operation_parameter_passing.emplace(TypeOperation::set_element,
                                                       ParameterPassing::value);
    auto schema{SoaSchema{
        .name = "FData",
        .members =
            {
                SoaMemberSchema{"locations", SoaMemberKind::nested, TypeRef{"@vectors"}},
                SoaMemberSchema{"teams", SoaMemberKind::array, TypeRef{"ETeam"}},
                SoaMemberSchema{"healths", SoaMemberKind::array, TypeRef{"int32"}},
            },
    }};

    auto const output{render_soa(std::move(schema), {{"vectors", std::move(vectors)}})};

    EXPECT_EQ(occurrences(output.header,
                          "void set(int32 const index, FVectors3f::equivalent_type const "
                          "new_locations, ETeam const& new_teams, int32 const new_healths)"),
              2);
    EXPECT_NE(output.header.find("locations.set(index, new_locations);"), std::string::npos);
    EXPECT_NE(output.header.find("teams[index] = new_teams;"), std::string::npos);
    EXPECT_NE(output.header.find("healths[index] = new_healths;"), std::string::npos);
    EXPECT_NE(output.header.find("auto add(FVectors3f::equivalent_type const new_locations, "
                                 "ETeam const& new_teams, int32 const new_healths) -> int32"),
              std::string::npos);
    EXPECT_NE(output.header.find("auto const index{num()};"), std::string::npos);
    EXPECT_NE(output.header.find("locations.add(new_locations);"), std::string::npos);
    EXPECT_NE(output.header.find("teams.Add(new_teams);"), std::string::npos);
    EXPECT_NE(output.header.find("healths.Add(new_healths);"), std::string::npos);
    EXPECT_NE(output.header.find("return index;"), std::string::npos);
}

TEST(Lowering, OmitsLogicalElementSetterAndAddForUnsupportedNestedMembers) {
    auto schema{SoaSchema{
        .name = "FData",
        .members =
            {
                SoaMemberSchema{"values", SoaMemberKind::nested, TypeRef{"@nested"}},
            },
    }};

    auto const output{render_soa(std::move(schema), {{"nested", CppType{"FNested"}}})};

    EXPECT_EQ(output.header.find("void set("), std::string::npos);
    EXPECT_EQ(output.header.find("auto add("), std::string::npos);
}

TEST(Lowering, SupportsMemberwiseCopying) {
    auto schema{basic_schema()};
    schema.operations = {StorageOperation::copy_element};
    schema.copy_element_memberwise = true;

    auto const output{render_soa(std::move(schema))};

    EXPECT_NE(output.header.find("ids[dst_i] = other.ids[src_i];"), std::string::npos);
    EXPECT_NE(output.header.find("weights[dst_i] = other.weights[src_i];"), std::string::npos);
    EXPECT_EQ(output.header.find("ml::copy_element(ids"), std::string::npos);
    EXPECT_NE(output.header.find("ml::copy_elements(ids, dst_i, other.ids, src_i, count);"),
              std::string::npos);
}

TEST(Lowering, LowersCustomFunctionsToTheirRequestedFile) {
    auto schema{basic_schema()};
    schema.view_name = "FMutableRows";
    schema.const_view_name = "FRows";
    schema.using_declarations = {"Index = int32"};
    schema.functions = {
        FunctionSchema{
            .name = "first",
            .return_type = TypeRef{"int32"},
            .body_lines = {"return ids[0];"},
            .is_inline = true,
        },
        FunctionSchema{
            .name = "sum",
            .return_type = TypeRef{"int32"},
            .body_lines = {"return helper(ids);"},
            .dependencies = {"helper"},
            .definition_in_source = true,
        },
    };

    auto const output{
        render_soa(std::move(schema), {{"helper", CppType{"helper", "Project/Helper.h"}}})};

    EXPECT_NE(output.header.find("using View = FMutableRows;"), std::string::npos);
    EXPECT_NE(output.header.find("using ConstView = FRows;"), std::string::npos);
    EXPECT_NE(output.header.find("using Index = int32;"), std::string::npos);
    EXPECT_NE(output.header.find("return ids[0];"), std::string::npos);
    EXPECT_EQ(output.header.find("return helper(ids);"), std::string::npos);
    EXPECT_NE(output.source.find("int32 FData::sum()"), std::string::npos);
    EXPECT_NE(output.source.find("return helper(ids);"), std::string::npos);
    EXPECT_NE(output.source.find("#include \"Project/Helper.h\""), std::string::npos);
}

TEST(Lowering, ValidatesSizesBeforePermutationAndSorting) {
    auto const output{render_soa(basic_schema())};

    EXPECT_NE(output.source.find("void FData::apply_permutation("), std::string::npos);
    EXPECT_NE(output.source.find("validate_array_sizes();"), std::string::npos);
    EXPECT_NE(output.header.find("template <typename Compare>"), std::string::npos);
    EXPECT_NE(output.header.find("template <auto Compare>"), std::string::npos);
    EXPECT_GE(occurrences(output.header, "validate_array_sizes();"), 2);
    EXPECT_NE(output.header.find("ml::fill_indices(scratch_indices);"), std::string::npos);
}

TEST(Lowering, FixedContainersOwnOneSizeAndImplementValueSemantics) {
    auto schema{basic_schema()};
    schema.fixed = FixedSoaSchema{"TDataStorage", {"TFixedData"}};

    auto const output{render_soa(std::move(schema))};

    EXPECT_NE(output.header.find("struct TDataStorage"), std::string::npos);
    EXPECT_NE(output.header.find("struct TFixedData"), std::string::npos);
    EXPECT_NE(output.header.find("TFixedData(TFixedData const& other)"), std::string::npos);
    EXPECT_NE(output.header.find("TFixedData(TFixedData&& other)"), std::string::npos);
    EXPECT_NE(output.header.find("~TFixedData() { reset(); }"), std::string::npos);
    EXPECT_NE(output.header.find("auto operator=(TFixedData const& other)"), std::string::npos);
    EXPECT_NE(output.header.find("void set(int32 const index, int32 const new_ids, float const "
                                 "new_weights)"),
              std::string::npos);
    EXPECT_NE(output.header.find("get_view().set(index, new_ids, new_weights);"),
              std::string::npos);
    EXPECT_NE(output.header.find("requires (Capacity >= 0)"), std::string::npos);
    EXPECT_EQ(occurrences(output.header, "size_type size_{};"), 1);
}

TEST(Lowering, GeneratesTypedSettingsApiAndRuntimeDescriptors) {
    auto const output{render_settings(SettingsModuleSchema{
        .settings =
            ModuleSettings{
                .name = "settings",
                .header = "Settings.h",
                .source = "Settings.cpp",
                .namespace_name = "game",
            },
        .api_name = "TSettingsAccess",
        .state_name = "FSettingsState",
        .categories = {{"video", "Video"}},
        .settings_list =
            {
                SettingSchema{
                    .name = "vsync",
                    .label = "VSync",
                    .category = "video",
                    .value_type = TypeRef{"bool"},
                    .backend = "engine",
                    .apply_mode = SettingApplyMode::deferred,
                    .control = SettingControlSchema{.kind = SettingControlKind::toggle},
                },
                SettingSchema{
                    .name = "frame_limit",
                    .label = "Frame Limit",
                    .category = "video",
                    .value_type = TypeRef{"float"},
                    .backend = "engine",
                    .apply_mode = SettingApplyMode::immediate,
                    .control =
                        SettingControlSchema{
                            .kind = SettingControlKind::choice,
                            .options_provider = "frame_limits",
                        },
                },
            },
    })};

    EXPECT_NE(output.header.find("enum class EGameSetting : uint8"), std::string::npos);
    EXPECT_NE(output.header.find("using FGameSettingValue = std::variant<bool, float>"),
              std::string::npos);
    EXPECT_NE(output.header.find("void set_vsync(bool const value)"), std::string::npos);
    EXPECT_NE(output.header.find("auto operator==(FSettingsState const&) const -> bool = default"),
              std::string::npos);
    EXPECT_NE(output.source.find("ESettingApplyMode::Deferred"), std::string::npos);
    EXPECT_NE(output.source.find("EGameSettingOptionProvider::FrameLimits"), std::string::npos);
    EXPECT_NE(output.source.find("std::get_if<float>"), std::string::npos);
}

} // namespace
} // namespace codegen
