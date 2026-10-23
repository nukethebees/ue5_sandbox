#include "compiler_test_context.h"
#include "test_utilities.h"

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
    EXPECT_NE(output.find("namespace rows"), std::string::npos);
    EXPECT_NE(output.find("struct View"), std::string::npos);
    EXPECT_NE(output.find("struct ConstView"), std::string::npos);
    EXPECT_NE(
        output.find("ColumnLayouts<std::int32_t, float, Token, sub::Token, other::Token, Base>"),
        std::string::npos);
    EXPECT_NE(output.find("Element<example::Base>* shadowed_types"), std::string::npos);
    EXPECT_NE(output.find("auto ids() const"), std::string::npos);
    EXPECT_NE(output.find("ids_column{0}"), std::string::npos);
    EXPECT_NE(output.find("nested_xs_column{1}"), std::string::npos);
    EXPECT_EQ(output.find("LayoutColumns"), std::string::npos);
    EXPECT_NE(output.find("CompactViewBase<Const>"), std::string::npos);
    EXPECT_EQ(output.find("State* state_{};"), std::string::npos);
    EXPECT_EQ(output.find("transfer_columns("), std::string::npos);
    EXPECT_EQ(output.find("TArray"), std::string::npos);
    EXPECT_EQ(output.find("FMemory"), std::string::npos);
    EXPECT_EQ(output.find("CoreMinimal"), std::string::npos);
    EXPECT_EQ(output.find("void default_construct_columns("), std::string::npos);
    EXPECT_EQ(output.find("source_pointers("), std::string::npos);
    EXPECT_NE(output.find("with_columns(Columns const& source, Func&& func)"), std::string::npos);
    EXPECT_NE(output.find("return func(source.ids(), nested_view.xs(),"), std::string::npos);
    EXPECT_NE(output.find("using resolved_view_type ="), std::string::npos);
    EXPECT_NE(output.find("Operations::release_storage(*this);"), std::string::npos);
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

TEST(SingleAllocationSoa, RejectsInvalidPartialAppendDeclarations) {
    for (auto const function :
         {"(function append int (parameter value float) :append-columns ())",
          "(function append int (parameter a float) (parameter b float) :append-columns (values "
          "values))",
          "(function append int (parameter value float) :append-columns (missing))",
          "(function append int (parameter value float) :append-columns (nested))",
          "(function append int :const true :append-columns ())",
          "(function append int :static true :append-columns ())",
          "(function append int :constexpr true :append-columns ())",
          "(function append int :body (\"return 0;\") :append-columns ())",
          "(function append void :append-columns ())",
          "(function append int (parameter value (type-ref float :suffix \" const&\")) "
          ":append-columns (values))",
          "(view-function append int :append-columns ())"}) {
        SCOPED_TRACE(function);
        EXPECT_THROW(
            render(std::string{"(struct Child (member xs array float)) (struct Rows (member values "
                               "array float) (member nested nested Child) "} +
                   function + ")"),
            std::exception);
    }
}

TEST(SingleAllocationSoa, RejectsGeneratedNameCollisions) {
    EXPECT_THROW(render(changed("small array", "validate array")), std::invalid_argument);
    EXPECT_THROW(render("(struct Counts (member column_count array std::uint32_t))"),
                 std::invalid_argument);
    for (auto const options : {"", ":layout-only true"}) {
        SCOPED_TRACE(options);
        EXPECT_THROW(render(std::string{"(struct Counts (member count array std::uint32_t))"
                                        "(struct Rows "} +
                            options + " (member column nested Counts))"),
                     std::invalid_argument);
    }
    EXPECT_THROW(render(schemas() + "(struct rows (member value array float))"),
                 std::invalid_argument);
    EXPECT_THROW(render("(struct FooBar (member value array float))"
                        "(struct Foo_Bar (member value array float))"),
                 std::invalid_argument);
    EXPECT_NO_THROW(render(schemas() + "(struct RowsView (member value array float))"));
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

TEST(SingleAllocationSoa, RejectsModuleDeclarationsCollidingWithSharedViewTypes) {
    CompilerTestContext context;
    auto const manifest{parse_test_manifest(context, R"(
        (module owner :header "Owner.h" :namespace example
          (struct Rows (member values array float)))
        (module collision :header "Collision.h" :namespace example::rows
          (record View)))")};
    EXPECT_THROW(render_test_manifest(context, manifest), std::invalid_argument);
}

TEST(SingleAllocationSoa, FlattensMultipleLevelsAndRepeatedNestedSchemas) {
    auto const output{render(schemas() + R"(
        (struct Grandparent
          (member first nested Rows)
          (member second nested Rows)))")};
    EXPECT_NE(output.find("second_nested_wide_column{"), std::string::npos);
    EXPECT_NE(output.find("Layout::second_nested_wide_column"), std::string::npos);
    EXPECT_EQ(output.find("last_column()"), std::string::npos);
    EXPECT_EQ(output.find("GrandparentView_first"), std::string::npos);
    EXPECT_EQ(output.find("GrandparentView_second_nested"), std::string::npos);
    EXPECT_NE(output.find("rows::ResolvedView"), std::string::npos);
    EXPECT_NE(output.find("child::ResolvedView"), std::string::npos);
    EXPECT_NE(output.find("second_nested_view.wide()"), std::string::npos);
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
    EXPECT_NE(header.find("struct ROWS_API LogicalRows"), std::string::npos);
    EXPECT_NE(header.find("namespace logical_rows"), std::string::npos);
    EXPECT_NE(header.find("struct ROWS_API View"), std::string::npos);
    EXPECT_NE(header.find("struct ROWS_API ConstView"), std::string::npos);
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
