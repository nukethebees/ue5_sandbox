#include "gui/planner_ui.hpp"
#include "gui/planner_ui_support.hpp"

#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/manifest.h>
#include <codegen/schema/module_settings_store.h>
#include <codegen/schema/normal_module_schemas.h>
#include <codegen/schema/record_member_schemas.h>
#include <codegen/schema/record_schemas.h>
#include <codegen/schema/schema_version.h>
#include <codegen/schema/type_refs.h>
#include <codegen/schema/type_registry.h>
#include <lispb/compiler_storage.h>
#include <lispb/project_loader.h>
#include <lispb/schema/editable_document.h>
#include <lispb/schema/indices.h>

#include <gtest/gtest.h>
#include <imgui_internal.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ioj::layout_planner {

struct PlannerUiTestAccess {
    static auto open_project(PlannerUi& ui, std::filesystem::path const& path) -> bool {
        return ui.load_project(path, false, true);
    }
    static void project_dialogs(PlannerUi& ui) { ui.draw_project_path_dialogs(); }
    static auto target_name(PlannerUi const& ui) -> std::string const& { return ui.target_name_; }
    static auto project_path(PlannerUi const& ui) -> std::filesystem::path const& {
        return ui.project_path_;
    }
    static void browser(PlannerUi& ui) { ui.draw_project_panel(); }
    static void files(PlannerUi& ui) { ui.schema_file_view_ = true; }
    static auto session(PlannerUi& ui) -> layout::PlannerAnalysisSession& {
        return ui.analysis_session_;
    }
    static void graph(PlannerUi& ui) { ui.draw_graph_panel(); }
    static void external(PlannerUi& ui, lispb::schema::TypeId type) {
        ui.draw_external_facts(type);
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
auto find_declared_type(lispb::schema::TypeGraph const& types,
                        std::string_view module,
                        std::string_view name) -> std::optional<lispb::schema::TypeId> {
    for (auto const node : types.types()) {
        if (types.strings().view(types.module_name(node)) == module &&
            types.strings().view(types.name(node)) == name) {
            return types.find(types.identity(node));
        }
    }
    return std::nullopt;
}

auto ui_fixture() -> layout::SchemaLoadResult {
    auto storage{std::make_unique<lispb::CompilerStorage>()};
    auto& declarations{storage->declarations};
    auto& strings{storage->strings};
    lispb::codegen::Manifest manifest{lispb::codegen::manifest_schema_version,
                                      lispb::codegen::TypeRegistry{storage->registered_types},
                                      declarations,
                                      {}};
    for (auto const name : {"first", "second"}) {
        auto const module{declarations.normal_modules.append()};

        declarations.normal_modules.settings[declarations.normal_modules.index(module)] = [&] {
            auto const new_module_settings_store{declarations.module_settings_store.append()};
            declarations.module_settings_store
                .name[declarations.module_settings_store.index(new_module_settings_store)] =
                strings.intern(name);
            declarations.module_settings_store
                .header[declarations.module_settings_store.index(new_module_settings_store)] =
                strings.add(std::string{name} + ".h");
            return new_module_settings_store;
        }();
        auto const record{declarations.records.append()};

        declarations.records.name[declarations.records.index(record)] =
            strings.intern(std::string{name} + "Record");
        auto const member{declarations.record_members.append()};

        declarations.record_members.name[declarations.record_members.index(member)] =
            strings.intern("value");
        declarations.record_members.type[declarations.record_members.index(member)] = [&] {
            auto const new_type_refs{declarations.type_refs.append()};
            declarations.type_refs.name[declarations.type_refs.index(new_type_refs)] =
                strings.intern("OpaqueVector");
            return new_type_refs;
        }();
        declarations.records.members[declarations.records.index(record)] =
            declarations.range<lispb::codegen::RecordMemberId>({member});
        declarations.normal_modules.declarations[declarations.normal_modules.index(module)] =
            declarations.range<lispb::codegen::DeclarationHandle>({record});
        manifest.modules.push_back(module);
    }
    layout::SchemaLoadResult loaded{};
    loaded.document = lispb::schema::EditableSchemaDocument::from_manifest(std::move(storage),
                                                                           std::move(manifest));
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

TEST_F(PlannerActions, ProjectChooserLoadsSelectedProductionSchemas) {
    auto initial{ui_fixture()};
    initial.project_path = "previous/project.lispb";
    PlannerUi ui{std::move(initial)};
    auto const project{std::filesystem::path{IOJ_SOURCE_DIR} / "lispb/project.lispb"};
    EXPECT_FALSE(PlannerUiTestAccess::open_project(ui, project));
    auto const draw_dialog{[&] {
        ImGui::NewFrame();
        PlannerUiTestAccess::project_dialogs(ui);
        ImGui::Render();
    }};
    draw_dialog();
    draw_dialog();

    auto const* dialog{ImGui::FindWindowByName("Choose schemas to open")};
    ASSERT_NE(dialog, nullptr);
    EXPECT_GE(dialog->Size.x, ImGui::GetFontSize() * 40.0F);
    activate("Choose schemas to open", "sandbox-code\nGame and simulation types");
    draw_dialog();

    EXPECT_EQ(PlannerUiTestAccess::target_name(ui), "sandbox-code");
    EXPECT_EQ(PlannerUiTestAccess::project_path(ui).lexically_normal(), project.lexically_normal());
    auto const& types{PlannerUiTestAccess::session(ui).inputs.workspace.types()};
    EXPECT_TRUE(find_declared_type(types, "native_sim_state", "OrchestratorState").has_value());
    EXPECT_TRUE(
        find_declared_type(types, "native_simulation_phase", "SimulationPhase").has_value());
}

TEST_F(PlannerActions, GraphButtonsSeparateFocusSelectionAndStableModuleScope) {
    PlannerUi ui{ui_fixture()};
    auto& session{PlannerUiTestAccess::session(ui)};
    auto const& types{session.inputs.workspace.types()};
    auto const first{*find_declared_type(types, "first", "firstRecord")};
    auto const second{*find_declared_type(types, "second", "secondRecord")};
    session.inputs.selection.select_type(types, first);
    frame(ui);
    frame(ui);
    EXPECT_FALSE(PlannerUiTestAccess::focus(ui));
    activate("Graph", "Focus selection");
    frame(ui);
    EXPECT_EQ(PlannerUiTestAccess::focus(ui), types.identity(first));
    activate("Graph", "View selected module");
    frame(ui);
    EXPECT_EQ(PlannerUiTestAccess::scope(ui).module, "first");
    EXPECT_EQ(PlannerUiTestAccess::projection(ui).nodes.size(), 2);
    session.inputs.selection.select_type(types, second);
    frame(ui);
    EXPECT_EQ(PlannerUiTestAccess::scope(ui).module, "first");
    EXPECT_EQ(PlannerUiTestAccess::focus(ui), types.identity(first));
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
