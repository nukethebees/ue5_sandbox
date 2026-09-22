#include <ioj/layout/workspace.hpp>

#include <codegen/schema/schema_version.h>

#include <gtest/gtest.h>

#include <filesystem>

namespace ioj::layout {
namespace {

auto enumeration(std::string name) -> codegen::EnumSchema {
    return {.name = std::move(name),
            .underlying_type =
                codegen::TypeRef{.name = "std::uint8_t", .suffix = {}, .nested = std::nullopt},
            .bit_width = std::nullopt,
            .signedness = std::nullopt,
            .reflection = codegen::EnumReflection::none,
            .values = {{.name = "Value",
                        .initializer = std::nullopt,
                        .display_name = std::nullopt,
                        .hidden = false,
                        .serialized_name = std::nullopt}},
            .enum_array = false,
            .count = std::nullopt,
            .conversions = {},
            .export_specifier = std::nullopt,
            .native_api = false,
            .unreal_projection = std::nullopt};
}

auto graph(bool const insert_before_target,
           std::string packed_field = "value",
           std::string soa_column = "values",
           bool const change_packed_kind = false,
           bool const change_soa_kind = false) -> lispb::schema::TypeGraph {
    std::vector<codegen::EnumSchema> enums;
    enums.push_back(enumeration("First"));
    if (insert_before_target) {
        enums.push_back(enumeration("Inserted"));
    }
    enums.push_back(enumeration("Target"));
    codegen::ModuleSettings settings{.name = "test",
                                     .header = std::filesystem::path{"Test.h"},
                                     .source = std::nullopt,
                                     .header_include = std::nullopt,
                                     .namespace_name = "test",
                                     .include_order = {},
                                     .prelude_lines = {}};
    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    manifest.modules.emplace_back(
        codegen::NormalModuleSchema{.settings = std::move(settings),
                                    .declarations = {std::make_move_iterator(enums.begin()),
                                                     std::make_move_iterator(enums.end())},
                                    .enum_helper_namespace = std::nullopt});

    codegen::ModuleSettings packed_settings{};
    packed_settings.name = "packed";
    packed_settings.header = "Packed.h";
    if (change_packed_kind) {
        manifest.modules.emplace_back(
            codegen::NormalModuleSchema{.settings = std::move(packed_settings),
                                        .declarations = {enumeration("Packet")},
                                        .enum_helper_namespace = std::nullopt});
    } else {
        codegen::PackedValueSchema packed{};
        packed.name = "Packet";
        packed.storage_type.name = "std::uint8_t";
        if (packed_field.empty()) {
            packed_field = "other";
        }
        codegen::PackedFieldSchema field{};
        field.name = std::move(packed_field);
        field.type.name = "std::uint8_t";
        field.bits = 8;
        packed.segments.emplace_back(std::move(field));
        manifest.modules.emplace_back(codegen::NormalModuleSchema{
            .settings = std::move(packed_settings), .declarations = {std::move(packed)}});
    }

    codegen::ModuleSettings soa_settings{};
    soa_settings.name = "soa";
    soa_settings.header = "Soa.h";
    if (change_soa_kind) {
        manifest.modules.emplace_back(
            codegen::NormalModuleSchema{.settings = std::move(soa_settings),
                                        .declarations = {enumeration("Columns")},
                                        .enum_helper_namespace = std::nullopt});
    } else {
        codegen::SoaSchema soa{};
        soa.name = "Columns";
        if (soa_column.empty()) {
            soa_column = "other";
        }
        soa.members.push_back(codegen::SoaMemberSchema{
            .name = std::move(soa_column),
            .kind = codegen::SoaMemberKind::array,
            .type = {.name = "std::uint32_t", .suffix = {}, .nested = std::nullopt},
            .fixed_schema = std::nullopt,
            .nested_schema = std::nullopt,
            .mask_field = false,
            .mask_dimensions = {},
            .relationship = std::nullopt});
        manifest.modules.emplace_back(
            codegen::NormalModuleSchema{.settings = std::move(soa_settings),
                                        .declarations = {std::move(soa)},
                                        .soa_backend = codegen::SoaBackend::standard_library,
                                        .soa_array_allocators = {}});
    }
    return lispb::schema::resolve_type_graph(manifest);
}

TEST(LayoutWorkspace, CreatesDuplicatesResetsAndDeletesVariants) {
    LayoutWorkspace workspace{graph(false)};
    auto const schema{*workspace.types().find_declared("soa", "Columns")};

    EXPECT_FALSE(workspace.set_capacity(schema, 100));
    auto const first{workspace.create_variant("First")};
    EXPECT_TRUE(workspace.set_capacity(schema, 100));
    auto const duplicate{workspace.duplicate_variant(first, "Copy")};
    ASSERT_TRUE(duplicate.has_value());
    EXPECT_EQ(workspace.active_variant().overrides.capacities.at(schema), 100);
    EXPECT_TRUE(workspace.rename_variant(*duplicate, "Second"));
    EXPECT_TRUE(workspace.reset_variant(*duplicate));
    EXPECT_TRUE(workspace.active_variant().overrides.capacities.empty());
    EXPECT_TRUE(workspace.delete_variant(*duplicate));
    EXPECT_EQ(workspace.active_variant_id(), LayoutWorkspace::baseline_variant_id);
}

TEST(LayoutWorkspace, OnlyChangesRevisionWhenStateChanges) {
    LayoutWorkspace workspace;
    auto const variant{workspace.create_variant("Variant")};
    auto const initial_revision{workspace.revision()};
    auto const graph_revision{workspace.graph_revision()};

    EXPECT_FALSE(workspace.select_variant(variant));
    EXPECT_FALSE(workspace.rename_variant(variant, "Variant"));
    EXPECT_EQ(workspace.revision(), initial_revision);
    EXPECT_EQ(workspace.graph_revision(), graph_revision);
    workspace.replace_types(graph(false));
    EXPECT_EQ(workspace.graph_revision(), graph_revision + 1);
}

TEST(LayoutWorkspace, TracksPositiveAnalysisElementCount) {
    LayoutWorkspace workspace;
    auto const initial_revision{workspace.revision()};

    EXPECT_EQ(workspace.element_count(), 1);
    EXPECT_FALSE(workspace.set_element_count(0));
    EXPECT_FALSE(workspace.set_element_count(1));
    EXPECT_EQ(workspace.revision(), initial_revision);
    EXPECT_TRUE(workspace.set_element_count(100'000));
    EXPECT_EQ(workspace.element_count(), 100'000);
    EXPECT_EQ(workspace.revision(), initial_revision + 1);
}

TEST(LayoutWorkspace, ResetsIndividualPlanningOverrides) {
    LayoutWorkspace workspace{graph(false)};
    auto const packed{*workspace.types().find_declared("packed", "Packet")};
    auto const soa{*workspace.types().find_declared("soa", "Columns")};
    workspace.create_variant("Variant");

    EXPECT_TRUE(workspace.set_packed_storage_type(packed, "std::uint64_t"));
    EXPECT_TRUE(workspace.set_packed_field_width(packed, "value", 12));
    EXPECT_TRUE(workspace.set_soa_column_type(soa, "values", "std::uint16_t"));
    EXPECT_TRUE(workspace.set_capacity(soa, 4'096));
    EXPECT_TRUE(workspace.set_packed_storage_type(packed, std::nullopt));
    EXPECT_TRUE(workspace.set_packed_field_width(packed, "value", std::nullopt));
    EXPECT_TRUE(workspace.set_soa_column_type(soa, "values", std::nullopt));
    EXPECT_TRUE(workspace.set_capacity(soa, std::nullopt));
    EXPECT_EQ(workspace.active_variant().overrides, VariantOverrides{});
}

TEST(LayoutWorkspace, RemapsOverridesByStableIdentityWhenTypeIdsChange) {
    LayoutWorkspace workspace{graph(false)};
    auto const old_packed{workspace.types().find_declared("packed", "Packet")};
    auto const old_soa{workspace.types().find_declared("soa", "Columns")};
    ASSERT_TRUE(old_packed.has_value());
    ASSERT_TRUE(old_soa.has_value());
    workspace.create_variant("Variant");
    ASSERT_TRUE(workspace.set_packed_storage_type(*old_packed, "std::uint16_t"));
    ASSERT_TRUE(workspace.set_packed_field_width(*old_packed, "value", 7));
    ASSERT_TRUE(workspace.set_soa_column_type(*old_soa, "values", "std::uint16_t"));
    ASSERT_TRUE(workspace.set_capacity(*old_soa, 42));

    workspace.replace_types(graph(true));

    auto const new_packed{workspace.types().find_declared("packed", "Packet")};
    auto const new_soa{workspace.types().find_declared("soa", "Columns")};
    ASSERT_TRUE(new_packed.has_value());
    ASSERT_TRUE(new_soa.has_value());
    EXPECT_NE(*new_packed, *old_packed);
    EXPECT_NE(*new_soa, *old_soa);
    EXPECT_EQ(workspace.active_variant().overrides.packed_storage_types.at(*new_packed),
              "std::uint16_t");
    EXPECT_EQ(workspace.active_variant().overrides.packed_field_widths.at(
                  {.type = *new_packed, .field_name = "value"}),
              7);
    EXPECT_EQ(workspace.active_variant().overrides.soa_column_types.at(
                  {.type = *new_soa, .field_name = "values"}),
              "std::uint16_t");
    EXPECT_EQ(workspace.active_variant().overrides.capacities.at(*new_soa), 42);
    EXPECT_FALSE(workspace.active_variant().overrides.capacities.contains(*old_soa));
}

TEST(LayoutWorkspace, DropsDeletedOrRenamedMemberOverridesWithoutResurrection) {
    LayoutWorkspace workspace{graph(false)};
    auto const packed{*workspace.types().find_declared("packed", "Packet")};
    auto const soa{*workspace.types().find_declared("soa", "Columns")};
    workspace.create_variant("Variant");
    ASSERT_TRUE(workspace.set_packed_field_width(packed, "value", 7));
    ASSERT_TRUE(workspace.set_soa_column_type(soa, "values", "std::uint16_t"));

    workspace.replace_types(graph(false, "", ""));
    EXPECT_TRUE(workspace.active_variant().overrides.packed_field_widths.empty());
    EXPECT_TRUE(workspace.active_variant().overrides.soa_column_types.empty());
    workspace.replace_types(graph(false));
    EXPECT_TRUE(workspace.active_variant().overrides.packed_field_widths.empty());
    EXPECT_TRUE(workspace.active_variant().overrides.soa_column_types.empty());

    ASSERT_TRUE(workspace.set_packed_field_width(packed, "value", 7));
    ASSERT_TRUE(workspace.set_soa_column_type(soa, "values", "std::uint16_t"));
    workspace.replace_types(graph(false, "renamed", "renamed"));
    EXPECT_TRUE(workspace.active_variant().overrides.packed_field_widths.empty());
    EXPECT_TRUE(workspace.active_variant().overrides.soa_column_types.empty());
}

TEST(LayoutWorkspace, DropsOverridesWhenOwnerChangesKind) {
    LayoutWorkspace workspace{graph(false)};
    auto const packed{*workspace.types().find_declared("packed", "Packet")};
    auto const soa{*workspace.types().find_declared("soa", "Columns")};
    workspace.create_variant("Variant");
    ASSERT_TRUE(workspace.set_packed_storage_type(packed, "std::uint16_t"));
    ASSERT_TRUE(workspace.set_packed_field_width(packed, "value", 7));
    ASSERT_TRUE(workspace.set_soa_column_type(soa, "values", "std::uint16_t"));
    ASSERT_TRUE(workspace.set_capacity(soa, 42));

    workspace.replace_types(graph(false, "value", "values", true, true));
    EXPECT_EQ(workspace.active_variant().overrides, VariantOverrides{});
}

TEST(LayoutWorkspace, RejectsImpossibleOverrideOwnersAndMembers) {
    LayoutWorkspace workspace{graph(false)};
    auto const packed{*workspace.types().find_declared("packed", "Packet")};
    auto const soa{*workspace.types().find_declared("soa", "Columns")};
    auto const enumeration_id{*workspace.types().find_declared("test", "Target")};
    workspace.create_variant("Variant");
    auto const revision{workspace.revision()};

    EXPECT_FALSE(workspace.set_packed_storage_type(enumeration_id, "std::uint16_t"));
    EXPECT_FALSE(workspace.set_packed_field_width(enumeration_id, "value", 7));
    EXPECT_FALSE(workspace.set_packed_field_width(packed, "missing", 7));
    EXPECT_FALSE(workspace.set_soa_column_type(packed, "value", "std::uint16_t"));
    EXPECT_FALSE(workspace.set_soa_column_type(soa, "missing", "std::uint16_t"));
    EXPECT_FALSE(workspace.set_capacity(packed, 42));
    EXPECT_FALSE(workspace.set_capacity(enumeration_id, 42));
    EXPECT_EQ(workspace.revision(), revision);
    EXPECT_EQ(workspace.active_variant().overrides, VariantOverrides{});
}

} // namespace
} // namespace ioj::layout
