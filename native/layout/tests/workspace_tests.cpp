#include "document_test_support.hpp"
#include <ioj/layout/document.hpp>
#include <ioj/layout/editor_session.hpp>

#include <lispb/strings.h>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ioj::layout::workspace_tests {
constexpr std::string_view source{R"((module game :header "Game.h"
    (record State :layout packed (member health @LayoutEditValue :bits 8)
    (projections (cpp :storage std::uint16_t)))
    (struct Ships (member values array std::uint32_t)) (integer-scalar LayoutEditValue :bit-width 1)))"};

TEST(DocumentVariants, CreatesDuplicatesRenamesResetsAndDeletesThroughHistory) {
    EditorSession editor;
    auto doc{editor.from_source(source)};
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
    EditorSession editor;
    auto doc{editor.from_source(source)};
    ASSERT_TRUE(doc.valid());
    auto create{doc.transaction()};
    create.create_variant("Alternative");
    ASSERT_TRUE(doc.commit(create));
    auto const variant{doc.variants().ids().back()};
    auto edit{doc.transaction()};
    edit.set_member_bits("game", "State", "health", 12);
    edit.set_variant_packed_storage(variant, "game", "State", "std::uint32_t");
    edit.set_variant_field_bits(variant, "game", "State", "health", 20);
    edit.set_variant_member_type(variant, "game", "Ships", "values", "std::uint16_t");
    edit.set_variant_capacity(variant, "game", "Ships", 42);
    ASSERT_TRUE(doc.commit(edit));
    auto const& settings{doc.variants().settings(variant)};
    EXPECT_EQ(settings.packed_storage, (std::vector<std::string>{"std::uint32_t"}));
    EXPECT_EQ(settings.field_widths, (std::vector<std::uint32_t>{20}));
    EXPECT_EQ(settings.column_spelling, (std::vector<std::string>{"std::uint16_t"}));
    EXPECT_EQ(settings.capacities, (std::vector<std::uint64_t>{42}));
    ASSERT_TRUE(doc.undo());
    EXPECT_EQ(doc.query().module("game").record("State").member("health").bits(), 8);
    EXPECT_EQ(doc.variants().settings(variant), VariantSettings{});
    ASSERT_TRUE(doc.redo());
    EXPECT_EQ(doc.query().module("game").record("State").member("health").bits(), 12);
}

TEST(DocumentVariants, ResolvesOverridesAfterRenameAndAllowsSpeculativeOverflow) {
    EditorSession editor;
    auto doc{editor.from_source(source)};
    ASSERT_TRUE(doc.valid());
    auto create{doc.transaction()};
    create.create_variant("Overflow");
    ASSERT_TRUE(doc.commit(create));
    auto const variant{doc.variants().ids().back()};
    auto edit{doc.transaction()};
    edit.set_variant_field_bits(variant, "game", "State", "health", 24);
    edit.rename_declaration("game", "State", "Renamed");
    ASSERT_TRUE(doc.commit(edit));
    auto const renamed{doc.find_type("game", "Renamed").value()};
    auto const declaration{doc.find_declaration(doc.types().identity(renamed)).value()};
    EXPECT_EQ(doc.variants().settings(variant).field_types.front(), declaration);
    EXPECT_EQ(doc.variants().settings(variant).field_widths.front(), 24);
    EXPECT_EQ(doc.query().module("game").record("Renamed").member("health").bits(), 8);
}

TEST(DocumentSettings, AllAnalysisInputsShareSchemaHistory) {
    EditorSession editor;
    auto doc{editor.from_source(source)};
    ASSERT_TRUE(doc.valid());
    auto const original{doc.analysis_settings()};
    auto const type{doc.find_type("game", "Ships").value()};
    auto const owner{doc.find_declaration(doc.types().identity(type)).value()};
    auto edit{doc.transaction()};
    edit.set_element_count(100'000);
    edit.set_default_soa_capacity(500);
    edit.set_access_operation(AccessOperation::read_write);
    edit.set_access_multiplicity(8);
    edit.set_abi_profile(AbiProfile{"unknown target"});
    edit.set_comparison_abi_profile(AbiProfile::host_common());
    edit.set_access_set("game", "Ships", {{"values", AccessOperation::write}});
    ASSERT_TRUE(doc.commit(edit));
    AnalysisSettings const expected{.abi = AbiProfile{"unknown target"},
                                    .comparison_abi = AbiProfile::host_common(),
                                    .element_count = 100'000,
                                    .default_soa_capacity = 500,
                                    .access_multiplicity = 8,
                                    .access_operation = AccessOperation::read_write,
                                    .access_sets = {{owner, {{"values", AccessOperation::write}}}}};
    EXPECT_EQ(doc.analysis_settings(), expected);
    EXPECT_FALSE(doc.dirty());
    ASSERT_TRUE(doc.undo());
    EXPECT_EQ(doc.analysis_settings(), original);
    ASSERT_TRUE(doc.redo());
    EXPECT_EQ(doc.analysis_settings(), expected);
}

TEST(DocumentSettings, DistributionOwnersSurviveRenameAndRestoreAfterDeletion) {
    EditorSession editor;
    auto doc{editor.from_source(R"((module game :header "Game.h"
        (union Payload (member small std::uint8_t) (member large std::uint32_t)
    (projections (cpp)))))")};
    ASSERT_TRUE(doc.valid());
    auto const type{doc.find_type("game", "Payload").value()};
    auto const owner{doc.find_declaration(doc.types().identity(type)).value()};
    std::array const weights{UnionDistributionEntry{"small", 3},
                             UnionDistributionEntry{"large", 1}};
    auto edit{doc.transaction()};
    edit.set_union_distribution("game", "Payload", weights);
    edit.rename_declaration("game", "Payload", "Message");
    ASSERT_TRUE(doc.commit(edit));
    EXPECT_EQ(doc.analysis_settings().union_distributions.at(owner).front().weight, 3);
    auto remove{doc.transaction()};
    remove.delete_declaration("game", "Message");
    ASSERT_TRUE(doc.commit(remove));
    EXPECT_TRUE(doc.analysis_settings().union_distributions.empty());
    ASSERT_TRUE(doc.undo());
    EXPECT_EQ(doc.analysis_settings().union_distributions.at(owner).back().weight, 1);
    ASSERT_TRUE(doc.undo());
    EXPECT_TRUE(doc.analysis_settings().union_distributions.empty());
    EXPECT_TRUE(doc.find_type("game", "Payload"));
}
TEST(WorkspaceVariants, NestedArrayPathsFollowRootMemberRenames) {
    EditorSession editor;
    auto doc{editor.from_source(R"((module game
        (integer-scalar Value :bit-width 4)
        (record Sample (member value @Value))
        (record Storage :layout packed
            (member samples @Sample :count 2)
            (member samples_extra @Value))))")};
    ASSERT_TRUE(doc.valid()) << diagnostic_messages(editor);
    auto create{doc.transaction()};
    create.create_variant("Expanded");
    ASSERT_TRUE(doc.commit(create));
    auto const variant{doc.variants().ids().back()};
    auto edit{doc.transaction()};
    edit.set_variant_field_bits(variant, "game", "Storage", "samples.value", 6);
    edit.set_variant_field_bits(variant, "game", "Storage", "samples_extra", 8);
    edit.rename_member("game", "Storage", "samples", "items");
    ASSERT_TRUE(doc.commit(edit)) << diagnostic_messages(editor);
    auto const& settings{doc.variants().settings(variant)};
    ASSERT_EQ(settings.field_names.size(), 2);
    EXPECT_EQ(doc.types().strings().view(settings.field_names[0]), "items.value");
    EXPECT_EQ(doc.types().strings().view(settings.field_names[1]), "samples_extra");
    auto invalid{doc.transaction()};
    invalid.set_variant_field_bits(variant, "game", "Storage", "items_0_value", 7);
    EXPECT_FALSE(doc.commit(invalid));
    EXPECT_EQ(doc.variants().settings(variant).field_names.size(), 2);
    auto erase{doc.transaction()};
    erase.delete_member("game", "Storage", "items");
    ASSERT_TRUE(doc.commit(erase)) << diagnostic_messages(editor);
    ASSERT_EQ(doc.variants().settings(variant).field_names.size(), 1);
    EXPECT_EQ(doc.types().strings().view(doc.variants().settings(variant).field_names[0]),
              "samples_extra");
}
} // namespace ioj::layout::workspace_tests
