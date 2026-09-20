#include <ioj/layout/analyzer.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace ioj::layout {
namespace {

struct TypeFixture {
    lispb::schema::TypeGraph types;
    lispb::schema::TypeId type;
};

auto entity_id_type() -> TypeFixture {
    codegen::EnumModuleSchema enums{};
    enums.settings.name = "entity_types";
    enums.settings.header = "EntityType.h";
    enums.settings.namespace_name = "project";
    codegen::EnumSchema enumeration{};
    enumeration.name = "EntityType";
    enumeration.underlying_type.name = "std::uint8_t";
    enumeration.values = {codegen::EnumeratorSchema{.name = "PlayerShip",
                                                    .initializer = "0",
                                                    .display_name = std::nullopt,
                                                    .hidden = false,
                                                    .serialized_name = std::nullopt},
                          codegen::EnumeratorSchema{.name = "COUNT",
                                                    .initializer = "1",
                                                    .display_name = std::nullopt,
                                                    .hidden = false,
                                                    .serialized_name = std::nullopt}};
    enumeration.count = "COUNT";
    enums.enums.push_back(std::move(enumeration));

    codegen::PackedValueModuleSchema packed{};
    packed.settings.name = "entity_ids";
    packed.settings.header = "EntityId.h";
    codegen::PackedValueSchema packed_value{};
    packed_value.name = "EntityUniqueId";
    packed_value.storage_type.name = "std::uint32_t";
    codegen::PackedFieldSchema index{};
    index.name = "index";
    index.type.name = "std::uint32_t";
    index.bits = 24;
    codegen::PackedFieldSchema entity_type{};
    entity_type.name = "entity_type";
    entity_type.type.name = "@entity_type";
    entity_type.bits = 8;
    entity_type.kind = codegen::PackedFieldKind::enumeration;
    packed_value.fields = {std::move(index), std::move(entity_type)};
    packed_value.invalid_value = 0xffffffff;
    packed.values.push_back(std::move(packed_value));

    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    manifest.types.emplace("entity_type", codegen::CppType{"project::EntityType"});
    manifest.modules = {std::move(enums), std::move(packed)};
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const type{*types.find_declared("entity_ids", "EntityUniqueId")};
    return {std::move(types), type};
}

auto soa_type(std::vector<std::pair<std::string, std::string>> columns = {{"min_xs", "float"},
                                                                          {"min_ys", "float"},
                                                                          {"min_zs", "float"},
                                                                          {"max_xs", "float"},
                                                                          {"max_ys", "float"},
                                                                          {"max_zs", "float"}})
    -> TypeFixture {
    std::vector<codegen::SoaMemberSchema> members;
    members.reserve(columns.size());
    for (auto& [name, type] : columns) {
        codegen::SoaMemberSchema member{};
        member.name = std::move(name);
        member.kind = codegen::SoaMemberKind::array;
        member.type.name = std::move(type);
        members.push_back(std::move(member));
    }
    codegen::SoaModuleSchema soa{};
    soa.settings.name = "soa";
    soa.settings.header = "Soa.h";
    soa.backend = codegen::SoaBackend::standard_library;
    codegen::SoaSchema schema{};
    schema.name = "Columns";
    schema.members = std::move(members);
    soa.structs.push_back(std::move(schema));
    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    manifest.modules = {std::move(soa)};
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const type{*types.find_declared("soa", "Columns")};
    return {std::move(types), type};
}

auto enum_domain_type(std::vector<codegen::EnumeratorSchema> values,
                      std::optional<std::string> count = std::nullopt,
                      std::string underlying = "std::uint8_t") -> TypeFixture {
    codegen::EnumModuleSchema module{};
    module.settings.name = "domains";
    module.settings.header = "Domains.h";
    codegen::EnumSchema enumeration{};
    enumeration.name = "Domain";
    enumeration.underlying_type.name = std::move(underlying);
    enumeration.values = std::move(values);
    enumeration.count = std::move(count);
    module.enums.push_back(std::move(enumeration));
    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    manifest.modules = {std::move(module)};
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const type{*types.find_declared("domains", "Domain")};
    return {std::move(types), type};
}

auto enum_value(std::string name, std::optional<std::string> initializer)
    -> codegen::EnumeratorSchema {
    return {.name = std::move(name),
            .initializer = std::move(initializer),
            .display_name = std::nullopt,
            .hidden = false,
            .serialized_name = std::nullopt};
}

auto record_member(std::string name,
                   std::string type,
                   std::optional<std::uint64_t> count = std::nullopt)
    -> codegen::RecordMemberSchema {
    return {.name = std::move(name),
            .type = codegen::TypeRef{.name = std::move(type), .suffix = {}, .nested = std::nullopt},
            .count = count};
}

auto record_type(std::vector<codegen::RecordSchema> records, std::string selected = "Record")
    -> TypeFixture {
    codegen::RecordModuleSchema module{};
    module.settings.name = "records";
    module.settings.header = "Records.h";
    module.records = std::move(records);
    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    manifest.modules = {std::move(module)};
    auto types{lispb::schema::resolve_type_graph(manifest)};
    auto const type{*types.find_declared("records", selected)};
    return {std::move(types), type};
}

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
    EXPECT_EQ(analysis.backing_bits, 8);
    EXPECT_EQ(analysis.backing_can_represent_domain, true);
    EXPECT_EQ(analysis.unused_backing_codes, 252);
    EXPECT_TRUE(analysis.diagnostics.empty());
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
    auto const signed_fixture{
        enum_domain_type({enum_value("Value", "200")}, std::nullopt, "std::int8_t")};
    auto const signed_analysis{Analyzer::analyze_enum(
        signed_fixture.types, signed_fixture.type, AbiProfile::host_common())};

    EXPECT_EQ(signed_analysis.minimum_required_bits, 8);
    EXPECT_EQ(signed_analysis.backing_can_represent_domain, false);
    EXPECT_FALSE(signed_analysis.diagnostics.empty());

    auto const unsigned_fixture{
        enum_domain_type({enum_value("Value", "-1")}, std::nullopt, "std::uint8_t")};
    auto const unsigned_analysis{Analyzer::analyze_enum(
        unsigned_fixture.types, unsigned_fixture.type, AbiProfile::host_common())};

    EXPECT_EQ(unsigned_analysis.minimum_required_bits, 1);
    EXPECT_EQ(unsigned_analysis.backing_can_represent_domain, false);
    EXPECT_FALSE(unsigned_analysis.diagnostics.empty());
}

TEST(RecordAnalyzer, ReportsOffsetsInternalAndTailPadding) {
    auto const fixture{
        record_type({codegen::RecordSchema{.name = "Record",
                                           .members = {record_member("small", "std::uint8_t"),
                                                       record_member("wide", "std::uint32_t"),
                                                       record_member("medium", "std::uint16_t")},
                                           .export_specifier = std::nullopt}})};

    auto const analysis{
        Analyzer::analyze_record(fixture.types, fixture.type, AbiProfile::host_common())};

    ASSERT_EQ(analysis.members.size(), 3U);
    EXPECT_EQ(analysis.members[0].offset_bytes, 0);
    EXPECT_EQ(analysis.members[1].offset_bytes, 4);
    EXPECT_EQ(analysis.members[1].padding_before_bytes, 3);
    EXPECT_EQ(analysis.members[2].offset_bytes, 8);
    EXPECT_EQ(analysis.payload_bytes, 7);
    EXPECT_EQ(analysis.internal_padding_bytes, 3);
    EXPECT_EQ(analysis.tail_padding_bytes, 2);
    EXPECT_EQ(analysis.size_bytes, 12);
    EXPECT_EQ(analysis.alignment_bytes, 4);
    EXPECT_EQ(analysis.aggregate.element_count, 1);
    EXPECT_EQ(analysis.aggregate.total_storage_bytes, 12);
    EXPECT_EQ(analysis.aggregate.total_payload_bytes, 7);
    EXPECT_EQ(analysis.aggregate.total_padding_bytes, 5);
    EXPECT_EQ(analysis.aggregate.minimum_cache_lines, 1);
    EXPECT_EQ(analysis.aggregate.complete_elements_per_cache_line, 5);
    EXPECT_EQ(analysis.aggregate.minimum_pages, 1);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(RecordAnalyzer, HandlesFixedArraysAndNestedRecords) {
    auto const fixture{record_type(
        {codegen::RecordSchema{.name = "Inner",
                               .members = {record_member("tag", "std::uint8_t"),
                                           record_member("value", "std::uint32_t")},
                               .export_specifier = std::nullopt},
         codegen::RecordSchema{.name = "Record",
                               .members = {record_member("prefix", "std::uint8_t"),
                                           record_member("inner", "Inner"),
                                           record_member("samples", "std::uint16_t", 2)},
                               .export_specifier = std::nullopt}},
        "Record")};

    auto const analysis{
        Analyzer::analyze_record(fixture.types, fixture.type, AbiProfile::host_common())};

    ASSERT_EQ(analysis.members.size(), 3U);
    EXPECT_EQ(analysis.members[1].element_facts->size_bytes, 8);
    EXPECT_EQ(analysis.members[1].offset_bytes, 4);
    EXPECT_EQ(analysis.members[2].element_count, 2);
    EXPECT_EQ(analysis.members[2].extent_bytes, 4);
    EXPECT_EQ(analysis.members[2].offset_bytes, 12);
    EXPECT_EQ(analysis.payload_bytes, 13);
    EXPECT_EQ(analysis.internal_padding_bytes, 3);
    EXPECT_EQ(analysis.tail_padding_bytes, 0);
    EXPECT_EQ(analysis.size_bytes, 16);
    EXPECT_EQ(analysis.alignment_bytes, 4);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(RecordAnalyzer, KeepsUnknownAndOverflowedLayoutsUnknown) {
    auto const unknown{
        record_type({codegen::RecordSchema{.name = "Record",
                                           .members = {record_member("value", "UnknownType")},
                                           .export_specifier = std::nullopt}})};
    auto const unknown_analysis{
        Analyzer::analyze_record(unknown.types, unknown.type, AbiProfile::host_common())};
    EXPECT_FALSE(unknown_analysis.size_bytes.has_value());
    EXPECT_FALSE(unknown_analysis.diagnostics.empty());

    auto const overflow{record_type({codegen::RecordSchema{
        .name = "Record",
        .members = {record_member(
            "values", "std::uint64_t", std::numeric_limits<std::uint64_t>::max())},
        .export_specifier = std::nullopt}})};
    auto const overflow_analysis{
        Analyzer::analyze_record(overflow.types, overflow.type, AbiProfile::host_common())};
    EXPECT_FALSE(overflow_analysis.size_bytes.has_value());
    EXPECT_FALSE(overflow_analysis.members[0].extent_bytes.has_value());
    EXPECT_FALSE(overflow_analysis.diagnostics.empty());
}

TEST(RecordAnalyzer, ScalesAggregateWasteAndDiagnosesUnknownOrOverflowedTargetFacts) {
    auto const fixture{record_type({codegen::RecordSchema{
        .name = "Record",
        .members = {record_member("small", "std::uint8_t"), record_member("wide", "std::uint32_t")},
        .export_specifier = std::nullopt}})};

    auto analysis{
        Analyzer::analyze_record(fixture.types, fixture.type, AbiProfile::host_common(), 100)};
    EXPECT_EQ(analysis.aggregate.element_count, 100);
    EXPECT_EQ(analysis.aggregate.total_storage_bytes, 800);
    EXPECT_EQ(analysis.aggregate.total_payload_bytes, 500);
    EXPECT_EQ(analysis.aggregate.total_internal_padding_bytes, 300);
    EXPECT_EQ(analysis.aggregate.total_tail_padding_bytes, 0);
    EXPECT_EQ(analysis.aggregate.total_padding_bytes, 300);
    EXPECT_EQ(analysis.aggregate.minimum_cache_lines, 13);
    EXPECT_EQ(analysis.aggregate.minimum_pages, 1);
    EXPECT_TRUE(analysis.diagnostics.empty());

    AbiProfile unknown_memory{"unknown-memory"};
    unknown_memory.set(
        "std::uint8_t",
        {.size_bytes = 1, .alignment_bytes = 1, .integer_signed = false, .unsigned_value_bits = 8});
    unknown_memory.set("std::uint32_t",
                       {.size_bytes = 4,
                        .alignment_bytes = 4,
                        .integer_signed = false,
                        .unsigned_value_bits = 32});
    analysis = Analyzer::analyze_record(fixture.types, fixture.type, unknown_memory, 100);
    EXPECT_EQ(analysis.aggregate.total_storage_bytes, 800);
    EXPECT_FALSE(analysis.aggregate.minimum_cache_lines.has_value());
    EXPECT_FALSE(analysis.aggregate.minimum_pages.has_value());
    EXPECT_FALSE(analysis.diagnostics.empty());

    analysis = Analyzer::analyze_record(fixture.types,
                                        fixture.type,
                                        AbiProfile::host_common(),
                                        std::numeric_limits<std::uint64_t>::max());
    EXPECT_FALSE(analysis.aggregate.total_storage_bytes.has_value());
    EXPECT_FALSE(analysis.aggregate.total_padding_bytes.has_value());
    EXPECT_FALSE(analysis.diagnostics.empty());
}

TEST(PackedAnalyzer, ReportsEntityUniqueIdLayout) {
    auto const fixture{entity_id_type()};
    auto const analysis{Analyzer::analyze_packed(
        fixture.types, fixture.type, Variant{}, AbiProfile::host_common())};

    ASSERT_EQ(analysis.storage_facts->size_bytes, 4);
    EXPECT_EQ(analysis.storage_bits, 32);
    EXPECT_EQ(analysis.bits_used, 32);
    EXPECT_EQ(analysis.unused_bits, 0);
    ASSERT_EQ(analysis.fields.size(), 2);
    EXPECT_EQ(analysis.fields[0].least_significant_bit, 0);
    EXPECT_EQ(analysis.fields[0].most_significant_bit, 23);
    EXPECT_EQ(analysis.fields[0].maximum_unsigned_value, 16'777'215);
    EXPECT_EQ(analysis.fields[1].least_significant_bit, 24);
    EXPECT_EQ(analysis.fields[1].most_significant_bit, 31);
    EXPECT_EQ(analysis.fields[1].maximum_unsigned_value, 255);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(PackedAnalyzer, ReportsUnusedAndExcessBits) {
    auto const fixture{entity_id_type()};
    Variant variant;
    variant.overrides.packed_field_widths[{.type = fixture.type, .field_name = "index"}] = 20;
    auto analysis{Analyzer::analyze_packed(
        fixture.types, fixture.type, variant, AbiProfile::host_common(), 100)};
    EXPECT_EQ(analysis.bits_used, 28);
    EXPECT_EQ(analysis.unused_bits, 4);
    EXPECT_EQ(analysis.aggregate.total_unused_bits, 400);

    variant.overrides.packed_field_widths[{.type = fixture.type, .field_name = "index"}] = 25;
    analysis =
        Analyzer::analyze_packed(fixture.types, fixture.type, variant, AbiProfile::host_common());
    EXPECT_FALSE(analysis.unused_bits.has_value());
    EXPECT_EQ(analysis.overflow_bits, 1);
    EXPECT_FALSE(analysis.diagnostics.empty());
}

TEST(PackedAnalyzer, ReportsSchemaAndOverrideProvenance) {
    auto const fixture{entity_id_type()};
    Variant variant;
    variant.overrides.packed_storage_types[fixture.type] = "std::uint64_t";
    variant.overrides.packed_field_widths[{.type = fixture.type, .field_name = "index"}] = 20;

    auto const analysis{
        Analyzer::analyze_packed(fixture.types, fixture.type, variant, AbiProfile::host_common())};

    EXPECT_EQ(analysis.schema_storage_type, "std::uint32_t");
    EXPECT_TRUE(analysis.storage_overridden);
    EXPECT_EQ(analysis.fields[0].logical_type, "std::uint32_t");
    EXPECT_EQ(analysis.fields[0].schema_bit_width, 24);
    EXPECT_EQ(analysis.fields[0].bit_width, 20);
    EXPECT_TRUE(analysis.fields[0].overridden);
    EXPECT_FALSE(analysis.fields[1].overridden);
}

TEST(PackedAnalyzer, DiagnosesZeroWidthFields) {
    auto const fixture{entity_id_type()};
    Variant variant;
    variant.overrides.packed_field_widths[{.type = fixture.type, .field_name = "index"}] = 0;
    auto const analysis{
        Analyzer::analyze_packed(fixture.types, fixture.type, variant, AbiProfile::host_common())};

    ASSERT_EQ(analysis.fields.size(), 2);
    EXPECT_FALSE(analysis.fields[0].most_significant_bit.has_value());
    EXPECT_EQ(analysis.bits_used, 8);
    EXPECT_FALSE(analysis.diagnostics.empty());
}

TEST(PackedAnalyzer, AppliesExplicitVariantOverrides) {
    auto const fixture{entity_id_type()};
    Variant variant;
    variant.overrides.packed_storage_types[fixture.type] = "std::uint64_t";
    variant.overrides.packed_field_widths[{.type = fixture.type, .field_name = "index"}] = 40;

    auto const analysis{
        Analyzer::analyze_packed(fixture.types, fixture.type, variant, AbiProfile::host_common())};

    EXPECT_EQ(analysis.storage_type, "std::uint64_t");
    EXPECT_EQ(analysis.storage_bits, 64);
    EXPECT_EQ(analysis.bits_used, 48);
    EXPECT_EQ(analysis.unused_bits, 16);
}

TEST(PackedAnalyzer, ReportsOverflowSafeAggregateMemoryAtSelectedScale) {
    auto const fixture{entity_id_type()};
    auto const analysis{Analyzer::analyze_packed(
        fixture.types, fixture.type, Variant{}, AbiProfile::host_common(), 1'000)};

    EXPECT_EQ(analysis.aggregate.element_count, 1'000);
    EXPECT_EQ(analysis.aggregate.total_storage_bytes, 4'000);
    EXPECT_EQ(analysis.aggregate.total_payload_bits, 32'000);
    EXPECT_EQ(analysis.aggregate.total_unused_bits, 0);
    EXPECT_EQ(analysis.aggregate.cache_line_bytes, 64);
    EXPECT_EQ(analysis.aggregate.minimum_cache_lines, 63);
    EXPECT_EQ(analysis.aggregate.complete_elements_per_cache_line, 16);
    EXPECT_EQ(analysis.aggregate.page_bytes, 4'096);
    EXPECT_EQ(analysis.aggregate.minimum_pages, 1);
    EXPECT_EQ(analysis.aggregate.complete_elements_per_page, 1'024);
}

TEST(PackedAnalyzer, KeepsUnknownTargetMemoryFactsUnknown) {
    auto const fixture{entity_id_type()};
    AbiProfile abi{"unknown memory"};
    abi.set("std::uint32_t",
            {.size_bytes = 4,
             .alignment_bytes = 4,
             .integer_signed = false,
             .unsigned_value_bits = 32});

    auto const analysis{Analyzer::analyze_packed(fixture.types, fixture.type, Variant{}, abi, 10)};

    EXPECT_EQ(analysis.aggregate.total_storage_bytes, 40);
    EXPECT_FALSE(analysis.aggregate.cache_line_bytes.has_value());
    EXPECT_FALSE(analysis.aggregate.minimum_cache_lines.has_value());
    EXPECT_FALSE(analysis.aggregate.page_bytes.has_value());
    EXPECT_FALSE(analysis.aggregate.minimum_pages.has_value());
    EXPECT_FALSE(analysis.diagnostics.empty());
}

TEST(PackedAnalyzer, DiagnosesAggregateOverflow) {
    auto const fixture{entity_id_type()};
    auto const analysis{Analyzer::analyze_packed(fixture.types,
                                                 fixture.type,
                                                 Variant{},
                                                 AbiProfile::host_common(),
                                                 std::numeric_limits<std::uint64_t>::max())};

    EXPECT_FALSE(analysis.aggregate.total_storage_bytes.has_value());
    EXPECT_FALSE(analysis.aggregate.total_payload_bits.has_value());
    EXPECT_FALSE(analysis.aggregate.minimum_cache_lines.has_value());
    EXPECT_FALSE(analysis.aggregate.minimum_pages.has_value());
    EXPECT_FALSE(analysis.diagnostics.empty());
}

TEST(SoaAnalyzer, ReportsSixFloatPayloadAcrossCapacities) {
    auto const fixture{soa_type()};
    auto const abi{AbiProfile::host_common()};
    for (auto const capacity : {std::uint64_t{0},
                                std::uint64_t{1},
                                std::uint64_t{4'096},
                                std::uint64_t{16'384},
                                std::uint64_t{65'536}}) {
        auto const analysis{
            Analyzer::analyze_soa(fixture.types, fixture.type, Variant{}, abi, capacity)};
        EXPECT_EQ(analysis.bytes_per_logical_element, 24);
        EXPECT_EQ(analysis.total_payload_bytes, 24 * capacity);
        ASSERT_EQ(analysis.columns.size(), 6);
        EXPECT_EQ(analysis.columns[0].total_bytes, 4 * capacity);
        EXPECT_EQ(analysis.columns[0].elements_per_cache_line, 16);
        EXPECT_EQ(analysis.columns[0].minimum_cache_lines,
                  (4 * capacity) / 64 + ((4 * capacity) % 64 == 0 ? 0 : 1));
        EXPECT_EQ(analysis.columns[0].complete_elements_per_page, 1'024);
        EXPECT_EQ(analysis.columns[0].minimum_pages,
                  (4 * capacity) / 4'096 + ((4 * capacity) % 4'096 == 0 ? 0 : 1));
        EXPECT_EQ(analysis.minimum_pages,
                  6 * ((4 * capacity) / 4'096 + ((4 * capacity) % 4'096 == 0 ? 0 : 1)));
    }
}

TEST(SoaAnalyzer, AppliesCapacityAndColumnTypeOverrides) {
    auto const fixture{soa_type()};
    Variant variant;
    variant.overrides.capacities[fixture.type] = 4'096;
    variant.overrides.soa_column_types[{.type = fixture.type, .field_name = "min_xs"}] = "double";

    auto const analysis{Analyzer::analyze_soa(
        fixture.types, fixture.type, variant, AbiProfile::host_common(), 65'536)};

    EXPECT_EQ(analysis.capacity, 4'096);
    EXPECT_TRUE(analysis.capacity_overridden);
    EXPECT_EQ(analysis.bytes_per_logical_element, 28);
    EXPECT_EQ(analysis.total_payload_bytes, 28 * 4'096);
    EXPECT_EQ(analysis.columns[0].type_facts->size_bytes, 8);
    EXPECT_EQ(analysis.columns[0].elements_per_cache_line, 8);
}

TEST(SoaAnalyzer, ReportsCacheLineTilingForNonDivisibleAndOversizedElements) {
    auto const fixture{soa_type({{"three", "three_bytes"}, {"wide_values", "wide"}})};
    AbiProfile abi{"test"};
    abi.set("three_bytes",
            {.size_bytes = 3,
             .alignment_bytes = 1,
             .integer_signed = std::nullopt,
             .unsigned_value_bits = {}});
    abi.set("wide",
            {.size_bytes = 80,
             .alignment_bytes = 16,
             .integer_signed = std::nullopt,
             .unsigned_value_bits = {}});
    abi.set_memory_facts(
        {.cache_line_bytes = 64, .page_bytes = 4'096, .provenance = "test profile"});

    auto const analysis{Analyzer::analyze_soa(fixture.types, fixture.type, Variant{}, abi, 1)};

    ASSERT_TRUE(analysis.columns[0].cache_line_tiling.has_value());
    auto const& three_bytes{*analysis.columns[0].cache_line_tiling};
    EXPECT_FALSE(three_bytes.exact_elements_per_cache_line.has_value());
    EXPECT_EQ(three_bytes.complete_elements_from_line_start, 21);
    EXPECT_EQ(three_bytes.boundary_fragment_bytes, 1);
    EXPECT_EQ(three_bytes.minimum_cache_lines_per_element, 1);

    ASSERT_TRUE(analysis.columns[1].cache_line_tiling.has_value());
    auto const& wide{*analysis.columns[1].cache_line_tiling};
    EXPECT_FALSE(wide.exact_elements_per_cache_line.has_value());
    EXPECT_EQ(wide.complete_elements_from_line_start, 0);
    EXPECT_EQ(wide.boundary_fragment_bytes, 0);
    EXPECT_EQ(wide.minimum_cache_lines_per_element, 2);
}

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

TEST(AbiProfile, ExposesExplicitX86MemoryFactsWithProvenance) {
    auto const abi{AbiProfile::host_common()};

    EXPECT_EQ(abi.memory_facts().cache_line_bytes, 64);
    EXPECT_EQ(abi.memory_facts().page_bytes, 4'096);
    EXPECT_FALSE(abi.memory_facts().provenance.empty());
    EXPECT_EQ(abi.find("std::uint8_t")->integer_signed, false);
    EXPECT_EQ(abi.find("std::int8_t")->integer_signed, true);
    EXPECT_FALSE(abi.find("float")->integer_signed.has_value());
}

TEST(SoaAnalyzer, LeavesCacheStatisticsUnknownWithoutTargetFact) {
    auto const fixture{soa_type({{"values", "four"}})};
    AbiProfile abi{"unknown memory"};
    abi.set("four",
            {.size_bytes = 4,
             .alignment_bytes = 4,
             .integer_signed = std::nullopt,
             .unsigned_value_bits = {}});

    auto const analysis{Analyzer::analyze_soa(fixture.types, fixture.type, Variant{}, abi, 100)};

    EXPECT_EQ(analysis.columns[0].total_bytes, 400);
    EXPECT_FALSE(analysis.columns[0].minimum_cache_lines.has_value());
    EXPECT_FALSE(analysis.columns[0].cache_line_tiling.has_value());
    EXPECT_FALSE(analysis.columns[0].minimum_pages.has_value());
    EXPECT_FALSE(analysis.minimum_pages.has_value());
    EXPECT_FALSE(analysis.diagnostics.empty());
}

TEST(SoaAnalyzer, UnknownTypesRemainUnknown) {
    auto const fixture{soa_type({{"unknown", "UnknownUserType"}})};
    auto const analysis{Analyzer::analyze_soa(
        fixture.types, fixture.type, Variant{}, AbiProfile::host_common(), 10)};

    EXPECT_FALSE(analysis.columns[0].type_facts.has_value());
    EXPECT_FALSE(analysis.total_payload_bytes.has_value());
    EXPECT_FALSE(analysis.diagnostics.empty());
}

TEST(SoaAnalyzer, ReportsIntegerOverflow) {
    auto const fixture{soa_type({{"values", "huge"}})};
    AbiProfile abi{"test"};
    abi.set("huge",
            {.size_bytes = std::numeric_limits<std::uint64_t>::max(),
             .alignment_bytes = 1,
             .integer_signed = std::nullopt,
             .unsigned_value_bits = std::nullopt});

    auto const analysis{Analyzer::analyze_soa(fixture.types, fixture.type, Variant{}, abi, 2)};

    EXPECT_FALSE(analysis.columns[0].total_bytes.has_value());
    EXPECT_FALSE(analysis.total_payload_bytes.has_value());
    EXPECT_FALSE(analysis.diagnostics.empty());
}

} // namespace
} // namespace ioj::layout
