#include "planner_ui_properties_common.hpp"

namespace ioj::layout_planner {

auto PlannerUi::draw_union_editor(TypeNode const& node, UnionType const& union_type) -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const declaration{document_->find_declaration(node.identity)};
    if (!declaration.has_value()) {
        return false;
    }
    auto const* schema{document_->union_schema(*declaration)};
    if (schema == nullptr) {
        return false;
    }

    if (analysis_session_.inputs.selection.field.empty() && !schema->alternatives.empty()) {
        analysis_session_.inputs.selection.field = schema->alternatives.front().name;
    }
    auto selected{std::ranges::find(schema->alternatives,
                                    analysis_session_.inputs.selection.field,
                                    &codegen::UnionAlternativeSchema::name)};
    if (selected == schema->alternatives.end() && !schema->alternatives.empty()) {
        selected = schema->alternatives.begin();
        analysis_session_.inputs.selection.field = selected->name;
    }
    auto const selected_index{selected == schema->alternatives.end()
                                  ? std::optional<std::size_t>{}
                                  : std::optional<std::size_t>{static_cast<std::size_t>(
                                        selected - schema->alternatives.begin())}};

    if (union_editor_declaration_ != declaration ||
        union_editor_alternative_ != analysis_session_.inputs.selection.field) {
        union_editor_declaration_ = declaration;
        union_editor_alternative_ = analysis_session_.inputs.selection.field;
        std::snprintf(union_export_specifier_.data(),
                      union_export_specifier_.size(),
                      "%s",
                      schema->export_specifier.value_or("").c_str());
        if (selected != schema->alternatives.end()) {
            std::snprintf(union_alternative_name_.data(),
                          union_alternative_name_.size(),
                          "%s",
                          selected->name.c_str());
            std::snprintf(union_alternative_type_.data(),
                          union_alternative_type_.size(),
                          "%s",
                          selected->type.name.c_str());
            union_alternative_is_array_ = selected->count.has_value();
            union_alternative_count_ = selected->count.value_or(1);
        }
    }

    detail::prepare_property_input("Export specifier (optional)");
    auto const export_submitted{ImGui::InputText("##Export specifier (optional)##union",
                                                 union_export_specifier_.data(),
                                                 union_export_specifier_.size(),
                                                 ImGuiInputTextFlags_EnterReturnsTrue)};
    if (export_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
        auto const export_specifier{optional_text(union_export_specifier_)};
        if (schema->export_specifier != export_specifier) {
            auto replacement{*schema};
            replacement.export_specifier = export_specifier;
            if (apply_document_edit(
                    ReplaceUnion{.declaration = *declaration, .schema = std::move(replacement)})) {
                analysis_session_.inputs.selection.field = union_editor_alternative_;
                return true;
            }
        }
    }

    if (ImGui::Button("+ Alternative")) {
        auto replacement{*schema};
        auto name{unique_union_alternative_name(replacement.alternatives, "alternative")};
        replacement.alternatives.push_back(
            {.name = name,
             .type =
                 codegen::TypeRef{.name = "std::uint32_t", .suffix = {}, .nested = std::nullopt},
             .count = std::nullopt});
        if (apply_document_edit(
                ReplaceUnion{.declaration = *declaration, .schema = std::move(replacement)})) {
            analysis_session_.inputs.selection.field = std::move(name);
            return true;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value());
    if (ImGui::Button("Duplicate")) {
        auto replacement{*schema};
        auto copy{replacement.alternatives[*selected_index]};
        copy.name = unique_union_alternative_name(replacement.alternatives, copy.name + "_copy");
        replacement.alternatives.insert(replacement.alternatives.begin() +
                                            static_cast<std::ptrdiff_t>(*selected_index + 1),
                                        copy);
        if (apply_document_edit(
                ReplaceUnion{.declaration = *declaration, .schema = std::move(replacement)})) {
            analysis_session_.inputs.selection.field = std::move(copy.name);
            return true;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() || *selected_index == 0);
    if (ImGui::Button("Move up")) {
        auto replacement{*schema};
        std::swap(replacement.alternatives[*selected_index],
                  replacement.alternatives[*selected_index - 1]);
        if (apply_document_edit(
                ReplaceUnion{.declaration = *declaration, .schema = std::move(replacement)})) {
            analysis_session_.inputs.selection.field = union_editor_alternative_;
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() ||
                         *selected_index + 1 >= schema->alternatives.size());
    if (ImGui::Button("Move down")) {
        auto replacement{*schema};
        std::swap(replacement.alternatives[*selected_index],
                  replacement.alternatives[*selected_index + 1]);
        if (apply_document_edit(
                ReplaceUnion{.declaration = *declaration, .schema = std::move(replacement)})) {
            analysis_session_.inputs.selection.field = union_editor_alternative_;
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() || schema->alternatives.size() == 1);
    if (ImGui::Button("Delete")) {
        auto replacement{*schema};
        replacement.alternatives.erase(replacement.alternatives.begin() +
                                       static_cast<std::ptrdiff_t>(*selected_index));
        auto const next_index{std::min(*selected_index, replacement.alternatives.size() - 1)};
        auto const next_name{replacement.alternatives[next_index].name};
        if (apply_document_edit(
                ReplaceUnion{.declaration = *declaration, .schema = std::move(replacement)})) {
            analysis_session_.inputs.selection.field = next_name;
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();

    std::optional<codegen::UnionSchema> pending;
    std::optional<TypeId> navigate_to;
    auto selected_after_edit{analysis_session_.inputs.selection.field};
    if (detail::begin_editable_table("union-schema-alternatives", 5, schema->alternatives.size())) {
        detail::editable_table_column("Edit", ImGuiTableColumnFlags_WidthFixed);
        detail::editable_table_column("Name");
        detail::editable_table_column("Semantic type");
        detail::editable_table_column("Fixed array", ImGuiTableColumnFlags_WidthFixed);
        detail::editable_table_column("Count", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();
        for (std::size_t index{}; index < schema->alternatives.size(); ++index) {
            auto const& alternative{schema->alternatives[index]};
            auto const row_selected{analysis_session_.inputs.selection.field == alternative.name};
            ImGui::PushID(static_cast<int>(index));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (detail::editable_table_row_handle(row_selected)) {
                analysis_session_.inputs.selection.field = alternative.name;
                union_editor_declaration_.reset();
            }
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("UNION_ALTERNATIVE_ROW", &index, sizeof(index));
                ImGui::Text("Move %s", alternative.name.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget()) {
                if (auto const* payload{ImGui::AcceptDragDropPayload("UNION_ALTERNATIVE_ROW")}) {
                    auto const source_index{*static_cast<std::size_t const*>(payload->Data)};
                    if (source_index < schema->alternatives.size() && source_index != index) {
                        pending = *schema;
                        selected_after_edit = pending->alternatives[source_index].name;
                        move_element(pending->alternatives, source_index, index);
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::TableNextColumn();
            if (row_selected) {
                ImGui::SetNextItemWidth(-1.0F);
                auto const submitted{ImGui::InputText("##name",
                                                      union_alternative_name_.data(),
                                                      union_alternative_name_.size(),
                                                      ImGuiInputTextFlags_EnterReturnsTrue)};
                if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                    pending = *schema;
                    pending->alternatives[index].name = union_alternative_name_.data();
                    selected_after_edit = pending->alternatives[index].name;
                }
            } else {
                ImGui::TextUnformatted(alternative.name.c_str());
            }

            ImGui::TableNextColumn();
            if (row_selected) {
                auto const type_controls_inline{detail::prepare_editable_type_input()};
                auto const submitted{ImGui::InputText("##type",
                                                      union_alternative_type_.data(),
                                                      union_alternative_type_.size(),
                                                      ImGuiInputTextFlags_EnterReturnsTrue)};
                if (!pending.has_value() && (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                    pending = *schema;
                    pending->alternatives[index].type.name = union_alternative_type_.data();
                }
                if (type_controls_inline) {
                    ImGui::SameLine();
                }
                if (auto picked{draw_type_picker(node.identity.module_name, node.identity)}) {
                    pending = *schema;
                    pending->alternatives[index].type.name = std::move(*picked);
                }
                ImGui::SameLine();
                if (index < union_type.alternatives.size() &&
                    detail::semantic_type_navigation_button()) {
                    navigate_to = union_type.alternatives[index].semantic_type.type;
                }
                detail::editable_table_content_hint(union_alternative_type_.data(), 80.0F);
            } else {
                ImGui::TextUnformatted(alternative.type.name.c_str());
            }

            ImGui::TableNextColumn();
            if (row_selected) {
                if (ImGui::Checkbox("##fixed-array", &union_alternative_is_array_)) {
                    pending = *schema;
                    pending->alternatives[index].count = union_alternative_is_array_
                                                           ? std::optional{union_alternative_count_}
                                                           : std::nullopt;
                }
            } else {
                ImGui::TextUnformatted(alternative.count.has_value() ? "yes" : "-");
            }

            ImGui::TableNextColumn();
            if (row_selected && union_alternative_is_array_) {
                ImGui::SetNextItemWidth(96.0F);
                auto const submitted{ImGui::InputScalar("##count",
                                                        ImGuiDataType_U64,
                                                        &union_alternative_count_,
                                                        nullptr,
                                                        nullptr,
                                                        "%llu",
                                                        ImGuiInputTextFlags_EnterReturnsTrue)};
                if (!pending.has_value() && (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                    pending = *schema;
                    pending->alternatives[index].count = union_alternative_count_;
                }
            } else if (alternative.count.has_value()) {
                ImGui::Text("%llu", static_cast<unsigned long long>(*alternative.count));
            } else {
                ImGui::TextUnformatted("1");
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (navigate_to.has_value()) {
        select_type(*navigate_to);
        analysis_session_.inputs.selection.field.clear();
        return true;
    }

    if (pending.has_value()) {
        auto const invalid_alternative{
            std::ranges::find_if(pending->alternatives, [](auto const& alternative) {
                return alternative.name.empty() || alternative.type.name.empty() ||
                       alternative.count == 0;
            })};
        if (invalid_alternative != pending->alternatives.end()) {
            if (invalid_alternative->name.empty()) {
                schema_edit_message_ = "Union alternative name cannot be empty.";
            } else if (invalid_alternative->type.name.empty()) {
                schema_edit_message_ = "Union alternative type cannot be empty.";
            } else {
                schema_edit_message_ = "Fixed array count must be greater than zero.";
            }
            return false;
        }
        if (apply_document_edit(
                ReplaceUnion{.declaration = *declaration, .schema = std::move(*pending)})) {
            analysis_session_.inputs.selection.field = std::move(selected_after_edit);
            return true;
        }
    }
    return false;
}

auto PlannerUi::draw_tagged_union_editor(TypeNode const& node, TaggedUnionType const& tagged_union)
    -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const declaration{document_->find_declaration(node.identity)};
    if (!declaration.has_value()) {
        return false;
    }
    auto const* schema{document_->tagged_union_schema(*declaration)};
    if (schema == nullptr) {
        return false;
    }

    if (analysis_session_.inputs.selection.field.empty() && !schema->alternatives.empty()) {
        analysis_session_.inputs.selection.field = schema->alternatives.front().name;
    }
    auto selected{std::ranges::find(schema->alternatives,
                                    analysis_session_.inputs.selection.field,
                                    &codegen::TaggedUnionAlternativeSchema::name)};
    if (selected == schema->alternatives.end() && !schema->alternatives.empty()) {
        selected = schema->alternatives.begin();
        analysis_session_.inputs.selection.field = selected->name;
    }
    auto const selected_index{selected == schema->alternatives.end()
                                  ? std::optional<std::size_t>{}
                                  : std::optional<std::size_t>{static_cast<std::size_t>(
                                        selected - schema->alternatives.begin())}};
    if (tagged_union_editor_declaration_ != declaration ||
        tagged_union_editor_alternative_ != analysis_session_.inputs.selection.field) {
        tagged_union_editor_declaration_ = declaration;
        tagged_union_editor_alternative_ = analysis_session_.inputs.selection.field;
        std::snprintf(tagged_union_export_specifier_.data(),
                      tagged_union_export_specifier_.size(),
                      "%s",
                      schema->export_specifier.value_or("").c_str());
        if (selected != schema->alternatives.end()) {
            std::snprintf(tagged_union_alternative_name_.data(),
                          tagged_union_alternative_name_.size(),
                          "%s",
                          selected->name.c_str());
            std::snprintf(tagged_union_alternative_type_.data(),
                          tagged_union_alternative_type_.size(),
                          "%s",
                          selected->type.name.c_str());
            tagged_union_alternative_is_array_ = selected->count.has_value();
            tagged_union_alternative_count_ = selected->count.value_or(1);
        }
    }

    detail::prepare_property_input("Export specifier (optional)");
    auto const export_submitted{ImGui::InputText("##Export specifier (optional)##tagged-union",
                                                 tagged_union_export_specifier_.data(),
                                                 tagged_union_export_specifier_.size(),
                                                 ImGuiInputTextFlags_EnterReturnsTrue)};
    if (export_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
        auto const export_specifier{optional_text(tagged_union_export_specifier_)};
        if (schema->export_specifier != export_specifier) {
            auto replacement{*schema};
            replacement.export_specifier = export_specifier;
            if (apply_document_edit(ReplaceTaggedUnion{.declaration = *declaration,
                                                       .schema = std::move(replacement)})) {
                analysis_session_.inputs.selection.field = tagged_union_editor_alternative_;
                return true;
            }
        }
    }

    std::optional<codegen::TaggedUnionSchema> pending;
    auto selected_after_edit{analysis_session_.inputs.selection.field};
    std::optional<TypeId> navigate_to;
    auto const& discriminator_node{
        analysis_session_.inputs.workspace.types().type(tagged_union.discriminant.type)};
    auto const& discriminator{std::get<EnumType>(discriminator_node.definition)};
    auto tag_available = [&](std::string const& tag, std::optional<std::size_t> const except) {
        auto const value{std::ranges::find(discriminator.enumerators, tag, &Enumerator::name)};
        if (value == discriminator.enumerators.end() || value->sentinel || value->count_sentinel) {
            return false;
        }
        for (std::size_t index{}; index < schema->alternatives.size(); ++index) {
            if ((!except.has_value() || index != *except) &&
                schema->alternatives[index].tag == tag) {
                return false;
            }
        }
        return true;
    };
    auto first_available_tag = [&](std::optional<std::size_t> const except) -> std::string {
        auto const found{std::ranges::find_if(discriminator.enumerators, [&](auto const& value) {
            return tag_available(value.name, except);
        })};
        return found == discriminator.enumerators.end() ? std::string{} : found->name;
    };

    if (ImGui::BeginCombo("Discriminant", discriminator_node.cpp_spelling.c_str())) {
        for (auto const& candidate : analysis_session_.inputs.workspace.types().types()) {
            auto const* enumeration{std::get_if<EnumType>(&candidate.definition)};
            if (enumeration == nullptr) {
                continue;
            }
            auto const all_tags_exist{
                std::ranges::all_of(schema->alternatives, [&](auto const& alt) {
                    auto const value{
                        std::ranges::find(enumeration->enumerators, alt.tag, &Enumerator::name)};
                    return value != enumeration->enumerators.end() && !value->sentinel &&
                           !value->count_sentinel;
                })};
            if (!all_tags_exist) {
                continue;
            }
            auto const current{candidate.identity == discriminator_node.identity};
            if (ImGui::Selectable(candidate.cpp_spelling.c_str(), current) && !current) {
                pending = *schema;
                pending->discriminant.name = candidate.cpp_spelling;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Go to discriminant")) {
        select_type(tagged_union.discriminant.type);
        analysis_session_.inputs.selection.field.clear();
        return true;
    }

    if (ImGui::Button("+ Alternative")) {
        auto const tag{first_available_tag(std::nullopt)};
        if (tag.empty()) {
            schema_edit_message_ = "The discriminant has no unused non-sentinel tag.";
        } else {
            auto replacement{*schema};
            auto name{unique_union_alternative_name(replacement.alternatives, "alternative")};
            codegen::TaggedUnionAlternativeSchema alternative{};
            alternative.name = name;
            alternative.type.name = "std::uint32_t";
            alternative.tag = tag;
            replacement.alternatives.push_back(std::move(alternative));
            if (apply_document_edit(ReplaceTaggedUnion{.declaration = *declaration,
                                                       .schema = std::move(replacement)})) {
                analysis_session_.inputs.selection.field = std::move(name);
                return true;
            }
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value());
    if (ImGui::Button("Duplicate")) {
        auto const tag{first_available_tag(std::nullopt)};
        if (tag.empty()) {
            schema_edit_message_ = "The discriminant has no unused non-sentinel tag.";
        } else {
            auto replacement{*schema};
            auto copy{replacement.alternatives[*selected_index]};
            copy.name =
                unique_union_alternative_name(replacement.alternatives, copy.name + "_copy");
            copy.tag = tag;
            replacement.alternatives.insert(replacement.alternatives.begin() +
                                                static_cast<std::ptrdiff_t>(*selected_index + 1),
                                            copy);
            if (apply_document_edit(ReplaceTaggedUnion{.declaration = *declaration,
                                                       .schema = std::move(replacement)})) {
                analysis_session_.inputs.selection.field = std::move(copy.name);
                return true;
            }
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() || *selected_index == 0);
    if (ImGui::Button("Move up")) {
        auto replacement{*schema};
        std::swap(replacement.alternatives[*selected_index],
                  replacement.alternatives[*selected_index - 1]);
        if (apply_document_edit(ReplaceTaggedUnion{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
            analysis_session_.inputs.selection.field = tagged_union_editor_alternative_;
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() ||
                         *selected_index + 1 >= schema->alternatives.size());
    if (ImGui::Button("Move down")) {
        auto replacement{*schema};
        std::swap(replacement.alternatives[*selected_index],
                  replacement.alternatives[*selected_index + 1]);
        if (apply_document_edit(ReplaceTaggedUnion{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
            analysis_session_.inputs.selection.field = tagged_union_editor_alternative_;
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() || schema->alternatives.size() == 1);
    if (ImGui::Button("Delete")) {
        auto replacement{*schema};
        replacement.alternatives.erase(replacement.alternatives.begin() +
                                       static_cast<std::ptrdiff_t>(*selected_index));
        auto const next_index{std::min(*selected_index, replacement.alternatives.size() - 1)};
        auto const next_name{replacement.alternatives[next_index].name};
        if (apply_document_edit(ReplaceTaggedUnion{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
            analysis_session_.inputs.selection.field = next_name;
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();

    if (detail::begin_editable_table(
            "tagged-union-schema-alternatives", 6, schema->alternatives.size())) {
        detail::editable_table_column("Edit", ImGuiTableColumnFlags_WidthFixed);
        detail::editable_table_column("Tag");
        detail::editable_table_column("Name");
        detail::editable_table_column("Semantic type");
        detail::editable_table_column("Fixed array", ImGuiTableColumnFlags_WidthFixed);
        detail::editable_table_column("Count", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();
        for (std::size_t index{}; index < schema->alternatives.size(); ++index) {
            auto const& alternative{schema->alternatives[index]};
            auto const row_selected{analysis_session_.inputs.selection.field == alternative.name};
            ImGui::PushID(static_cast<int>(index));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (detail::editable_table_row_handle(row_selected)) {
                analysis_session_.inputs.selection.field = alternative.name;
                tagged_union_editor_declaration_.reset();
            }
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("TAGGED_UNION_ALTERNATIVE_ROW", &index, sizeof(index));
                ImGui::Text("Move %s", alternative.name.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget()) {
                if (auto const* payload{
                        ImGui::AcceptDragDropPayload("TAGGED_UNION_ALTERNATIVE_ROW")}) {
                    auto const source_index{*static_cast<std::size_t const*>(payload->Data)};
                    if (source_index < schema->alternatives.size() && source_index != index) {
                        pending = *schema;
                        selected_after_edit = pending->alternatives[source_index].name;
                        move_element(pending->alternatives, source_index, index);
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::TableNextColumn();
            if (row_selected) {
                if (ImGui::BeginCombo("##tag", alternative.tag.c_str())) {
                    for (auto const& value : discriminator.enumerators) {
                        if (!tag_available(value.name, index)) {
                            continue;
                        }
                        if (ImGui::Selectable(value.name.c_str(), value.name == alternative.tag)) {
                            pending = *schema;
                            pending->alternatives[index].tag = value.name;
                        }
                    }
                    ImGui::EndCombo();
                }
            } else {
                ImGui::TextUnformatted(alternative.tag.c_str());
            }

            ImGui::TableNextColumn();
            if (row_selected) {
                ImGui::SetNextItemWidth(-1.0F);
                auto const submitted{ImGui::InputText("##name",
                                                      tagged_union_alternative_name_.data(),
                                                      tagged_union_alternative_name_.size(),
                                                      ImGuiInputTextFlags_EnterReturnsTrue)};
                if (!pending.has_value() && (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                    pending = *schema;
                    pending->alternatives[index].name = tagged_union_alternative_name_.data();
                    selected_after_edit = pending->alternatives[index].name;
                }
            } else {
                ImGui::TextUnformatted(alternative.name.c_str());
            }

            ImGui::TableNextColumn();
            if (row_selected) {
                auto const type_controls_inline{detail::prepare_editable_type_input()};
                auto const submitted{ImGui::InputText("##type",
                                                      tagged_union_alternative_type_.data(),
                                                      tagged_union_alternative_type_.size(),
                                                      ImGuiInputTextFlags_EnterReturnsTrue)};
                if (!pending.has_value() && (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                    pending = *schema;
                    pending->alternatives[index].type.name = tagged_union_alternative_type_.data();
                }
                if (type_controls_inline) {
                    ImGui::SameLine();
                }
                if (auto picked{draw_type_picker(node.identity.module_name, node.identity)}) {
                    pending = *schema;
                    pending->alternatives[index].type.name = std::move(*picked);
                }
                ImGui::SameLine();
                if (index < tagged_union.alternatives.size() &&
                    detail::semantic_type_navigation_button()) {
                    navigate_to = tagged_union.alternatives[index].semantic_type.type;
                }
                detail::editable_table_content_hint(tagged_union_alternative_type_.data(), 80.0F);
            } else {
                ImGui::TextUnformatted(alternative.type.name.c_str());
            }

            ImGui::TableNextColumn();
            if (row_selected) {
                if (ImGui::Checkbox("##fixed-array", &tagged_union_alternative_is_array_)) {
                    pending = *schema;
                    pending->alternatives[index].count =
                        tagged_union_alternative_is_array_
                            ? std::optional{tagged_union_alternative_count_}
                            : std::nullopt;
                }
            } else {
                ImGui::TextUnformatted(alternative.count.has_value() ? "yes" : "-");
            }

            ImGui::TableNextColumn();
            if (row_selected && tagged_union_alternative_is_array_) {
                ImGui::SetNextItemWidth(96.0F);
                auto const submitted{ImGui::InputScalar("##count",
                                                        ImGuiDataType_U64,
                                                        &tagged_union_alternative_count_,
                                                        nullptr,
                                                        nullptr,
                                                        "%llu",
                                                        ImGuiInputTextFlags_EnterReturnsTrue)};
                if (!pending.has_value() && (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                    pending = *schema;
                    pending->alternatives[index].count = tagged_union_alternative_count_;
                }
            } else if (alternative.count.has_value()) {
                ImGui::Text("%llu", static_cast<unsigned long long>(*alternative.count));
            } else {
                ImGui::TextUnformatted("1");
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (navigate_to.has_value()) {
        select_type(*navigate_to);
        analysis_session_.inputs.selection.field.clear();
        return true;
    }
    if (pending.has_value()) {
        auto const invalid_alternative{
            std::ranges::find_if(pending->alternatives, [](auto const& alternative) {
                return alternative.name.empty() || alternative.type.name.empty() ||
                       alternative.tag.empty() || alternative.count == 0;
            })};
        if (invalid_alternative != pending->alternatives.end()) {
            schema_edit_message_ = "Tagged-union name, type, tag, and positive count are required.";
            return false;
        }
        if (apply_document_edit(
                ReplaceTaggedUnion{.declaration = *declaration, .schema = std::move(*pending)})) {
            analysis_session_.inputs.selection.field = std::move(selected_after_edit);
            return true;
        }
    }
    return false;
}

auto PlannerUi::draw_record_editor(TypeNode const& node, RecordType const& record) -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const declaration{document_->find_declaration(node.identity)};
    if (!declaration.has_value()) {
        return false;
    }
    auto const* schema{document_->record_schema(*declaration)};
    if (schema == nullptr) {
        return false;
    }

    if (analysis_session_.inputs.selection.field.empty() && !schema->members.empty()) {
        analysis_session_.inputs.selection.field = schema->members.front().name;
    }
    auto selected{std::ranges::find(schema->members,
                                    analysis_session_.inputs.selection.field,
                                    &codegen::RecordMemberSchema::name)};
    if (selected == schema->members.end() && !schema->members.empty()) {
        selected = schema->members.begin();
        analysis_session_.inputs.selection.field = selected->name;
    }
    auto const selected_index{selected == schema->members.end()
                                  ? std::optional<std::size_t>{}
                                  : std::optional<std::size_t>{static_cast<std::size_t>(
                                        selected - schema->members.begin())}};
    if (record_editor_declaration_ != declaration ||
        record_editor_member_ != analysis_session_.inputs.selection.field) {
        record_editor_declaration_ = declaration;
        record_editor_member_ = analysis_session_.inputs.selection.field;
        std::snprintf(record_export_specifier_.data(),
                      record_export_specifier_.size(),
                      "%s",
                      schema->export_specifier.value_or("").c_str());
        if (selected != schema->members.end()) {
            std::snprintf(record_member_name_.data(),
                          record_member_name_.size(),
                          "%s",
                          selected->name.c_str());
            std::snprintf(record_member_type_.data(),
                          record_member_type_.size(),
                          "%s",
                          selected->type.name.c_str());
            record_member_is_array_ = selected->count.has_value();
            record_member_count_ = selected->count.value_or(1);
            std::snprintf(record_relationship_target_.data(),
                          record_relationship_target_.size(),
                          "%s",
                          selected->relationship.has_value()
                              ? selected->relationship->target.name.c_str()
                              : "");
            auto const relationship_kind{
                selected->relationship.has_value()
                    ? std::ranges::find(semantic_relationship_kinds, selected->relationship->kind)
                    : semantic_relationship_kinds.end()};
            record_relationship_kind_ =
                relationship_kind == semantic_relationship_kinds.end()
                    ? 0
                    : static_cast<int>(relationship_kind - semantic_relationship_kinds.begin());
            auto const relationship_unit{
                selected->relationship.has_value() && selected->relationship->unit.has_value()
                    ? std::ranges::find(semantic_relationship_units, *selected->relationship->unit)
                    : semantic_relationship_units.end()};
            record_relationship_unit_ =
                relationship_unit == semantic_relationship_units.end()
                    ? 0
                    : static_cast<int>(relationship_unit - semantic_relationship_units.begin());
        }
    }

    detail::prepare_property_input("Export specifier (optional)");
    auto const export_submitted{ImGui::InputText("##Export specifier (optional)##record",
                                                 record_export_specifier_.data(),
                                                 record_export_specifier_.size(),
                                                 ImGuiInputTextFlags_EnterReturnsTrue)};
    if (export_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
        auto const export_specifier{optional_text(record_export_specifier_)};
        if (schema->export_specifier != export_specifier) {
            auto replacement{*schema};
            replacement.export_specifier = export_specifier;
            if (apply_document_edit(
                    ReplaceRecord{.declaration = *declaration, .schema = std::move(replacement)})) {
                analysis_session_.inputs.selection.field = record_editor_member_;
                return true;
            }
        }
    }

    if (ImGui::Button("+ Member")) {
        auto replacement{*schema};
        auto name{unique_record_member_name(replacement.members, "member")};
        replacement.members.push_back(codegen::RecordMemberSchema{
            .name = name,
            .type = codegen::TypeRef{.name = "std::uint32_t", .suffix = {}, .nested = std::nullopt},
            .count = std::nullopt,
            .relationship = std::nullopt});
        if (apply_document_edit(
                ReplaceRecord{.declaration = *declaration, .schema = std::move(replacement)})) {
            analysis_session_.inputs.selection.field = std::move(name);
            return true;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value());
    if (ImGui::Button("Duplicate")) {
        auto replacement{*schema};
        auto copy{replacement.members[*selected_index]};
        copy.name = unique_record_member_name(replacement.members, copy.name + "_copy");
        replacement.members.insert(
            replacement.members.begin() + static_cast<std::ptrdiff_t>(*selected_index + 1), copy);
        if (apply_document_edit(
                ReplaceRecord{.declaration = *declaration, .schema = std::move(replacement)})) {
            analysis_session_.inputs.selection.field = std::move(copy.name);
            return true;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() || *selected_index == 0);
    if (ImGui::Button("Move up")) {
        auto replacement{*schema};
        std::swap(replacement.members[*selected_index], replacement.members[*selected_index - 1]);
        if (apply_document_edit(
                ReplaceRecord{.declaration = *declaration, .schema = std::move(replacement)})) {
            analysis_session_.inputs.selection.field = record_editor_member_;
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() ||
                         *selected_index + 1 >= schema->members.size());
    if (ImGui::Button("Move down")) {
        auto replacement{*schema};
        std::swap(replacement.members[*selected_index], replacement.members[*selected_index + 1]);
        if (apply_document_edit(
                ReplaceRecord{.declaration = *declaration, .schema = std::move(replacement)})) {
            analysis_session_.inputs.selection.field = record_editor_member_;
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value());
    if (ImGui::Button("Delete")) {
        auto replacement{*schema};
        auto const deleted_name{replacement.members[*selected_index].name};
        replacement.members.erase(replacement.members.begin() +
                                  static_cast<std::ptrdiff_t>(*selected_index));
        auto const next_name{
            replacement.members.empty()
                ? std::string{}
                : replacement.members[std::min(*selected_index, replacement.members.size() - 1)]
                      .name};
        if (apply_document_edit(
                ReplaceRecord{.declaration = *declaration, .schema = std::move(replacement)})) {
            analysis_session_.inputs.selection.record_access_members.erase(deleted_name);
            analysis_session_.inputs.selection.field = next_name;
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Selected only")) {
        analysis_session_.inputs.selection.record_access_members.clear();
        analysis_session_.inputs.selection.record_access_set_explicit = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Access all")) {
        analysis_session_.inputs.selection.record_access_members.clear();
        for (auto const& member : schema->members) {
            analysis_session_.inputs.selection.record_access_members.insert_or_assign(
                member.name, analysis_session_.inputs.access_operation);
        }
        analysis_session_.inputs.selection.record_access_set_explicit = true;
    }

    std::optional<codegen::RecordSchema> pending;
    std::optional<TypeId> navigate_to;
    std::optional<std::pair<std::string, std::string>> renamed_member;
    auto selected_after_edit{analysis_session_.inputs.selection.field};
    if (detail::begin_editable_table("record-schema-members", 7, schema->members.size())) {
        detail::editable_table_column("Edit", ImGuiTableColumnFlags_WidthFixed);
        detail::editable_table_column("Access", ImGuiTableColumnFlags_WidthFixed);
        detail::editable_table_column("Operation", ImGuiTableColumnFlags_WidthFixed);
        detail::editable_table_column("Name");
        detail::editable_table_column("Semantic type");
        detail::editable_table_column("Fixed array", ImGuiTableColumnFlags_WidthFixed);
        detail::editable_table_column("Count", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();
        for (std::size_t index{}; index < schema->members.size(); ++index) {
            auto const& member{schema->members[index]};
            auto const row_selected{analysis_session_.inputs.selection.field == member.name};
            ImGui::PushID(static_cast<int>(index));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (detail::editable_table_row_handle(row_selected)) {
                analysis_session_.inputs.selection.field = member.name;
                record_editor_declaration_.reset();
            }
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("RECORD_MEMBER_ROW", &index, sizeof(index));
                ImGui::Text("Move %s", member.name.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget()) {
                if (auto const* payload{ImGui::AcceptDragDropPayload("RECORD_MEMBER_ROW")}) {
                    auto const source_index{*static_cast<std::size_t const*>(payload->Data)};
                    if (source_index < schema->members.size() && source_index != index) {
                        pending = *schema;
                        selected_after_edit = pending->members[source_index].name;
                        move_element(pending->members, source_index, index);
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::TableNextColumn();
            auto accessed{
                analysis_session_.inputs.selection.record_access_set_explicit
                    ? analysis_session_.inputs.selection.record_access_members.contains(member.name)
                    : analysis_session_.inputs.selection.field == member.name};
            if (ImGui::Checkbox("##access", &accessed)) {
                if (!analysis_session_.inputs.selection.record_access_set_explicit) {
                    analysis_session_.inputs.selection.record_access_members.clear();
                    if (!analysis_session_.inputs.selection.field.empty()) {
                        analysis_session_.inputs.selection.record_access_members.insert_or_assign(
                            analysis_session_.inputs.selection.field,
                            analysis_session_.inputs.access_operation);
                    }
                    analysis_session_.inputs.selection.record_access_set_explicit = true;
                }
                if (accessed) {
                    analysis_session_.inputs.selection.record_access_members.insert_or_assign(
                        member.name, analysis_session_.inputs.access_operation);
                } else {
                    analysis_session_.inputs.selection.record_access_members.erase(member.name);
                }
            }

            ImGui::TableNextColumn();
            if (accessed) {
                auto operation{analysis_session_.inputs.access_operation};
                if (analysis_session_.inputs.selection.record_access_set_explicit) {
                    if (auto const found{
                            analysis_session_.inputs.selection.record_access_members.find(
                                member.name)};
                        found != analysis_session_.inputs.selection.record_access_members.end()) {
                        operation = found->second;
                    }
                }
                auto operation_index{static_cast<int>(operation)};
                ImGui::SetNextItemWidth(105.0F);
                if (ImGui::Combo(
                        "##access-operation", &operation_index, "Read\0Write\0Read + write\0")) {
                    if (!analysis_session_.inputs.selection.record_access_set_explicit) {
                        analysis_session_.inputs.selection.record_access_members.clear();
                        if (!analysis_session_.inputs.selection.field.empty()) {
                            analysis_session_.inputs.selection.record_access_members
                                .insert_or_assign(analysis_session_.inputs.selection.field,
                                                  analysis_session_.inputs.access_operation);
                        }
                        analysis_session_.inputs.selection.record_access_set_explicit = true;
                    }
                    analysis_session_.inputs.selection.record_access_members.insert_or_assign(
                        member.name, static_cast<AccessOperation>(operation_index));
                }
            } else {
                ImGui::TextDisabled("-");
            }

            ImGui::TableNextColumn();
            if (row_selected) {
                ImGui::SetNextItemWidth(-1.0F);
                auto const submitted{ImGui::InputText("##name",
                                                      record_member_name_.data(),
                                                      record_member_name_.size(),
                                                      ImGuiInputTextFlags_EnterReturnsTrue)};
                if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                    pending = *schema;
                    pending->members[index].name = record_member_name_.data();
                    selected_after_edit = pending->members[index].name;
                    renamed_member = std::pair{member.name, selected_after_edit};
                }
            } else {
                ImGui::TextUnformatted(member.name.c_str());
            }

            ImGui::TableNextColumn();
            if (row_selected) {
                auto const type_controls_inline{detail::prepare_editable_type_input()};
                auto const submitted{ImGui::InputText("##type",
                                                      record_member_type_.data(),
                                                      record_member_type_.size(),
                                                      ImGuiInputTextFlags_EnterReturnsTrue)};
                if (!pending.has_value() && (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                    pending = *schema;
                    pending->members[index].type.name = record_member_type_.data();
                }
                if (type_controls_inline) {
                    ImGui::SameLine();
                }
                if (auto picked{draw_type_picker(node.identity.module_name, node.identity)}) {
                    pending = *schema;
                    pending->members[index].type.name = std::move(*picked);
                }
                ImGui::SameLine();
                if (index < record.members.size() && detail::semantic_type_navigation_button()) {
                    navigate_to = record.members[index].semantic_type.type;
                }
                detail::editable_table_content_hint(record_member_type_.data(), 80.0F);
            } else {
                ImGui::TextUnformatted(member.type.name.c_str());
            }

            ImGui::TableNextColumn();
            if (row_selected) {
                if (ImGui::Checkbox("##fixed-array", &record_member_is_array_)) {
                    pending = *schema;
                    pending->members[index].count = record_member_is_array_
                                                      ? std::optional{record_member_count_}
                                                      : std::nullopt;
                }
            } else {
                ImGui::TextUnformatted(member.count.has_value() ? "yes" : "-");
            }

            ImGui::TableNextColumn();
            if (row_selected && record_member_is_array_) {
                ImGui::SetNextItemWidth(96.0F);
                auto const submitted{ImGui::InputScalar("##count",
                                                        ImGuiDataType_U64,
                                                        &record_member_count_,
                                                        nullptr,
                                                        nullptr,
                                                        "%llu",
                                                        ImGuiInputTextFlags_EnterReturnsTrue)};
                if (!pending.has_value() && (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                    pending = *schema;
                    pending->members[index].count = record_member_count_;
                }
            } else if (member.count.has_value()) {
                ImGui::Text("%llu", static_cast<unsigned long long>(*member.count));
            } else {
                ImGui::TextUnformatted("1");
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (!pending.has_value() && selected_index.has_value()) {
        auto const member_name{schema->members[*selected_index].name};
        auto const& member{schema->members[*selected_index]};
        auto const* resolved_member{
            *selected_index < record.members.size() ? &record.members[*selected_index] : nullptr};
        auto const current_kind{semantic_relationship_kinds[static_cast<std::size_t>(
            std::clamp(record_relationship_kind_,
                       0,
                       static_cast<int>(semantic_relationship_kinds.size() - 1)))]};
        auto const current_unit{semantic_relationship_units[static_cast<std::size_t>(
            std::clamp(record_relationship_unit_,
                       0,
                       static_cast<int>(semantic_relationship_units.size() - 1)))]};
        separator_text_with_tooltip(
            "Selected member relationship",
            "Describe how this record member relates to another semantic type.");
        ImGui::SetNextItemWidth(180.0F);
        if (ImGui::BeginCombo("Kind", codegen::semantic_relation_kind_name(current_kind).data())) {
            for (std::size_t kind_index{}; kind_index < semantic_relationship_kinds.size();
                 ++kind_index) {
                auto const kind{semantic_relationship_kinds[kind_index]};
                auto const chosen{record_relationship_kind_ == static_cast<int>(kind_index)};
                if (ImGui::Selectable(codegen::semantic_relation_kind_name(kind).data(), chosen)) {
                    record_relationship_kind_ = static_cast<int>(kind_index);
                    if (member.relationship.has_value()) {
                        auto replacement{*schema};
                        replacement.members[*selected_index].relationship->kind = kind;
                        replacement.members[*selected_index].relationship->unit =
                            kind == codegen::SemanticRelationKind::offset_into
                                ? std::optional{current_unit}
                                : std::nullopt;
                        if (apply_document_edit(ReplaceRecord{.declaration = *declaration,
                                                              .schema = std::move(replacement)})) {
                            record_editor_declaration_.reset();
                            analysis_session_.inputs.selection.field = member_name;
                            ImGui::EndCombo();
                            return true;
                        }
                    }
                }
            }
            ImGui::EndCombo();
        }
        if (current_kind == codegen::SemanticRelationKind::offset_into) {
            ImGui::SetNextItemWidth(180.0F);
            if (ImGui::BeginCombo("Unit",
                                  codegen::semantic_relation_unit_name(current_unit).data())) {
                for (std::size_t unit_index{}; unit_index < semantic_relationship_units.size();
                     ++unit_index) {
                    auto const unit{semantic_relationship_units[unit_index]};
                    auto const chosen{record_relationship_unit_ == static_cast<int>(unit_index)};
                    if (ImGui::Selectable(codegen::semantic_relation_unit_name(unit).data(),
                                          chosen)) {
                        record_relationship_unit_ = static_cast<int>(unit_index);
                        if (member.relationship.has_value()) {
                            auto replacement{*schema};
                            replacement.members[*selected_index].relationship->unit = unit;
                            if (apply_document_edit(
                                    ReplaceRecord{.declaration = *declaration,
                                                  .schema = std::move(replacement)})) {
                                record_editor_declaration_.reset();
                                analysis_session_.inputs.selection.field = member_name;
                                ImGui::EndCombo();
                                return true;
                            }
                        }
                    }
                }
                ImGui::EndCombo();
            }
        }

        ImGui::SetNextItemWidth(std::max(80.0F, ImGui::GetContentRegionAvail().x - 132.0F));
        auto const target_submitted{ImGui::InputText("Target",
                                                     record_relationship_target_.data(),
                                                     record_relationship_target_.size(),
                                                     ImGuiInputTextFlags_EnterReturnsTrue)};
        if (member.relationship.has_value() &&
            (target_submitted || ImGui::IsItemDeactivatedAfterEdit())) {
            if (record_relationship_target_.front() == '\0') {
                schema_edit_message_ = "Relationship target cannot be empty.";
            } else {
                auto replacement{*schema};
                replacement.members[*selected_index].relationship->target.name =
                    record_relationship_target_.data();
                if (apply_document_edit(ReplaceRecord{.declaration = *declaration,
                                                      .schema = std::move(replacement)})) {
                    record_editor_declaration_.reset();
                    analysis_session_.inputs.selection.field = member_name;
                    return true;
                }
            }
        }
        ImGui::SameLine();
        ImGui::PushID("record-relationship-target");
        auto picked_relationship_target{draw_type_picker(node.identity.module_name, node.identity)};
        ImGui::PopID();
        if (picked_relationship_target.has_value()) {
            auto replacement{*schema};
            replacement.members[*selected_index].relationship = codegen::SemanticRelationSchema{
                .kind = current_kind,
                .target = codegen::TypeRef{.name = *picked_relationship_target,
                                           .suffix = {},
                                           .nested = std::nullopt},
                .unit = current_kind == codegen::SemanticRelationKind::offset_into
                          ? std::optional{current_unit}
                          : std::nullopt};
            if (apply_document_edit(
                    ReplaceRecord{.declaration = *declaration, .schema = std::move(replacement)})) {
                record_editor_declaration_.reset();
                analysis_session_.inputs.selection.field = member_name;
                return true;
            }
        }
        ImGui::SameLine();
        if (member.relationship.has_value()) {
            if (ImGui::SmallButton("Clear")) {
                auto replacement{*schema};
                replacement.members[*selected_index].relationship.reset();
                if (apply_document_edit(ReplaceRecord{.declaration = *declaration,
                                                      .schema = std::move(replacement)})) {
                    record_editor_declaration_.reset();
                    analysis_session_.inputs.selection.field = member_name;
                    return true;
                }
            }
            ImGui::SameLine();
            if (resolved_member != nullptr && resolved_member->relationship.has_value() &&
                detail::semantic_type_navigation_button()) {
                navigate_to = resolved_member->relationship->target.type;
            }
        } else {
            ImGui::BeginDisabled(record_relationship_target_.front() == '\0');
            if (ImGui::SmallButton("Add")) {
                auto replacement{*schema};
                replacement.members[*selected_index].relationship = codegen::SemanticRelationSchema{
                    .kind = current_kind,
                    .target = codegen::TypeRef{.name = record_relationship_target_.data(),
                                               .suffix = {},
                                               .nested = std::nullopt},
                    .unit = current_kind == codegen::SemanticRelationKind::offset_into
                              ? std::optional{current_unit}
                              : std::nullopt};
                if (apply_document_edit(ReplaceRecord{.declaration = *declaration,
                                                      .schema = std::move(replacement)})) {
                    record_editor_declaration_.reset();
                    analysis_session_.inputs.selection.field = member_name;
                    return true;
                }
            }
            ImGui::EndDisabled();
        }
        ImGui::TextDisabled(
            "The relationship is durable semantic metadata; record offsets and ABI layout still "
            "come only from the member type, count, and target profile.");
    }

    if (navigate_to.has_value()) {
        select_type(*navigate_to);
        analysis_session_.inputs.selection.field.clear();
        analysis_session_.inputs.selection.record_access_members.clear();
        analysis_session_.inputs.selection.record_access_set_explicit = false;
        return true;
    }

    if (pending.has_value()) {
        auto const invalid_member{std::ranges::find_if(pending->members, [](auto const& member) {
            return member.name.empty() || member.type.name.empty() || member.count == 0;
        })};
        if (invalid_member != pending->members.end()) {
            if (invalid_member->name.empty()) {
                schema_edit_message_ = "Record member name cannot be empty.";
            } else if (invalid_member->type.name.empty()) {
                schema_edit_message_ = "Record member type cannot be empty.";
            } else {
                schema_edit_message_ = "Fixed array count must be greater than zero.";
            }
            return false;
        }
        if (apply_document_edit(
                ReplaceRecord{.declaration = *declaration, .schema = std::move(*pending)})) {
            if (analysis_session_.inputs.selection.record_access_set_explicit &&
                renamed_member.has_value()) {
                auto const existing{analysis_session_.inputs.selection.record_access_members.find(
                    renamed_member->first)};
                if (existing != analysis_session_.inputs.selection.record_access_members.end()) {
                    auto const operation{existing->second};
                    analysis_session_.inputs.selection.record_access_members.erase(existing);
                    analysis_session_.inputs.selection.record_access_members.insert_or_assign(
                        renamed_member->second, operation);
                }
            }
            analysis_session_.inputs.selection.field = std::move(selected_after_edit);
            return true;
        }
    }
    return false;
}

} // namespace ioj::layout_planner
