#include <ioj/layout/document.hpp>
#include <ioj/layout/planner_session.hpp>
#include <ioj/layout/workspace.hpp>

#include <lispb/strings.h>

#include <gtest/gtest.h>

#include <string_view>

namespace ioj::layout {
namespace workspace_tests {
constexpr std::string_view source{R"((module game :header "Game.h" :backend standard-library
    (packed-value State :storage std::uint16_t (field health std::uint16_t :bits 8))
    (struct Ships (member values array std::uint32_t))))"};

TEST(DocumentVariants, CreatesDuplicatesRenamesResetsAndDeletesThroughHistory) {
    auto doc{Document::from_source(source)};
    ASSERT_TRUE(doc.valid());
    auto create{doc.transaction()};
    create.create_variant("Compact");
    ASSERT_TRUE(doc.commit(create));
    auto const compact{doc.variants().ids().back()};
    auto settings{doc.transaction()};
    settings.set_variant_capacity(compact, "game", "Ships", 100);
    settings.set_variant_field_bits(compact, "game", "State", "health", 4);
    settings.duplicate_variant(compact, "Copy");
    ASSERT_TRUE(doc.commit(settings));
    auto const copy{doc.variants().ids().back()};
    EXPECT_EQ(doc.variants().settings(copy), doc.variants().settings(compact));
    auto reset{doc.transaction()};
    reset.rename_variant(copy, "Empty");
    reset.reset_variant(copy);
    ASSERT_TRUE(doc.commit(reset));
    EXPECT_EQ(doc.variants().name(copy), "Empty");
    EXPECT_EQ(doc.variants().settings(copy), VariantSettings{});
    EXPECT_NE(doc.variants().settings(compact), VariantSettings{});
    auto remove{doc.transaction()};
    remove.delete_variant(copy);
    ASSERT_TRUE(doc.commit(remove));
    EXPECT_FALSE(doc.variants().contains(copy));
    ASSERT_TRUE(doc.undo());
    EXPECT_TRUE(doc.variants().contains(copy));
    ASSERT_TRUE(doc.undo());
    EXPECT_EQ(doc.variants().name(copy), "Copy");
    EXPECT_EQ(doc.variants().settings(copy), doc.variants().settings(compact));
}

TEST(DocumentVariants, MixedSchemaAndVariantEditsUndoAsOneTransaction) {
    auto doc{Document::from_source(source)};
    ASSERT_TRUE(doc.valid());
    auto create{doc.transaction()};
    create.create_variant("Alternative");
    ASSERT_TRUE(doc.commit(create));
    auto const variant{doc.variants().ids().back()};
    auto edit{doc.transaction()};
    edit.set_field_bits("game", "State", "health", 12);
    edit.set_variant_packed_storage(variant, "game", "State", "std::uint32_t");
    edit.set_variant_field_bits(variant, "game", "State", "health", 20);
    edit.set_variant_member_type(variant, "game", "Ships", "values", "std::uint16_t");
    edit.set_variant_capacity(variant, "game", "Ships", 42);
    ASSERT_TRUE(doc.commit(edit));
    LayoutWorkspace workspace;
    workspace.bind(doc);
    ASSERT_TRUE(workspace.select_variant(variant));
    auto const state{doc.find_type("game", "State").value()};
    auto const ships{doc.find_type("game", "Ships").value()};
    EXPECT_EQ(workspace.active_variant().overrides.packed_storage_types.at(state), "std::uint32_t");
    EXPECT_EQ(workspace.active_variant().overrides.packed_field_widths.at(
                  {state, doc.types().strings().find("health")}),
              20);
    EXPECT_EQ(workspace.active_variant().overrides.soa_column_types.at(
                  {ships, doc.types().strings().find("values")}),
              "std::uint16_t");
    EXPECT_EQ(workspace.active_variant().overrides.capacities.at(ships), 42);
    ASSERT_TRUE(doc.undo());
    EXPECT_EQ(doc.query().module("game").packed_value("State").field("health").bits(), 8);
    EXPECT_EQ(doc.variants().settings(variant), VariantSettings{});
    ASSERT_TRUE(doc.redo());
    EXPECT_EQ(doc.query().module("game").packed_value("State").field("health").bits(), 12);
}

TEST(DocumentVariants, ResolvesOverridesAfterRenameAndAllowsSpeculativeOverflow) {
    auto doc{Document::from_source(source)};
    ASSERT_TRUE(doc.valid());
    auto create{doc.transaction()};
    create.create_variant("Overflow");
    ASSERT_TRUE(doc.commit(create));
    auto const variant{doc.variants().ids().back()};
    auto edit{doc.transaction()};
    edit.set_variant_field_bits(variant, "game", "State", "health", 24);
    edit.rename_declaration("game", "State", "Renamed");
    ASSERT_TRUE(doc.commit(edit));
    PlannerAnalysisSession session{doc.types()};
    session.edit_inputs().selection.select_type(doc.types(), doc.find_type("game", "Renamed"));
    ASSERT_TRUE(session.refresh(&doc));
    ASSERT_TRUE(session.edit_inputs().workspace.select_variant(variant));
    ASSERT_TRUE(session.refresh(&doc));
    ASSERT_TRUE(session.results().active_packed);
    EXPECT_EQ(session.results().active_packed->overflow_bits, 8);
    EXPECT_EQ(session.results().active_packed->bits_used, 24);
    EXPECT_EQ(doc.query().module("game").packed_value("Renamed").field("health").bits(), 8);
}

TEST(LayoutWorkspace, TracksAnalysisElementCountSeparatelyFromDocumentHistory) {
    auto doc{Document::from_source(source)};
    ASSERT_TRUE(doc.valid());
    LayoutWorkspace workspace;
    workspace.bind(doc);
    EXPECT_EQ(workspace.element_count(), 1);
    EXPECT_FALSE(workspace.set_element_count(1));
    EXPECT_TRUE(workspace.set_element_count(100'000));
    EXPECT_EQ(workspace.element_count(), 100'000);
    EXPECT_FALSE(doc.can_undo());
    EXPECT_FALSE(doc.dirty());
}
} // namespace
} // namespace ioj::layout
