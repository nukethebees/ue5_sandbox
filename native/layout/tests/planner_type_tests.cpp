#include "document_test_support.hpp"
#include <ioj/layout/abi_profile.hpp>
#include <ioj/layout/analysis/analysis_context.hpp>
#include <ioj/layout/analysis/analysis_request.hpp>
#include <ioj/layout/analysis/analysis_storage.hpp>
#include <ioj/layout/analysis/analyzer.hpp>
#include <ioj/layout/declaration_capabilities.hpp>
#include <ioj/layout/document.hpp>
#include <ioj/layout/editor_session.hpp>
#include <ioj/layout/type_facts.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

namespace ioj::layout {
namespace planner_type_tests {

struct CapabilityCase {
    char const* name;
    char const* declaration;
    DeclarationCapabilities expected;
    char const* backend{"standard-library"};
};

class DeclarationCapabilityTest : public testing::TestWithParam<CapabilityCase> {};

TEST_P(DeclarationCapabilityTest, ReportsAuthoringAndAnalysisCapabilities) {
    EditorSession editor;
    auto const& test{GetParam()};
    auto const document{editor.from_source(
        std::string{R"((module test :header "Test.h" :source "Test.cpp" :backend )"} +
        test.backend + ' ' + test.declaration + ')')};
    ASSERT_TRUE(document.valid()) << diagnostic_messages(editor);
    auto const type{document.find_type("test", "Value")};
    ASSERT_TRUE(type);
    EXPECT_EQ(declaration_capabilities(document.types(), *type), test.expected);
}

INSTANTIATE_TEST_SUITE_P(
    Declarations,
    DeclarationCapabilityTest,
    testing::Values(
        CapabilityCase{"Enumeration",
                       "(enum Value std::uint8_t (value First))",
                       {DeclarationKind::enumeration, true, true, true, false}},
        CapabilityCase{
            "Packed",
            "(packed-value Value :storage std::uint8_t (field bits std::uint8_t :bits 8))",
            {DeclarationKind::packed, true, true, true, true}},
        CapabilityCase{"StandardSoa",
                       "(struct Value (member values array float))",
                       {DeclarationKind::soa, true, true, true, true}},
        CapabilityCase{"UnrealSoa",
                       "(struct Value (member values array float))",
                       {DeclarationKind::soa, true, false, false, false},
                       "unreal"},
        CapabilityCase{"StandardVector",
                       "(vector-soa Value :value-type float :equivalent-type Vector :components (x "
                       "y) :equivalent-members (x y))",
                       {DeclarationKind::vector_soa, true, false, true, true}},
        CapabilityCase{"UnrealVector",
                       "(vector-soa Value :value-type float :equivalent-type Vector :components (x "
                       "y) :equivalent-members (x y))",
                       {DeclarationKind::vector_soa, true, false, false, false},
                       "unreal"}),
    [](auto const& info) { return info.param.name; });

TEST(AnalysisBatch, ReusesPhysicalRowsAcrossCountsAndSeparatesAbiScenarios) {
    EditorSession editor;
    auto document{editor.from_source(R"((module test :header "Test.h"
        (record Value (member value std::uint32_t))))")};
    ASSERT_TRUE(document.valid()) << diagnostic_messages(editor);
    auto const type{document.find_type("test", "Value").value()};
    auto const known{AbiProfile::host_common()};
    AbiProfile const unknown{"unknown"};
    std::array requests{AnalysisRequest{.type = type, .abi = &known, .element_count = 10},
                        AnalysisRequest{.type = type, .abi = &known, .element_count = 20},
                        AnalysisRequest{.type = type, .abi = &unknown, .element_count = 10}};
    AnalysisStorage storage;
    AnalysisContext context{document, storage};
    Analyzer::analyze(context, requests);
    ASSERT_EQ(storage.runs.size(), 3);
    auto const first{storage.runs[0].analysis};
    auto const second{storage.runs[1].analysis};
    auto const third{storage.runs[2].analysis};
    EXPECT_EQ(first, second);
    EXPECT_NE(first, third);
    EXPECT_EQ(storage.totals[0].total_storage_bytes, 40);
    EXPECT_EQ(storage.totals[1].total_storage_bytes, 80);
    EXPECT_FALSE(storage.totals[2].total_storage_bytes);
    EXPECT_EQ(std::ranges::count(storage.components.parent, first), 1);
    EXPECT_FALSE(storage.types.physical[third.value]);
    ASSERT_GT(editor.diagnostics().size(), 0);
    auto const errors{editor.diagnostics().size()};
    storage.clear();
    EXPECT_TRUE(storage.types.schema_type.empty());
    EXPECT_TRUE(storage.runs.empty());
    EXPECT_EQ(editor.diagnostics().size(), errors);
}

TEST(AnalysisBatch, ExternalFactsApplyOnlyToTheirRequestedTarget) {
    EditorSession editor;
    auto document{editor.from_source(R"((module test :header "Test.h"
        (record Value (member value OpaqueVector))))")};
    ASSERT_TRUE(document.valid());
    auto const type{document.find_type("test", "Value").value()};
    auto supplied{AbiProfile::host_common()};
    TypeFacts facts{};
    facts.size_bytes = 12;
    facts.alignment_bytes = 4;
    facts.origin = FactOrigin::manual_assumption;
    facts.provenance = "SDK assumption";
    supplied.set("OpaqueVector", facts);
    auto const unknown{AbiProfile::host_common()};
    std::array requests{AnalysisRequest{.type = type, .abi = &supplied},
                        AnalysisRequest{.type = type, .abi = &unknown}};
    AnalysisStorage storage;
    AnalysisContext context{document, storage};
    Analyzer::analyze(context, requests);
    ASSERT_EQ(storage.runs.size(), 2);
    EXPECT_EQ(storage.types.physical[storage.runs[0].analysis.value]->size_bytes, 12);
    EXPECT_FALSE(storage.types.physical[storage.runs[1].analysis.value]);
    EXPECT_FALSE(unknown.find("OpaqueVector"));
}

TEST(AnalysisBatch, VariantOverflowDoesNotAlterAuthoredSchema) {
    EditorSession editor;
    auto document{editor.from_source(R"((module test :header "Test.h"
        (packed-value Value :storage std::uint8_t (field value std::uint8_t :bits 8))))")};
    ASSERT_TRUE(document.valid());
    auto create{document.transaction()};
    create.create_variant("Wide");
    ASSERT_TRUE(document.commit(create));
    auto const variant{document.variants().ids().back()};
    auto edit{document.transaction()};
    edit.set_variant_field_bits(variant, "test", "Value", "value", 16);
    ASSERT_TRUE(document.commit(edit));
    auto const type{document.find_type("test", "Value").value()};
    auto const abi{AbiProfile::host_common()};
    std::array requests{AnalysisRequest{.type = type, .abi = &abi},
                        AnalysisRequest{.type = type, .abi = &abi, .variant = variant}};
    AnalysisStorage storage;
    AnalysisContext context{document, storage};
    Analyzer::analyze(context, requests);
    ASSERT_EQ(storage.runs.size(), 2);
    EXPECT_EQ(storage.types.packed[storage.runs[0].analysis.value]->overflow_bits, 0);
    EXPECT_EQ(storage.types.packed[storage.runs[1].analysis.value]->overflow_bits, 8);
    EXPECT_GT(editor.diagnostics().size(), 0);
    EXPECT_EQ(document.query().module("test").packed_value("Value").field("value").bits(), 8);
}
}
}
