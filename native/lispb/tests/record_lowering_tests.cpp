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
#include <lispb/schema/source_renderer.h>
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

TEST(RecordLowering, RejectsInvalidSettingsBindingsAndOverrideDependencies) {
    for (
        auto const declarations : {
            R"((record Config (member speed float))
           (settings-schema Fields :target Config :target-header "Records.h"
             (section "config" "config" :record Config)))",
            R"((record Config :generate-overrides true (member speed float)))",
            R"((record Config :generate-settings true :generate-overrides true
             (member speed (type-ref float :suffix "&"))))",
            R"((record Config :generate-settings true :generate-overrides true
             (member speed (type-ref float :suffix " const"))))",
            R"((record Child (member speed float))
           (record Config :generate-settings true :generate-overrides true (member child Child)))",
            R"((record Config :generate-settings true (member speed float))
           (settings-schema Fields :target Config :target-header "Records.h"
             (section "config" "config" :record Config)
             (field "config.speed" "speed")))",
            R"((record Config :generate-settings true (member speed float :settings-exclude true))
           (settings-schema Fields :target Config :target-header "Records.h"
             (section "config" "config" :record Config)))",
            R"((record Config :generate-settings true (member speed float :settings-name "")))",
            R"((record Config :generate-settings true (member child Config))
           (settings-schema Fields :target Config :target-header "Records.h"
             (section "config" "config" :record Config)))",
            R"((record Config :generate-settings true :generate-overrides true (member speed float))
           (record ConfigOverrides))"}) {
        SCOPED_TRACE(declarations);
        CompilerTestContext context;
        auto manifest{parse_test_manifest(
            context, std::string{R"((module records :header "Records.h" )"} + declarations + ")")};
        EXPECT_THROW(static_cast<void>(lower_test_manifest(context, manifest)),
                     std::invalid_argument);
    }
}

TEST(RecordLowering, SettingsMetadataDoesNotChangeOrdinaryRecordOutput) {
    auto const render{[](std::string_view const options) {
        CompilerTestContext context;
        auto manifest{parse_test_manifest(
            context,
            std::string{R"((module records :header "Records.h" (record Config )"} +
                std::string{options} + R"( (member max_speed float :initializer "2.f"))))")};
        return render_test_manifest(context, manifest).front().content;
    }};
    EXPECT_EQ(render(""), render(":generate-settings true"));
    EXPECT_EQ(render(":generate-settings true"), render(":generate-settings true"));
}

TEST(RecordLowering, SettingsOptionsSurviveSourceRoundTrip) {
    CompilerTestContext context;
    auto const manifest{parse_test_manifest(context, R"(
        (module records :header "Records.h"
          (record Config :generate-settings true :generate-overrides true
            (member max_speed float :initializer "2.f" :settings-name "speed")
            (member asset float :settings-exclude true))
          (settings-schema Fields :target Root :target-header "Root.h"
            (section "config" "config" :record Config)
            (field "extra" "extra"))))")};
    auto const source{
        schema::render_module_source(manifest.declarations, manifest.modules.front())};
    CompilerTestContext reparsed;
    auto const round_trip{parse_test_manifest(reparsed, source)};
    EXPECT_EQ(schema::render_module_source(round_trip.declarations, round_trip.modules.front()),
              source);
    EXPECT_EQ(render_test_manifest(context, manifest).front().content,
              render_test_manifest(reparsed, round_trip).front().content);
}

TEST(RecordLowering, ReaderOptionsSurviveRoundTripAndLeaveOrdinaryOutputUnchanged) {
    auto const render{[](std::string_view const options) {
        CompilerTestContext context;
        auto const manifest{parse_test_manifest(
            context,
            std::format(R"((module records :header "Records.h" :namespace example
                (record Config {} (member max_speed float :initializer "2.f"))))",
                        options))};
        auto const source{
            schema::render_module_source(manifest.declarations, manifest.modules.front())};
        CompilerTestContext reparsed;
        auto const round_trip{parse_test_manifest(reparsed, source)};
        auto const output{render_test_manifest(context, manifest)};
        auto const second{render_test_manifest(reparsed, round_trip)};
        EXPECT_EQ(output.size(), second.size());
        for (std::size_t i{}; i < output.size(); ++i) {
            EXPECT_EQ(output[i].content, second[i].content);
        }
        return output;
    }};
    auto const ordinary{render("")};
    auto const reader{render(":generate-s7-reader true :s7-tag config")};
    ASSERT_EQ(ordinary.size(), 1u);
    ASSERT_EQ(reader.size(), 2u);
    EXPECT_EQ(ordinary.front().content, reader.front().content);
    EXPECT_EQ(reader.front().content.find("s7/reader"), std::string::npos);
}

TEST(RecordLowering, RetainsReaderConstraintsInCanonicalSource) {
    CompilerTestContext context;
    auto const manifest{parse_test_manifest(context, R"(
        (module records :header "Records.h"
          (record Config :generate-s7-reader true :s7-tag config
            :s7-reader-includes ("CustomReader.h")
            (member value int :s7-name renamed :required true :nonzero true
              :positive true :nonnegative true :finite true :minimum -9223372036854775808 :maximum 9)
            (member text int :nonempty true))))")};
    auto const source{
        schema::render_module_source(manifest.declarations, manifest.modules.front())};
    CompilerTestContext reparsed;
    auto const round_trip{parse_test_manifest(reparsed, source)};
    EXPECT_EQ(source,
              schema::render_module_source(round_trip.declarations, round_trip.modules.front()));
    auto const first{render_test_manifest(context, manifest)};
    auto const second{render_test_manifest(reparsed, round_trip)};
    ASSERT_EQ(first.size(), 2u);
    EXPECT_EQ(first.back().content, second.back().content);
}

TEST(RecordLowering, RejectsAmbiguousReaderFieldsAndInvalidBounds) {
    for (
        auto const declarations :
        {R"((record Config :generate-s7-reader true))",
         R"((record Config :s7-tag config))",
         R"((record Config :generate-s7-reader true :s7-tag config
            (member first int :s7-name same) (member second int :s7-name same)))",
         R"((record Config :generate-s7-reader true :s7-tag config (member value int :s7-name "")))",
         R"((record Config :generate-s7-reader true :s7-tag config (member value int :minimum 4 :maximum 2)))",
         R"((record Config :generate-s7-reader true :s7-tag config (member value int :minimum "1;bad")))",
         R"((record Config :generate-s7-reader true :s7-tag config (member value int :minimum -9223372036854775809)))"}) {
        SCOPED_TRACE(declarations);
        CompilerTestContext context;
        EXPECT_THROW(
            {
                auto const manifest{parse_test_manifest(
                    context,
                    std::format(R"((module records :header "Records.h" {}))", declarations))};
                static_cast<void>(lower_test_manifest(context, manifest));
            },
            std::exception);
    }
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
