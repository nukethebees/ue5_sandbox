#include <codegen/generator.h>

#include <gtest/gtest.h>

#include <string>

namespace codegen {
namespace {

auto static_table_module() -> StaticTableModuleSchema {
    return StaticTableModuleSchema{
        .settings = ModuleSettings{
            .name = "tables",
            .header = "Tables.h",
            .namespace_name = "project::generated",
            .prelude_lines = {"class FForward;"},
        },
        .tables = {StaticTableSchema{
            .name = "FValues",
            .rows = {StaticTableRowSchema{"first"},
                     StaticTableRowSchema{"second"},
                     StaticTableRowSchema{"third"}},
            .columns = {StaticTableColumnSchema{"ids", TypeRef{"int32"}},
                        StaticTableColumnSchema{"values", TypeRef{"@value"}}},
            .groups = {StaticTableGroupSchema{
                "point", TypeRef{"@point"}, {"ids", "values"}}},
            .export_specifier = "PROJECT_API",
        }},
    };
}

auto render_static_table(StaticTableModuleSchema module) -> std::string {
    Manifest const manifest{
        .schema_version = manifest_schema_version,
        .types = {{"value", CppType{"FValue", "Project/Value.h"}},
                  {"point", CppType{"FPoint", "Project/Point.h"}}},
        .modules = {std::move(module)},
    };
    auto const files{render_modules(lower_modules(manifest))};
    EXPECT_EQ(files.size(), 1);
    return files.front().content;
}

TEST(StaticTableLowering, GeneratesFixedRowsColumnsAndCoreFunctions) {
    auto const output{render_static_table(static_table_module())};

    EXPECT_NE(output.find("#include \"Project/Value.h\""), std::string::npos);
    EXPECT_NE(output.find("#include \"Project/Point.h\""), std::string::npos);
    EXPECT_NE(output.find("#include \"Containers/StaticArray.h\""), std::string::npos);
    EXPECT_NE(output.find("struct PROJECT_API FValues"), std::string::npos);
    EXPECT_NE(output.find("static constexpr int32 num_rows{3};"), std::string::npos);
    EXPECT_NE(output.find("static constexpr int32 first_index{0};"), std::string::npos);
    EXPECT_NE(output.find("static constexpr int32 third_index{2};"), std::string::npos);
    EXPECT_NE(output.find("static constexpr auto num() noexcept -> int32"), std::string::npos);
    EXPECT_NE(output.find("auto apply_arrays(this auto&& self, TFunc&& func)"),
              std::string::npos);
    EXPECT_NE(output.find("auto apply_array_pairs(this Self&& self, Other&& other, TFunc&& func)"),
              std::string::npos);
    EXPECT_NE(output.find("auto get_point(int32 const index) const -> FPoint"),
              std::string::npos);
    EXPECT_NE(output.find("return FPoint{ids[index], values[index]};"), std::string::npos);
    EXPECT_NE(output.find("TStaticArray<int32, num_rows> ids{};"), std::string::npos);
    EXPECT_NE(output.find("TStaticArray<FValue, num_rows> values{};"), std::string::npos);
    EXPECT_NE(output.find("namespace project::generated"), std::string::npos);
    EXPECT_NE(output.find("class FForward;"), std::string::npos);
    EXPECT_EQ(output.find("get_view"), std::string::npos);
    EXPECT_EQ(output.find("set_num"), std::string::npos);
}

TEST(StaticTableLowering, GeneratesMultipleTablesInOneHeader) {
    auto module{static_table_module()};
    auto second{module.tables.front()};
    second.name = "FOtherValues";
    module.tables.push_back(std::move(second));

    auto const output{render_static_table(std::move(module))};

    EXPECT_NE(output.find("struct PROJECT_API FValues"), std::string::npos);
    EXPECT_NE(output.find("struct PROJECT_API FOtherValues"), std::string::npos);
}

} // namespace
} // namespace codegen
