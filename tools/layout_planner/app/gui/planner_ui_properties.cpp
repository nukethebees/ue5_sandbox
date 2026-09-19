#include "planner_ui.hpp"

#include "planner_ui_support.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <variant>

namespace ioj::layout_planner {
namespace {

using namespace layout;

void draw_override_note(bool const overridden) {
    if (overridden) {
        ImGui::SameLine();
        ImGui::TextColored({0.4F, 0.75F, 0.95F, 1.0F}, "Override");
    }
}

} // namespace

void PlannerUi::draw_properties_panel() {
    ImGui::Begin("Properties");
    if (!selected_schema_.has_value()) {
        ImGui::TextDisabled("No selection.");
        ImGui::End();
        return;
    }
    auto const* definition{workspace_.catalog().find(*selected_schema_)};
    if (definition == nullptr) {
        ImGui::TextDisabled("The selected schema is unavailable.");
        ImGui::End();
        return;
    }

    auto editable{workspace_.active_variant_id() != LayoutWorkspace::baseline_variant_id};
    if (!editable) {
        ImGui::TextUnformatted("Baseline");
        ImGui::TextDisabled("Loaded from LispB — read only.");
        ImGui::TextWrapped(
            "Experiments are session-only variants. The production schema is never modified.");
        if (auto const* soa{std::get_if<SoaLayout>(definition)}; soa != nullptr) {
            static_cast<void>(soa);
            ImGui::TextDisabled("Capacity uses the planner default of %llu.",
                                static_cast<unsigned long long>(workspace_.default_capacity()));
        }
        if (ImGui::Button("Create editable variant", {-1.0F, 0.0F})) {
            create_variant_for_selected_schema();
            editable = true;
        }
        ImGui::Separator();
    } else {
        ImGui::Text("Editing %s", workspace_.active_variant().name.c_str());
        ImGui::TextDisabled("Session-only experiment");
        ImGui::Separator();
    }

    ImGui::BeginDisabled(!editable);
    if (auto const* packed{std::get_if<PackedLayout>(definition)}) {
        ImGui::TextUnformatted("Packed storage");
        ImGui::Text("Schema storage: %s", packed->storage_type.c_str());
        auto const& analysis{*active_packed_};
        if (ImGui::BeginCombo("Planning storage", analysis.storage_type.c_str())) {
            if (ImGui::Selectable("Schema storage", !analysis.storage_overridden)) {
                workspace_.set_packed_storage_type(packed->id, std::nullopt);
            }
            for (auto const& [type, facts] : abi_.types()) {
                if (!facts.unsigned_value_bits.has_value()) {
                    continue;
                }
                if (ImGui::Selectable(type.c_str(), analysis.storage_type == type)) {
                    workspace_.set_packed_storage_type(packed->id, type);
                }
            }
            ImGui::EndCombo();
        }
        draw_override_note(analysis.storage_overridden);
        if (analysis.storage_overridden) {
            ImGui::SameLine();
            if (ImGui::SmallButton("Reset storage")) {
                workspace_.set_packed_storage_type(packed->id, std::nullopt);
            }
        }

        if (selected_field_.empty() && !packed->fields.empty()) {
            selected_field_ = packed->fields.front().name;
        }
        if (auto const* field{detail::packed_field(*packed, selected_field_)}; field != nullptr) {
            auto const found{
                std::ranges::find(analysis.fields, field->name, &PackedFieldAnalysis::name)};
            if (found != analysis.fields.end()) {
                ImGui::SeparatorText(field->name.c_str());
                ImGui::Text("Logical type: %s", found->logical_type.c_str());
                ImGui::Text("Schema width: %u bits", found->schema_bit_width);
                auto width{found->bit_width};
                if (ImGui::InputScalar("Planning width", ImGuiDataType_U32, &width)) {
                    width = std::clamp(width, std::uint32_t{1}, std::uint32_t{64});
                    workspace_.set_packed_field_width(packed->id, field->name, width);
                }
                draw_override_note(found->overridden);
                if (found->overridden) {
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Reset field")) {
                        workspace_.set_packed_field_width(packed->id, field->name, std::nullopt);
                    }
                }
            }
        }
    } else if (auto const* soa{std::get_if<SoaLayout>(definition)}) {
        auto const& analysis{*active_soa_};
        ImGui::TextUnformatted("Planner capacity");
        ImGui::TextDisabled("Baseline default: %llu",
                            static_cast<unsigned long long>(workspace_.default_capacity()));
        auto capacity{analysis.capacity};
        if (ImGui::InputScalar("Capacity", ImGuiDataType_U64, &capacity)) {
            workspace_.set_capacity(soa->id, capacity);
        }
        draw_override_note(analysis.capacity_overridden);
        if (analysis.capacity_overridden) {
            ImGui::SameLine();
            if (ImGui::SmallButton("Reset capacity")) {
                workspace_.set_capacity(soa->id, std::nullopt);
            }
        }
        for (auto const quick : {std::uint64_t{1},
                                 std::uint64_t{4'096},
                                 std::uint64_t{16'384},
                                 std::uint64_t{65'536}}) {
            ImGui::SameLine();
            auto const label{std::to_string(quick)};
            if (ImGui::SmallButton(label.c_str())) {
                workspace_.set_capacity(soa->id, quick);
            }
        }

        if (selected_field_.empty() && !soa->columns.empty()) {
            selected_field_ = soa->columns.front().name;
        }
        if (auto const* column{detail::soa_column(*soa, selected_field_)}; column != nullptr) {
            auto const found{
                std::ranges::find(analysis.columns, column->name, &SoaColumnAnalysis::name)};
            if (found != analysis.columns.end()) {
                ImGui::SeparatorText(column->name.c_str());
                ImGui::Text("Schema type: %s", found->schema_type.c_str());
                if (ImGui::BeginCombo("Planning type", found->physical_type.c_str())) {
                    if (ImGui::Selectable("Schema type", !found->overridden)) {
                        workspace_.set_soa_column_type(soa->id, column->name, std::nullopt);
                    }
                    for (auto const& [type, facts] : abi_.types()) {
                        static_cast<void>(facts);
                        if (ImGui::Selectable(type.c_str(), found->physical_type == type)) {
                            workspace_.set_soa_column_type(soa->id, column->name, type);
                        }
                    }
                    ImGui::EndCombo();
                }
                draw_override_note(found->overridden);
                if (found->overridden) {
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Reset column")) {
                        workspace_.set_soa_column_type(soa->id, column->name, std::nullopt);
                    }
                }
            }
        }
    }
    ImGui::EndDisabled();
    ImGui::End();
}

void PlannerUi::draw_variants_panel() {
    ImGui::Begin("Variants");
    for (auto const& variant : workspace_.variants()) {
        auto const selected{workspace_.active_variant_id() == variant.id};
        auto const label{variant.id == LayoutWorkspace::baseline_variant_id
                             ? "Baseline"
                             : variant.name + " (" +
                                   std::to_string(detail::override_count(variant.overrides)) +
                                   " overrides)"};
        if (ImGui::Selectable(label.c_str(), selected)) {
            workspace_.select_variant(variant.id);
            sync_variant_name();
        }
    }
    ImGui::Separator();
    if (ImGui::Button("New experiment")) {
        create_variant_for_selected_schema();
    }
    ImGui::SameLine();
    if (ImGui::Button("Duplicate")) {
        auto const name{workspace_.active_variant().name + " copy"};
        workspace_.duplicate_variant(workspace_.active_variant_id(), name);
        sync_variant_name();
    }

    auto const baseline{workspace_.active_variant_id() == LayoutWorkspace::baseline_variant_id};
    ImGui::BeginDisabled(baseline);
    ImGui::SameLine();
    if (ImGui::Button("Reset all overrides")) {
        workspace_.reset_variant(workspace_.active_variant_id());
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete")) {
        workspace_.delete_variant(workspace_.active_variant_id());
        sync_variant_name();
    }
    if (variant_name_id_ != workspace_.active_variant_id()) {
        sync_variant_name();
    }
    if (ImGui::InputText("Name",
                         variant_name_.data(),
                         variant_name_.size(),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        workspace_.rename_variant(workspace_.active_variant_id(), variant_name_.data());
        sync_variant_name();
    }
    ImGui::EndDisabled();
    ImGui::End();
}

} // namespace ioj::layout_planner
