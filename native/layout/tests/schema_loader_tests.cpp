#include <ioj/layout/analyzer.hpp>
#include <ioj/layout/schema_loader.hpp>

#include <gtest/gtest.h>

#include <filesystem>

namespace ioj::layout {
namespace {

auto diagnostic_text(SchemaLoadResult const& loaded) -> std::string {
    std::string result;
    for (auto const& diagnostic : loaded.diagnostics) {
        result += diagnostic.message + '\n';
    }
    return result;
}

TEST(SchemaLoader, LoadsSemanticEnumsPackedValuesAndSoas) {
    auto const project_path{std::filesystem::path{SANDBOX_SOURCE_DIR} / "lispb/project.lispb"};
    auto const loaded{load_lispb_schema(project_path, "sandbox-code")};
    ASSERT_TRUE(loaded.loaded) << diagnostic_text(loaded);

    auto const entity_type{loaded.types.find_declared("native_entity_type", "EntityType")};
    auto const entity_id{loaded.types.find_declared("native_entity_unique_id", "EntityUniqueId")};
    auto const world_aabbs{loaded.types.find_declared("native_world_aabbs", "WorldAABBsColumns")};
    auto const vectors{loaded.types.find_declared("native_vectors_3f", "Vectors3f")};
    ASSERT_TRUE(entity_type.has_value());
    ASSERT_TRUE(entity_id.has_value());
    ASSERT_TRUE(world_aabbs.has_value());
    ASSERT_TRUE(vectors.has_value());

    auto const& enumeration{
        std::get<lispb::schema::EnumType>(loaded.types.type(*entity_type).definition)};
    EXPECT_EQ(enumeration.enumerators.front().display_name, "Player Ship");

    auto const& packed{
        std::get<lispb::schema::PackedType>(loaded.types.type(*entity_id).definition)};
    ASSERT_EQ(packed.fields.size(), 2U);
    EXPECT_EQ(packed.fields[1].semantic_type.type, *entity_type);
    EXPECT_EQ(packed.invalid_raw_value, 0xffffffffU);

    auto const& soa{std::get<lispb::schema::SoaType>(loaded.types.type(*world_aabbs).definition)};
    ASSERT_EQ(soa.columns.size(), 6U);
    EXPECT_EQ(soa.related_storage_name, "WorldAABBs");

    auto const& vector_soa{
        std::get<lispb::schema::SoaType>(loaded.types.type(*vectors).definition)};
    ASSERT_EQ(vector_soa.columns.size(), 3U);
}

TEST(SchemaLoader, DerivesFactsThroughSemanticRepresentations) {
    auto const project_path{std::filesystem::path{SANDBOX_SOURCE_DIR} / "lispb/project.lispb"};
    auto const loaded{load_lispb_schema(project_path, "sandbox-code")};
    ASSERT_TRUE(loaded.loaded) << diagnostic_text(loaded);

    auto const history{loaded.types.find_declared("native_entity_history", "EntityHistoryColumns")};
    ASSERT_TRUE(history.has_value());
    auto const analysis{
        Analyzer::analyze_soa(loaded.types, *history, Variant{}, AbiProfile::host_common(), 1)};

    EXPECT_EQ(analysis.bytes_per_logical_element, 15);
    EXPECT_EQ(analysis.total_payload_bytes, 15);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(SchemaLoader, ReportsMissingProjectsWithoutThrowing) {
    auto const loaded{load_lispb_schema("missing/project.lispb", "sandbox-code")};
    EXPECT_FALSE(loaded.loaded);
    EXPECT_FALSE(loaded.diagnostics.empty());
}

} // namespace
} // namespace ioj::layout
