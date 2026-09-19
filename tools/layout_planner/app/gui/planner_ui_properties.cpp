#include "planner_ui.hpp"

#include "planner_ui_support.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdint>
#include <string>

namespace ioj::layout_planner {
namespace {

using namespace layout;
using namespace lispb::schema;

void draw_override_note(bool const overridden) {
    if (overridden) {
        ImGui::SameLine();
        ImGui::TextColored({0.4F, 0.75F, 0.95F, 1.0F}, "Override");
    }
}

void draw_optional_text(std::optional<std::string> const& value) {
    ImGui::TextUnformatted(value.has_value() ? value->c_str() : "-");
}

} // namespace

void PlannerUi::draw_properties_panel() {
    ImGui::Begin("Properties");
    if (!selected_type_.has_value()) {
        ImGui::TextDisabled("No selection.");
        ImGui::End();
        return;
    }

    auto const selected{*selected_type_};
    auto const& node{workspace_.types().type(selected)};
    ImGui::Text("%s", node.identity.name.c_str());
    ImGui::TextDisabled("%s", node.identity.module_name.c_str());
    ImGui::TextDisabled("%s", node.cpp_spelling.c_str());

    if (auto const* enumeration{std::get_if<EnumType>(&node.definition)}) {
        ImGui::SeparatorText("Enum");
        auto const& underlying{workspace_.types().type(enumeration->underlying_type.type)};
        ImGui::TextUnformatted("Underlying type");
        ImGui::SameLine();
        if (ImGui::SmallButton(underlying.cpp_spelling.c_str())) {
            selected_type_ = enumeration->underlying_type.type;
            selected_field_.clear();
        }
        if (enumeration->count.has_value()) {
            ImGui::Text("Count sentinel: %s", enumeration->count->c_str());
        }

        if (ImGui::BeginTable("enumerators",
                              5,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Value");
            ImGui::TableSetupColumn("Display name");
            ImGui::TableSetupColumn("Serialized name");
            ImGui::TableSetupColumn("Flags");
            ImGui::TableHeadersRow();
            for (auto const& value : enumeration->enumerators) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(value.name.c_str());
                ImGui::TableNextColumn();
                draw_optional_text(value.explicit_value);
                ImGui::TableNextColumn();
                draw_optional_text(value.display_name);
                ImGui::TableNextColumn();
                draw_optional_text(value.serialized_name);
                ImGui::TableNextColumn();
                auto flags{std::string{}};
                if (value.hidden) {
                    flags = "hidden";
                }
                if (value.count_sentinel) {
                    flags += flags.empty() ? "count" : ", count";
                }
                ImGui::TextDisabled("%s", flags.empty() ? "-" : flags.c_str());
            }
            ImGui::EndTable();
        }
    } else if (auto const* external{std::get_if<ExternalType>(&node.definition)}) {
        ImGui::SeparatorText("External type");
        ImGui::Text("C++ spelling: %s", external->cpp_type.spelling.c_str());
        if (!external->registered_names.empty()) {
            ImGui::TextUnformatted("Registered as");
            for (auto const& name : external->registered_names) {
                ImGui::BulletText("@%s", name.c_str());
            }
        }
        ImGui::TextDisabled("Internal structure is not declared in LispB.");
    } else {
        auto editable{workspace_.active_variant_id() != LayoutWorkspace::baseline_variant_id};
        if (!editable) {
            ImGui::SeparatorText("Baseline");
            ImGui::TextDisabled("Loaded from LispB — read only.");
            ImGui::TextWrapped(
                "Experiments are session-only variants. The production schema is never modified.");
            if (std::holds_alternative<SoaType>(node.definition)) {
                ImGui::TextDisabled("Capacity uses the planner default of %llu.",
                                    static_cast<unsigned long long>(workspace_.default_capacity()));
            }
            if (ImGui::Button("Create editable variant", {-1.0F, 0.0F})) {
                create_variant_for_selected_schema();
                editable = true;
            }
        } else {
            ImGui::SeparatorText("Experiment");
            ImGui::Text("Editing %s", workspace_.active_variant().name.c_str());
        }

        ImGui::BeginDisabled(!editable);
        if (auto const* packed{std::get_if<PackedType>(&node.definition)}) {
            ImGui::SeparatorText("Packed storage");
            ImGui::Text("Schema storage: %s", active_packed_->schema_storage_type.c_str());
            auto const& analysis{*active_packed_};
            if (ImGui::BeginCombo("Planning storage", analysis.storage_type.c_str())) {
                if (ImGui::Selectable("Schema storage", !analysis.storage_overridden)) {
                    workspace_.set_packed_storage_type(selected, std::nullopt);
                }
                for (auto const& [type, facts] : abi_.types()) {
                    if (facts.unsigned_value_bits.has_value() &&
                        ImGui::Selectable(type.c_str(), analysis.storage_type == type)) {
                        workspace_.set_packed_storage_type(selected, type);
                    }
                }
                ImGui::EndCombo();
            }
            draw_override_note(analysis.storage_overridden);
            if (analysis.storage_overridden) {
                ImGui::SameLine();
                if (ImGui::SmallButton("Reset storage")) {
                    workspace_.set_packed_storage_type(selected, std::nullopt);
                }
            }

            if (selected_field_.empty() && !packed->fields.empty()) {
                selected_field_ = packed->fields.front().name;
            }
            ImGui::SeparatorText("Fields");
            if (ImGui::BeginTable("packed-fields",
                                  4,
                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_Resizable |
                                      ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("Field");
                ImGui::TableSetupColumn("Semantic type");
                ImGui::TableSetupColumn("Schema");
                ImGui::TableSetupColumn("Planning");
                ImGui::TableHeadersRow();
                for (auto const& field : analysis.fields) {
                    ImGui::PushID(field.name.c_str());
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    if (ImGui::Selectable(field.name.c_str(), selected_field_ == field.name)) {
                        selected_field_ = field.name;
                    }
                    ImGui::TableNextColumn();
                    auto const& semantic_type{workspace_.types().type(field.semantic_type)};
                    if (ImGui::SmallButton(semantic_type.cpp_spelling.c_str())) {
                        selected_type_ = field.semantic_type;
                        selected_field_.clear();
                    }
                    ImGui::TableNextColumn();
                    ImGui::Text("%u bits", field.schema_bit_width);
                    ImGui::TableNextColumn();
                    auto width{field.bit_width};
                    ImGui::SetNextItemWidth(-1.0F);
                    if (ImGui::InputScalar("##planning-width", ImGuiDataType_U32, &width)) {
                        width = std::clamp(width, std::uint32_t{1}, std::uint32_t{64});
                        workspace_.set_packed_field_width(selected, field.name, width);
                    }
                    if (field.overridden && ImGui::SmallButton("Reset")) {
                        workspace_.set_packed_field_width(selected, field.name, std::nullopt);
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        } else if (auto const* soa{std::get_if<SoaType>(&node.definition)}) {
            auto const& analysis{*active_soa_};
            ImGui::SeparatorText("Planner capacity");
            auto capacity{analysis.capacity};
            if (ImGui::InputScalar("Capacity", ImGuiDataType_U64, &capacity)) {
                workspace_.set_capacity(selected, capacity);
            }
            draw_override_note(analysis.capacity_overridden);
            if (analysis.capacity_overridden) {
                ImGui::SameLine();
                if (ImGui::SmallButton("Reset capacity")) {
                    workspace_.set_capacity(selected, std::nullopt);
                }
            }

            if (selected_field_.empty() && !soa->columns.empty()) {
                selected_field_ = soa->columns.front().name;
            }
            ImGui::SeparatorText("Columns");
            if (ImGui::BeginTable("soa-columns-properties",
                                  3,
                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_Resizable |
                                      ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("Column");
                ImGui::TableSetupColumn("Semantic type");
                ImGui::TableSetupColumn("Planning");
                ImGui::TableHeadersRow();
                for (auto const& column : analysis.columns) {
                    ImGui::PushID(column.name.c_str());
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    if (ImGui::Selectable(column.name.c_str(), selected_field_ == column.name)) {
                        selected_field_ = column.name;
                    }
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(column.schema_type.c_str());
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-1.0F);
                    if (ImGui::BeginCombo("##planning-type", column.physical_type.c_str())) {
                        if (ImGui::Selectable("Schema type", !column.overridden)) {
                            workspace_.set_soa_column_type(selected, column.name, std::nullopt);
                        }
                        for (auto const& [type, facts] : abi_.types()) {
                            static_cast<void>(facts);
                            if (ImGui::Selectable(type.c_str(), column.physical_type == type)) {
                                workspace_.set_soa_column_type(selected, column.name, type);
                            }
                        }
                        ImGui::EndCombo();
                    }
                    if (column.overridden && ImGui::SmallButton("Reset")) {
                        workspace_.set_soa_column_type(selected, column.name, std::nullopt);
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }
        ImGui::EndDisabled();
    }

    auto draw_links = [&](char const* heading, std::span<TypeId const> const links) {
        ImGui::SeparatorText(heading);
        if (links.empty()) {
            ImGui::TextDisabled("None");
        }
        for (auto const linked : links) {
            auto const& linked_node{workspace_.types().type(linked)};
            ImGui::PushID(static_cast<int>(linked.value));
            if (ImGui::SmallButton(linked_node.cpp_spelling.c_str())) {
                selected_type_ = linked;
                selected_field_.clear();
            }
            ImGui::PopID();
        }
    };
    draw_links("Depends on", workspace_.types().dependencies_of(selected));
    draw_links("Used by", workspace_.types().users_of(selected));
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
            packed_dragged_divider_.reset();
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
