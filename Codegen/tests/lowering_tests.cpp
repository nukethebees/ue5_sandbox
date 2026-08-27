#include <codegen/generator.h>

#include <gtest/gtest.h>

#include <map>
#include <string>
#include <utility>

namespace codegen {
namespace {

struct RenderedModule {
    std::string header;
    std::string source;
};

auto render_soa(SoaSchema schema,
                std::map<std::string, CppType> types = {}) -> RenderedModule {
    auto const files{render_modules(lower_modules(Manifest{
        .schema_version = 1,
        .types = std::move(types),
        .modules = {SoaModuleSchema{
            .settings = ModuleSettings{
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

auto basic_schema() -> SoaSchema {
    return SoaSchema{
        .name = "FData",
        .members = {
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
    EXPECT_NE(output.source.find("void FData::reserve(int32 const count)"),
              std::string::npos);
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

TEST(Lowering, UsesRegisteredAndGenericNestedRemovalOperations) {
    CppType registered{"FRegistered", "Project/Registered.h"};
    registered.member_operations.emplace(TypeOperation::remove_at_swap, "erase_swap");
    auto schema{SoaSchema{
        .name = "FData",
        .members = {
            SoaMemberSchema{"values", SoaMemberKind::array, TypeRef{"int32"}},
            SoaMemberSchema{"registered", SoaMemberKind::nested, TypeRef{"@registered"}},
            SoaMemberSchema{"generic", SoaMemberKind::nested, TypeRef{"@generic"}},
        },
        .operations = {StorageOperation::remove_at_swap},
    }};

    auto const output{render_soa(
        std::move(schema),
        {{"registered", std::move(registered)}, {"generic", CppType{"FGeneric"}}})};

    EXPECT_NE(output.header.find("values.RemoveAtSwap(index, count, allow_shrinking);"),
              std::string::npos);
    EXPECT_NE(output.header.find("registered.erase_swap(index, count, allow_shrinking);"),
              std::string::npos);
    EXPECT_NE(output.header.find(
                  "ml::remove_at_swap(generic, index, count, allow_shrinking);"),
              std::string::npos);
}

TEST(Lowering, SupportsMemberwiseCopying) {
    auto schema{basic_schema()};
    schema.operations = {StorageOperation::copy_element};
    schema.copy_element_memberwise = true;

    auto const output{render_soa(std::move(schema))};

    EXPECT_NE(output.header.find("ids[dst_i] = other.ids[src_i];"), std::string::npos);
    EXPECT_NE(output.header.find("weights[dst_i] = other.weights[src_i];"),
              std::string::npos);
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
    EXPECT_NE(output.header.find("TFixedData(TFixedData const& other)"),
              std::string::npos);
    EXPECT_NE(output.header.find("TFixedData(TFixedData&& other)"), std::string::npos);
    EXPECT_NE(output.header.find("~TFixedData() { reset(); }"), std::string::npos);
    EXPECT_NE(output.header.find("auto operator=(TFixedData const& other)"),
              std::string::npos);
    EXPECT_NE(output.header.find("requires (Capacity >= 0)"), std::string::npos);
    EXPECT_EQ(occurrences(output.header, "size_type size_{};"), 1);
}

} // namespace
} // namespace codegen
