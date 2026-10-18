#include "analyzer_test_fixtures.hpp"
#include <ioj/layout/abi_profile.hpp>
#include <ioj/layout/analyzer/numeric_delta.hpp>
#include <ioj/layout/analyzer/numeric_delta_direction.hpp>
#include <ioj/layout/type_facts.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>

namespace ioj::layout {
namespace analyzer_common_tests {

TEST(Analyzer, ReportsFactualNumericDeltas) {
    EXPECT_EQ(numeric_delta(100, 150), (NumericDelta{NumericDeltaDirection::increased, 50, 50.0}));

    EXPECT_EQ(numeric_delta(100, 50), (NumericDelta{NumericDeltaDirection::decreased, 50, 50.0}));

    EXPECT_EQ(numeric_delta(0, 1), (NumericDelta{NumericDeltaDirection::increased, 1}));
    EXPECT_FALSE(numeric_delta(std::nullopt, 1).has_value());
}

TEST(AbiProfile, ResolvesSchemaRepresentationsWithoutGuessingCycles) {
    auto abi{AbiProfile::host_common()};
    abi.set_representation("EntityId", "std::uint32_t");
    abi.set_representation("CycleA", "CycleB");
    abi.set_representation("CycleB", "CycleA");

    EXPECT_EQ(abi.find("EntityId")->size_bytes, 4);
    EXPECT_FALSE(abi.find("CycleA").has_value());
    EXPECT_FALSE(abi.find("Unknown").has_value());
}

TEST(AbiProfile, RejectsImpossibleCompleteObjectsWithoutReplacingFacts) {
    AbiProfile profile{"test"};
    TypeFacts facts{};
    facts.size_bytes = 4;
    facts.alignment_bytes = 4;
    profile.set("Value", facts);
    facts.size_bytes = 3;
    EXPECT_THROW(profile.set("Value", facts), std::invalid_argument);
    EXPECT_EQ(profile.find("Value")->size_bytes, 4);
    auto const parsed{parse_abi_profile("ioj-layout-profile 1\nname \"test\"\ntype \"Value\" 3 4 "
                                        "non-integer unknown \"import\"\n")};
    EXPECT_FALSE(parsed.has_value());
}

TEST(AbiProfile, RoundTripsExtendedAlignmentOriginsAndExplicitPointerPolicy) {
    auto profile{AbiProfile::host_common()};
    TypeFacts aligned{};
    aligned.size_bytes = 128;
    aligned.alignment_bytes = 64;
    aligned.origin = FactOrigin::manual_assumption;
    aligned.provenance = "vendor documentation, not measured";
    profile.set("Aligned", aligned);
    auto const parsed{parse_abi_profile(serialize_abi_profile(profile))};
    ASSERT_TRUE(parsed.has_value()) << parsed.error().message;
    EXPECT_EQ(parsed->find("Aligned"), aligned);
    EXPECT_EQ(parsed->object_pointer_representation(), profile.object_pointer_representation());
    EXPECT_EQ(parsed->find("float")->origin, FactOrigin::compiler_probe);
    EXPECT_THROW(profile.set_object_pointer_representation("missing"), std::invalid_argument);
    EXPECT_THROW(profile.set_object_pointer_representation("std::uint64_t"), std::invalid_argument);
}

TEST(AbiProfile, ValidatesIntegerMetadataOnBothEntryPaths) {
    AbiProfile profile{"test"};
    TypeFacts facts{};
    facts.size_bytes = 1;
    facts.alignment_bytes = 1;
    facts.unsigned_value_bits = 9;
    EXPECT_THROW(profile.set("Value", facts), std::invalid_argument);
    facts.integer_signed = false;
    EXPECT_THROW(profile.set("Value", facts), std::invalid_argument);
    facts.unsigned_value_bits.reset();
    profile.set("Value", facts);
    auto const parsed{parse_abi_profile(serialize_abi_profile(profile))};
    ASSERT_TRUE(parsed.has_value()) << parsed.error().message;
    EXPECT_EQ(parsed->find("Value"), facts);
}

TEST(AbiProfile, ExposesExplicitX86MemoryFactsWithProvenance) {
    auto const abi{AbiProfile::host_common()};

    EXPECT_FALSE(abi.name().empty());
    EXPECT_TRUE(abi.identity().platform.has_value());
    EXPECT_TRUE(abi.identity().architecture.has_value());
    EXPECT_FALSE(abi.identity().abi.has_value());
    EXPECT_TRUE(abi.identity().compiler.has_value());
    EXPECT_TRUE(abi.identity().build_configuration.has_value());
    EXPECT_EQ(abi.memory_facts().cache_line_bytes, 64);
    EXPECT_EQ(abi.memory_facts().page_bytes, 4'096);
    EXPECT_FALSE(abi.memory_facts().provenance.empty());
    EXPECT_FALSE(abi.find("std::uint8_t")->provenance.empty());
    EXPECT_EQ(abi.find("std::uint8_t")->integer_signed, false);
    EXPECT_EQ(abi.find("std::int8_t")->integer_signed, true);
    EXPECT_FALSE(abi.find("float")->integer_signed.has_value());
}

TEST(AbiProfile, RetainsFullyDescribedIdentitySeparatelyFromFactProvenance) {
    AbiProfileIdentity const identity{.platform = "Windows",
                                      .architecture = "x86-64",
                                      .abi = "Microsoft x64",
                                      .compiler = "MSVC 19.44",
                                      .build_configuration = "Shipping"};
    AbiProfile abi{"Windows shipping", identity};
    abi.set("word",
            {.size_bytes = 8,
             .alignment_bytes = 8,
             .integer_signed = false,
             .unsigned_value_bits = 64,
             .provenance = "generated probe output"});
    abi.set_memory_facts(
        {.cache_line_bytes = 64, .page_bytes = 4'096, .provenance = "selected machine profile"});

    EXPECT_EQ(abi.identity(), identity);
    EXPECT_EQ(abi.find("word")->provenance, "generated probe output");
    EXPECT_EQ(abi.memory_facts().provenance, "selected machine profile");
}

TEST(AbiProfile, LeavesUnspecifiedIdentityAndFactsUnknown) {
    AbiProfileIdentity identity;
    identity.architecture = "x86-64";
    AbiProfile abi{"partial", std::move(identity)};
    abi.set("word",
            {.size_bytes = 8,
             .alignment_bytes = 8,
             .integer_signed = false,
             .unsigned_value_bits = 64,
             .provenance = {}});

    EXPECT_EQ(abi.identity(), (AbiProfileIdentity{.architecture = "x86-64"}));
    EXPECT_TRUE(abi.find("word")->provenance.empty());
    EXPECT_EQ(abi.memory_facts(), MemoryFacts{});
}

TEST(AbiProfile, ParsesAndSerializesGeneratedTargetFactsDeterministically) {
    auto const source{R"profile(ioj-layout-profile 1
name "Windows x64 Debug"
identity platform "Windows"
identity architecture "AMD64"
identity abi "Microsoft x64"
identity compiler "MSVC 19.44"
identity build-configuration "Debug"
type "float" 4 4 non-integer unknown "probe float"
type "std::uint32_t" 4 4 unsigned 32 "probe uint32"
representation "EntityId" "ProjectWord"
representation "ProjectWord" "std::uint32_t"
memory cache-line 64
memory page 4096
memory l1-data 32768
memory l3 0
memory-provenance "machine profile"
)profile"};

    auto const parsed{parse_abi_profile(source)};
    ASSERT_TRUE(parsed.has_value()) << parsed.error().message;
    EXPECT_EQ(parsed->name(), "Windows x64 Debug");
    EXPECT_EQ(parsed->identity(),
              (AbiProfileIdentity{.platform = "Windows",
                                  .architecture = "AMD64",
                                  .abi = "Microsoft x64",
                                  .compiler = "MSVC 19.44",
                                  .build_configuration = "Debug"}));
    EXPECT_EQ(parsed->find("EntityId"),
              (TypeFacts{.size_bytes = 4,
                         .alignment_bytes = 4,
                         .integer_signed = false,
                         .unsigned_value_bits = 32,
                         .provenance = "probe uint32"}));
    EXPECT_EQ(parsed->find("float")->integer_signed, std::nullopt);
    EXPECT_EQ(parsed->memory_facts(),
              (MemoryFacts{.cache_line_bytes = 64,
                           .page_bytes = 4096,
                           .l1_data_cache_bytes = 32768,
                           .l3_cache_bytes = 0,
                           .provenance = "machine profile"}));

    auto const serialized{serialize_abi_profile(*parsed)};
    auto const reparsed{parse_abi_profile(serialized)};
    ASSERT_TRUE(reparsed.has_value()) << reparsed.error().message;
    EXPECT_EQ(*reparsed, *parsed);
    EXPECT_EQ(serialize_abi_profile(*reparsed), serialized);
}

TEST(AbiProfile, KeepsOmittedGeneratedFactsUnknown) {
    auto const parsed{parse_abi_profile(R"profile(ioj-layout-profile 1
name "Partial target"
identity architecture "arm64"
type "word" 8 8 signed unknown ""
memory-provenance ""
)profile")};

    ASSERT_TRUE(parsed.has_value()) << parsed.error().message;
    EXPECT_EQ(parsed->identity(), (AbiProfileIdentity{.architecture = "arm64"}));
    EXPECT_EQ(parsed->memory_facts(), MemoryFacts{});
    EXPECT_EQ(parsed->find("word"),
              (TypeFacts{.size_bytes = 8, .alignment_bytes = 8, .integer_signed = true}));
    EXPECT_FALSE(parsed->find("unknown").has_value());
}

TEST(AbiProfile, RejectsMalformedOrContradictoryGeneratedFactsWithoutPublishing) {
    std::array const invalid_profiles{
        "name \"missing header\"\n",
        "ioj-layout-profile 1\nname \"zero size\"\ntype \"word\" 0 1 unsigned 1 \"probe\"\n",
        "ioj-layout-profile 1\nname \"bad alignment\"\ntype \"word\" 4 3 unsigned 32 \"probe\"\n",
        "ioj-layout-profile 1\nname \"bad signed bits\"\ntype \"word\" 4 4 signed 31 \"probe\"\n",
        "ioj-layout-profile 1\nname \"wide bits\"\ntype \"word\" 1 1 unsigned 9 \"probe\"\n",
        "ioj-layout-profile 1\nname \"duplicate\"\ntype \"word\" 4 4 unsigned 32 \"a\"\ntype "
        "\"word\" 4 4 unsigned 32 \"b\"\n",
        "ioj-layout-profile 1\nname \"duplicate identity\"\nidentity abi \"one\"\nidentity abi "
        "\"two\"\n",
        "ioj-layout-profile 1\nname \"cycle\"\nrepresentation \"A\" \"B\"\nrepresentation \"B\" "
        "\"A\"\n",
        "ioj-layout-profile 1\nname \"zero memory\"\nmemory cache-line 0\n",
        "ioj-layout-profile 1\nname \"unknown directive\"\nmystery 1\n"};

    for (auto const* const source : invalid_profiles) {
        SCOPED_TRACE(source);
        auto const parsed{parse_abi_profile(source)};
        ASSERT_FALSE(parsed.has_value());
        EXPECT_FALSE(parsed.error().message.empty());
    }
}

}
}
