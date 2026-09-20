#include <ioj/layout/workspace.hpp>

#include <codegen/schema/schema_version.h>

#include <gtest/gtest.h>

#include <filesystem>

namespace ioj::layout {
namespace {

auto enumeration(std::string name) -> codegen::EnumSchema {
    return {.name = std::move(name),
            .underlying_type = {.name = "std::uint8_t", .suffix = {}, .nested = std::nullopt},
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

auto graph(bool const insert_before_target) -> lispb::schema::TypeGraph {
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
    return lispb::schema::resolve_type_graph(
        codegen::Manifest{.schema_version = codegen::manifest_schema_version,
                          .types = {},
                          .modules = {codegen::EnumModuleSchema{.settings = std::move(settings),
                                                                .helper_namespace = std::nullopt,
                                                                .enums = std::move(enums)}}});
}

TEST(LayoutWorkspace, CreatesDuplicatesResetsAndDeletesVariants) {
    LayoutWorkspace workspace;
    auto const schema{lispb::schema::TypeId{1}};

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

    EXPECT_FALSE(workspace.select_variant(variant));
    EXPECT_FALSE(workspace.rename_variant(variant, "Variant"));
    EXPECT_EQ(workspace.revision(), initial_revision);
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
    LayoutWorkspace workspace;
    auto const packed{lispb::schema::TypeId{1}};
    auto const soa{lispb::schema::TypeId{2}};
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
    auto const old_target{workspace.types().find_declared("test", "Target")};
    ASSERT_TRUE(old_target.has_value());
    workspace.create_variant("Variant");
    ASSERT_TRUE(workspace.set_capacity(*old_target, 42));
    ASSERT_TRUE(workspace.set_packed_field_width(*old_target, "field", 7));

    workspace.replace_types(graph(true));

    auto const new_target{workspace.types().find_declared("test", "Target")};
    ASSERT_TRUE(new_target.has_value());
    EXPECT_NE(*new_target, *old_target);
    EXPECT_EQ(workspace.active_variant().overrides.capacities.at(*new_target), 42);
    EXPECT_EQ(workspace.active_variant().overrides.packed_field_widths.at(
                  {.type = *new_target, .field_name = "field"}),
              7);
    EXPECT_FALSE(workspace.active_variant().overrides.capacities.contains(*old_target));
}

} // namespace
} // namespace ioj::layout
