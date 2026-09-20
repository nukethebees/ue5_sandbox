#include "planner_ui.hpp"

#include "planner_ui_support.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <charconv>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <utility>
#include <variant>

namespace ioj::layout_planner {
namespace {

using namespace layout;
using namespace lispb::schema;

auto parse_float_setting(std::string_view const line, std::string_view const prefix)
    -> std::optional<float> {
    if (!line.starts_with(prefix)) {
        return std::nullopt;
    }
    auto const value{line.substr(prefix.size())};
    float parsed{};
    auto const [end, error]{std::from_chars(value.data(), value.data() + value.size(), parsed)};
    return error == std::errc{} && end == value.data() + value.size() ? std::optional{parsed}
                                                                      : std::nullopt;
}

auto parse_int_setting(std::string_view const line, std::string_view const prefix)
    -> std::optional<int> {
    if (!line.starts_with(prefix)) {
        return std::nullopt;
    }
    auto const value{line.substr(prefix.size())};
    int parsed{};
    auto const [end, error]{std::from_chars(value.data(), value.data() + value.size(), parsed)};
    return error == std::errc{} && end == value.data() + value.size() ? std::optional{parsed}
                                                                      : std::nullopt;
}

} // namespace

PlannerUi::PlannerUi(SchemaLoadResult loaded)
    : project_path_{std::move(loaded.project_path)}
    , target_name_{std::move(loaded.target_name)}
    , document_{std::move(loaded.document)}
    , workspace_{document_.has_value() ? document_->types() : TypeGraph{}}
    , load_diagnostics_{std::move(loaded.diagnostics)} {
    auto const types{workspace_.types().types()};
    auto const found{std::ranges::find_if(types, [](auto const& type) {
        if (std::holds_alternative<EnumType>(type.definition) ||
            std::holds_alternative<PackedType>(type.definition) ||
            std::holds_alternative<RecordType>(type.definition)) {
            return true;
        }
        auto const* soa{std::get_if<SoaType>(&type.definition)};
        return soa != nullptr && soa->backend == codegen::SoaBackend::standard_library;
    })};
    if (found != types.end()) {
        selected_type_ = TypeId{static_cast<std::uint32_t>(found - types.begin())};
    }
    sync_variant_name();
}

void PlannerUi::register_settings_handler() {
    ImGuiSettingsHandler handler;
    handler.TypeName = "MemoryLayoutPlanner";
    handler.TypeHash = ImHashStr(handler.TypeName);
    handler.ReadOpenFn = settings_read_open;
    handler.ReadLineFn = settings_read_line;
    handler.WriteAllFn = settings_write_all;
    handler.UserData = this;
    ImGui::AddSettingsHandler(&handler);
}

void PlannerUi::finish_startup(bool const reopen_recent_project) {
    auto const fallback_path{project_path_};
    if (reopen_recent_project) {
        auto const candidates{recent_projects_};
        for (auto const& path : candidates) {
            if (load_project(path, true)) {
                return;
            }
        }
    }
    if (!fallback_path.empty()) {
        static_cast<void>(load_project(fallback_path, true));
    }
}

auto PlannerUi::saved_window_size() const -> std::optional<WindowSize> {
    if (!window_width_.has_value() || !window_height_.has_value() || *window_width_ <= 0 ||
        *window_height_ <= 0) {
        return std::nullopt;
    }
    return WindowSize{.width = *window_width_, .height = *window_height_};
}

void PlannerUi::remember_window_size(WindowSize const size) {
    if (size.width <= 0 || size.height <= 0 ||
        (window_width_ == size.width && window_height_ == size.height)) {
        return;
    }
    window_width_ = size.width;
    window_height_ = size.height;
    ImGui::MarkIniSettingsDirty();
}

void PlannerUi::request_close() {
    if (!document_.has_value() || !document_->dirty()) {
        close_confirmed_ = true;
        return;
    }
    open_close_confirmation_ = true;
}

auto PlannerUi::take_close_confirmation() -> bool {
    return std::exchange(close_confirmed_, false);
}

void PlannerUi::validate_comparison_variants() {
    if (workspace_.variant(comparison_a_variant_id_) == nullptr) {
        comparison_a_variant_id_ = LayoutWorkspace::baseline_variant_id;
    }
    if (comparison_b_follows_active_) {
        comparison_b_variant_id_ = workspace_.active_variant_id();
    } else if (workspace_.variant(comparison_b_variant_id_) == nullptr) {
        comparison_b_variant_id_ = LayoutWorkspace::baseline_variant_id;
    }
}

auto PlannerUi::settings_read_open(ImGuiContext*, ImGuiSettingsHandler* handler, char const* name)
    -> void* {
    if (std::strcmp(name, "Settings") != 0) {
        return nullptr;
    }
    auto* ui{static_cast<PlannerUi*>(handler->UserData)};
    ui->recent_projects_.clear();
    return ui;
}

void PlannerUi::settings_read_line(ImGuiContext*,
                                   ImGuiSettingsHandler*,
                                   void* entry,
                                   char const* line) {
    auto* ui{static_cast<PlannerUi*>(entry)};
    auto const value{std::string_view{line}};
    if (auto const text_scale{parse_float_setting(value, "TextScale=")}) {
        ui->text_scale_ = std::clamp(*text_scale, 0.75F, 1.75F);
        return;
    }
    if (auto const width{parse_int_setting(value, "WindowWidth=")}) {
        ui->window_width_ = *width;
        return;
    }
    if (auto const height{parse_int_setting(value, "WindowHeight=")}) {
        ui->window_height_ = *height;
        return;
    }
    constexpr std::string_view graph_open_prefix{"GraphOpen="};
    if (value.starts_with(graph_open_prefix)) {
        ui->graph_view_open_ = value.substr(graph_open_prefix.size()) != "0";
        return;
    }
    constexpr std::string_view recent_prefix{"RecentProject="};
    if (value.starts_with(recent_prefix) && value.size() > recent_prefix.size() &&
        ui->recent_projects_.size() < 20) {
        ui->recent_projects_.emplace_back(value.substr(recent_prefix.size()));
    }
}

void PlannerUi::settings_write_all(ImGuiContext*,
                                   ImGuiSettingsHandler* handler,
                                   ImGuiTextBuffer* output) {
    auto const* ui{static_cast<PlannerUi const*>(handler->UserData)};
    output->appendf("[%s][Settings]\n", handler->TypeName);
    output->appendf("TextScale=%g\n", ui->text_scale_);
    if (auto const size{ui->saved_window_size()}) {
        output->appendf("WindowWidth=%d\n", size->width);
        output->appendf("WindowHeight=%d\n", size->height);
    }
    output->appendf("GraphOpen=%d\n", ui->graph_view_open_ ? 1 : 0);
    for (auto const& path : ui->recent_projects_) {
        output->appendf("RecentProject=%s\n", path.string().c_str());
    }
    output->append("\n");
}

auto PlannerUi::draw() -> bool {
    ImGui::GetStyle().FontScaleMain = text_scale_;
    auto const view_changed{draw_view_menu()};
    ImGui::GetStyle().FontScaleMain = text_scale_;
    auto const dockspace_id{ImGui::DockSpaceOverViewport()};
    setup_default_dock_layout(dockspace_id);
    refresh_analysis();

    auto const revision_before{workspace_.revision()};
    draw_project_panel();
    refresh_analysis();
    draw_layout_panel();
    refresh_analysis();
    draw_properties_panel();
    draw_variants_panel();
    refresh_analysis();
    draw_comparison_panel();
    draw_graph_panel();
    draw_new_enum_dialog();
    draw_new_packed_value_dialog();
    draw_new_soa_dialog();
    draw_source_preview();
    draw_close_confirmation();
    draw_project_path_dialogs();
    return view_changed || revision_before != workspace_.revision() ||
           std::exchange(project_changed_, false);
}

auto PlannerUi::draw_view_menu() -> bool {
    bool changed{};
    if (!ImGui::BeginMainMenuBar()) {
        return false;
    }
    changed |= draw_file_menu();
    if (ImGui::BeginMenu("View")) {
        auto percentage{text_scale_ * 100.0F};
        if (ImGui::SliderFloat("Text size", &percentage, 75.0F, 175.0F, "%.0f%%")) {
            text_scale_ = percentage / 100.0F;
            ImGui::MarkIniSettingsDirty();
            changed = true;
        }
        if (ImGui::MenuItem("Reset text size")) {
            text_scale_ = 1.0F;
            ImGui::MarkIniSettingsDirty();
            changed = true;
        }
        if (ImGui::MenuItem("Reset panel layout")) {
            reset_dock_layout_requested_ = true;
            changed = true;
        }
        if (ImGui::MenuItem("Graph", nullptr, graph_view_open_)) {
            graph_view_open_ = !graph_view_open_;
            ImGui::MarkIniSettingsDirty();
            changed = true;
        }
        ImGui::EndMenu();
    }
    if (document_.has_value() && document_->dirty()) {
        ImGui::SameLine();
        ImGui::TextColored({0.95F, 0.72F, 0.25F, 1.0F}, "Unsaved LispB changes");
    }
    ImGui::EndMainMenuBar();
    return changed;
}

auto PlannerUi::draw_file_menu() -> bool {
    bool changed{};
    if (!ImGui::BeginMenu("File")) {
        return false;
    }
    auto const has_document{document_.has_value()};
    if (ImGui::MenuItem("Open Project...")) {
        std::snprintf(open_project_path_.data(),
                      open_project_path_.size(),
                      "%s",
                      project_path_.string().c_str());
        open_project_dialog_ = true;
    }
    if (ImGui::BeginMenu("Open Recent", !recent_projects_.empty())) {
        for (auto const& path : recent_projects_) {
            auto const label{path.string()};
            if (ImGui::MenuItem(label.c_str(), nullptr, path == project_path_)) {
                changed |= load_project(path);
            }
        }
        ImGui::EndMenu();
    }
    ImGui::Separator();
    ImGui::BeginDisabled(!has_document || !document_->can_undo());
    if (ImGui::MenuItem("Undo")) {
        auto const selection{selected_type_.transform(
            [&](TypeId const type) { return workspace_.types().type(type).identity; })};
        auto result{document_->undo()};
        if (result.has_value() && *result) {
            sync_document_graph(selection);
            changed = true;
        } else if (!result.has_value()) {
            schema_edit_message_ = result.error().message;
        }
    }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!has_document || !document_->can_redo());
    if (ImGui::MenuItem("Redo")) {
        auto const selection{selected_type_.transform(
            [&](TypeId const type) { return workspace_.types().type(type).identity; })};
        auto result{document_->redo()};
        if (result.has_value() && *result) {
            sync_document_graph(selection);
            changed = true;
        } else if (!result.has_value()) {
            schema_edit_message_ = result.error().message;
        }
    }
    ImGui::EndDisabled();
    ImGui::Separator();
    ImGui::BeginDisabled(!has_document || !document_->dirty());
    if (ImGui::MenuItem("Preview LispB changes")) {
        open_source_preview_ = true;
    }
    if (ImGui::MenuItem("Save")) {
        auto const selection{selected_type_.transform(
            [&](TypeId const type) { return workspace_.types().type(type).identity; })};
        auto result{document_->save()};
        if (result.has_value()) {
            sync_document_graph(selection);
            schema_edit_message_ =
                "Saved and reloaded " + std::to_string(result->size()) + " LispB source file(s).";
            changed = true;
        } else {
            schema_edit_message_ = result.error().message;
        }
    }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!has_document);
    if (ImGui::MenuItem("Save As...")) {
        auto destination{project_path_.parent_path() /
                         (project_path_.stem().string() + "_copy.lispb")};
        std::snprintf(save_as_project_path_.data(),
                      save_as_project_path_.size(),
                      "%s",
                      destination.string().c_str());
        open_save_as_dialog_ = true;
    }
    ImGui::EndDisabled();
    ImGui::EndMenu();

    return changed;
}

void PlannerUi::draw_project_path_dialogs() {
    if (open_project_dialog_) {
        ImGui::OpenPopup("Open LispB project");
        open_project_dialog_ = false;
    }
    if (ImGui::BeginPopupModal("Open LispB project", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Project manifest path");
        ImGui::SetNextItemWidth(720.0F);
        ImGui::InputText(
            "##open-project-path", open_project_path_.data(), open_project_path_.size());
        ImGui::BeginDisabled(open_project_path_.front() == '\0');
        if (ImGui::Button("Open")) {
            if (load_project(open_project_path_.data())) {
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        if (!schema_edit_message_.empty()) {
            ImGui::TextWrapped("%s", schema_edit_message_.c_str());
        }
        ImGui::EndPopup();
    }

    if (open_save_as_dialog_) {
        ImGui::OpenPopup("Save LispB project as");
        open_save_as_dialog_ = false;
    }
    if (ImGui::BeginPopupModal(
            "Save LispB project as", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("New project manifest path");
        ImGui::SetNextItemWidth(720.0F);
        ImGui::InputText(
            "##save-as-project-path", save_as_project_path_.data(), save_as_project_path_.size());
        ImGui::TextDisabled("A sibling <name>_schema directory will contain the cloned sources.");
        ImGui::BeginDisabled(save_as_project_path_.front() == '\0' || !document_.has_value());
        if (ImGui::Button("Save As")) {
            auto cloned{clone_lispb_schema(*document_, save_as_project_path_.data(), target_name_)};
            if (cloned.loaded) {
                adopt_loaded_schema(std::move(cloned));
                schema_edit_message_ = "Saved and opened the cloned LispB project.";
                ImGui::CloseCurrentPopup();
            } else {
                schema_edit_message_ = cloned.diagnostics.empty()
                                         ? "Could not clone the LispB project."
                                         : cloned.diagnostics.front().message;
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        if (!schema_edit_message_.empty()) {
            ImGui::TextWrapped("%s", schema_edit_message_.c_str());
        }
        ImGui::EndPopup();
    }
}

void PlannerUi::draw_source_preview() {
    if (open_source_preview_) {
        ImGui::OpenPopup("LispB source preview");
        open_source_preview_ = false;
    }
    if (!ImGui::BeginPopupModal(
            "LispB source preview", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    auto updates{document_->preview_source_updates()};
    if (!updates.has_value()) {
        ImGui::TextWrapped("%s", updates.error().message.c_str());
    } else {
        for (auto const& update : *updates) {
            ImGui::SeparatorText(update.path.string().c_str());
            if (ImGui::BeginTabBar(update.path.string().c_str())) {
                if (ImGui::BeginTabItem("Updated")) {
                    ImGui::BeginChild("updated-source", {760.0F, 360.0F}, true);
                    ImGui::TextUnformatted(update.updated.c_str());
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Original")) {
                    ImGui::BeginChild("original-source", {760.0F, 360.0F}, true);
                    ImGui::TextUnformatted(update.original.c_str());
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
    }
    if (ImGui::Button("Close")) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void PlannerUi::draw_close_confirmation() {
    if (open_close_confirmation_) {
        ImGui::OpenPopup("Unsaved LispB changes");
        open_close_confirmation_ = false;
    }
    if (!ImGui::BeginPopupModal(
            "Unsaved LispB changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    ImGui::TextWrapped("The editable schema contains unsaved LispB changes.");
    ImGui::TextUnformatted("Save them before closing?");
    if (ImGui::Button("Save and close")) {
        auto result{document_->save()};
        if (result.has_value()) {
            close_confirmed_ = true;
            ImGui::CloseCurrentPopup();
        } else {
            schema_edit_message_ = result.error().message;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Discard and close")) {
        close_confirmed_ = true;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        ImGui::CloseCurrentPopup();
    }
    if (!schema_edit_message_.empty()) {
        ImGui::TextWrapped("%s", schema_edit_message_.c_str());
    }
    ImGui::EndPopup();
}

auto PlannerUi::apply_document_edit(SchemaEditCommand command,
                                    std::optional<TypeIdentity> selection) -> bool {
    if (!document_.has_value()) {
        schema_edit_message_ = "No editable LispB document is loaded.";
        return false;
    }
    if (!selection.has_value() && selected_type_.has_value()) {
        selection = workspace_.types().type(*selected_type_).identity;
    }
    auto result{document_->apply(std::move(command))};
    if (!result.has_value()) {
        schema_edit_message_ = result.error().message;
        return false;
    }
    if (!*result) {
        return false;
    }
    schema_edit_message_.clear();
    sync_document_graph(std::move(selection));
    return true;
}

void PlannerUi::sync_document_graph(std::optional<TypeIdentity> selection) {
    workspace_.replace_types(document_->types());
    selected_type_.reset();
    if (selection.has_value()) {
        selected_type_ = workspace_.types().find(*selection);
    }
    selected_field_.clear();
    enum_editor_declaration_.reset();
    enum_editor_value_.clear();
    packed_editor_declaration_.reset();
    packed_editor_field_.clear();
    soa_editor_declaration_.reset();
    soa_editor_member_.clear();
    packed_dragged_divider_.reset();
    packed_dragged_variant_id_.reset();
    packed_dragged_left_width_.reset();
    packed_dragged_right_width_.reset();
}

auto PlannerUi::load_project(std::filesystem::path const& path, bool const allow_dirty) -> bool {
    if (!allow_dirty && document_.has_value() && document_->dirty()) {
        schema_edit_message_ =
            "Save or undo the current LispB changes before opening another project.";
        return false;
    }
    auto loaded{load_lispb_schema(path, target_name_)};
    if (!loaded.loaded) {
        schema_edit_message_ = loaded.diagnostics.empty() ? "Could not load the LispB project."
                                                          : loaded.diagnostics.front().message;
        return false;
    }
    adopt_loaded_schema(std::move(loaded));
    schema_edit_message_.clear();
    return true;
}

void PlannerUi::adopt_loaded_schema(SchemaLoadResult loaded) {
    project_path_ = std::move(loaded.project_path);
    target_name_ = std::move(loaded.target_name);
    document_ = std::move(loaded.document);
    load_diagnostics_ = std::move(loaded.diagnostics);
    workspace_ = LayoutWorkspace{document_.has_value() ? document_->types() : TypeGraph{}};
    selected_type_.reset();
    auto const types{workspace_.types().types()};
    for (std::size_t index{}; index < types.size(); ++index) {
        auto const& definition{types[index].definition};
        auto const* soa{std::get_if<SoaType>(&definition)};
        if (std::holds_alternative<EnumType>(definition) ||
            std::holds_alternative<PackedType>(definition) ||
            std::holds_alternative<RecordType>(definition) ||
            (soa != nullptr && soa->backend == codegen::SoaBackend::standard_library)) {
            selected_type_ = TypeId{static_cast<std::uint32_t>(index)};
            break;
        }
    }
    selected_field_.clear();
    selected_enumerator_.clear();
    enum_editor_declaration_.reset();
    enum_editor_value_.clear();
    packed_editor_declaration_.reset();
    packed_editor_field_.clear();
    soa_editor_declaration_.reset();
    soa_editor_member_.clear();
    packed_dragged_divider_.reset();
    packed_dragged_variant_id_.reset();
    packed_dragged_left_width_.reset();
    packed_dragged_right_width_.reset();
    graph_pan_x_ = 32.0F;
    graph_pan_y_ = 32.0F;
    graph_zoom_ = 1.0F;
    graph_focus_selected_ = false;
    cached_type_.reset();
    cached_revision_ = std::numeric_limits<std::uint64_t>::max();
    comparison_a_variant_id_ = LayoutWorkspace::baseline_variant_id;
    comparison_b_variant_id_ = LayoutWorkspace::baseline_variant_id;
    comparison_b_follows_active_ = true;
    project_changed_ = true;
    sync_variant_name();
    remember_recent_project(project_path_);
}

void PlannerUi::remember_recent_project(std::filesystem::path const& path) {
    if (path.empty()) {
        return;
    }
    auto const normalized{std::filesystem::absolute(path).lexically_normal()};
    recent_projects_.erase(
        std::remove(recent_projects_.begin(), recent_projects_.end(), normalized),
        recent_projects_.end());
    recent_projects_.insert(recent_projects_.begin(), normalized);
    if (recent_projects_.size() > 20) {
        recent_projects_.resize(20);
    }
    ImGui::MarkIniSettingsDirty();
}

void PlannerUi::setup_default_dock_layout(unsigned int const dockspace_id) {
    auto const* existing_node{ImGui::DockBuilderGetNode(dockspace_id)};
    if (!reset_dock_layout_requested_ &&
        (dock_layout_initialized_ || existing_node == nullptr || existing_node->IsSplitNode())) {
        dock_layout_initialized_ = true;
        return;
    }
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->WorkSize);

    auto center_id{dockspace_id};
    auto const left_id{
        ImGui::DockBuilderSplitNode(center_id, ImGuiDir_Left, 0.22F, nullptr, &center_id)};
    auto const right_id{
        ImGui::DockBuilderSplitNode(center_id, ImGuiDir_Right, 0.34F, nullptr, &center_id)};
    auto const comparison_id{
        ImGui::DockBuilderSplitNode(center_id, ImGuiDir_Down, 0.34F, nullptr, &center_id)};
    auto left_top_id{left_id};
    auto const variants_id{
        ImGui::DockBuilderSplitNode(left_top_id, ImGuiDir_Down, 0.34F, nullptr, &left_top_id)};
    ImGui::DockBuilderDockWindow("Project / Schema", left_top_id);
    ImGui::DockBuilderDockWindow("Variants", variants_id);
    ImGui::DockBuilderDockWindow("Graph", center_id);
    ImGui::DockBuilderDockWindow("Layout", center_id);
    ImGui::DockBuilderDockWindow("Properties", right_id);
    ImGui::DockBuilderDockWindow("Comparison", comparison_id);
    ImGui::DockBuilderFinish(dockspace_id);
    dock_layout_initialized_ = true;
    reset_dock_layout_requested_ = false;
}

void PlannerUi::refresh_analysis() {
    validate_comparison_variants();
    if (cached_revision_ == workspace_.revision() && cached_type_ == selected_type_ &&
        cached_comparison_a_variant_id_ == comparison_a_variant_id_ &&
        cached_comparison_b_variant_id_ == comparison_b_variant_id_) {
        return;
    }
    enum_domain_.reset();
    baseline_packed_.reset();
    active_packed_.reset();
    packed_variants_.clear();
    baseline_soa_.reset();
    active_soa_.reset();
    soa_variants_.clear();
    comparison_a_packed_.reset();
    comparison_b_packed_.reset();
    comparison_a_soa_.reset();
    comparison_b_soa_.reset();
    record_analysis_.reset();
    cached_revision_ = workspace_.revision();
    cached_type_ = selected_type_;
    cached_comparison_a_variant_id_ = comparison_a_variant_id_;
    cached_comparison_b_variant_id_ = comparison_b_variant_id_;
    if (!selected_type_.has_value()) {
        return;
    }
    auto const& definition{workspace_.types().type(*selected_type_).definition};
    auto const& baseline{*workspace_.variant(LayoutWorkspace::baseline_variant_id)};
    auto const& active{workspace_.active_variant()};
    auto const& comparison_a{*workspace_.variant(comparison_a_variant_id_)};
    auto const& comparison_b{*workspace_.variant(comparison_b_variant_id_)};
    auto const element_count{workspace_.element_count()};
    if (std::holds_alternative<EnumType>(definition)) {
        enum_domain_ = Analyzer::analyze_enum(workspace_.types(), *selected_type_, abi_);
    } else if (std::holds_alternative<RecordType>(definition)) {
        record_analysis_ = Analyzer::analyze_record(workspace_.types(), *selected_type_, abi_);
    } else if (std::holds_alternative<PackedType>(definition)) {
        baseline_packed_ = Analyzer::analyze_packed(
            workspace_.types(), *selected_type_, baseline, abi_, element_count);
        active_packed_ = Analyzer::analyze_packed(
            workspace_.types(), *selected_type_, active, abi_, element_count);
        for (auto const& variant : workspace_.variants()) {
            if (variant.id != LayoutWorkspace::baseline_variant_id) {
                packed_variants_.emplace_back(
                    variant.id,
                    Analyzer::analyze_packed(
                        workspace_.types(), *selected_type_, variant, abi_, element_count));
            }
        }
        comparison_a_packed_ = Analyzer::analyze_packed(
            workspace_.types(), *selected_type_, comparison_a, abi_, element_count);
        comparison_b_packed_ = Analyzer::analyze_packed(
            workspace_.types(), *selected_type_, comparison_b, abi_, element_count);
    } else if (auto const* soa{std::get_if<SoaType>(&definition)};
               soa != nullptr && soa->backend == codegen::SoaBackend::standard_library) {
        baseline_soa_ = Analyzer::analyze_soa(
            workspace_.types(), *selected_type_, baseline, abi_, workspace_.default_capacity());
        active_soa_ = Analyzer::analyze_soa(
            workspace_.types(), *selected_type_, active, abi_, workspace_.default_capacity());
        for (auto const& variant : workspace_.variants()) {
            if (variant.id != LayoutWorkspace::baseline_variant_id) {
                soa_variants_.emplace_back(variant.id,
                                           Analyzer::analyze_soa(workspace_.types(),
                                                                 *selected_type_,
                                                                 variant,
                                                                 abi_,
                                                                 workspace_.default_capacity()));
            }
        }
        comparison_a_soa_ = Analyzer::analyze_soa(
            workspace_.types(), *selected_type_, comparison_a, abi_, workspace_.default_capacity());
        comparison_b_soa_ = Analyzer::analyze_soa(
            workspace_.types(), *selected_type_, comparison_b, abi_, workspace_.default_capacity());
    }
}

void PlannerUi::draw_layout_panel() {
    ImGui::Begin("Layout");
    if (ImGui::Button("+ Add variant")) {
        create_variant_for_selected_schema();
    }
    ImGui::Separator();
    if (!selected_type_.has_value()) {
        ImGui::TextDisabled("Select a supported schema.");
    } else if (auto const& definition{workspace_.types().type(*selected_type_).definition};
               auto const* packed = std::get_if<PackedType>(&definition)) {
        draw_packed_layout(*packed, *baseline_packed_);
    } else if (auto const* soa{std::get_if<SoaType>(&definition)};
               soa != nullptr && soa->backend == codegen::SoaBackend::standard_library) {
        draw_soa_layout(*soa, *baseline_soa_);
    } else if (std::holds_alternative<RecordType>(definition)) {
        draw_record_layout(*record_analysis_);
    } else if (std::holds_alternative<EnumType>(definition)) {
        ImGui::TextDisabled("Enums have semantic metadata but no standalone aggregate layout.");
    } else {
        ImGui::TextDisabled("This type is not supported by the layout analyzer.");
    }
    ImGui::End();
}

void PlannerUi::draw_diagnostics(std::vector<Diagnostic> const& diagnostics) const {
    for (auto const& diagnostic : diagnostics) {
        ImGui::PushStyleColor(ImGuiCol_Text, detail::diagnostic_color(diagnostic.severity));
        ImGui::TextWrapped("%s", diagnostic.message.c_str());
        ImGui::PopStyleColor();
    }
}

void PlannerUi::sync_variant_name() {
    auto const& name{workspace_.active_variant().name};
    std::snprintf(variant_name_.data(), variant_name_.size(), "%s", name.c_str());
    variant_name_id_ = workspace_.active_variant_id();
}

void PlannerUi::create_variant_for_selected_schema() {
    auto const name{selected_type_.has_value()
                        ? workspace_.types().type(*selected_type_).identity.name + " experiment " +
                              std::to_string(next_variant_number_++)
                        : "Experiment " + std::to_string(next_variant_number_++)};
    workspace_.create_variant(name);
    sync_variant_name();
    refresh_analysis();
}

} // namespace ioj::layout_planner
