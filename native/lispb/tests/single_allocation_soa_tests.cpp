#include "compiler_test_context.h"
#include "test_utilities.h"
#include "text_expectations.h"

#include <codegen/ast/cpp_types.h>
#include <codegen/generator.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/schema_version.h>
#include <lispb/compiler_storage.h>
#include <lispb/indices.h>
#include <lispb/schema/source_renderer.h>
#include <lispb/strings.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace single_allocation_tests {
using lispb::codegen::CompilerTestContext;
using lispb::test_support::expect_contains_all;
using lispb::test_support::expect_contains_none;

auto generated_headers(std::span<lispb::codegen::GeneratedFile const> files) -> std::string {
    std::string headers;
    for (auto const& file : files) {
        if (file.path.extension() == ".h") {
            headers += file.content;
        }
    }
    return headers;
}

TEST(SingleAllocationSoa, OrdinarySchemasGenerateNativeOwnersAndViews) {
    CompilerTestContext ctx;
    auto const files{render_test_manifest(ctx, parse_test_manifest(ctx, R"(
        (module native :header "Native.h" :namespace example
          (struct WorldAABBs (member xs array float))
          (struct IDs (member values array std::uint32_t))
          (struct HTTPServer (member values array std::uint32_t))
          (struct Child (member xs array float))
          (struct Rows
            (member ids array std::int32_t)
            (member nested nested Child)
            (member local_types array example::Token)
            (member nested_types array example::sub::Token)
            (member foreign_types array other::Token)
            (member shadowed_types array example::Base))))"))};
    auto const output{generated_headers(files)};
    expect_contains_all(output,
                        {"namespace world_aabbs",
                         "namespace ids",
                         "namespace http_server",
                         "namespace rows",
                         "other::Token",
                         "struct CompactView",
                         "struct CompactConstView"});
    EXPECT_FALSE(output.contains("struct DataPointers"));
}

TEST(SingleAllocationSoa, RowOperationsUseNestedEquivalentValues) {
    CompilerTestContext ctx;
    auto const files{render_test_manifest(ctx, parse_test_manifest(ctx, R"(
        (module native :header "Native.h"
          (struct Points :equivalent-type Point :layout-only true
            (member xs array float :equivalent-member x) (member ys array float :equivalent-member y))
          (struct Rows
            (member ids array std::int32_t)
            (member points nested Points))))"))};
    auto const output{generated_headers(files)};
    expect_contains_all(output,
                        {"equivalent_values(",
                         "func(value.x, value.y)",
                         "logical_columns(",
                         "source.view_points()",
                         "RowTypes<"});
}

auto schemas() -> std::string {
    return R"(
        (struct Child (member small array std::uint8_t) (member wide array Aligned256))
        (struct Rows
          (member ids array std::int32_t)
          (member nested nested Child)))";
}

auto render(std::string_view declarations) -> std::string {
    CompilerTestContext ctx;
    auto const files{render_test_manifest(
        ctx,
        parse_test_manifest(ctx,
                            R"((module test :header "Test.h" :source "Test.cpp" )" +
                                std::string{declarations} + ")"))};
    return generated_headers(files);
}

TEST(SingleAllocationSoa, AuthoredRowFunctionsSuppressAutomaticOverloads) {
    struct Case {
        std::string_view function;
        bool owner_operations;
        bool view_operations;
    };
    for (auto const& test :
         {Case{"(function set void (parameter value float) :body (\"\"))", false, true},
          Case{"(function add void (parameter value float) :body (\"\"))", true, true},
          Case{"(mutable-view-function set void (parameter value float) :body (\"\"))",
               true,
               false}}) {
        SCOPED_TRACE(test.function);
        auto const output{
            render("(struct Rows (member values array float) " + std::string{test.function} + ")")};
        EXPECT_EQ(output.contains("OwnerRowOperations<"), test.owner_operations);
        EXPECT_EQ(output.contains("ViewRowOperations<"), test.view_operations);
    }
}

auto changed(std::string_view from, std::string_view to) -> std::string {
    auto source{schemas()};
    auto const position{source.find(from)};
    EXPECT_NE(position, std::string::npos);
    source.replace(position, from.size(), to);
    return source;
}

class InvalidSoaDeclaration : public testing::TestWithParam<std::string> {};

TEST_P(InvalidSoaDeclaration, RejectsDeclaration) {
    EXPECT_THROW(render(GetParam()), std::invalid_argument);
}

INSTANTIATE_TEST_SUITE_P(EmptySchema, InvalidSoaDeclaration, testing::Values("(struct Rows)"));

TEST(SingleAllocationSoa, PartialAppendSurvivesCloningAndSourceRoundTrip) {
    CompilerTestContext ctx;
    auto manifest{parse_test_manifest(ctx, R"(
        (module test :header "Test.h"
          (struct Rows
            (member values array float)
            (function append std::uint32_t (parameter value float) :append-columns (values))
            (function append_empty std::uint32_t :append-columns ()))))")};
    auto& declarations{manifest.declarations};
    auto& functions{declarations.functions};
    auto const original{
        lispb::schema::render_module_source(declarations, manifest.modules.front())};
    auto& soas{declarations.soas};
    auto const methods{declarations.items(soas.functions.front())};
    std::vector<lispb::codegen::FunctionSchemaId> clones;
    for (auto const method : methods) {
        clones.push_back(functions.clone(method));
    }
    soas.functions.front() = declarations.range(clones);
    EXPECT_EQ(lispb::schema::render_module_source(declarations, manifest.modules.front()),
              original);
    CompilerTestContext reparsed;
    auto const round_trip{parse_test_manifest(reparsed, original)};
    EXPECT_EQ(
        lispb::schema::render_module_source(round_trip.declarations, round_trip.modules.front()),
        original);
    auto const& options{round_trip.declarations.functions.append_columns};
    ASSERT_EQ(options.size(), 2U);
    ASSERT_TRUE(options[0]);
    ASSERT_TRUE(options[1]);
    EXPECT_EQ(options[0]->size(), 1U);
    EXPECT_TRUE(options[1]->empty());
}

class InvalidPartialAppend : public testing::TestWithParam<char const*> {};

TEST_P(InvalidPartialAppend, RejectsDeclaration) {
    EXPECT_THROW(
        render(std::string{"(struct Child (member xs array float)) (struct Rows (member values "
                           "array float) (member nested nested Child) "} +
               GetParam() + ")"),
        std::exception);
}

INSTANTIATE_TEST_SUITE_P(
    SingleAllocationSoa,
    InvalidPartialAppend,
    testing::Values(
        "(function append int (parameter value float) :append-columns ())",
        ("(function append int (parameter a float) (parameter b float) :append-columns (values "
         "values))"),
        "(function append int (parameter value float) :append-columns (missing))",
        "(function append int (parameter value float) :append-columns (nested))",
        "(function append int :const true :append-columns ())",
        "(function append int :static true :append-columns ())",
        "(function append int :constexpr true :append-columns ())",
        "(function append int :body (\"return 0;\") :append-columns ())",
        "(function append void :append-columns ())",
        ("(function append int (parameter value (type-ref float :suffix \" const&\")) "
         ":append-columns (values))"),
        "(view-function append int :append-columns ())"));

struct InvalidSoaMutation {
    std::string_view from;
    std::string_view to;
};

class InvalidSoaMember : public testing::TestWithParam<InvalidSoaMutation> {};

TEST_P(InvalidSoaMember, RejectsDeclaration) {
    auto const& test{GetParam()};
    SCOPED_TRACE(test.to);
    EXPECT_THROW(render(changed(test.from, test.to)), std::invalid_argument);
}

INSTANTIATE_TEST_SUITE_P(
    FlatteningAndGeneratedNames,
    InvalidSoaMember,
    testing::Values(InvalidSoaMutation{"small array", "validate array"},
                    InvalidSoaMutation{"nested nested Child", "nested nested int"},
                    InvalidSoaMutation{"nested nested Child", "nested nested Missing"},
                    InvalidSoaMutation{
                        "(member small array std::uint8_t) (member wide array Aligned256)",
                        "(member cycle nested Rows)"},
                    InvalidSoaMutation{"nested nested Child", "nested nested Opaque"},
                    InvalidSoaMutation{
                        "(member ids array std::int32_t)",
                        "(member ids array std::int32_t) (member nested_small array std::int32_t)"},
                    InvalidSoaMutation{"ids array", "trailing_ array"}));

INSTANTIATE_TEST_SUITE_P(
    GeneratedNameCollisions,
    InvalidSoaDeclaration,
    testing::Values(
        "(struct Counts (member column_count array std::uint32_t))",
        "(struct Counts (member count array std::uint32_t))"
        "(struct Rows (member column nested Counts))",
        "(struct Counts (member count array std::uint32_t))"
        "(struct Rows :layout-only true (member column nested Counts))",
        schemas() + "(struct rows (member value array float))",
        "(struct FooBar (member value array float)) (struct Foo_Bar (member value array float))"));

TEST(SingleAllocationSoa, AllowsNonconflictingOwnerAndMemberNames) {
    EXPECT_NO_THROW(render(schemas() + "(struct RowsView (member value array float))"));
    auto source{schemas()};
    for (auto position{source.find("Child")}; position != std::string::npos;
         position = source.find("Child", position)) {
        source.replace(position, 5, "SingleRowsStorage");
        position += 17;
    }
    EXPECT_NO_THROW(render(source));
    EXPECT_NO_THROW(render(
        changed("(member ids array std::int32_t)",
                "(member ids array std::int32_t) (member layout_start array std::int32_t)")));
    EXPECT_NO_THROW(
        render(changed("(member ids array std::int32_t)",
                       "(member ids array std::int32_t) (member col_layout array std::int32_t)")));
}

class ConflictingSoaModuleDeclaration : public testing::TestWithParam<char const*> {};

TEST_P(ConflictingSoaModuleDeclaration, RejectsSharedViewTypeCollisions) {
    CompilerTestContext ctx;
    auto const manifest{parse_test_manifest(ctx, std::string{R"(
        (module owner :header "Owner.h" :namespace example
          (struct Rows (member values array float)))
        (module collision :header "Collision.h" :namespace example::rows
          (record )"} + GetParam() + " (projections (cpp))))")};
    EXPECT_THROW(render_test_manifest(ctx, manifest), std::invalid_argument);
}

INSTANTIATE_TEST_SUITE_P(SingleAllocationSoa,
                         ConflictingSoaModuleDeclaration,
                         testing::Values("CompactView", "Pointers", "ColumnTypes"));

TEST(SingleAllocationSoa, FlattensMultipleLevelsAndRepeatedNestedSchemas) {
    auto const output{render(schemas() + R"(
        (struct Grandparent
          (member first nested Rows)
          (member second nested Rows)))")};
    expect_contains_all(output, {"first_nested_wide_column{", "second_nested_wide_column{"});
    expect_contains_none(
        output, {"last_column()", "GrandparentView_first", "GrandparentView_second_nested"});
    expect_contains_all(output, {"rows::Schema", "child::Schema", "second_nested_view.wide()"});
}

TEST(SingleAllocationSoa, PreservesCommonApiInSchemaNamespace) {
    CompilerTestContext ctx;
    auto const files{render_test_manifest(ctx, parse_test_manifest(ctx, R"(
        (module api :header "Api.h" :source "Api.cpp"
          (struct LogicalRows
            :export-specifier ROWS_API :using-declarations ("Value = float")
            (member values array float)
            (function first float :const true :body #cpp{return $column(values)[0];}cpp#)
            (function external_first float :const true :definition-in-source true
              :body #cpp{return $column(values)[0];}cpp#)
            (mutable-view-function clear_first void :body #cpp{$column(values)[0] = 0;}cpp#)
            (view-function read_first float :const true :body #cpp{return $column(values)[0];}cpp#))))"))};
    auto const header{generated_headers(files)};
    ASSERT_EQ(files.at(1).path, "Api.cpp");
    auto const& implementation{files.at(1).content};
    expect_contains_all(header,
                        {"struct ROWS_API LogicalRows",
                         "namespace logical_rows",
                         "struct ROWS_API CompactView",
                         "struct ROWS_API CompactConstView",
                         "using Value = float;",
                         "read_first() const",
                         "clear_first()"});
    EXPECT_FALSE(header.contains("$column("));
    EXPECT_TRUE(implementation.contains("LogicalRows::external_first() const"));
}

INSTANTIATE_TEST_SUITE_P(UnknownLogicalColumns,
                         InvalidSoaDeclaration,
                         testing::Values(R"((struct Rows (member values array float)
        (function broken float :body #cpp{return $column(missing)[0];}cpp#)))"));

}
