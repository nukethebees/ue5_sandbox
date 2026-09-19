#include "planner_ui.hpp"

#include "planner_ui_support.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <cstdio>
#include <utility>
#include <variant>

namespace ioj::layout_planner {
namespace {

using namespace layout;

} // namespace

PlannerUi::PlannerUi(CatalogLoadResult loaded)
    : workspace_{std::move(loaded.catalog)}
    , load_diagnostics_{std::move(loaded.diagnostics)} {
    for (auto const& representation : loaded.type_representations) {
        abi_.set_representation(representation.spelling, representation.represented_by);
    }
    auto const& items{workspace_.catalog().items()};
    if (!items.empty()) {
        selected_schema_ = detail::definition_id(items.front());
    }
    sync_variant_name();
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
            changed = true;
        }
        if (ImGui::MenuItem("Reset text size")) {
            text_scale_ = 1.0F;
            changed = true;
        }
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
    return changed;
}

void PlannerUi::setup_default_dock_layout(unsigned int const dockspace_id) {
    auto const* existing_node{ImGui::DockBuilderGetNode(dockspace_id)};
    if (dock_layout_initialized_ || existing_node == nullptr || existing_node->IsSplitNode()) {
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
        ImGui::DockBuilderSplitNode(center_id, ImGuiDir_Right, 0.27F, nullptr, &center_id)};
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
}

void PlannerUi::refresh_analysis() {
    if (cached_revision_ == workspace_.revision() && cached_schema_ == selected_schema_) {
        return;
    }
    baseline_packed_.reset();
    active_packed_.reset();
    baseline_soa_.reset();
    active_soa_.reset();
    cached_revision_ = workspace_.revision();
    cached_schema_ = selected_schema_;
    if (!selected_schema_.has_value()) {
        return;
    }
    auto const* definition{workspace_.catalog().find(*selected_schema_)};
    if (definition == nullptr) {
        return;
    }
    auto const& baseline{*workspace_.variant(LayoutWorkspace::baseline_variant_id)};
    auto const& active{workspace_.active_variant()};
    if (auto const* packed{std::get_if<PackedLayout>(definition)}) {
        baseline_packed_ = Analyzer::analyze(*packed, baseline, abi_);
        active_packed_ = Analyzer::analyze(*packed, active, abi_);
    } else if (auto const* soa{std::get_if<SoaLayout>(definition)}) {
        baseline_soa_ = Analyzer::analyze(*soa, baseline, abi_, workspace_.default_capacity());
        active_soa_ = Analyzer::analyze(*soa, active, abi_, workspace_.default_capacity());
    }
}

void PlannerUi::draw_layout_panel() {
    ImGui::Begin("Layout");
    if (!selected_schema_.has_value()) {
        ImGui::TextDisabled("Select a supported schema.");
    } else if (auto const* definition{workspace_.catalog().find(*selected_schema_)};
               definition == nullptr) {
        ImGui::TextDisabled("The selected schema is unavailable.");
    } else if (auto const* packed{std::get_if<PackedLayout>(definition)}) {
        draw_packed_layout(*packed, *baseline_packed_, *active_packed_);
    } else if (auto const* soa{std::get_if<SoaLayout>(definition)}) {
        draw_soa_layout(*soa, *baseline_soa_, *active_soa_);
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
    auto const name{selected_schema_.has_value()
                        ? selected_schema_->schema_name + " experiment " +
                              std::to_string(next_variant_number_++)
                        : "Experiment " + std::to_string(next_variant_number_++)};
    workspace_.create_variant(name);
    sync_variant_name();
    refresh_analysis();
}

} // namespace ioj::layout_planner
