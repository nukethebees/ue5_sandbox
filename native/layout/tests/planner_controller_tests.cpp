#include "document_test_support.hpp"
#include <ioj/layout/editor_session.hpp>
#include <ioj/layout/planner_controller.hpp>

#include <gtest/gtest.h>

#include <string_view>
#include <utility>

namespace ioj::layout::planner_controller_tests {
constexpr std::string_view source{R"((module game :header "Game.h"
    (record State :layout packed (member health @LayoutEditValue :bits 8)
    (projections (cpp :storage std::uint16_t)))
    (record Position (member x float) (member y float)
    (projections (cpp))) (integer-scalar LayoutEditValue :bit-width 1)))"};

TEST(PlannerController, DrainsOrderedCommandsAndAnalyzesOncePerChangedFrame) {
    EditorSession editor;
    PlannerController controller{editor, editor.from_source(source)};
    ASSERT_TRUE(controller.drain()) << diagnostic_messages(editor);
    EXPECT_EQ(controller.analysis_count(), 1);
    EXPECT_FALSE(controller.drain());
    EXPECT_EQ(controller.analysis_count(), 1);

    auto edit{controller.document()->transaction()};
    edit.set_member_bits("game", "State", "health", 12);
    controller.submit(std::move(edit));
    controller.select_declaration("game", "Position");
    controller.select_declaration("game", "State");
    EXPECT_EQ(controller.document()->query().module("game").record("State").member("health").bits(),
              8);
    EXPECT_EQ(controller.analysis_count(), 1);
    ASSERT_TRUE(controller.drain()) << diagnostic_messages(editor);
    EXPECT_EQ(controller.document()->query().module("game").record("State").member("health").bits(),
              12);
    EXPECT_EQ(controller.analysis_count(), 2);
    EXPECT_FALSE(controller.drain());
    EXPECT_EQ(controller.analysis_count(), 2);
}

TEST(PlannerController, RejectedEditsRetainRedoAndReportCentralDiagnostics) {
    EditorSession editor;
    PlannerController controller{editor, editor.from_source(source)};
    ASSERT_TRUE(controller.drain());
    auto edit{controller.document()->transaction()};
    edit.set_member_bits("game", "State", "health", 12);
    controller.submit(std::move(edit));
    ASSERT_TRUE(controller.drain());
    controller.undo();
    ASSERT_TRUE(controller.drain());
    EXPECT_TRUE(controller.document()->can_redo());
    auto invalid{controller.document()->transaction()};
    invalid.set_member_bits("game", "State", "health", 32);
    controller.submit(std::move(invalid));
    controller.drain();
    EXPECT_GT(editor.diagnostics().size(), 0);
    EXPECT_TRUE(controller.document()->can_redo());
    EXPECT_EQ(controller.document()->query().module("game").record("State").member("health").bits(),
              8);
    controller.redo();
    ASSERT_TRUE(controller.drain()) << diagnostic_messages(editor);
    EXPECT_EQ(controller.document()->query().module("game").record("State").member("health").bits(),
              12);
}

TEST(PlannerController, MultipleHistoryCommandsPublishOnlyTheFinalState) {
    EditorSession editor;
    PlannerController controller{editor, editor.from_source(source)};
    ASSERT_TRUE(controller.drain());
    auto edit{controller.document()->transaction()};
    edit.set_member_bits("game", "State", "health", 4);
    controller.submit(std::move(edit));
    controller.undo();
    controller.redo();
    ASSERT_TRUE(controller.drain());
    EXPECT_EQ(controller.analysis_count(), 2);
    EXPECT_EQ(controller.document()->query().module("game").record("State").member("health").bits(),
              4);
    EXPECT_FALSE(controller.drain());
}
}
