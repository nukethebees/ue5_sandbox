#include "planner_ui.hpp"

#include "planner_ui_support.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <string_view>

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

template <std::size_t Size>
void set_buffer(std::array<char, Size>& buffer, std::optional<std::string> const& value) {
    std::snprintf(buffer.data(), buffer.size(), "%s", value.value_or("").c_str());
}

template <std::size_t Size>
auto optional_text(std::array<char, Size> const& buffer) -> std::optional<std::string> {
    return buffer.front() == '\0' ? std::nullopt : std::optional<std::string>{buffer.data()};
}

auto parse_unsigned(std::string_view text) -> std::optional<std::uint64_t> {
    auto base{10};
    if (text.starts_with("0x") || text.starts_with("0X")) {
        base = 16;
        text.remove_prefix(2);
    }
    if (text.empty()) {
        return std::nullopt;
    }
    std::uint64_t value{};
    auto const [end, error]{std::from_chars(text.data(), text.data() + text.size(), value, base)};
    return error == std::errc{} && end == text.data() + text.size() ? std::optional{value}
                                                                    : std::nullopt;
}

auto unique_field_name(std::vector<codegen::PackedFieldSchema> const& fields,
                       std::string const& stem) -> std::string {
    auto suffix{std::size_t{1}};
    auto candidate{stem};
    while (std::ranges::find(fields, candidate, &codegen::PackedFieldSchema::name) !=
           fields.end()) {
        candidate = stem + std::to_string(suffix++);
    }
    return candidate;
}

template <typename Value>
void move_element(std::vector<Value>& values,
                  std::size_t const source_index,
                  std::size_t const target_index) {
    if (source_index < target_index) {
        std::rotate(values.begin() + static_cast<std::ptrdiff_t>(source_index),
                    values.begin() + static_cast<std::ptrdiff_t>(source_index + 1),
                    values.begin() + static_cast<std::ptrdiff_t>(target_index + 1));
    } else if (source_index > target_index) {
        std::rotate(values.begin() + static_cast<std::ptrdiff_t>(target_index),
                    values.begin() + static_cast<std::ptrdiff_t>(source_index),
                    values.begin() + static_cast<std::ptrdiff_t>(source_index + 1));
    }
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
        auto const revision_before_edit{workspace_.revision()};
        draw_enum_editor(node, *enumeration);
        if (workspace_.revision() != revision_before_edit) {
            ImGui::End();
            return;
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
        if (auto const* packed{std::get_if<PackedType>(&node.definition)}) {
            ImGui::SeparatorText("LispB packed declaration");
            if (draw_packed_editor(node, *packed)) {
                ImGui::End();
                return;
            }
        }

        auto const editable{workspace_.active_variant_id() != LayoutWorkspace::baseline_variant_id};
        if (!editable) {
            ImGui::SeparatorText("Baseline");
            if (std::holds_alternative<PackedType>(node.definition)) {
                ImGui::TextDisabled("Planning overrides are read only on the baseline.");
                ImGui::TextWrapped("Edit the LispB declaration above, or create an experiment for "
                                   "session-only physical overrides.");
            } else {
                ImGui::TextDisabled("Loaded from LispB — read only.");
                ImGui::TextWrapped("Experiments are session-only variants. The production schema "
                                   "is never modified.");
            }
            ImGui::TextDisabled("Use + Add variant in the Layout panel to create an experiment.");
            if (std::holds_alternative<SoaType>(node.definition)) {
                ImGui::TextDisabled("Capacity uses the planner default of %llu.",
                                    static_cast<unsigned long long>(workspace_.default_capacity()));
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

void PlannerUi::draw_enum_editor(TypeNode const& node, EnumType const&) {
    if (!document_.has_value()) {
        return;
    }
    auto const declaration{document_->find_declaration(node.identity)};
    if (!declaration.has_value()) {
        return;
    }
    auto const* schema{document_->enum_schema(*declaration)};
    if (schema == nullptr) {
        return;
    }

    if (selected_enumerator_.empty() && !schema->values.empty()) {
        selected_enumerator_ = schema->values.front().name;
    }
    auto selected{
        std::ranges::find(schema->values, selected_enumerator_, &codegen::EnumeratorSchema::name)};
    if (selected == schema->values.end() && !schema->values.empty()) {
        selected = schema->values.begin();
        selected_enumerator_ = selected->name;
    }
    auto const selected_index{selected == schema->values.end()
                                  ? std::optional<std::size_t>{}
                                  : std::optional<std::size_t>{static_cast<std::size_t>(
                                        selected - schema->values.begin())}};
    if (enum_editor_declaration_ != declaration || enum_editor_value_ != selected_enumerator_) {
        enum_editor_declaration_ = declaration;
        enum_editor_value_ = selected_enumerator_;
        if (selected != schema->values.end()) {
            std::snprintf(
                enum_value_name_.data(), enum_value_name_.size(), "%s", selected->name.c_str());
            set_buffer(enum_value_initializer_, selected->initializer);
            set_buffer(enum_value_display_name_, selected->display_name);
            set_buffer(enum_value_serialized_name_, selected->serialized_name);
        }
    }

    if (ImGui::Button("+ Enumerator")) {
        auto replacement{*schema};
        auto suffix{replacement.values.size()};
        std::string name;
        do {
            name = "Value" + std::to_string(suffix++);
        } while (std::ranges::find(replacement.values, name, &codegen::EnumeratorSchema::name) !=
                 replacement.values.end());
        auto insertion_index{replacement.values.size()};
        if (replacement.count.has_value()) {
            auto const count{std::ranges::find(
                replacement.values, *replacement.count, &codegen::EnumeratorSchema::name)};
            if (count != replacement.values.end()) {
                insertion_index = static_cast<std::size_t>(count - replacement.values.begin());
            }
        }
        replacement.values.insert(replacement.values.begin() +
                                      static_cast<std::ptrdiff_t>(insertion_index),
                                  {.name = name,
                                   .initializer = std::nullopt,
                                   .display_name = std::nullopt,
                                   .hidden = false,
                                   .serialized_name = std::nullopt});
        if (apply_document_edit(
                ReplaceEnum{.declaration = *declaration, .schema = std::move(replacement)})) {
            selected_enumerator_ = std::move(name);
            return;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value());
    if (ImGui::Button("Duplicate")) {
        auto replacement{*schema};
        auto copy{replacement.values[*selected_index]};
        auto suffix{std::size_t{1}};
        auto const stem{copy.name + "_copy"};
        copy.name = stem;
        while (std::ranges::find(replacement.values, copy.name, &codegen::EnumeratorSchema::name) !=
               replacement.values.end()) {
            copy.name = stem + std::to_string(suffix++);
        }
        auto const insertion_index{schema->count == replacement.values[*selected_index].name
                                       ? *selected_index
                                       : *selected_index + 1};
        replacement.values.insert(
            replacement.values.begin() + static_cast<std::ptrdiff_t>(insertion_index), copy);
        if (apply_document_edit(
                ReplaceEnum{.declaration = *declaration, .schema = std::move(replacement)})) {
            selected_enumerator_ = std::move(copy.name);
            return;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() || *selected_index == 0);
    if (ImGui::Button("Move up")) {
        auto replacement{*schema};
        std::swap(replacement.values[*selected_index], replacement.values[*selected_index - 1]);
        if (apply_document_edit(
                ReplaceEnum{.declaration = *declaration, .schema = std::move(replacement)})) {
            selected_enumerator_ = enum_editor_value_;
            return;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() ||
                         *selected_index + 1 >= schema->values.size());
    if (ImGui::Button("Move down")) {
        auto replacement{*schema};
        std::swap(replacement.values[*selected_index], replacement.values[*selected_index + 1]);
        if (apply_document_edit(
                ReplaceEnum{.declaration = *declaration, .schema = std::move(replacement)})) {
            selected_enumerator_ = enum_editor_value_;
            return;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() || schema->values.size() == 1);
    if (ImGui::Button("Delete")) {
        auto replacement{*schema};
        auto const deleted_name{replacement.values[*selected_index].name};
        replacement.values.erase(replacement.values.begin() +
                                 static_cast<std::ptrdiff_t>(*selected_index));
        if (replacement.count == deleted_name) {
            replacement.count.reset();
        }
        auto const next_index{std::min(*selected_index, replacement.values.size() - 1)};
        auto const next_name{replacement.values[next_index].name};
        if (apply_document_edit(
                ReplaceEnum{.declaration = *declaration, .schema = std::move(replacement)})) {
            selected_enumerator_ = next_name;
            return;
        }
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();

    std::optional<codegen::EnumSchema> pending;
    auto selected_after_edit{selected_enumerator_};
    if (ImGui::BeginTable("enumerators",
                          7,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Edit", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Value");
        ImGui::TableSetupColumn("Display");
        ImGui::TableSetupColumn("Serialized");
        ImGui::TableSetupColumn("Hidden", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();
        for (std::size_t index{}; index < schema->values.size(); ++index) {
            auto const& value{schema->values[index]};
            auto const row_selected{selected_enumerator_ == value.name};
            ImGui::PushID(static_cast<int>(index));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (ImGui::Selectable("::", row_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                selected_enumerator_ = value.name;
                enum_editor_declaration_.reset();
            }
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("ENUMERATOR_ROW", &index, sizeof(index));
                ImGui::Text("Move %s", value.name.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget()) {
                if (auto const* payload{ImGui::AcceptDragDropPayload("ENUMERATOR_ROW")}) {
                    auto const source_index{*static_cast<std::size_t const*>(payload->Data)};
                    if (source_index < schema->values.size() && source_index != index) {
                        pending = *schema;
                        selected_after_edit = pending->values[source_index].name;
                        move_element(pending->values, source_index, index);
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::TableNextColumn();
            if (row_selected) {
                ImGui::SetNextItemWidth(-1.0F);
                auto const submitted{ImGui::InputText("##name",
                                                      enum_value_name_.data(),
                                                      enum_value_name_.size(),
                                                      ImGuiInputTextFlags_EnterReturnsTrue)};
                if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                    pending = *schema;
                    auto& edited{pending->values[index]};
                    auto const previous_name{edited.name};
                    edited.name = enum_value_name_.data();
                    if (pending->count == previous_name) {
                        pending->count = edited.name;
                    }
                    selected_after_edit = edited.name;
                }
            } else {
                ImGui::TextUnformatted(value.name.c_str());
            }

            auto draw_optional_editor = [&](char const* label,
                                            auto& buffer,
                                            std::optional<std::string> codegen::EnumeratorSchema::*
                                                member) {
                ImGui::TableNextColumn();
                if (row_selected) {
                    ImGui::SetNextItemWidth(-1.0F);
                    auto const submitted{ImGui::InputText(
                        label, buffer.data(), buffer.size(), ImGuiInputTextFlags_EnterReturnsTrue)};
                    if (!pending.has_value() &&
                        (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                        pending = *schema;
                        pending->values[index].*member = optional_text(buffer);
                    }
                } else {
                    draw_optional_text(value.*member);
                }
            };
            draw_optional_editor(
                "##value", enum_value_initializer_, &codegen::EnumeratorSchema::initializer);
            draw_optional_editor(
                "##display", enum_value_display_name_, &codegen::EnumeratorSchema::display_name);
            draw_optional_editor("##serialized",
                                 enum_value_serialized_name_,
                                 &codegen::EnumeratorSchema::serialized_name);

            ImGui::TableNextColumn();
            auto hidden{value.hidden};
            if (row_selected) {
                if (ImGui::Checkbox("##hidden", &hidden)) {
                    pending = *schema;
                    pending->values[index].hidden = hidden;
                }
            } else {
                ImGui::TextUnformatted(hidden ? "yes" : "-");
            }

            ImGui::TableNextColumn();
            auto count_sentinel{schema->count == value.name};
            if (row_selected) {
                if (ImGui::Checkbox("##count", &count_sentinel)) {
                    pending = *schema;
                    if (count_sentinel) {
                        pending->count = value.name;
                    } else if (pending->count == value.name) {
                        pending->count.reset();
                    }
                }
            } else {
                ImGui::TextUnformatted(count_sentinel ? "yes" : "-");
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (pending.has_value()) {
        if (pending->values[*selected_index].name.empty()) {
            schema_edit_message_ = "Enumerator name cannot be empty.";
            return;
        }
        if (apply_document_edit(
                ReplaceEnum{.declaration = *declaration, .schema = std::move(*pending)})) {
            selected_enumerator_ = std::move(selected_after_edit);
        }
    }
}

auto PlannerUi::draw_packed_editor(TypeNode const& node, PackedType const& packed) -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const declaration{document_->find_declaration(node.identity)};
    if (!declaration.has_value()) {
        return false;
    }
    auto const* schema{document_->packed_value_schema(*declaration)};
    if (schema == nullptr) {
        return false;
    }

    if (selected_field_.empty() && !schema->fields.empty()) {
        selected_field_ = schema->fields.front().name;
    }
    auto selected{
        std::ranges::find(schema->fields, selected_field_, &codegen::PackedFieldSchema::name)};
    if (selected == schema->fields.end() && !schema->fields.empty()) {
        selected = schema->fields.begin();
        selected_field_ = selected->name;
    }
    auto const selected_index{selected == schema->fields.end()
                                  ? std::optional<std::size_t>{}
                                  : std::optional<std::size_t>{static_cast<std::size_t>(
                                        selected - schema->fields.begin())}};

    if (packed_editor_declaration_ != declaration || packed_editor_field_ != selected_field_) {
        packed_editor_declaration_ = declaration;
        packed_editor_field_ = selected_field_;
        std::snprintf(packed_storage_type_.data(),
                      packed_storage_type_.size(),
                      "%s",
                      schema->storage_type.name.c_str());
        std::snprintf(packed_invalid_value_.data(),
                      packed_invalid_value_.size(),
                      "%s",
                      schema->invalid_value.has_value()
                          ? std::to_string(*schema->invalid_value).c_str()
                          : "");
        if (selected != schema->fields.end()) {
            std::snprintf(
                packed_field_name_.data(), packed_field_name_.size(), "%s", selected->name.c_str());
            std::snprintf(packed_field_type_.data(),
                          packed_field_type_.size(),
                          "%s",
                          selected->type.name.c_str());
            packed_field_bits_ = selected->bits;
        }
    }

    ImGui::SetNextItemWidth(-1.0F);
    auto const storage_submitted{ImGui::InputText("Storage type",
                                                  packed_storage_type_.data(),
                                                  packed_storage_type_.size(),
                                                  ImGuiInputTextFlags_EnterReturnsTrue)};
    if (storage_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
        if (packed_storage_type_.front() == '\0') {
            schema_edit_message_ = "Packed storage type cannot be empty.";
        } else {
            auto replacement{*schema};
            replacement.storage_type.name = packed_storage_type_.data();
            if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                       .schema = std::move(replacement)})) {
                selected_field_ = packed_editor_field_;
                return true;
            }
        }
    }

    ImGui::SetNextItemWidth(-1.0F);
    auto const invalid_submitted{ImGui::InputText("Invalid raw value",
                                                  packed_invalid_value_.data(),
                                                  packed_invalid_value_.size(),
                                                  ImGuiInputTextFlags_EnterReturnsTrue)};
    if (invalid_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
        auto replacement{*schema};
        if (packed_invalid_value_.front() == '\0') {
            replacement.invalid_value.reset();
        } else if (auto const value{parse_unsigned(packed_invalid_value_.data())}) {
            replacement.invalid_value = *value;
        } else {
            schema_edit_message_ = "Invalid raw value must be an unsigned decimal or 0x value.";
            return false;
        }
        if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
            selected_field_ = packed_editor_field_;
            return true;
        }
    }
    ImGui::TextDisabled("Leave invalid raw value empty when every storage code is valid.");

    if (ImGui::Button("+ Field")) {
        auto replacement{*schema};
        auto name{unique_field_name(replacement.fields, "field")};
        replacement.fields.push_back(codegen::PackedFieldSchema{
            name,
            codegen::TypeRef{.name = "std::uint8_t", .suffix = {}, .nested = std::nullopt},
            1});
        if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
            selected_field_ = std::move(name);
            return true;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value());
    if (ImGui::Button("Duplicate")) {
        auto replacement{*schema};
        auto copy{replacement.fields[*selected_index]};
        copy.name = unique_field_name(replacement.fields, copy.name + "_copy");
        replacement.fields.insert(
            replacement.fields.begin() + static_cast<std::ptrdiff_t>(*selected_index + 1), copy);
        if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
            selected_field_ = std::move(copy.name);
            return true;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() || *selected_index == 0);
    if (ImGui::Button("Move up")) {
        auto replacement{*schema};
        std::swap(replacement.fields[*selected_index], replacement.fields[*selected_index - 1]);
        if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
            selected_field_ = packed_editor_field_;
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() ||
                         *selected_index + 1 >= schema->fields.size());
    if (ImGui::Button("Move down")) {
        auto replacement{*schema};
        std::swap(replacement.fields[*selected_index], replacement.fields[*selected_index + 1]);
        if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
            selected_field_ = packed_editor_field_;
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() || schema->fields.size() == 1);
    if (ImGui::Button("Delete")) {
        auto replacement{*schema};
        replacement.fields.erase(replacement.fields.begin() +
                                 static_cast<std::ptrdiff_t>(*selected_index));
        auto const next_index{std::min(*selected_index, replacement.fields.size() - 1)};
        auto const next_name{replacement.fields[next_index].name};
        if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
            selected_field_ = next_name;
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();

    std::optional<codegen::PackedValueSchema> pending;
    auto selected_after_edit{selected_field_};
    if (ImGui::BeginTable("packed-schema-fields",
                          7,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Edit", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Semantic type");
        ImGui::TableSetupColumn("Bits", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Kind");
        ImGui::TableSetupColumn("Range", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Representable");
        ImGui::TableHeadersRow();
        for (std::size_t index{}; index < schema->fields.size(); ++index) {
            auto const& field{schema->fields[index]};
            auto const row_selected{selected_field_ == field.name};
            ImGui::PushID(static_cast<int>(index));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (ImGui::Selectable("::", row_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                selected_field_ = field.name;
                packed_editor_declaration_.reset();
            }
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("PACKED_FIELD_ROW", &index, sizeof(index));
                ImGui::Text("Move %s", field.name.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget()) {
                if (auto const* payload{ImGui::AcceptDragDropPayload("PACKED_FIELD_ROW")}) {
                    auto const source_index{*static_cast<std::size_t const*>(payload->Data)};
                    if (source_index < schema->fields.size() && source_index != index) {
                        pending = *schema;
                        selected_after_edit = pending->fields[source_index].name;
                        move_element(pending->fields, source_index, index);
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::TableNextColumn();
            if (row_selected) {
                ImGui::SetNextItemWidth(-1.0F);
                auto const submitted{ImGui::InputText("##name",
                                                      packed_field_name_.data(),
                                                      packed_field_name_.size(),
                                                      ImGuiInputTextFlags_EnterReturnsTrue)};
                if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                    pending = *schema;
                    pending->fields[index].name = packed_field_name_.data();
                    selected_after_edit = pending->fields[index].name;
                }
            } else {
                ImGui::TextUnformatted(field.name.c_str());
            }

            ImGui::TableNextColumn();
            if (row_selected) {
                ImGui::SetNextItemWidth(-1.0F);
                auto const submitted{ImGui::InputText("##type",
                                                      packed_field_type_.data(),
                                                      packed_field_type_.size(),
                                                      ImGuiInputTextFlags_EnterReturnsTrue)};
                if (!pending.has_value() && (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                    pending = *schema;
                    pending->fields[index].type.name = packed_field_type_.data();
                }
            } else {
                ImGui::TextUnformatted(field.type.name.c_str());
            }

            ImGui::TableNextColumn();
            if (row_selected) {
                ImGui::SetNextItemWidth(72.0F);
                auto const submitted{ImGui::InputInt(
                    "##bits", &packed_field_bits_, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue)};
                if (!pending.has_value() && (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                    pending = *schema;
                    pending->fields[index].bits = packed_field_bits_;
                }
            } else {
                ImGui::Text("%d", field.bits);
            }

            ImGui::TableNextColumn();
            auto const kind_label{field.kind == codegen::PackedFieldKind::enumeration ? "enum"
                                                                                      : "unsigned"};
            if (row_selected && ImGui::BeginCombo("##kind", kind_label)) {
                if (ImGui::Selectable("unsigned",
                                      field.kind == codegen::PackedFieldKind::unsigned_integer)) {
                    pending = *schema;
                    pending->fields[index].kind = codegen::PackedFieldKind::unsigned_integer;
                }
                if (ImGui::Selectable("enum",
                                      field.kind == codegen::PackedFieldKind::enumeration)) {
                    pending = *schema;
                    pending->fields[index].kind = codegen::PackedFieldKind::enumeration;
                }
                ImGui::EndCombo();
            } else if (!row_selected) {
                ImGui::TextUnformatted(kind_label);
            }

            ImGui::TableNextColumn();
            auto range_helper{field.range_helper};
            if (row_selected) {
                if (ImGui::Checkbox("##range-helper", &range_helper)) {
                    pending = *schema;
                    pending->fields[index].range_helper = range_helper;
                }
            } else {
                ImGui::TextUnformatted(range_helper ? "yes" : "-");
            }

            ImGui::TableNextColumn();
            if (index < packed.fields.size()) {
                auto const maximum{
                    packed.fields[index].bit_width >= 64
                        ? std::optional<std::uint64_t>{std::numeric_limits<std::uint64_t>::max()}
                        : std::optional<std::uint64_t>{
                              (std::uint64_t{1} << packed.fields[index].bit_width) - 1}};
                ImGui::Text("0..%s", detail::format_number(maximum).c_str());
            } else {
                ImGui::TextDisabled("Unknown");
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (pending.has_value()) {
        if (pending->fields[*selected_index].name.empty()) {
            schema_edit_message_ = "Packed field name cannot be empty.";
            return false;
        }
        if (pending->fields[*selected_index].type.name.empty()) {
            schema_edit_message_ = "Packed field semantic type cannot be empty.";
            return false;
        }
        if (apply_document_edit(
                ReplacePackedValue{.declaration = *declaration, .schema = std::move(*pending)})) {
            selected_field_ = std::move(selected_after_edit);
            return true;
        }
    }
    return false;
}

void PlannerUi::draw_variants_panel() {
    ImGui::Begin("Variants");
    auto const baseline_before_actions{workspace_.active_variant_id() ==
                                       LayoutWorkspace::baseline_variant_id};
    if (ImGui::BeginTable("variant-actions",
                          2,
                          ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings)) {
        ImGui::TableNextColumn();
        if (ImGui::Button("New experiment", {-1.0F, 0.0F})) {
            create_variant_for_selected_schema();
        }
        ImGui::TableNextColumn();
        if (ImGui::Button("Duplicate", {-1.0F, 0.0F})) {
            auto const name{workspace_.active_variant().name + " copy"};
            workspace_.duplicate_variant(workspace_.active_variant_id(), name);
            sync_variant_name();
        }
        ImGui::TableNextColumn();
        ImGui::BeginDisabled(baseline_before_actions);
        if (ImGui::Button("Reset all overrides", {-1.0F, 0.0F})) {
            workspace_.reset_variant(workspace_.active_variant_id());
        }
        ImGui::TableNextColumn();
        if (ImGui::Button("Delete", {-1.0F, 0.0F})) {
            workspace_.delete_variant(workspace_.active_variant_id());
            sync_variant_name();
            packed_dragged_divider_.reset();
            packed_dragged_variant_id_.reset();
        }
        ImGui::EndDisabled();
        ImGui::EndTable();
    }

    auto const baseline{workspace_.active_variant_id() == LayoutWorkspace::baseline_variant_id};
    ImGui::BeginDisabled(baseline);
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

    ImGui::Separator();
    if (ImGui::BeginChild("variant-list", {0.0F, 0.0F}, false)) {
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
                packed_dragged_variant_id_.reset();
            }
        }
    }
    ImGui::EndChild();
    ImGui::End();
}

} // namespace ioj::layout_planner
