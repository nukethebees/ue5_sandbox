#include "analyzer_test_fixtures.hpp"

namespace ioj::layout {
namespace {

TEST(Analyzer, ReportsFactualNumericDeltas) {
    auto const increased{numeric_delta(100, 150)};
    ASSERT_TRUE(increased.has_value());
    EXPECT_EQ(increased->direction, NumericDeltaDirection::increased);
    EXPECT_EQ(increased->magnitude, 50);
    EXPECT_DOUBLE_EQ(*increased->percentage, 50.0);

    auto const decreased{numeric_delta(150, 100)};
    ASSERT_TRUE(decreased.has_value());
    EXPECT_EQ(decreased->direction, NumericDeltaDirection::decreased);
    EXPECT_EQ(decreased->magnitude, 50);

    auto const zero_baseline{numeric_delta(0, 1)};
    ASSERT_TRUE(zero_baseline.has_value());
    EXPECT_FALSE(zero_baseline->percentage.has_value());
    EXPECT_FALSE(numeric_delta(std::nullopt, 1).has_value());
}

TEST(AbiProfile, ResolvesSchemaRepresentationsWithoutGuessingCycles) {
    auto abi{AbiProfile::host_common()};
    abi.set_representation("EntityUniqueId", "std::uint32_t");
    abi.set_representation("CycleA", "CycleB");
    abi.set_representation("CycleB", "CycleA");

    EXPECT_EQ(abi.find("EntityUniqueId")->size_bytes, 4);
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
    AbiProfile abi{"Windows shipping",
                   {.platform = "Windows",
                    .architecture = "x86-64",
                    .abi = "Microsoft x64",
                    .compiler = "MSVC 19.44",
                    .build_configuration = "Shipping"}};
    abi.set("word",
            {.size_bytes = 8,
             .alignment_bytes = 8,
             .integer_signed = false,
             .unsigned_value_bits = 64,
             .provenance = "generated probe output"});
    abi.set_memory_facts({.cache_line_bytes = 64,
                          .page_bytes = 4'096,
                          .l1_data_cache_bytes = std::nullopt,
                          .l2_cache_bytes = std::nullopt,
                          .l3_cache_bytes = std::nullopt,
                          .provenance = "selected machine profile"});

    EXPECT_EQ(abi.identity().platform, "Windows");
    EXPECT_EQ(abi.identity().architecture, "x86-64");
    EXPECT_EQ(abi.identity().abi, "Microsoft x64");
    EXPECT_EQ(abi.identity().compiler, "MSVC 19.44");
    EXPECT_EQ(abi.identity().build_configuration, "Shipping");
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

    EXPECT_FALSE(abi.identity().platform.has_value());
    EXPECT_EQ(abi.identity().architecture, "x86-64");
    EXPECT_FALSE(abi.identity().abi.has_value());
    EXPECT_FALSE(abi.identity().compiler.has_value());
    EXPECT_FALSE(abi.identity().build_configuration.has_value());
    EXPECT_TRUE(abi.find("word")->provenance.empty());
    EXPECT_FALSE(abi.memory_facts().cache_line_bytes.has_value());
    EXPECT_FALSE(abi.memory_facts().page_bytes.has_value());
    EXPECT_TRUE(abi.memory_facts().provenance.empty());
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
    EXPECT_EQ(parsed->identity().platform, "Windows");
    EXPECT_EQ(parsed->identity().architecture, "AMD64");
    EXPECT_EQ(parsed->identity().abi, "Microsoft x64");
    EXPECT_EQ(parsed->identity().compiler, "MSVC 19.44");
    EXPECT_EQ(parsed->identity().build_configuration, "Debug");
    ASSERT_TRUE(parsed->find("EntityId").has_value());
    EXPECT_EQ(parsed->find("EntityId")->size_bytes, 4);
    EXPECT_EQ(parsed->find("EntityId")->alignment_bytes, 4);
    EXPECT_EQ(parsed->find("EntityId")->integer_signed, false);
    EXPECT_EQ(parsed->find("EntityId")->unsigned_value_bits, 32);
    EXPECT_EQ(parsed->find("EntityId")->provenance, "probe uint32");
    EXPECT_EQ(parsed->find("float")->integer_signed, std::nullopt);
    EXPECT_EQ(parsed->memory_facts().cache_line_bytes, 64);
    EXPECT_EQ(parsed->memory_facts().page_bytes, 4'096);
    EXPECT_EQ(parsed->memory_facts().l1_data_cache_bytes, 32'768);
    EXPECT_EQ(parsed->memory_facts().l2_cache_bytes, std::nullopt);
    EXPECT_EQ(parsed->memory_facts().l3_cache_bytes, 0);
    EXPECT_EQ(parsed->memory_facts().provenance, "machine profile");

    auto const serialized{serialize_abi_profile(*parsed)};
    auto const reparsed{parse_abi_profile(serialized)};
    ASSERT_TRUE(reparsed.has_value()) << reparsed.error().message;
    EXPECT_EQ(reparsed->name(), parsed->name());
    EXPECT_EQ(reparsed->identity(), parsed->identity());
    EXPECT_EQ(reparsed->types(), parsed->types());
    EXPECT_EQ(reparsed->representations(), parsed->representations());
    EXPECT_EQ(reparsed->memory_facts(), parsed->memory_facts());
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
    EXPECT_FALSE(parsed->identity().platform.has_value());
    EXPECT_EQ(parsed->identity().architecture, "arm64");
    EXPECT_FALSE(parsed->identity().abi.has_value());
    EXPECT_FALSE(parsed->memory_facts().cache_line_bytes.has_value());
    EXPECT_FALSE(parsed->memory_facts().page_bytes.has_value());
    EXPECT_TRUE(parsed->memory_facts().provenance.empty());
    ASSERT_TRUE(parsed->find("word").has_value());
    EXPECT_EQ(parsed->find("word")->integer_signed, true);
    EXPECT_FALSE(parsed->find("word")->unsigned_value_bits.has_value());
    EXPECT_TRUE(parsed->find("word")->provenance.empty());
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

TEST(Analyzer, ReportsAggregateWorkingSetFitForExplicitCacheCapacities) {
    auto abi{AbiProfile::host_common()};
    abi.set_memory_facts({.cache_line_bytes = 64,
                          .page_bytes = 4'096,
                          .l1_data_cache_bytes = 4'096,
                          .l2_cache_bytes = 8'192,
                          .l3_cache_bytes = 16'384,
                          .provenance = "synthetic cache profile"});

    auto const packed_fixture{entity_id_type()};
    auto const packed{
        Analyzer::analyze_packed(packed_fixture.types, packed_fixture.type, Variant{}, abi, 1'024)};
    EXPECT_EQ(packed.aggregate.cache_capacity.working_set_bytes, 4'096);
    EXPECT_EQ(packed.aggregate.cache_capacity.fits_l1_data, true);
    EXPECT_EQ(packed.aggregate.cache_capacity.fits_l2, true);
    EXPECT_EQ(packed.aggregate.cache_capacity.fits_l3, true);

    auto const record_fixture{record_type({codegen::RecordSchema{
        .name = "Record",
        .members = {record_member("small", "std::uint8_t"), record_member("wide", "std::uint32_t")},
        .export_specifier = std::nullopt}})};
    auto const record{
        Analyzer::analyze_record(record_fixture.types, record_fixture.type, abi, 1'025)};
    EXPECT_EQ(record.aggregate.cache_capacity.working_set_bytes, 8'200);
    EXPECT_EQ(record.aggregate.cache_capacity.fits_l1_data, false);
    EXPECT_EQ(record.aggregate.cache_capacity.fits_l2, false);
    EXPECT_EQ(record.aggregate.cache_capacity.fits_l3, true);

    auto const soa_fixture{soa_type()};
    auto const soa{
        Analyzer::analyze_soa(soa_fixture.types, soa_fixture.type, Variant{}, abi, 1'000)};
    EXPECT_EQ(soa.cache_capacity.working_set_bytes, 24'000);
    EXPECT_EQ(soa.cache_capacity.fits_l1_data, false);
    EXPECT_EQ(soa.cache_capacity.fits_l2, false);
    EXPECT_EQ(soa.cache_capacity.fits_l3, false);
}

TEST(Analyzer, LeavesCacheFitUnknownWhenCapacityOrWorkingSetIsUnknown) {
    auto const packed_fixture{entity_id_type()};
    auto const packed{Analyzer::analyze_packed(
        packed_fixture.types, packed_fixture.type, Variant{}, AbiProfile::host_common(), 1)};
    EXPECT_FALSE(packed.aggregate.cache_capacity.l1_data_capacity_bytes.has_value());
    EXPECT_FALSE(packed.aggregate.cache_capacity.fits_l1_data.has_value());
    EXPECT_FALSE(packed.aggregate.cache_capacity.fits_l2.has_value());
    EXPECT_FALSE(packed.aggregate.cache_capacity.fits_l3.has_value());

    auto const soa_fixture{soa_type({{"unknown", "UnknownUserType"}})};
    AbiProfile abi{"unknown working set"};
    abi.set_memory_facts({.cache_line_bytes = std::nullopt,
                          .page_bytes = std::nullopt,
                          .l1_data_cache_bytes = 32'768,
                          .l2_cache_bytes = 1'048'576,
                          .l3_cache_bytes = std::nullopt,
                          .provenance = "partial synthetic profile"});
    auto const soa{
        Analyzer::analyze_soa(soa_fixture.types, soa_fixture.type, Variant{}, abi, 1'000)};
    EXPECT_FALSE(soa.cache_capacity.working_set_bytes.has_value());
    EXPECT_FALSE(soa.cache_capacity.fits_l1_data.has_value());
    EXPECT_FALSE(soa.cache_capacity.fits_l2.has_value());
    EXPECT_FALSE(soa.cache_capacity.fits_l3.has_value());
}

} // namespace
} // namespace ioj::layout
