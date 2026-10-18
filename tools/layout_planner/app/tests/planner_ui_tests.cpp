#include "gui/planner_ui.hpp"

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
    PlannerUi ui;
    draw(ui);
    EXPECT_EQ(ui.window_title(), "Memory Layout Planner");
    ui.request_close();
    EXPECT_TRUE(ui.take_close_confirmation());
    EXPECT_FALSE(ui.take_close_confirmation());
}

TEST_F(PlannerUiTest, RemembersWindowSizeThroughSettings) {
    PlannerUi ui;
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
    auto document{layout::Document::from_source("(broken")};
    ASSERT_FALSE(document.valid());
    PlannerUi ui{std::move(document)};
    draw(ui);
}

TEST_F(PlannerUiTest, ChangedDocumentRequiresCloseConfirmation) {
    auto document{layout::Document::from_source(R"(
        (module game :header "Game.h" :backend standard-library
            (packed-value ShipState :storage std::uint16_t
                (field health std::uint16_t :bits 8))))")};
    ASSERT_TRUE(document.valid());
    auto transaction{document.transaction()};
    transaction.set_field_bits("game", "ShipState", "health", 4);
    ASSERT_TRUE(document.commit(transaction));
    PlannerUi ui{std::move(document)};
    draw(ui);
    EXPECT_TRUE(ui.window_title().ends_with(" *"));
    ui.request_close();
    EXPECT_FALSE(ui.take_close_confirmation());
    draw(ui);
}
}
}
