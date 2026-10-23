#include "compiler_test_context.h"
#include "test_utilities.h"

#include <codegen/ast/cpp_types.h>
#include <codegen/generator.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/schema_version.h>
#include <lispb/compiler_storage.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <string_view>

namespace single_allocation_tests {
using lispb::codegen::CompilerTestContext;

TEST(SingleAllocationSoa, OrdinarySchemasGenerateNativeOwnersAndViews) {
    CompilerTestContext context;
    auto const files{render_test_manifest(context, parse_test_manifest(context, R"(
        (module native :header "Native.h" :namespace example
          (struct Child (member xs array float))
          (struct Rows
            (member ids array std::int32_t)
            (member nested nested Child)
            (member local_types array example::Token)
            (member nested_types array example::sub::Token)
            (member foreign_types array other::Token)
            (member shadowed_types array example::ColLayout))))"))};
    auto const& output{files.front().content};
    EXPECT_NE(output.find("struct RowsView"), std::string::npos);
    EXPECT_NE(output.find("struct RowsConstView"), std::string::npos);
    EXPECT_NE(output.find("ColLayout<std::int32_t> IdsColumn"), std::string::npos);
    EXPECT_NE(output.find("auto ids() const"), std::string::npos);
    EXPECT_NE(output.find("NestedXsColumn{IdsColumn}"), std::string::npos);
    EXPECT_NE(output.find("ColLayout<Token> LocalTypesColumn"), std::string::npos);
    EXPECT_NE(output.find("ColLayout<sub::Token> NestedTypesColumn"), std::string::npos);
    EXPECT_NE(output.find("ColLayout<other::Token> ForeignTypesColumn"), std::string::npos);
    EXPECT_NE(output.find("ColLayout<example::ColLayout> ShadowedTypesColumn"), std::string::npos);
    EXPECT_EQ(output.find("using Base::count_;"), std::string::npos);
    EXPECT_EQ(output.find("this->count_"), std::string::npos);
    EXPECT_EQ(output.find("CompactViewState"), std::string::npos);
    EXPECT_NE(output.find("State* state_{};"), std::string::npos);
    EXPECT_NE(output.find("transfer_n<Overlapping>(dst, src, count);"), std::string::npos);
    EXPECT_NE(output.find("transfer(destination.nested_xs, source.nested_xs);"), std::string::npos);
    EXPECT_EQ(output.find("TArray"), std::string::npos);
    EXPECT_EQ(output.find("FMemory"), std::string::npos);
    EXPECT_EQ(output.find("CoreMinimal"), std::string::npos);
    EXPECT_NE(output.find("using ioj::native_soa::default_construct_n;"), std::string::npos);
    EXPECT_NE(output.find("default_construct_n(columns.nested_xs, count);"), std::string::npos);
    EXPECT_NE(output.find("using ioj::native_soa::source_data;"), std::string::npos);
    EXPECT_NE(output.find("source_data(nested_view.xs())"), std::string::npos);
    EXPECT_NE(output.find("Operations::release_storage(*this);"), std::string::npos);
    EXPECT_NE(output.find("transfer_columns<false>(destination, source, num_);"),
              std::string::npos);
}

TEST(SingleAllocationSoa, RowOperationsUseNestedEquivalentValues) {
    CompilerTestContext context;
    auto const files{render_test_manifest(context, parse_test_manifest(context, R"(
        (module native :header "Native.h"
          (struct Points :equivalent-type Point :layout-only true
            (member xs array float :equivalent-member x) (member ys array float :equivalent-member y))
          (struct Rows
            (member ids array std::int32_t)
            (member points nested Points))))"))};
    auto const& output{files.front().content};
    EXPECT_NE(output.find("void set(size_type index, std::int32_t const new_ids, Point const "
                          "new_points) const"),
              std::string::npos);
    EXPECT_NE(output.find("view_points().set(index, new_points);"), std::string::npos);
    EXPECT_NE(output.find("set(index, (value.x), (value.y));"), std::string::npos);
    EXPECT_NE(output.find("size_type add(std::int32_t const new_ids, Point const new_points)"),
              std::string::npos);
}

auto schemas() -> std::string {
    return R"(
        (struct Child (member small array std::uint8_t) (member wide array Aligned256))
        (struct Rows
          (member ids array std::int32_t)
          (member nested nested Child)))";
}

auto render(std::string_view declarations) -> std::string {
    CompilerTestContext context;
    auto const files{render_test_manifest(
        context,
        parse_test_manifest(context,
                            R"((module test :header "Test.h" :source "Test.cpp" )" +
                                std::string{declarations} + ")"))};
    return files.front().content;
}

auto changed(std::string_view from, std::string_view to) -> std::string {
    auto source{schemas()};
    auto const position{source.find(from)};
    EXPECT_NE(position, std::string::npos);
    source.replace(position, from.size(), to);
    return source;
}

TEST(SingleAllocationSoa, RejectsEmptySchema) {
    EXPECT_THROW(render("(struct Rows)"), std::invalid_argument);
}

TEST(SingleAllocationSoa, RejectsGeneratedNameCollisions) {
    EXPECT_THROW(render(changed("small array", "validate array")), std::invalid_argument);
    EXPECT_THROW(render(schemas() + "(struct RowsView (member value array float))"),
                 std::invalid_argument);
    EXPECT_THROW(render(schemas() + "(struct RowsLayout (member value array float))"),
                 std::invalid_argument);
}

TEST(SingleAllocationSoa, RejectsInvalidFlatteningAndOwnerNames) {
    EXPECT_THROW(render(changed("nested nested Child", "nested nested int")),
                 std::invalid_argument);
    EXPECT_THROW(render(changed("nested nested Child", "nested nested Missing")),
                 std::invalid_argument);
    EXPECT_THROW(render(changed("(member small array std::uint8_t) (member wide array Aligned256)",
                                "(member cycle nested Rows)")),
                 std::invalid_argument);
    EXPECT_THROW(render(changed("nested nested Child", "nested nested Opaque")),
                 std::invalid_argument);
    auto source{schemas()};
    for (auto position{source.find("Child")}; position != std::string::npos;
         position = source.find("Child", position)) {
        source.replace(position, 5, "SingleRowsStorage");
        position += 17;
    }
    EXPECT_NO_THROW(render(source));
    EXPECT_THROW(
        render(changed("(member ids array std::int32_t)",
                       "(member ids array std::int32_t) (member nested_small array std::int32_t)")),
        std::invalid_argument);
    EXPECT_NO_THROW(render(
        changed("(member ids array std::int32_t)",
                "(member ids array std::int32_t) (member layout_start array std::int32_t)")));
    EXPECT_NO_THROW(
        render(changed("(member ids array std::int32_t)",
                       "(member ids array std::int32_t) (member col_layout array std::int32_t)")));
    EXPECT_THROW(render(changed("ids array", "trailing_ array")), std::invalid_argument);
}

TEST(SingleAllocationSoa, FlattensMultipleLevelsAndRepeatedNestedSchemas) {
    auto const output{render(schemas() + R"(
        (struct Grandparent
          (member first nested Rows)
          (member second nested Rows)))")};
    EXPECT_NE(output.find("SecondIdsColumn{FirstNestedWideColumn}"), std::string::npos);
    EXPECT_NE(output.find("GrandparentView_first"), std::string::npos);
    EXPECT_NE(output.find("GrandparentView_second_nested"), std::string::npos);
    EXPECT_NE(output.find("second_nested_view.wide()"), std::string::npos);
}

TEST(SingleAllocationSoa, PreservesCommonApiAndCustomViewNames) {
    CompilerTestContext context;
    auto const files{render_test_manifest(context, parse_test_manifest(context, R"(
        (module api :header "Api.h" :source "Api.cpp"
          (struct LogicalRows :view-name NamedView :const-view-name NamedConstView
            :export-specifier ROWS_API :using-declarations ("Value = float")
            (member values array float)
            (function first float :const true :body #cpp{return $column(values)[0];}cpp#)
            (function external_first float :const true :definition-in-source true
              :body #cpp{return $column(values)[0];}cpp#)
            (mutable-view-function clear_first void :body #cpp{$column(values)[0] = 0;}cpp#)
            (view-function read_first float :const true :body #cpp{return $column(values)[0];}cpp#))))"))};
    auto const& header{files.front().content};
    auto const& implementation{files.back().content};
    EXPECT_NE(header.find("struct ROWS_API LogicalRows"), std::string::npos);
    EXPECT_NE(header.find("struct ROWS_API NamedView"), std::string::npos);
    EXPECT_NE(header.find("struct ROWS_API NamedConstView"), std::string::npos);
    EXPECT_NE(header.find("using Value = float;"), std::string::npos);
    EXPECT_NE(header.find("read_first() const"), std::string::npos);
    EXPECT_NE(header.find("clear_first()"), std::string::npos);
    EXPECT_EQ(header.find("$column("), std::string::npos);
    EXPECT_NE(implementation.find("LogicalRows::external_first() const"), std::string::npos);
}

TEST(SingleAllocationSoa, RejectsUnknownLogicalColumns) {
    EXPECT_THROW(render(R"((struct Rows (member values array float)
        (function broken float :body #cpp{return $column(missing)[0];}cpp#)))"),
                 std::invalid_argument);
}

}
