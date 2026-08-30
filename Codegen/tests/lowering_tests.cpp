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
        .settings = ModuleSettings{
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
                    EnumeratorSchema{"First"},
                    EnumeratorSchema{"Readable", "7", "Readable Value"},
                    EnumeratorSchema{"COUNT", std::nullopt, std::nullopt, true},
                },
            .conversions =
                {
                    EnumConversion::lex_to_string,
                    EnumConversion::string_view,
                    EnumConversion::display_string_view,
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
    EXPECT_NE(output.header.find("PROJECT_API auto LexToString(EMode const value) -> TCHAR const*;"),
              std::string::npos);
    EXPECT_NE(output.header.find("namespace project {\nPROJECT_API auto to_string_view"),
              std::string::npos);
    EXPECT_EQ(output.header.find("auto to_string(EMode"), std::string::npos);

    EXPECT_NE(output.source.find("#include \"Project/Modes.h\""), std::string::npos);
    EXPECT_NE(output.source.find("switch (value)"), std::string::npos);
    EXPECT_NE(output.source.find("TEXT(\"<invalid EMode>\")"), std::string::npos);
    EXPECT_NE(output.source.find("return TEXT(\"Readable Value\");"), std::string::npos);
    EXPECT_NE(output.source.find("auto LexToString(EMode const value) -> TCHAR const*"),
              std::string::npos);
}

TEST(Lowering, EmitsPlainEnumsInTheirNamespace) {
    auto const output{render_enum(EnumModuleSchema{
        .settings = ModuleSettings{
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
    EXPECT_NE(output.header.find("requires (Capacity >= 0)"), std::string::npos);
    EXPECT_EQ(occurrences(output.header, "size_type size_{};"), 1);
}

} // namespace
} // namespace codegen
