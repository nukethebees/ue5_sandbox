#include <ioj/layout/physical_facts.hpp>

#include <codegen/source_loader.h>
#include <gtest/gtest.h>
#include <native_soa_types.h>

#include <array>
#include <cstddef>
#include <filesystem>
#include <tuple>

namespace ioj::layout {
namespace {

auto physical_fixture() -> lispb::schema::TypeGraph {
    auto const root{std::filesystem::path{IOJ_SOURCE_DIR} / "native/lispb/native_soa"};
    auto const manifest{
        codegen::load_sources(root / "types.lispb", std::array{root / "fixture.lispb"})};
    return lispb::schema::resolve_type_graph(manifest);
}

TEST(PhysicalFacts, GeneratedCppAgreesForPointersNestedRecordsAndAlignedExternalMembers) {
    using namespace ml::native_soa_fixture;
    auto const types{physical_fixture()};
    auto profile{AbiProfile::host_common()};
    TypeFacts aligned{};
    aligned.size_bytes = sizeof(Aligned32);
    aligned.alignment_bytes = alignof(Aligned32);
    aligned.origin = FactOrigin::compiler_probe;
    aligned.provenance = "compiled differential fixture";
    profile.set("Aligned32", aligned);
    PhysicalFactsResolver resolver{types, profile};
    for (auto const& [name, size, alignment] :
         {std::tuple{"Pair", sizeof(Pair), alignof(Pair)},
          std::tuple{"Node", sizeof(Node), alignof(Node)},
          std::tuple{"First", sizeof(First), alignof(First)},
          std::tuple{"Second", sizeof(Second), alignof(Second)},
          std::tuple{"Envelope", sizeof(Envelope), alignof(Envelope)}}) {
        auto const facts{resolver.resolve(*types.find_declared("native_soa_fixture", name))};
        ASSERT_TRUE(facts.facts.has_value()) << name;
        EXPECT_EQ(facts.facts->size_bytes, size) << name;
        EXPECT_EQ(facts.facts->alignment_bytes, alignment) << name;
        EXPECT_EQ(facts.facts->origin, FactOrigin::derived);
    }
    auto const envelope{
        resolver.analyze_record(*types.find_declared("native_soa_fixture", "Envelope"))};
    EXPECT_EQ(envelope.members[0].offset_bytes, offsetof(Envelope, pairs));
    EXPECT_EQ(envelope.members[1].offset_bytes, offsetof(Envelope, node));
    EXPECT_EQ(envelope.members[2].offset_bytes, offsetof(Envelope, aligned));
    auto const rows{Analyzer::analyze_soa(
        types, *types.find_declared("native_soa_fixture", "PairRows"), Variant{}, profile, 7)};
    PairRows generated;
    generated.set_num(7);
    EXPECT_EQ(rows.columns[0].total_bytes, generated.pairs.size() * sizeof(generated.pairs[0]));
    EXPECT_EQ(rows.columns[1].total_bytes,
              generated.pointers.size() * sizeof(generated.pointers[0]));
}

TEST(PhysicalFacts, MissingPointeeFactsDoNotBlockPointersAndProfilesStaySeparate) {
    auto const types{physical_fixture()};
    auto profile{AbiProfile::host_common()};
    PhysicalFactsResolver known{types, profile};
    EXPECT_TRUE(known.resolve_spelling("Opaque*").facts.has_value());
    AbiProfile other{"other target"};
    PhysicalFactsResolver unknown{types, other};
    auto const missing{unknown.resolve_spelling("Opaque*")};
    EXPECT_FALSE(missing.facts.has_value());
    ASSERT_FALSE(missing.diagnostics.empty());
    EXPECT_EQ(missing.diagnostics.front().missing_physical_type, "Opaque*");
    EXPECT_FALSE(known.resolve_spelling("Opaque").facts.has_value());
}

TEST(PhysicalFacts, UnsupportedUsesAndReferenceStorageRemainUnknown) {
    auto const types{physical_fixture()};
    auto profile{AbiProfile::host_common()};
    PhysicalFactsResolver resolver{types, profile};
    for (auto const spelling : {"float[3]", "float(*)()", "float&", "float&&", "float Owner::*"}) {
        auto const result{resolver.resolve_spelling(spelling)};
        EXPECT_FALSE(result.facts.has_value()) << spelling;
        EXPECT_FALSE(result.diagnostics.empty()) << spelling;
    }
    TypeFacts owner{};
    owner.size_bytes = 32;
    owner.alignment_bytes = 8;
    profile.set("Owner", owner);
    PhysicalFactsResolver nested{types, profile};
    EXPECT_FALSE(nested.resolve_spelling("Owner::Inner").facts.has_value());
}

TEST(PhysicalFacts, SuppliedFactsCannotSilentlyReplaceGeneratedLayout) {
    auto const types{physical_fixture()};
    auto profile{AbiProfile::host_common()};
    TypeFacts incorrect{};
    incorrect.size_bytes = 64;
    incorrect.alignment_bytes = 8;
    profile.set("ml::native_soa_fixture::Pair", incorrect);
    auto const pair{*types.find_declared("native_soa_fixture", "Pair")};
    auto const result{PhysicalFactsResolver{types, profile}.resolve(pair)};
    EXPECT_FALSE(result.facts.has_value());
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_EQ(result.diagnostics.front().severity, DiagnosticSeverity::error);
    EXPECT_FALSE(Analyzer::analyze_record(types, pair, profile).size_bytes.has_value());
}

} // namespace
} // namespace ioj::layout
