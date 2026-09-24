#include "gui/planner_ui_support.hpp"

#include <gtest/gtest.h>
#include <imgui_internal.h>

namespace ioj::layout_planner {
namespace {

TEST(PlannerTable, ComboAutoFitIncludesArrowAndIgnoresCurrentWidgetWidth) {
    ImGui::CreateContext();
    auto& io{ImGui::GetIO()};
    io.IniFilename = nullptr;
    io.DisplaySize = {800.0F, 600.0F};
    unsigned char* pixels{};
    int width{};
    int height{};
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    for (int frame{}; frame < 2; ++frame) {
        ImGui::NewFrame();
        ImGui::SetNextWindowSize({700.0F, 400.0F});
        ImGui::Begin("Table test");
        if (detail::begin_editable_table("fields", 1, 1)) {
            ImGui::TableSetupColumn("Kind", ImGuiTableColumnFlags_WidthFixed, 400.0F);
            ImGui::TableHeadersRow();
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1.0F);
            if (ImGui::BeginCombo("##kind", "unsigned")) {
                ImGui::EndCombo();
            }
            detail::editable_table_content_hint("unsigned", ImGui::GetFrameHeight());
            auto* table{ImGui::GetCurrentTable()};
            auto const expected{ImGui::CalcTextSize("unsigned").x +
                                2.0F * ImGui::GetStyle().FramePadding.x + ImGui::GetFrameHeight()};
            EXPECT_FLOAT_EQ(table->InnerWindow->DC.CursorMaxPos.x - table->Columns[0].WorkMinX,
                            expected);
            ImGui::TableNextRow();
            auto const measured{table->Columns[0].ContentMaxXUnfrozen - table->Columns[0].WorkMinX};
            EXPECT_FLOAT_EQ(measured, expected);
            EXPECT_LT(measured, 400.0F);
            ImGui::EndTable();
        }
        ImGui::End();
        ImGui::Render();
    }

    ImGui::DestroyContext();
}

} // namespace
} // namespace ioj::layout_planner
