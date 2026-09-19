#include <ioj/layout/workspace.hpp>

#include <gtest/gtest.h>

namespace ioj::layout {
namespace {

TEST(LayoutWorkspace, CreatesDuplicatesResetsAndDeletesVariants) {
    LayoutWorkspace workspace;
    auto const schema{SchemaId{.kind = SchemaKind::packed_value,
                               .module_name = "module",
                               .schema_name = "Packed"}};

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

} // namespace
} // namespace ioj::layout
