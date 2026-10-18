#include "document_test_support.hpp"
#include <ioj/layout/document.hpp>
#include <ioj/layout/planner_session.hpp>
#include <ioj/layout/planner_type.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

namespace ioj::layout {
namespace planner_type_tests {

struct CapabilityCase {
    char const* name;
    char const* declaration;
    DeclarationKind kind;
    bool editable;
    bool physical;
    bool variants;
    char const* backend{"standard-library"};
};

class DeclarationCapabilityTest : public testing::TestWithParam<CapabilityCase> {};

TEST_P(DeclarationCapabilityTest, ReportsAuthoringAndAnalysisCapabilities) {
    auto const& test{GetParam()};
    auto const document{Document::from_source(
        std::string{R"((module test :header "Test.h" :source "Test.cpp" :backend )"} +
        test.backend + ' ' + test.declaration + ')')};
    ASSERT_TRUE(document.valid()) << diagnostic_messages(document);
    auto const type{document.find_type("test", "Value")};
    ASSERT_TRUE(type);
    auto const capabilities{declaration_capabilities(document.types(), *type)};
    EXPECT_EQ(capabilities.kind, test.kind);
    EXPECT_TRUE(capabilities.inspectable);
    EXPECT_EQ(capabilities.editable, test.editable);
    EXPECT_EQ(capabilities.physical_analysis_available, test.physical);
    EXPECT_EQ(capabilities.supports_variant_overrides, test.variants);
}

INSTANTIATE_TEST_SUITE_P(
    Declarations,
    DeclarationCapabilityTest,
    testing::Values(
        CapabilityCase{"Enumeration",
                       "(enum Value std::uint8_t (value First))",
                       DeclarationKind::enumeration,
                       true,
                       true,
                       false},
        CapabilityCase{
            "Packed",
            "(packed-value Value :storage std::uint8_t (field bits std::uint8_t :bits 8))",
            DeclarationKind::packed,
            true,
            true,
            true},
        CapabilityCase{"StandardSoa",
                       "(struct Value (member values array float))",
                       DeclarationKind::soa,
                       true,
                       true,
                       true},
        CapabilityCase{"UnrealSoa",
                       "(struct Value (member values array float))",
                       DeclarationKind::soa,
                       false,
                       false,
                       false,
                       "unreal"},
        CapabilityCase{"StandardVector",
                       "(vector-soa Value :value-type float :equivalent-type Vector :components (x "
                       "y) :equivalent-members (x y))",
                       DeclarationKind::vector_soa,
                       false,
                       true,
                       true},
        CapabilityCase{"UnrealVector",
                       "(vector-soa Value :value-type float :equivalent-type Vector :components (x "
                       "y) :equivalent-members (x y))",
                       DeclarationKind::vector_soa,
                       false,
                       false,
                       false,
                       "unreal"}),
    [](auto const& info) { return info.param.name; });

struct StatusCase {
    char const* name;
    char const* source;
    LayoutStatus known;
    LayoutStatus unknown;
};

class DeclarationStatusTest : public testing::TestWithParam<StatusCase> {};

TEST_P(DeclarationStatusTest, DistinguishesUnknownFactsFromErrors) {
    auto const& test{GetParam()};
    auto const document{Document::from_source(test.source)};
    ASSERT_TRUE(document.valid()) << diagnostic_messages(document);
    auto const type{document.find_type("test", "Value")};
    ASSERT_TRUE(type);
    EXPECT_EQ(declaration_status(
                  document.types(), *type, ResolvedVariant{}, AbiProfile::host_common(), 64),
              test.known);
    EXPECT_EQ(
        declaration_status(document.types(), *type, ResolvedVariant{}, AbiProfile{"unknown"}, 64),
        test.unknown);
}

INSTANTIATE_TEST_SUITE_P(
    Declarations,
    DeclarationStatusTest,
    testing::Values(
        StatusCase{
            "Packed",
            R"((module test :header "Test.h" (packed-value Value :storage std::uint8_t (field value std::uint8_t :bits 8))))",
            LayoutStatus::available,
            LayoutStatus::unknown},
        StatusCase{
            "Semantic",
            R"((module test :header "Test.h" (integer-scalar Value :signed false :minimum 0 :maximum 10)))",
            LayoutStatus::available,
            LayoutStatus::available},
        StatusCase{
            "TaggedUnion",
            R"((module test :header "Test.h" (enum Kind std::uint8_t (value First)) (tagged-union Value :discriminant Kind (alternative first std::uint32_t :tag First))))",
            LayoutStatus::available,
            LayoutStatus::unknown},
        StatusCase{
            "OpaqueColumn",
            R"((module test :header "Test.h" :backend standard-library (struct Value (member opaque array UnknownColumnType))))",
            LayoutStatus::unknown,
            LayoutStatus::unknown}),
    [](auto const& info) { return info.param.name; });

TEST(PlannerType, ExternalFactsUnblockValuesOnlyOnTheActiveTarget) {
    auto const document{Document::from_source(R"(
        (module test :header "Test.h"
            (record Value (member value OpaqueVector))
            (record Pointer (member value (type-ref OpaqueVector :suffix "*")))))")};
    ASSERT_TRUE(document.valid()) << diagnostic_messages(document);
    auto const& types{document.types()};
    auto const external{external_dependencies(types).front().types.front()};
    PlannerAnalysisSession session{types};
    session.edit_inputs().selection.select_type(types, external);
    ASSERT_TRUE(session.refresh(nullptr));
    ASSERT_TRUE(session.results().external_type);
    EXPECT_FALSE(session.results().external_type->physical.facts);
    EXPECT_EQ(session.results().external_type->blocked_declarations.size(), 1);
    TypeFacts facts{};
    facts.size_bytes = 3;
    facts.alignment_bytes = 4;
    EXPECT_FALSE(session.set_external_type_facts(external, facts));
    facts.size_bytes = 12;
    facts.origin = FactOrigin::compiler_probe;
    facts.provenance = "SDK assumption";
    ASSERT_TRUE(session.set_external_type_facts(external, facts));
    ASSERT_TRUE(session.refresh(nullptr));
    auto const& result{*session.results().external_type};
    ASSERT_TRUE(result.physical.facts);
    EXPECT_FALSE(result.semantics_known);
    EXPECT_TRUE(result.blocked_declarations.empty());
    EXPECT_EQ(result.physical.facts->origin, FactOrigin::manual_assumption);
    EXPECT_EQ(result.physical.facts->provenance, "SDK assumption");
    EXPECT_EQ(session.status(external), LayoutStatus::available);
    EXPECT_FALSE(session.comparison_abi().find("OpaqueVector"));
    auto const imported{parse_abi_profile(serialize_abi_profile(session.primary_abi()))};
    ASSERT_TRUE(imported);
    EXPECT_EQ(imported->find("OpaqueVector"), result.physical.facts);
    session.set_primary_abi(AbiProfile::host_common());
    ASSERT_TRUE(session.refresh(nullptr));
    EXPECT_FALSE(session.results().external_type->physical.facts);
}

TEST(PlannerType, SwitchingUnsupportedBackendClearsPhysicalResults) {
    auto const document{Document::from_source(R"(
        (module standard :header "Standard.h" :backend standard-library
            (struct Columns (member values array float)))
        (module unreal :header "Unreal.h" :source "Unreal.cpp" :backend unreal
            (struct Columns (member values array float))))")};
    ASSERT_TRUE(document.valid()) << diagnostic_messages(document);
    PlannerAnalysisSession session{document.types()};
    for (auto const module : {"standard", "unreal", "standard"}) {
        ASSERT_TRUE(session.edit_inputs().selection.select_type(
            document.types(), document.find_type(module, "Columns")));
        ASSERT_TRUE(session.refresh(nullptr));
        EXPECT_EQ(session.results().active_soa.has_value(), std::string_view{module} == "standard");
        EXPECT_EQ(session.results().baseline_soa.has_value(),
                  std::string_view{module} == "standard");
    }
}

TEST(PlannerType, StatusTracksVariantRelationshipCapacity) {
    auto document{Document::from_source(R"(
        (module entities :header "Entities.h" :backend standard-library
            (struct Entities (member values array std::uint8_t)))
        (module test :header "Test.h"
            (packed-value Value :storage std::uint64_t
                (field value std::uint64_t :bits 17 (relation index_into Entities)))))")};
    ASSERT_TRUE(document.valid()) << diagnostic_messages(document);
    auto const type{*document.find_type("test", "Value")};
    PlannerAnalysisSession session{document.types()};
    EXPECT_EQ(session.status(type), LayoutStatus::available);
    auto create{document.transaction()};
    create.create_variant("narrow");
    ASSERT_TRUE(document.commit(create)) << diagnostic_messages(document);
    auto const variant{document.variants().ids().back()};
    auto edit{document.transaction()};
    edit.set_variant_field_bits(variant, "test", "Value", "value", 8);
    ASSERT_TRUE(document.commit(edit)) << diagnostic_messages(document);
    session.edit_inputs().selection.select_type(document.types(), type);
    ASSERT_TRUE(session.refresh(&document));
    ASSERT_TRUE(session.edit_inputs().workspace.select_variant(variant));
    ASSERT_TRUE(session.refresh(&document));
    EXPECT_EQ(session.status(type), LayoutStatus::error);
    ASSERT_TRUE(session.results().active_packed);
    EXPECT_TRUE(std::ranges::any_of(
        session.results().active_packed->diagnostics,
        [](auto const& diagnostic) { return diagnostic.severity == DiagnosticSeverity::error; }));
}

TEST(PlannerType, VarintStatusUsesSelectedElementCount) {
    auto const document{Document::from_source(R"(
        (module test :header "Test.h"
            (integer-scalar Source :signed false :minimum 0 :maximum 16384)
            (integer-varint Value :source Source :encoding unsigned)))")};
    ASSERT_TRUE(document.valid()) << diagnostic_messages(document);
    auto const type{*document.find_type("test", "Value")};
    PlannerAnalysisSession session{document.types()};
    EXPECT_EQ(session.status(type), LayoutStatus::available);
    ASSERT_TRUE(session.edit_inputs().workspace.set_element_count(
        (std::numeric_limits<std::uint64_t>::max)()));
    EXPECT_EQ(session.status(type), LayoutStatus::error);
}

}
}
