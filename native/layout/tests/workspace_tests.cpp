#include "analyzer_test_fixtures.hpp"
#include <ioj/layout/workspace.hpp>

#include <codegen/schema/schema_version.h>
#include <lispb/compiler_storage.h>
#include <lispb/schema/type_graph.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace ioj::layout {
namespace {

auto graph(lispb::CompilerStorage& storage,
           bool const insert_before_target,
           std::string packed_field = "value",
           std::string soa_column = "values",
           bool const change_packed_kind = false,
           bool const change_soa_kind = false) -> lispb::schema::TypeGraph {
    std::string source{R"((module test :header "Test.h" :namespace test
        (enum First std::uint8_t (value Value)))"};
    if (insert_before_target) {
        source += " (enum Inserted std::uint8_t (value Value))";
    }
    source += R"( (enum Target std::uint8_t (value Value)))
        (module packed :header "Packed.h" )";
    if (change_packed_kind) {
        source += "(enum Packet std::uint8_t (value Value))";
    } else {
        source += "(packed-value Packet :storage std::uint8_t (field " +
                  (packed_field.empty() ? "other" : packed_field) + " std::uint8_t :bits 8))";
    }
    source += R"() (module soa :header "Soa.h" :backend standard-library )";
    if (change_soa_kind) {
        source += "(enum Columns std::uint8_t (value Value))";
    } else {
        source += "(struct Columns (member " + (soa_column.empty() ? "other" : soa_column) +
                  " array std::uint32_t))";
    }
    source += ')';
    return fixture_graph(storage, source);
}

TEST(LayoutWorkspace, CreatesDuplicatesResetsAndDeletesVariants) {
    lispb::CompilerStorage storage;
    auto types{graph(storage, false)};
    LayoutWorkspace workspace{types};
    auto const schema{*fixture_find(workspace.types(), "soa", "Columns")};

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
    lispb::CompilerStorage storage;
    LayoutWorkspace workspace;
    auto const variant{workspace.create_variant("Variant")};
    auto const initial_revision{workspace.revision()};
    auto const graph_revision{workspace.graph_revision()};

    EXPECT_FALSE(workspace.select_variant(variant));
    EXPECT_FALSE(workspace.rename_variant(variant, "Variant"));
    EXPECT_EQ(workspace.revision(), initial_revision);
    EXPECT_EQ(workspace.graph_revision(), graph_revision);
    auto replacement_types_6{graph(storage, false)};
    workspace.replace_types(replacement_types_6);
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
    lispb::CompilerStorage storage;
    auto types{graph(storage, false)};
    LayoutWorkspace workspace{types};
    auto const packed{*fixture_find(workspace.types(), "packed", "Packet")};
    auto const soa{*fixture_find(workspace.types(), "soa", "Columns")};
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
    lispb::CompilerStorage storage;
    auto types{graph(storage, false)};
    LayoutWorkspace workspace{types};
    auto const old_packed{fixture_find(workspace.types(), "packed", "Packet")};
    auto const old_soa{fixture_find(workspace.types(), "soa", "Columns")};
    ASSERT_TRUE(old_packed.has_value());
    ASSERT_TRUE(old_soa.has_value());
    workspace.create_variant("Variant");
    ASSERT_TRUE(workspace.set_packed_storage_type(*old_packed, "std::uint16_t"));
    ASSERT_TRUE(workspace.set_packed_field_width(*old_packed, "value", 7));
    ASSERT_TRUE(workspace.set_soa_column_type(*old_soa, "values", "std::uint16_t"));
    ASSERT_TRUE(workspace.set_capacity(*old_soa, 42));

    auto replacement_types_5{graph(storage, true)};
    workspace.replace_types(replacement_types_5);

    auto const new_packed{fixture_find(workspace.types(), "packed", "Packet")};
    auto const new_soa{fixture_find(workspace.types(), "soa", "Columns")};
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
    lispb::CompilerStorage storage;
    auto types{graph(storage, false)};
    LayoutWorkspace workspace{types};
    auto const packed{*fixture_find(workspace.types(), "packed", "Packet")};
    auto const soa{*fixture_find(workspace.types(), "soa", "Columns")};
    workspace.create_variant("Variant");
    ASSERT_TRUE(workspace.set_packed_field_width(packed, "value", 7));
    ASSERT_TRUE(workspace.set_soa_column_type(soa, "values", "std::uint16_t"));

    auto replacement_types_4{graph(storage, false, "", "")};
    workspace.replace_types(replacement_types_4);
    EXPECT_TRUE(workspace.active_variant().overrides.packed_field_widths.empty());
    EXPECT_TRUE(workspace.active_variant().overrides.soa_column_types.empty());
    auto replacement_types_3{graph(storage, false)};
    workspace.replace_types(replacement_types_3);
    EXPECT_TRUE(workspace.active_variant().overrides.packed_field_widths.empty());
    EXPECT_TRUE(workspace.active_variant().overrides.soa_column_types.empty());

    ASSERT_TRUE(workspace.set_packed_field_width(packed, "value", 7));
    ASSERT_TRUE(workspace.set_soa_column_type(soa, "values", "std::uint16_t"));
    auto replacement_types_2{graph(storage, false, "renamed", "renamed")};
    workspace.replace_types(replacement_types_2);
    EXPECT_TRUE(workspace.active_variant().overrides.packed_field_widths.empty());
    EXPECT_TRUE(workspace.active_variant().overrides.soa_column_types.empty());
}

TEST(LayoutWorkspace, DropsOverridesWhenOwnerChangesKind) {
    lispb::CompilerStorage storage;
    auto types{graph(storage, false)};
    LayoutWorkspace workspace{types};
    auto const packed{*fixture_find(workspace.types(), "packed", "Packet")};
    auto const soa{*fixture_find(workspace.types(), "soa", "Columns")};
    workspace.create_variant("Variant");
    ASSERT_TRUE(workspace.set_packed_storage_type(packed, "std::uint16_t"));
    ASSERT_TRUE(workspace.set_packed_field_width(packed, "value", 7));
    ASSERT_TRUE(workspace.set_soa_column_type(soa, "values", "std::uint16_t"));
    ASSERT_TRUE(workspace.set_capacity(soa, 42));

    auto replacement_types_1{graph(storage, false, "value", "values", true, true)};
    workspace.replace_types(replacement_types_1);
    EXPECT_EQ(workspace.active_variant().overrides, VariantOverrides{});
}

TEST(LayoutWorkspace, RejectsImpossibleOverrideOwnersAndMembers) {
    lispb::CompilerStorage storage;
    auto types{graph(storage, false)};
    LayoutWorkspace workspace{types};
    auto const packed{*fixture_find(workspace.types(), "packed", "Packet")};
    auto const soa{*fixture_find(workspace.types(), "soa", "Columns")};
    auto const enumeration_id{*fixture_find(workspace.types(), "test", "Target")};
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
