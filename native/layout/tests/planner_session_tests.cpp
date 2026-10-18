#include <ioj/layout/document.hpp>
#include <ioj/layout/planner_session.hpp>

#include <gtest/gtest.h>

#include <string_view>

namespace ioj::layout {
namespace planner_session_tests {
constexpr std::string_view source{R"((module game :header "Game.h"
    (packed-value State :storage std::uint16_t (field health std::uint16_t :bits 8))
    (record Position (member x float) (member y float))
    (union Payload (alternative small std::uint8_t) (alternative large std::uint32_t))))"};

TEST(PlannerSession, RefreshesOnlyAfterDocumentOrInputChanges) {
    auto document{Document::from_source(source)};
    ASSERT_TRUE(document.valid());
    PlannerAnalysisSession session{document.types()};
    auto const type{document.find_type("game", "State").value()};
    session.edit_inputs().selection.select_type(document.types(), type);
    ASSERT_TRUE(session.refresh(&document));
    ASSERT_TRUE(session.results().active_packed);
    EXPECT_EQ(session.results().active_packed->bits_used, 8);
    EXPECT_FALSE(session.refresh(&document));

    auto tx{document.transaction()};
    tx.set_field_bits("game", "State", "health", 12);
    ASSERT_TRUE(document.commit(tx));
    ASSERT_TRUE(session.refresh(&document));
    EXPECT_EQ(session.results().active_packed->bits_used, 12);
    EXPECT_FALSE(session.refresh(&document));
    ASSERT_TRUE(document.undo());
    ASSERT_TRUE(session.refresh(&document));
    EXPECT_EQ(session.results().active_packed->bits_used, 8);

    session.edit_inputs().access_multiplicity = 20;
    EXPECT_TRUE(session.refresh(&document));
    EXPECT_FALSE(session.refresh(&document));
}

TEST(PlannerSession, SelectionChangeClearsTypeSpecificAnalysis) {
    auto document{Document::from_source(source)};
    ASSERT_TRUE(document.valid());
    PlannerAnalysisSession session{document.types()};
    auto& inputs{session.edit_inputs()};
    inputs.selection.select_type(document.types(), document.find_type("game", "Position"));
    inputs.selection.field = "x";
    ASSERT_TRUE(session.refresh(&document));
    ASSERT_TRUE(session.results().record_access_analysis);
    session.edit_inputs().selection.select_type(document.types(),
                                                document.find_type("game", "State"));
    ASSERT_TRUE(session.refresh(&document));
    EXPECT_FALSE(session.results().record_analysis);
    EXPECT_FALSE(session.results().record_access_analysis);
    EXPECT_TRUE(session.results().active_packed);
    EXPECT_TRUE(session.inputs().selection.field.empty());
}

TEST(PlannerSession, UnknownAbiRemainsUnknownAndTargetChangesRefreshBothSides) {
    auto document{Document::from_source(source)};
    ASSERT_TRUE(document.valid());
    PlannerAnalysisSession session{document.types()};
    auto const type{document.find_type("game", "Position").value()};
    session.edit_inputs().selection.select_type(document.types(), type);
    ASSERT_TRUE(session.refresh(&document));
    EXPECT_EQ(session.results().record_analysis->size_bytes, 8);
    ASSERT_TRUE(session.set_primary_abi(AbiProfile{"unknown"}));
    ASSERT_TRUE(session.refresh(&document));
    EXPECT_FALSE(session.results().record_analysis->size_bytes);
    EXPECT_EQ(session.status(type), LayoutStatus::unknown);
    EXPECT_FALSE(session.set_primary_abi(AbiProfile{"unknown"}));
    EXPECT_FALSE(session.refresh(&document));
    ASSERT_TRUE(session.set_comparison_abi(AbiProfile{"other unknown"}));
    ASSERT_TRUE(session.refresh(&document));
    ASSERT_TRUE(session.results().record_target_comparison);
    EXPECT_FALSE(session.results().record_target_comparison->second.size_bytes);
}

TEST(PlannerSession, DistributionUpdatesProduceFreshAnalysisAndUndoRetainsOwner) {
    auto document{Document::from_source(source)};
    ASSERT_TRUE(document.valid());
    PlannerAnalysisSession session{document.types()};
    auto const type{document.find_type("game", "Payload").value()};
    auto const declaration{document.find_declaration(document.types().identity(type)).value()};
    session.edit_inputs().selection.select_type(document.types(), type);
    ASSERT_TRUE(session.set_union_distribution_weight(declaration, "small", 3));
    ASSERT_TRUE(session.set_union_distribution_weight(declaration, "large", 1));
    ASSERT_TRUE(session.refresh(&document));
    ASSERT_TRUE(session.results().union_distribution_analysis);
    EXPECT_FALSE(session.set_union_distribution_weight(declaration, "small", 3));
    EXPECT_FALSE(session.refresh(&document));
    auto tx{document.transaction()};
    tx.set_field_bits("game", "State", "health", 4);
    ASSERT_TRUE(document.commit(tx));
    ASSERT_TRUE(session.refresh(&document));
    ASSERT_TRUE(session.union_distribution(declaration));
    EXPECT_EQ(session.union_distribution(declaration)->at("small"), 3);
    ASSERT_TRUE(document.undo());
    ASSERT_TRUE(session.refresh(&document));
    EXPECT_EQ(session.union_distribution(declaration)->at("large"), 1);
    EXPECT_TRUE(session.clear_union_distribution(declaration));
    EXPECT_TRUE(session.refresh(&document));
    EXPECT_FALSE(session.union_distribution(declaration));
}

TEST(PlannerSession, DeletedSelectionIsClearedOnRefresh) {
    auto document{Document::from_source(source)};
    ASSERT_TRUE(document.valid());
    PlannerAnalysisSession session{document.types()};
    session.edit_inputs().selection.select_type(document.types(),
                                                document.find_type("game", "Position"));
    ASSERT_TRUE(session.refresh(&document));
    auto tx{document.transaction()};
    tx.delete_declaration("game", "Position");
    ASSERT_TRUE(document.commit(tx));
    ASSERT_TRUE(session.refresh(&document));
    EXPECT_FALSE(session.inputs().selection.type);
    EXPECT_FALSE(session.results().record_analysis);
}
} // namespace
} // namespace ioj::layout
