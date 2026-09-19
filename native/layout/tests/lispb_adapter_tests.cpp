#include <ioj/layout/analyzer.hpp>
#include <ioj/layout/lispb_adapter.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <variant>

namespace ioj::layout {
namespace {

auto schema_abi(CatalogLoadResult const& loaded) -> AbiProfile {
    auto abi{AbiProfile::host_common()};
    for (auto const& representation : loaded.type_representations) {
        abi.set_representation(representation.spelling, representation.represented_by);
    }
    return abi;
}

TEST(LispbAdapter, LoadsEntityUniqueIdAndWorldAabbs) {
    auto const project_path{std::filesystem::path{SANDBOX_SOURCE_DIR} / "lispb/project.lispb"};
    auto const loaded{load_lispb_catalog(project_path, "sandbox-code")};
    ASSERT_TRUE(loaded.loaded);

    auto const packed_id{SchemaId{.kind = SchemaKind::packed_value,
                                  .module_name = "native_entity_unique_id",
                                  .schema_name = "EntityUniqueId"}};
    auto const* packed_definition{loaded.catalog.find(packed_id)};
    ASSERT_NE(packed_definition, nullptr);
    auto const& packed{std::get<PackedLayout>(*packed_definition)};
    EXPECT_EQ(packed.storage_type, "std::uint32_t");
    ASSERT_EQ(packed.fields.size(), 2);
    EXPECT_EQ(packed.fields[0].name, "index");
    EXPECT_EQ(packed.fields[0].bit_width, 24);
    EXPECT_EQ(packed.fields[1].name, "entity_type");
    EXPECT_EQ(packed.fields[1].bit_width, 8);
    EXPECT_EQ(packed.invalid_raw_value, 0xffffffff);

    auto const soa_id{SchemaId{.kind = SchemaKind::standard_library_soa,
                               .module_name = "native_world_aabbs",
                               .schema_name = "WorldAABBsColumns"}};
    auto const* soa_definition{loaded.catalog.find(soa_id)};
    ASSERT_NE(soa_definition, nullptr);
    auto const& soa{std::get<SoaLayout>(*soa_definition)};
    ASSERT_EQ(soa.columns.size(), 6);
    for (auto const& column : soa.columns) {
        EXPECT_EQ(column.logical_type, "float");
    }
    EXPECT_EQ(soa.related_storage_name, "WorldAABBs");

    auto const vectors_id{SchemaId{.kind = SchemaKind::standard_library_soa,
                                   .module_name = "native_vectors_3f",
                                   .schema_name = "Vectors3f"}};
    auto const* vectors_definition{loaded.catalog.find(vectors_id)};
    ASSERT_NE(vectors_definition, nullptr);
    auto const& vectors{std::get<SoaLayout>(*vectors_definition)};
    ASSERT_EQ(vectors.columns.size(), 3);
    EXPECT_EQ(vectors.columns.front().logical_type, "float");
}

TEST(LispbAdapter, DerivesFactsForGeneratedEnumsAndPackedValues) {
    auto const project_path{std::filesystem::path{SANDBOX_SOURCE_DIR} / "lispb/project.lispb"};
    auto const loaded{load_lispb_catalog(project_path, "sandbox-code")};
    ASSERT_TRUE(loaded.loaded);

    auto const id{SchemaId{.kind = SchemaKind::standard_library_soa,
                           .module_name = "native_entity_history",
                           .schema_name = "EntityHistoryColumns"}};
    auto const* definition{loaded.catalog.find(id)};
    ASSERT_NE(definition, nullptr);
    auto const analysis{
        Analyzer::analyze(std::get<SoaLayout>(*definition), Variant{}, schema_abi(loaded), 1)};

    EXPECT_EQ(analysis.bytes_per_logical_element, 15);
    EXPECT_EQ(analysis.total_payload_bytes, 15);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(LispbAdapter, ReportsMissingProjectsWithoutThrowing) {
    auto const loaded{load_lispb_catalog("missing/project.lispb", "sandbox-code")};
    EXPECT_FALSE(loaded.loaded);
    EXPECT_FALSE(loaded.diagnostics.empty());
}

} // namespace
} // namespace ioj::layout
