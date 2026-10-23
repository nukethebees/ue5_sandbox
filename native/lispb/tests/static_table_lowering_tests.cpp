#include "compiler_test_context.h"
#include "test_utilities.h"

#include <codegen/ast/cpp_types.h>
#include <codegen/ast/indices.h>
#include <codegen/generator.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/schema_version.h>
#include <lispb/compiler_storage.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <gtest/gtest.h>

#include <string>
#include <string_view>

namespace lispb::codegen {
namespace {

auto render_static_table(CompilerTestContext& context, bool const two_tables = false)
    -> std::string {
    std::string source{R"((module tables :header "Tables.h" :namespace project::generated
        :prelude ("class FForward;"))"};
    auto append_table = [&](std::string_view name) {
        source += "(table " + std::string{name} + R"( :export-specifier PROJECT_API
            (row first) (row second) (row third)
            (column ids int32) (column values @value)
            (group point @point :columns (ids values))))";
    };
    append_table("FValues");
    if (two_tables) {
        append_table("FOtherValues");
    }
    source += ')';
    auto const files{render_test_manifest(
        context,
        parse_test_manifest(
            context,
            source,
            {{"value",
              context.storage.cpp_types.add(
                  "FValue", {context.storage.cpp_types.dependency("FValue", "Project/Value.h")})},
             {"point",
              context.storage.cpp_types.add(
                  "FPoint",
                  {context.storage.cpp_types.dependency("FPoint", "Project/Point.h")})}}))};
    EXPECT_EQ(files.size(), 1);
    return files.front().content;
}

TEST(StaticTableLowering, GeneratesFixedRowsColumnsAndCoreFunctions) {
    CompilerTestContext context;
    auto const output{render_static_table(context)};

    EXPECT_NE(output.find("#include \"Project/Value.h\""), std::string::npos);
    EXPECT_NE(output.find("#include \"Project/Point.h\""), std::string::npos);
    EXPECT_NE(output.find("#include \"Containers/StaticArray.h\""), std::string::npos);
    EXPECT_NE(output.find("struct PROJECT_API FValues"), std::string::npos);
    EXPECT_NE(output.find("static constexpr int32 num_rows{3};"), std::string::npos);
    EXPECT_NE(output.find("static constexpr int32 first_index{0};"), std::string::npos);
    EXPECT_NE(output.find("static constexpr int32 third_index{2};"), std::string::npos);
    EXPECT_NE(output.find("static constexpr auto num() noexcept -> int32"), std::string::npos);
    EXPECT_NE(output.find("auto apply_arrays(this auto&& self, TFunc&& func)"), std::string::npos);
    EXPECT_NE(output.find("auto apply_array_pairs(this Self&& self, Other&& other, TFunc&& func)"),
              std::string::npos);
    EXPECT_NE(output.find("auto get_point(int32 const index) const -> FPoint"), std::string::npos);
    EXPECT_NE(output.find("return FPoint{ids[index], values[index]};"), std::string::npos);
    EXPECT_NE(output.find("TStaticArray<std::int32_t, num_rows> ids{};"), std::string::npos);
    EXPECT_NE(output.find("TStaticArray<FValue, num_rows> values{};"), std::string::npos);
    EXPECT_NE(output.find("namespace project::generated"), std::string::npos);
    EXPECT_NE(output.find("class FForward;"), std::string::npos);
    EXPECT_EQ(output.find("get_view"), std::string::npos);
    EXPECT_EQ(output.find("set_num"), std::string::npos);
}

TEST(StaticTableLowering, GeneratesMultipleTablesInOneHeader) {
    CompilerTestContext context;
    auto const output{render_static_table(context, true)};

    EXPECT_NE(output.find("struct PROJECT_API FValues"), std::string::npos);
    EXPECT_NE(output.find("struct PROJECT_API FOtherValues"), std::string::npos);
}

} // namespace
} // namespace lispb::codegen
