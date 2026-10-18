#include "planner_ui_properties_common.hpp"

#include <codegen/schema/enumerator_schemas.h>
#include <codegen/schema/record_member_schemas.h>
#include <codegen/schema/record_schemas.h>
#include <codegen/schema/semantic_relation_kind.h>
#include <codegen/schema/semantic_relation_schemas.h>
#include <codegen/schema/tagged_union_alternative_schemas.h>
#include <codegen/schema/tagged_union_schemas.h>
#include <codegen/schema/type_refs.h>
#include <codegen/schema/union_alternative_schemas.h>
#include <codegen/schema/union_schemas.h>
#include <lispb/compiler_storage.h>
#include <lispb/schema/enum_domains.h>
#include <lispb/schema/record_members.h>
#include <lispb/schema/tagged_union_alternatives.h>
#include <lispb/schema/union_alternatives.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>

namespace ioj::layout_planner {

auto PlannerUi::draw_union_editor(TypeId const node, TypeDefinitionId const& union_type) -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const declaration{
        document_->find_declaration(analysis_session_.inputs.workspace.types().identity(node))};
    if (!declaration.has_value()) {
        return false;
    }
    auto const schema{
        document_->declaration_handle(*declaration, lispb::codegen::DeclarationKind::union_type)};
    if (!schema.has_value()) {
        return false;
    }

    if (analysis_session_.inputs.selection.field.empty() &&
        !declarations()
             .items(document_->storage()
                        .declarations.unions
                        .alternatives[document_->storage().declarations.unions.index(*schema)])
             .empty()) {
        analysis_session_.inputs.selection.field =
            text(declarations().union_alternatives.name[declarations().union_alternatives.index(
                declarations()
                    .items(document_->storage().declarations.unions.alternatives
                               [document_->storage().declarations.unions.index(*schema)])
                    .front())]);
    }
    auto selected{std::ranges::find(
        declarations().items(
            document_->storage()
                .declarations.unions
                .alternatives[document_->storage().declarations.unions.index(*schema)]),
        strings().intern(analysis_session_.inputs.selection.field),
        [&](auto id) {
            return declarations()
                .union_alternatives.name[declarations().union_alternatives.index(id)];
        })};
    if (selected ==
            declarations()
                .items(document_->storage()
                           .declarations.unions
                           .alternatives[document_->storage().declarations.unions.index(*schema)])
                .end() &&
        !declarations()
             .items(document_->storage()
                        .declarations.unions
                        .alternatives[document_->storage().declarations.unions.index(*schema)])
             .empty()) {
        selected =
            declarations()
                .items(document_->storage()
                           .declarations.unions
                           .alternatives[document_->storage().declarations.unions.index(*schema)])
                .begin();
        analysis_session_.inputs.selection.field =
            text(declarations()
                     .union_alternatives.name[declarations().union_alternatives.index(*selected)]);
    }
    auto const selected_index{
        selected == declarations()
                        .items(document_->storage().declarations.unions.alternatives
                                   [document_->storage().declarations.unions.index(*schema)])
                        .end()
            ? std::optional<std::size_t>{}
            : std::optional<std::size_t>{static_cast<std::size_t>(
                  selected -
                  declarations()
                      .items(document_->storage().declarations.unions.alternatives
                                 [document_->storage().declarations.unions.index(*schema)])
                      .begin())}};

    if (union_editor_declaration_ != declaration ||
        union_editor_alternative_ != analysis_session_.inputs.selection.field) {
        union_editor_declaration_ = declaration;
        union_editor_alternative_ = analysis_session_.inputs.selection.field;
        std::snprintf(
            union_export_specifier_.data(),
            union_export_specifier_.size(),
            "%s",
            text(document_->storage()
                     .declarations.unions
                     .export_specifier[document_->storage().declarations.unions.index(*schema)])
                .c_str());
        if (selected !=
            declarations()
                .items(document_->storage()
                           .declarations.unions
                           .alternatives[document_->storage().declarations.unions.index(*schema)])
                .end()) {
            std::snprintf(
                union_alternative_name_.data(),
                union_alternative_name_.size(),
                "%s",
                text(declarations().union_alternatives.name[declarations().union_alternatives.index(
                         *selected)])
                    .c_str());
            std::snprintf(union_alternative_type_.data(),
                          union_alternative_type_.size(),
                          "%s",
                          text(declarations().type_refs.name[declarations().type_refs.index(
                                   declarations().union_alternatives.type
                                       [declarations().union_alternatives.index(*selected)])])
                              .c_str());
            union_alternative_is_array_ =
                declarations()
                    .union_alternatives.count[declarations().union_alternatives.index(*selected)]
                    .has_value();
            union_alternative_count_ =
                declarations()
                    .union_alternatives.count[declarations().union_alternatives.index(*selected)]
                    .value_or(1);
        }
    }

    detail::prepare_property_input("Export specifier (optional)");
    auto const export_submitted{ImGui::InputText("##Export specifier (optional)##union",
                                                 union_export_specifier_.data(),
                                                 union_export_specifier_.size(),
                                                 ImGuiInputTextFlags_EnterReturnsTrue)};
    if (export_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
        auto const export_specifier{std::string_view{union_export_specifier_.data()}};
        if (text(document_->storage()
                     .declarations.unions
                     .export_specifier[document_->storage().declarations.unions.index(*schema)]) !=
            export_specifier) {
            auto replacement{document_->storage().declarations.unions.clone(*schema)};
            document_->storage()
                .declarations.unions
                .export_specifier[document_->storage().declarations.unions.index(replacement)] =
                strings().intern(export_specifier);
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
                analysis_session_.inputs.selection.field = union_editor_alternative_;
                return true;
            }
        }
    }

    if (ImGui::Button("+ Alternative")) {
        auto replacement{document_->storage().declarations.unions.clone(*schema)};
        auto name{unique_child_name(
            declarations(),
            declarations().union_alternatives,
            declarations().union_alternatives.name,
            declarations().items(
                document_->storage()
                    .declarations.unions
                    .alternatives[document_->storage().declarations.unions.index(replacement)]),
            "alternative")};
        document_->storage()
            .declarations.unions
            .alternatives[document_->storage().declarations.unions.index(replacement)] = [&] {
            auto const original_items{declarations().items(
                document_->storage()
                    .declarations.unions
                    .alternatives[document_->storage().declarations.unions.index(replacement)])};
            auto rows{std::vector(original_items.begin(), original_items.end())};
            rows.push_back([&] {
                auto const new_union_alternatives{declarations().union_alternatives.append()};
                declarations().union_alternatives.name[declarations().union_alternatives.index(
                    new_union_alternatives)] = strings().intern(name);
                declarations().union_alternatives.type[declarations().union_alternatives.index(
                    new_union_alternatives)] = [&] {
                    auto const new_type_refs{declarations().type_refs.append()};
                    declarations().type_refs.name[declarations().type_refs.index(new_type_refs)] =
                        strings().intern("std::uint32_t");
                    declarations()
                        .type_refs.suffix[declarations().type_refs.index(new_type_refs)] = {};
                    declarations().type_refs.nested[declarations().type_refs.index(new_type_refs)] =
                        std::nullopt;
                    return new_type_refs;
                }();
                declarations().union_alternatives.count[declarations().union_alternatives.index(
                    new_union_alternatives)] = std::nullopt;
                return new_union_alternatives;
            }());
            return declarations().range(rows);
        }();
        if (apply_document_edit(
                document_->replace_declaration(*declaration, std::move(replacement)))) {
            analysis_session_.inputs.selection.field = std::move(name);
            return true;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value());
    if (ImGui::Button("Duplicate")) {
        auto replacement{document_->storage().declarations.unions.clone(*schema)};
        auto copy{declarations().union_alternatives.clone(declarations().items(
            document_->storage()
                .declarations.unions.alternatives[document_->storage().declarations.unions.index(
                    replacement)])[*selected_index])};
        declarations().union_alternatives.name[declarations().union_alternatives.index(copy)] =
            strings().intern(unique_child_name(
                declarations(),
                declarations().union_alternatives,
                declarations().union_alternatives.name,
                declarations().items(
                    document_->storage()
                        .declarations.unions
                        .alternatives[document_->storage().declarations.unions.index(replacement)]),
                text(declarations()
                         .union_alternatives.name[declarations().union_alternatives.index(copy)]) +
                    "_copy"));
        document_->storage()
            .declarations.unions
            .alternatives[document_->storage().declarations.unions.index(replacement)] = [&] {
            auto const original_items{declarations().items(
                document_->storage()
                    .declarations.unions
                    .alternatives[document_->storage().declarations.unions.index(replacement)])};
            auto rows{std::vector(original_items.begin(), original_items.end())};
            rows.insert(rows.begin() + static_cast<std::ptrdiff_t>(*selected_index + 1), copy);
            return declarations().range(rows);
        }();
        if (apply_document_edit(
                document_->replace_declaration(*declaration, std::move(replacement)))) {
            analysis_session_.inputs.selection.field =
                text(declarations()
                         .union_alternatives.name[declarations().union_alternatives.index(copy)]);
            return true;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() || *selected_index == 0);
    if (ImGui::Button("Move up")) {
        auto replacement{document_->storage().declarations.unions.clone(*schema)};
        document_->storage()
            .declarations.unions
            .alternatives[document_->storage().declarations.unions.index(replacement)] = [&] {
            auto const original_items{declarations().items(
                document_->storage()
                    .declarations.unions
                    .alternatives[document_->storage().declarations.unions.index(replacement)])};
            auto rows{std::vector(original_items.begin(), original_items.end())};
            std::swap(rows[*selected_index], rows[*selected_index - 1]);
            return declarations().range(rows);
        }();
        if (apply_document_edit(
                document_->replace_declaration(*declaration, std::move(replacement)))) {
            analysis_session_.inputs.selection.field = union_editor_alternative_;
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(
        !selected_index.has_value() ||
        *selected_index + 1 >=
            declarations()
                .items(document_->storage()
                           .declarations.unions
                           .alternatives[document_->storage().declarations.unions.index(*schema)])
                .size());
    if (ImGui::Button("Move down")) {
        auto replacement{document_->storage().declarations.unions.clone(*schema)};
        document_->storage()
            .declarations.unions
            .alternatives[document_->storage().declarations.unions.index(replacement)] = [&] {
            auto const original_items{declarations().items(
                document_->storage()
                    .declarations.unions
                    .alternatives[document_->storage().declarations.unions.index(replacement)])};
            auto rows{std::vector(original_items.begin(), original_items.end())};
            std::swap(rows[*selected_index], rows[*selected_index + 1]);
            return declarations().range(rows);
        }();
        if (apply_document_edit(
                document_->replace_declaration(*declaration, std::move(replacement)))) {
            analysis_session_.inputs.selection.field = union_editor_alternative_;
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(
        !selected_index.has_value() ||
        declarations()
                .items(document_->storage()
                           .declarations.unions
                           .alternatives[document_->storage().declarations.unions.index(*schema)])
                .size() == 1);
    if (ImGui::Button("Delete")) {
        auto replacement{document_->storage().declarations.unions.clone(*schema)};
        document_->storage()
            .declarations.unions
            .alternatives[document_->storage().declarations.unions.index(replacement)] = [&] {
            auto const original_items{declarations().items(
                document_->storage()
                    .declarations.unions
                    .alternatives[document_->storage().declarations.unions.index(replacement)])};
            auto rows{std::vector(original_items.begin(), original_items.end())};
            rows.erase(rows.begin() + static_cast<std::ptrdiff_t>(*selected_index));
            return declarations().range(rows);
        }();
        auto const next_index{std::min(
            *selected_index,
            declarations()
                    .items(document_->storage().declarations.unions.alternatives
                               [document_->storage().declarations.unions.index(replacement)])
                    .size() -
                1)};
        auto const next_name{
            text(declarations().union_alternatives.name[declarations().union_alternatives.index(
                declarations().items(document_->storage().declarations.unions.alternatives
                                         [document_->storage().declarations.unions.index(
                                             replacement)])[next_index])])};
        if (apply_document_edit(
                document_->replace_declaration(*declaration, std::move(replacement)))) {
            analysis_session_.inputs.selection.field = next_name;
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();

    std::optional<lispb::codegen::DeclarationHandle> pending;
    std::optional<TypeId> navigate_to;
    auto selected_after_edit{analysis_session_.inputs.selection.field};
    if (detail::begin_editable_table(
            "union-schema-alternatives",
            5,
            declarations()
                .items(document_->storage()
                           .declarations.unions
                           .alternatives[document_->storage().declarations.unions.index(*schema)])
                .size())) {
        detail::editable_table_column("", ImGuiTableColumnFlags_WidthFixed);
        detail::editable_table_column("Name");
        detail::editable_table_column("Semantic type");
        detail::editable_table_column("Fixed array", ImGuiTableColumnFlags_WidthFixed);
        detail::editable_table_column("Count", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();
        for (std::size_t index{};
             index <
             declarations()
                 .items(document_->storage()
                            .declarations.unions
                            .alternatives[document_->storage().declarations.unions.index(*schema)])
                 .size();
             ++index) {
            auto const alternative{declarations().items(
                document_->storage()
                    .declarations.unions
                    .alternatives[document_->storage().declarations.unions.index(*schema)])[index]};
            auto const row_selected{
                analysis_session_.inputs.selection.field ==
                text(declarations().union_alternatives.name[declarations().union_alternatives.index(
                    alternative)])};
            ImGui::PushID(static_cast<int>(index));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (detail::editable_table_row_handle(row_selected)) {
                analysis_session_.inputs.selection.field = text(
                    declarations().union_alternatives.name[declarations().union_alternatives.index(
                        alternative)]);
                union_editor_declaration_.reset();
            }
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("UNION_ALTERNATIVE_ROW", &index, sizeof(index));
                ImGui::Text("Move %s",
                            text(declarations()
                                     .union_alternatives
                                     .name[declarations().union_alternatives.index(alternative)])
                                .c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget()) {
                if (auto const* payload{ImGui::AcceptDragDropPayload("UNION_ALTERNATIVE_ROW")}) {
                    auto const source_index{*static_cast<std::size_t const*>(payload->Data)};
                    if (source_index <
                            declarations()
                                .items(
                                    document_->storage().declarations.unions.alternatives
                                        [document_->storage().declarations.unions.index(*schema)])
                                .size() &&
                        source_index != index) {
                        pending = declarations().unions.clone(*schema);
                        selected_after_edit = text(
                            declarations()
                                .union_alternatives
                                .name[declarations().union_alternatives.index(declarations().items(
                                    declarations().unions.alternatives[declarations().unions.index(
                                        *pending)])[source_index])]);
                        declarations()
                            .unions.alternatives[declarations().unions.index(*pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .unions.alternatives[declarations().unions.index(*pending)])};
                            auto rows{std::vector(original_items.begin(), original_items.end())};
                            move_element(rows, source_index, index);
                            return declarations().range(rows);
                        }();
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
                    pending = declarations().unions.clone(*schema);
                    declarations().unions.alternatives[declarations().unions.index(*pending)] =
                        [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .unions.alternatives[declarations().unions.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().union_alternatives.clone(rows_1[index])};
                                declarations()
                                    .union_alternatives
                                    .name[declarations().union_alternatives.index(row_0)] =
                                    strings().intern(union_alternative_name_.data());
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                    selected_after_edit =
                        text(declarations()
                                 .union_alternatives
                                 .name[declarations().union_alternatives.index(declarations().items(
                                     declarations().unions.alternatives[declarations().unions.index(
                                         *pending)])[index])]);
                }
            } else {
                ImGui::TextUnformatted(
                    text(declarations()
                             .union_alternatives
                             .name[declarations().union_alternatives.index(alternative)])
                        .c_str());
            }

            ImGui::TableNextColumn();
            if (row_selected) {
                auto const type_controls_inline{detail::prepare_editable_type_input()};
                auto const submitted{ImGui::InputText("##type",
                                                      union_alternative_type_.data(),
                                                      union_alternative_type_.size(),
                                                      ImGuiInputTextFlags_EnterReturnsTrue)};
                if (!pending.has_value() && (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                    pending = declarations().unions.clone(*schema);
                    declarations().unions.alternatives[declarations().unions.index(*pending)] =
                        [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .unions.alternatives[declarations().unions.index(*pending)])};
                            auto rows_2{std::vector(original_items.begin(), original_items.end())};
                            rows_2[index] = [&] {
                                auto row_1{declarations().union_alternatives.clone(rows_2[index])};
                                declarations()
                                    .union_alternatives
                                    .type[declarations().union_alternatives.index(row_1)] = [&] {
                                    auto row_0{declarations().type_refs.clone(
                                        declarations()
                                            .union_alternatives
                                            .type[declarations().union_alternatives.index(row_1)])};
                                    declarations()
                                        .type_refs.name[declarations().type_refs.index(row_0)] =
                                        strings().intern(union_alternative_type_.data());
                                    return row_0;
                                }();
                                return row_1;
                            }();
                            return declarations().range(rows_2);
                        }();
                }
                if (type_controls_inline) {
                    ImGui::SameLine();
                }
                if (auto picked{draw_type_picker(
                        analysis_session_.inputs.workspace.types().module_name(node),
                        analysis_session_.inputs.workspace.types().identity(node))}) {
                    pending = declarations().unions.clone(*schema);
                    declarations().unions.alternatives[declarations().unions.index(*pending)] =
                        [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .unions.alternatives[declarations().unions.index(*pending)])};
                            auto rows_2{std::vector(original_items.begin(), original_items.end())};
                            rows_2[index] = [&] {
                                auto row_1{declarations().union_alternatives.clone(rows_2[index])};
                                declarations()
                                    .union_alternatives
                                    .type[declarations().union_alternatives.index(row_1)] = [&] {
                                    auto row_0{declarations().type_refs.clone(
                                        declarations()
                                            .union_alternatives
                                            .type[declarations().union_alternatives.index(row_1)])};
                                    declarations()
                                        .type_refs.name[declarations().type_refs.index(row_0)] =
                                        strings().intern(std::move(*picked));
                                    return row_0;
                                }();
                                return row_1;
                            }();
                            return declarations().range(rows_2);
                        }();
                }
                ImGui::SameLine();
                if (index < definitions().unions.alternatives_of(union_type).size() &&
                    detail::semantic_type_navigation_button()) {
                    navigate_to =
                        analysis_session_.inputs.workspace.types()
                            .occurrences()
                            .type[analysis_session_.inputs.workspace.types().occurrences().index(
                                definitions().union_alternatives.semantic_type
                                    [definitions().union_alternatives.index(
                                        definitions().unions.alternatives_of(union_type)[index])])];
                }
                detail::editable_table_content_hint(union_alternative_type_.data(), 80.0F);
            } else {
                ImGui::TextUnformatted(
                    text(declarations().type_refs.name[declarations().type_refs.index(
                             declarations().union_alternatives.type
                                 [declarations().union_alternatives.index(alternative)])])
                        .c_str());
            }

            ImGui::TableNextColumn();
            if (row_selected) {
                if (ImGui::Checkbox("##fixed-array", &union_alternative_is_array_)) {
                    pending = declarations().unions.clone(*schema);
                    declarations().unions.alternatives[declarations().unions.index(*pending)] =
                        [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .unions.alternatives[declarations().unions.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().union_alternatives.clone(rows_1[index])};
                                declarations()
                                    .union_alternatives
                                    .count[declarations().union_alternatives.index(row_0)] =
                                    union_alternative_is_array_
                                        ? std::optional{union_alternative_count_}
                                        : std::nullopt;
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                }
            } else {
                ImGui::TextUnformatted(
                    declarations()
                            .union_alternatives
                            .count[declarations().union_alternatives.index(alternative)]
                            .has_value()
                        ? "yes"
                        : "-");
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
                    pending = declarations().unions.clone(*schema);
                    declarations().unions.alternatives[declarations().unions.index(*pending)] =
                        [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .unions.alternatives[declarations().unions.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().union_alternatives.clone(rows_1[index])};
                                declarations()
                                    .union_alternatives
                                    .count[declarations().union_alternatives.index(row_0)] =
                                    union_alternative_count_;
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                }
            } else if (declarations()
                           .union_alternatives
                           .count[declarations().union_alternatives.index(alternative)]
                           .has_value()) {
                ImGui::Text("%llu",
                            static_cast<unsigned long long>(
                                *declarations()
                                     .union_alternatives
                                     .count[declarations().union_alternatives.index(alternative)]));
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
        auto const invalid_alternative{std::ranges::find_if(
            declarations().items(
                declarations().unions.alternatives[declarations().unions.index(*pending)]),
            [&](auto const id) {
                auto const alternative{id};
                return text(declarations()
                                .union_alternatives
                                .name[declarations().union_alternatives.index(alternative)])
                           .empty() ||
                       text(declarations().type_refs.name[declarations().type_refs.index(
                                declarations().union_alternatives.type
                                    [declarations().union_alternatives.index(alternative)])])
                           .empty() ||
                       declarations()
                               .union_alternatives
                               .count[declarations().union_alternatives.index(alternative)] == 0;
            })};
        if (invalid_alternative !=
            declarations()
                .items(declarations().unions.alternatives[declarations().unions.index(*pending)])
                .end()) {
            if (text(declarations().union_alternatives.name[declarations().union_alternatives.index(
                         *invalid_alternative)])
                    .empty()) {
                schema_edit_message_ = "Union alternative name cannot be empty.";
            } else if (text(
                           declarations().type_refs.name[declarations().type_refs.index(
                               declarations()
                                   .union_alternatives.type[declarations().union_alternatives.index(
                                       *invalid_alternative)])])
                           .empty()) {
                schema_edit_message_ = "Union alternative type cannot be empty.";
            } else {
                schema_edit_message_ = "Fixed array count must be greater than zero.";
            }
            return false;
        }
        if (apply_document_edit(
                document_->replace_declaration(*declaration, std::move(*pending)))) {
            analysis_session_.inputs.selection.field = std::move(selected_after_edit);
            return true;
        }
    }
    return false;
}

auto PlannerUi::draw_tagged_union_editor(TypeId const node, TypeDefinitionId const& tagged_union)
    -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const declaration{
        document_->find_declaration(analysis_session_.inputs.workspace.types().identity(node))};
    if (!declaration.has_value()) {
        return false;
    }
    auto const schema{
        document_->declaration_handle(*declaration, lispb::codegen::DeclarationKind::tagged_union)};
    if (!schema.has_value()) {
        return false;
    }

    if (analysis_session_.inputs.selection.field.empty() &&
        !declarations()
             .items(
                 document_->storage()
                     .declarations.tagged_unions
                     .alternatives[document_->storage().declarations.tagged_unions.index(*schema)])
             .empty()) {
        analysis_session_.inputs.selection.field = text(
            declarations()
                .tagged_union_alternatives.name[declarations().tagged_union_alternatives.index(
                    declarations()
                        .items(document_->storage().declarations.tagged_unions.alternatives
                                   [document_->storage().declarations.tagged_unions.index(*schema)])
                        .front())]);
    }
    auto selected{std::ranges::find(
        declarations().items(
            document_->storage()
                .declarations.tagged_unions
                .alternatives[document_->storage().declarations.tagged_unions.index(*schema)]),
        strings().intern(analysis_session_.inputs.selection.field),
        [&](auto id) {
            return declarations()
                .tagged_union_alternatives.name[declarations().tagged_union_alternatives.index(id)];
        })};
    if (selected == declarations()
                        .items(document_->storage().declarations.tagged_unions.alternatives
                                   [document_->storage().declarations.tagged_unions.index(*schema)])
                        .end() &&
        !declarations()
             .items(
                 document_->storage()
                     .declarations.tagged_unions
                     .alternatives[document_->storage().declarations.tagged_unions.index(*schema)])
             .empty()) {
        selected = declarations()
                       .items(document_->storage().declarations.tagged_unions.alternatives
                                  [document_->storage().declarations.tagged_unions.index(*schema)])
                       .begin();
        analysis_session_.inputs.selection.field =
            text(declarations()
                     .tagged_union_alternatives
                     .name[declarations().tagged_union_alternatives.index(*selected)]);
    }
    auto const selected_index{
        selected == declarations()
                        .items(document_->storage().declarations.tagged_unions.alternatives
                                   [document_->storage().declarations.tagged_unions.index(*schema)])
                        .end()
            ? std::optional<std::size_t>{}
            : std::optional<std::size_t>{static_cast<std::size_t>(
                  selected -
                  declarations()
                      .items(document_->storage().declarations.tagged_unions.alternatives
                                 [document_->storage().declarations.tagged_unions.index(*schema)])
                      .begin())}};
    if (tagged_union_editor_declaration_ != declaration ||
        tagged_union_editor_alternative_ != analysis_session_.inputs.selection.field) {
        tagged_union_editor_declaration_ = declaration;
        tagged_union_editor_alternative_ = analysis_session_.inputs.selection.field;
        std::snprintf(tagged_union_export_specifier_.data(),
                      tagged_union_export_specifier_.size(),
                      "%s",
                      text(document_->storage().declarations.tagged_unions.export_specifier
                               [document_->storage().declarations.tagged_unions.index(*schema)])
                          .c_str());
        if (selected !=
            declarations()
                .items(document_->storage().declarations.tagged_unions.alternatives
                           [document_->storage().declarations.tagged_unions.index(*schema)])
                .end()) {
            std::snprintf(tagged_union_alternative_name_.data(),
                          tagged_union_alternative_name_.size(),
                          "%s",
                          text(declarations()
                                   .tagged_union_alternatives
                                   .name[declarations().tagged_union_alternatives.index(*selected)])
                              .c_str());
            std::snprintf(
                tagged_union_alternative_type_.data(),
                tagged_union_alternative_type_.size(),
                "%s",
                text(declarations().type_refs.name[declarations().type_refs.index(
                         declarations().tagged_union_alternatives.type
                             [declarations().tagged_union_alternatives.index(*selected)])])
                    .c_str());
            tagged_union_alternative_is_array_ =
                declarations()
                    .tagged_union_alternatives
                    .count[declarations().tagged_union_alternatives.index(*selected)]
                    .has_value();
            tagged_union_alternative_count_ =
                declarations()
                    .tagged_union_alternatives
                    .count[declarations().tagged_union_alternatives.index(*selected)]
                    .value_or(1);
        }
    }

    detail::prepare_property_input("Export specifier (optional)");
    auto const export_submitted{ImGui::InputText("##Export specifier (optional)##tagged-union",
                                                 tagged_union_export_specifier_.data(),
                                                 tagged_union_export_specifier_.size(),
                                                 ImGuiInputTextFlags_EnterReturnsTrue)};
    if (export_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
        auto const export_specifier{std::string_view{tagged_union_export_specifier_.data()}};
        if (text(document_->storage().declarations.tagged_unions.export_specifier
                     [document_->storage().declarations.tagged_unions.index(*schema)]) !=
            export_specifier) {
            auto replacement{document_->storage().declarations.tagged_unions.clone(*schema)};
            document_->storage().declarations.tagged_unions.export_specifier
                [document_->storage().declarations.tagged_unions.index(replacement)] =
                strings().intern(export_specifier);
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
                analysis_session_.inputs.selection.field = tagged_union_editor_alternative_;
                return true;
            }
        }
    }

    std::optional<lispb::codegen::DeclarationHandle> pending;
    auto selected_after_edit{analysis_session_.inputs.selection.field};
    std::optional<TypeId> navigate_to;
    auto const& discriminator_node{
        analysis_session_.inputs.workspace.types()
            .occurrences()
            .type[analysis_session_.inputs.workspace.types().occurrences().index(
                definitions()
                    .tagged_unions.discriminant[definitions().tagged_unions.index(tagged_union)])]};
    auto const& discriminator{
        analysis_session_.inputs.workspace.types().definition(discriminator_node)};
    auto tag_available = [&](std::string const& tag, std::optional<std::size_t> const except) {
        auto const value{std::ranges::find(
            analysis_session_.inputs.workspace.types().enum_domains().values(
                analysis_session_.inputs.workspace.types()
                    .definitions()
                    .enums
                    .domain[analysis_session_.inputs.workspace.types().definitions().enums.index(
                        discriminator)]),
            strings().intern(tag),
            [&](auto id) {
                return declarations().enumerators.name[declarations().enumerators.index(id)];
            })};
        if (value == analysis_session_.inputs.workspace.types()
                         .enum_domains()
                         .values(analysis_session_.inputs.workspace.types()
                                     .definitions()
                                     .enums.domain[analysis_session_.inputs.workspace.types()
                                                       .definitions()
                                                       .enums.index(discriminator)])
                         .end() ||
            declarations().enumerators.sentinel[declarations().enumerators.index(*value)] ||
            (*value ==
             analysis_session_.inputs.workspace.types()
                 .enum_domains()
                 .count_value[analysis_session_.inputs.workspace.types().enum_domains().index(
                     analysis_session_.inputs.workspace.types().definitions().enums.domain
                         [analysis_session_.inputs.workspace.types().definitions().enums.index(
                             discriminator)])])) {
            return false;
        }
        for (std::size_t index{};
             index <
             declarations()
                 .items(document_->storage().declarations.tagged_unions.alternatives
                            [document_->storage().declarations.tagged_unions.index(*schema)])
                 .size();
             ++index) {
            if ((!except.has_value() || index != *except) &&
                declarations()
                        .tagged_union_alternatives
                        .tag[declarations().tagged_union_alternatives.index(declarations().items(
                            document_->storage().declarations.tagged_unions.alternatives
                                [document_->storage().declarations.tagged_unions.index(
                                    *schema)])[index])] == strings().intern(tag)) {
                return false;
            }
        }
        return true;
    };
    auto first_available_tag = [&](std::optional<std::size_t> const except) -> std::string {
        auto const found{std::ranges::find_if(
            analysis_session_.inputs.workspace.types().enum_domains().values(
                analysis_session_.inputs.workspace.types()
                    .definitions()
                    .enums
                    .domain[analysis_session_.inputs.workspace.types().definitions().enums.index(
                        discriminator)]),
            [&](auto const& value) {
                return tag_available(
                    text(declarations().enumerators.name[declarations().enumerators.index(value)]),
                    except);
            })};
        return found == analysis_session_.inputs.workspace.types()
                            .enum_domains()
                            .values(analysis_session_.inputs.workspace.types()
                                        .definitions()
                                        .enums.domain[analysis_session_.inputs.workspace.types()
                                                          .definitions()
                                                          .enums.index(discriminator)])
                            .end()
                 ? std::string{}
                 : text(declarations().enumerators.name[declarations().enumerators.index(*found)]);
    };

    if (ImGui::BeginCombo(
            "Discriminant",
            text(analysis_session_.inputs.workspace.types().cpp_spelling(discriminator_node))
                .c_str())) {
        for (auto const& candidate : analysis_session_.inputs.workspace.types().types()) {
            auto const enumeration{
                ((analysis_session_.inputs.workspace.types().definition(candidate)).kind() ==
                         lispb::schema::TypeDefinitionKind::enumeration
                     ? std::optional{analysis_session_.inputs.workspace.types().definition(
                           candidate)}
                     : std::nullopt)};
            if (!enumeration) {
                continue;
            }
            auto const all_tags_exist{std::ranges::all_of(
                declarations().items(
                    document_->storage().declarations.tagged_unions.alternatives
                        [document_->storage().declarations.tagged_unions.index(*schema)]),
                [&](auto const id) {
                    auto const alt{id};
                    auto const value{std::ranges::find(
                        analysis_session_.inputs.workspace.types().enum_domains().values(
                            analysis_session_.inputs.workspace.types()
                                .definitions()
                                .enums.domain[analysis_session_.inputs.workspace.types()
                                                  .definitions()
                                                  .enums.index(*enumeration)]),
                        declarations()
                            .tagged_union_alternatives
                            .tag[declarations().tagged_union_alternatives.index(alt)],
                        [&](auto id) {
                            return declarations()
                                .enumerators.name[declarations().enumerators.index(id)];
                        })};
                    return value !=
                               analysis_session_.inputs.workspace.types()
                                   .enum_domains()
                                   .values(
                                       analysis_session_.inputs.workspace.types()
                                           .definitions()
                                           .enums.domain[analysis_session_.inputs.workspace.types()
                                                             .definitions()
                                                             .enums.index(*enumeration)])
                                   .end() &&
                           !declarations()
                                .enumerators.sentinel[declarations().enumerators.index(*value)] &&
                           !(*value ==
                             analysis_session_.inputs.workspace.types().enum_domains().count_value
                                 [analysis_session_.inputs.workspace.types().enum_domains().index(
                                     analysis_session_.inputs.workspace.types()
                                         .definitions()
                                         .enums.domain[analysis_session_.inputs.workspace.types()
                                                           .definitions()
                                                           .enums.index(*enumeration)])]);
                })};
            if (!all_tags_exist) {
                continue;
            }
            auto const current{
                analysis_session_.inputs.workspace.types().identity(candidate) ==
                analysis_session_.inputs.workspace.types().identity(discriminator_node)};
            if (ImGui::Selectable(
                    text(analysis_session_.inputs.workspace.types().cpp_spelling(candidate))
                        .c_str(),
                    current) &&
                !current) {
                pending = declarations().tagged_unions.clone(*schema);
                declarations()
                    .tagged_unions.discriminant[declarations().tagged_unions.index(*pending)] =
                    [&] {
                        auto row_0{declarations().type_refs.clone(
                            declarations()
                                .tagged_unions
                                .discriminant[declarations().tagged_unions.index(*pending)])};
                        declarations().type_refs.name[declarations().type_refs.index(row_0)] =
                            strings().intern(
                                analysis_session_.inputs.workspace.types().cpp_spelling(candidate));
                        return row_0;
                    }();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Go to discriminant")) {
        select_type(
            analysis_session_.inputs.workspace.types()
                .occurrences()
                .type[analysis_session_.inputs.workspace.types().occurrences().index(
                    definitions().tagged_unions.discriminant[definitions().tagged_unions.index(
                        tagged_union)])]);
        analysis_session_.inputs.selection.field.clear();
        return true;
    }

    if (ImGui::Button("+ Alternative")) {
        auto const tag{first_available_tag(std::nullopt)};
        if (tag.empty()) {
            schema_edit_message_ = "The discriminant has no unused non-sentinel tag.";
        } else {
            auto replacement{document_->storage().declarations.tagged_unions.clone(*schema)};
            auto name{unique_child_name(
                declarations(),
                declarations().tagged_union_alternatives,
                declarations().tagged_union_alternatives.name,
                declarations().items(
                    document_->storage().declarations.tagged_unions.alternatives
                        [document_->storage().declarations.tagged_unions.index(replacement)]),
                "alternative")};
            auto const alternative{declarations().tagged_union_alternatives.append()};

            declarations()
                .tagged_union_alternatives
                .name[declarations().tagged_union_alternatives.index(alternative)] =
                strings().intern(name);
            declarations()
                .tagged_union_alternatives
                .type[declarations().tagged_union_alternatives.index(alternative)] = [&] {
                auto const new_type_refs{declarations().type_refs.append()};
                declarations().type_refs.name[declarations().type_refs.index(new_type_refs)] =
                    strings().intern("std::uint32_t");
                return new_type_refs;
            }();
            declarations()
                .tagged_union_alternatives
                .tag[declarations().tagged_union_alternatives.index(alternative)] =
                strings().intern(tag);
            document_->storage()
                .declarations.tagged_unions
                .alternatives[document_->storage().declarations.tagged_unions.index(replacement)] =
                [&] {
                    auto const original_items{declarations().items(
                        document_->storage().declarations.tagged_unions.alternatives
                            [document_->storage().declarations.tagged_unions.index(replacement)])};
                    auto rows{std::vector(original_items.begin(), original_items.end())};
                    rows.push_back(std::move(alternative));
                    return declarations().range(rows);
                }();
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
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
            auto replacement{document_->storage().declarations.tagged_unions.clone(*schema)};
            auto copy{declarations().tagged_union_alternatives.clone(declarations().items(
                document_->storage()
                    .declarations.tagged_unions
                    .alternatives[document_->storage().declarations.tagged_unions.index(
                        replacement)])[*selected_index])};
            declarations()
                .tagged_union_alternatives
                .name[declarations().tagged_union_alternatives.index(copy)] =
                strings().intern(unique_child_name(
                    declarations(),
                    declarations().tagged_union_alternatives,
                    declarations().tagged_union_alternatives.name,
                    declarations().items(
                        document_->storage().declarations.tagged_unions.alternatives
                            [document_->storage().declarations.tagged_unions.index(replacement)]),
                    text(declarations()
                             .tagged_union_alternatives
                             .name[declarations().tagged_union_alternatives.index(copy)]) +
                        "_copy"));
            declarations()
                .tagged_union_alternatives
                .tag[declarations().tagged_union_alternatives.index(copy)] = strings().intern(tag);
            document_->storage()
                .declarations.tagged_unions
                .alternatives[document_->storage().declarations.tagged_unions.index(replacement)] =
                [&] {
                    auto const original_items{declarations().items(
                        document_->storage().declarations.tagged_unions.alternatives
                            [document_->storage().declarations.tagged_unions.index(replacement)])};
                    auto rows{std::vector(original_items.begin(), original_items.end())};
                    rows.insert(rows.begin() + static_cast<std::ptrdiff_t>(*selected_index + 1),
                                copy);
                    return declarations().range(rows);
                }();
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
                analysis_session_.inputs.selection.field =
                    text(declarations()
                             .tagged_union_alternatives
                             .name[declarations().tagged_union_alternatives.index(copy)]);
                return true;
            }
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() || *selected_index == 0);
    if (ImGui::Button("Move up")) {
        auto replacement{document_->storage().declarations.tagged_unions.clone(*schema)};
        document_->storage()
            .declarations.tagged_unions
            .alternatives[document_->storage().declarations.tagged_unions.index(replacement)] =
            [&] {
                auto const original_items{declarations().items(
                    document_->storage().declarations.tagged_unions.alternatives
                        [document_->storage().declarations.tagged_unions.index(replacement)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                std::swap(rows[*selected_index], rows[*selected_index - 1]);
                return declarations().range(rows);
            }();
        if (apply_document_edit(
                document_->replace_declaration(*declaration, std::move(replacement)))) {
            analysis_session_.inputs.selection.field = tagged_union_editor_alternative_;
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(
        !selected_index.has_value() ||
        *selected_index + 1 >=
            declarations()
                .items(document_->storage().declarations.tagged_unions.alternatives
                           [document_->storage().declarations.tagged_unions.index(*schema)])
                .size());
    if (ImGui::Button("Move down")) {
        auto replacement{document_->storage().declarations.tagged_unions.clone(*schema)};
        document_->storage()
            .declarations.tagged_unions
            .alternatives[document_->storage().declarations.tagged_unions.index(replacement)] =
            [&] {
                auto const original_items{declarations().items(
                    document_->storage().declarations.tagged_unions.alternatives
                        [document_->storage().declarations.tagged_unions.index(replacement)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                std::swap(rows[*selected_index], rows[*selected_index + 1]);
                return declarations().range(rows);
            }();
        if (apply_document_edit(
                document_->replace_declaration(*declaration, std::move(replacement)))) {
            analysis_session_.inputs.selection.field = tagged_union_editor_alternative_;
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(
        !selected_index.has_value() ||
        declarations()
                .items(document_->storage().declarations.tagged_unions.alternatives
                           [document_->storage().declarations.tagged_unions.index(*schema)])
                .size() == 1);
    if (ImGui::Button("Delete")) {
        auto replacement{document_->storage().declarations.tagged_unions.clone(*schema)};
        document_->storage()
            .declarations.tagged_unions
            .alternatives[document_->storage().declarations.tagged_unions.index(replacement)] =
            [&] {
                auto const original_items{declarations().items(
                    document_->storage().declarations.tagged_unions.alternatives
                        [document_->storage().declarations.tagged_unions.index(replacement)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.erase(rows.begin() + static_cast<std::ptrdiff_t>(*selected_index));
                return declarations().range(rows);
            }();
        auto const next_index{std::min(
            *selected_index,
            declarations()
                    .items(document_->storage().declarations.tagged_unions.alternatives
                               [document_->storage().declarations.tagged_unions.index(replacement)])
                    .size() -
                1)};
        auto const next_name{
            text(declarations()
                     .tagged_union_alternatives
                     .name[declarations().tagged_union_alternatives.index(declarations().items(
                         document_->storage().declarations.tagged_unions.alternatives
                             [document_->storage().declarations.tagged_unions.index(
                                 replacement)])[next_index])])};
        if (apply_document_edit(
                document_->replace_declaration(*declaration, std::move(replacement)))) {
            analysis_session_.inputs.selection.field = next_name;
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();

    if (detail::begin_editable_table(
            "tagged-union-schema-alternatives",
            6,
            declarations()
                .items(document_->storage().declarations.tagged_unions.alternatives
                           [document_->storage().declarations.tagged_unions.index(*schema)])
                .size())) {
        detail::editable_table_column("", ImGuiTableColumnFlags_WidthFixed);
        detail::editable_table_column("Tag");
        detail::editable_table_column("Name");
        detail::editable_table_column("Semantic type");
        detail::editable_table_column("Fixed array", ImGuiTableColumnFlags_WidthFixed);
        detail::editable_table_column("Count", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();
        for (std::size_t index{};
             index <
             declarations()
                 .items(document_->storage().declarations.tagged_unions.alternatives
                            [document_->storage().declarations.tagged_unions.index(*schema)])
                 .size();
             ++index) {
            auto const alternative{declarations().items(
                document_->storage().declarations.tagged_unions.alternatives
                    [document_->storage().declarations.tagged_unions.index(*schema)])[index]};
            auto const row_selected{
                analysis_session_.inputs.selection.field ==
                text(declarations()
                         .tagged_union_alternatives
                         .name[declarations().tagged_union_alternatives.index(alternative)])};
            ImGui::PushID(static_cast<int>(index));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (detail::editable_table_row_handle(row_selected)) {
                analysis_session_.inputs.selection.field =
                    text(declarations()
                             .tagged_union_alternatives
                             .name[declarations().tagged_union_alternatives.index(alternative)]);
                tagged_union_editor_declaration_.reset();
            }
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("TAGGED_UNION_ALTERNATIVE_ROW", &index, sizeof(index));
                ImGui::Text(
                    "Move %s",
                    text(declarations()
                             .tagged_union_alternatives
                             .name[declarations().tagged_union_alternatives.index(alternative)])
                        .c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget()) {
                if (auto const* payload{
                        ImGui::AcceptDragDropPayload("TAGGED_UNION_ALTERNATIVE_ROW")}) {
                    auto const source_index{*static_cast<std::size_t const*>(payload->Data)};
                    if (source_index <
                            declarations()
                                .items(document_->storage().declarations.tagged_unions.alternatives
                                           [document_->storage().declarations.tagged_unions.index(
                                               *schema)])
                                .size() &&
                        source_index != index) {
                        pending = declarations().tagged_unions.clone(*schema);
                        selected_after_edit = text(
                            declarations()
                                .tagged_union_alternatives
                                .name[declarations().tagged_union_alternatives.index(
                                    declarations().items(declarations().tagged_unions.alternatives
                                                             [declarations().tagged_unions.index(
                                                                 *pending)])[source_index])]);
                        declarations()
                            .tagged_unions
                            .alternatives[declarations().tagged_unions.index(*pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .tagged_unions
                                    .alternatives[declarations().tagged_unions.index(*pending)])};
                            auto rows{std::vector(original_items.begin(), original_items.end())};
                            move_element(rows, source_index, index);
                            return declarations().range(rows);
                        }();
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::TableNextColumn();
            if (row_selected) {
                if (ImGui::BeginCombo(
                        "##tag",
                        text(declarations()
                                 .tagged_union_alternatives
                                 .tag[declarations().tagged_union_alternatives.index(alternative)])
                            .c_str())) {
                    for (auto const id :
                         analysis_session_.inputs.workspace.types().enum_domains().values(
                             analysis_session_.inputs.workspace.types()
                                 .definitions()
                                 .enums.domain[analysis_session_.inputs.workspace.types()
                                                   .definitions()
                                                   .enums.index(discriminator)])) {
                        auto const value{id};
                        if (!tag_available(
                                text(
                                    declarations()
                                        .enumerators.name[declarations().enumerators.index(value)]),
                                index)) {
                            continue;
                        }
                        if (ImGui::Selectable(
                                text(declarations()
                                         .enumerators.name[declarations().enumerators.index(value)])
                                    .c_str(),
                                declarations().enumerators.name[declarations().enumerators.index(
                                    value)] ==
                                    declarations()
                                        .tagged_union_alternatives
                                        .tag[declarations().tagged_union_alternatives.index(
                                            alternative)])) {
                            pending = declarations().tagged_unions.clone(*schema);
                            declarations()
                                .tagged_unions
                                .alternatives[declarations().tagged_unions.index(*pending)] = [&] {
                                auto const original_items{declarations().items(
                                    declarations().tagged_unions.alternatives
                                        [declarations().tagged_unions.index(*pending)])};
                                auto rows_1{
                                    std::vector(original_items.begin(), original_items.end())};
                                rows_1[index] = [&] {
                                    auto row_0{declarations().tagged_union_alternatives.clone(
                                        rows_1[index])};
                                    declarations().tagged_union_alternatives.tag
                                        [declarations().tagged_union_alternatives.index(row_0)] =
                                        declarations()
                                            .enumerators
                                            .name[declarations().enumerators.index(value)];
                                    return row_0;
                                }();
                                return declarations().range(rows_1);
                            }();
                        }
                    }
                    ImGui::EndCombo();
                }
            } else {
                ImGui::TextUnformatted(
                    text(declarations()
                             .tagged_union_alternatives
                             .tag[declarations().tagged_union_alternatives.index(alternative)])
                        .c_str());
            }

            ImGui::TableNextColumn();
            if (row_selected) {
                ImGui::SetNextItemWidth(-1.0F);
                auto const submitted{ImGui::InputText("##name",
                                                      tagged_union_alternative_name_.data(),
                                                      tagged_union_alternative_name_.size(),
                                                      ImGuiInputTextFlags_EnterReturnsTrue)};
                if (!pending.has_value() && (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                    pending = declarations().tagged_unions.clone(*schema);
                    declarations()
                        .tagged_unions.alternatives[declarations().tagged_unions.index(*pending)] =
                        [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .tagged_unions
                                    .alternatives[declarations().tagged_unions.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{
                                    declarations().tagged_union_alternatives.clone(rows_1[index])};
                                declarations()
                                    .tagged_union_alternatives
                                    .name[declarations().tagged_union_alternatives.index(row_0)] =
                                    strings().intern(tagged_union_alternative_name_.data());
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                    selected_after_edit = text(
                        declarations().tagged_union_alternatives.name
                            [declarations().tagged_union_alternatives.index(declarations().items(
                                declarations().tagged_unions.alternatives
                                    [declarations().tagged_unions.index(*pending)])[index])]);
                }
            } else {
                ImGui::TextUnformatted(
                    text(declarations()
                             .tagged_union_alternatives
                             .name[declarations().tagged_union_alternatives.index(alternative)])
                        .c_str());
            }

            ImGui::TableNextColumn();
            if (row_selected) {
                auto const type_controls_inline{detail::prepare_editable_type_input()};
                auto const submitted{ImGui::InputText("##type",
                                                      tagged_union_alternative_type_.data(),
                                                      tagged_union_alternative_type_.size(),
                                                      ImGuiInputTextFlags_EnterReturnsTrue)};
                if (!pending.has_value() && (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                    pending = declarations().tagged_unions.clone(*schema);
                    declarations().tagged_unions.alternatives[declarations().tagged_unions.index(
                        *pending)] = [&] {
                        auto const original_items{declarations().items(
                            declarations()
                                .tagged_unions
                                .alternatives[declarations().tagged_unions.index(*pending)])};
                        auto rows_2{std::vector(original_items.begin(), original_items.end())};
                        rows_2[index] = [&] {
                            auto row_1{
                                declarations().tagged_union_alternatives.clone(rows_2[index])};
                            declarations()
                                .tagged_union_alternatives
                                .type[declarations().tagged_union_alternatives.index(row_1)] = [&] {
                                auto row_0{declarations().type_refs.clone(
                                    declarations().tagged_union_alternatives.type
                                        [declarations().tagged_union_alternatives.index(row_1)])};
                                declarations()
                                    .type_refs.name[declarations().type_refs.index(row_0)] =
                                    strings().intern(tagged_union_alternative_type_.data());
                                return row_0;
                            }();
                            return row_1;
                        }();
                        return declarations().range(rows_2);
                    }();
                }
                if (type_controls_inline) {
                    ImGui::SameLine();
                }
                if (auto picked{draw_type_picker(
                        analysis_session_.inputs.workspace.types().module_name(node),
                        analysis_session_.inputs.workspace.types().identity(node))}) {
                    pending = declarations().tagged_unions.clone(*schema);
                    declarations().tagged_unions.alternatives[declarations().tagged_unions.index(
                        *pending)] = [&] {
                        auto const original_items{declarations().items(
                            declarations()
                                .tagged_unions
                                .alternatives[declarations().tagged_unions.index(*pending)])};
                        auto rows_2{std::vector(original_items.begin(), original_items.end())};
                        rows_2[index] = [&] {
                            auto row_1{
                                declarations().tagged_union_alternatives.clone(rows_2[index])};
                            declarations()
                                .tagged_union_alternatives
                                .type[declarations().tagged_union_alternatives.index(row_1)] = [&] {
                                auto row_0{declarations().type_refs.clone(
                                    declarations().tagged_union_alternatives.type
                                        [declarations().tagged_union_alternatives.index(row_1)])};
                                declarations()
                                    .type_refs.name[declarations().type_refs.index(row_0)] =
                                    strings().intern(std::move(*picked));
                                return row_0;
                            }();
                            return row_1;
                        }();
                        return declarations().range(rows_2);
                    }();
                }
                ImGui::SameLine();
                if (index < definitions().tagged_unions.alternatives_of(tagged_union).size() &&
                    detail::semantic_type_navigation_button()) {
                    navigate_to =
                        analysis_session_.inputs.workspace.types()
                            .occurrences()
                            .type[analysis_session_.inputs.workspace.types().occurrences().index(
                                definitions().tagged_union_alternatives.semantic_type
                                    [definitions().tagged_union_alternatives.index(
                                        definitions().tagged_unions.alternatives_of(
                                            tagged_union)[index])])];
                }
                detail::editable_table_content_hint(tagged_union_alternative_type_.data(), 80.0F);
            } else {
                ImGui::TextUnformatted(
                    text(declarations().type_refs.name[declarations().type_refs.index(
                             declarations().tagged_union_alternatives.type
                                 [declarations().tagged_union_alternatives.index(alternative)])])
                        .c_str());
            }

            ImGui::TableNextColumn();
            if (row_selected) {
                if (ImGui::Checkbox("##fixed-array", &tagged_union_alternative_is_array_)) {
                    pending = declarations().tagged_unions.clone(*schema);
                    declarations()
                        .tagged_unions.alternatives[declarations().tagged_unions.index(*pending)] =
                        [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .tagged_unions
                                    .alternatives[declarations().tagged_unions.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{
                                    declarations().tagged_union_alternatives.clone(rows_1[index])};
                                declarations()
                                    .tagged_union_alternatives
                                    .count[declarations().tagged_union_alternatives.index(row_0)] =
                                    tagged_union_alternative_is_array_
                                        ? std::optional{tagged_union_alternative_count_}
                                        : std::nullopt;
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                }
            } else {
                ImGui::TextUnformatted(
                    declarations()
                            .tagged_union_alternatives
                            .count[declarations().tagged_union_alternatives.index(alternative)]
                            .has_value()
                        ? "yes"
                        : "-");
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
                    pending = declarations().tagged_unions.clone(*schema);
                    declarations()
                        .tagged_unions.alternatives[declarations().tagged_unions.index(*pending)] =
                        [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .tagged_unions
                                    .alternatives[declarations().tagged_unions.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{
                                    declarations().tagged_union_alternatives.clone(rows_1[index])};
                                declarations()
                                    .tagged_union_alternatives
                                    .count[declarations().tagged_union_alternatives.index(row_0)] =
                                    tagged_union_alternative_count_;
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                }
            } else if (declarations()
                           .tagged_union_alternatives
                           .count[declarations().tagged_union_alternatives.index(alternative)]
                           .has_value()) {
                ImGui::Text(
                    "%llu",
                    static_cast<unsigned long long>(
                        *declarations()
                             .tagged_union_alternatives
                             .count[declarations().tagged_union_alternatives.index(alternative)]));
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
        auto const invalid_alternative{std::ranges::find_if(
            declarations().items(
                declarations()
                    .tagged_unions.alternatives[declarations().tagged_unions.index(*pending)]),
            [&](auto const id) {
                auto const alternative{id};
                return text(declarations()
                                .tagged_union_alternatives
                                .name[declarations().tagged_union_alternatives.index(alternative)])
                           .empty() ||
                       text(declarations().type_refs.name[declarations().type_refs.index(
                                declarations().tagged_union_alternatives.type
                                    [declarations().tagged_union_alternatives.index(alternative)])])
                           .empty() ||
                       text(declarations()
                                .tagged_union_alternatives
                                .tag[declarations().tagged_union_alternatives.index(alternative)])
                           .empty() ||
                       declarations().tagged_union_alternatives.count
                               [declarations().tagged_union_alternatives.index(alternative)] == 0;
            })};
        if (invalid_alternative !=
            declarations()
                .items(declarations().tagged_unions.alternatives[declarations().tagged_unions.index(
                    *pending)])
                .end()) {
            schema_edit_message_ = "Tagged-union name, type, tag, and positive count are required.";
            return false;
        }
        if (apply_document_edit(
                document_->replace_declaration(*declaration, std::move(*pending)))) {
            analysis_session_.inputs.selection.field = std::move(selected_after_edit);
            return true;
        }
    }
    return false;
}

auto PlannerUi::draw_record_editor(TypeId const node, TypeDefinitionId const& record) -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const declaration{
        document_->find_declaration(analysis_session_.inputs.workspace.types().identity(node))};
    if (!declaration.has_value()) {
        return false;
    }
    auto const schema{
        document_->declaration_handle(*declaration, lispb::codegen::DeclarationKind::record)};
    if (!schema.has_value()) {
        return false;
    }

    if (analysis_session_.inputs.selection.field.empty() &&
        !declarations()
             .items(document_->storage()
                        .declarations.records
                        .members[document_->storage().declarations.records.index(*schema)])
             .empty()) {
        analysis_session_.inputs.selection.field =
            text(declarations().record_members.name[declarations().record_members.index(
                declarations()
                    .items(document_->storage().declarations.records.members
                               [document_->storage().declarations.records.index(*schema)])
                    .front())]);
    }
    auto selected{std::ranges::find(
        declarations().items(
            document_->storage()
                .declarations.records
                .members[document_->storage().declarations.records.index(*schema)]),
        strings().intern(analysis_session_.inputs.selection.field),
        [&](auto id) {
            return declarations().record_members.name[declarations().record_members.index(id)];
        })};
    if (selected ==
            declarations()
                .items(document_->storage()
                           .declarations.records
                           .members[document_->storage().declarations.records.index(*schema)])
                .end() &&
        !declarations()
             .items(document_->storage()
                        .declarations.records
                        .members[document_->storage().declarations.records.index(*schema)])
             .empty()) {
        selected =
            declarations()
                .items(document_->storage()
                           .declarations.records
                           .members[document_->storage().declarations.records.index(*schema)])
                .begin();
        analysis_session_.inputs.selection.field = text(
            declarations().record_members.name[declarations().record_members.index(*selected)]);
    }
    auto const selected_index{
        selected ==
                declarations()
                    .items(document_->storage()
                               .declarations.records
                               .members[document_->storage().declarations.records.index(*schema)])
                    .end()
            ? std::optional<std::size_t>{}
            : std::optional<std::size_t>{static_cast<std::size_t>(
                  selected -
                  declarations()
                      .items(document_->storage()
                                 .declarations.records
                                 .members[document_->storage().declarations.records.index(*schema)])
                      .begin())}};
    if (record_editor_declaration_ != declaration ||
        record_editor_member_ != analysis_session_.inputs.selection.field) {
        record_editor_declaration_ = declaration;
        record_editor_member_ = analysis_session_.inputs.selection.field;
        std::snprintf(
            record_export_specifier_.data(),
            record_export_specifier_.size(),
            "%s",
            text(document_->storage()
                     .declarations.records
                     .export_specifier[document_->storage().declarations.records.index(*schema)])
                .c_str());
        if (selected !=
            declarations()
                .items(document_->storage()
                           .declarations.records
                           .members[document_->storage().declarations.records.index(*schema)])
                .end()) {
            std::snprintf(
                record_member_name_.data(),
                record_member_name_.size(),
                "%s",
                text(declarations()
                         .record_members.name[declarations().record_members.index(*selected)])
                    .c_str());
            std::snprintf(
                record_member_type_.data(),
                record_member_type_.size(),
                "%s",
                text(declarations().type_refs.name[declarations().type_refs.index(
                         declarations()
                             .record_members.type[declarations().record_members.index(*selected)])])
                    .c_str());
            record_member_is_array_ =
                declarations()
                    .record_members.count[declarations().record_members.index(*selected)]
                    .has_value();
            record_member_count_ =
                declarations()
                    .record_members.count[declarations().record_members.index(*selected)]
                    .value_or(1);
            std::snprintf(
                record_relationship_target_.data(),
                record_relationship_target_.size(),
                "%s",
                declarations()
                        .record_members.relationship[declarations().record_members.index(*selected)]
                        .has_value()
                    ? text(declarations().type_refs.name[declarations().type_refs.index(
                               declarations().semantic_relations.target
                                   [declarations().semantic_relations.index(
                                       (*declarations().record_members.relationship
                                             [declarations().record_members.index(*selected)]))])])
                          .c_str()
                    : "");
            auto const relationship_kind{
                declarations()
                        .record_members.relationship[declarations().record_members.index(*selected)]
                        .has_value()
                    ? std::ranges::find(
                          semantic_relationship_kinds,
                          declarations()
                              .semantic_relations.kind[declarations().semantic_relations.index(
                                  (*declarations().record_members.relationship
                                        [declarations().record_members.index(*selected)]))])
                    : semantic_relationship_kinds.end()};
            record_relationship_kind_ =
                relationship_kind == semantic_relationship_kinds.end()
                    ? 0
                    : static_cast<int>(relationship_kind - semantic_relationship_kinds.begin());
            auto const relationship_unit{
                declarations()
                            .record_members
                            .relationship[declarations().record_members.index(*selected)]
                            .has_value() &&
                        declarations()
                            .semantic_relations
                            .unit[declarations().semantic_relations.index(
                                (*declarations().record_members.relationship
                                      [declarations().record_members.index(*selected)]))]
                            .has_value()
                    ? std::ranges::find(
                          semantic_relationship_units,
                          *declarations()
                               .semantic_relations.unit[declarations().semantic_relations.index(
                                   (*declarations().record_members.relationship
                                         [declarations().record_members.index(*selected)]))])
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
        auto const export_specifier{std::string_view{record_export_specifier_.data()}};
        if (text(document_->storage()
                     .declarations.records
                     .export_specifier[document_->storage().declarations.records.index(*schema)]) !=
            export_specifier) {
            auto replacement{document_->storage().declarations.records.clone(*schema)};
            document_->storage()
                .declarations.records
                .export_specifier[document_->storage().declarations.records.index(replacement)] =
                strings().intern(export_specifier);
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
                analysis_session_.inputs.selection.field = record_editor_member_;
                return true;
            }
        }
    }

    if (ImGui::Button("+ Member")) {
        auto replacement{document_->storage().declarations.records.clone(*schema)};
        auto name{unique_child_name(
            declarations(),
            declarations().record_members,
            declarations().record_members.name,
            declarations().items(
                document_->storage()
                    .declarations.records
                    .members[document_->storage().declarations.records.index(replacement)]),
            "member")};
        document_->storage()
            .declarations.records
            .members[document_->storage().declarations.records.index(replacement)] = [&] {
            auto const original_items{declarations().items(
                document_->storage()
                    .declarations.records
                    .members[document_->storage().declarations.records.index(replacement)])};
            auto rows{std::vector(original_items.begin(), original_items.end())};
            rows.push_back([&] {
                auto const new_record_members{declarations().record_members.append()};
                declarations()
                    .record_members.name[declarations().record_members.index(new_record_members)] =
                    strings().intern(name);
                declarations().record_members.type[declarations().record_members.index(
                    new_record_members)] = [&] {
                    auto const new_type_refs{declarations().type_refs.append()};
                    declarations().type_refs.name[declarations().type_refs.index(new_type_refs)] =
                        strings().intern("std::uint32_t");
                    declarations()
                        .type_refs.suffix[declarations().type_refs.index(new_type_refs)] = {};
                    declarations().type_refs.nested[declarations().type_refs.index(new_type_refs)] =
                        std::nullopt;
                    return new_type_refs;
                }();
                declarations()
                    .record_members.count[declarations().record_members.index(new_record_members)] =
                    std::nullopt;
                declarations().record_members.relationship[declarations().record_members.index(
                    new_record_members)] = std::nullopt;
                return new_record_members;
            }());
            return declarations().range(rows);
        }();
        if (apply_document_edit(
                document_->replace_declaration(*declaration, std::move(replacement)))) {
            analysis_session_.inputs.selection.field = std::move(name);
            return true;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value());
    if (ImGui::Button("Duplicate")) {
        auto replacement{document_->storage().declarations.records.clone(*schema)};
        auto copy{declarations().record_members.clone(declarations().items(
            document_->storage()
                .declarations.records.members[document_->storage().declarations.records.index(
                    replacement)])[*selected_index])};
        declarations().record_members.name[declarations().record_members.index(copy)] =
            strings().intern(unique_child_name(
                declarations(),
                declarations().record_members,
                declarations().record_members.name,
                declarations().items(
                    document_->storage()
                        .declarations.records
                        .members[document_->storage().declarations.records.index(replacement)]),
                text(
                    declarations().record_members.name[declarations().record_members.index(copy)]) +
                    "_copy"));
        document_->storage()
            .declarations.records
            .members[document_->storage().declarations.records.index(replacement)] = [&] {
            auto const original_items{declarations().items(
                document_->storage()
                    .declarations.records
                    .members[document_->storage().declarations.records.index(replacement)])};
            auto rows{std::vector(original_items.begin(), original_items.end())};
            rows.insert(rows.begin() + static_cast<std::ptrdiff_t>(*selected_index + 1), copy);
            return declarations().range(rows);
        }();
        if (apply_document_edit(
                document_->replace_declaration(*declaration, std::move(replacement)))) {
            analysis_session_.inputs.selection.field =
                text(declarations().record_members.name[declarations().record_members.index(copy)]);
            return true;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() || *selected_index == 0);
    if (ImGui::Button("Move up")) {
        auto replacement{document_->storage().declarations.records.clone(*schema)};
        document_->storage()
            .declarations.records
            .members[document_->storage().declarations.records.index(replacement)] = [&] {
            auto const original_items{declarations().items(
                document_->storage()
                    .declarations.records
                    .members[document_->storage().declarations.records.index(replacement)])};
            auto rows{std::vector(original_items.begin(), original_items.end())};
            std::swap(rows[*selected_index], rows[*selected_index - 1]);
            return declarations().range(rows);
        }();
        if (apply_document_edit(
                document_->replace_declaration(*declaration, std::move(replacement)))) {
            analysis_session_.inputs.selection.field = record_editor_member_;
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(
        !selected_index.has_value() ||
        *selected_index + 1 >=
            declarations()
                .items(document_->storage()
                           .declarations.records
                           .members[document_->storage().declarations.records.index(*schema)])
                .size());
    if (ImGui::Button("Move down")) {
        auto replacement{document_->storage().declarations.records.clone(*schema)};
        document_->storage()
            .declarations.records
            .members[document_->storage().declarations.records.index(replacement)] = [&] {
            auto const original_items{declarations().items(
                document_->storage()
                    .declarations.records
                    .members[document_->storage().declarations.records.index(replacement)])};
            auto rows{std::vector(original_items.begin(), original_items.end())};
            std::swap(rows[*selected_index], rows[*selected_index + 1]);
            return declarations().range(rows);
        }();
        if (apply_document_edit(
                document_->replace_declaration(*declaration, std::move(replacement)))) {
            analysis_session_.inputs.selection.field = record_editor_member_;
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value());
    if (ImGui::Button("Delete")) {
        auto replacement{document_->storage().declarations.records.clone(*schema)};
        auto const deleted_name{
            text(declarations().record_members.name[declarations().record_members.index(
                declarations().items(document_->storage().declarations.records.members
                                         [document_->storage().declarations.records.index(
                                             replacement)])[*selected_index])])};
        document_->storage()
            .declarations.records
            .members[document_->storage().declarations.records.index(replacement)] = [&] {
            auto const original_items{declarations().items(
                document_->storage()
                    .declarations.records
                    .members[document_->storage().declarations.records.index(replacement)])};
            auto rows{std::vector(original_items.begin(), original_items.end())};
            rows.erase(rows.begin() + static_cast<std::ptrdiff_t>(*selected_index));
            return declarations().range(rows);
        }();
        auto const next_name{
            declarations()
                    .items(
                        document_->storage()
                            .declarations.records
                            .members[document_->storage().declarations.records.index(replacement)])
                    .empty()
                ? std::string{}
                : text(declarations().record_members.name[declarations().record_members.index(
                      declarations().items(
                          document_->storage().declarations.records.members
                              [document_->storage().declarations.records.index(replacement)])
                          [std::min(
                              *selected_index,
                              declarations()
                                      .items(document_->storage().declarations.records.members
                                                 [document_->storage().declarations.records.index(
                                                     replacement)])
                                      .size() -
                                  1)])])};
        if (apply_document_edit(
                document_->replace_declaration(*declaration, std::move(replacement)))) {
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
        for (auto const member_id : declarations().items(
                 document_->storage()
                     .declarations.records
                     .members[document_->storage().declarations.records.index(*schema)])) {
            auto const member{member_id};
            analysis_session_.inputs.selection.record_access_members.insert_or_assign(
                text(declarations()
                         .record_members.name[declarations().record_members.index(member)]),
                analysis_session_.inputs.access_operation);
        }
        analysis_session_.inputs.selection.record_access_set_explicit = true;
    }

    std::optional<lispb::codegen::DeclarationHandle> pending;
    std::optional<TypeId> navigate_to;
    std::optional<std::pair<std::string, std::string>> renamed_member;
    auto selected_after_edit{analysis_session_.inputs.selection.field};
    if (detail::begin_editable_table(
            "record-schema-members",
            7,
            declarations()
                .items(document_->storage()
                           .declarations.records
                           .members[document_->storage().declarations.records.index(*schema)])
                .size())) {
        detail::editable_table_column("", ImGuiTableColumnFlags_WidthFixed);
        detail::editable_table_column("Access", ImGuiTableColumnFlags_WidthFixed);
        detail::editable_table_column("Operation", ImGuiTableColumnFlags_WidthFixed);
        detail::editable_table_column("Name");
        detail::editable_table_column("Semantic type");
        detail::editable_table_column("Fixed array", ImGuiTableColumnFlags_WidthFixed);
        detail::editable_table_column("Count", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();
        for (std::size_t index{};
             index <
             declarations()
                 .items(document_->storage()
                            .declarations.records
                            .members[document_->storage().declarations.records.index(*schema)])
                 .size();
             ++index) {
            auto const member{declarations().items(
                document_->storage()
                    .declarations.records
                    .members[document_->storage().declarations.records.index(*schema)])[index]};
            auto const row_selected{
                analysis_session_.inputs.selection.field ==
                text(declarations()
                         .record_members.name[declarations().record_members.index(member)])};
            ImGui::PushID(static_cast<int>(index));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (detail::editable_table_row_handle(row_selected)) {
                analysis_session_.inputs.selection.field =
                    text(declarations()
                             .record_members.name[declarations().record_members.index(member)]);
                record_editor_declaration_.reset();
            }
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("RECORD_MEMBER_ROW", &index, sizeof(index));
                ImGui::Text(
                    "Move %s",
                    text(declarations()
                             .record_members.name[declarations().record_members.index(member)])
                        .c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget()) {
                if (auto const* payload{ImGui::AcceptDragDropPayload("RECORD_MEMBER_ROW")}) {
                    auto const source_index{*static_cast<std::size_t const*>(payload->Data)};
                    if (source_index <
                            declarations()
                                .items(
                                    document_->storage().declarations.records.members
                                        [document_->storage().declarations.records.index(*schema)])
                                .size() &&
                        source_index != index) {
                        pending = declarations().records.clone(*schema);
                        selected_after_edit = text(
                            declarations().record_members.name[declarations().record_members.index(
                                declarations().items(
                                    declarations().records.members[declarations().records.index(
                                        *pending)])[source_index])]);
                        declarations().records.members[declarations().records.index(*pending)] =
                            [&] {
                                auto const original_items{declarations().items(
                                    declarations()
                                        .records.members[declarations().records.index(*pending)])};
                                auto rows{
                                    std::vector(original_items.begin(), original_items.end())};
                                move_element(rows, source_index, index);
                                return declarations().range(rows);
                            }();
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::TableNextColumn();
            auto accessed{
                analysis_session_.inputs.selection.record_access_set_explicit
                    ? analysis_session_.inputs.selection.record_access_members.contains(text(
                          declarations()
                              .record_members.name[declarations().record_members.index(member)]))
                    : analysis_session_.inputs.selection.field ==
                          text(declarations()
                                   .record_members
                                   .name[declarations().record_members.index(member)])};
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
                        text(declarations()
                                 .record_members.name[declarations().record_members.index(member)]),
                        analysis_session_.inputs.access_operation);
                } else {
                    analysis_session_.inputs.selection.record_access_members.erase(text(
                        declarations()
                            .record_members.name[declarations().record_members.index(member)]));
                }
            }

            ImGui::TableNextColumn();
            if (accessed) {
                auto operation{analysis_session_.inputs.access_operation};
                if (analysis_session_.inputs.selection.record_access_set_explicit) {
                    if (auto const found{
                            analysis_session_.inputs.selection.record_access_members.find(
                                text(declarations()
                                         .record_members
                                         .name[declarations().record_members.index(member)]))};
                        found != analysis_session_.inputs.selection.record_access_members.end()) {
                        operation = found->second;
                    }
                }
                auto operation_index{static_cast<int>(operation)};
                ImGui::SetNextItemWidth(105.0F);
                if (ImGui::Combo("##access-operation", &operation_index, "R\0W\0RW\0")) {
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
                        text(declarations()
                                 .record_members.name[declarations().record_members.index(member)]),
                        static_cast<AccessOperation>(operation_index));
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
                    pending = declarations().records.clone(*schema);
                    declarations().records.members[declarations().records.index(*pending)] = [&] {
                        auto const original_items{declarations().items(
                            declarations()
                                .records.members[declarations().records.index(*pending)])};
                        auto rows_1{std::vector(original_items.begin(), original_items.end())};
                        rows_1[index] = [&] {
                            auto row_0{declarations().record_members.clone(rows_1[index])};
                            declarations()
                                .record_members.name[declarations().record_members.index(row_0)] =
                                strings().intern(record_member_name_.data());
                            return row_0;
                        }();
                        return declarations().range(rows_1);
                    }();
                    selected_after_edit =
                        text(declarations().record_members.name[declarations().record_members.index(
                            declarations().items(
                                declarations().records.members[declarations().records.index(
                                    *pending)])[index])]);
                    renamed_member = std::pair{
                        text(declarations()
                                 .record_members.name[declarations().record_members.index(member)]),
                        selected_after_edit};
                }
            } else {
                ImGui::TextUnformatted(
                    text(declarations()
                             .record_members.name[declarations().record_members.index(member)])
                        .c_str());
            }

            ImGui::TableNextColumn();
            if (row_selected) {
                auto const type_controls_inline{detail::prepare_editable_type_input()};
                auto const submitted{ImGui::InputText("##type",
                                                      record_member_type_.data(),
                                                      record_member_type_.size(),
                                                      ImGuiInputTextFlags_EnterReturnsTrue)};
                if (!pending.has_value() && (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                    pending = declarations().records.clone(*schema);
                    declarations().records.members[declarations().records.index(*pending)] = [&] {
                        auto const original_items{declarations().items(
                            declarations()
                                .records.members[declarations().records.index(*pending)])};
                        auto rows_2{std::vector(original_items.begin(), original_items.end())};
                        rows_2[index] = [&] {
                            auto row_1{declarations().record_members.clone(rows_2[index])};
                            declarations()
                                .record_members.type[declarations().record_members.index(row_1)] =
                                [&] {
                                    auto row_0{declarations().type_refs.clone(
                                        declarations()
                                            .record_members
                                            .type[declarations().record_members.index(row_1)])};
                                    declarations()
                                        .type_refs.name[declarations().type_refs.index(row_0)] =
                                        strings().intern(record_member_type_.data());
                                    return row_0;
                                }();
                            return row_1;
                        }();
                        return declarations().range(rows_2);
                    }();
                }
                if (type_controls_inline) {
                    ImGui::SameLine();
                }
                if (auto picked{draw_type_picker(
                        analysis_session_.inputs.workspace.types().module_name(node),
                        analysis_session_.inputs.workspace.types().identity(node))}) {
                    pending = declarations().records.clone(*schema);
                    declarations().records.members[declarations().records.index(*pending)] = [&] {
                        auto const original_items{declarations().items(
                            declarations()
                                .records.members[declarations().records.index(*pending)])};
                        auto rows_2{std::vector(original_items.begin(), original_items.end())};
                        rows_2[index] = [&] {
                            auto row_1{declarations().record_members.clone(rows_2[index])};
                            declarations()
                                .record_members.type[declarations().record_members.index(row_1)] =
                                [&] {
                                    auto row_0{declarations().type_refs.clone(
                                        declarations()
                                            .record_members
                                            .type[declarations().record_members.index(row_1)])};
                                    declarations()
                                        .type_refs.name[declarations().type_refs.index(row_0)] =
                                        strings().intern(std::move(*picked));
                                    return row_0;
                                }();
                            return row_1;
                        }();
                        return declarations().range(rows_2);
                    }();
                }
                ImGui::SameLine();
                if (index < definitions().records.members_of(record).size() &&
                    detail::semantic_type_navigation_button()) {
                    navigate_to =
                        analysis_session_.inputs.workspace.types()
                            .occurrences()
                            .type[analysis_session_.inputs.workspace.types().occurrences().index(
                                definitions().record_members.semantic_type
                                    [definitions().record_members.index(
                                        definitions().records.members_of(record)[index])])];
                }
                detail::editable_table_content_hint(record_member_type_.data(), 80.0F);
            } else {
                ImGui::TextUnformatted(
                    text(declarations().type_refs.name[declarations().type_refs.index(
                             declarations().record_members.type[declarations().record_members.index(
                                 member)])])
                        .c_str());
            }

            ImGui::TableNextColumn();
            if (row_selected) {
                if (ImGui::Checkbox("##fixed-array", &record_member_is_array_)) {
                    pending = declarations().records.clone(*schema);
                    declarations().records.members[declarations().records.index(*pending)] = [&] {
                        auto const original_items{declarations().items(
                            declarations()
                                .records.members[declarations().records.index(*pending)])};
                        auto rows_1{std::vector(original_items.begin(), original_items.end())};
                        rows_1[index] = [&] {
                            auto row_0{declarations().record_members.clone(rows_1[index])};
                            declarations()
                                .record_members.count[declarations().record_members.index(row_0)] =
                                record_member_is_array_ ? std::optional{record_member_count_}
                                                        : std::nullopt;
                            return row_0;
                        }();
                        return declarations().range(rows_1);
                    }();
                }
            } else {
                ImGui::TextUnformatted(
                    declarations()
                            .record_members.count[declarations().record_members.index(member)]
                            .has_value()
                        ? "yes"
                        : "-");
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
                    pending = declarations().records.clone(*schema);
                    declarations().records.members[declarations().records.index(*pending)] = [&] {
                        auto const original_items{declarations().items(
                            declarations()
                                .records.members[declarations().records.index(*pending)])};
                        auto rows_1{std::vector(original_items.begin(), original_items.end())};
                        rows_1[index] = [&] {
                            auto row_0{declarations().record_members.clone(rows_1[index])};
                            declarations()
                                .record_members.count[declarations().record_members.index(row_0)] =
                                record_member_count_;
                            return row_0;
                        }();
                        return declarations().range(rows_1);
                    }();
                }
            } else if (declarations()
                           .record_members.count[declarations().record_members.index(member)]
                           .has_value()) {
                ImGui::Text(
                    "%llu",
                    static_cast<unsigned long long>(
                        *declarations()
                             .record_members.count[declarations().record_members.index(member)]));
            } else {
                ImGui::TextUnformatted("1");
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (!pending.has_value() && selected_index.has_value()) {
        auto const member_name{
            text(declarations().record_members.name[declarations().record_members.index(
                declarations().items(document_->storage().declarations.records.members
                                         [document_->storage().declarations.records.index(
                                             *schema)])[*selected_index])])};
        auto const member{declarations().items(
            document_->storage()
                .declarations.records.members[document_->storage().declarations.records.index(
                    *schema)])[*selected_index]};
        auto const resolved_member{
            *selected_index < definitions().records.members_of(record).size()
                ? std::optional{definitions().records.members_of(record)[*selected_index]}
                : std::nullopt};
        auto const current_kind{semantic_relationship_kinds[static_cast<std::size_t>(
            std::clamp(record_relationship_kind_,
                       0,
                       static_cast<int>(semantic_relationship_kinds.size() - 1)))]};
        auto const current_unit{semantic_relationship_units[static_cast<std::size_t>(
            std::clamp(record_relationship_unit_,
                       0,
                       static_cast<int>(semantic_relationship_units.size() - 1)))]};
        if (section_with_tooltip(
                "Selected member relationship",
                "Describe how this record member relates to another semantic type.")) {
            ImGui::SetNextItemWidth(180.0F);
            if (ImGui::BeginCombo(
                    "Kind", lispb::codegen::semantic_relation_kind_name(current_kind).data())) {
                for (std::size_t kind_index{}; kind_index < semantic_relationship_kinds.size();
                     ++kind_index) {
                    auto const kind{semantic_relationship_kinds[kind_index]};
                    auto const chosen{record_relationship_kind_ == static_cast<int>(kind_index)};
                    if (ImGui::Selectable(lispb::codegen::semantic_relation_kind_name(kind).data(),
                                          chosen)) {
                        record_relationship_kind_ = static_cast<int>(kind_index);
                        if (declarations()
                                .record_members
                                .relationship[declarations().record_members.index(member)]
                                .has_value()) {
                            auto replacement{
                                document_->storage().declarations.records.clone(*schema)};
                            document_->storage()
                                .declarations.records
                                .members[document_->storage().declarations.records.index(
                                    replacement)] = [&] {
                                auto const original_items{declarations().items(
                                    document_->storage()
                                        .declarations.records
                                        .members[document_->storage().declarations.records.index(
                                            replacement)])};
                                auto rows_2{
                                    std::vector(original_items.begin(), original_items.end())};
                                rows_2[*selected_index] = [&] {
                                    auto row_1{declarations().record_members.clone(
                                        rows_2[*selected_index])};
                                    (*declarations().record_members.relationship
                                          [declarations().record_members.index(row_1)]) = [&] {
                                        auto row_0{declarations().semantic_relations.clone(
                                            (*declarations().record_members.relationship
                                                  [declarations().record_members.index(row_1)]))};
                                        declarations()
                                            .semantic_relations
                                            .kind[declarations().semantic_relations.index(row_0)] =
                                            kind;
                                        return row_0;
                                    }();
                                    return row_1;
                                }();
                                return declarations().range(rows_2);
                            }();
                            document_->storage()
                                .declarations.records
                                .members[document_->storage().declarations.records.index(
                                    replacement)] = [&] {
                                auto const original_items{declarations().items(
                                    document_->storage()
                                        .declarations.records
                                        .members[document_->storage().declarations.records.index(
                                            replacement)])};
                                auto rows_2{
                                    std::vector(original_items.begin(), original_items.end())};
                                rows_2[*selected_index] = [&] {
                                    auto row_1{declarations().record_members.clone(
                                        rows_2[*selected_index])};
                                    (*declarations().record_members.relationship
                                          [declarations().record_members.index(row_1)]) = [&] {
                                        auto row_0{declarations().semantic_relations.clone(
                                            (*declarations().record_members.relationship
                                                  [declarations().record_members.index(row_1)]))};
                                        declarations()
                                            .semantic_relations
                                            .unit[declarations().semantic_relations.index(row_0)] =
                                            kind == lispb::codegen::SemanticRelationKind::
                                                        offset_into
                                                ? std::optional{current_unit}
                                                : std::nullopt;
                                        return row_0;
                                    }();
                                    return row_1;
                                }();
                                return declarations().range(rows_2);
                            }();
                            if (apply_document_edit(document_->replace_declaration(
                                    *declaration, std::move(replacement)))) {
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
            if (current_kind == lispb::codegen::SemanticRelationKind::offset_into) {
                ImGui::SetNextItemWidth(180.0F);
                if (ImGui::BeginCombo(
                        "Unit", lispb::codegen::semantic_relation_unit_name(current_unit).data())) {
                    for (std::size_t unit_index{}; unit_index < semantic_relationship_units.size();
                         ++unit_index) {
                        auto const unit{semantic_relationship_units[unit_index]};
                        auto const chosen{record_relationship_unit_ ==
                                          static_cast<int>(unit_index)};
                        if (ImGui::Selectable(
                                lispb::codegen::semantic_relation_unit_name(unit).data(), chosen)) {
                            record_relationship_unit_ = static_cast<int>(unit_index);
                            if (declarations()
                                    .record_members
                                    .relationship[declarations().record_members.index(member)]
                                    .has_value()) {
                                auto replacement{
                                    document_->storage().declarations.records.clone(*schema)};
                                document_->storage()
                                    .declarations.records
                                    .members[document_->storage().declarations.records.index(
                                        replacement)] = [&] {
                                    auto const original_items{declarations().items(
                                        document_->storage().declarations.records.members
                                            [document_->storage().declarations.records.index(
                                                replacement)])};
                                    auto rows_2{
                                        std::vector(original_items.begin(), original_items.end())};
                                    rows_2[*selected_index] = [&] {
                                        auto row_1{declarations().record_members.clone(
                                            rows_2[*selected_index])};
                                        (*declarations().record_members.relationship
                                              [declarations().record_members.index(row_1)]) = [&] {
                                            auto row_0{declarations().semantic_relations.clone(
                                                (*declarations().record_members.relationship
                                                      [declarations().record_members.index(
                                                          row_1)]))};
                                            declarations().semantic_relations.unit
                                                [declarations().semantic_relations.index(row_0)] =
                                                unit;
                                            return row_0;
                                        }();
                                        return row_1;
                                    }();
                                    return declarations().range(rows_2);
                                }();
                                if (apply_document_edit(document_->replace_declaration(
                                        *declaration, std::move(replacement)))) {
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
            if (declarations()
                    .record_members.relationship[declarations().record_members.index(member)]
                    .has_value() &&
                (target_submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                if (record_relationship_target_.front() == '\0') {
                    schema_edit_message_ = "Relationship target cannot be empty.";
                } else {
                    auto replacement{document_->storage().declarations.records.clone(*schema)};
                    document_->storage().declarations.records.members
                        [document_->storage().declarations.records.index(replacement)] = [&] {
                        auto const original_items{declarations().items(
                            document_->storage().declarations.records.members
                                [document_->storage().declarations.records.index(replacement)])};
                        auto rows_3{std::vector(original_items.begin(), original_items.end())};
                        rows_3[*selected_index] = [&] {
                            auto row_2{
                                declarations().record_members.clone(rows_3[*selected_index])};
                            (*declarations()
                                  .record_members
                                  .relationship[declarations().record_members.index(row_2)]) = [&] {
                                auto row_1{declarations().semantic_relations.clone(
                                    (*declarations().record_members.relationship
                                          [declarations().record_members.index(row_2)]))};
                                declarations()
                                    .semantic_relations
                                    .target[declarations().semantic_relations.index(row_1)] = [&] {
                                    auto row_0{declarations().type_refs.clone(
                                        declarations().semantic_relations.target
                                            [declarations().semantic_relations.index(row_1)])};
                                    declarations()
                                        .type_refs.name[declarations().type_refs.index(row_0)] =
                                        strings().intern(record_relationship_target_.data());
                                    return row_0;
                                }();
                                return row_1;
                            }();
                            return row_2;
                        }();
                        return declarations().range(rows_3);
                    }();
                    if (apply_document_edit(
                            document_->replace_declaration(*declaration, std::move(replacement)))) {
                        record_editor_declaration_.reset();
                        analysis_session_.inputs.selection.field = member_name;
                        return true;
                    }
                }
            }
            ImGui::SameLine();
            ImGui::PushID("record-relationship-target");
            auto picked_relationship_target{
                draw_type_picker(analysis_session_.inputs.workspace.types().module_name(node),
                                 analysis_session_.inputs.workspace.types().identity(node))};
            ImGui::PopID();
            if (picked_relationship_target.has_value()) {
                auto replacement{document_->storage().declarations.records.clone(*schema)};
                document_->storage()
                    .declarations.records
                    .members[document_->storage().declarations.records.index(replacement)] = [&] {
                    auto const original_items{declarations().items(
                        document_->storage().declarations.records.members
                            [document_->storage().declarations.records.index(replacement)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[*selected_index] = [&] {
                        auto row_0{declarations().record_members.clone(rows_1[*selected_index])};
                        declarations()
                            .record_members
                            .relationship[declarations().record_members.index(row_0)] = [&] {
                            auto const new_semantic_relations{
                                declarations().semantic_relations.append()};
                            declarations()
                                .semantic_relations.kind[declarations().semantic_relations.index(
                                    new_semantic_relations)] = current_kind;
                            declarations()
                                .semantic_relations.target[declarations().semantic_relations.index(
                                    new_semantic_relations)] = [&] {
                                auto const new_type_refs{declarations().type_refs.append()};
                                declarations()
                                    .type_refs.name[declarations().type_refs.index(new_type_refs)] =
                                    strings().intern(*picked_relationship_target);
                                declarations().type_refs.suffix[declarations().type_refs.index(
                                    new_type_refs)] = {};
                                declarations().type_refs.nested[declarations().type_refs.index(
                                    new_type_refs)] = std::nullopt;
                                return new_type_refs;
                            }();
                            declarations()
                                .semantic_relations.unit[declarations().semantic_relations.index(
                                    new_semantic_relations)] =
                                current_kind == lispb::codegen::SemanticRelationKind::offset_into
                                    ? std::optional{current_unit}
                                    : std::nullopt;
                            return new_semantic_relations;
                        }();
                        return row_0;
                    }();
                    return declarations().range(rows_1);
                }();
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
                    record_editor_declaration_.reset();
                    analysis_session_.inputs.selection.field = member_name;
                    return true;
                }
            }
            ImGui::SameLine();
            if (declarations()
                    .record_members.relationship[declarations().record_members.index(member)]
                    .has_value()) {
                if (ImGui::SmallButton("Clear")) {
                    auto replacement{document_->storage().declarations.records.clone(*schema)};
                    document_->storage().declarations.records.members
                        [document_->storage().declarations.records.index(replacement)] = [&] {
                        auto const original_items{declarations().items(
                            document_->storage().declarations.records.members
                                [document_->storage().declarations.records.index(replacement)])};
                        auto rows_1{std::vector(original_items.begin(), original_items.end())};
                        rows_1[*selected_index] = [&] {
                            auto row_0{
                                declarations().record_members.clone(rows_1[*selected_index])};
                            declarations()
                                .record_members
                                .relationship[declarations().record_members.index(row_0)]
                                .reset();
                            return row_0;
                        }();
                        return declarations().range(rows_1);
                    }();
                    if (apply_document_edit(
                            document_->replace_declaration(*declaration, std::move(replacement)))) {
                        record_editor_declaration_.reset();
                        analysis_session_.inputs.selection.field = member_name;
                        return true;
                    }
                }
                ImGui::SameLine();
                if (resolved_member.has_value() &&
                    lispb::schema::is_valid(
                        definitions()
                            .record_members
                            .relationship[definitions().record_members.index(*resolved_member)]) &&
                    detail::semantic_type_navigation_button()) {
                    navigate_to =
                        analysis_session_.inputs.workspace.types()
                            .occurrences()
                            .type[analysis_session_.inputs.workspace.types().occurrences().index(
                                analysis_session_.inputs.workspace.types()
                                    .relationships()
                                    .target[analysis_session_.inputs.workspace.types()
                                                .relationships()
                                                .index(definitions().record_members.relationship
                                                           [definitions().record_members.index(
                                                               *resolved_member)])])];
                }
            } else {
                ImGui::BeginDisabled(record_relationship_target_.front() == '\0');
                if (ImGui::SmallButton("Add")) {
                    auto replacement{document_->storage().declarations.records.clone(*schema)};
                    document_->storage().declarations.records.members
                        [document_->storage().declarations.records.index(replacement)] = [&] {
                        auto const original_items{declarations().items(
                            document_->storage().declarations.records.members
                                [document_->storage().declarations.records.index(replacement)])};
                        auto rows_1{std::vector(original_items.begin(), original_items.end())};
                        rows_1[*selected_index] = [&] {
                            auto row_0{
                                declarations().record_members.clone(rows_1[*selected_index])};
                            declarations()
                                .record_members
                                .relationship[declarations().record_members.index(row_0)] = [&] {
                                auto const new_semantic_relations{
                                    declarations().semantic_relations.append()};
                                declarations()
                                    .semantic_relations
                                    .kind[declarations().semantic_relations.index(
                                        new_semantic_relations)] = current_kind;
                                declarations()
                                    .semantic_relations
                                    .target[declarations().semantic_relations.index(
                                        new_semantic_relations)] = [&] {
                                    auto const new_type_refs{declarations().type_refs.append()};
                                    declarations().type_refs.name[declarations().type_refs.index(
                                        new_type_refs)] =
                                        strings().intern(record_relationship_target_.data());
                                    declarations().type_refs.suffix[declarations().type_refs.index(
                                        new_type_refs)] = {};
                                    declarations().type_refs.nested[declarations().type_refs.index(
                                        new_type_refs)] = std::nullopt;
                                    return new_type_refs;
                                }();
                                declarations()
                                    .semantic_relations
                                    .unit[declarations().semantic_relations.index(
                                        new_semantic_relations)] =
                                    current_kind ==
                                            lispb::codegen::SemanticRelationKind::offset_into
                                        ? std::optional{current_unit}
                                        : std::nullopt;
                                return new_semantic_relations;
                            }();
                            return row_0;
                        }();
                        return declarations().range(rows_1);
                    }();
                    if (apply_document_edit(
                            document_->replace_declaration(*declaration, std::move(replacement)))) {
                        record_editor_declaration_.reset();
                        analysis_session_.inputs.selection.field = member_name;
                        return true;
                    }
                }
                ImGui::EndDisabled();
            }
            ImGui::TextDisabled("The relationship is durable semantic metadata; record offsets and "
                                "ABI layout still "
                                "come only from the member type, count, and target profile.");
        }
    }

    if (navigate_to.has_value()) {
        select_type(*navigate_to);
        analysis_session_.inputs.selection.field.clear();
        analysis_session_.inputs.selection.record_access_members.clear();
        analysis_session_.inputs.selection.record_access_set_explicit = false;
        return true;
    }

    if (pending.has_value()) {
        auto const invalid_member{std::ranges::find_if(
            declarations().items(
                declarations().records.members[declarations().records.index(*pending)]),
            [&](auto const id) {
                auto const member{id};
                return text(declarations()
                                .record_members.name[declarations().record_members.index(member)])
                           .empty() ||
                       text(declarations().type_refs.name[declarations().type_refs.index(
                                declarations().record_members.type
                                    [declarations().record_members.index(member)])])
                           .empty() ||
                       declarations()
                               .record_members.count[declarations().record_members.index(member)] ==
                           0;
            })};
        if (invalid_member !=
            declarations()
                .items(declarations().records.members[declarations().records.index(*pending)])
                .end()) {
            if (text(declarations()
                         .record_members.name[declarations().record_members.index(*invalid_member)])
                    .empty()) {
                schema_edit_message_ = "Record member name cannot be empty.";
            } else if (text(declarations().type_refs.name[declarations().type_refs.index(
                                declarations().record_members.type
                                    [declarations().record_members.index(*invalid_member)])])
                           .empty()) {
                schema_edit_message_ = "Record member type cannot be empty.";
            } else {
                schema_edit_message_ = "Fixed array count must be greater than zero.";
            }
            return false;
        }
        if (apply_document_edit(
                document_->replace_declaration(*declaration, std::move(*pending)))) {
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
