#include "compiler_test_context.h"
#include "test_utilities.h"

#include <codegen/ast/cpp_types.h>
#include <codegen/ast/indices.h>
#include <codegen/generator.h>
#include <codegen/schema.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/schema_version.h>
#include <lispb/compiler_storage.h>
#include <lispb/indices.h>
#include <lispb/schema/type_graph.h>
#include <lispb/strings.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace lispb::codegen {
namespace {

TEST(RecordLowering, GeneratesSemanticMembersAndFixedArrays) {
    CompilerTestContext context;
    auto manifest{parse_test_manifest(
        context,
        R"(
        (module records :header "Records.h" :namespace project
            (record Position (member x float) (member y float))
            (record Sample :export-specifier PROJECT_API
                (member position Position) (member values @value :count 3))))",
        {{"value",
          context.storage.cpp_types.add(
              "Value", {context.storage.cpp_types.dependency("Value", "Project/Value.h")})}})};

    auto const files{render_test_manifest(context, manifest)};

    ASSERT_EQ(files.size(), 1U);
    auto const& output{files.front().content};
    EXPECT_NE(output.find("#include <array>"), std::string::npos);
    EXPECT_NE(output.find("#include \"Project/Value.h\""), std::string::npos);
    EXPECT_NE(output.find("namespace project"), std::string::npos);
    EXPECT_NE(output.find("struct Position"), std::string::npos);
    EXPECT_NE(output.find("float x;"), std::string::npos);
    EXPECT_NE(output.find("struct PROJECT_API Sample"), std::string::npos);
    EXPECT_NE(output.find("Position position;"), std::string::npos);
    EXPECT_NE(output.find("std::array<Value, 3> values;"), std::string::npos);
}

TEST(RecordLowering, AllowsEmptyAndRejectsDuplicateAndZeroCountMembers) {
    CompilerTestContext context;
    auto record = [&](std::string_view members) {
        return parse_test_manifest(context,
                                   R"((module records :header "Records.h" (record Record )" +
                                       std::string{members} + "))");
    };
    EXPECT_NO_THROW(
        static_cast<void>(lower_test_manifest(context, record("(member value float)"))));
    auto const files{render_test_manifest(context, record(""))};
    ASSERT_EQ(files.size(), 1U);
    EXPECT_NE(files.front().content.find("struct Record"), std::string::npos);
    EXPECT_THROW(static_cast<void>(lower_test_manifest(
                     context, record("(member value float) (member value float)"))),
                 std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(lower_test_manifest(context, record("(member values float :count 0)"))),
        std::invalid_argument);
}

TEST(RecordLowering, EmitsByValueDependenciesBeforeTheirUsers) {
    CompilerTestContext context;
    auto manifest{parse_test_manifest(context, R"(
        (module records :header "Records.h"
            (record Sample (member position Position))
            (record Position (member x float))))")};

    auto const files{render_test_manifest(context, manifest)};
    auto const& output{files.front().content};
    EXPECT_LT(output.find("struct Position"), output.find("struct Sample"));
}

TEST(RecordLowering, ForwardDeclaresMutuallyReferringPointerRecords) {
    CompilerTestContext context;
    auto manifest{parse_test_manifest(context, R"(
        (module records :header "Records.h"
            (record First (member next (type-ref Second :suffix "*")))
            (record Second (member next (type-ref First :suffix "*")))))")};
    auto const files{render_test_manifest(context, manifest)};
    auto const& output{files.front().content};
    EXPECT_NE(output.find("struct First;"), std::string::npos);
    EXPECT_NE(output.find("struct Second;"), std::string::npos);
    EXPECT_NE(output.find("Second* next;"), std::string::npos);
    EXPECT_NE(output.find("First* next;"), std::string::npos);
}

TEST(RecordLowering, SoaPointerCyclesRetainSemanticEdgesInEitherSourceOrder) {
    CompilerTestContext context;
    std::array<std::string_view, 2> declarations{
        R"((struct A
            (member others array (type-ref B :suffix "*"))))",
        R"((struct B
            (member others array (type-ref A :suffix "*"))))"};
    for (int order{}; order < 2; ++order) {
        auto manifest{parse_test_manifest(
            context,
            R"((module pointers :header "Pointers.h" :namespace example::inner )" +
                std::string{declarations[0]} + std::string{declarations[1]} + ")")};
        auto const graph{schema::resolve_type_graph(manifest)};
        for (auto const& [name, other] : {std::pair{"A", "B"}, std::pair{"B", "A"}}) {
            auto const dependencies{
                graph.dependencies_of(*find_test_type(graph, "pointers", name))};
            EXPECT_NE(std::ranges::find(dependencies, *find_test_type(graph, "pointers", other)),
                      dependencies.end());
        }
        auto const output{render_test_manifest(context, manifest).front().content};
        EXPECT_LT(output.find("namespace example::inner"), output.find("struct A;"));
        auto const view_definition{output.find("struct ViewImpl")};
        ASSERT_NE(view_definition, std::string::npos);
        EXPECT_LT(output.find("struct A;"), view_definition);
        EXPECT_LT(output.find("struct B;"), view_definition);
        EXPECT_EQ(output.find("struct A;", output.find("struct A;") + 1), std::string::npos);
        std::ranges::reverse(declarations);
    }
}

TEST(RecordLowering, ForwardDeclarationsMatchGeneratedRecordKindsAndExcludeAliasesAndExternals) {
    CompilerTestContext context;
    std::string source{
        R"((module kinds :header "Kinds.h" :source "Kinds.cpp" :header-include "Kinds.h"
        (record Uses )"};
    for (auto const name : {"Record",
                            "Raw",
                            "Tagged",
                            "Packed",
                            "Table",
                            "Facade",
                            "Values",
                            "Alias",
                            "Tag",
                            "External"}) {
        source += "(member p" + std::string{name} + " (type-ref " + name + R"( :suffix "*")))";
    }
    source += R"((member facade_reference (type-ref Facade :suffix "&")))
        (struct LayoutOnly :layout-only true (member values array float))
        (record Record)
        (union Raw (alternative value float))
        (enum Tag std::uint8_t (value Value))
        (tagged-union Tagged :discriminant Tag (alternative value float :tag Value))
        (packed-value Packed :storage std::uint8_t (field value std::uint8_t :bits 8))
        (table Table (row row) (column value float))
        (facade Facade External target (method reset void))
        (struct Values (member xs array float) (member ys array float))
        (integer-scalar Alias :signed false :minimum 0 :maximum 255 :bit-width 8
            :cpp-emission alias :cpp-type std::uint8_t)))";
    auto manifest{parse_test_manifest(context, source)};
    auto const output{render_test_manifest(context, manifest).front().content};
    for (auto const name : {"Record", "Tagged", "Packed", "Table", "Values"}) {
        EXPECT_NE(output.find(std::string{"struct "} + name + ";"), std::string::npos) << name;
    }
    EXPECT_NE(output.find("union Raw;"), std::string::npos);
    EXPECT_NE(output.find("class Facade;"), std::string::npos);
    EXPECT_EQ(output.find("struct Facade;"), std::string::npos);
    for (auto const name : {"Alias", "Tag", "External", "LayoutOnly"}) {
        EXPECT_EQ(output.find(std::string{"struct "} + name + ";"), std::string::npos) << name;
    }
    EXPECT_LT(output.find("using Alias ="), output.find("struct Uses"));
}

} // namespace
} // namespace lispb::codegen
