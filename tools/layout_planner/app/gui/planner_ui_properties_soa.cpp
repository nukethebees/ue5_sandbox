#include "planner_ui_properties_common.hpp"

namespace ioj::layout_planner {

auto PlannerUi::draw_soa_editor(TypeNode const& node, SoaType const& soa) -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const declaration{document_->find_declaration(node.identity)};
    if (!declaration.has_value()) {
        return false;
    }
    auto const* schema{document_->soa_schema(*declaration)};
    if (schema == nullptr) {
        return false;
    }

    if (analysis_session_.inputs.selection.field.empty() && !schema->members.empty()) {
        analysis_session_.inputs.selection.field = schema->members.front().name;
    }
    auto selected{std::ranges::find(schema->members,
                                    analysis_session_.inputs.selection.field,
                                    &codegen::SoaMemberSchema::name)};
    if (selected == schema->members.end() && !schema->members.empty()) {
        selected = schema->members.begin();
        analysis_session_.inputs.selection.field = selected->name;
    }
    auto const selected_index{selected == schema->members.end()
                                  ? std::optional<std::size_t>{}
                                  : std::optional<std::size_t>{static_cast<std::size_t>(
                                        selected - schema->members.begin())}};
    auto const selected_is_mask_storage{selected != schema->members.end() &&
                                        schema->field_mask_name.has_value() &&
                                        selected->type.name == *schema->field_mask_name};
    auto const selected_is_final_mask_field{
        selected != schema->members.end() && selected->mask_field &&
        std::ranges::count_if(schema->members,
                              [](auto const& member) { return member.mask_field; }) == 1};

    if (soa_editor_declaration_ != declaration ||
        soa_editor_member_ != analysis_session_.inputs.selection.field) {
        soa_editor_declaration_ = declaration;
        soa_editor_member_ = analysis_session_.inputs.selection.field;
        std::snprintf(soa_view_name_.data(),
                      soa_view_name_.size(),
                      "%s",
                      schema->view_name.value_or("").c_str());
        std::snprintf(soa_const_view_name_.data(),
                      soa_const_view_name_.size(),
                      "%s",
                      schema->const_view_name.value_or("").c_str());
        std::snprintf(soa_equivalent_type_.data(),
                      soa_equivalent_type_.size(),
                      "%s",
                      schema->equivalent_type.has_value() ? schema->equivalent_type->name.c_str()
                                                          : "");
        std::snprintf(soa_export_specifier_.data(),
                      soa_export_specifier_.size(),
                      "%s",
                      schema->export_specifier.value_or("").c_str());
        soa_using_declarations_.clear();
        soa_using_declarations_.reserve(schema->using_declarations.size());
        for (auto const& using_declaration : schema->using_declarations) {
            std::array<char, 256> declaration_text{};
            std::snprintf(
                declaration_text.data(), declaration_text.size(), "%s", using_declaration.c_str());
            soa_using_declarations_.push_back(declaration_text);
        }
        if (schema->using_declarations.empty()) {
            soa_using_declaration_index_.reset();
        } else if (!soa_using_declaration_index_.has_value() ||
                   *soa_using_declaration_index_ >= schema->using_declarations.size()) {
            soa_using_declaration_index_ = 0;
        }
        soa_function_names_.clear();
        soa_function_return_types_.clear();
        soa_function_names_.reserve(schema->functions.size());
        soa_function_return_types_.reserve(schema->functions.size());
        for (auto const& function : schema->functions) {
            std::array<char, 128> name{};
            std::array<char, 128> return_type{};
            std::snprintf(name.data(), name.size(), "%s", function.name.c_str());
            std::snprintf(
                return_type.data(), return_type.size(), "%s", function.return_type.name.c_str());
            soa_function_names_.push_back(name);
            soa_function_return_types_.push_back(return_type);
        }
        if (schema->functions.empty()) {
            soa_function_index_.reset();
        } else if (!soa_function_index_.has_value() ||
                   *soa_function_index_ >= schema->functions.size()) {
            soa_function_index_ = 0;
        }
        soa_parameter_names_.clear();
        soa_parameter_types_.clear();
        soa_parameter_defaults_.clear();
        if (soa_function_index_.has_value()) {
            auto const& function{schema->functions[*soa_function_index_]};
            auto const& parameters{function.parameters};
            soa_parameter_names_.reserve(parameters.size());
            soa_parameter_types_.reserve(parameters.size());
            soa_parameter_defaults_.reserve(parameters.size());
            for (auto const& parameter : parameters) {
                std::array<char, 128> name{};
                std::array<char, 128> type{};
                std::array<char, 128> default_value{};
                std::snprintf(name.data(), name.size(), "%s", parameter.name.c_str());
                std::snprintf(type.data(), type.size(), "%s", parameter.type.name.c_str());
                std::snprintf(default_value.data(),
                              default_value.size(),
                              "%s",
                              parameter.default_value.value_or("").c_str());
                soa_parameter_names_.push_back(name);
                soa_parameter_types_.push_back(type);
                soa_parameter_defaults_.push_back(default_value);
            }
            if (parameters.empty()) {
                soa_parameter_index_.reset();
            } else if (!soa_parameter_index_.has_value() ||
                       *soa_parameter_index_ >= parameters.size()) {
                soa_parameter_index_ = 0;
            }

            soa_function_body_lines_.clear();
            soa_function_body_lines_.reserve(function.body_lines.size());
            for (auto const& body_line : function.body_lines) {
                soa_function_body_lines_.push_back(body_line);
            }
            if (function.body_lines.empty()) {
                soa_function_body_index_.reset();
            } else if (!soa_function_body_index_.has_value() ||
                       *soa_function_body_index_ >= function.body_lines.size()) {
                soa_function_body_index_ = 0;
            }

            soa_function_dependencies_.clear();
            soa_function_dependencies_.reserve(function.dependencies.size());
            for (auto const& dependency : function.dependencies) {
                soa_function_dependencies_.push_back(dependency);
            }
            if (function.dependencies.empty()) {
                soa_function_dependency_index_.reset();
            } else if (!soa_function_dependency_index_.has_value() ||
                       *soa_function_dependency_index_ >= function.dependencies.size()) {
                soa_function_dependency_index_ = 0;
            }
            soa_function_trailing_return_type_ = function.trailing_return_type.has_value()
                                                   ? function.trailing_return_type->name
                                                   : "";
            soa_function_template_parameters_ = function.template_parameters.value_or("");
            soa_function_requires_clause_ = function.requires_clause.value_or("");
        } else {
            soa_parameter_index_.reset();
            soa_function_body_lines_.clear();
            soa_function_body_index_.reset();
            soa_function_dependencies_.clear();
            soa_function_dependency_index_.reset();
            soa_function_trailing_return_type_.clear();
            soa_function_template_parameters_.clear();
            soa_function_requires_clause_.clear();
        }
        std::snprintf(soa_single_allocation_name_.data(),
                      soa_single_allocation_name_.size(),
                      "%s",
                      schema->single_allocation.value_or("").c_str());
        soa_single_allocation_variant_names_.clear();
        soa_single_allocation_variant_allocators_.clear();
        soa_single_allocation_variant_names_.reserve(schema->single_allocation_variants.size());
        soa_single_allocation_variant_allocators_.reserve(
            schema->single_allocation_variants.size());
        for (auto const& variant : schema->single_allocation_variants) {
            std::array<char, 128> name{};
            std::array<char, 128> allocator{};
            std::snprintf(name.data(), name.size(), "%s", variant.name.c_str());
            std::snprintf(allocator.data(), allocator.size(), "%s", variant.allocator.name.c_str());
            soa_single_allocation_variant_names_.push_back(name);
            soa_single_allocation_variant_allocators_.push_back(allocator);
        }
        if (schema->single_allocation_variants.empty()) {
            soa_single_allocation_variant_index_.reset();
        } else if (!soa_single_allocation_variant_index_.has_value() ||
                   *soa_single_allocation_variant_index_ >=
                       schema->single_allocation_variants.size()) {
            soa_single_allocation_variant_index_ = 0;
        }
        soa_fixed_container_names_.clear();
        if (schema->fixed.has_value()) {
            std::snprintf(soa_fixed_storage_name_.data(),
                          soa_fixed_storage_name_.size(),
                          "%s",
                          schema->fixed->storage_name.c_str());
            soa_fixed_container_names_.reserve(schema->fixed->containers.size());
            for (auto const& container : schema->fixed->containers) {
                std::array<char, 128> name{};
                std::snprintf(name.data(), name.size(), "%s", container.c_str());
                soa_fixed_container_names_.push_back(name);
            }
            if (schema->fixed->containers.empty()) {
                soa_fixed_container_index_.reset();
            } else if (!soa_fixed_container_index_.has_value() ||
                       *soa_fixed_container_index_ >= schema->fixed->containers.size()) {
                soa_fixed_container_index_ = 0;
            }
        } else {
            soa_fixed_storage_name_.front() = '\0';
            soa_fixed_container_index_.reset();
        }
        if (selected != schema->members.end()) {
            std::snprintf(
                soa_member_name_.data(), soa_member_name_.size(), "%s", selected->name.c_str());
            std::snprintf(soa_member_type_.data(),
                          soa_member_type_.size(),
                          "%s",
                          selected->type.name.c_str());
            std::snprintf(soa_member_fixed_schema_.data(),
                          soa_member_fixed_schema_.size(),
                          "%s",
                          selected->fixed_schema.value_or("").c_str());
            std::snprintf(soa_member_nested_schema_.data(),
                          soa_member_nested_schema_.size(),
                          "%s",
                          selected->nested_schema.value_or("").c_str());
            std::snprintf(soa_relationship_target_.data(),
                          soa_relationship_target_.size(),
                          "%s",
                          selected->relationship.has_value()
                              ? selected->relationship->target.name.c_str()
                              : "");
            auto const relationship_kind{
                selected->relationship.has_value()
                    ? std::ranges::find(semantic_relationship_kinds, selected->relationship->kind)
                    : semantic_relationship_kinds.begin()};
            soa_relationship_kind_ =
                relationship_kind == semantic_relationship_kinds.end()
                    ? 0
                    : static_cast<int>(relationship_kind - semantic_relationship_kinds.begin());
            auto const relationship_unit{
                selected->relationship.has_value() && selected->relationship->unit.has_value()
                    ? std::ranges::find(semantic_relationship_units, *selected->relationship->unit)
                    : semantic_relationship_units.begin()};
            soa_relationship_unit_ =
                relationship_unit == semantic_relationship_units.end()
                    ? 0
                    : static_cast<int>(relationship_unit - semantic_relationship_units.begin());
            soa_mask_dimension_names_.clear();
            soa_mask_dimension_extents_.clear();
            soa_mask_dimension_names_.reserve(selected->mask_dimensions.size());
            soa_mask_dimension_extents_.reserve(selected->mask_dimensions.size());
            for (auto const& dimension : selected->mask_dimensions) {
                std::array<char, 128> name{};
                std::array<char, 128> extent{};
                std::snprintf(name.data(), name.size(), "%s", dimension.index_name.c_str());
                std::snprintf(extent.data(), extent.size(), "%s", dimension.extent.c_str());
                soa_mask_dimension_names_.push_back(name);
                soa_mask_dimension_extents_.push_back(extent);
            }
            if (selected->mask_dimensions.empty()) {
                soa_mask_dimension_index_.reset();
            } else if (!soa_mask_dimension_index_.has_value() ||
                       *soa_mask_dimension_index_ >= selected->mask_dimensions.size()) {
                soa_mask_dimension_index_ = 0;
            }
        }
    }

    if (ImGui::Button("+ Column")) {
        auto replacement{*schema};
        auto name{unique_member_name(replacement.members, "column")};
        replacement.members.push_back(codegen::SoaMemberSchema{
            .name = name,
            .kind = codegen::SoaMemberKind::array,
            .type = codegen::TypeRef{.name = "std::uint32_t", .suffix = {}, .nested = std::nullopt},
            .fixed_schema = std::nullopt,
            .nested_schema = std::nullopt,
            .mask_field = false,
            .mask_dimensions = {},
            .relationship = std::nullopt});
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
            analysis_session_.inputs.selection.field = std::move(name);
            return true;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() || selected_is_mask_storage);
    if (ImGui::Button("Duplicate")) {
        auto replacement{*schema};
        auto copy{replacement.members[*selected_index]};
        copy.name = unique_member_name(replacement.members, copy.name + "_copy");
        replacement.members.insert(
            replacement.members.begin() + static_cast<std::ptrdiff_t>(*selected_index + 1), copy);
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
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
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
            analysis_session_.inputs.selection.field = soa_editor_member_;
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
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
            analysis_session_.inputs.selection.field = soa_editor_member_;
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() || schema->members.size() == 1 ||
                         selected_is_mask_storage || selected_is_final_mask_field);
    if (ImGui::Button("Delete")) {
        auto replacement{*schema};
        auto const deleted_name{replacement.members[*selected_index].name};
        replacement.members.erase(replacement.members.begin() +
                                  static_cast<std::ptrdiff_t>(*selected_index));
        auto const next_index{std::min(*selected_index, replacement.members.size() - 1)};
        auto const next_name{replacement.members[next_index].name};
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
            analysis_session_.inputs.selection.soa_access_columns.erase(deleted_name);
            analysis_session_.inputs.selection.field = next_name;
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Selected only")) {
        analysis_session_.inputs.selection.soa_access_columns.clear();
        analysis_session_.inputs.selection.soa_access_set_explicit = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Access all")) {
        analysis_session_.inputs.selection.soa_access_columns.clear();
        for (auto const& member : schema->members) {
            analysis_session_.inputs.selection.soa_access_columns.insert_or_assign(
                member.name, analysis_session_.inputs.access_operation);
        }
        analysis_session_.inputs.selection.soa_access_set_explicit = true;
    }

    std::optional<codegen::SoaSchema> pending;
    std::optional<TypeId> navigate_to;
    std::optional<std::pair<std::string, std::string>> renamed_member;
    auto selected_after_edit{analysis_session_.inputs.selection.field};
    if (detail::begin_editable_table("soa-schema-members", 6, schema->members.size())) {
        detail::editable_table_column("Edit", ImGuiTableColumnFlags_WidthFixed);
        detail::editable_table_column("Access", ImGuiTableColumnFlags_WidthFixed);
        detail::editable_table_column("Operation", ImGuiTableColumnFlags_WidthFixed);
        detail::editable_table_column("Name");
        detail::editable_table_column("Semantic type");
        detail::editable_table_column("Kind");
        ImGui::TableHeadersRow();
        for (std::size_t index{}; index < schema->members.size(); ++index) {
            auto const& member{schema->members[index]};
            auto const row_selected{analysis_session_.inputs.selection.field == member.name};
            ImGui::PushID(static_cast<int>(index));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (detail::editable_table_row_handle(row_selected)) {
                analysis_session_.inputs.selection.field = member.name;
                soa_editor_declaration_.reset();
            }
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("SOA_MEMBER_ROW", &index, sizeof(index));
                ImGui::Text("Move %s", member.name.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget()) {
                if (auto const* payload{ImGui::AcceptDragDropPayload("SOA_MEMBER_ROW")}) {
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
                analysis_session_.inputs.selection.soa_access_set_explicit
                    ? analysis_session_.inputs.selection.soa_access_columns.contains(member.name)
                    : analysis_session_.inputs.selection.field == member.name};
            if (ImGui::Checkbox("##access", &accessed)) {
                if (!analysis_session_.inputs.selection.soa_access_set_explicit) {
                    analysis_session_.inputs.selection.soa_access_columns.clear();
                    if (!analysis_session_.inputs.selection.field.empty()) {
                        analysis_session_.inputs.selection.soa_access_columns.insert_or_assign(
                            analysis_session_.inputs.selection.field,
                            analysis_session_.inputs.access_operation);
                    }
                    analysis_session_.inputs.selection.soa_access_set_explicit = true;
                }
                if (accessed) {
                    analysis_session_.inputs.selection.soa_access_columns.insert_or_assign(
                        member.name, analysis_session_.inputs.access_operation);
                } else {
                    analysis_session_.inputs.selection.soa_access_columns.erase(member.name);
                }
            }

            ImGui::TableNextColumn();
            if (accessed) {
                auto operation{analysis_session_.inputs.access_operation};
                if (analysis_session_.inputs.selection.soa_access_set_explicit) {
                    if (auto const found{analysis_session_.inputs.selection.soa_access_columns.find(
                            member.name)};
                        found != analysis_session_.inputs.selection.soa_access_columns.end()) {
                        operation = found->second;
                    }
                }
                auto operation_index{static_cast<int>(operation)};
                ImGui::SetNextItemWidth(105.0F);
                if (ImGui::Combo(
                        "##access-operation", &operation_index, "Read\0Write\0Read + write\0")) {
                    if (!analysis_session_.inputs.selection.soa_access_set_explicit) {
                        analysis_session_.inputs.selection.soa_access_columns.clear();
                        if (!analysis_session_.inputs.selection.field.empty()) {
                            analysis_session_.inputs.selection.soa_access_columns.insert_or_assign(
                                analysis_session_.inputs.selection.field,
                                analysis_session_.inputs.access_operation);
                        }
                        analysis_session_.inputs.selection.soa_access_set_explicit = true;
                    }
                    analysis_session_.inputs.selection.soa_access_columns.insert_or_assign(
                        member.name, static_cast<AccessOperation>(operation_index));
                }
            } else {
                ImGui::TextDisabled("-");
            }

            ImGui::TableNextColumn();
            auto const row_mask_storage{schema->field_mask_name.has_value() &&
                                        member.type.name == *schema->field_mask_name};
            if (row_selected) {
                ImGui::SetNextItemWidth(-1.0F);
                auto const submitted{ImGui::InputText("##name",
                                                      soa_member_name_.data(),
                                                      soa_member_name_.size(),
                                                      ImGuiInputTextFlags_EnterReturnsTrue)};
                if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                    pending = *schema;
                    pending->members[index].name = soa_member_name_.data();
                    selected_after_edit = pending->members[index].name;
                    renamed_member = std::pair{member.name, selected_after_edit};
                }
            } else {
                ImGui::TextUnformatted(member.name.c_str());
            }

            ImGui::TableNextColumn();
            if (row_selected && !row_mask_storage) {
                auto const type_controls_inline{detail::prepare_editable_type_input()};
                auto const submitted{ImGui::InputText("##type",
                                                      soa_member_type_.data(),
                                                      soa_member_type_.size(),
                                                      ImGuiInputTextFlags_EnterReturnsTrue)};
                if (!pending.has_value() && (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                    pending = *schema;
                    pending->members[index].type.name = soa_member_type_.data();
                }
                if (type_controls_inline) {
                    ImGui::SameLine();
                }
                if (auto picked{draw_type_picker(node.identity.module_name, node.identity)}) {
                    pending = *schema;
                    pending->members[index].type.name = std::move(*picked);
                }
                ImGui::SameLine();
                if (index < soa.columns.size() && detail::semantic_type_navigation_button()) {
                    navigate_to = soa.columns[index].semantic_type.type;
                }
                detail::editable_table_content_hint(soa_member_type_.data(), 80.0F);
            } else {
                ImGui::TextUnformatted(member.type.name.c_str());
            }

            ImGui::TableNextColumn();
            auto const kind_label{member.kind == codegen::SoaMemberKind::nested ? "nested"
                                                                                : "array"};
            if (row_selected && ImGui::BeginCombo("##kind", kind_label)) {
                if (ImGui::Selectable("array", member.kind == codegen::SoaMemberKind::array)) {
                    pending = *schema;
                    pending->members[index].kind = codegen::SoaMemberKind::array;
                    pending->members[index].fixed_schema.reset();
                    pending->members[index].nested_schema.reset();
                    soa_member_fixed_schema_.front() = '\0';
                    soa_member_nested_schema_.front() = '\0';
                }
                auto const mask_storage{schema->field_mask_name.has_value() &&
                                        member.type.name == *schema->field_mask_name};
                ImGui::BeginDisabled(member.mask_field || mask_storage);
                if (ImGui::Selectable("nested", member.kind == codegen::SoaMemberKind::nested)) {
                    pending = *schema;
                    pending->members[index].kind = codegen::SoaMemberKind::nested;
                }
                ImGui::EndDisabled();
                ImGui::EndCombo();
            } else if (!row_selected) {
                ImGui::TextUnformatted(kind_label);
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (selected_index.has_value()) {
        auto const& member{schema->members[*selected_index]};
        auto const* resolved_column{
            *selected_index < soa.columns.size() ? &soa.columns[*selected_index] : nullptr};
        ImGui::SeparatorText("Selected column details");
        if (member.kind == codegen::SoaMemberKind::nested) {
            ImGui::SetNextItemWidth(-1.0F);
            auto const fixed_submitted{ImGui::InputText("Fixed schema (optional)",
                                                        soa_member_fixed_schema_.data(),
                                                        soa_member_fixed_schema_.size(),
                                                        ImGuiInputTextFlags_EnterReturnsTrue)};
            if (fixed_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                if (!pending.has_value()) {
                    pending = *schema;
                }
                auto value{std::string{soa_member_fixed_schema_.data()}};
                if (value.empty()) {
                    pending->members[*selected_index].fixed_schema.reset();
                } else {
                    pending->members[*selected_index].fixed_schema = std::move(value);
                }
            }

            ImGui::SetNextItemWidth(-1.0F);
            auto const nested_submitted{ImGui::InputText("Nested schema (optional)",
                                                         soa_member_nested_schema_.data(),
                                                         soa_member_nested_schema_.size(),
                                                         ImGuiInputTextFlags_EnterReturnsTrue)};
            if (nested_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                if (!pending.has_value()) {
                    pending = *schema;
                }
                auto value{std::string{soa_member_nested_schema_.data()}};
                if (value.empty()) {
                    pending->members[*selected_index].nested_schema.reset();
                } else {
                    pending->members[*selected_index].nested_schema = std::move(value);
                }
            }
        } else {
            ImGui::TextDisabled("Fixed and nested schema references apply only to nested columns.");
        }

        if (!pending.has_value()) {
            auto record_module_index{std::optional<std::size_t>{}};
            auto fallback_record_module_index{std::optional<std::size_t>{}};
            auto const& modules{document_->manifest().modules};
            for (std::size_t module_index{}; module_index < modules.size(); ++module_index) {
                auto const* record_module{
                    std::get_if<codegen::RecordModuleSchema>(&modules[module_index])};
                if (record_module == nullptr) {
                    continue;
                }
                if (!fallback_record_module_index.has_value()) {
                    fallback_record_module_index = module_index;
                }
                if (record_module->settings.namespace_name.value_or("") ==
                    node.identity.namespace_name) {
                    record_module_index = module_index;
                    break;
                }
            }
            if (!record_module_index.has_value()) {
                record_module_index = fallback_record_module_index;
            }
            auto const mask_storage{schema->field_mask_name.has_value() &&
                                    member.type.name == *schema->field_mask_name};
            auto const can_create_record{record_module_index.has_value() &&
                                         resolved_column != nullptr && !mask_storage &&
                                         member.kind == codegen::SoaMemberKind::array};
            ImGui::BeginDisabled(!can_create_record);
            if (ImGui::Button("Create record for selected column...")) {
                new_record_module_index_ = *record_module_index;
                auto const suggested_name{suggested_type_name(member.name, "Record")};
                auto& record_module{
                    std::get<codegen::RecordModuleSchema>(modules[new_record_module_index_])};
                auto unique_name{suggested_name};
                auto suffix_number{std::size_t{1}};
                while (std::ranges::find(
                           record_module.records, unique_name, &codegen::RecordSchema::name) !=
                       record_module.records.end()) {
                    unique_name = suggested_name + std::to_string(suffix_number++);
                }
                std::snprintf(
                    new_record_name_.data(), new_record_name_.size(), "%s", unique_name.c_str());
                auto first_member_type{member.type.name};
                auto const& current_type{analysis_session_.inputs.workspace.types().type(
                    resolved_column->semantic_type.type)};
                if (!first_member_type.starts_with('@') &&
                    current_type.identity.origin == TypeOrigin::declaration) {
                    first_member_type = current_type.cpp_spelling;
                }
                std::snprintf(new_record_member_type_.data(),
                              new_record_member_type_.size(),
                              "%s",
                              first_member_type.c_str());
                pending_soa_record_binding_ = PendingSoaRecordBinding{
                    .soa_declaration = *declaration, .column_name = member.name};
                module_initiated_dialog_ = false;
                open_new_record_dialog_ = true;
            }
            ImGui::EndDisabled();
            if (!record_module_index.has_value()) {
                ImGui::SameLine();
                ImGui::TextDisabled("No record module is available.");
            } else if (resolved_column == nullptr) {
                ImGui::SameLine();
                ImGui::TextDisabled("The selected column type is unresolved.");
            } else if (mask_storage || member.kind != codegen::SoaMemberKind::array) {
                ImGui::SameLine();
                ImGui::TextDisabled("Available for ordinary array columns.");
            } else {
                ImGui::SameLine();
                ImGui::TextDisabled("Creates a shared record, then binds this column.");
            }

            auto const current_kind{semantic_relationship_kinds[static_cast<std::size_t>(
                std::clamp(soa_relationship_kind_,
                           0,
                           static_cast<int>(semantic_relationship_kinds.size() - 1)))]};
            auto const current_unit{semantic_relationship_units[static_cast<std::size_t>(
                std::clamp(soa_relationship_unit_,
                           0,
                           static_cast<int>(semantic_relationship_units.size() - 1)))]};
            separator_text_with_tooltip(
                "Selected column relationship",
                "Describe how this SoA column relates to another semantic type.");
            ImGui::SetNextItemWidth(180.0F);
            if (ImGui::BeginCombo("Kind##soa-column-relationship",
                                  codegen::semantic_relation_kind_name(current_kind).data())) {
                for (std::size_t kind_index{}; kind_index < semantic_relationship_kinds.size();
                     ++kind_index) {
                    auto const kind{semantic_relationship_kinds[kind_index]};
                    auto const chosen{soa_relationship_kind_ == static_cast<int>(kind_index)};
                    if (ImGui::Selectable(codegen::semantic_relation_kind_name(kind).data(),
                                          chosen)) {
                        soa_relationship_kind_ = static_cast<int>(kind_index);
                        if (member.relationship.has_value()) {
                            auto replacement{*schema};
                            replacement.members[*selected_index].relationship->kind = kind;
                            replacement.members[*selected_index].relationship->unit =
                                kind == codegen::SemanticRelationKind::offset_into
                                    ? std::optional{current_unit}
                                    : std::nullopt;
                            if (apply_document_edit(ReplaceSoa{.declaration = *declaration,
                                                               .schema = std::move(replacement)})) {
                                soa_editor_declaration_.reset();
                                analysis_session_.inputs.selection.field = member.name;
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
                if (ImGui::BeginCombo("Unit##soa-column-relationship",
                                      codegen::semantic_relation_unit_name(current_unit).data())) {
                    for (std::size_t unit_index{}; unit_index < semantic_relationship_units.size();
                         ++unit_index) {
                        auto const unit{semantic_relationship_units[unit_index]};
                        auto const chosen{soa_relationship_unit_ == static_cast<int>(unit_index)};
                        if (ImGui::Selectable(codegen::semantic_relation_unit_name(unit).data(),
                                              chosen)) {
                            soa_relationship_unit_ = static_cast<int>(unit_index);
                            if (member.relationship.has_value()) {
                                auto replacement{*schema};
                                replacement.members[*selected_index].relationship->unit = unit;
                                if (apply_document_edit(
                                        ReplaceSoa{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
                                    soa_editor_declaration_.reset();
                                    analysis_session_.inputs.selection.field = member.name;
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
            auto const target_submitted{ImGui::InputText("Target##soa-column-relationship",
                                                         soa_relationship_target_.data(),
                                                         soa_relationship_target_.size(),
                                                         ImGuiInputTextFlags_EnterReturnsTrue)};
            if (member.relationship.has_value() &&
                (target_submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                if (soa_relationship_target_.front() == '\0') {
                    schema_edit_message_ = "Relationship target cannot be empty.";
                } else {
                    auto replacement{*schema};
                    replacement.members[*selected_index].relationship->target.name =
                        soa_relationship_target_.data();
                    if (apply_document_edit(ReplaceSoa{.declaration = *declaration,
                                                       .schema = std::move(replacement)})) {
                        soa_editor_declaration_.reset();
                        analysis_session_.inputs.selection.field = member.name;
                        return true;
                    }
                }
            }
            ImGui::SameLine();
            ImGui::PushID("soa-column-relationship-target");
            auto picked_relationship_target{
                draw_type_picker(node.identity.module_name, node.identity)};
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
                if (apply_document_edit(ReplaceSoa{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
                    soa_editor_declaration_.reset();
                    analysis_session_.inputs.selection.field = member.name;
                    return true;
                }
            }
            ImGui::SameLine();
            if (member.relationship.has_value()) {
                if (ImGui::SmallButton("Clear##soa-column-relationship")) {
                    auto replacement{*schema};
                    replacement.members[*selected_index].relationship.reset();
                    if (apply_document_edit(ReplaceSoa{.declaration = *declaration,
                                                       .schema = std::move(replacement)})) {
                        soa_editor_declaration_.reset();
                        analysis_session_.inputs.selection.field = member.name;
                        return true;
                    }
                }
                ImGui::SameLine();
                if (resolved_column != nullptr && resolved_column->relationship.has_value() &&
                    ImGui::SmallButton(">##soa-column-relationship")) {
                    navigate_to = resolved_column->relationship->target.type;
                }
            } else {
                ImGui::BeginDisabled(soa_relationship_target_.front() == '\0');
                if (ImGui::SmallButton("Add##soa-column-relationship")) {
                    auto replacement{*schema};
                    replacement.members[*selected_index].relationship =
                        codegen::SemanticRelationSchema{
                            .kind = current_kind,
                            .target = codegen::TypeRef{.name = soa_relationship_target_.data(),
                                                       .suffix = {},
                                                       .nested = std::nullopt},
                            .unit = current_kind == codegen::SemanticRelationKind::offset_into
                                      ? std::optional{current_unit}
                                      : std::nullopt};
                    if (apply_document_edit(ReplaceSoa{.declaration = *declaration,
                                                       .schema = std::move(replacement)})) {
                        soa_editor_declaration_.reset();
                        analysis_session_.inputs.selection.field = member.name;
                        return true;
                    }
                }
                ImGui::EndDisabled();
            }
            ImGui::TextDisabled(
                "The relationship is durable semantic metadata; SoA storage, capacity, and "
                "allocator placement remain separate physical/session concerns.");
        }

        auto const mask_configured{schema->field_mask_name.has_value() &&
                                   schema->field_enum_name.has_value()};
        if (!mask_configured) {
            auto const eligible{member.kind == codegen::SoaMemberKind::array};
            ImGui::BeginDisabled(!eligible);
            if (ImGui::Button("Enable generated field mask for this column")) {
                auto replacement{*schema};
                replacement.field_mask_name = schema->name + "FieldMask";
                replacement.field_enum_name = schema->name + "Field";
                replacement.members[*selected_index].mask_field = true;
                auto const storage_name{unique_member_name(replacement.members, "field_mask")};
                replacement.members.push_back(codegen::SoaMemberSchema{
                    .name = storage_name,
                    .kind = codegen::SoaMemberKind::array,
                    .type = codegen::TypeRef{.name = *replacement.field_mask_name,
                                             .suffix = {},
                                             .nested = std::nullopt},
                    .fixed_schema = std::nullopt,
                    .nested_schema = std::nullopt,
                    .mask_field = false,
                    .mask_dimensions = {},
                    .relationship = std::nullopt});
                if (apply_document_edit(ReplaceSoa{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
                    analysis_session_.inputs.selection.field = member.name;
                    return true;
                }
            }
            ImGui::EndDisabled();
            if (!eligible) {
                ImGui::TextDisabled("Generated mask fields must be array columns.");
            }
        } else {
            ImGui::Text("Generated mask: %s", schema->field_mask_name->c_str());
            ImGui::Text("Generated field enum: %s", schema->field_enum_name->c_str());
            auto const storage_member{member.type.name == *schema->field_mask_name};
            auto const mask_field_count{std::ranges::count_if(
                schema->members, [](auto const& candidate) { return candidate.mask_field; })};
            auto included{member.mask_field};
            auto const can_toggle{member.kind == codegen::SoaMemberKind::array && !storage_member &&
                                  (!included || mask_field_count > 1)};
            ImGui::BeginDisabled(!can_toggle);
            if (ImGui::Checkbox("Included in generated field mask", &included)) {
                auto replacement{*schema};
                replacement.members[*selected_index].mask_field = included;
                if (!included) {
                    replacement.members[*selected_index].mask_dimensions.clear();
                }
                if (apply_document_edit(ReplaceSoa{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
                    soa_editor_declaration_.reset();
                    return true;
                }
            }
            ImGui::EndDisabled();
            if (storage_member) {
                ImGui::TextDisabled("This is the generated mask storage column.");
            } else if (member.kind != codegen::SoaMemberKind::array) {
                ImGui::TextDisabled("Generated mask fields must be array columns.");
            } else if (member.mask_field && mask_field_count == 1) {
                ImGui::TextDisabled("At least one generated mask field is required.");
            }

            if (ImGui::Button("Disable generated field mask")) {
                auto replacement{*schema};
                auto const mask_type{*replacement.field_mask_name};
                auto selected_after_disable{analysis_session_.inputs.selection.field};
                auto const selected_is_storage{member.type.name == mask_type};
                std::erase_if(replacement.members, [&](auto const& candidate) {
                    return candidate.type.name == mask_type;
                });
                for (auto& candidate : replacement.members) {
                    candidate.mask_field = false;
                    candidate.mask_dimensions.clear();
                }
                replacement.field_mask_name.reset();
                replacement.field_enum_name.reset();
                if (selected_is_storage && !replacement.members.empty()) {
                    selected_after_disable = replacement.members.front().name;
                }
                if (apply_document_edit(ReplaceSoa{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
                    analysis_session_.inputs.selection.field = std::move(selected_after_disable);
                    return true;
                }
            }
        }

        if (member.mask_field) {
            ImGui::SeparatorText("Mask dimensions");
            if (ImGui::Button("+ Dimension")) {
                auto replacement{*schema};
                auto& dimensions{replacement.members[*selected_index].mask_dimensions};
                auto const name{unique_mask_dimension_name(dimensions, "index")};
                dimensions.push_back(
                    codegen::SoaMaskDimensionSchema{.index_name = name, .extent = "1"});
                auto const new_index{dimensions.size() - 1};
                if (apply_document_edit(ReplaceSoa{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
                    soa_mask_dimension_index_ = new_index;
                    soa_editor_declaration_.reset();
                    return true;
                }
            }
            ImGui::SameLine();
            auto const dimension_index{soa_mask_dimension_index_};
            auto const has_dimension{dimension_index.has_value() &&
                                     *dimension_index < member.mask_dimensions.size()};
            ImGui::BeginDisabled(!has_dimension);
            if (ImGui::Button("Duplicate dimension")) {
                auto replacement{*schema};
                auto& dimensions{replacement.members[*selected_index].mask_dimensions};
                auto copy{dimensions[*dimension_index]};
                copy.index_name = unique_mask_dimension_name(dimensions, copy.index_name + "_copy");
                dimensions.insert(
                    dimensions.begin() + static_cast<std::ptrdiff_t>(*dimension_index + 1), copy);
                if (apply_document_edit(ReplaceSoa{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
                    soa_mask_dimension_index_ = *dimension_index + 1;
                    soa_editor_declaration_.reset();
                    return true;
                }
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(!has_dimension || *dimension_index == 0);
            if (ImGui::Button("Dimension up")) {
                auto replacement{*schema};
                auto& dimensions{replacement.members[*selected_index].mask_dimensions};
                std::swap(dimensions[*dimension_index], dimensions[*dimension_index - 1]);
                if (apply_document_edit(ReplaceSoa{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
                    soa_mask_dimension_index_ = *dimension_index - 1;
                    soa_editor_declaration_.reset();
                    return true;
                }
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(!has_dimension ||
                                 *dimension_index + 1 >= member.mask_dimensions.size());
            if (ImGui::Button("Dimension down")) {
                auto replacement{*schema};
                auto& dimensions{replacement.members[*selected_index].mask_dimensions};
                std::swap(dimensions[*dimension_index], dimensions[*dimension_index + 1]);
                if (apply_document_edit(ReplaceSoa{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
                    soa_mask_dimension_index_ = *dimension_index + 1;
                    soa_editor_declaration_.reset();
                    return true;
                }
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Delete dimension")) {
                auto replacement{*schema};
                auto& dimensions{replacement.members[*selected_index].mask_dimensions};
                dimensions.erase(dimensions.begin() +
                                 static_cast<std::ptrdiff_t>(*dimension_index));
                auto const next_index{
                    dimensions.empty()
                        ? std::optional<std::size_t>{}
                        : std::optional{std::min(*dimension_index, dimensions.size() - 1)}};
                if (apply_document_edit(ReplaceSoa{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
                    soa_mask_dimension_index_ = next_index;
                    soa_editor_declaration_.reset();
                    return true;
                }
            }
            ImGui::EndDisabled();

            if (soa_mask_dimension_names_.size() == member.mask_dimensions.size() &&
                soa_mask_dimension_extents_.size() == member.mask_dimensions.size() &&
                detail::begin_editable_table(
                    "soa-mask-dimensions", 3, member.mask_dimensions.size())) {
                detail::editable_table_column("Edit", ImGuiTableColumnFlags_WidthFixed);
                detail::editable_table_column("Index name");
                detail::editable_table_column("Extent");
                ImGui::TableHeadersRow();
                for (std::size_t index{}; index < member.mask_dimensions.size(); ++index) {
                    ImGui::PushID(static_cast<int>(index));
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    auto const row_selected{soa_mask_dimension_index_ == index};
                    if (detail::editable_table_row_handle(row_selected)) {
                        soa_mask_dimension_index_ = index;
                    }
                    if (ImGui::BeginDragDropSource()) {
                        ImGui::SetDragDropPayload("SOA_MASK_DIMENSION_ROW", &index, sizeof(index));
                        ImGui::Text("Move %s", member.mask_dimensions[index].index_name.c_str());
                        ImGui::EndDragDropSource();
                    }
                    if (ImGui::BeginDragDropTarget()) {
                        if (auto const* payload{
                                ImGui::AcceptDragDropPayload("SOA_MASK_DIMENSION_ROW")}) {
                            auto const source_index{
                                *static_cast<std::size_t const*>(payload->Data)};
                            if (source_index < member.mask_dimensions.size() &&
                                source_index != index) {
                                pending = *schema;
                                move_element(pending->members[*selected_index].mask_dimensions,
                                             source_index,
                                             index);
                                soa_mask_dimension_index_ = index;
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }

                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-1.0F);
                    auto const name_submitted{
                        ImGui::InputText("##name",
                                         soa_mask_dimension_names_[index].data(),
                                         soa_mask_dimension_names_[index].size(),
                                         ImGuiInputTextFlags_EnterReturnsTrue)};
                    if (name_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                        if (!pending.has_value()) {
                            pending = *schema;
                        }
                        pending->members[*selected_index].mask_dimensions[index].index_name =
                            soa_mask_dimension_names_[index].data();
                    }

                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-1.0F);
                    auto const extent_submitted{
                        ImGui::InputText("##extent",
                                         soa_mask_dimension_extents_[index].data(),
                                         soa_mask_dimension_extents_[index].size(),
                                         ImGuiInputTextFlags_EnterReturnsTrue)};
                    if (extent_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                        if (!pending.has_value()) {
                            pending = *schema;
                        }
                        pending->members[*selected_index].mask_dimensions[index].extent =
                            soa_mask_dimension_extents_[index].data();
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }
    }

    ImGui::SeparatorText("Storage operations");
    auto const all_storage_operations{codegen::all_storage_operations()};
    auto const all_operations_enabled{
        std::ranges::all_of(all_storage_operations, [&](auto const operation) {
            return has_storage_operation(schema->operations, operation);
        })};
    ImGui::BeginDisabled(pending.has_value() || all_operations_enabled);
    auto const enable_all_operations{ImGui::Button("Enable all operations")};
    ImGui::EndDisabled();
    if (enable_all_operations) {
        auto replacement{*schema};
        replacement.operations = all_storage_operations;
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
            return true;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(pending.has_value() || schema->operations.empty());
    auto const disable_all_operations{ImGui::Button("Disable all operations")};
    ImGui::EndDisabled();
    if (disable_all_operations) {
        auto replacement{*schema};
        replacement.operations.clear();
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
            return true;
        }
    }
    auto operation_applied{false};
    ImGui::BeginDisabled(pending.has_value());
    if (ImGui::BeginTable("soa-storage-operations", 2, ImGuiTableFlags_SizingStretchSame)) {
        for (auto const operation : all_storage_operations) {
            auto const descriptor{storage_operation_descriptor(operation)};
            ImGui::TableNextColumn();
            auto enabled{has_storage_operation(schema->operations, operation)};
            auto const toggled{ImGui::Checkbox(descriptor.source_name, &enabled)};
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", descriptor.description);
            }
            if (toggled) {
                auto replacement{*schema};
                replacement.operations =
                    with_storage_operation(schema->operations, operation, enabled);
                if (apply_document_edit(ReplaceSoa{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
                    operation_applied = true;
                    break;
                }
            }
        }
        ImGui::EndTable();
    }
    ImGui::EndDisabled();
    if (operation_applied) {
        return true;
    }

    ImGui::SeparatorText("Generation policy");
    ImGui::SetNextItemWidth(-1.0F);
    auto const export_submitted{ImGui::InputText("Export specifier (optional)",
                                                 soa_export_specifier_.data(),
                                                 soa_export_specifier_.size(),
                                                 ImGuiInputTextFlags_EnterReturnsTrue)};
    if (export_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
        if (!pending.has_value()) {
            pending = *schema;
        }
        pending->export_specifier = optional_text(soa_export_specifier_);
    }

    ImGui::TextUnformatted("Equivalent row type (optional)");
    ImGui::SetNextItemWidth(std::max(60.0F, ImGui::GetContentRegionAvail().x - 58.0F));
    auto const equivalent_submitted{ImGui::InputText("##soa-equivalent-type",
                                                     soa_equivalent_type_.data(),
                                                     soa_equivalent_type_.size(),
                                                     ImGuiInputTextFlags_EnterReturnsTrue)};
    if (equivalent_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
        if (!pending.has_value()) {
            pending = *schema;
        }
        if (soa_equivalent_type_.front() == '\0') {
            pending->equivalent_type.reset();
        } else if (pending->equivalent_type.has_value()) {
            pending->equivalent_type->name = soa_equivalent_type_.data();
        } else {
            pending->equivalent_type = codegen::TypeRef{
                .name = soa_equivalent_type_.data(), .suffix = {}, .nested = std::nullopt};
        }
    }
    ImGui::SameLine();
    ImGui::PushID("soa-equivalent-type");
    if (auto picked{draw_type_picker(node.identity.module_name, node.identity)}) {
        if (!pending.has_value()) {
            pending = *schema;
        }
        if (pending->equivalent_type.has_value()) {
            pending->equivalent_type->name = *picked;
        } else {
            pending->equivalent_type =
                codegen::TypeRef{.name = *picked, .suffix = {}, .nested = std::nullopt};
        }
    }
    ImGui::PopID();
    ImGui::SameLine();
    ImGui::BeginDisabled(pending.has_value() || !soa.equivalent_type.has_value());
    if (ImGui::SmallButton(">##equivalent-type")) {
        navigate_to = soa.equivalent_type->type;
    }
    ImGui::EndDisabled();

    auto layout_only{schema->layout_only};
    ImGui::BeginDisabled(pending.has_value());
    auto const layout_only_toggled{ImGui::Checkbox("Layout only", &layout_only)};
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Keep this declaration available as a nested layout without emitting its "
                          "standalone storage/view API.");
    }
    ImGui::EndDisabled();
    if (layout_only_toggled) {
        auto replacement{*schema};
        replacement.layout_only = layout_only;
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
            return true;
        }
    }
    auto copy_element_memberwise{schema->copy_element_memberwise};
    ImGui::BeginDisabled(pending.has_value());
    auto const copy_memberwise_toggled{
        ImGui::Checkbox("Copy elements memberwise", &copy_element_memberwise)};
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Generate direct member assignment for copy-element; this has no effect "
                          "unless the copy-element storage operation is enabled.");
    }
    ImGui::EndDisabled();
    if (copy_memberwise_toggled) {
        auto replacement{*schema};
        replacement.copy_element_memberwise = copy_element_memberwise;
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
            return true;
        }
    }

    ImGui::SeparatorText("Using declarations");
    ImGui::BeginDisabled(pending.has_value());
    auto const add_using_declaration{ImGui::Button("+ Using declaration")};
    ImGui::EndDisabled();
    if (add_using_declaration) {
        auto replacement{*schema};
        replacement.using_declarations.push_back(
            unique_soa_using_declaration(replacement.using_declarations));
        auto const new_index{replacement.using_declarations.size() - 1};
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
            soa_using_declaration_index_ = new_index;
            soa_editor_declaration_.reset();
            return true;
        }
    }
    ImGui::SameLine();
    auto const using_index{soa_using_declaration_index_};
    auto const has_using{using_index.has_value() &&
                         *using_index < schema->using_declarations.size()};
    ImGui::BeginDisabled(pending.has_value() || !has_using || *using_index == 0);
    auto const move_using_up{ImGui::Button("Using up")};
    ImGui::EndDisabled();
    if (move_using_up) {
        auto replacement{*schema};
        std::swap(replacement.using_declarations[*using_index],
                  replacement.using_declarations[*using_index - 1]);
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
            soa_using_declaration_index_ = *using_index - 1;
            soa_editor_declaration_.reset();
            return true;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(pending.has_value() || !has_using ||
                         *using_index + 1 >= schema->using_declarations.size());
    auto const move_using_down{ImGui::Button("Using down")};
    ImGui::EndDisabled();
    if (move_using_down) {
        auto replacement{*schema};
        std::swap(replacement.using_declarations[*using_index],
                  replacement.using_declarations[*using_index + 1]);
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
            soa_using_declaration_index_ = *using_index + 1;
            soa_editor_declaration_.reset();
            return true;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(pending.has_value() || !has_using);
    auto const delete_using{ImGui::Button("Delete using")};
    ImGui::EndDisabled();
    if (delete_using) {
        auto replacement{*schema};
        replacement.using_declarations.erase(replacement.using_declarations.begin() +
                                             static_cast<std::ptrdiff_t>(*using_index));
        auto const next_index{
            replacement.using_declarations.empty()
                ? std::optional<std::size_t>{}
                : std::optional{std::min(*using_index, replacement.using_declarations.size() - 1)}};
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
            soa_using_declaration_index_ = next_index;
            soa_editor_declaration_.reset();
            return true;
        }
    }

    ImGui::BeginDisabled(pending.has_value());
    if (soa_using_declarations_.size() == schema->using_declarations.size() &&
        detail::begin_editable_table(
            "soa-using-declarations", 2, schema->using_declarations.size())) {
        detail::editable_table_column("Edit", ImGuiTableColumnFlags_WidthFixed);
        detail::editable_table_column("Declaration after 'using'");
        ImGui::TableHeadersRow();
        for (std::size_t index{}; index < schema->using_declarations.size(); ++index) {
            ImGui::PushID(static_cast<int>(index));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            auto const row_selected{soa_using_declaration_index_ == index};
            if (detail::editable_table_row_handle(row_selected)) {
                soa_using_declaration_index_ = index;
            }
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("SOA_USING_DECLARATION_ROW", &index, sizeof(index));
                ImGui::Text("Move %s", schema->using_declarations[index].c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget()) {
                if (auto const* payload{
                        ImGui::AcceptDragDropPayload("SOA_USING_DECLARATION_ROW")}) {
                    auto const source_index{*static_cast<std::size_t const*>(payload->Data)};
                    if (source_index < schema->using_declarations.size() && source_index != index) {
                        pending = *schema;
                        move_element(pending->using_declarations, source_index, index);
                        soa_using_declaration_index_ = index;
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1.0F);
            auto const submitted{ImGui::InputText("##declaration",
                                                  soa_using_declarations_[index].data(),
                                                  soa_using_declarations_[index].size(),
                                                  ImGuiInputTextFlags_EnterReturnsTrue)};
            if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                if (!pending.has_value()) {
                    pending = *schema;
                }
                pending->using_declarations[index] = soa_using_declarations_[index].data();
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::EndDisabled();

    ImGui::SeparatorText("Custom functions");
    ImGui::BeginDisabled(pending.has_value());
    auto const add_function{ImGui::Button("+ Function")};
    ImGui::EndDisabled();
    if (add_function) {
        auto replacement{*schema};
        auto function{codegen::FunctionSchema{}};
        function.name = unique_soa_function_name(replacement.functions, "custom_function");
        function.return_type =
            codegen::TypeRef{.name = "void", .suffix = {}, .nested = std::nullopt};
        function.is_inline = true;
        replacement.functions.push_back(std::move(function));
        auto const new_index{replacement.functions.size() - 1};
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
            soa_function_index_ = new_index;
            soa_editor_declaration_.reset();
            return true;
        }
    }
    ImGui::SameLine();
    auto const function_index{soa_function_index_};
    auto const has_function{function_index.has_value() &&
                            *function_index < schema->functions.size()};
    ImGui::BeginDisabled(pending.has_value() || !has_function);
    auto const duplicate_function{ImGui::Button("Duplicate function")};
    ImGui::EndDisabled();
    if (duplicate_function) {
        auto replacement{*schema};
        auto copy{replacement.functions[*function_index]};
        copy.name = unique_soa_function_name(replacement.functions, copy.name + "_copy");
        replacement.functions.insert(replacement.functions.begin() +
                                         static_cast<std::ptrdiff_t>(*function_index + 1),
                                     std::move(copy));
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
            soa_function_index_ = *function_index + 1;
            soa_editor_declaration_.reset();
            return true;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(pending.has_value() || !has_function || *function_index == 0);
    auto const move_function_up{ImGui::Button("Function up")};
    ImGui::EndDisabled();
    if (move_function_up) {
        auto replacement{*schema};
        std::swap(replacement.functions[*function_index],
                  replacement.functions[*function_index - 1]);
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
            soa_function_index_ = *function_index - 1;
            soa_editor_declaration_.reset();
            return true;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(pending.has_value() || !has_function ||
                         *function_index + 1 >= schema->functions.size());
    auto const move_function_down{ImGui::Button("Function down")};
    ImGui::EndDisabled();
    if (move_function_down) {
        auto replacement{*schema};
        std::swap(replacement.functions[*function_index],
                  replacement.functions[*function_index + 1]);
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
            soa_function_index_ = *function_index + 1;
            soa_editor_declaration_.reset();
            return true;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(pending.has_value() || !has_function);
    auto const delete_function{ImGui::Button("Delete function")};
    ImGui::EndDisabled();
    if (delete_function) {
        auto replacement{*schema};
        replacement.functions.erase(replacement.functions.begin() +
                                    static_cast<std::ptrdiff_t>(*function_index));
        auto const next_index{
            replacement.functions.empty()
                ? std::optional<std::size_t>{}
                : std::optional{std::min(*function_index, replacement.functions.size() - 1)}};
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
            soa_function_index_ = next_index;
            soa_editor_declaration_.reset();
            return true;
        }
    }

    ImGui::BeginDisabled(pending.has_value());
    if (soa_function_names_.size() == schema->functions.size() &&
        soa_function_return_types_.size() == schema->functions.size() &&
        detail::begin_editable_table("soa-functions", 8, schema->functions.size())) {
        detail::editable_table_column("Edit", ImGuiTableColumnFlags_WidthFixed);
        detail::editable_table_column("Name");
        detail::editable_table_column("Return");
        detail::editable_table_column("const", ImGuiTableColumnFlags_WidthFixed);
        detail::editable_table_column("noexcept", ImGuiTableColumnFlags_WidthFixed);
        detail::editable_table_column("static", ImGuiTableColumnFlags_WidthFixed);
        detail::editable_table_column("inline", ImGuiTableColumnFlags_WidthFixed);
        detail::editable_table_column("source", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();
        for (std::size_t index{}; index < schema->functions.size(); ++index) {
            auto const& function{schema->functions[index]};
            ImGui::PushID(static_cast<int>(index));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            auto const row_selected{soa_function_index_ == index};
            if (detail::editable_table_row_handle(row_selected)) {
                soa_function_index_ = index;
                soa_editor_declaration_.reset();
            }
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("SOA_FUNCTION_ROW", &index, sizeof(index));
                ImGui::Text("Move %s", function.name.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget()) {
                if (auto const* payload{ImGui::AcceptDragDropPayload("SOA_FUNCTION_ROW")}) {
                    auto const source_index{*static_cast<std::size_t const*>(payload->Data)};
                    if (source_index < schema->functions.size() && source_index != index) {
                        pending = *schema;
                        move_element(pending->functions, source_index, index);
                        soa_function_index_ = index;
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1.0F);
            auto const name_submitted{ImGui::InputText("##name",
                                                       soa_function_names_[index].data(),
                                                       soa_function_names_[index].size(),
                                                       ImGuiInputTextFlags_EnterReturnsTrue)};
            if (name_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                if (!pending.has_value()) {
                    pending = *schema;
                }
                pending->functions[index].name = soa_function_names_[index].data();
            }

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1.0F);
            auto const return_submitted{ImGui::InputText("##return",
                                                         soa_function_return_types_[index].data(),
                                                         soa_function_return_types_[index].size(),
                                                         ImGuiInputTextFlags_EnterReturnsTrue)};
            if (return_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                if (!pending.has_value()) {
                    pending = *schema;
                }
                pending->functions[index].return_type.name =
                    soa_function_return_types_[index].data();
            }

            auto edit_flag = [&](char const* label, bool const current, auto&& apply) {
                ImGui::TableNextColumn();
                auto value{current};
                if (ImGui::Checkbox(label, &value)) {
                    if (!pending.has_value()) {
                        pending = *schema;
                    }
                    apply(pending->functions[index], value);
                }
            };
            edit_flag("##const", function.is_const, [](auto& edited, bool const value) {
                edited.is_const = value;
                if (value) {
                    edited.is_static = false;
                }
            });
            edit_flag("##noexcept", function.is_noexcept, [](auto& edited, bool const value) {
                edited.is_noexcept = value;
            });
            edit_flag("##static", function.is_static, [](auto& edited, bool const value) {
                edited.is_static = value;
                if (value) {
                    edited.is_const = false;
                }
            });
            edit_flag("##inline", function.is_inline, [](auto& edited, bool const value) {
                edited.is_inline = value;
                if (value) {
                    edited.definition_in_source = false;
                }
            });
            edit_flag(
                "##source", function.definition_in_source, [](auto& edited, bool const value) {
                    edited.definition_in_source = value;
                    if (value) {
                        edited.is_inline = false;
                    }
                });
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::EndDisabled();

    if (has_function) {
        auto const& function{schema->functions[*function_index]};

        ImGui::TextUnformatted("Parameters");
        ImGui::BeginDisabled(pending.has_value());
        auto const add_parameter{ImGui::Button("+ Parameter")};
        ImGui::EndDisabled();
        if (add_parameter) {
            auto replacement{*schema};
            auto& parameters{replacement.functions[*function_index].parameters};
            auto parameter{codegen::ParameterSchema{}};
            parameter.name = unique_function_parameter_name(parameters, "parameter");
            parameter.type =
                codegen::TypeRef{.name = "std::uint32_t", .suffix = {}, .nested = std::nullopt};
            if (std::ranges::any_of(parameters, [](auto const& value) {
                    return value.default_value.has_value();
                })) {
                parameter.default_value = "{}";
            }
            parameters.push_back(std::move(parameter));
            auto const new_index{parameters.size() - 1};
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_parameter_index_ = new_index;
                soa_editor_declaration_.reset();
                return true;
            }
        }
        ImGui::SameLine();
        auto const parameter_index{soa_parameter_index_};
        auto const has_parameter{parameter_index.has_value() &&
                                 *parameter_index < function.parameters.size()};
        ImGui::BeginDisabled(pending.has_value() || !has_parameter);
        auto const duplicate_parameter{ImGui::Button("Duplicate parameter")};
        ImGui::EndDisabled();
        if (duplicate_parameter) {
            auto replacement{*schema};
            auto& parameters{replacement.functions[*function_index].parameters};
            auto copy{parameters[*parameter_index]};
            copy.name = unique_function_parameter_name(parameters, copy.name + "_copy");
            parameters.insert(parameters.begin() +
                                  static_cast<std::ptrdiff_t>(*parameter_index + 1),
                              std::move(copy));
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_parameter_index_ = *parameter_index + 1;
                soa_editor_declaration_.reset();
                return true;
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(pending.has_value() || !has_parameter || *parameter_index == 0);
        auto const move_parameter_up{ImGui::Button("Parameter up")};
        ImGui::EndDisabled();
        if (move_parameter_up) {
            auto replacement{*schema};
            auto& parameters{replacement.functions[*function_index].parameters};
            std::swap(parameters[*parameter_index], parameters[*parameter_index - 1]);
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_parameter_index_ = *parameter_index - 1;
                soa_editor_declaration_.reset();
                return true;
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(pending.has_value() || !has_parameter ||
                             *parameter_index + 1 >= function.parameters.size());
        auto const move_parameter_down{ImGui::Button("Parameter down")};
        ImGui::EndDisabled();
        if (move_parameter_down) {
            auto replacement{*schema};
            auto& parameters{replacement.functions[*function_index].parameters};
            std::swap(parameters[*parameter_index], parameters[*parameter_index + 1]);
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_parameter_index_ = *parameter_index + 1;
                soa_editor_declaration_.reset();
                return true;
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(pending.has_value() || !has_parameter);
        auto const delete_parameter{ImGui::Button("Delete parameter")};
        ImGui::EndDisabled();
        if (delete_parameter) {
            auto replacement{*schema};
            auto& parameters{replacement.functions[*function_index].parameters};
            parameters.erase(parameters.begin() + static_cast<std::ptrdiff_t>(*parameter_index));
            auto const next_index{
                parameters.empty()
                    ? std::optional<std::size_t>{}
                    : std::optional{std::min(*parameter_index, parameters.size() - 1)}};
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_parameter_index_ = next_index;
                soa_editor_declaration_.reset();
                return true;
            }
        }

        ImGui::BeginDisabled(pending.has_value());
        if (soa_parameter_names_.size() == function.parameters.size() &&
            soa_parameter_types_.size() == function.parameters.size() &&
            soa_parameter_defaults_.size() == function.parameters.size() &&
            detail::begin_editable_table(
                "soa-function-parameters", 5, function.parameters.size())) {
            detail::editable_table_column("Edit", ImGuiTableColumnFlags_WidthFixed);
            detail::editable_table_column("Name");
            detail::editable_table_column("Type");
            detail::editable_table_column("Pick", ImGuiTableColumnFlags_WidthFixed);
            detail::editable_table_column("Default");
            ImGui::TableHeadersRow();
            for (std::size_t index{}; index < function.parameters.size(); ++index) {
                auto const& parameter{function.parameters[index]};
                ImGui::PushID(static_cast<int>(index));
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                auto const row_selected{soa_parameter_index_ == index};
                if (detail::editable_table_row_handle(row_selected)) {
                    soa_parameter_index_ = index;
                }
                if (ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload("SOA_FUNCTION_PARAMETER_ROW", &index, sizeof(index));
                    ImGui::Text("Move %s", parameter.name.c_str());
                    ImGui::EndDragDropSource();
                }
                if (ImGui::BeginDragDropTarget()) {
                    if (auto const* payload{
                            ImGui::AcceptDragDropPayload("SOA_FUNCTION_PARAMETER_ROW")}) {
                        auto const source_index{*static_cast<std::size_t const*>(payload->Data)};
                        if (source_index < function.parameters.size() && source_index != index) {
                            pending = *schema;
                            move_element(pending->functions[*function_index].parameters,
                                         source_index,
                                         index);
                            soa_parameter_index_ = index;
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-1.0F);
                auto const name_submitted{ImGui::InputText("##name",
                                                           soa_parameter_names_[index].data(),
                                                           soa_parameter_names_[index].size(),
                                                           ImGuiInputTextFlags_EnterReturnsTrue)};
                if (name_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                    if (!pending.has_value()) {
                        pending = *schema;
                    }
                    pending->functions[*function_index].parameters[index].name =
                        soa_parameter_names_[index].data();
                }

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-1.0F);
                auto const type_submitted{ImGui::InputText("##type",
                                                           soa_parameter_types_[index].data(),
                                                           soa_parameter_types_[index].size(),
                                                           ImGuiInputTextFlags_EnterReturnsTrue)};
                if (type_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                    if (!pending.has_value()) {
                        pending = *schema;
                    }
                    pending->functions[*function_index].parameters[index].type.name =
                        soa_parameter_types_[index].data();
                }

                ImGui::TableNextColumn();
                ImGui::PushID("type-picker");
                if (auto picked{draw_type_picker(node.identity.module_name, node.identity)}) {
                    if (!pending.has_value()) {
                        pending = *schema;
                    }
                    pending->functions[*function_index].parameters[index].type.name = *picked;
                }
                ImGui::PopID();

                ImGui::TableNextColumn();
                auto has_default{parameter.default_value.has_value()};
                if (ImGui::Checkbox("##has-default", &has_default)) {
                    if (!pending.has_value()) {
                        pending = *schema;
                    }
                    auto& edited{
                        pending->functions[*function_index].parameters[index].default_value};
                    edited = has_default ? std::optional<std::string>{"{}"} : std::nullopt;
                }
                if (parameter.default_value.has_value()) {
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(-1.0F);
                    auto const default_submitted{
                        ImGui::InputText("##default",
                                         soa_parameter_defaults_[index].data(),
                                         soa_parameter_defaults_[index].size(),
                                         ImGuiInputTextFlags_EnterReturnsTrue)};
                    if (default_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                        if (!pending.has_value()) {
                            pending = *schema;
                        }
                        pending->functions[*function_index].parameters[index].default_value =
                            soa_parameter_defaults_[index].data();
                    }
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::EndDisabled();

        ImGui::PushID(static_cast<int>(*function_index));
        ImGui::SeparatorText("Body fragments");
        ImGui::BeginDisabled(pending.has_value());
        auto const add_body_line{ImGui::Button("+ Body fragment")};
        ImGui::EndDisabled();
        if (add_body_line) {
            auto replacement{*schema};
            auto& body_lines{replacement.functions[*function_index].body_lines};
            body_lines.emplace_back("// TODO");
            auto const new_index{body_lines.size() - 1};
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_function_body_index_ = new_index;
                soa_editor_declaration_.reset();
                ImGui::PopID();
                return true;
            }
        }
        ImGui::SameLine();
        auto const body_index{soa_function_body_index_};
        auto const has_body_line{body_index.has_value() &&
                                 *body_index < function.body_lines.size()};
        ImGui::BeginDisabled(pending.has_value() || !has_body_line);
        auto const duplicate_body_line{ImGui::Button("Duplicate body")};
        ImGui::EndDisabled();
        if (duplicate_body_line) {
            auto replacement{*schema};
            auto& body_lines{replacement.functions[*function_index].body_lines};
            body_lines.insert(body_lines.begin() + static_cast<std::ptrdiff_t>(*body_index + 1),
                              body_lines[*body_index]);
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_function_body_index_ = *body_index + 1;
                soa_editor_declaration_.reset();
                ImGui::PopID();
                return true;
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(pending.has_value() || !has_body_line || *body_index == 0);
        auto const move_body_up{ImGui::Button("Body up")};
        ImGui::EndDisabled();
        if (move_body_up) {
            auto replacement{*schema};
            auto& body_lines{replacement.functions[*function_index].body_lines};
            std::swap(body_lines[*body_index], body_lines[*body_index - 1]);
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_function_body_index_ = *body_index - 1;
                soa_editor_declaration_.reset();
                ImGui::PopID();
                return true;
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(pending.has_value() || !has_body_line ||
                             *body_index + 1 >= function.body_lines.size());
        auto const move_body_down{ImGui::Button("Body down")};
        ImGui::EndDisabled();
        if (move_body_down) {
            auto replacement{*schema};
            auto& body_lines{replacement.functions[*function_index].body_lines};
            std::swap(body_lines[*body_index], body_lines[*body_index + 1]);
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_function_body_index_ = *body_index + 1;
                soa_editor_declaration_.reset();
                ImGui::PopID();
                return true;
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(pending.has_value() || !has_body_line);
        auto const delete_body_line{ImGui::Button("Delete body")};
        ImGui::EndDisabled();
        if (delete_body_line) {
            auto replacement{*schema};
            auto& body_lines{replacement.functions[*function_index].body_lines};
            body_lines.erase(body_lines.begin() + static_cast<std::ptrdiff_t>(*body_index));
            auto const next_index{
                body_lines.empty() ? std::optional<std::size_t>{}
                                   : std::optional{std::min(*body_index, body_lines.size() - 1)}};
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_function_body_index_ = next_index;
                soa_editor_declaration_.reset();
                ImGui::PopID();
                return true;
            }
        }

        ImGui::BeginDisabled(pending.has_value());
        if (soa_function_body_lines_.size() == function.body_lines.size() &&
            detail::begin_editable_table("function-body-lines", 2, function.body_lines.size())) {
            detail::editable_table_column("Edit", ImGuiTableColumnFlags_WidthFixed);
            detail::editable_table_column("C++ fragment");
            ImGui::TableHeadersRow();
            for (std::size_t index{}; index < function.body_lines.size(); ++index) {
                ImGui::PushID(static_cast<int>(index));
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                auto const row_selected{soa_function_body_index_ == index};
                if (detail::editable_table_row_handle(row_selected)) {
                    soa_function_body_index_ = index;
                }
                if (ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload("SOA_FUNCTION_BODY_ROW", &index, sizeof(index));
                    ImGui::TextUnformatted("Move body fragment");
                    ImGui::EndDragDropSource();
                }
                if (ImGui::BeginDragDropTarget()) {
                    if (auto const* payload{
                            ImGui::AcceptDragDropPayload("SOA_FUNCTION_BODY_ROW")}) {
                        auto const source_index{*static_cast<std::size_t const*>(payload->Data)};
                        if (source_index < function.body_lines.size() && source_index != index) {
                            pending = *schema;
                            move_element(pending->functions[*function_index].body_lines,
                                         source_index,
                                         index);
                            soa_function_body_index_ = index;
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                ImGui::TableNextColumn();
                input_text_multiline("##body",
                                     soa_function_body_lines_[index],
                                     ImVec2{-1.0F, ImGui::GetTextLineHeight() * 2.5F});
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    if (!pending.has_value()) {
                        pending = *schema;
                    }
                    pending->functions[*function_index].body_lines[index] =
                        soa_function_body_lines_[index];
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::EndDisabled();

        ImGui::SeparatorText("Declared dependencies");
        ImGui::SetNextItemWidth(220.0F);
        ImGui::BeginDisabled(pending.has_value());
        input_text("Dependency key", soa_new_function_dependency_);
        ImGui::SameLine();
        auto const add_dependency{ImGui::Button("+ Dependency")};
        ImGui::EndDisabled();
        if (add_dependency) {
            if (soa_new_function_dependency_.find_first_not_of(" \t\r\n") == std::string::npos) {
                schema_edit_message_ = "SoA function dependency key cannot be empty.";
                ImGui::PopID();
                return false;
            }
            auto replacement{*schema};
            auto& dependencies{replacement.functions[*function_index].dependencies};
            dependencies.push_back(soa_new_function_dependency_);
            auto const new_index{dependencies.size() - 1};
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_new_function_dependency_.clear();
                soa_function_dependency_index_ = new_index;
                soa_editor_declaration_.reset();
                ImGui::PopID();
                return true;
            }
        }
        auto const dependency_index{soa_function_dependency_index_};
        auto const has_dependency{dependency_index.has_value() &&
                                  *dependency_index < function.dependencies.size()};
        ImGui::BeginDisabled(pending.has_value() || !has_dependency);
        auto const duplicate_dependency{ImGui::Button("Duplicate dependency")};
        ImGui::EndDisabled();
        if (duplicate_dependency) {
            auto replacement{*schema};
            auto& dependencies{replacement.functions[*function_index].dependencies};
            dependencies.insert(dependencies.begin() +
                                    static_cast<std::ptrdiff_t>(*dependency_index + 1),
                                dependencies[*dependency_index]);
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_function_dependency_index_ = *dependency_index + 1;
                soa_editor_declaration_.reset();
                ImGui::PopID();
                return true;
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(pending.has_value() || !has_dependency || *dependency_index == 0);
        auto const move_dependency_up{ImGui::Button("Dependency up")};
        ImGui::EndDisabled();
        if (move_dependency_up) {
            auto replacement{*schema};
            auto& dependencies{replacement.functions[*function_index].dependencies};
            std::swap(dependencies[*dependency_index], dependencies[*dependency_index - 1]);
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_function_dependency_index_ = *dependency_index - 1;
                soa_editor_declaration_.reset();
                ImGui::PopID();
                return true;
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(pending.has_value() || !has_dependency ||
                             *dependency_index + 1 >= function.dependencies.size());
        auto const move_dependency_down{ImGui::Button("Dependency down")};
        ImGui::EndDisabled();
        if (move_dependency_down) {
            auto replacement{*schema};
            auto& dependencies{replacement.functions[*function_index].dependencies};
            std::swap(dependencies[*dependency_index], dependencies[*dependency_index + 1]);
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_function_dependency_index_ = *dependency_index + 1;
                soa_editor_declaration_.reset();
                ImGui::PopID();
                return true;
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(pending.has_value() || !has_dependency);
        auto const delete_dependency{ImGui::Button("Delete dependency")};
        ImGui::EndDisabled();
        if (delete_dependency) {
            auto replacement{*schema};
            auto& dependencies{replacement.functions[*function_index].dependencies};
            dependencies.erase(dependencies.begin() +
                               static_cast<std::ptrdiff_t>(*dependency_index));
            auto const next_index{
                dependencies.empty()
                    ? std::optional<std::size_t>{}
                    : std::optional{std::min(*dependency_index, dependencies.size() - 1)}};
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_function_dependency_index_ = next_index;
                soa_editor_declaration_.reset();
                ImGui::PopID();
                return true;
            }
        }

        ImGui::BeginDisabled(pending.has_value());
        if (soa_function_dependencies_.size() == function.dependencies.size() &&
            detail::begin_editable_table(
                "function-dependencies", 2, function.dependencies.size())) {
            detail::editable_table_column("Edit", ImGuiTableColumnFlags_WidthFixed);
            detail::editable_table_column("Registered dependency key");
            ImGui::TableHeadersRow();
            for (std::size_t index{}; index < function.dependencies.size(); ++index) {
                ImGui::PushID(static_cast<int>(index));
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                auto const row_selected{soa_function_dependency_index_ == index};
                if (detail::editable_table_row_handle(row_selected)) {
                    soa_function_dependency_index_ = index;
                }
                if (ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload("SOA_FUNCTION_DEPENDENCY_ROW", &index, sizeof(index));
                    ImGui::Text("Move %s", function.dependencies[index].c_str());
                    ImGui::EndDragDropSource();
                }
                if (ImGui::BeginDragDropTarget()) {
                    if (auto const* payload{
                            ImGui::AcceptDragDropPayload("SOA_FUNCTION_DEPENDENCY_ROW")}) {
                        auto const source_index{*static_cast<std::size_t const*>(payload->Data)};
                        if (source_index < function.dependencies.size() && source_index != index) {
                            pending = *schema;
                            move_element(pending->functions[*function_index].dependencies,
                                         source_index,
                                         index);
                            soa_function_dependency_index_ = index;
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-1.0F);
                auto const submitted{input_text("##dependency",
                                                soa_function_dependencies_[index],
                                                ImGuiInputTextFlags_EnterReturnsTrue)};
                if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                    if (!pending.has_value()) {
                        pending = *schema;
                    }
                    pending->functions[*function_index].dependencies[index] =
                        soa_function_dependencies_[index];
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::EndDisabled();

        ImGui::SeparatorText("Advanced signature");
        auto has_trailing_return{function.trailing_return_type.has_value()};
        ImGui::BeginDisabled(pending.has_value());
        auto const trailing_toggled{
            ImGui::Checkbox("Use trailing return type", &has_trailing_return)};
        ImGui::EndDisabled();
        if (trailing_toggled) {
            auto replacement{*schema};
            auto& edited{replacement.functions[*function_index]};
            if (has_trailing_return) {
                edited.trailing_return_type =
                    edited.return_type.name == "auto"
                        ? codegen::TypeRef{.name = "void", .suffix = {}, .nested = std::nullopt}
                        : edited.return_type;
                edited.return_type =
                    codegen::TypeRef{.name = "auto", .suffix = {}, .nested = std::nullopt};
            } else {
                edited.return_type = *edited.trailing_return_type;
                edited.trailing_return_type.reset();
            }
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_editor_declaration_.reset();
                ImGui::PopID();
                return true;
            }
        }
        if (function.trailing_return_type.has_value()) {
            ImGui::SetNextItemWidth(220.0F);
            auto const submitted{input_text("Trailing return type",
                                            soa_function_trailing_return_type_,
                                            ImGuiInputTextFlags_EnterReturnsTrue)};
            if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                if (!pending.has_value()) {
                    pending = *schema;
                }
                pending->functions[*function_index].trailing_return_type->name =
                    soa_function_trailing_return_type_;
            }
            ImGui::SameLine();
            ImGui::PushID("trailing-return-picker");
            if (auto picked{draw_type_picker(node.identity.module_name, node.identity)}) {
                if (!pending.has_value()) {
                    pending = *schema;
                }
                pending->functions[*function_index].trailing_return_type->name = *picked;
            }
            ImGui::PopID();
        }

        auto has_template_parameters{function.template_parameters.has_value()};
        ImGui::BeginDisabled(pending.has_value());
        auto const template_toggled{
            ImGui::Checkbox("Template parameters", &has_template_parameters)};
        ImGui::EndDisabled();
        if (template_toggled) {
            auto replacement{*schema};
            replacement.functions[*function_index].template_parameters =
                has_template_parameters ? std::optional<std::string>{"typename T"} : std::nullopt;
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_editor_declaration_.reset();
                ImGui::PopID();
                return true;
            }
        }
        if (function.template_parameters.has_value()) {
            ImGui::SetNextItemWidth(-1.0F);
            auto const submitted{input_text("Template parameter text",
                                            soa_function_template_parameters_,
                                            ImGuiInputTextFlags_EnterReturnsTrue)};
            if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                if (!pending.has_value()) {
                    pending = *schema;
                }
                pending->functions[*function_index].template_parameters =
                    soa_function_template_parameters_;
            }
        }

        auto has_requires_clause{function.requires_clause.has_value()};
        ImGui::BeginDisabled(pending.has_value());
        auto const requires_toggled{ImGui::Checkbox("Requires clause", &has_requires_clause)};
        ImGui::EndDisabled();
        if (requires_toggled) {
            auto replacement{*schema};
            replacement.functions[*function_index].requires_clause =
                has_requires_clause ? std::optional<std::string>{"true"} : std::nullopt;
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_editor_declaration_.reset();
                ImGui::PopID();
                return true;
            }
        }
        if (function.requires_clause.has_value()) {
            ImGui::SetNextItemWidth(-1.0F);
            auto const submitted{input_text("Requires expression",
                                            soa_function_requires_clause_,
                                            ImGuiInputTextFlags_EnterReturnsTrue)};
            if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                if (!pending.has_value()) {
                    pending = *schema;
                }
                pending->functions[*function_index].requires_clause = soa_function_requires_clause_;
            }
        }
        ImGui::PopID();
    }

    ImGui::SeparatorText("View types");
    auto explicit_view_name{schema->view_name.has_value()};
    if (ImGui::Checkbox("Explicit mutable view type", &explicit_view_name)) {
        auto replacement{*schema};
        replacement.view_name =
            explicit_view_name ? std::optional{schema->name + "View"} : std::nullopt;
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
            soa_editor_declaration_.reset();
            return true;
        }
    }
    if (schema->view_name.has_value()) {
        ImGui::SetNextItemWidth(-1.0F);
        auto const submitted{ImGui::InputText("Mutable view type",
                                              soa_view_name_.data(),
                                              soa_view_name_.size(),
                                              ImGuiInputTextFlags_EnterReturnsTrue)};
        if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
            if (soa_view_name_.front() == '\0') {
                schema_edit_message_ = "Explicit mutable view type cannot be empty; disable it to "
                                       "use the derived name.";
                return false;
            }
            auto replacement{*schema};
            replacement.view_name = soa_view_name_.data();
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_editor_declaration_.reset();
                return true;
            }
        }
    } else {
        ImGui::TextDisabled("Derived mutable view: %sView", schema->name.c_str());
    }

    auto explicit_const_view_name{schema->const_view_name.has_value()};
    if (ImGui::Checkbox("Explicit const view type", &explicit_const_view_name)) {
        auto replacement{*schema};
        replacement.const_view_name =
            explicit_const_view_name ? std::optional{schema->name + "ConstView"} : std::nullopt;
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
            soa_editor_declaration_.reset();
            return true;
        }
    }
    if (schema->const_view_name.has_value()) {
        ImGui::SetNextItemWidth(-1.0F);
        auto const submitted{ImGui::InputText("Const view type",
                                              soa_const_view_name_.data(),
                                              soa_const_view_name_.size(),
                                              ImGuiInputTextFlags_EnterReturnsTrue)};
        if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
            if (soa_const_view_name_.front() == '\0') {
                schema_edit_message_ =
                    "Explicit const view type cannot be empty; disable it to use the derived name.";
                return false;
            }
            auto replacement{*schema};
            replacement.const_view_name = soa_const_view_name_.data();
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_editor_declaration_.reset();
                return true;
            }
        }
    } else {
        ImGui::TextDisabled("Derived const view: %sConstView", schema->name.c_str());
    }

    ImGui::SeparatorText("Fixed layout");
    if (!schema->fixed.has_value()) {
        if (ImGui::Button("Enable fixed layout")) {
            auto storage_name{document_->unique_soa_generated_type_name(
                *declaration, schema->name + "FixedStorage")};
            if (!storage_name.has_value()) {
                schema_edit_message_ = storage_name.error().message;
                return false;
            }
            auto replacement{*schema};
            replacement.fixed =
                codegen::FixedSoaSchema{.storage_name = std::move(*storage_name), .containers = {}};
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_editor_declaration_.reset();
                return true;
            }
        }
    } else {
        ImGui::SetNextItemWidth(-1.0F);
        auto const storage_submitted{ImGui::InputText("Storage type",
                                                      soa_fixed_storage_name_.data(),
                                                      soa_fixed_storage_name_.size(),
                                                      ImGuiInputTextFlags_EnterReturnsTrue)};
        if (storage_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
            if (soa_fixed_storage_name_.front() == '\0') {
                schema_edit_message_ = "Fixed-layout storage type cannot be empty.";
                return false;
            }
            auto replacement{*schema};
            replacement.fixed->storage_name = soa_fixed_storage_name_.data();
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_editor_declaration_.reset();
                return true;
            }
        }

        if (ImGui::Button("+ Fixed container")) {
            auto container_name{
                document_->unique_soa_generated_type_name(*declaration, schema->name + "Fixed")};
            if (!container_name.has_value()) {
                schema_edit_message_ = container_name.error().message;
                return false;
            }
            auto replacement{*schema};
            replacement.fixed->containers.push_back(std::move(*container_name));
            auto const new_index{replacement.fixed->containers.size() - 1};
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_fixed_container_index_ = new_index;
                soa_editor_declaration_.reset();
                return true;
            }
        }
        ImGui::SameLine();
        auto const container_index{soa_fixed_container_index_};
        auto const has_container{container_index.has_value() &&
                                 *container_index < schema->fixed->containers.size()};
        ImGui::BeginDisabled(!has_container);
        if (ImGui::Button("Duplicate fixed container")) {
            auto container_name{document_->unique_soa_generated_type_name(
                *declaration, schema->fixed->containers[*container_index] + "_copy")};
            if (!container_name.has_value()) {
                schema_edit_message_ = container_name.error().message;
            } else {
                auto replacement{*schema};
                replacement.fixed->containers.insert(
                    replacement.fixed->containers.begin() +
                        static_cast<std::ptrdiff_t>(*container_index + 1),
                    std::move(*container_name));
                if (apply_document_edit(ReplaceSoa{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
                    soa_fixed_container_index_ = *container_index + 1;
                    soa_editor_declaration_.reset();
                    return true;
                }
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!has_container || *container_index == 0);
        if (ImGui::Button("Fixed container up")) {
            auto replacement{*schema};
            std::swap(replacement.fixed->containers[*container_index],
                      replacement.fixed->containers[*container_index - 1]);
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_fixed_container_index_ = *container_index - 1;
                soa_editor_declaration_.reset();
                return true;
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!has_container ||
                             *container_index + 1 >= schema->fixed->containers.size());
        if (ImGui::Button("Fixed container down")) {
            auto replacement{*schema};
            std::swap(replacement.fixed->containers[*container_index],
                      replacement.fixed->containers[*container_index + 1]);
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_fixed_container_index_ = *container_index + 1;
                soa_editor_declaration_.reset();
                return true;
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Delete fixed container")) {
            auto replacement{*schema};
            replacement.fixed->containers.erase(replacement.fixed->containers.begin() +
                                                static_cast<std::ptrdiff_t>(*container_index));
            auto const next_index{
                replacement.fixed->containers.empty()
                    ? std::optional<std::size_t>{}
                    : std::optional{
                          std::min(*container_index, replacement.fixed->containers.size() - 1)}};
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_fixed_container_index_ = next_index;
                soa_editor_declaration_.reset();
                return true;
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Disable fixed layout")) {
            auto replacement{*schema};
            replacement.fixed.reset();
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_fixed_container_index_.reset();
                soa_editor_declaration_.reset();
                return true;
            }
        }

        if (soa_fixed_container_names_.size() == schema->fixed->containers.size() &&
            detail::begin_editable_table(
                "soa-fixed-containers", 2, schema->fixed->containers.size())) {
            detail::editable_table_column("Edit", ImGuiTableColumnFlags_WidthFixed);
            detail::editable_table_column("Container type");
            ImGui::TableHeadersRow();
            for (std::size_t index{}; index < schema->fixed->containers.size(); ++index) {
                ImGui::PushID(static_cast<int>(index));
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                auto const row_selected{soa_fixed_container_index_ == index};
                if (detail::editable_table_row_handle(row_selected)) {
                    soa_fixed_container_index_ = index;
                }
                if (ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload("SOA_FIXED_CONTAINER_ROW", &index, sizeof(index));
                    ImGui::Text("Move %s", schema->fixed->containers[index].c_str());
                    ImGui::EndDragDropSource();
                }
                if (ImGui::BeginDragDropTarget()) {
                    if (auto const* payload{
                            ImGui::AcceptDragDropPayload("SOA_FIXED_CONTAINER_ROW")}) {
                        auto const source_index{*static_cast<std::size_t const*>(payload->Data)};
                        if (source_index < schema->fixed->containers.size() &&
                            source_index != index) {
                            pending = *schema;
                            move_element(pending->fixed->containers, source_index, index);
                            soa_fixed_container_index_ = index;
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-1.0F);
                auto const name_submitted{ImGui::InputText("##name",
                                                           soa_fixed_container_names_[index].data(),
                                                           soa_fixed_container_names_[index].size(),
                                                           ImGuiInputTextFlags_EnterReturnsTrue)};
                if (name_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                    if (!pending.has_value()) {
                        pending = *schema;
                    }
                    pending->fixed->containers[index] = soa_fixed_container_names_[index].data();
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }

    ImGui::SeparatorText("Single allocation");
    if (!schema->single_allocation.has_value()) {
        if (ImGui::Button("Enable single allocation")) {
            auto owner_name{
                document_->unique_soa_storage_owner_name(*declaration, schema->name + "Single")};
            if (!owner_name.has_value()) {
                schema_edit_message_ = owner_name.error().message;
                return false;
            }
            auto replacement{*schema};
            replacement.single_allocation = std::move(*owner_name);
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_editor_declaration_.reset();
                return true;
            }
        }
    } else {
        ImGui::SetNextItemWidth(-1.0F);
        auto const submitted{ImGui::InputText("Owner type",
                                              soa_single_allocation_name_.data(),
                                              soa_single_allocation_name_.size(),
                                              ImGuiInputTextFlags_EnterReturnsTrue)};
        if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
            if (soa_single_allocation_name_.front() == '\0') {
                schema_edit_message_ = "Single-allocation owner type cannot be empty.";
                return false;
            }
            auto replacement{*schema};
            replacement.single_allocation = soa_single_allocation_name_.data();
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_editor_declaration_.reset();
                return true;
            }
        }
        ImGui::Text("Generated storage: %sStorage", schema->single_allocation->c_str());
        ImGui::TextUnformatted("New variant allocator");
        ImGui::SetNextItemWidth(std::max(120.0F, ImGui::GetContentRegionAvail().x - 120.0F));
        ImGui::InputText("##new-variant-allocator",
                         soa_new_single_allocation_allocator_.data(),
                         soa_new_single_allocation_allocator_.size());
        ImGui::SameLine();
        if (auto picked{draw_type_picker(node.identity.module_name, node.identity)}) {
            std::snprintf(soa_new_single_allocation_allocator_.data(),
                          soa_new_single_allocation_allocator_.size(),
                          "%s",
                          picked->c_str());
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(soa_new_single_allocation_allocator_.front() == '\0');
        if (ImGui::Button("+ Variant")) {
            auto variant_name{document_->unique_soa_storage_owner_name(
                *declaration, schema->name + "SingleVariant")};
            if (!variant_name.has_value()) {
                schema_edit_message_ = variant_name.error().message;
            } else {
                auto replacement{*schema};
                replacement.single_allocation_variants.push_back(codegen::SingleAllocationVariant{
                    .name = std::move(*variant_name),
                    .allocator =
                        codegen::TypeRef{.name = soa_new_single_allocation_allocator_.data(),
                                         .suffix = {},
                                         .nested = std::nullopt}});
                auto const new_index{replacement.single_allocation_variants.size() - 1};
                if (apply_document_edit(ReplaceSoa{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
                    soa_single_allocation_variant_index_ = new_index;
                    soa_editor_declaration_.reset();
                    return true;
                }
            }
        }
        ImGui::EndDisabled();

        auto const variant_index{soa_single_allocation_variant_index_};
        auto const has_variant{variant_index.has_value() &&
                               *variant_index < schema->single_allocation_variants.size()};
        ImGui::BeginDisabled(!has_variant);
        if (ImGui::Button("Duplicate variant")) {
            auto variant_name{document_->unique_soa_storage_owner_name(
                *declaration, schema->single_allocation_variants[*variant_index].name + "_copy")};
            if (!variant_name.has_value()) {
                schema_edit_message_ = variant_name.error().message;
            } else {
                auto replacement{*schema};
                auto copy{replacement.single_allocation_variants[*variant_index]};
                copy.name = std::move(*variant_name);
                replacement.single_allocation_variants.insert(
                    replacement.single_allocation_variants.begin() +
                        static_cast<std::ptrdiff_t>(*variant_index + 1),
                    std::move(copy));
                if (apply_document_edit(ReplaceSoa{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
                    soa_single_allocation_variant_index_ = *variant_index + 1;
                    soa_editor_declaration_.reset();
                    return true;
                }
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!has_variant || *variant_index == 0);
        if (ImGui::Button("Variant up")) {
            auto replacement{*schema};
            std::swap(replacement.single_allocation_variants[*variant_index],
                      replacement.single_allocation_variants[*variant_index - 1]);
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_single_allocation_variant_index_ = *variant_index - 1;
                soa_editor_declaration_.reset();
                return true;
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!has_variant ||
                             *variant_index + 1 >= schema->single_allocation_variants.size());
        if (ImGui::Button("Variant down")) {
            auto replacement{*schema};
            std::swap(replacement.single_allocation_variants[*variant_index],
                      replacement.single_allocation_variants[*variant_index + 1]);
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_single_allocation_variant_index_ = *variant_index + 1;
                soa_editor_declaration_.reset();
                return true;
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Delete variant")) {
            auto replacement{*schema};
            replacement.single_allocation_variants.erase(
                replacement.single_allocation_variants.begin() +
                static_cast<std::ptrdiff_t>(*variant_index));
            auto const next_index{
                replacement.single_allocation_variants.empty()
                    ? std::optional<std::size_t>{}
                    : std::optional{std::min(*variant_index,
                                             replacement.single_allocation_variants.size() - 1)}};
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_single_allocation_variant_index_ = next_index;
                soa_editor_declaration_.reset();
                return true;
            }
        }
        ImGui::EndDisabled();

        if (soa_single_allocation_variant_names_.size() ==
                schema->single_allocation_variants.size() &&
            soa_single_allocation_variant_allocators_.size() ==
                schema->single_allocation_variants.size() &&
            detail::begin_editable_table(
                "soa-single-allocation-variants", 3, schema->single_allocation_variants.size())) {
            detail::editable_table_column("Edit", ImGuiTableColumnFlags_WidthFixed);
            detail::editable_table_column("Owner type");
            detail::editable_table_column("Allocator type");
            ImGui::TableHeadersRow();
            for (std::size_t index{}; index < schema->single_allocation_variants.size(); ++index) {
                ImGui::PushID(static_cast<int>(index));
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                auto const row_selected{soa_single_allocation_variant_index_ == index};
                if (detail::editable_table_row_handle(row_selected)) {
                    soa_single_allocation_variant_index_ = index;
                }
                if (ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload(
                        "SOA_SINGLE_ALLOCATION_VARIANT_ROW", &index, sizeof(index));
                    ImGui::Text("Move %s", schema->single_allocation_variants[index].name.c_str());
                    ImGui::EndDragDropSource();
                }
                if (ImGui::BeginDragDropTarget()) {
                    if (auto const* payload{
                            ImGui::AcceptDragDropPayload("SOA_SINGLE_ALLOCATION_VARIANT_ROW")}) {
                        auto const source_index{*static_cast<std::size_t const*>(payload->Data)};
                        if (source_index < schema->single_allocation_variants.size() &&
                            source_index != index) {
                            pending = *schema;
                            move_element(pending->single_allocation_variants, source_index, index);
                            soa_single_allocation_variant_index_ = index;
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-1.0F);
                auto const name_submitted{
                    ImGui::InputText("##name",
                                     soa_single_allocation_variant_names_[index].data(),
                                     soa_single_allocation_variant_names_[index].size(),
                                     ImGuiInputTextFlags_EnterReturnsTrue)};
                if (name_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                    if (!pending.has_value()) {
                        pending = *schema;
                    }
                    pending->single_allocation_variants[index].name =
                        soa_single_allocation_variant_names_[index].data();
                }

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(std::max(60.0F, ImGui::GetContentRegionAvail().x - 28.0F));
                auto const allocator_submitted{
                    ImGui::InputText("##allocator",
                                     soa_single_allocation_variant_allocators_[index].data(),
                                     soa_single_allocation_variant_allocators_[index].size(),
                                     ImGuiInputTextFlags_EnterReturnsTrue)};
                if (allocator_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                    if (!pending.has_value()) {
                        pending = *schema;
                    }
                    pending->single_allocation_variants[index].allocator.name =
                        soa_single_allocation_variant_allocators_[index].data();
                }
                ImGui::SameLine();
                if (auto picked{draw_type_picker(node.identity.module_name, node.identity)}) {
                    if (!pending.has_value()) {
                        pending = *schema;
                    }
                    pending->single_allocation_variants[index].allocator.name = std::move(*picked);
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }

        ImGui::BeginDisabled(!schema->single_allocation_variants.empty());
        if (ImGui::Button("Disable single allocation")) {
            auto replacement{*schema};
            replacement.single_allocation.reset();
            if (apply_document_edit(
                    ReplaceSoa{.declaration = *declaration, .schema = std::move(replacement)})) {
                soa_editor_declaration_.reset();
                return true;
            }
        }
        ImGui::EndDisabled();
        if (!schema->single_allocation_variants.empty()) {
            ImGui::TextDisabled("Remove allocator variants before disabling single allocation.");
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
        auto const invalid_member{std::ranges::find_if(pending->members, [](auto const& member) {
            return member.name.empty() || member.type.name.empty();
        })};
        if (invalid_member != pending->members.end()) {
            schema_edit_message_ = invalid_member->name.empty()
                                     ? "SoA column name cannot be empty."
                                     : "SoA column type cannot be empty.";
            return false;
        }
        if (pending->fixed.has_value() &&
            std::ranges::any_of(pending->fixed->containers,
                                [](std::string const& name) { return name.empty(); })) {
            schema_edit_message_ = "Fixed-layout container type cannot be empty.";
            return false;
        }
        if (std::ranges::any_of(pending->using_declarations, [](std::string const& declaration) {
                return declaration.find_first_not_of(" \t\r\n") == std::string::npos;
            })) {
            schema_edit_message_ = "SoA using declaration cannot be empty.";
            return false;
        }
        auto const invalid_function{
            std::ranges::find_if(pending->functions, [](auto const& function) {
                return function.name.empty() || function.return_type.name.empty();
            })};
        if (invalid_function != pending->functions.end()) {
            schema_edit_message_ = invalid_function->name.empty()
                                     ? "SoA function name cannot be empty."
                                     : "SoA function return type cannot be empty.";
            return false;
        }
        for (auto const& function : pending->functions) {
            if (function.trailing_return_type.has_value() &&
                function.trailing_return_type->name.empty()) {
                schema_edit_message_ = "SoA function trailing return type cannot be empty.";
                return false;
            }
            if (function.template_parameters.has_value() &&
                function.template_parameters->find_first_not_of(" \t\r\n") == std::string::npos) {
                schema_edit_message_ = "SoA function template parameters cannot be empty.";
                return false;
            }
            if (function.requires_clause.has_value() &&
                function.requires_clause->find_first_not_of(" \t\r\n") == std::string::npos) {
                schema_edit_message_ = "SoA function requires clause cannot be empty.";
                return false;
            }
            auto const invalid_dependency{
                std::ranges::find_if(function.dependencies, [](auto const& dependency) {
                    return dependency.find_first_not_of(" \t\r\n") == std::string::npos;
                })};
            if (invalid_dependency != function.dependencies.end()) {
                schema_edit_message_ = "SoA function dependency key cannot be empty.";
                return false;
            }
            auto const invalid_parameter{
                std::ranges::find_if(function.parameters, [](auto const& parameter) {
                    return parameter.name.empty() || parameter.type.name.empty() ||
                           (parameter.default_value.has_value() &&
                            parameter.default_value->find_first_not_of(" \t\r\n") ==
                                std::string::npos);
                })};
            if (invalid_parameter != function.parameters.end()) {
                if (invalid_parameter->name.empty()) {
                    schema_edit_message_ = "SoA function parameter name cannot be empty.";
                } else if (invalid_parameter->type.name.empty()) {
                    schema_edit_message_ = "SoA function parameter type cannot be empty.";
                } else {
                    schema_edit_message_ = "SoA function parameter default cannot be empty.";
                }
                return false;
            }
        }
        auto const invalid_variant{
            std::ranges::find_if(pending->single_allocation_variants, [](auto const& variant) {
                return variant.name.empty() || variant.allocator.name.empty();
            })};
        if (invalid_variant != pending->single_allocation_variants.end()) {
            schema_edit_message_ = invalid_variant->name.empty()
                                     ? "Single-allocation variant owner type cannot be empty."
                                     : "Single-allocation variant allocator type cannot be empty.";
            return false;
        }
        if (apply_document_edit(
                ReplaceSoa{.declaration = *declaration, .schema = std::move(*pending)})) {
            if (analysis_session_.inputs.selection.soa_access_set_explicit &&
                renamed_member.has_value()) {
                auto const existing{analysis_session_.inputs.selection.soa_access_columns.find(
                    renamed_member->first)};
                if (existing != analysis_session_.inputs.selection.soa_access_columns.end()) {
                    auto const operation{existing->second};
                    analysis_session_.inputs.selection.soa_access_columns.erase(existing);
                    analysis_session_.inputs.selection.soa_access_columns.insert_or_assign(
                        renamed_member->second, operation);
                }
            }
            analysis_session_.inputs.selection.field = std::move(selected_after_edit);
            soa_editor_declaration_.reset();
            return true;
        }
    }
    return false;
}

} // namespace ioj::layout_planner
