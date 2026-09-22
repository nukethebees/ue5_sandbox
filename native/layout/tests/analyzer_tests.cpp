#include "analyzer_test_fixtures.hpp"

namespace ioj::layout {
namespace {

TEST(EnumAnalyzer, DerivesUnsignedImplicitValuesAndCountSentinelWidth) {
    auto const fixture{enum_domain_type({enum_value("First", "0"),
                                         enum_value("Second", std::nullopt),
                                         enum_value("Seventh", "7"),
                                         enum_value("Count", std::nullopt)},
                                        "Count")};

    auto const analysis{
        Analyzer::analyze_enum(fixture.types, fixture.type, AbiProfile::host_common())};

    EXPECT_EQ(analysis.live_value_count, 3);
    EXPECT_EQ(analysis.reserved_value_count, 1);
    ASSERT_EQ(analysis.enumerators.size(), 4U);
    EXPECT_EQ(analysis.enumerators[1].code, (EnumCodeValue{.negative = false, .magnitude = 1}));
    EXPECT_EQ(analysis.enumerators[3].code, (EnumCodeValue{.negative = false, .magnitude = 8}));
    EXPECT_EQ(analysis.minimum_value, (EnumCodeValue{.negative = false, .magnitude = 0}));
    EXPECT_EQ(analysis.maximum_value, (EnumCodeValue{.negative = false, .magnitude = 8}));
    EXPECT_EQ(analysis.signed_domain, false);
    EXPECT_EQ(analysis.minimum_required_bits, 4);
    EXPECT_FALSE(analysis.declared_bit_width.has_value());
    EXPECT_EQ(analysis.effective_bit_width, 4);
    EXPECT_EQ(analysis.semantic_width_can_represent_domain, true);
    EXPECT_EQ(analysis.unused_semantic_codes, 12);
    EXPECT_EQ(analysis.backing_bits, 8);
    EXPECT_EQ(analysis.backing_can_represent_domain, true);
    EXPECT_EQ(analysis.unused_backing_codes, 252);
    EXPECT_EQ(analysis.aggregate.element_count, 1);
    EXPECT_EQ(analysis.aggregate.total_storage_bytes, 1);
    EXPECT_EQ(analysis.aggregate.minimum_cache_lines, 1);
    EXPECT_EQ(analysis.aggregate.complete_elements_per_cache_line, 64);
    EXPECT_EQ(analysis.aggregate.cache_line_straddling_elements, 0);
    EXPECT_EQ(analysis.aggregate.minimum_pages, 1);
    EXPECT_EQ(analysis.aggregate.complete_elements_per_page, 4'096);
    EXPECT_EQ(analysis.aggregate.page_straddling_elements, 0);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(EnumAnalyzer, ReportsExplicitSemanticWidthSeparatelyFromPhysicalBacking) {
    auto const fixture{enum_domain_type(
        {enum_value("Zero", "0"), enum_value("Seven", "7")}, std::nullopt, "std::uint8_t", 4)};

    auto const analysis{
        Analyzer::analyze_enum(fixture.types, fixture.type, AbiProfile::host_common())};

    EXPECT_EQ(analysis.minimum_required_bits, 3);
    EXPECT_EQ(analysis.declared_bit_width, 4);
    EXPECT_EQ(analysis.effective_bit_width, 4);
    EXPECT_EQ(analysis.semantic_width_can_represent_domain, true);
    EXPECT_EQ(analysis.unused_semantic_codes, 14);
    EXPECT_EQ(analysis.backing_bits, 8);
    EXPECT_EQ(analysis.unused_backing_codes, 254);
}

TEST(EnumAnalyzer, ReportsDerivedCppBackingSeparatelyFromSemanticWidth) {
    auto const fixture{enum_domain_type({enum_value("Zero", "0"), enum_value("Maximum", "4095")},
                                        std::nullopt,
                                        std::nullopt,
                                        12,
                                        false)};

    auto const analysis{
        Analyzer::analyze_enum(fixture.types, fixture.type, AbiProfile::host_common())};

    EXPECT_EQ(analysis.declared_bit_width, 12);
    EXPECT_EQ(analysis.effective_bit_width, 12);
    EXPECT_EQ(analysis.backing_type, "std::uint16_t");
    EXPECT_EQ(analysis.backing_bits, 16);
    EXPECT_EQ(analysis.backing_can_represent_domain, true);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(EnumAnalyzer, SeparatesNamedSentinelsFromCountSentinel) {
    auto const fixture{enum_domain_type({enum_value("Ready", "0"),
                                         enum_value("Invalid", "14", true),
                                         enum_value("Pending", "15", true),
                                         enum_value("Count", "16")},
                                        "Count")};

    auto const analysis{
        Analyzer::analyze_enum(fixture.types, fixture.type, AbiProfile::host_common())};

    EXPECT_EQ(analysis.live_value_count, 1);
    EXPECT_EQ(analysis.reserved_value_count, 3);
    EXPECT_EQ(analysis.minimum_required_bits, 5);
    ASSERT_EQ(analysis.enumerators.size(), 4U);
    EXPECT_TRUE(analysis.enumerators[1].sentinel);
    EXPECT_FALSE(analysis.enumerators[1].count_sentinel);
    EXPECT_TRUE(analysis.enumerators[2].sentinel);
    EXPECT_FALSE(analysis.enumerators[2].count_sentinel);
    EXPECT_FALSE(analysis.enumerators[3].sentinel);
    EXPECT_TRUE(analysis.enumerators[3].count_sentinel);
}

TEST(EnumAnalyzer, DerivesSignedTwosComplementWidth) {
    auto const fixture{enum_domain_type(
        {enum_value("Low", "-4"), enum_value("High", "+3")}, std::nullopt, "std::int8_t")};

    auto const analysis{
        Analyzer::analyze_enum(fixture.types, fixture.type, AbiProfile::host_common())};

    EXPECT_EQ(analysis.minimum_value, (EnumCodeValue{.negative = true, .magnitude = 4}));
    EXPECT_EQ(analysis.maximum_value, (EnumCodeValue{.negative = false, .magnitude = 3}));
    EXPECT_EQ(analysis.signed_domain, true);
    EXPECT_EQ(analysis.minimum_required_bits, 3);
    EXPECT_EQ(analysis.backing_bits, 8);
    EXPECT_EQ(analysis.backing_can_represent_domain, true);
    EXPECT_EQ(analysis.unused_backing_codes, 254);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(EnumAnalyzer, ExplicitSignednessConstrainsSemanticWidth) {
    auto const signed_fixture{enum_domain_type({enum_value("Zero", "0"), enum_value("Seven", "7")},
                                               std::nullopt,
                                               "std::uint8_t",
                                               std::nullopt,
                                               true)};
    auto const signed_analysis{Analyzer::analyze_enum(
        signed_fixture.types, signed_fixture.type, AbiProfile::host_common())};

    EXPECT_EQ(signed_analysis.declared_signedness, true);
    EXPECT_EQ(signed_analysis.signed_domain, true);
    EXPECT_EQ(signed_analysis.minimum_required_bits, 4);
}

TEST(EnumAnalyzer, KeepsExpressionAndDependentImplicitValuesUnknown) {
    auto const fixture{
        enum_domain_type({enum_value("Mask", "1 << 3"), enum_value("Next", std::nullopt)})};

    auto const analysis{
        Analyzer::analyze_enum(fixture.types, fixture.type, AbiProfile::host_common())};

    EXPECT_FALSE(analysis.enumerators[0].code.has_value());
    EXPECT_FALSE(analysis.enumerators[1].code.has_value());
    EXPECT_FALSE(analysis.minimum_required_bits.has_value());
    EXPECT_FALSE(analysis.unused_backing_codes.has_value());
    EXPECT_EQ(analysis.diagnostics.size(), 2U);
}

TEST(EnumAnalyzer, AcceptsFullUnsignedWidthAndDiagnosesImplicitOverflow) {
    auto const fixture{enum_domain_type(
        {enum_value("Maximum", "0xffffffffffffffffULL"), enum_value("Overflow", std::nullopt)},
        std::nullopt,
        "std::uint64_t")};

    auto const analysis{
        Analyzer::analyze_enum(fixture.types, fixture.type, AbiProfile::host_common())};

    EXPECT_EQ(
        analysis.enumerators[0].code,
        (EnumCodeValue{.negative = false, .magnitude = std::numeric_limits<std::uint64_t>::max()}));
    EXPECT_FALSE(analysis.enumerators[1].code.has_value());
    EXPECT_FALSE(analysis.minimum_required_bits.has_value());
    EXPECT_FALSE(analysis.diagnostics.empty());
}

TEST(EnumAnalyzer, DiagnosesDomainsThatDoNotFitBackingSignedness) {
    auto target{AbiProfile{"Narrow enum target"}};
    target.set("NarrowSigned",
               TypeFacts{.size_bytes = 1,
                         .alignment_bytes = 1,
                         .integer_signed = true,
                         .unsigned_value_bits = std::nullopt,
                         .provenance = "Test target"});
    target.set("NarrowUnsigned",
               TypeFacts{.size_bytes = 1,
                         .alignment_bytes = 1,
                         .integer_signed = false,
                         .unsigned_value_bits = 8,
                         .provenance = "Test target"});

    auto const signed_fixture{
        enum_domain_type({enum_value("Value", "200")}, std::nullopt, "NarrowSigned")};
    auto const signed_analysis{
        Analyzer::analyze_enum(signed_fixture.types, signed_fixture.type, target)};

    EXPECT_EQ(signed_analysis.minimum_required_bits, 8);
    EXPECT_EQ(signed_analysis.backing_can_represent_domain, false);
    EXPECT_FALSE(signed_analysis.diagnostics.empty());

    auto const unsigned_fixture{
        enum_domain_type({enum_value("Value", "-1")}, std::nullopt, "NarrowUnsigned")};
    auto const unsigned_analysis{
        Analyzer::analyze_enum(unsigned_fixture.types, unsigned_fixture.type, target)};

    EXPECT_EQ(unsigned_analysis.minimum_required_bits, 1);
    EXPECT_EQ(unsigned_analysis.backing_can_represent_domain, false);
    EXPECT_FALSE(unsigned_analysis.diagnostics.empty());
}

TEST(EnumAnalyzer, ComparesExplicitBackingAcrossTargetsWithoutChangingSemanticWidth) {
    auto const fixture{enum_domain_type(
        {enum_value("Zero", "0"), enum_value("Seven", "7")}, std::nullopt, "std::uint8_t", 4)};
    auto wider_target{AbiProfile::host_common()};
    wider_target.set("std::uint8_t",
                     TypeFacts{.size_bytes = 2,
                               .alignment_bytes = 2,
                               .integer_signed = false,
                               .unsigned_value_bits = 16,
                               .provenance = "Test wider target"});
    wider_target.set_memory_facts({.cache_line_bytes = 128,
                                   .page_bytes = 8'192,
                                   .l1_data_cache_bytes = 150,
                                   .l2_cache_bytes = std::nullopt,
                                   .l3_cache_bytes = std::nullopt,
                                   .provenance = "Test wider memory"});
    auto first_target{AbiProfile::host_common()};
    first_target.set_memory_facts({.cache_line_bytes = 64,
                                   .page_bytes = 4'096,
                                   .l1_data_cache_bytes = 150,
                                   .l2_cache_bytes = std::nullopt,
                                   .l3_cache_bytes = std::nullopt,
                                   .provenance = "Test first memory"});

    auto const first{Analyzer::analyze_enum(fixture.types, fixture.type, first_target, 100)};
    auto const second{Analyzer::analyze_enum(fixture.types, fixture.type, wider_target, 100)};
    auto const comparison{Analyzer::compare_enum_targets(first, second)};

    EXPECT_EQ(comparison.first.effective_bit_width, 4);
    EXPECT_EQ(comparison.second.effective_bit_width, 4);
    ASSERT_TRUE(comparison.backing_size_delta.has_value());
    EXPECT_EQ(comparison.backing_size_delta->direction, NumericDeltaDirection::increased);
    EXPECT_EQ(comparison.backing_size_delta->magnitude, 1);
    ASSERT_TRUE(comparison.backing_alignment_delta.has_value());
    EXPECT_EQ(comparison.backing_alignment_delta->magnitude, 1);
    ASSERT_TRUE(comparison.backing_value_bit_delta.has_value());
    EXPECT_EQ(comparison.backing_value_bit_delta->magnitude, 8);
    ASSERT_TRUE(comparison.backing_bit_delta.has_value());
    EXPECT_EQ(comparison.backing_bit_delta->magnitude, 8);
    EXPECT_EQ(comparison.first.unused_backing_codes, 254);
    EXPECT_EQ(comparison.second.unused_backing_codes, 65'534);
    ASSERT_TRUE(comparison.unused_backing_code_delta.has_value());
    EXPECT_EQ(comparison.unused_backing_code_delta->magnitude, 65'280);
    EXPECT_EQ(comparison.backing_fit_changed, false);
    EXPECT_EQ(comparison.first.aggregate.total_storage_bytes, 100);
    EXPECT_EQ(comparison.second.aggregate.total_storage_bytes, 200);
    ASSERT_TRUE(comparison.total_storage_delta.has_value());
    EXPECT_EQ(comparison.total_storage_delta->magnitude, 100);
    ASSERT_TRUE(comparison.cache_line_size_delta.has_value());
    EXPECT_EQ(comparison.cache_line_size_delta->magnitude, 64);
    ASSERT_TRUE(comparison.minimum_cache_line_delta.has_value());
    EXPECT_EQ(comparison.minimum_cache_line_delta->direction, NumericDeltaDirection::unchanged);
    ASSERT_TRUE(comparison.page_size_delta.has_value());
    EXPECT_EQ(comparison.page_size_delta->magnitude, 4'096);
    EXPECT_EQ(comparison.first.aggregate.cache_capacity.fits_l1_data, true);
    EXPECT_EQ(comparison.second.aggregate.cache_capacity.fits_l1_data, false);
    EXPECT_TRUE(comparison.diagnostics.empty());
}

TEST(EnumAnalyzer, ComparesDerivedBackingStorageSeparatelyFromValueCapacity) {
    auto const fixture{enum_domain_type({enum_value("Zero", "0"), enum_value("Maximum", "4095")},
                                        std::nullopt,
                                        std::nullopt,
                                        12,
                                        false)};
    auto padded_target{AbiProfile::host_common()};
    padded_target.set("std::uint16_t",
                      TypeFacts{.size_bytes = 4,
                                .alignment_bytes = 4,
                                .integer_signed = false,
                                .unsigned_value_bits = 16,
                                .provenance = "Test padded target"});

    auto const first{
        Analyzer::analyze_enum(fixture.types, fixture.type, AbiProfile::host_common())};
    auto const second{Analyzer::analyze_enum(fixture.types, fixture.type, padded_target)};
    auto const comparison{Analyzer::compare_enum_targets(first, second)};
    auto const identical{Analyzer::compare_enum_targets(first, first)};

    EXPECT_EQ(comparison.first.backing_type, "std::uint16_t");
    EXPECT_EQ(comparison.second.backing_type, "std::uint16_t");
    ASSERT_TRUE(comparison.backing_size_delta.has_value());
    EXPECT_EQ(comparison.backing_size_delta->magnitude, 2);
    ASSERT_TRUE(comparison.backing_bit_delta.has_value());
    EXPECT_EQ(comparison.backing_bit_delta->magnitude, 16);
    ASSERT_TRUE(comparison.backing_value_bit_delta.has_value());
    EXPECT_EQ(comparison.backing_value_bit_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.first.unused_backing_codes, comparison.second.unused_backing_codes);
    ASSERT_TRUE(comparison.unused_backing_code_delta.has_value());
    EXPECT_EQ(comparison.unused_backing_code_delta->direction, NumericDeltaDirection::unchanged);
    ASSERT_TRUE(identical.backing_size_delta.has_value());
    EXPECT_EQ(identical.backing_size_delta->direction, NumericDeltaDirection::unchanged);
    ASSERT_TRUE(identical.backing_alignment_delta.has_value());
    EXPECT_EQ(identical.backing_alignment_delta->direction, NumericDeltaDirection::unchanged);
    ASSERT_TRUE(identical.backing_fit_changed.has_value());
    EXPECT_FALSE(*identical.backing_fit_changed);
}

TEST(EnumAnalyzer, KeepsUnknownTargetBackingFactsUnknownInComparison) {
    auto const fixture{enum_domain_type(
        {enum_value("Zero", "0"), enum_value("Seven", "7")}, std::nullopt, "std::uint8_t", 4)};
    auto const known{
        Analyzer::analyze_enum(fixture.types, fixture.type, AbiProfile::host_common())};
    auto const unknown{
        Analyzer::analyze_enum(fixture.types, fixture.type, AbiProfile{"Unknown target"})};

    auto const comparison{Analyzer::compare_enum_targets(known, unknown)};

    EXPECT_FALSE(comparison.backing_size_delta.has_value());
    EXPECT_FALSE(comparison.backing_alignment_delta.has_value());
    EXPECT_FALSE(comparison.backing_value_bit_delta.has_value());
    EXPECT_FALSE(comparison.backing_bit_delta.has_value());
    EXPECT_FALSE(comparison.unused_backing_code_delta.has_value());
    EXPECT_FALSE(comparison.backing_fit_changed.has_value());
    EXPECT_FALSE(comparison.total_storage_delta.has_value());
    EXPECT_FALSE(comparison.minimum_cache_line_delta.has_value());
    EXPECT_FALSE(comparison.minimum_page_delta.has_value());
    EXPECT_TRUE(std::ranges::any_of(comparison.diagnostics, [](Diagnostic const& diagnostic) {
        return diagnostic.message.starts_with("Second target enum:");
    }));
}

TEST(EnumAnalyzer, RejectsMismatchedSemanticEnumAnalysesTransactionally) {
    auto const fixture{enum_domain_type(
        {enum_value("Zero", "0"), enum_value("Seven", "7")}, std::nullopt, "std::uint8_t", 4)};
    auto const first{
        Analyzer::analyze_enum(fixture.types, fixture.type, AbiProfile::host_common())};
    auto second{first};
    second.minimum_required_bits = 4;

    auto comparison{Analyzer::compare_enum_targets(first, second)};

    EXPECT_FALSE(comparison.backing_size_delta.has_value());
    EXPECT_NE(comparison.diagnostics.back().message.find("same semantic domain"),
              std::string::npos);

    second = first;
    second.enumerators[0].code = EnumCodeValue{.negative = false, .magnitude = 1};
    comparison = Analyzer::compare_enum_targets(first, second);

    EXPECT_FALSE(comparison.backing_size_delta.has_value());
    EXPECT_NE(comparison.diagnostics.back().message.find("same ordered enumerators"),
              std::string::npos);

    second = first;
    second.aggregate.element_count = 2;
    comparison = Analyzer::compare_enum_targets(first, second);

    EXPECT_FALSE(comparison.total_storage_delta.has_value());
    EXPECT_NE(comparison.diagnostics.back().message.find("different element counts"),
              std::string::npos);
}

TEST(EnumAnalyzer, KeepsStandaloneBackingAggregateOverflowUnknown) {
    auto const fixture{enum_domain_type({enum_value("Zero", "0"), enum_value("Maximum", "4095")},
                                        std::nullopt,
                                        "std::uint16_t",
                                        12,
                                        false)};

    auto const analysis{Analyzer::analyze_enum(fixture.types,
                                               fixture.type,
                                               AbiProfile::host_common(),
                                               std::numeric_limits<std::uint64_t>::max())};

    EXPECT_FALSE(analysis.aggregate.total_storage_bytes.has_value());
    EXPECT_FALSE(analysis.aggregate.minimum_cache_lines.has_value());
    EXPECT_FALSE(analysis.aggregate.minimum_pages.has_value());
    EXPECT_FALSE(analysis.aggregate.cache_capacity.working_set_bytes.has_value());
    EXPECT_TRUE(std::ranges::any_of(analysis.diagnostics, [](Diagnostic const& diagnostic) {
        return diagnostic.message.find("aggregate storage overflows") != std::string::npos;
    }));
}

TEST(IntegerScalarAnalyzer, ReportsDomainSentinelAndDerivedWidthWithoutPhysicalFacts) {
    auto const fixture{integer_scalar_type()};

    auto const analysis{Analyzer::analyze_integer_scalar(fixture.types, fixture.type)};

    EXPECT_FALSE(analysis.signedness);
    EXPECT_EQ(analysis.minimum_value, codegen::PackedIntegerValue{0});
    EXPECT_EQ(analysis.maximum_value, codegen::PackedIntegerValue{10});
    EXPECT_EQ(analysis.live_value_count, 11U);
    EXPECT_EQ(analysis.sentinel_code_count, 1U);
    EXPECT_EQ(analysis.required_code_count, 12U);
    EXPECT_EQ(analysis.minimum_required_bits, 4U);
    EXPECT_FALSE(analysis.declared_bit_width.has_value());
    EXPECT_EQ(analysis.effective_bit_width, 4U);
    EXPECT_EQ(analysis.unused_codes, 4U);
    ASSERT_EQ(analysis.named_codes.size(), 2U);
    EXPECT_TRUE(analysis.named_codes[1].sentinel);
}

TEST(IntegerScalarAnalyzer, ReportsZeroUnusedCodesForFullSixtyFourBitDomains) {
    auto make_fixture = [](bool const signedness) {
        codegen::Manifest manifest{
            .schema_version = codegen::manifest_schema_version,
            .types = {},
            .modules = {codegen::ScalarModuleSchema{
                .settings = codegen::ModuleSettings{.name = "full_domain",
                                                    .header = "FullDomain.h",
                                                    .source = std::nullopt,
                                                    .header_include = std::nullopt,
                                                    .namespace_name = std::nullopt,
                                                    .include_order = {},
                                                    .prelude_lines = {}},
                .scalars = {codegen::IntegerScalarSchema{
                    .name = "FullDomain",
                    .signedness = signedness,
                    .minimum_value = signedness ? codegen::PackedIntegerValue{(
                                                      std::numeric_limits<std::int64_t>::min)()}
                                                : codegen::PackedIntegerValue{0},
                    .maximum_value = signedness ? codegen::PackedIntegerValue{(
                                                      std::numeric_limits<std::int64_t>::max)()}
                                                : codegen::PackedIntegerValue{(
                                                      std::numeric_limits<std::uint64_t>::max)()},
                    .bit_width = std::nullopt,
                    .named_codes = {},
                    .relationship = std::nullopt}}}}};
        auto types{lispb::schema::resolve_type_graph(manifest)};
        auto const type{*types.find_declared("full_domain", "FullDomain")};
        return TypeFixture{std::move(types), type};
    };

    for (auto const signedness : {false, true}) {
        auto const fixture{make_fixture(signedness)};
        auto const analysis{Analyzer::analyze_integer_scalar(fixture.types, fixture.type)};

        EXPECT_EQ(analysis.signedness, signedness);
        EXPECT_EQ(analysis.effective_bit_width, 64U);
        EXPECT_FALSE(analysis.live_value_count.has_value());
        EXPECT_FALSE(analysis.required_code_count.has_value());
        EXPECT_EQ(analysis.unused_codes, 0U);
    }
}

TEST(IntegerScalarAnalyzer, DerivesLinkedIndexCapacityWithSentinel) {
    auto const fixture{relationship_capacity_scalar_type(
        codegen::SemanticRelationKind::index_into, 12, 4'094, 4'095)};
    std::array capacity{RelationshipTargetFacts{
        .target = fixture.target, .element_capacity = 4'000, .byte_extent = std::nullopt}};

    auto const fits{Analyzer::analyze_integer_scalar(fixture.types, fixture.packed, capacity)};
    EXPECT_EQ(fits.relationship_kind, codegen::SemanticRelationKind::index_into);
    EXPECT_EQ(fits.relationship_target, "Entities");
    EXPECT_EQ(fits.relationship_target_extent, 4'000);
    EXPECT_EQ(fits.relationship_live_value_count,
              (ExactCodeCount{.value = 4'000, .two_to_64 = false}));
    EXPECT_EQ(fits.relationship_required_code_count,
              (ExactCodeCount{.value = 4'001, .two_to_64 = false}));
    EXPECT_EQ(fits.relationship_minimum_required_bits, 12);
    EXPECT_EQ(fits.relationship_width_sufficient, true);
    EXPECT_EQ(fits.relationship_code_space_capacity_limit, 4'095);
    EXPECT_EQ(fits.relationship_capacity_headroom, 95);
    EXPECT_EQ(fits.relationship_semantic_capacity_limit, 4'095);
    EXPECT_EQ(fits.relationship_sentinel_capacity_limit, 4'095);
    EXPECT_EQ(fits.relationship_effective_capacity_limit, 4'095);
    EXPECT_EQ(fits.relationship_effective_capacity_headroom, 95);
    EXPECT_TRUE(fits.diagnostics.empty());

    capacity.front().element_capacity = 4'096;
    auto const insufficient{
        Analyzer::analyze_integer_scalar(fixture.types, fixture.packed, capacity)};
    EXPECT_EQ(insufficient.relationship_minimum_required_bits, 13);
    EXPECT_EQ(insufficient.relationship_width_sufficient, false);
    EXPECT_EQ(insufficient.relationship_code_space_capacity_limit, 4'095);
    EXPECT_FALSE(insufficient.relationship_capacity_headroom.has_value());
    EXPECT_EQ(insufficient.relationship_effective_capacity_limit, 4'095);
    EXPECT_FALSE(insufficient.relationship_effective_capacity_headroom.has_value());
    EXPECT_FALSE(insufficient.diagnostics.empty());
}

TEST(IntegerScalarAnalyzer, ComparesLinkedCapacityAcrossWidthThreshold) {
    auto const fixture{relationship_capacity_scalar_type(
        codegen::SemanticRelationKind::index_into, 12, 4'094, 4'095)};
    std::array const first_capacity{RelationshipTargetFacts{
        .target = fixture.target, .element_capacity = 4'000, .byte_extent = std::nullopt}};
    std::array const second_capacity{RelationshipTargetFacts{
        .target = fixture.target, .element_capacity = 4'096, .byte_extent = std::nullopt}};

    auto const comparison{Analyzer::compare_integer_scalar_capacity(
        fixture.types, fixture.packed, first_capacity, second_capacity)};

    EXPECT_EQ(comparison.first.relationship_target_extent, 4'000);
    EXPECT_EQ(comparison.second.relationship_target_extent, 4'096);
    ASSERT_TRUE(comparison.relationship_target_extent_delta.has_value());
    EXPECT_EQ(comparison.relationship_target_extent_delta->direction,
              NumericDeltaDirection::increased);
    EXPECT_EQ(comparison.relationship_target_extent_delta->magnitude, 96);
    EXPECT_EQ(comparison.first.relationship_required_code_count,
              (ExactCodeCount{.value = 4'001, .two_to_64 = false}));
    EXPECT_EQ(comparison.second.relationship_required_code_count,
              (ExactCodeCount{.value = 4'097, .two_to_64 = false}));
    EXPECT_EQ(comparison.first.relationship_minimum_required_bits, 12);
    EXPECT_EQ(comparison.second.relationship_minimum_required_bits, 13);
    ASSERT_TRUE(comparison.relationship_minimum_required_bit_delta.has_value());
    EXPECT_EQ(comparison.relationship_minimum_required_bit_delta->direction,
              NumericDeltaDirection::increased);
    EXPECT_EQ(comparison.relationship_minimum_required_bit_delta->magnitude, 1);
    EXPECT_EQ(comparison.first.relationship_width_sufficient, true);
    EXPECT_EQ(comparison.second.relationship_width_sufficient, false);
    EXPECT_EQ(comparison.first.relationship_code_space_capacity_limit, 4'095);
    EXPECT_EQ(comparison.second.relationship_code_space_capacity_limit, 4'095);
    EXPECT_EQ(comparison.first.relationship_capacity_headroom, 95);
    EXPECT_FALSE(comparison.second.relationship_capacity_headroom.has_value());
    ASSERT_TRUE(comparison.relationship_code_space_capacity_limit_delta.has_value());
    EXPECT_EQ(comparison.relationship_code_space_capacity_limit_delta->direction,
              NumericDeltaDirection::unchanged);
    EXPECT_FALSE(comparison.relationship_capacity_headroom_delta.has_value());
    EXPECT_EQ(comparison.first.relationship_effective_capacity_limit, 4'095);
    EXPECT_EQ(comparison.second.relationship_effective_capacity_limit, 4'095);
    EXPECT_EQ(comparison.first.relationship_effective_capacity_headroom, 95);
    EXPECT_FALSE(comparison.second.relationship_effective_capacity_headroom.has_value());
    ASSERT_TRUE(comparison.relationship_effective_capacity_limit_delta.has_value());
    EXPECT_EQ(comparison.relationship_effective_capacity_limit_delta->direction,
              NumericDeltaDirection::unchanged);
    EXPECT_FALSE(comparison.relationship_effective_capacity_headroom_delta.has_value());
    EXPECT_EQ(comparison.relationship_width_fit_changed, true);
}

TEST(IntegerScalarAnalyzer, DerivesElementAndByteOffsetExtents) {
    auto const elements{
        relationship_capacity_scalar_type(codegen::SemanticRelationKind::offset_into, 12, 4'095)};
    std::array const element_facts{RelationshipTargetFacts{
        .target = elements.target, .element_capacity = 4'000, .byte_extent = 8'192}};
    auto const element_analysis{
        Analyzer::analyze_integer_scalar(elements.types, elements.packed, element_facts)};
    EXPECT_EQ(element_analysis.relationship_unit, codegen::SemanticRelationUnit::elements);
    EXPECT_EQ(element_analysis.relationship_target_extent, 4'000);
    EXPECT_EQ(element_analysis.relationship_live_value_count,
              (ExactCodeCount{.value = 4'000, .two_to_64 = false}));
    EXPECT_EQ(element_analysis.relationship_minimum_required_bits, 12);
    EXPECT_EQ(element_analysis.relationship_width_sufficient, true);

    auto const bytes{relationship_capacity_scalar_type(codegen::SemanticRelationKind::offset_into,
                                                       12,
                                                       4'095,
                                                       std::nullopt,
                                                       codegen::SemanticRelationUnit::bytes)};
    std::array const byte_facts{RelationshipTargetFacts{
        .target = bytes.target, .element_capacity = 4'000, .byte_extent = 8'192}};
    auto const byte_analysis{
        Analyzer::analyze_integer_scalar(bytes.types, bytes.packed, byte_facts)};
    EXPECT_EQ(byte_analysis.relationship_unit, codegen::SemanticRelationUnit::bytes);
    EXPECT_EQ(byte_analysis.relationship_target_extent, 8'192);
    EXPECT_EQ(byte_analysis.relationship_live_value_count,
              (ExactCodeCount{.value = 8'192, .two_to_64 = false}));
    EXPECT_EQ(byte_analysis.relationship_minimum_required_bits, 13);
    EXPECT_EQ(byte_analysis.relationship_width_sufficient, false);

    std::array const no_byte_extent{RelationshipTargetFacts{
        .target = bytes.target, .element_capacity = 4'000, .byte_extent = std::nullopt}};
    auto const unknown{Analyzer::analyze_integer_scalar(bytes.types, bytes.packed, no_byte_extent)};
    EXPECT_FALSE(unknown.relationship_target_extent.has_value());
    EXPECT_FALSE(unknown.relationship_minimum_required_bits.has_value());
}

TEST(IntegerScalarAnalyzer, ComparesByteOffsetWidthThreshold) {
    auto const fixture{relationship_capacity_scalar_type(codegen::SemanticRelationKind::offset_into,
                                                         12,
                                                         4'095,
                                                         std::nullopt,
                                                         codegen::SemanticRelationUnit::bytes)};
    std::array const first{RelationshipTargetFacts{
        .target = fixture.target, .element_capacity = 1'000, .byte_extent = 4'096}};
    std::array const second{RelationshipTargetFacts{
        .target = fixture.target, .element_capacity = 1'000, .byte_extent = 4'097}};

    auto const comparison{
        Analyzer::compare_integer_scalar_capacity(fixture.types, fixture.packed, first, second)};

    EXPECT_EQ(comparison.first.relationship_target_extent, 4'096);
    EXPECT_EQ(comparison.second.relationship_target_extent, 4'097);
    EXPECT_EQ(comparison.first.relationship_minimum_required_bits, 12);
    EXPECT_EQ(comparison.second.relationship_minimum_required_bits, 13);
    EXPECT_EQ(comparison.first.relationship_width_sufficient, true);
    EXPECT_EQ(comparison.second.relationship_width_sufficient, false);
    EXPECT_EQ(comparison.relationship_width_fit_changed, true);
}

TEST(IntegerScalarAnalyzer, HandlesLinkedCountExactAndBeyondTwoTo64) {
    auto const full{relationship_capacity_scalar_type(
        codegen::SemanticRelationKind::count_of, 64, (std::numeric_limits<std::uint64_t>::max)())};
    std::array const capacity{
        RelationshipTargetFacts{.target = full.target,
                                .element_capacity = (std::numeric_limits<std::uint64_t>::max)(),
                                .byte_extent = std::nullopt}};

    auto const exact{Analyzer::analyze_integer_scalar(full.types, full.packed, capacity)};
    EXPECT_EQ(exact.relationship_live_value_count, (ExactCodeCount{.value = 0, .two_to_64 = true}));
    EXPECT_EQ(exact.relationship_required_code_count,
              (ExactCodeCount{.value = 0, .two_to_64 = true}));
    EXPECT_EQ(exact.relationship_minimum_required_bits, 64);
    EXPECT_EQ(exact.relationship_width_sufficient, true);
    EXPECT_EQ(exact.relationship_code_space_capacity_limit,
              (std::numeric_limits<std::uint64_t>::max)());
    EXPECT_EQ(exact.relationship_capacity_headroom, 0);
    EXPECT_EQ(exact.relationship_semantic_capacity_limit,
              (std::numeric_limits<std::uint64_t>::max)());
    EXPECT_EQ(exact.relationship_sentinel_capacity_limit,
              (std::numeric_limits<std::uint64_t>::max)());
    EXPECT_EQ(exact.relationship_effective_capacity_limit,
              (std::numeric_limits<std::uint64_t>::max)());
    EXPECT_EQ(exact.relationship_effective_capacity_headroom, 0);
    EXPECT_TRUE(exact.diagnostics.empty());

    auto const with_sentinel{
        relationship_capacity_scalar_type(codegen::SemanticRelationKind::count_of,
                                          64,
                                          (std::numeric_limits<std::uint64_t>::max)() - 1,
                                          (std::numeric_limits<std::uint64_t>::max)())};
    std::array const overflowing_capacity{
        RelationshipTargetFacts{.target = with_sentinel.target,
                                .element_capacity = (std::numeric_limits<std::uint64_t>::max)(),
                                .byte_extent = std::nullopt}};
    auto const beyond{Analyzer::analyze_integer_scalar(
        with_sentinel.types, with_sentinel.packed, overflowing_capacity)};
    EXPECT_EQ(beyond.relationship_live_value_count,
              (ExactCodeCount{.value = 0, .two_to_64 = true}));
    EXPECT_FALSE(beyond.relationship_required_code_count.has_value());
    EXPECT_EQ(beyond.relationship_minimum_required_bits, 65);
    EXPECT_EQ(beyond.relationship_width_sufficient, false);
    EXPECT_EQ(beyond.relationship_code_space_capacity_limit,
              (std::numeric_limits<std::uint64_t>::max)() - 1);
    EXPECT_FALSE(beyond.relationship_capacity_headroom.has_value());
    EXPECT_EQ(beyond.relationship_semantic_capacity_limit,
              (std::numeric_limits<std::uint64_t>::max)() - 1);
    EXPECT_EQ(beyond.relationship_sentinel_capacity_limit,
              (std::numeric_limits<std::uint64_t>::max)() - 1);
    EXPECT_EQ(beyond.relationship_effective_capacity_limit,
              (std::numeric_limits<std::uint64_t>::max)() - 1);
    EXPECT_FALSE(beyond.relationship_effective_capacity_headroom.has_value());
    EXPECT_FALSE(beyond.diagnostics.empty());
}

TEST(IntegerScalarAnalyzer, SeparatesSemanticSentinelAndCodeSpaceCapacityLimits) {
    auto const fixture{
        relationship_capacity_scalar_type(codegen::SemanticRelationKind::index_into, 12, 100, 200)};
    std::array const capacity{RelationshipTargetFacts{
        .target = fixture.target, .element_capacity = 50, .byte_extent = std::nullopt}};

    auto const analysis{Analyzer::analyze_integer_scalar(fixture.types, fixture.packed, capacity)};

    EXPECT_EQ(analysis.relationship_code_space_capacity_limit, 4'095);
    EXPECT_EQ(analysis.relationship_capacity_headroom, 4'045);
    EXPECT_EQ(analysis.relationship_semantic_capacity_limit, 101);
    EXPECT_EQ(analysis.relationship_sentinel_capacity_limit, 200);
    EXPECT_EQ(analysis.relationship_effective_capacity_limit, 101);
    EXPECT_EQ(analysis.relationship_effective_capacity_headroom, 51);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(OptionalSentinelAnalyzer, ReportsCodeSpaceAndScaledPayloadWithoutAbiFacts) {
    auto const fixture{optional_sentinel_type()};

    auto const analysis{Analyzer::analyze_optional_sentinel(fixture.types, fixture.type, 100)};

    EXPECT_EQ(analysis.sentinel_name, "Invalid");
    EXPECT_EQ(analysis.sentinel_value, codegen::PackedIntegerValue{15});
    EXPECT_FALSE(analysis.source_signedness);
    EXPECT_EQ(analysis.source_minimum, codegen::PackedIntegerValue{0});
    EXPECT_EQ(analysis.source_maximum, codegen::PackedIntegerValue{10});
    EXPECT_EQ(analysis.present_value_count, 11U);
    EXPECT_EQ(analysis.absence_code_count, 1U);
    EXPECT_EQ(analysis.other_sentinel_code_count, 1U);
    EXPECT_EQ(analysis.unused_code_count, 3U);
    EXPECT_EQ(analysis.encoded_storage_bits, 4U);
    EXPECT_EQ(analysis.total_code_count, (ExactCodeCount{.value = 16, .two_to_64 = false}));
    EXPECT_EQ(analysis.element_count, 100U);
    EXPECT_EQ(analysis.total_encoded_bits, 400U);
    EXPECT_TRUE(analysis.diagnostics.empty());

    auto const overflow{Analyzer::analyze_optional_sentinel(
        fixture.types, fixture.type, (std::numeric_limits<std::uint64_t>::max)())};
    EXPECT_FALSE(overflow.total_encoded_bits.has_value());
    ASSERT_EQ(overflow.diagnostics.size(), 1U);
    EXPECT_EQ(overflow.diagnostics.front().severity, DiagnosticSeverity::warning);
}

TEST(OptionalPresenceBitAnalyzer, ReportsCanonicalAbsenceAndScaledPayloadWithoutAbiFacts) {
    auto const fixture{optional_presence_bit_type()};

    auto const analysis{Analyzer::analyze_optional_presence_bit(fixture.types, fixture.type, 100)};

    EXPECT_FALSE(analysis.source_signedness);
    EXPECT_EQ(analysis.source_minimum, codegen::PackedIntegerValue{0});
    EXPECT_EQ(analysis.source_maximum, codegen::PackedIntegerValue{10});
    EXPECT_EQ(analysis.present_value_count, 11U);
    EXPECT_EQ(analysis.canonical_absence_state_count, 1U);
    EXPECT_EQ(analysis.source_sentinel_code_count, 2U);
    EXPECT_EQ(analysis.source_unused_payload_codes, 3U);
    EXPECT_EQ(analysis.presence_bits, 1U);
    EXPECT_EQ(analysis.payload_bits, 4U);
    EXPECT_EQ(analysis.encoded_storage_bits, 5U);
    EXPECT_EQ(analysis.noncanonical_absence_patterns, 15U);
    EXPECT_EQ(analysis.element_count, 100U);
    EXPECT_EQ(analysis.total_encoded_bits, 500U);
    EXPECT_TRUE(analysis.diagnostics.empty());

    auto const overflow{Analyzer::analyze_optional_presence_bit(
        fixture.types, fixture.type, (std::numeric_limits<std::uint64_t>::max)())};
    EXPECT_FALSE(overflow.total_encoded_bits.has_value());
    ASSERT_EQ(overflow.diagnostics.size(), 1U);
    EXPECT_EQ(overflow.diagnostics.front().severity, DiagnosticSeverity::warning);
}

TEST(OptionalPresenceBitAnalyzer, PreservesHonest65BitEncodingForFullWidthSource) {
    auto const fixture{optional_presence_bit_type(64)};

    auto const analysis{Analyzer::analyze_optional_presence_bit(fixture.types, fixture.type)};

    EXPECT_FALSE(analysis.present_value_count.has_value());
    EXPECT_EQ(analysis.source_sentinel_code_count, 0U);
    EXPECT_EQ(analysis.source_unused_payload_codes, 0U);
    EXPECT_EQ(analysis.payload_bits, 64U);
    EXPECT_EQ(analysis.encoded_storage_bits, 65U);
    EXPECT_EQ(analysis.noncanonical_absence_patterns, (std::numeric_limits<std::uint64_t>::max)());
    EXPECT_EQ(analysis.total_encoded_bits, 65U);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(OptionalEncodingComparison, ReportsPolicyAndScaledBitConsequences) {
    auto const fixture{optional_comparison_types()};

    auto const comparison{Analyzer::compare_optional_encodings(
        fixture.types, fixture.sentinel, fixture.presence, 100)};

    EXPECT_TRUE(comparison.supported_encodings);
    EXPECT_TRUE(comparison.compatible_source);
    EXPECT_EQ(comparison.first.kind, OptionalEncodingKind::sentinel);
    EXPECT_EQ(comparison.second.kind, OptionalEncodingKind::presence_bit);
    EXPECT_EQ(comparison.first.absence_sentinel_name, "Invalid");
    EXPECT_FALSE(comparison.second.absence_sentinel_name.has_value());
    EXPECT_EQ(comparison.first.present_value_count, 11U);
    EXPECT_EQ(comparison.second.present_value_count, 11U);
    EXPECT_EQ(comparison.first.canonical_absence_state_count, 1U);
    EXPECT_EQ(comparison.second.canonical_absence_state_count, 1U);
    EXPECT_EQ(comparison.first.source_sentinel_code_count, 2U);
    EXPECT_EQ(comparison.second.source_sentinel_code_count, 2U);
    EXPECT_EQ(comparison.first.source_sentinel_codes_used_for_absence, 1U);
    EXPECT_EQ(comparison.second.source_sentinel_codes_used_for_absence, 0U);
    EXPECT_EQ(comparison.first.remaining_source_sentinel_code_count, 1U);
    EXPECT_EQ(comparison.second.remaining_source_sentinel_code_count, 2U);
    EXPECT_EQ(comparison.first.source_unused_payload_codes, 3U);
    EXPECT_EQ(comparison.second.source_unused_payload_codes, 3U);
    EXPECT_EQ(comparison.first.noncanonical_absence_patterns, 0U);
    EXPECT_EQ(comparison.second.noncanonical_absence_patterns, 15U);
    EXPECT_EQ(comparison.first.encoded_storage_bits, 4U);
    EXPECT_EQ(comparison.second.encoded_storage_bits, 5U);
    ASSERT_TRUE(comparison.encoded_storage_bit_delta.has_value());
    EXPECT_EQ(comparison.encoded_storage_bit_delta->direction, NumericDeltaDirection::increased);
    EXPECT_EQ(comparison.encoded_storage_bit_delta->magnitude, 1U);
    EXPECT_EQ(comparison.first.total_encoded_bits, 400U);
    EXPECT_EQ(comparison.second.total_encoded_bits, 500U);
    ASSERT_TRUE(comparison.total_encoded_bit_delta.has_value());
    EXPECT_EQ(comparison.total_encoded_bit_delta->magnitude, 100U);
    EXPECT_TRUE(comparison.diagnostics.empty());
}

TEST(OptionalEncodingComparison, SupportsSiblingSentinelPolicies) {
    auto const fixture{optional_comparison_types()};

    auto const comparison{Analyzer::compare_optional_encodings(
        fixture.types, fixture.sentinel, fixture.second_sentinel, 100)};

    EXPECT_TRUE(comparison.supported_encodings);
    EXPECT_TRUE(comparison.compatible_source);
    EXPECT_EQ(comparison.first.kind, OptionalEncodingKind::sentinel);
    EXPECT_EQ(comparison.second.kind, OptionalEncodingKind::sentinel);
    EXPECT_EQ(comparison.first.absence_sentinel_name, "Invalid");
    EXPECT_EQ(comparison.second.absence_sentinel_name, "Pending");
    ASSERT_TRUE(comparison.encoded_storage_bit_delta.has_value());
    EXPECT_EQ(comparison.encoded_storage_bit_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.encoded_storage_bit_delta->magnitude, 0U);
}

TEST(OptionalEncodingComparison, Preserves64To65BitBoundaryAndOverflow) {
    auto const fixture{optional_comparison_types(true)};

    auto const comparison{
        Analyzer::compare_optional_encodings(fixture.types, fixture.sentinel, fixture.presence)};
    EXPECT_TRUE(comparison.compatible_source);
    EXPECT_EQ(comparison.first.encoded_storage_bits, 64U);
    EXPECT_EQ(comparison.second.encoded_storage_bits, 65U);
    EXPECT_EQ(comparison.first.source_unused_payload_codes, 0U);
    EXPECT_EQ(comparison.second.source_unused_payload_codes, 0U);
    EXPECT_EQ(comparison.second.noncanonical_absence_patterns,
              (std::numeric_limits<std::uint64_t>::max)());
    EXPECT_EQ(comparison.first.total_encoded_bits, 64U);
    EXPECT_EQ(comparison.second.total_encoded_bits, 65U);

    auto const overflow{
        Analyzer::compare_optional_encodings(fixture.types,
                                             fixture.sentinel,
                                             fixture.presence,
                                             (std::numeric_limits<std::uint64_t>::max)())};
    EXPECT_FALSE(overflow.first.total_encoded_bits.has_value());
    EXPECT_FALSE(overflow.second.total_encoded_bits.has_value());
    EXPECT_FALSE(overflow.total_encoded_bit_delta.has_value());
    EXPECT_EQ(overflow.diagnostics.size(), 2U);
}

TEST(OptionalEncodingComparison, RejectsDifferentSourcesAndNonOptionalTypes) {
    auto const fixture{optional_comparison_types()};

    auto const incompatible{Analyzer::compare_optional_encodings(
        fixture.types, fixture.sentinel, fixture.other_presence)};
    EXPECT_TRUE(incompatible.supported_encodings);
    EXPECT_FALSE(incompatible.compatible_source);
    ASSERT_EQ(incompatible.diagnostics.size(), 1U);
    EXPECT_EQ(incompatible.diagnostics.front().severity, DiagnosticSeverity::error);

    auto const unsupported{
        Analyzer::compare_optional_encodings(fixture.types, fixture.source, fixture.presence)};
    EXPECT_FALSE(unsupported.supported_encodings);
    EXPECT_FALSE(unsupported.compatible_source);
    ASSERT_EQ(unsupported.diagnostics.size(), 1U);
    EXPECT_EQ(unsupported.diagnostics.front().severity, DiagnosticSeverity::error);
}

TEST(LinearQuantizedAnalyzer, ReportsCapacityResolutionAndEndpointFacts) {
    auto const fixture{linear_quantized_type()};

    auto const analysis{Analyzer::analyze_linear_quantized(fixture.types, fixture.type)};

    EXPECT_EQ(analysis.source_span, 1000U);
    EXPECT_EQ(analysis.encoded_storage_bits, 8U);
    EXPECT_EQ(analysis.total_code_count, (ExactCodeCount{.value = 256, .two_to_64 = false}));
    EXPECT_EQ(analysis.reserved_code_count, 1U);
    EXPECT_EQ(analysis.usable_code_count, (ExactCodeCount{.value = 255, .two_to_64 = false}));
    EXPECT_NEAR(static_cast<double>(analysis.resolution), 1000.0 / 254.0, 1e-12);
    EXPECT_NEAR(static_cast<double>(analysis.maximum_rounding_error), 500.0 / 254.0, 1e-12);
    EXPECT_TRUE(analysis.minimum_endpoint_exact);
    EXPECT_TRUE(analysis.maximum_endpoint_exact);
    EXPECT_EQ(analysis.clipping, codegen::QuantizationClipping::clamp);
}

TEST(LinearQuantizedAnalyzer, PreservesHonestExactCapacityAt64Bits) {
    auto const full_fixture{linear_quantized_type(64, 0)};
    auto const full{Analyzer::analyze_linear_quantized(full_fixture.types, full_fixture.type)};
    EXPECT_EQ(full.total_code_count, (ExactCodeCount{.value = 0, .two_to_64 = true}));
    EXPECT_EQ(full.usable_code_count, (ExactCodeCount{.value = 0, .two_to_64 = true}));
    EXPECT_GT(full.resolution, 0.0L);

    auto const reserved_fixture{linear_quantized_type(64, 1)};
    auto const reserved{
        Analyzer::analyze_linear_quantized(reserved_fixture.types, reserved_fixture.type)};
    EXPECT_EQ(reserved.total_code_count, (ExactCodeCount{.value = 0, .two_to_64 = true}));
    EXPECT_EQ(
        reserved.usable_code_count,
        (ExactCodeCount{.value = (std::numeric_limits<std::uint64_t>::max)(), .two_to_64 = false}));
    EXPECT_GT(reserved.resolution, 0.0L);
}

TEST(LinearQuantizedAnalyzer, HandlesCrossZeroAndWhollyNegativeSignedDomains) {
    auto const crossing_fixture{linear_quantized_type(8, 1, true, -100, 100)};
    auto const crossing{
        Analyzer::analyze_linear_quantized(crossing_fixture.types, crossing_fixture.type)};
    EXPECT_EQ(crossing.source_minimum, codegen::PackedIntegerValue{-100});
    EXPECT_EQ(crossing.source_maximum, codegen::PackedIntegerValue{100});
    EXPECT_EQ(crossing.source_span, 200U);
    EXPECT_EQ(crossing.usable_code_count, (ExactCodeCount{.value = 255, .two_to_64 = false}));
    EXPECT_NEAR(static_cast<double>(crossing.resolution), 200.0 / 254.0, 1e-12);
    EXPECT_NEAR(static_cast<double>(crossing.maximum_rounding_error), 100.0 / 254.0, 1e-12);
    EXPECT_TRUE(crossing.minimum_endpoint_exact);
    EXPECT_TRUE(crossing.maximum_endpoint_exact);

    auto const negative_fixture{linear_quantized_type(10, 2, true, -1000, -1)};
    auto const negative{
        Analyzer::analyze_linear_quantized(negative_fixture.types, negative_fixture.type)};
    EXPECT_EQ(negative.source_minimum, codegen::PackedIntegerValue{-1000});
    EXPECT_EQ(negative.source_maximum, codegen::PackedIntegerValue{-1});
    EXPECT_EQ(negative.source_span, 999U);
    EXPECT_EQ(negative.usable_code_count, (ExactCodeCount{.value = 1022, .two_to_64 = false}));
    EXPECT_NEAR(static_cast<double>(negative.resolution), 999.0 / 1021.0, 1e-12);
    EXPECT_NEAR(static_cast<double>(negative.maximum_rounding_error), 999.0 / 2042.0, 1e-12);
}

TEST(LinearQuantizedAnalyzer, PreservesFullSigned64BitSpanWithoutOverflow) {
    auto const fixture{linear_quantized_type(
        64,
        0,
        true,
        codegen::PackedIntegerValue::from_parts(true, std::uint64_t{1} << 63),
        codegen::PackedIntegerValue{(std::numeric_limits<std::int64_t>::max)()})};

    auto const analysis{Analyzer::analyze_linear_quantized(fixture.types, fixture.type)};

    EXPECT_EQ(analysis.source_span, (std::numeric_limits<std::uint64_t>::max)());
    EXPECT_EQ(analysis.total_code_count, (ExactCodeCount{.value = 0, .two_to_64 = true}));
    EXPECT_EQ(analysis.usable_code_count, (ExactCodeCount{.value = 0, .two_to_64 = true}));
    EXPECT_EQ(analysis.resolution, 1.0L);
    EXPECT_EQ(analysis.maximum_rounding_error, 0.5L);
}

TEST(LinearQuantizedComparison, ReportsPrecisionAndScaledPayloadConsequences) {
    auto const fixture{linear_quantized_pair()};

    auto const comparison{
        Analyzer::compare_linear_quantized(fixture.types, fixture.first, fixture.second, 1'000)};

    EXPECT_TRUE(comparison.compatible_source);
    ASSERT_TRUE(comparison.encoded_storage_bit_delta.has_value());
    EXPECT_EQ(comparison.encoded_storage_bit_delta->direction, NumericDeltaDirection::increased);
    EXPECT_EQ(comparison.encoded_storage_bit_delta->magnitude, 2U);
    ASSERT_TRUE(comparison.total_code_count_delta.has_value());
    EXPECT_EQ(comparison.total_code_count_delta->magnitude, 768U);
    ASSERT_TRUE(comparison.usable_code_count_delta.has_value());
    EXPECT_EQ(comparison.usable_code_count_delta->magnitude, 767U);
    ASSERT_TRUE(comparison.reserved_code_count_delta.has_value());
    EXPECT_EQ(comparison.reserved_code_count_delta->magnitude, 1U);
    EXPECT_EQ(comparison.clipping_changed, true);
    ASSERT_TRUE(comparison.resolution_delta.has_value());
    EXPECT_LT(*comparison.resolution_delta, 0.0L);
    ASSERT_TRUE(comparison.maximum_rounding_error_delta.has_value());
    EXPECT_LT(*comparison.maximum_rounding_error_delta, 0.0L);
    EXPECT_EQ(comparison.first_total_encoded_bits, 8'000U);
    EXPECT_EQ(comparison.second_total_encoded_bits, 10'000U);
    ASSERT_TRUE(comparison.total_encoded_bit_delta.has_value());
    EXPECT_EQ(comparison.total_encoded_bit_delta->direction, NumericDeltaDirection::increased);
    EXPECT_EQ(comparison.total_encoded_bit_delta->magnitude, 2'000U);
    EXPECT_TRUE(comparison.diagnostics.empty());
}

TEST(LinearQuantizedComparison, PreservesExactTwoTo64CapacityDelta) {
    auto const fixture{linear_quantized_pair(true, 8, 64)};

    auto const comparison{
        Analyzer::compare_linear_quantized(fixture.types, fixture.first, fixture.second)};

    ASSERT_TRUE(comparison.total_code_count_delta.has_value());
    EXPECT_EQ(comparison.total_code_count_delta->direction, NumericDeltaDirection::increased);
    EXPECT_EQ(comparison.total_code_count_delta->magnitude,
              (std::numeric_limits<std::uint64_t>::max)() - 255U);
}

TEST(LinearQuantizedComparison, ComparesRepresentationsOfOneSignedDomain) {
    auto const fixture{linear_quantized_pair(true, 8, 10, true, -100, 100)};

    auto const comparison{
        Analyzer::compare_linear_quantized(fixture.types, fixture.first, fixture.second, 10'000)};

    EXPECT_TRUE(comparison.compatible_source);
    EXPECT_EQ(comparison.first.source_minimum, codegen::PackedIntegerValue{-100});
    EXPECT_EQ(comparison.second.source_maximum, codegen::PackedIntegerValue{100});
    EXPECT_EQ(comparison.first.source_span, 200U);
    EXPECT_EQ(comparison.second.source_span, 200U);
    ASSERT_TRUE(comparison.resolution_delta.has_value());
    EXPECT_LT(*comparison.resolution_delta, 0.0L);
    ASSERT_TRUE(comparison.maximum_rounding_error_delta.has_value());
    EXPECT_LT(*comparison.maximum_rounding_error_delta, 0.0L);
    EXPECT_EQ(comparison.first_total_encoded_bits, 80'000U);
    EXPECT_EQ(comparison.second_total_encoded_bits, 100'000U);
    EXPECT_TRUE(comparison.diagnostics.empty());
}

TEST(LinearQuantizedComparison, RejectsDifferentSemanticSources) {
    auto const fixture{linear_quantized_pair(false)};

    auto const comparison{
        Analyzer::compare_linear_quantized(fixture.types, fixture.first, fixture.second)};

    EXPECT_FALSE(comparison.compatible_source);
    EXPECT_FALSE(comparison.encoded_storage_bit_delta.has_value());
    EXPECT_FALSE(comparison.total_encoded_bit_delta.has_value());
    ASSERT_EQ(comparison.diagnostics.size(), 1U);
    EXPECT_EQ(comparison.diagnostics[0].severity, DiagnosticSeverity::error);
}

TEST(LinearQuantizedComparison, ReportsScaledPayloadOverflowWithoutInventingStorage) {
    auto const fixture{linear_quantized_pair()};

    auto const comparison{Analyzer::compare_linear_quantized(
        fixture.types, fixture.first, fixture.second, (std::numeric_limits<std::uint64_t>::max)())};

    EXPECT_FALSE(comparison.first_total_encoded_bits.has_value());
    EXPECT_FALSE(comparison.second_total_encoded_bits.has_value());
    EXPECT_FALSE(comparison.total_encoded_bit_delta.has_value());
    ASSERT_EQ(comparison.diagnostics.size(), 2U);
}

TEST(FixedPointAnalyzer, ReportsSignedRangeResolutionAndRounding) {
    auto const fixture{fixed_point_type(true, 16, 4)};

    auto const analysis{Analyzer::analyze_fixed_point(fixture.types, fixture.type, 1'000)};

    EXPECT_TRUE(analysis.signedness);
    EXPECT_EQ(analysis.total_bits, 16U);
    EXPECT_EQ(analysis.fractional_bits, 4U);
    EXPECT_EQ(analysis.whole_bits, 11U);
    EXPECT_EQ(analysis.minimum_raw_value, codegen::PackedIntegerValue{-32'768});
    EXPECT_EQ(analysis.maximum_raw_value, codegen::PackedIntegerValue{32'767});
    EXPECT_DOUBLE_EQ(static_cast<double>(analysis.scale), 16.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(analysis.resolution), 0.0625);
    EXPECT_DOUBLE_EQ(static_cast<double>(analysis.minimum_value), -2'048.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(analysis.maximum_value), 2'047.9375);
    EXPECT_DOUBLE_EQ(static_cast<double>(analysis.maximum_rounding_error), 0.03125);
    EXPECT_EQ(analysis.rounding, codegen::FixedPointRounding::nearest_even);
    EXPECT_EQ(analysis.total_encoded_bits, 16'000U);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(FixedPointAnalyzer, SupportsUnsignedFractionOnlyAndTowardZeroRounding) {
    auto const fixture{fixed_point_type(false, 8, 8, codegen::FixedPointRounding::toward_zero)};

    auto const analysis{Analyzer::analyze_fixed_point(fixture.types, fixture.type)};

    EXPECT_FALSE(analysis.signedness);
    EXPECT_EQ(analysis.whole_bits, 0U);
    EXPECT_EQ(analysis.minimum_raw_value, codegen::PackedIntegerValue{0});
    EXPECT_EQ(analysis.maximum_raw_value, codegen::PackedIntegerValue{255});
    EXPECT_DOUBLE_EQ(static_cast<double>(analysis.minimum_value), 0.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(analysis.maximum_value), 255.0 / 256.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(analysis.maximum_rounding_error), 1.0 / 256.0);
}

TEST(FixedPointAnalyzer, PreservesFullWidthRawRangeAndReportsScaledOverflow) {
    auto const fixture{fixed_point_type(false, 64, 64)};

    auto const analysis{Analyzer::analyze_fixed_point(
        fixture.types, fixture.type, (std::numeric_limits<std::uint64_t>::max)())};

    EXPECT_EQ(analysis.maximum_raw_value,
              codegen::PackedIntegerValue{(std::numeric_limits<std::uint64_t>::max)()});
    EXPECT_GT(analysis.resolution, 0.0L);
    EXPECT_FALSE(analysis.total_encoded_bits.has_value());
    ASSERT_EQ(analysis.diagnostics.size(), 1U);
    EXPECT_EQ(analysis.diagnostics.front().severity, DiagnosticSeverity::error);
}

TEST(MiniFloatAnalyzer, ReportsBinary16StyleCodeRolesAndNumericalRange) {
    auto const fixture{mini_float_type(1, 5, 10, 15)};

    auto const analysis{Analyzer::analyze_mini_float(fixture.types, fixture.type, 1'000)};

    EXPECT_EQ(analysis.sign_bits, 1U);
    EXPECT_EQ(analysis.exponent_bits, 5U);
    EXPECT_EQ(analysis.significand_bits, 10U);
    EXPECT_EQ(analysis.total_bits, 16U);
    EXPECT_EQ(analysis.exponent_bias, 15);
    EXPECT_EQ(analysis.exponent_code_count, 32U);
    EXPECT_EQ(analysis.normal_exponent_code_count, 30U);
    EXPECT_EQ(analysis.minimum_normal_exponent, -14);
    EXPECT_EQ(analysis.maximum_normal_exponent, 15);
    EXPECT_EQ(analysis.total_code_count, (ExactCodeCount{.value = 65'536, .two_to_64 = false}));
    EXPECT_EQ(analysis.zero_code_count, 2U);
    EXPECT_EQ(analysis.infinity_code_count, 2U);
    EXPECT_EQ(analysis.nan_code_count, 2'046U);
    EXPECT_EQ(analysis.nonzero_subnormal_code_count, 2'046U);
    ASSERT_TRUE(analysis.minimum_positive_subnormal.has_value());
    ASSERT_TRUE(analysis.minimum_positive_normal.has_value());
    ASSERT_TRUE(analysis.maximum_finite.has_value());
    ASSERT_TRUE(analysis.minimum_finite.has_value());
    ASSERT_TRUE(analysis.unit_interval_resolution.has_value());
    EXPECT_DOUBLE_EQ(static_cast<double>(*analysis.minimum_positive_subnormal),
                     std::ldexp(1.0, -24));
    EXPECT_DOUBLE_EQ(static_cast<double>(*analysis.minimum_positive_normal), std::ldexp(1.0, -14));
    EXPECT_DOUBLE_EQ(static_cast<double>(*analysis.maximum_finite), 65'504.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(*analysis.minimum_finite), -65'504.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(*analysis.unit_interval_resolution), std::ldexp(1.0, -10));
    EXPECT_DOUBLE_EQ(static_cast<double>(analysis.maximum_relative_rounding_error),
                     std::ldexp(1.0, -11));
    EXPECT_EQ(analysis.total_encoded_bits, 16'000U);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(MiniFloatAnalyzer, SupportsUnsignedFormatsWithoutFractionPayloads) {
    auto const fixture{mini_float_type(0, 3, 0, 3)};

    auto const analysis{Analyzer::analyze_mini_float(fixture.types, fixture.type)};

    EXPECT_EQ(analysis.total_bits, 3U);
    EXPECT_EQ(analysis.total_code_count, (ExactCodeCount{.value = 8, .two_to_64 = false}));
    EXPECT_EQ(analysis.zero_code_count, 1U);
    EXPECT_EQ(analysis.infinity_code_count, 1U);
    EXPECT_EQ(analysis.nan_code_count, 0U);
    EXPECT_EQ(analysis.nonzero_subnormal_code_count, 0U);
    EXPECT_FALSE(analysis.minimum_positive_subnormal.has_value());
    EXPECT_DOUBLE_EQ(static_cast<double>(*analysis.minimum_positive_normal), 0.25);
    EXPECT_DOUBLE_EQ(static_cast<double>(*analysis.maximum_finite), 8.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(*analysis.minimum_finite), 0.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(*analysis.unit_interval_resolution), 1.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(analysis.maximum_relative_rounding_error), 0.5);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(MiniFloatAnalyzer, PreservesTwoTo64CodeSpaceAndReportsScaledOverflow) {
    auto const fixture{mini_float_type(1, 2, 61, 1)};

    auto const analysis{Analyzer::analyze_mini_float(
        fixture.types, fixture.type, (std::numeric_limits<std::uint64_t>::max)())};

    EXPECT_EQ(analysis.total_bits, 64U);
    EXPECT_EQ(analysis.total_code_count, (ExactCodeCount{.value = 0, .two_to_64 = true}));
    EXPECT_EQ(analysis.nan_code_count, (std::uint64_t{1} << 62) - 2);
    EXPECT_EQ(analysis.nonzero_subnormal_code_count, (std::uint64_t{1} << 62) - 2);
    EXPECT_FALSE(analysis.total_encoded_bits.has_value());
    ASSERT_EQ(analysis.diagnostics.size(), 1U);
    EXPECT_EQ(analysis.diagnostics.front().severity, DiagnosticSeverity::error);
}

TEST(MiniFloatAnalyzer, KeepsExactExponentFactsWhenHostNumericsOverflow) {
    auto const fixture{mini_float_type(1, 5, 10, -32'768)};

    auto const analysis{Analyzer::analyze_mini_float(fixture.types, fixture.type)};

    EXPECT_EQ(analysis.minimum_normal_exponent, 32'769);
    EXPECT_EQ(analysis.maximum_normal_exponent, 32'798);
    EXPECT_FALSE(analysis.minimum_positive_subnormal.has_value());
    EXPECT_FALSE(analysis.minimum_positive_normal.has_value());
    EXPECT_FALSE(analysis.maximum_finite.has_value());
    EXPECT_FALSE(analysis.minimum_finite.has_value());
    EXPECT_FALSE(analysis.unit_interval_resolution.has_value());
    EXPECT_GE(analysis.diagnostics.size(), 4U);
    for (auto const& diagnostic : analysis.diagnostics) {
        EXPECT_EQ(diagnostic.severity, DiagnosticSeverity::warning);
    }
}

TEST(IntegerVarintAnalyzer, ReportsUnsignedEncodedByteRangeAndScaledBounds) {
    auto const fixture{
        integer_varint_type(false, 0, 16'384, codegen::IntegerVarintEncoding::unsigned_varint)};

    auto const analysis{Analyzer::analyze_integer_varint(fixture.types, fixture.type, 100)};

    EXPECT_EQ(analysis.minimum_encoded_bytes, 1U);
    EXPECT_EQ(analysis.maximum_encoded_bytes, 3U);
    EXPECT_EQ(analysis.minimum_total_bytes, 100U);
    EXPECT_EQ(analysis.maximum_total_bytes, 300U);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(IntegerVarintAnalyzer, IncludesSourceSentinelsInEncodedByteRange) {
    auto const fixture{integer_varint_type(false,
                                           0,
                                           100,
                                           codegen::IntegerVarintEncoding::unsigned_varint,
                                           (std::numeric_limits<std::uint64_t>::max)())};

    auto const analysis{Analyzer::analyze_integer_varint(fixture.types, fixture.type)};

    EXPECT_EQ(analysis.minimum_encoded_bytes, 1U);
    EXPECT_EQ(analysis.maximum_encoded_bytes, 10U);
}

TEST(IntegerVarintAnalyzer, ReportsSignedLeb128RangeAcrossZero) {
    auto const fixture{
        integer_varint_type(true, -65, 64, codegen::IntegerVarintEncoding::signed_varint)};

    auto const analysis{Analyzer::analyze_integer_varint(fixture.types, fixture.type)};

    EXPECT_EQ(analysis.minimum_encoded_bytes, 1U);
    EXPECT_EQ(analysis.maximum_encoded_bytes, 2U);
}

TEST(IntegerVarintAnalyzer, HandlesFullSignedZigzagRangeAndAggregateOverflow) {
    auto const fixture{integer_varint_type(true,
                                           (std::numeric_limits<std::int64_t>::min)(),
                                           (std::numeric_limits<std::int64_t>::max)(),
                                           codegen::IntegerVarintEncoding::zigzag_varint)};

    auto const analysis{Analyzer::analyze_integer_varint(
        fixture.types, fixture.type, (std::numeric_limits<std::uint64_t>::max)())};

    EXPECT_EQ(analysis.minimum_encoded_bytes, 1U);
    EXPECT_EQ(analysis.maximum_encoded_bytes, 10U);
    EXPECT_EQ(analysis.minimum_total_bytes, (std::numeric_limits<std::uint64_t>::max)());
    EXPECT_FALSE(analysis.maximum_total_bytes.has_value());
    ASSERT_EQ(analysis.diagnostics.size(), 1U);
}

TEST(IntegerVarintComparison, ComparesSameSourceEncodingBounds) {
    auto const fixture{integer_varint_pair()};

    auto const comparison{
        Analyzer::compare_integer_varint(fixture.types, fixture.first, fixture.second, 1'000)};

    EXPECT_TRUE(comparison.compatible_source);
    EXPECT_EQ(comparison.first.encoding, codegen::IntegerVarintEncoding::signed_varint);
    EXPECT_EQ(comparison.second.encoding, codegen::IntegerVarintEncoding::zigzag_varint);
    ASSERT_TRUE(comparison.minimum_encoded_byte_delta.has_value());
    EXPECT_EQ(comparison.minimum_encoded_byte_delta->direction, NumericDeltaDirection::unchanged);
    ASSERT_TRUE(comparison.maximum_encoded_byte_delta.has_value());
    EXPECT_EQ(comparison.maximum_encoded_byte_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.first.minimum_total_bytes, 1'000U);
    EXPECT_EQ(comparison.first.maximum_total_bytes, 2'000U);
    EXPECT_EQ(comparison.second.minimum_total_bytes, 1'000U);
    EXPECT_EQ(comparison.second.maximum_total_bytes, 2'000U);
    EXPECT_TRUE(comparison.diagnostics.empty());
}

TEST(IntegerVarintComparison, RejectsDifferentSemanticSources) {
    auto const fixture{integer_varint_pair(false)};

    auto const comparison{
        Analyzer::compare_integer_varint(fixture.types, fixture.first, fixture.second)};

    EXPECT_FALSE(comparison.compatible_source);
    EXPECT_FALSE(comparison.minimum_encoded_byte_delta.has_value());
    ASSERT_EQ(comparison.diagnostics.size(), 1U);
    EXPECT_EQ(comparison.diagnostics.front().severity, DiagnosticSeverity::error);
}

TEST(IntegerVarintDistribution, ComputesWeightedExpectedSizeFromExplicitValues) {
    auto const fixture{
        integer_varint_type(false, 0, 16'384, codegen::IntegerVarintEncoding::unsigned_varint)};
    std::array const entries{IntegerVarintDistributionEntry{.value = 0, .weight = 1},
                             IntegerVarintDistributionEntry{.value = 128, .weight = 3},
                             IntegerVarintDistributionEntry{.value = 16'384, .weight = 1}};

    auto const analysis{
        Analyzer::analyze_integer_varint_distribution(fixture.types, fixture.type, entries, 100)};

    EXPECT_EQ(analysis.valid_entry_count, 3U);
    EXPECT_EQ(analysis.total_weight, 5U);
    EXPECT_EQ(analysis.total_encoded_bytes, 10U);
    ASSERT_TRUE(analysis.expected_bytes_per_value.has_value());
    EXPECT_EQ(*analysis.expected_bytes_per_value, 2.0L);
    ASSERT_TRUE(analysis.expected_selected_bytes.has_value());
    EXPECT_EQ(*analysis.expected_selected_bytes, 200.0L);
    ASSERT_EQ(analysis.entries.size(), 3U);
    EXPECT_EQ(analysis.entries[0].encoded_bytes, 1U);
    EXPECT_EQ(analysis.entries[0].weighted_encoded_bytes, 1U);
    EXPECT_EQ(analysis.entries[1].encoded_bytes, 2U);
    EXPECT_EQ(analysis.entries[1].weighted_encoded_bytes, 6U);
    EXPECT_EQ(analysis.entries[2].encoded_bytes, 3U);
    EXPECT_EQ(analysis.entries[2].weighted_encoded_bytes, 3U);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(IntegerVarintDistribution, ComputesSignedAndZigZagWeightedSizes) {
    auto const signed_fixture{
        integer_varint_type(true, -65, 64, codegen::IntegerVarintEncoding::signed_varint)};
    std::array const entries{IntegerVarintDistributionEntry{.value = -64, .weight = 2},
                             IntegerVarintDistributionEntry{.value = 64, .weight = 1}};

    auto const signed_analysis{Analyzer::analyze_integer_varint_distribution(
        signed_fixture.types, signed_fixture.type, entries, 3)};

    EXPECT_EQ(signed_analysis.total_weight, 3U);
    EXPECT_EQ(signed_analysis.total_encoded_bytes, 4U);
    ASSERT_TRUE(signed_analysis.expected_bytes_per_value.has_value());
    EXPECT_NEAR(static_cast<double>(*signed_analysis.expected_bytes_per_value), 4.0 / 3.0, 1e-12);
    ASSERT_TRUE(signed_analysis.expected_selected_bytes.has_value());
    EXPECT_NEAR(static_cast<double>(*signed_analysis.expected_selected_bytes), 4.0, 1e-12);

    auto const zigzag_fixture{
        integer_varint_type(true, -65, 64, codegen::IntegerVarintEncoding::zigzag_varint)};
    auto const zigzag_analysis{Analyzer::analyze_integer_varint_distribution(
        zigzag_fixture.types, zigzag_fixture.type, entries, 3)};

    EXPECT_EQ(zigzag_analysis.total_weight, 3U);
    EXPECT_EQ(zigzag_analysis.total_encoded_bytes, 4U);
    ASSERT_TRUE(zigzag_analysis.expected_bytes_per_value.has_value());
    EXPECT_NEAR(static_cast<double>(*zigzag_analysis.expected_bytes_per_value), 4.0 / 3.0, 1e-12);
    EXPECT_TRUE(zigzag_analysis.diagnostics.empty());
}

TEST(IntegerVarintDistribution, EmptyInputKeepsExpectedSizeUnknown) {
    auto const fixture{
        integer_varint_type(false, 0, 100, codegen::IntegerVarintEncoding::unsigned_varint)};

    auto const analysis{Analyzer::analyze_integer_varint_distribution(
        fixture.types, fixture.type, std::span<IntegerVarintDistributionEntry const>{})};

    EXPECT_EQ(analysis.valid_entry_count, 0U);
    EXPECT_EQ(analysis.total_weight, 0U);
    EXPECT_EQ(analysis.total_encoded_bytes, 0U);
    EXPECT_FALSE(analysis.expected_bytes_per_value.has_value());
    ASSERT_EQ(analysis.diagnostics.size(), 1U);
    EXPECT_EQ(analysis.diagnostics.front().severity, DiagnosticSeverity::warning);
}

TEST(IntegerVarintDistribution, AcceptsSentinelsAndDuplicateEntries) {
    auto const fixture{integer_varint_type(false,
                                           0,
                                           100,
                                           codegen::IntegerVarintEncoding::unsigned_varint,
                                           (std::numeric_limits<std::uint64_t>::max)())};
    std::array const entries{
        IntegerVarintDistributionEntry{.value = 0, .weight = 2},
        IntegerVarintDistributionEntry{.value = 0, .weight = 3},
        IntegerVarintDistributionEntry{.value = (std::numeric_limits<std::uint64_t>::max)(),
                                       .weight = 1}};

    auto const analysis{
        Analyzer::analyze_integer_varint_distribution(fixture.types, fixture.type, entries)};

    EXPECT_EQ(analysis.total_weight, 6U);
    EXPECT_EQ(analysis.total_encoded_bytes, 15U);
    ASSERT_TRUE(analysis.expected_bytes_per_value.has_value());
    EXPECT_EQ(*analysis.expected_bytes_per_value, 2.5L);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(IntegerVarintDistribution, RejectsOutOfDomainAndZeroWeightOnlyInput) {
    auto const fixture{
        integer_varint_type(false, 0, 100, codegen::IntegerVarintEncoding::unsigned_varint)};
    std::array const entries{IntegerVarintDistributionEntry{.value = 50, .weight = 0},
                             IntegerVarintDistributionEntry{.value = 101, .weight = 4}};

    auto const analysis{
        Analyzer::analyze_integer_varint_distribution(fixture.types, fixture.type, entries)};

    EXPECT_EQ(analysis.valid_entry_count, 1U);
    EXPECT_EQ(analysis.total_weight, 0U);
    EXPECT_EQ(analysis.total_encoded_bytes, 0U);
    EXPECT_FALSE(analysis.expected_bytes_per_value.has_value());
    ASSERT_EQ(analysis.diagnostics.size(), 2U);
    EXPECT_EQ(analysis.diagnostics[0].severity, DiagnosticSeverity::error);
    EXPECT_EQ(analysis.diagnostics[1].severity, DiagnosticSeverity::warning);
}

TEST(IntegerVarintDistribution, KeepsNumericalExpectationWhenExactTotalsOverflow) {
    auto const fixture{integer_varint_type(true,
                                           (std::numeric_limits<std::int64_t>::min)(),
                                           (std::numeric_limits<std::int64_t>::max)(),
                                           codegen::IntegerVarintEncoding::zigzag_varint)};
    std::array const entries{IntegerVarintDistributionEntry{
                                 .value = 0, .weight = (std::numeric_limits<std::uint64_t>::max)()},
                             IntegerVarintDistributionEntry{.value = 1, .weight = 1}};

    auto const analysis{
        Analyzer::analyze_integer_varint_distribution(fixture.types, fixture.type, entries)};

    EXPECT_FALSE(analysis.total_weight.has_value());
    EXPECT_FALSE(analysis.total_encoded_bytes.has_value());
    ASSERT_TRUE(analysis.expected_bytes_per_value.has_value());
    EXPECT_EQ(*analysis.expected_bytes_per_value, 1.0L);
    ASSERT_EQ(analysis.diagnostics.size(), 2U);
}

TEST(IntegerVarintDistribution, RetainsEntryFactsWhenWeightedBytesOverflow) {
    auto const fixture{
        integer_varint_type(true, -64, 64, codegen::IntegerVarintEncoding::zigzag_varint)};
    std::array const entries{IntegerVarintDistributionEntry{
        .value = 64, .weight = (std::numeric_limits<std::uint64_t>::max)()}};

    auto const analysis{
        Analyzer::analyze_integer_varint_distribution(fixture.types, fixture.type, entries)};

    ASSERT_EQ(analysis.entries.size(), 1U);
    EXPECT_EQ(analysis.entries.front().encoded_bytes, 2U);
    EXPECT_FALSE(analysis.entries.front().weighted_encoded_bytes.has_value());
    EXPECT_EQ(analysis.total_weight, (std::numeric_limits<std::uint64_t>::max)());
    EXPECT_FALSE(analysis.total_encoded_bytes.has_value());
    ASSERT_TRUE(analysis.expected_bytes_per_value.has_value());
    EXPECT_EQ(*analysis.expected_bytes_per_value, 2.0L);
    ASSERT_EQ(analysis.diagnostics.size(), 1U);
}

TEST(IntegerVarintDistributionComparison, AppliesOneDistributionToBothEncodings) {
    auto const fixture{integer_varint_pair()};
    std::array const entries{IntegerVarintDistributionEntry{.value = -64, .weight = 2},
                             IntegerVarintDistributionEntry{.value = 64, .weight = 1}};

    auto const comparison{Analyzer::compare_integer_varint_distribution(
        fixture.types, fixture.first, fixture.second, entries, 3)};

    EXPECT_TRUE(comparison.compatible_source);
    EXPECT_EQ(comparison.first.total_weight, 3U);
    EXPECT_EQ(comparison.second.total_weight, 3U);
    EXPECT_EQ(comparison.first.total_encoded_bytes, 4U);
    EXPECT_EQ(comparison.second.total_encoded_bytes, 4U);
    ASSERT_TRUE(comparison.total_encoded_byte_delta.has_value());
    EXPECT_EQ(comparison.total_encoded_byte_delta->direction, NumericDeltaDirection::unchanged);
    ASSERT_TRUE(comparison.expected_bytes_per_value_delta.has_value());
    EXPECT_EQ(*comparison.expected_bytes_per_value_delta, 0.0L);
    ASSERT_TRUE(comparison.expected_selected_bytes_delta.has_value());
    EXPECT_EQ(*comparison.expected_selected_bytes_delta, 0.0L);
    EXPECT_TRUE(comparison.diagnostics.empty());
}

TEST(IntegerVarintDistributionComparison, RejectsDifferentSemanticSources) {
    auto const fixture{integer_varint_pair(false)};
    std::array const entries{IntegerVarintDistributionEntry{.value = 0, .weight = 1}};

    auto const comparison{Analyzer::compare_integer_varint_distribution(
        fixture.types, fixture.first, fixture.second, entries)};

    EXPECT_FALSE(comparison.compatible_source);
    EXPECT_FALSE(comparison.total_encoded_byte_delta.has_value());
    EXPECT_FALSE(comparison.expected_bytes_per_value_delta.has_value());
    ASSERT_EQ(comparison.diagnostics.size(), 1U);
    EXPECT_EQ(comparison.diagnostics.front().severity, DiagnosticSeverity::error);
}

} // namespace
} // namespace ioj::layout
