#include "analyzer_test_fixtures.hpp"

#include <gtest/gtest.h>
#include <native_soa_types.h>

#include <array>
#include <cstddef>
#include <filesystem>
#include <string>
#include <tuple>

namespace ioj::layout::physical_facts_tests {
TEST(PhysicalFacts, GeneratedCppAgreesForPointersNestedRecordsAndAlignedExternalMembers) {
    using namespace ml::native_soa_fixture;
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
        std::array requests{AnalysisRequest{.type = *document.find_type("native_soa_fixture", name),
                                            .abi = &profile}};
        Analyzer::analyze(context, requests);
        auto const& facts{storage.types.physical[storage.runs.front().analysis.value]};
        ASSERT_TRUE(facts);
        EXPECT_EQ(facts->size_bytes, size);
        EXPECT_EQ(facts->alignment_bytes, alignment);
        EXPECT_EQ(facts->origin, FactOrigin::derived);
    }
    auto const envelope{storage.runs.front().analysis};
    auto const first{storage.types.aggregate[envelope.value]->members.begin};
    EXPECT_EQ(storage.components.offset_bytes[first], offsetof(Envelope, pairs));
    EXPECT_EQ(storage.components.offset_bytes[first + 1], offsetof(Envelope, node));
    EXPECT_EQ(storage.components.offset_bytes[first + 2], offsetof(Envelope, aligned));
    std::array requests{
        AnalysisRequest{.type = *document.find_type("native_soa_fixture", "PairRows"),
                        .abi = &profile,
                        .element_count = 7,
                        .default_capacity = 7}};
    Analyzer::analyze(context, requests);
    auto const first_column{storage.types.soa[storage.runs.front().analysis.value]->columns.begin};
    PairRows generated;
    generated.set_num(7);
    EXPECT_EQ(storage.columns.total_bytes[first_column],
              generated.pairs.size() * sizeof(generated.pairs[0]));
    EXPECT_EQ(storage.columns.total_bytes[first_column + 1],
              generated.pointers.size() * sizeof(Node*));
}

TEST(PhysicalFacts, SingleOnlySchemasUseOwnerFactsAndLayoutsHaveNoPhysicalObject) {
    using namespace ml::native_soa_fixture;
    EditorSession editor;
    auto document{editor.load(std::filesystem::path{IOJ_SOURCE_DIR} / "lispb/project.lispb",
                              "native-soa-fixture")};
    ASSERT_TRUE(document.valid());
    auto profile{AbiProfile::host_common()};
    TypeFacts owner{};
    owner.size_bytes = sizeof(ApiOwner);
    owner.alignment_bytes = alignof(ApiOwner);
    owner.origin = FactOrigin::compiler_probe;
    profile.set("ml::native_soa_fixture::ApiOwner", owner);
    std::array requests{
        AnalysisRequest{.type = *document.find_type("native_soa_fixture", "ApiRows"),
                        .abi = &profile},
        AnalysisRequest{.type = *document.find_type("native_soa_fixture", "ApiCoordinates"),
                        .abi = &profile}};
    AnalysisStorage storage;
    AnalysisContext context{document, storage};
    Analyzer::analyze(context, requests);
    auto const& facts{storage.types.physical[storage.runs[0].analysis.value]};
    ASSERT_TRUE(facts);
    EXPECT_EQ(facts->size_bytes, sizeof(ApiOwner));
    EXPECT_FALSE(storage.types.physical[storage.runs[1].analysis.value]);
    expect_analysis_diagnostics(context);
}

struct PhysicalCase {
    char const* name;
    char const* spelling;
    bool known;
    char const* declaration{""};
};
class PhysicalSpelling : public testing::TestWithParam<PhysicalCase> {};
TEST_P(PhysicalSpelling, DoesNotGuessUnsupportedOrMissingRepresentations) {
    EditorSession editor;
    auto const& test{GetParam()};
    auto document{editor.from_source(std::string{R"((module test :header "Test.h" )"} +
                                     test.declaration + R"((record Value (member value ")" +
                                     test.spelling + "\")))")};
    ASSERT_TRUE(document.valid()) << diagnostic_messages(editor);
    auto const profile{AbiProfile::host_common()};
    std::array requests{
        AnalysisRequest{.type = *document.find_type("test", "Value"), .abi = &profile}};
    AnalysisStorage storage;
    AnalysisContext context{document, storage};
    analyze_requests(context, requests, !test.known);
    auto const& facts{storage.types.physical[storage.runs.front().analysis.value]};
    EXPECT_EQ(facts.has_value(), test.known);
}
INSTANTIATE_TEST_SUITE_P(
    Uses,
    PhysicalSpelling,
    testing::Values(PhysicalCase{"Pointer", "Opaque*", true},
                    PhysicalCase{"UnknownValue", "Opaque", false},
                    PhysicalCase{"Array", "float[3]", false},
                    PhysicalCase{"FunctionPointer", "float(*)()", false},
                    PhysicalCase{"Reference", "float&", false},
                    PhysicalCase{"RvalueReference", "float&&", false},
                    PhysicalCase{"MemberPointer", "float Owner::*", false},
                    PhysicalCase{"NestedUnknown", "Owner::Inner", false},
                    PhysicalCase{"LogicalValue",
                                 "Logical",
                                 false,
                                 "(integer-scalar Logical :signed false :minimum 0 :maximum 100)"}),
    [](auto const& info) { return info.param.name; });

TEST(PhysicalFacts, PointerPolicyRetainsManualEvidence) {
    EditorSession editor;
    auto document{editor.from_source(
        R"((module test :header "Test.h" (record Value (member value "Opaque*"))))")};
    ASSERT_TRUE(document.valid());
    AbiProfile profile{"assumed target"};
    TypeFacts pointer{};
    pointer.size_bytes = 8;
    pointer.alignment_bytes = 8;
    pointer.origin = FactOrigin::manual_assumption;
    pointer.provenance = "Unverified SDK assumption";
    profile.set("void*", pointer);
    profile.set_object_pointer_representation("void*");
    std::array requests{
        AnalysisRequest{.type = *document.find_type("test", "Value"), .abi = &profile}};
    AnalysisStorage storage;
    AnalysisContext context{document, storage};
    Analyzer::analyze(context, requests);
    ASSERT_TRUE(storage.components.element_facts.front());
    EXPECT_EQ(storage.components.element_facts.front()->origin, FactOrigin::manual_assumption);
    EXPECT_TRUE(storage.components.element_facts.front()->provenance.contains(pointer.provenance));
}

TEST(PhysicalFacts, SuppliedFactsCannotSilentlyReplaceGeneratedLayout) {
    EditorSession editor;
    auto document{editor.from_source(
        R"((module test :header "Test.h" (record Value (member value float))))")};
    ASSERT_TRUE(document.valid());
    auto profile{AbiProfile::host_common()};
    TypeFacts incorrect{};
    incorrect.size_bytes = 64;
    incorrect.alignment_bytes = 8;
    profile.set("Value", incorrect);
    std::array requests{
        AnalysisRequest{.type = *document.find_type("test", "Value"), .abi = &profile}};
    AnalysisStorage storage;
    AnalysisContext context{document, storage};
    Analyzer::analyze(context, requests);
    EXPECT_FALSE(storage.types.physical[storage.runs.front().analysis.value]);
    ASSERT_GT(editor.diagnostics().size(), 0);
    EXPECT_EQ(editor.diagnostics().severity(
                  lispb::DiagnosticId{static_cast<std::uint32_t>(editor.diagnostics().size() - 1)}),
              lispb::DiagnosticSeverity::error);
}
}
