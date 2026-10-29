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
#include <stdexcept>
#include <string>
#include <string_view>

namespace single_allocation_tests {
using lispb::codegen::CompilerTestContext;
using lispb::test_support::expect_contains_all;
using lispb::test_support::expect_contains_none;

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
            (member shadowed_types array example::Base))))"))};
    auto const& output{files.front().content};
    expect_contains_all(
        output,
        {"namespace rows",
         "struct View",
         "struct ConstView",
         "ColumnLayouts<std::int32_t, float, Token, sub::Token, other::Token, Base>",
         "Element<Base>* shadowed_types",
         "struct DataPointers",
         "ResolvedViewBase<Layout::column_types, DataPointers, Const>"});
    EXPECT_FALSE(output.contains("make_data_unchecked("));
    expect_contains_all(output, {"auto ids() const", "ids_column{0}", "nested_xs_column{1}"});
    EXPECT_FALSE(output.contains("LayoutColumns"));
    EXPECT_TRUE(output.contains("CompactViewBase<Const>"));
    expect_contains_none(output,
                         {"State* state_{};",
                          "transfer_columns(",
                          "TArray",
                          "FMemory",
                          "CoreMinimal",
                          "void default_construct_columns(",
                          "source_pointers("});
    expect_contains_all(output,
                        {"with_columns(Columns const& source, Func&& func)",
                         "return func(source.ids(), nested_view.xs(),",
                         "using resolved_view_type =",
                         "Operations::release_storage(*this);"});
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
    expect_contains_all(output,
                        {"void set(size_type index, std::int32_t const new_ids, Point const "
                         "new_points) const",
                         "view_points().set(index, new_points);",
                         "set(index, (value.x), (value.y));",
                         "size_type add(std::int32_t const new_ids, Point const new_points)",
                         "using Base::set;",
                         "using RowOperations::add;"});
    EXPECT_FALSE(output.contains("points().xs()[index] = new_points_xs;"));
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

TEST(SingleAllocationSoa, AuthoredRowFunctionsSuppressAutomaticOverloads) {
    struct Case {
        std::string_view function;
        bool owner_operations;
        bool view_operations;
        bool automatic_add;
    };
    for (auto const& test :
         {Case{"(function set void (parameter value float) :body (\"\"))", false, true, false},
          Case{"(function add void (parameter value float) :body (\"\"))", true, true, false},
          Case{"(mutable-view-function set void (parameter value float) :body (\"\"))",
               true,
               false,
               true}}) {
        SCOPED_TRACE(test.function);
        auto const output{
            render("(struct Rows (member values array float) " + std::string{test.function} + ")")};
        EXPECT_EQ(output.contains("OwnerRowOperations<"), test.owner_operations);
        EXPECT_EQ(output.contains("ViewRowOperations<"), test.view_operations);
        EXPECT_EQ(output.contains("using RowOperations::add;"), test.automatic_add);
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
    CompilerTestContext context;
    auto manifest{parse_test_manifest(context, R"(
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
    CompilerTestContext context;
    auto const manifest{parse_test_manifest(context, std::string{R"(
        (module owner :header "Owner.h" :namespace example
          (struct Rows (member values array float)))
        (module collision :header "Collision.h" :namespace example::rows
          (record )"} + GetParam() + " (projections (cpp))))")};
    EXPECT_THROW(render_test_manifest(context, manifest), std::invalid_argument);
}

INSTANTIATE_TEST_SUITE_P(SingleAllocationSoa,
                         ConflictingSoaModuleDeclaration,
                         testing::Values("View", "DataPointers"));

TEST(SingleAllocationSoa, FlattensMultipleLevelsAndRepeatedNestedSchemas) {
    auto const output{render(schemas() + R"(
        (struct Grandparent
          (member first nested Rows)
          (member second nested Rows)))")};
    expect_contains_all(output,
                        {"second_nested_wide_column{", "Layout::second_nested_wide_column"});
    expect_contains_none(
        output, {"last_column()", "GrandparentView_first", "GrandparentView_second_nested"});
    expect_contains_all(output,
                        {"rows::ResolvedView", "child::ResolvedView", "second_nested_view.wide()"});
}

TEST(SingleAllocationSoa, PreservesCommonApiInSchemaNamespace) {
    CompilerTestContext context;
    auto const files{render_test_manifest(context, parse_test_manifest(context, R"(
        (module api :header "Api.h" :source "Api.cpp"
          (struct LogicalRows
            :export-specifier ROWS_API :using-declarations ("Value = float")
            (member values array float)
            (function first float :const true :body #cpp{return $column(values)[0];}cpp#)
            (function external_first float :const true :definition-in-source true
              :body #cpp{return $column(values)[0];}cpp#)
            (mutable-view-function clear_first void :body #cpp{$column(values)[0] = 0;}cpp#)
            (view-function read_first float :const true :body #cpp{return $column(values)[0];}cpp#))))"))};
    auto const& header{files.front().content};
    auto const& implementation{files.back().content};
    expect_contains_all(header,
                        {"struct ROWS_API LogicalRows",
                         "namespace logical_rows",
                         "struct ROWS_API View",
                         "struct ROWS_API ConstView",
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
