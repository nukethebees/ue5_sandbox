#include "analyzer_test_fixtures.hpp"

#include <gtest/gtest.h>
#include <native_soa/types.hpp>

#include <array>
#include <cstddef>
#include <filesystem>
#include <string>
#include <tuple>

namespace ioj::layout::physical_facts_tests {
TEST(PhysicalFacts, GeneratedCppAgreesForPointersNestedRecordsAndAlignedExternalMembers) {
    using namespace ioj::native_soa_fixture;
    EditorSession editor;
    auto document{editor.load(std::filesystem::path{IOJ_SOURCE_DIR} / "lispb/project.lispb",
                              "native-soa-fixture")};
    ASSERT_TRUE(document.valid()) << diagnostic_messages(editor);
    auto profile{AbiProfile::host_common()};
    TypeFacts aligned{};
    aligned.size_bytes = sizeof(Aligned32);
    aligned.alignment_bytes = alignof(Aligned32);
    aligned.origin = FactOrigin::compiler_probe;
    aligned.provenance = "compiled differential fixture";
    profile.set("Aligned32", aligned);
    AnalysisStorage storage;
    AnalysisContext context{document, storage};
    for (auto const& [name, size, alignment] :
         {std::tuple{"Pair", sizeof(Pair), alignof(Pair)},
          std::tuple{"Node", sizeof(Node), alignof(Node)},
          std::tuple{"First", sizeof(First), alignof(First)},
          std::tuple{"Second", sizeof(Second), alignof(Second)},
          std::tuple{"Envelope", sizeof(Envelope), alignof(Envelope)}}) {
        SCOPED_TRACE(name);
        std::array<AnalysisRequest, 1> requests{
            {{.type = *document.find_type("native_soa_fixture", name), .abi = &profile}}};
        Analyzer::analyze(context, requests);
        auto const& facts{storage.types.physical()[storage.runs.front().analysis.value]};
        ASSERT_TRUE(facts);
        EXPECT_EQ(storage.physical_facts.size_bytes()[facts->value], size);
        EXPECT_EQ(storage.physical_facts.alignment_bytes()[facts->value], alignment);
        EXPECT_EQ(storage.physical_facts.origin()[facts->value], FactOrigin::derived);
    }
    auto const envelope{storage.runs.front().analysis};
    auto const first{storage.types.aggregate()[envelope.value]->members.begin};
    EXPECT_EQ(storage.components.offset_bytes()[first], offsetof(Envelope, pairs));
    EXPECT_EQ(storage.components.offset_bytes()[first + 1], offsetof(Envelope, node));
    EXPECT_EQ(storage.components.offset_bytes()[first + 2], offsetof(Envelope, aligned));
    std::array<AnalysisRequest, 1> requests{
        {{.type = *document.find_type("native_soa_fixture", "PairRows"),
          .abi = &profile,
          .element_count = 7,
          .default_capacity = 7}}};
    Analyzer::analyze(context, requests);
    auto const first_column{
        storage.types.soa()[storage.runs.front().analysis.value]->columns.begin};
    PairRows generated;
    generated.set_num(7);
    EXPECT_EQ(storage.columns.total_bytes()[first_column],
              generated.capacity() * sizeof(generated.pairs()[0]));
    EXPECT_EQ(storage.columns.total_bytes()[first_column + 1],
              generated.capacity() * sizeof(Node*));
    auto const& layout{storage.types.soa()[storage.runs.front().analysis.value].value()};
    EXPECT_EQ(layout.capacity, generated.capacity());
    EXPECT_EQ(layout.allocation_count, 1);
    EXPECT_EQ(layout.total_allocation_bytes,
              PairRows::Layout::layout_bytes(generated.capacity() /
                                             PairRows::Layout::capacity_granularity));
    EXPECT_EQ(layout.allocation_alignment_bytes, PairRows::Layout::allocation_alignment);
}

TEST(PhysicalFacts, SoaOwnersHavePhysicalFactsAndLayoutsHaveNoPhysicalObject) {
    using namespace ioj::native_soa_fixture;
    EditorSession editor;
    auto document{editor.load(std::filesystem::path{IOJ_SOURCE_DIR} / "lispb/project.lispb",
                              "native-soa-fixture")};
    ASSERT_TRUE(document.valid());
    auto profile{AbiProfile::host_common()};
    TypeFacts owner{};
    owner.size_bytes = sizeof(ApiOwner);
    owner.alignment_bytes = alignof(ApiOwner);
    owner.origin = FactOrigin::compiler_probe;
    profile.set("ioj::native_soa_fixture::ApiOwner", owner);
    std::array<AnalysisRequest, 2> requests{
        {{.type = *document.find_type("native_soa_fixture", "ApiOwner"), .abi = &profile},
         {.type = *document.find_type("native_soa_fixture", "ApiCoordinates"), .abi = &profile}}};
    AnalysisStorage storage;
    AnalysisContext context{document, storage};
    Analyzer::analyze(context, requests);
    auto const& facts{storage.types.physical()[storage.runs[0].analysis.value]};
    ASSERT_TRUE(facts);
    EXPECT_EQ(storage.physical_facts.size_bytes()[facts->value], sizeof(ApiOwner));
    EXPECT_FALSE(storage.types.physical()[storage.runs[1].analysis.value]);
    expect_analysis_diagnostics(context);
}

struct PhysicalCase {
    char const* name;
    char const* spelling;
    bool known;
};
class PhysicalSpelling : public testing::TestWithParam<PhysicalCase> {};
TEST_P(PhysicalSpelling, DoesNotGuessUnsupportedOrMissingRepresentations) {
    EditorSession editor;
    auto const& test{GetParam()};
    auto document{editor.from_source(
        std::string{R"((module test :header "Test.h" (record Value (member value ")"} +
        test.spelling + "\") (projections (cpp))))")};
    ASSERT_TRUE(document.valid()) << diagnostic_messages(editor);
    auto const profile{AbiProfile::host_common()};
    std::array<AnalysisRequest, 1> requests{
        {{.type = *document.find_type("test", "Value"), .abi = &profile}}};
    AnalysisStorage storage;
    AnalysisContext context{document, storage};
    analyze_requests(context, requests, !test.known);
    auto const& facts{storage.types.physical()[storage.runs.front().analysis.value]};
    EXPECT_EQ(facts.has_value(), test.known);
}
INSTANTIATE_TEST_SUITE_P(Uses,
                         PhysicalSpelling,
                         testing::Values(PhysicalCase{"Pointer", "Opaque*", true},
                                         PhysicalCase{"UnknownValue", "Opaque", false},
                                         PhysicalCase{"Array", "float[3]", false},
                                         PhysicalCase{"FunctionPointer", "float(*)()", false},
                                         PhysicalCase{"Reference", "float&", false},
                                         PhysicalCase{"RvalueReference", "float&&", false},
                                         PhysicalCase{"MemberPointer", "float Owner::*", false},
                                         PhysicalCase{"NestedUnknown", "Owner::Inner", false}),
                         [](auto const& info) { return info.param.name; });

TEST(PhysicalFacts, NativeProjectionRejectsLogicalOnlyMember) {
    EditorSession editor;
    auto const document{editor.from_source(R"((module test :header "Test.h"
        (integer-scalar Logical :bit-width 7)
        (record Value (member value @Logical) (projections (cpp)))))")};
    EXPECT_FALSE(document.valid());
    EXPECT_TRUE(diagnostic_messages(editor).contains("has no C++ owner for member value"));
}

TEST(PhysicalFacts, PointerPolicyRetainsManualEvidence) {
    EditorSession editor;
    auto document{editor.from_source(
        R"((module test :header "Test.h" (record Value (member value "Opaque*")
    (projections (cpp)))))")};
    ASSERT_TRUE(document.valid());
    AbiProfile profile{"assumed target"};
    TypeFacts pointer{};
    pointer.size_bytes = 8;
    pointer.alignment_bytes = 8;
    pointer.origin = FactOrigin::manual_assumption;
    pointer.provenance = "Unverified SDK assumption";
    profile.set("void*", pointer);
    profile.set_object_pointer_representation("void*");
    std::array<AnalysisRequest, 1> requests{
        {{.type = *document.find_type("test", "Value"), .abi = &profile}}};
    AnalysisStorage storage;
    AnalysisContext context{document, storage};
    Analyzer::analyze(context, requests);
    ASSERT_TRUE(storage.components.element_facts().front());
    EXPECT_EQ(storage.physical_facts.origin()[storage.components.element_facts().front()->value],
              FactOrigin::manual_assumption);
    EXPECT_TRUE(storage.strings
                    .view(storage.physical_facts
                              .provenance()[storage.components.element_facts().front()->value])
                    .contains(pointer.provenance));
}

TEST(PhysicalFacts, SuppliedFactsCannotSilentlyReplaceGeneratedLayout) {
    EditorSession editor;
    auto document{editor.from_source(
        R"((module test :header "Test.h" (record Value (member value float)
    (projections (cpp)))))")};
    ASSERT_TRUE(document.valid());
    auto profile{AbiProfile::host_common()};
    TypeFacts incorrect{};
    incorrect.size_bytes = 64;
    incorrect.alignment_bytes = 8;
    profile.set("Value", incorrect);
    std::array<AnalysisRequest, 1> requests{
        {{.type = *document.find_type("test", "Value"), .abi = &profile}}};
    AnalysisStorage storage;
    AnalysisContext context{document, storage};
    Analyzer::analyze(context, requests);
    EXPECT_FALSE(storage.types.physical()[storage.runs.front().analysis.value]);
    ASSERT_GT(editor.diagnostics().size(), 0);
    EXPECT_EQ(editor.diagnostics().severity(
                  lispb::DiagnosticId{static_cast<std::uint32_t>(editor.diagnostics().size() - 1)}),
              lispb::DiagnosticSeverity::error);
}
}
