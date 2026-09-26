#include "gui/planner_ui.hpp"
#include "gui/planner_ui_support.hpp"

#include <codegen/schema/schema_version.h>

#include <gtest/gtest.h>
#include <imgui_internal.h>

namespace ioj::layout_planner {

struct PlannerUiTestAccess {
    static void browser(PlannerUi& ui) { ui.draw_project_panel(); }
    static void files(PlannerUi& ui) { ui.schema_file_view_ = true; }
    static auto session(PlannerUi& ui) -> layout::PlannerAnalysisSession& {
        return ui.analysis_session_;
    }
    static void graph(PlannerUi& ui) { ui.draw_graph_panel(); }
    static void external(PlannerUi& ui, lispb::schema::TypeId type) {
        ui.draw_external_facts(session(ui).inputs.workspace.types().type(type));
    }
    static auto focus(PlannerUi const& ui) { return ui.graph_focus_; }
    static auto scope(PlannerUi const& ui) { return ui.graph_scope_; }
    static auto projection(PlannerUi const& ui) -> layout::GraphProjection const& {
        return ui.graph_projection_;
    }
    static auto positions(PlannerUi& ui) -> std::vector<layout::GraphPoint>& {
        return ui.graph_positions_;
    }
    static void fact_inputs(PlannerUi& ui) {
        std::snprintf(ui.external_size_.data(), ui.external_size_.size(), "12");
        std::snprintf(ui.external_alignment_.data(), ui.external_alignment_.size(), "4");
    }
    static auto fact_edits(PlannerUi const& ui) -> bool {
        return ui.unedited_target_profile_.has_value();
    }
    static void discard_facts(PlannerUi& ui) { ui.discard_target_profile_edits(); }
};

namespace {
auto ui_fixture() -> layout::SchemaLoadResult {
    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    for (auto const name : {"first", "second"}) {
        codegen::NormalModuleSchema module{};
        module.settings.name = name;
        module.settings.header = std::string{name} + ".h";
        codegen::RecordSchema record{};
        record.name = std::string{name} + "Record";
        codegen::RecordMemberSchema member{};
        member.name = "value";
        member.type.name = "OpaqueVector";
        record.members.push_back(member);
        module.declarations.push_back(record);
        manifest.modules.emplace_back(module);
    }
    layout::SchemaLoadResult loaded{};
    loaded.document = lispb::schema::EditableSchemaDocument::from_manifest(std::move(manifest));
    loaded.loaded = true;
    return loaded;
}

class PlannerActions : public testing::Test {
  protected:
    void SetUp() override {
        ImGui::CreateContext();
        auto& io{ImGui::GetIO()};
        io.IniFilename = nullptr;
        io.DisplaySize = {1500.0F, 1000.0F};
        unsigned char* pixels{};
        int width{};
        int height{};
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    }
    void TearDown() override { ImGui::DestroyContext(); }
    static void activate(char const* window, char const* label) {
        auto* target{ImGui::FindWindowByName(window)};
        ASSERT_NE(target, nullptr);
        ImGui::ActivateItemByID(target->GetID(label));
    }
    static void frame(PlannerUi& ui) {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0.0F, 0.0F});
        ImGui::SetNextWindowSize({1400.0F, 900.0F});
        PlannerUiTestAccess::graph(ui);
        ImGui::Render();
    }
};

TEST_F(PlannerActions, GraphButtonsSeparateFocusSelectionAndStableModuleScope) {
    PlannerUi ui{ui_fixture()};
    auto& session{PlannerUiTestAccess::session(ui)};
    auto const& types{session.inputs.workspace.types()};
    auto const first{*types.find_declared("first", "firstRecord")};
    auto const second{*types.find_declared("second", "secondRecord")};
    session.inputs.selection.select_type(types, first);
    frame(ui);
    frame(ui);
    EXPECT_FALSE(PlannerUiTestAccess::focus(ui));
    activate("Graph", "Focus selection");
    frame(ui);
    EXPECT_EQ(PlannerUiTestAccess::focus(ui), types.type(first).identity);
    activate("Graph", "View selected module");
    frame(ui);
    EXPECT_EQ(PlannerUiTestAccess::scope(ui).module, "first");
    EXPECT_EQ(PlannerUiTestAccess::projection(ui).nodes.size(), 2);
    session.inputs.selection.select_type(types, second);
    frame(ui);
    EXPECT_EQ(PlannerUiTestAccess::scope(ui).module, "first");
    EXPECT_EQ(PlannerUiTestAccess::focus(ui), types.type(first).identity);
    activate("Graph", "Clear focus");
    frame(ui);
    EXPECT_FALSE(PlannerUiTestAccess::focus(ui));
    EXPECT_EQ(session.inputs.selection.type, second);
    auto& positions{PlannerUiTestAccess::positions(ui)};
    positions[0] = {100000.0F, -50000.0F};
    frame(ui);
    EXPECT_EQ(positions[0], (layout::GraphPoint{100000.0F, -50000.0F}));
}

TEST_F(PlannerActions, ExternalApplyButtonLabelsAssumptionsAndDiscardRestoresProfile) {
    PlannerUi ui{ui_fixture()};
    auto& session{PlannerUiTestAccess::session(ui)};
    auto const& types{session.inputs.workspace.types()};
    auto const type{layout::external_dependencies(types).front().types.front()};
    session.inputs.selection.select_type(types, type);
    ASSERT_TRUE(session.refresh(nullptr));
    auto const external_frame{[&] {
        ImGui::NewFrame();
        ImGui::SetNextWindowSize({1000.0F, 900.0F});
        ImGui::Begin("External test");
        PlannerUiTestAccess::external(ui, type);
        ImGui::End();
        ImGui::Render();
    }};
    external_frame();
    external_frame();
    PlannerUiTestAccess::fact_inputs(ui);
    activate("External test", "Apply manual facts");
    external_frame();
    ASSERT_TRUE(session.primary_abi().find("OpaqueVector"));
    EXPECT_EQ(session.primary_abi().find("OpaqueVector")->origin,
              layout::FactOrigin::manual_assumption);
    EXPECT_TRUE(PlannerUiTestAccess::fact_edits(ui));
    PlannerUiTestAccess::discard_facts(ui);
    EXPECT_FALSE(session.primary_abi().find("OpaqueVector"));
    EXPECT_FALSE(PlannerUiTestAccess::fact_edits(ui));
}

TEST_F(PlannerActions, ModuleButtonsReachModulesHiddenUnderCollapsedSources) {
    PlannerUi ui{ui_fixture()};
    PlannerUiTestAccess::files(ui);
    auto const browser_frame{[&] {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0.0F, 0.0F});
        ImGui::SetNextWindowSize({1400.0F, 950.0F});
        PlannerUiTestAccess::browser(ui);
        ImGui::Render();
    }};
    browser_frame();
    browser_frame();
    auto* window{ImGui::FindWindowByName("Project / Schema")};
    ASSERT_NE(window, nullptr);
    auto const source_id{window->GetID("Unsaved modules")};
    auto const module_id{ImHashStr("first  [module]", 0, ImHashStr("first", 0, source_id))};
    activate("Project / Schema", "Expand all modules");
    browser_frame();
    EXPECT_EQ(window->StateStorage.GetInt(source_id), 1);
    EXPECT_EQ(window->StateStorage.GetInt(module_id), 1);
    activate("Project / Schema", "Collapse all modules");
    browser_frame();
    EXPECT_EQ(window->StateStorage.GetInt(source_id), 0);
    // Opening only the parent later must not resurrect the child's pre-collapse state.
    window->StateStorage.SetInt(source_id, 1);
    browser_frame();
    EXPECT_EQ(window->StateStorage.GetInt(module_id), 0);
    activate("Project / Schema", "Expand all modules");
    browser_frame();
    EXPECT_EQ(window->StateStorage.GetInt(module_id), 1);
    window->StateStorage.SetInt(module_id, 0);
    browser_frame();
    EXPECT_EQ(window->StateStorage.GetInt(module_id), 0);
}
} // namespace
} // namespace ioj::layout_planner
