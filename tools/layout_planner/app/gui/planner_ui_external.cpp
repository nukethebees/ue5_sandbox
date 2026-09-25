#include <imgui_internal.h>
#include "../platform/file_dialog.hpp"
#include "planner_ui_properties_common.hpp"

namespace ioj::layout_planner {

auto PlannerUi::export_target_profile() -> bool {
    if (file_dialog_ == nullptr) {
        target_profile_load_error_ = "Profile export requires a file dialog.";
        return false;
    }
    auto const suggested{target_profile_path_.front() == '\0' ? "target.profile"
                                                              : target_profile_path_.data()};
    auto chosen{file_dialog_->save_file(suggested, "profile")};
    if (!chosen.has_value()) {
        target_profile_load_error_ = chosen.error();
        return false;
    }
    if (!chosen->has_value()) {
        return false;
    }
    auto const saved{save_abi_profile(**chosen, analysis_session_.primary_abi())};
    if (!saved) {
        target_profile_load_error_ = saved.error();
        return false;
    }
    auto const path{std::filesystem::absolute(**chosen).lexically_normal()};
    std::snprintf(
        target_profile_path_.data(), target_profile_path_.size(), "%s", path.string().c_str());
    if (!project_path_.empty()) {
        persisted_target_profile_paths_[project_path_.lexically_normal().generic_string()] = path;
        ImGui::MarkIniSettingsDirty();
    }
    unedited_target_profile_.reset();
    target_profile_load_error_.clear();
    return true;
}

void PlannerUi::discard_target_profile_edits() {
    if (unedited_target_profile_.has_value()) {
        analysis_session_.set_primary_abi(std::move(*unedited_target_profile_));
        unedited_target_profile_.reset();
        sync_target_memory_fact_inputs();
        refresh_analysis();
    }
}

void PlannerUi::draw_external_facts(TypeNode const& node) {
    auto const& analysis{analysis_session_.results().external_type};
    if (!analysis.has_value()) {
        ImGui::TextDisabled("External physical analysis is unavailable.");
        return;
    }
    auto const& facts{analysis->physical.facts};
    ImGui::SeparatorText("Target physical facts");
    ImGui::TextWrapped("Semantic description: %s; physical layout: %s",
                       analysis->semantics_known ? "known" : "unknown",
                       facts.has_value() ? "known" : "unknown");
    auto const& profile{analysis_session_.primary_abi()};
    ImGui::TextWrapped("Profile: %s", profile.name().c_str());
    ImGui::TextWrapped("Target: %s / %s / %s",
                       profile.identity().platform.value_or("Unknown platform").c_str(),
                       profile.identity().architecture.value_or("Unknown architecture").c_str(),
                       profile.identity().compiler.value_or("Unknown compiler").c_str());
    ImGui::TextWrapped("Profile file: %s",
                       target_profile_path_.front() == '\0' ? "Built-in/session"
                                                            : target_profile_path_.data());
    if (facts.has_value()) {
        ImGui::TextWrapped("Size: %llu bytes; alignment: %llu bytes",
                           static_cast<unsigned long long>(facts->size_bytes),
                           static_cast<unsigned long long>(facts->alignment_bytes));
        ImGui::TextWrapped("Evidence: %s; source: %s",
                           fact_origin_name(facts->origin).data(),
                           facts->provenance.c_str());
    }
    if (external_facts_identity_ != node.identity || external_facts_source_ != facts) {
        external_facts_identity_ = node.identity;
        external_facts_source_ = facts;
        auto const size{facts ? std::to_string(facts->size_bytes) : std::string{}};
        auto const alignment{facts ? std::to_string(facts->alignment_bytes) : std::string{}};
        std::snprintf(external_size_.data(), external_size_.size(), "%s", size.c_str());
        std::snprintf(
            external_alignment_.data(), external_alignment_.size(), "%s", alignment.c_str());
        external_provenance_.fill('\0');
        external_facts_error_.clear();
    }
    detail::prepare_property_input("Size (bytes)");
    ImGui::InputText("Size (bytes)",
                     external_size_.data(),
                     external_size_.size(),
                     ImGuiInputTextFlags_CharsDecimal);
    detail::prepare_property_input("Alignment (bytes)");
    ImGui::InputText("Alignment (bytes)",
                     external_alignment_.data(),
                     external_alignment_.size(),
                     ImGuiInputTextFlags_CharsDecimal);
    detail::prepare_property_input("Assumption source");
    ImGui::InputText("Assumption source", external_provenance_.data(), external_provenance_.size());
    detail::WrappingButtonRow buttons;
    if (buttons.button("Apply manual facts")) {
        auto const size{detail::parse_unsigned(external_size_.data())};
        auto const alignment{detail::parse_unsigned(external_alignment_.data())};
        if (!size.has_value() || !alignment.has_value()) {
            external_facts_error_ = "Enter a complete-object size and alignment in bytes.";
        } else {
            auto edited{facts.value_or(TypeFacts{})};
            edited.size_bytes = *size;
            edited.alignment_bytes = *alignment;
            edited.provenance = external_provenance_.data();
            auto const previous{profile};
            auto const applied{analysis_session_.set_external_type_facts(
                *analysis_session_.inputs.selection.type, std::move(edited))};
            if (!applied) {
                external_facts_error_ = applied.error();
            } else if (*applied) {
                if (!unedited_target_profile_.has_value()) {
                    unedited_target_profile_ = previous;
                }
                refresh_analysis();
                return;
            }
        }
    }
    if (buttons.button("Target profile / import")) {
        target_profile_view_open_ = true;
        focus_target_profile_view_ = true;
    }
    if (buttons.button("Export profile")) {
        static_cast<void>(export_target_profile());
    }
    if (!external_facts_error_.empty()) {
        ImGui::TextWrapped("%s", external_facts_error_.c_str());
    }
    if (unedited_target_profile_.has_value()) {
        ImGui::TextWrapped("Manual facts are session changes. Export the profile to keep them.");
    }
    ImGui::TextWrapped("Analyses blocked by missing facts: %zu",
                       analysis->blocked_declarations.size());
    for (auto const& identity : analysis->blocked_declarations) {
        auto const label{identity.module_name + "::" + identity.name};
        if (ImGui::Selectable(label.c_str())) {
            if (auto const target{analysis_session_.inputs.workspace.types().find(identity)}) {
                select_type(*target);
                return;
            }
        }
    }
    draw_diagnostics(analysis->physical.diagnostics);
}

} // namespace ioj::layout_planner
