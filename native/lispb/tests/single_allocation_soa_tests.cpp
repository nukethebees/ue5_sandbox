#include "compiler_test_context.h"
#include "test_utilities.h"

#include <codegen/ast/cpp_types.h>
#include <codegen/generator.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/schema_version.h>
#include <codegen/schema/soa_backend.h>
#include <codegen/schema/soa_storage.h>
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
using lispb::codegen::SoaBackend;
using lispb::codegen::SoaStorage;

TEST(SingleAllocationSoa, StdlibBackendReusesLayoutWithoutUnrealDependencies) {
    CompilerTestContext context;
    auto const files{render_test_manifest(context, parse_test_manifest(context, R"(
        (module native :header "Native.h" :namespace example :backend standard-library
          (struct Child (member xs array float))
          (struct Rows
            (member ids array std::int32_t)
            (member nested nested Child :nested-schema Child)
            (member local_types array example::Token)
            (member nested_types array example::sub::Token)
            (member foreign_types array other::Token)
            (member shadowed_types array example::Layout)
            (single-allocation SingleRows))))"))};
    auto const& output{files.front().content};
    EXPECT_EQ(output.find("struct RowsView"), std::string::npos);
    EXPECT_EQ(output.find("struct RowsConstView"), std::string::npos);
    EXPECT_NE(output.find("ColLayout<std::int32_t> IdsColumn"), std::string::npos);
    EXPECT_NE(output.find("auto ids() const"), std::string::npos);
    EXPECT_NE(output.find("NestedXsColumn{IdsColumn}"), std::string::npos);
    EXPECT_NE(output.find("ColLayout<Token> LocalTypesColumn"), std::string::npos);
    EXPECT_NE(output.find("ColLayout<sub::Token> NestedTypesColumn"), std::string::npos);
    EXPECT_NE(output.find("ColLayout<other::Token> ForeignTypesColumn"), std::string::npos);
    EXPECT_NE(output.find("ColLayout<example::Layout> ShadowedTypesColumn"), std::string::npos);
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

TEST(SingleAllocationSoa, StdlibRowOperationsUseNestedEquivalentValues) {
    CompilerTestContext context;
    auto const files{render_test_manifest(context, parse_test_manifest(context, R"(
        (module native :header "Native.h" :backend standard-library
          (struct Points :equivalent-type Point :layout-only true
            (member xs array float) (member ys array float))
          (struct Rows
            (member ids array std::int32_t)
            (member points nested Points :nested-schema Points))))"))};
    auto const& output{files.front().content};
    EXPECT_NE(output.find("void set(size_type const index, std::int32_t const new_ids, Point const "
                          "new_points) const"),
              std::string::npos);
    EXPECT_NE(output.find("points.set(index, new_points);"), std::string::npos);
    EXPECT_NE(
        output.find("auto add(std::int32_t const new_ids, Point const new_points) -> size_type"),
        std::string::npos);
    EXPECT_NE(output.find("ids.emplace_back(new_ids);"), std::string::npos);
    EXPECT_NE(output.find("points.add(new_points);"), std::string::npos);
    EXPECT_EQ(output.find("add_defaulted(1);"), std::string::npos);
}

auto schemas() -> std::string {
    return R"(
        (struct Child (member small array uint8) (member wide array Aligned256))
        (struct Rows
          (member ids array int32)
          (member nested nested Child :nested-schema Child)
          (single-allocation SingleRows)))";
}

auto render(std::string_view declarations) -> std::string {
    CompilerTestContext context;
    auto const files{render_test_manifest(
        context,
        parse_test_manifest(context,
                            "(module test :header \"Test.h\" :source \"Test.cpp\" " +
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

TEST(SingleAllocationSoa, AllocatorVariantsApplyToNestedColumns) {
    auto source{changed("(struct Rows", "(struct Rows :storage both")};
    source +=
        "(array-allocator Test TestArrayAllocator) (array-allocator Other OtherArrayAllocator)";
    auto const output{render(source)};
    EXPECT_NE(output.find("TArray<uint8, TestArrayAllocator> small;"), std::string::npos);
    EXPECT_NE(output.find("TArray<int32, TestArrayAllocator> ids;"), std::string::npos);
    EXPECT_NE(output.find("TestChild nested;"), std::string::npos);
    EXPECT_NE(output.find("TArray<Aligned256, OtherArrayAllocator> wide;"), std::string::npos);
    EXPECT_NE(output.find("OtherChild nested;"), std::string::npos);
    EXPECT_NE(output.find("TArray<uint8> small;"), std::string::npos);
    EXPECT_EQ(output.find("TestSingleRows"), std::string::npos);
    source += "(array-allocator Test OtherArrayAllocator)";
    EXPECT_THROW(render(source), std::invalid_argument);
}

TEST(SingleAllocationSoa, ArrayAllocatorVariantsSkipSingleOnlySchemas) {
    auto const output{render(schemas() + "(array-allocator Custom CustomAllocator)")};
    EXPECT_NE(output.find("struct CustomChild"), std::string::npos);
    EXPECT_EQ(output.find("struct CustomRows"), std::string::npos);
    EXPECT_NE(output.find("struct SingleRows"), std::string::npos);
}

TEST(SingleAllocationSoa, RejectsEmptySchema) {
    EXPECT_THROW(render("(struct Rows (single-allocation SingleRows))"), std::invalid_argument);
}

TEST(SingleAllocationSoa, RejectsCompactViewNameCollisions) {
    EXPECT_THROW(render(changed("small array", "validate array")), std::invalid_argument);
    EXPECT_THROW(
        render(changed("single-allocation SingleRows", "single-allocation RowsSingleLayout")),
        std::invalid_argument);
    EXPECT_THROW(
        render(changed("single-allocation SingleRows", "single-allocation RowsSingleConstView")),
        std::invalid_argument);
    EXPECT_THROW(render(changed("(member ids array int32)",
                                "(member ids array int32) (member view_nested array float)")),
                 std::invalid_argument);
    auto source{schemas()};
    for (auto position{source.find("Child")}; position != std::string::npos;
         position = source.find("Child", position)) {
        source.replace(position, 5, "RowsSingleView_nested");
        position += 21;
    }
    EXPECT_THROW(render(source), std::invalid_argument);
}
TEST(SingleAllocationSoa, RejectsInvalidFlatteningAndOwnerNames) {
    EXPECT_THROW(render(changed(":nested-schema Child", "")), std::invalid_argument);
    EXPECT_THROW(render(changed(":nested-schema Child", ":nested-schema Missing")),
                 std::invalid_argument);
    EXPECT_THROW(render(changed("(member small array uint8) (member wide array Aligned256)",
                                "(member cycle nested Rows :nested-schema Rows)")),
                 std::invalid_argument);
    EXPECT_THROW(render(changed("nested nested Child", "nested nested Opaque")),
                 std::invalid_argument);
    EXPECT_THROW(render(changed("single-allocation SingleRows", "single-allocation ChildView")),
                 std::invalid_argument);
    auto source{schemas()};
    for (auto position{source.find("Child")}; position != std::string::npos;
         position = source.find("Child", position)) {
        source.replace(position, 5, "SingleRowsStorage");
        position += 17;
    }
    EXPECT_NO_THROW(render(source));
    EXPECT_THROW(render(changed("(member ids array int32)",
                                "(member ids array int32) (member nested_small array int32)")),
                 std::invalid_argument);
    EXPECT_NO_THROW(render(changed("(member ids array int32)",
                                   "(member ids array int32) (member layout_start array int32)")));
    EXPECT_NO_THROW(render(changed("(member ids array int32)",
                                   "(member ids array int32) (member col_layout array int32)")));
    EXPECT_THROW(render(changed("(member ids array int32)",
                                "(member ids array int32 :nested-schema Child)")),
                 std::invalid_argument);
    EXPECT_THROW(render(changed("ids array", "trailing_ array")), std::invalid_argument);
}

TEST(SingleAllocationSoa, FlattensMultipleLevelsAndRepeatedNestedSchemas) {
    auto const output{render(schemas() + R"(
        (struct Grandparent
          (member first nested Rows :nested-schema Rows)
          (member second nested Rows :nested-schema Rows)
          (single-allocation SingleGrandparent)))")};
    EXPECT_NE(output.find("SecondIdsColumn{FirstNestedWideColumn}"), std::string::npos);
    EXPECT_NE(output.find("GrandparentSingleView_first"), std::string::npos);
    EXPECT_NE(output.find("GrandparentSingleView_second_nested"), std::string::npos);
    EXPECT_NE(output.find("second_nested_view.wide()"), std::string::npos);
}

TEST(SingleAllocationSoa, CommonApiIsEmittedForEveryRequestedRepresentation) {
    for (auto const backend : {SoaBackend::unreal, SoaBackend::standard_library}) {
        for (auto const policy :
             {SoaStorage::vector, SoaStorage::single_allocation, SoaStorage::both}) {
            SCOPED_TRACE(static_cast<int>(backend));
            SCOPED_TRACE(soa_storage_name(policy));
            std::string source{"(module api :header \"Api.h\" :source \"Api.cpp\" :backend "};
            source += backend == SoaBackend::unreal ? "unreal" : "standard-library";
            source += R"(
                (struct LogicalRows :view-name NamedView :const-view-name NamedConstView
                  :operations (set-num) :export-specifier ROWS_API :using-declarations ("Value = float")
                  :storage )";
            source += soa_storage_name(policy);
            source += R"(
                  (member values array float)
                  (function first float :const true :body #cpp{return $column(values)[0];}cpp#)
                  (function external_first float :const true :definition-in-source true :body #cpp{return $column(values)[0];}cpp#)
                  (mutable-view-function clear_first void :body #cpp{$column(values)[0] = 0;}cpp#)
                  (view-function read_first float :const true :body #cpp{return $column(values)[0];}cpp#))";
            if (policy != SoaStorage::vector) {
                source += "(single-allocation CompactRows)";
            }
            source += "))";
            CompilerTestContext context;
            auto const files{render_test_manifest(context, parse_test_manifest(context, source))};
            auto const& header{files.front().content};
            auto const& implementation{files.back().content};
            EXPECT_NE(header.find("struct ROWS_API NamedView"), std::string::npos);
            EXPECT_NE(header.find("struct ROWS_API NamedConstView"), std::string::npos);
            EXPECT_NE(header.find("using Value = float;"), std::string::npos);
            EXPECT_NE(header.find("read_first() const"), std::string::npos);
            EXPECT_NE(header.find("clear_first()"), std::string::npos);
            EXPECT_EQ(header.find("$column("), std::string::npos);
            if (policy != SoaStorage::vector) {
                EXPECT_NE(header.find("struct ROWS_API CompactRows"), std::string::npos);
                EXPECT_NE(implementation.find("CompactRows::external_first() const"),
                          std::string::npos);
                EXPECT_EQ(header.find("using Operations::"), std::string::npos);
                EXPECT_NE(header.find("void copy_columns_from("), std::string::npos);
            }
            if (policy == SoaStorage::single_allocation) {
                EXPECT_EQ(header.find("struct LogicalRows {"), std::string::npos);
                EXPECT_EQ(header.find("LogicalRowsSingleViewImpl"), std::string::npos);
            }
            if (policy == SoaStorage::both) {
                EXPECT_NE(header.find("struct ROWS_API LogicalRowsSingleView"), std::string::npos);
                EXPECT_NE(implementation.find("LogicalRows::external_first() const"),
                          std::string::npos);
            }
        }
    }
}

TEST(SingleAllocationSoa, RejectsIncompatibleRepresentationSettingsWithLocalDiagnostics) {
    auto check = [](std::string_view options, std::string_view extra, std::string_view message) {
        try {
            render("(struct Rows " + std::string{options} + " (member values array float) " +
                   std::string{extra} + ")");
            FAIL() << "Expected rejection containing " << message;
        } catch (std::invalid_argument const& error) {
            EXPECT_NE(std::string_view{error.what()}.find(message), std::string_view::npos)
                << error.what();
        }
    };
    check(":array-allocator Allocator",
          "(single-allocation Owner)",
          "array-allocator requires vector storage");
    check(":copy-element-memberwise true",
          "(single-allocation Owner)",
          "single-allocation leaves use trivial copying");
    check(":storage vector", "(single-allocation Owner)", "storage vector cannot declare");
    check(":layout-only true", "(single-allocation Owner)", "layout-only cannot declare");
    check(":storage single-allocation", "", "requires a single-allocation owner");
    check(":storage both", "", "requires a single-allocation owner");
    check("",
          "(single-allocation Owner) (function broken float :body #cpp{return "
          "$column(missing)[0];}cpp#)",
          "has no column 'missing'");
}

}
