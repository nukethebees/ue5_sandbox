#include "compiler_test_context.h"
#include "test_utilities.h"
#include "text_expectations.h"

#include <codegen/ast/cpp_types.h>
#include <codegen/ast/indices.h>
#include <codegen/generator.h>
#include <codegen/schema.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/schema_version.h>
#include <codegen/sexpr/source_error.h>
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
using lispb::test_support::expect_contains_all;
namespace {

TEST(RecordLowering, GeneratesUniqueConstructorsAndPreservesSpecialForms) {
    CompilerTestContext context;
    auto manifest{parse_test_manifest(context, R"(
      (module constructors :header "Constructors.h" :s7-constructors true
        (record First :generate-s7-reader true :s7-tag "shared"
    (projections (cpp)))
        (record Second :generate-s7-reader true :s7-tag "shared"
    (projections (cpp)))
        (record Special :generate-s7-reader true :s7-tag "special" :s7-constructor false
    (projections (cpp)))))")};
    auto const files{render_test_manifest(context, manifest)};
    auto const& output{files.front().content};
    auto const constructor{output.find("(define (shared . arguments)")};
    ASSERT_NE(constructor, std::string::npos);
    EXPECT_EQ(output.find("(define (shared . arguments)", constructor + 1), std::string::npos);
    EXPECT_FALSE(output.contains("(define (special"));
}

class InvalidConstructorTags : public testing::TestWithParam<char const*> {};

TEST_P(InvalidConstructorTags, RejectsUnsafeAndConflictingConstructorTags) {
    CompilerTestContext context;
    auto manifest{parse_test_manifest(
        context,
        std::format("(module constructors :header Constructors.h :s7-constructors true {})",
                    GetParam()))};
    EXPECT_THROW(static_cast<void>(render_test_manifest(context, manifest)), std::invalid_argument);
}

INSTANTIATE_TEST_SUITE_P(RecordLowering,
                         InvalidConstructorTags,
                         testing::Values(R"((record Bad :generate-s7-reader true :s7-tag "a) (bad"
    (projections (cpp))))",
                                         R"((record First :generate-s7-reader true :s7-tag "shared"
    (projections (cpp)))
           (record Second :generate-s7-reader true :s7-tag "shared" :s7-constructor false
    (projections (cpp))))"));

TEST(RecordLowering, GeneratesSemanticMembersAndFixedArrays) {
    CompilerTestContext context;
    auto manifest{parse_test_manifest(
        context,
        R"(
        (module records :header "Records.h" :namespace project
            (record Position (member x float) (member y float)
    (projections (cpp)))
            (record Sample :export-specifier PROJECT_API
                (member position Position) (member values @value :count 3)
    (projections (cpp)))))",
        {{"value",
          context.storage.cpp_types.add(
              "Value", {context.storage.cpp_types.dependency("Value", "Project/Value.h")})}})};

    auto const files{render_test_manifest(context, manifest)};

    ASSERT_EQ(files.size(), 1U);
    auto const& output{files.front().content};
    expect_contains_all(output,
                        {"#include <array>",
                         "#include \"Project/Value.h\"",
                         "namespace project",
                         "struct Position",
                         "float x;",
                         "struct PROJECT_API Sample",
                         "Position position;",
                         "std::array<Value, 3> values;"});
}

class InvalidRecordDeclaration : public testing::TestWithParam<char const*> {};

TEST_P(InvalidRecordDeclaration, RejectsDeclaration) {
    CompilerTestContext context;
    auto manifest{parse_test_manifest(
        context, std::format(R"((module records :header "Records.h" {}))", GetParam()))};
    EXPECT_THROW(static_cast<void>(lower_test_manifest(context, manifest)), std::invalid_argument);
}

INSTANTIATE_TEST_SUITE_P(
    SettingsBindingsAndConstrainedNames,
    InvalidRecordDeclaration,
    testing::Values(R"((record Config (member speed float :s7-name "")
    (projections (cpp))))",
                    R"((record Config :constraint-policy example::policy
    (projections (cpp))) (record ConfigValue
    (projections (cpp))))",
                    R"((record Config :generate-visitor true (member Field int)
    (projections (cpp))))",
                    R"((record Config :generate-visitor true (member visit_field int)
    (projections (cpp))))",
                    R"((record Config :generate-visitor true (member get_field int)
    (projections (cpp))))",
                    R"((record Config :generate-visitor true (member invoke_field int)
    (projections (cpp))))",
                    R"((record Selected :generate-visitor true
    (projections (cpp))))",
                    R"((record Field :generate-visitor true
    (projections (cpp))))",
                    R"((record Self :generate-visitor true
    (projections (cpp))))"));

INSTANTIATE_TEST_SUITE_P(
    DuplicateAndZeroCountMembers,
    InvalidRecordDeclaration,
    testing::Values("(record Record (member value float) (member value float))",
                    "(record Record (member values float :count 0))"));

class RemovedRecordOption : public testing::TestWithParam<char const*> {};

TEST_P(RemovedRecordOption, RejectsRemovedOverrideGenerationOption) {
    CompilerTestContext context;
    EXPECT_THROW(static_cast<void>(parse_test_manifest(
                     context, std::format("(module records :header Records.h {})", GetParam()))),
                 sexpr::SourceError);
}

INSTANTIATE_TEST_SUITE_P(
    RecordLowering,
    RemovedRecordOption,
    testing::Values("(record Config :generate-overrides true)",
                    "(record Config :generate-settings true)",
                    "(record Config (member value int :settings-exclude true))",
                    "(record Config (member value int :settings-name value))"));

TEST(RecordLowering, SettingsMetadataDoesNotChangeOrdinaryRecordOutput) {
    auto const render{[](std::string_view const options) {
        CompilerTestContext context;
        auto manifest{
            parse_test_manifest(context,
                                std::format(R"((module records :header "Records.h" (record Config
                (member max_speed float :initializer 2.f {})
    (projections (cpp)))))",
                                            options))};
        return render_test_manifest(context, manifest).front().content;
    }};
    EXPECT_EQ(render(""), render(":s7-exclude true :s7-name speed"));
}

TEST(RecordLowering, SettingsOptionsSurviveSourceRoundTrip) {
    CompilerTestContext context;
    auto const manifest{parse_test_manifest(context, R"(
        (module records :header "Records.h"
          (record Config :generate-visitor true
            (member max_speed float :initializer "2.f" :s7-name "speed")
            (member asset float :s7-exclude true)
    (projections (cpp)))
          (record Range :constraint-policy example::policy
            :generate-s7-reader true :s7-tag "range"
            (member minimum int :initializer 0 :required true)
            (member maximum int :initializer 1 :required true)
    (projections (cpp)))
))")};
    auto const source{
        schema::render_module_source(manifest.declarations, manifest.modules.front())};
    CompilerTestContext reparsed;
    auto const round_trip{parse_test_manifest(reparsed, source)};
    EXPECT_EQ(schema::render_module_source(round_trip.declarations, round_trip.modules.front()),
              source);
    auto const output{render_test_manifest(context, manifest).front().content};
    EXPECT_EQ(output, render_test_manifest(reparsed, round_trip).front().content);
    EXPECT_TRUE(output.contains("case Field::max_speed:\n"));
    EXPECT_FALSE(output.contains("case Field::max_speed: {"));
    EXPECT_TRUE(output.contains("return invoke_field<Field::max_speed>"));
}

TEST(RecordLowering, ReaderOptionsSurviveRoundTripAndLeaveOrdinaryOutputUnchanged) {
    auto const render{[](std::string_view const options) {
        CompilerTestContext context;
        auto const manifest{parse_test_manifest(
            context,
            std::format(R"((module records :header "Records.h" :namespace example
                (record Config {} (member max_speed float :initializer "2.f")
    (projections (cpp)))))",
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
    auto const positional{render(":generate-s7-reader true :s7-tag config :s7-positional true")};
    ASSERT_EQ(ordinary.size(), 1u);
    ASSERT_EQ(reader.size(), 2u);
    EXPECT_EQ(ordinary.front().content, reader.front().content);
    EXPECT_EQ(ordinary.front().content, positional.front().content);
    EXPECT_FALSE(reader.front().content.contains("s7/reader"));
}

TEST(RecordLowering, RetainsReaderConstraintsInCanonicalSource) {
    CompilerTestContext context;
    auto const manifest{parse_test_manifest(context, R"(
        (module records :header "Records.h"
          (record Config :generate-s7-reader true :s7-tag config
            :s7-reader-includes ("CustomReader.h")
            (member value int :s7-name renamed :required true :nonzero true
              :positive true :nonnegative true :finite true :minimum -9223372036854775808 :maximum 9)
            (member text int :nonempty true)
    (projections (cpp)))))")};
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

class InvalidRecordReader : public testing::TestWithParam<char const*> {};

TEST_P(InvalidRecordReader, RejectsAmbiguousReaderFieldsAndInvalidBounds) {
    CompilerTestContext context;
    EXPECT_THROW(
        {
            auto const manifest{parse_test_manifest(
                context, std::format(R"((module records :header "Records.h" {}))", GetParam()))};
            static_cast<void>(lower_test_manifest(context, manifest));
        },
        std::exception);
}

INSTANTIATE_TEST_SUITE_P(
    RecordLowering,
    InvalidRecordReader,
    testing::Values(
        R"((record Config :generate-s7-reader true
    (projections (cpp))))",
        R"((record Config :s7-tag config
    (projections (cpp))))",
        R"((record Config :s7-positional true
    (projections (cpp))))",
        R"((record Config :generate-s7-reader true :s7-tag config
            (member first int :s7-name same) (member second int :s7-name same)
    (projections (cpp))))",
        R"((record Config :generate-s7-reader true :s7-tag config (member value int :s7-name "")
    (projections (cpp))))",
        R"((record Config :generate-s7-reader true :s7-tag config (member value int :minimum 4 :maximum 2)
    (projections (cpp))))",
        R"((record Config :generate-s7-reader true :s7-tag config (member value int :minimum "1;bad")
    (projections (cpp))))",
        R"((record Config :generate-s7-reader true :s7-tag config (member value int :minimum -9223372036854775809)
    (projections (cpp))))"));

TEST(RecordLowering, AllowsEmptyAndNonemptyRecords) {
    CompilerTestContext context;
    auto record = [&](std::string_view members) {
        return parse_test_manifest(
            context,
            R"((module records :header "Records.h" (record Record (projections (cpp)) )" +
                std::string{members} + "))");
    };
    EXPECT_NO_THROW(
        static_cast<void>(lower_test_manifest(context, record("(member value float)"))));
    auto const files{render_test_manifest(context, record(""))};
    ASSERT_EQ(files.size(), 1U);
    EXPECT_TRUE(files.front().content.contains("struct Record"));
}

TEST(RecordLowering, EmitsByValueDependenciesBeforeTheirUsers) {
    CompilerTestContext context;
    auto manifest{parse_test_manifest(context, R"(
        (module records :header "Records.h"
            (record Sample (member position Position)
    (projections (cpp)))
            (record Position (member x float)
    (projections (cpp)))))")};

    auto const files{render_test_manifest(context, manifest)};
    auto const& output{files.front().content};
    EXPECT_LT(output.find("struct Position"), output.find("struct Sample"));
}

TEST(RecordLowering, ForwardDeclaresMutuallyReferringPointerRecords) {
    CompilerTestContext context;
    auto manifest{parse_test_manifest(context, R"(
        (module records :header "Records.h"
            (record First (member next (type-ref Second :suffix "*"))
    (projections (cpp)))
            (record Second (member next (type-ref First :suffix "*"))
    (projections (cpp)))))")};
    auto const files{render_test_manifest(context, manifest)};
    auto const& output{files.front().content};
    expect_contains_all(output,
                        {"struct First;", "struct Second;", "Second* next;", "First* next;"});
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
        (record Uses (projections (cpp)) )"};
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
        (record Record
    (projections (cpp)))
        (union Raw (member value float)
    (projections (cpp)))
        (enum Tag std::uint8_t (value Value)
    (projections (cpp)))
        (union TaggedPayload (member value float :case Value) (projections (cpp)))
        (record Tagged (member tag Tag) (member payload TaggedPayload :tag tag)
          (projections (cpp)))
        (record Packed :layout packed :bits 8 (member value std::uint8_t :bits 8)
    (projections (cpp :storage std::uint8_t)))
        (table Table (row row) (column value float))
        (facade Facade External target (method reset void))
        (struct Values (member xs array float) (member ys array float))
        (integer-scalar Alias :signed false :minimum 0 :maximum 255 :bit-width 8
    (projections (cpp :storage std::uint8_t)))))";
    auto manifest{parse_test_manifest(context, source)};
    auto const output{render_test_manifest(context, manifest).front().content};
    for (auto const name : {"Record", "Tagged", "Packed", "Table", "Values"}) {
        EXPECT_NE(output.find(std::string{"struct "} + name + ";"), std::string::npos) << name;
    }
    expect_contains_all(output, {"union Raw;", "class Facade;"});
    EXPECT_FALSE(output.contains("struct Facade;"));
    for (auto const name : {"Tag", "External", "LayoutOnly"}) {
        EXPECT_EQ(output.find(std::string{"struct "} + name + ";"), std::string::npos) << name;
    }
    EXPECT_TRUE(output.contains("struct Alias;"));
}

} // namespace
} // namespace lispb::codegen
