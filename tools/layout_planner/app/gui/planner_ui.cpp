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
    : workspace_{std::move(loaded.types)}
    , load_diagnostics_{std::move(loaded.diagnostics)} {
    auto const types{workspace_.types().types()};
    auto const found{std::ranges::find_if(types, [](auto const& type) {
        if (std::holds_alternative<EnumType>(type.definition) ||
            std::holds_alternative<PackedType>(type.definition)) {
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
    return std::strcmp(name, "Settings") == 0 ? handler->UserData : nullptr;
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
    draw_properties_panel();
    draw_variants_panel();
    refresh_analysis();
    draw_comparison_panel();
    return view_changed || revision_before != workspace_.revision();
}

auto PlannerUi::draw_view_menu() -> bool {
    bool changed{};
    if (!ImGui::BeginMainMenuBar()) {
        return false;
    }
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
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
    return changed;
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
    baseline_packed_.reset();
    active_packed_.reset();
    baseline_soa_.reset();
    active_soa_.reset();
    comparison_a_packed_.reset();
    comparison_b_packed_.reset();
    comparison_a_soa_.reset();
    comparison_b_soa_.reset();
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
    if (std::holds_alternative<PackedType>(definition)) {
        baseline_packed_ =
            Analyzer::analyze_packed(workspace_.types(), *selected_type_, baseline, abi_);
        active_packed_ =
            Analyzer::analyze_packed(workspace_.types(), *selected_type_, active, abi_);
        comparison_a_packed_ =
            Analyzer::analyze_packed(workspace_.types(), *selected_type_, comparison_a, abi_);
        comparison_b_packed_ =
            Analyzer::analyze_packed(workspace_.types(), *selected_type_, comparison_b, abi_);
    } else if (auto const* soa{std::get_if<SoaType>(&definition)};
               soa != nullptr && soa->backend == codegen::SoaBackend::standard_library) {
        baseline_soa_ = Analyzer::analyze_soa(
            workspace_.types(), *selected_type_, baseline, abi_, workspace_.default_capacity());
        active_soa_ = Analyzer::analyze_soa(
            workspace_.types(), *selected_type_, active, abi_, workspace_.default_capacity());
        comparison_a_soa_ = Analyzer::analyze_soa(
            workspace_.types(), *selected_type_, comparison_a, abi_, workspace_.default_capacity());
        comparison_b_soa_ = Analyzer::analyze_soa(
            workspace_.types(), *selected_type_, comparison_b, abi_, workspace_.default_capacity());
    }
}

void PlannerUi::draw_layout_panel() {
    ImGui::Begin("Layout");
    if (!selected_type_.has_value()) {
        ImGui::TextDisabled("Select a supported schema.");
    } else if (auto const& definition{workspace_.types().type(*selected_type_).definition};
               auto const* packed = std::get_if<PackedType>(&definition)) {
        draw_packed_layout(*packed, *baseline_packed_, *active_packed_);
    } else if (auto const* soa{std::get_if<SoaType>(&definition)};
               soa != nullptr && soa->backend == codegen::SoaBackend::standard_library) {
        draw_soa_layout(*soa, *baseline_soa_, *active_soa_);
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
