#include <ioj/layout/analyzer.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

namespace ioj::layout {
namespace {

auto entity_id_layout() -> PackedLayout {
    return {
        .id = {.kind = SchemaKind::packed_value,
               .module_name = "native_entity_unique_id",
               .schema_name = "EntityUniqueId"},
        .storage_type = "std::uint32_t",
        .fields = {{.name = "index",
                    .logical_type = "std::uint32_t",
                    .bit_width = 24,
                    .kind = PackedFieldKind::unsigned_integer},
                   {.name = "entity_type",
                    .logical_type = "EntityType",
                    .bit_width = 8,
                    .kind = PackedFieldKind::enumeration}},
        .invalid_raw_value = 0xffffffff,
    };
}

auto six_float_soa() -> SoaLayout {
    return {
        .id = {.kind = SchemaKind::standard_library_soa,
               .module_name = "native_world_aabbs",
               .schema_name = "WorldAABBsColumns"},
        .columns = {{.name = "min_xs", .logical_type = "float"},
                    {.name = "min_ys", .logical_type = "float"},
                    {.name = "min_zs", .logical_type = "float"},
                    {.name = "max_xs", .logical_type = "float"},
                    {.name = "max_ys", .logical_type = "float"},
                    {.name = "max_zs", .logical_type = "float"}},
    };
}

TEST(PackedAnalyzer, ReportsEntityUniqueIdLayout) {
    auto const analysis{
        Analyzer::analyze(entity_id_layout(), Variant{}, AbiProfile::host_common())};

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
    auto layout{entity_id_layout()};
    layout.fields[0].bit_width = 20;
    auto analysis{Analyzer::analyze(layout, Variant{}, AbiProfile::host_common())};
    EXPECT_EQ(analysis.bits_used, 28);
    EXPECT_EQ(analysis.unused_bits, 4);

    layout.fields[0].bit_width = 25;
    analysis = Analyzer::analyze(layout, Variant{}, AbiProfile::host_common());
    EXPECT_FALSE(analysis.unused_bits.has_value());
    EXPECT_FALSE(analysis.diagnostics.empty());
}

TEST(PackedAnalyzer, AppliesExplicitVariantOverrides) {
    auto const layout{entity_id_layout()};
    Variant variant;
    variant.overrides.packed_storage_types[layout.id] = "std::uint64_t";
    variant.overrides.packed_field_widths[{.schema = layout.id, .field_name = "index"}] = 40;

    auto const analysis{Analyzer::analyze(layout, variant, AbiProfile::host_common())};

    EXPECT_EQ(analysis.storage_type, "std::uint64_t");
    EXPECT_EQ(analysis.storage_bits, 64);
    EXPECT_EQ(analysis.bits_used, 48);
    EXPECT_EQ(analysis.unused_bits, 16);
}

TEST(SoaAnalyzer, ReportsSixFloatPayloadAcrossCapacities) {
    auto const layout{six_float_soa()};
    auto const abi{AbiProfile::host_common()};
    for (auto const capacity : {std::uint64_t{0},
                                std::uint64_t{1},
                                std::uint64_t{4'096},
                                std::uint64_t{16'384},
                                std::uint64_t{65'536}}) {
        auto const analysis{Analyzer::analyze(layout, Variant{}, abi, capacity)};
        EXPECT_EQ(analysis.bytes_per_logical_element, 24);
        EXPECT_EQ(analysis.total_payload_bytes, 24 * capacity);
        ASSERT_EQ(analysis.columns.size(), 6);
        EXPECT_EQ(analysis.columns[0].total_bytes, 4 * capacity);
        EXPECT_EQ(analysis.columns[0].elements_per_cache_line, 16);
        EXPECT_EQ(analysis.columns[0].minimum_cache_lines,
                  (4 * capacity) / 64 + ((4 * capacity) % 64 == 0 ? 0 : 1));
    }
}

TEST(SoaAnalyzer, AppliesCapacityAndColumnTypeOverrides) {
    auto const layout{six_float_soa()};
    Variant variant;
    variant.overrides.capacities[layout.id] = 4'096;
    variant.overrides.soa_column_types[{.schema = layout.id, .field_name = "min_xs"}] = "double";

    auto const analysis{Analyzer::analyze(layout, variant, AbiProfile::host_common(), 65'536)};

    EXPECT_EQ(analysis.capacity, 4'096);
    EXPECT_EQ(analysis.bytes_per_logical_element, 28);
    EXPECT_EQ(analysis.total_payload_bytes, 28 * 4'096);
    EXPECT_EQ(analysis.columns[0].type_facts->size_bytes, 8);
    EXPECT_EQ(analysis.columns[0].elements_per_cache_line, 8);
}

TEST(SoaAnalyzer, UnknownTypesRemainUnknown) {
    auto layout{six_float_soa()};
    layout.columns[0].logical_type = "UnknownUserType";

    auto const analysis{Analyzer::analyze(layout, Variant{}, AbiProfile::host_common(), 10)};

    EXPECT_FALSE(analysis.columns[0].type_facts.has_value());
    EXPECT_FALSE(analysis.total_payload_bytes.has_value());
    EXPECT_FALSE(analysis.diagnostics.empty());
}

TEST(SoaAnalyzer, ReportsIntegerOverflow) {
    auto const layout{SoaLayout{
        .id = {.kind = SchemaKind::standard_library_soa,
               .module_name = "test",
               .schema_name = "Overflow"},
        .columns = {{.name = "values", .logical_type = "huge"}},
    }};
    AbiProfile abi{"test"};
    abi.set("huge",
            {.size_bytes = std::numeric_limits<std::uint64_t>::max(),
             .alignment_bytes = 1,
             .unsigned_value_bits = std::nullopt});

    auto const analysis{Analyzer::analyze(layout, Variant{}, abi, 2)};

    EXPECT_FALSE(analysis.columns[0].total_bytes.has_value());
    EXPECT_FALSE(analysis.total_payload_bytes.has_value());
    EXPECT_FALSE(analysis.diagnostics.empty());
}

} // namespace
} // namespace ioj::layout
