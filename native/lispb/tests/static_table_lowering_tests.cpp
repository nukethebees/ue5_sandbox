#include "compiler_test_context.h"
#include "test_utilities.h"
#include "text_expectations.h"

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
using lispb::test_support::expect_contains_all;
using lispb::test_support::expect_contains_none;
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
    auto& types{context.storage.cpp_types};
    auto const manifest{parse_test_manifest(
        context,
        source,
        {{"value", types.add("FValue", {types.dependency("FValue", "Project/Value.h")})},
         {"point", types.add("FPoint", {types.dependency("FPoint", "Project/Point.h")})}})};
    auto const files{render_test_manifest(context, manifest)};
    EXPECT_EQ(files.size(), 1);
    return files.front().content;
}

TEST(StaticTableLowering, GeneratesFixedRowsColumnsAndCoreFunctions) {
    CompilerTestContext context;
    auto const output{render_static_table(context)};

    expect_contains_all(output,
                        {"#include \"Project/Value.h\"",
                         "#include \"Project/Point.h\"",
                         "#include \"Containers/StaticArray.h\"",
                         "struct PROJECT_API FValues",
                         "static constexpr int32 num_rows{3};",
                         "static constexpr int32 first_index{0};",
                         "static constexpr int32 third_index{2};",
                         "static constexpr auto num() noexcept -> int32",
                         "auto apply_arrays(this auto&& self, TFunc&& func)",
                         "auto apply_array_pairs(this Self&& self, Other&& other, TFunc&& func)",
                         "auto get_point(int32 const index) const -> FPoint",
                         "return FPoint{ids[index], values[index]};",
                         "TStaticArray<std::int32_t, num_rows> ids{};",
                         "TStaticArray<FValue, num_rows> values{};",
                         "namespace project::generated",
                         "class FForward;"});
    expect_contains_none(output, {"get_view", "set_num"});
}

TEST(StaticTableLowering, GeneratesMultipleTablesInOneHeader) {
    CompilerTestContext context;
    auto const output{render_static_table(context, true)};

    expect_contains_all(output, {"struct PROJECT_API FValues", "struct PROJECT_API FOtherValues"});
}

} // namespace
} // namespace lispb::codegen
