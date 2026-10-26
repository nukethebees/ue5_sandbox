#include "gui/planner_ui.hpp"
#include <ioj/layout/editor_session.hpp>

#include <gtest/gtest.h>
#include <imgui.h>

#include <string>
#include <string_view>
#include <utility>

namespace ioj::layout_planner {
namespace {
class PlannerUiTest : public testing::Test {
  protected:
    void SetUp() override {
        ImGui::CreateContext();
        auto& io{ImGui::GetIO()};
        io.IniFilename = nullptr;
        io.DisplaySize = {1200.0F, 800.0F};
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        unsigned char* pixels{};
        int width{};
        int height{};
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    }
    void TearDown() override { ImGui::DestroyContext(); }
    void draw(PlannerUi& ui) {
        ImGui::NewFrame();
        ui.draw();
        ImGui::Render();
    }
};

TEST_F(PlannerUiTest, EmptyWorkspaceCanDrawAndClose) {
    layout::EditorSession editor;
    PlannerUi ui{editor};
    draw(ui);
    EXPECT_EQ(ui.window_title(), "Memory Layout Planner");
    ui.request_close();
    EXPECT_TRUE(ui.take_close_confirmation());
    EXPECT_FALSE(ui.take_close_confirmation());
}

TEST_F(PlannerUiTest, RemembersWindowSizeThroughSettings) {
    layout::EditorSession editor;
    PlannerUi ui{editor};
    ui.register_settings_handler();
    ui.remember_window_size({1280, 720});
    auto const settings{std::string{ImGui::SaveIniSettingsToMemory()}};
    ui.remember_window_size({800, 600});
    ImGui::LoadIniSettingsFromMemory(settings.c_str());
    ASSERT_TRUE(ui.saved_window_size());
    EXPECT_EQ(ui.saved_window_size()->width, 1280);
    EXPECT_EQ(ui.saved_window_size()->height, 720);
}

TEST_F(PlannerUiTest, InvalidInputCanDrawDiagnostics) {
    layout::EditorSession editor;
    auto document{editor.from_source("(broken")};
    ASSERT_FALSE(document.valid());
    PlannerUi ui{editor, std::move(document)};
    draw(ui);
}

TEST_F(PlannerUiTest, ChangedDocumentRequiresCloseConfirmation) {
    layout::EditorSession editor;
    auto document{editor.from_source(R"(
        (module game :header "Game.h"
            (record ShipState :layout packed
                (member health @LayoutEditValue :bits 8)
    (projections (cpp :storage std::uint16_t))) (integer-scalar LayoutEditValue :bit-width 1)))")};
    ASSERT_TRUE(document.valid());
    auto transaction{document.transaction()};
    transaction.set_member_bits("game", "ShipState", "health", 4);
    ASSERT_TRUE(document.commit(transaction));
    PlannerUi ui{editor, std::move(document)};
    draw(ui);
    EXPECT_TRUE(ui.window_title().ends_with(" *"));
    ui.request_close();
    EXPECT_FALSE(ui.take_close_confirmation());
    draw(ui);
}

TEST_F(PlannerUiTest, UnchangedFramesDoNotRepeatAnalysisDiagnostics) {
    layout::EditorSession editor;
    auto document{editor.from_source(R"((module game :header "Game.h"
        (record Value (member value UnknownExternal)
    (projections (cpp)))))")};
    ASSERT_TRUE(document.valid());
    PlannerUi ui{editor, std::move(document)};
    draw(ui);
    auto const count{editor.diagnostics().size()};
    ASSERT_GT(count, 0);
    draw(ui);
    draw(ui);
    EXPECT_EQ(editor.diagnostics().size(), count);
}
}
}
