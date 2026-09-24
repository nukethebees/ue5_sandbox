#include "planner_ui_browser_common.hpp"

namespace ioj::layout_planner {

void PlannerUi::draw_project_panel() {
    if (!project_view_open_) {
        return;
    }
    auto const was_open{project_view_open_};
    ImGui::Begin("Project / Schema", &project_view_open_, ImGuiWindowFlags_HorizontalScrollbar);
    persist_view_visibility(was_open, project_view_open_);
    if (!project_path_.empty()) {
        ImGui::PushTextWrapPos(0.0F);
        ImGui::TextDisabled("%s", project_path_.string().c_str());
        ImGui::PopTextWrapPos();
    }
    if (project_document_.has_value()) {
        if (detail::section("Project sources")) {
            ImGui::PushTextWrapPos(0.0F);
            ImGui::TextDisabled("Target: %s", target_name_.c_str());
            ImGui::PopTextWrapPos();
            std::optional<std::filesystem::path> unregister_source;
            std::optional<lispb::RenameCppSchemaSource> rename_source;
            auto const target_found{project_document_->project().targets.find(target_name_)};
            auto const* project_target{
                target_found == project_document_->project().targets.end()
                    ? nullptr
                    : std::get_if<lispb::CppSchemaTarget>(&target_found->second)};
            if (project_target != nullptr) {
                for (auto const& source : project_target->sources) {
                    auto const source_label{source.generic_string()};
                    auto const pending{project_document_->source_is_pending(source)};
                    auto const renamed_from{project_document_->renamed_source_original(source)};
                    ImGui::PushID(source_label.c_str());
                    auto source_display{source_label};
                    if (pending) {
                        source_display += " (pending new)";
                    } else if (renamed_from.has_value()) {
                        source_display +=
                            " (rename pending from " + renamed_from->generic_string() + ")";
                    }
                    auto const schema_dirty{document_.has_value() && document_->dirty()};
                    ImGui::BeginDisabled(pending || renamed_from.has_value() || schema_dirty);
                    if (ImGui::SmallButton("-")) {
                        unregister_source = source;
                    }
                    ImGui::EndDisabled();
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                        ImGui::SetTooltip(
                            "%s",
                            pending ? "Undo the pending creation to remove this unpublished file."
                            : renamed_from.has_value()
                                ? "Undo the staged rename before unregistering this file."
                            : schema_dirty
                                ? "Save or discard schema edits before unregistering this file."
                                : "Unregister this file. The file remains on disk.");
                    }
                    ImGui::SameLine();
                    ImGui::BeginDisabled(pending || schema_dirty);
                    if (rename_project_source_ == source) {
                        if (focus_rename_project_source_) {
                            ImGui::SetKeyboardFocusHere();
                            focus_rename_project_source_ = false;
                        }
                        ImGui::SetNextItemWidth(-1.0F);
                        auto const submitted{
                            ImGui::InputText("##source-name",
                                             rename_project_source_path_.data(),
                                             rename_project_source_path_.size(),
                                             ImGuiInputTextFlags_EnterReturnsTrue |
                                                 ImGuiInputTextFlags_AutoSelectAll)};
                        auto const deactivated{ImGui::IsItemDeactivated()};
                        if ((ImGui::IsItemActive() || deactivated) &&
                            ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                            rename_project_source_.reset();
                        } else if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                            if (rename_project_source_path_.front() == '\0') {
                                schema_edit_message_ = "Source path cannot be empty.";
                                focus_rename_project_source_ = true;
                            } else if (std::filesystem::path{rename_project_source_path_.data()} ==
                                       source) {
                                rename_project_source_.reset();
                            } else {
                                rename_source = {.target_name = target_name_,
                                                 .source = source,
                                                 .destination = rename_project_source_path_.data()};
                            }
                        } else if (deactivated) {
                            rename_project_source_.reset();
                        }
                    } else {
                        ImGui::TextWrapped("%s", source_display.c_str());
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip(
                                "Double-click to rename. Enter applies; Escape cancels.");
                            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                                rename_project_source_ = source;
                                std::snprintf(rename_project_source_path_.data(),
                                              rename_project_source_path_.size(),
                                              "%s",
                                              source_label.c_str());
                                schema_edit_message_.clear();
                                focus_rename_project_source_ = true;
                            }
                        }
                    }
                    ImGui::EndDisabled();
                    ImGui::PopID();
                }
            }
            if (unregister_source.has_value() &&
                apply_project_edit(lispb::RemoveCppSchemaSource{.target_name = target_name_,
                                                                .source = *unregister_source})) {
                schema_edit_message_ =
                    "Staged source unregistration. The source file will remain on disk after Save.";
            }
            if (unregister_source.has_value()) {
                rename_project_source_.reset();
            } else if (rename_source.has_value()) {
                if (apply_project_edit(*rename_source)) {
                    if (rename_project_source_ == rename_source->source) {
                        rename_project_source_.reset();
                        rename_project_source_path_.fill('\0');
                    }
                    schema_edit_message_ =
                        "Staged source rename. Save to move the file and reload.";
                } else {
                    focus_rename_project_source_ = true;
                }
            }
            ImGui::SetNextItemWidth(-1.0F);
            ImGui::InputTextWithHint("##new-project-source",
                                     "relative/path/to/new-source.lispb",
                                     new_project_source_path_.data(),
                                     new_project_source_path_.size());
            detail::WrappingButtonRow source_actions;
            auto const schema_dirty{document_.has_value() && document_->dirty()};
            ImGui::BeginDisabled(new_project_source_path_.front() == '\0' || schema_dirty);
            if (source_actions.button("Register existing")) {
                if (apply_project_edit(lispb::AddCppSchemaSource{
                        .target_name = target_name_, .source = new_project_source_path_.data()})) {
                    new_project_source_path_.fill('\0');
                    source_view_open_ = true;
                    focus_source_view_ = true;
                    schema_edit_message_ =
                        "Staged an existing LispB source registration. Preview it, then Save to "
                        "reload.";
                }
            }
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip(
                    "The existing source is validated with the complete target before the draft is "
                    "accepted.");
            }
            ImGui::BeginDisabled(new_project_source_path_.front() == '\0' || schema_dirty);
            if (source_actions.button("Create empty")) {
                if (apply_project_edit(
                        lispb::CreateCppSchemaSource{.target_name = target_name_,
                                                     .source = new_project_source_path_.data(),
                                                     .contents = {}})) {
                    new_project_source_path_.fill('\0');
                    source_view_open_ = true;
                    focus_source_view_ = true;
                    schema_edit_message_ = "Staged a new empty LispB source. Preview it, then Save "
                                           "to publish and reload.";
                }
            }
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("The destination stays absent until explicit Save. The first "
                                  "module is authored "
                                  "through + New module after reload.");
            }
            if (project_history_active()) {
                ImGui::PushTextWrapPos(0.0F);
                ImGui::TextDisabled("Project source-list draft active; schema editing resumes "
                                    "after Save or discard.");
                ImGui::PopTextWrapPos();
                if (ImGui::SmallButton("Discard project source draft")) {
                    auto loaded{load_lispb_schema(project_path_, target_name_)};
                    if (loaded.loaded) {
                        adopt_loaded_schema(std::move(loaded));
                        schema_edit_message_ = "Discarded the project source-list draft.";
                    } else {
                        schema_edit_message_ = loaded.diagnostics.empty()
                                                 ? "Could not reload the LispB project."
                                                 : loaded.diagnostics.front().message;
                    }
                }
            }
        }
    }

    if (detail::section("Schema declarations")) {
        ImGui::BeginDisabled(!document_.has_value() || project_history_active());
        detail::WrappingButtonRow declaration_buttons;
        if (declaration_buttons.button("+ New module")) {
            declaration_after_new_module_.reset();
            module_initiated_dialog_ = false;
            open_new_module_dialog_ = true;
        }
        ImGui::EndDisabled();
        if (!schema_edit_message_.empty()) {
            ImGui::TextWrapped("%s", schema_edit_message_.c_str());
        }
        ImGui::SetNextItemWidth(-1.0F);
        ImGui::InputTextWithHint(
            "##schema-filter", "Filter declarations", schema_filter_.data(), schema_filter_.size());

        if (!document_.has_value()) {
            ImGui::TextDisabled("No schema loaded.");
        }

        auto const filter{std::string_view{schema_filter_.data()}};
        auto const modules{document_.has_value() ? std::span{document_->manifest().modules}
                                                 : std::span<codegen::ModuleSchema const>{}};
        std::optional<std::size_t> create_record_in_module;
        std::optional<std::size_t> requested_delete_module;
        std::optional<std::pair<DeclarationId, std::string>> rename_record;
        for (std::size_t module_index{}; module_index < modules.size(); ++module_index) {
            auto const& module{modules[module_index]};
            auto const* normal{std::get_if<codegen::NormalModuleSchema>(&module)};
            if (normal == nullptr) {
                continue;
            }
            std::vector<DeclarationInfo const*> declarations;
            for (auto const& declaration : document_->declarations()) {
                if (declaration.module_index != module_index) {
                    continue;
                }
                auto const type{
                    analysis_session_.inputs.workspace.types().find(declaration.identity)};
                if (type.has_value()) {
                    auto const& node{analysis_session_.inputs.workspace.types().type(*type)};
                    if (matches_filter(node, filter)) {
                        declarations.push_back(&declaration);
                    }
                } else {
                    auto const& schema{normal->declarations[declaration.declaration_index]};
                    auto const search_text{declaration.identity.module_name + " " +
                                           declaration.identity.namespace_name + " " +
                                           declaration.identity.name + " " +
                                           std::string{codegen::declaration_head(schema)}};
                    if (lowercase(search_text).find(lowercase(filter)) != std::string::npos) {
                        declarations.push_back(&declaration);
                    }
                }
            }
            if (declarations.empty() && !filter.empty()) {
                continue;
            }
            std::ranges::sort(declarations, {}, &DeclarationInfo::declaration_index);

            auto const label{module_label(modules[module_index])};
            ImGui::PushID(static_cast<int>(module_index));
            ImGui::BeginDisabled(project_history_active());
            if (ImGui::SmallButton("+")) {
                ImGui::OpenPopup("add-declaration");
            }
            auto const add_menu_position{
                ImVec2{ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y}};
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip(
                    project_history_active()
                        ? "Save or discard project source changes before adding declarations."
                        : "Add declaration");
            }
            auto add_declaration =
                [&](char const* item_label, std::size_t& selected_module_index, bool& open_dialog) {
                    if (ImGui::MenuItem(item_label)) {
                        selected_module_index = module_index;
                        module_initiated_dialog_ = true;
                        open_dialog = true;
                        return true;
                    }
                    return false;
                };
            ImGui::SetNextWindowPos(add_menu_position, ImGuiCond_Appearing);
            if (ImGui::BeginPopup("add-declaration")) {
                ImGui::BeginDisabled(project_history_active());
                if (add_declaration("Enum", new_enum_module_index_, open_new_enum_dialog_)) {
                    pending_packed_enum_binding_.reset();
                }
                add_declaration(
                    "Packed value", new_packed_module_index_, open_new_packed_value_dialog_);
                add_declaration("Integer scalar",
                                new_integer_scalar_module_index_,
                                open_new_integer_scalar_dialog_);
                add_declaration("Quantization",
                                new_linear_quantized_module_index_,
                                open_new_linear_quantized_dialog_);
                add_declaration(
                    "Varint", new_integer_varint_module_index_, open_new_integer_varint_dialog_);
                add_declaration(
                    "Fixed point", new_fixed_point_module_index_, open_new_fixed_point_dialog_);
                add_declaration(
                    "Mini float", new_mini_float_module_index_, open_new_mini_float_dialog_);
                add_declaration("Optional",
                                new_optional_sentinel_module_index_,
                                open_new_optional_sentinel_dialog_);
                add_declaration("Presence optional",
                                new_optional_presence_bit_module_index_,
                                open_new_optional_presence_bit_dialog_);
                if (ImGui::MenuItem("Record")) {
                    create_record_in_module = module_index;
                }
                add_declaration("Union", new_union_module_index_, open_new_union_dialog_);
                add_declaration(
                    "Tagged union", new_tagged_union_module_index_, open_new_tagged_union_dialog_);
                auto const supports_soa{normal->soa_backend ==
                                        codegen::SoaBackend::standard_library};
                ImGui::BeginDisabled(!supports_soa);
                add_declaration("SoA", new_soa_module_index_, open_new_soa_dialog_);
                ImGui::EndDisabled();
                if (!supports_soa && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                    ImGui::SetTooltip("SoA authoring requires a standard-library backend module.");
                }
                ImGui::Separator();
                for (auto const* kind :
                     {"Vector SoA", "Homogeneous layout", "Static table", "Facade"}) {
                    ImGui::MenuItem(kind, nullptr, false, false);
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                        ImGui::SetTooltip("Create this declaration in the LispB source; planner "
                                          "authoring is not yet available.");
                    }
                }
                ImGui::EndDisabled();
                ImGui::EndPopup();
            }
            ImGui::SameLine();
            if (open_record_module_ == module_index) {
                ImGui::SetNextItemOpen(true, ImGuiCond_Always);
                open_record_module_.reset();
            }
            auto const open{ImGui::TreeNodeEx(label.c_str(),
                                              ImGuiTreeNodeFlags_DefaultOpen |
                                                  ImGuiTreeNodeFlags_NoTreePushOnOpen)};
            if (ImGui::IsItemHovered() && !declarations.empty() &&
                declarations.front()->source.has_value()) {
                auto const source_index{declarations.front()->source->source_file_index};
                if (source_index < document_->source_files().size()) {
                    ImGui::SetTooltip(
                        "%s", document_->source_files()[source_index].path.string().c_str());
                }
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("...")) {
                ImGui::OpenPopup("module-actions");
            }
            auto const actions_menu_position{
                ImVec2{ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y}};
            ImGui::SetItemTooltip("Module actions");
            ImGui::SetNextWindowPos(actions_menu_position, ImGuiCond_Appearing);
            if (ImGui::BeginPopup("module-actions")) {
                if (ImGui::MenuItem("Delete module",
                                    nullptr,
                                    false,
                                    !project_history_active() && modules.size() > 1)) {
                    requested_delete_module = module_index;
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                    if (modules.size() == 1) {
                        ImGui::SetTooltip("A LispB project must contain at least one module.");
                    } else if (project_history_active()) {
                        ImGui::SetTooltip(
                            "Save or discard project source changes before deleting modules.");
                    }
                }
                ImGui::EndPopup();
            }
            if (open) {
                ImGui::TreePush("declarations");
                if (declarations.empty()) {
                    ImGui::PushTextWrapPos(0.0F);
                    ImGui::TextDisabled("Empty module; use its + button to add a declaration.");
                    ImGui::PopTextWrapPos();
                }
                for (auto const* declaration : declarations) {
                    auto const resolved_type{
                        analysis_session_.inputs.workspace.types().find(declaration->identity)};
                    auto const& schema{normal->declarations[declaration->declaration_index]};
                    auto const* node{
                        resolved_type.has_value()
                            ? &analysis_session_.inputs.workspace.types().type(*resolved_type)
                            : nullptr};
                    auto const capabilities{node != nullptr ? declaration_capabilities(*node)
                                                            : declaration_capabilities(schema)};
                    if (!capabilities.inspectable) {
                        ImGui::TextDisabled("%s  [%s, read-only]",
                                            declaration->identity.name.c_str(),
                                            std::string{codegen::declaration_head(schema)}.c_str());
                        ImGui::SetItemTooltip(
                            "This declaration could not be resolved for planner inspection.");
                        continue;
                    }
                    auto const kind_label{declaration_kind_label(capabilities.kind)};
                    auto const item_label{declaration->identity.name + "  [" + kind_label +
                                          (capabilities.editable ? "]" : ", view only]")};
                    auto const selected{
                        node != nullptr
                            ? analysis_session_.inputs.selection.type == resolved_type
                            : analysis_session_.inputs.selection.declaration == declaration->id};
                    ImGui::PushID(static_cast<int>(declaration->id.value));
                    auto const editing_record{inline_record_rename_ == declaration->id};
                    if (editing_record) {
                        if (focus_inline_record_rename_) {
                            ImGui::SetKeyboardFocusHere();
                            focus_inline_record_rename_ = false;
                        }
                        ImGui::SetNextItemWidth(-1.0F);
                        auto const submitted{
                            ImGui::InputText("##record-name",
                                             inline_record_name_.data(),
                                             inline_record_name_.size(),
                                             ImGuiInputTextFlags_EnterReturnsTrue)};
                        if (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                            inline_record_rename_.reset();
                        } else if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                            if (inline_record_name_.front() == '\0') {
                                schema_edit_message_ = "Record name cannot be empty.";
                            } else if (node != nullptr &&
                                       node->identity.name == inline_record_name_.data()) {
                                inline_record_rename_.reset();
                            } else {
                                rename_record = {declaration->id, inline_record_name_.data()};
                            }
                        }
                    } else if (ImGui::Selectable(item_label.c_str(), selected)) {
                        if (resolved_type.has_value()) {
                            select_type(*resolved_type);
                        } else {
                            select_declaration(declaration->id);
                        }
                        analysis_session_.inputs.selection.field.clear();
                        analysis_session_.inputs.selection.packed_access_fields.clear();
                        analysis_session_.inputs.selection.packed_access_set_explicit = false;
                        analysis_session_.inputs.selection.record_access_members.clear();
                        analysis_session_.inputs.selection.record_access_set_explicit = false;
                        packed_dragged_divider_.reset();
                        packed_dragged_variant_id_.reset();
                        if (node != nullptr &&
                            std::holds_alternative<RecordType>(node->definition)) {
                            inline_record_rename_ = declaration->id;
                            std::snprintf(inline_record_name_.data(),
                                          inline_record_name_.size(),
                                          "%s",
                                          node->identity.name.c_str());
                            focus_inline_record_rename_ = true;
                        } else {
                            inline_record_rename_.reset();
                        }
                    }
                    auto const state{resolved_type.has_value() &&
                                             capabilities.physical_analysis_available
                                         ? analysis_session_.status(*resolved_type)
                                         : LayoutStatus::unknown};
                    auto const* status{!capabilities.physical_analysis_available
                                           ? "physical analysis unavailable"
                                       : state == LayoutStatus::available ? "analysis available"
                                       : state == LayoutStatus::unknown   ? "layout facts unknown"
                                                                          : "analysis error"};
                    ImGui::TextDisabled("%s", status);
                    ImGui::PopID();
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        if (requested_delete_module.has_value()) {
            delete_module_index_ = *requested_delete_module;
            delete_module_name_ =
                std::visit([](auto const& module) { return module.settings.name; },
                           modules[*delete_module_index_]);
            schema_edit_message_.clear();
            ImGui::OpenPopup("Delete module?");
        }
        if (ImGui::BeginPopupModal("Delete module?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            if (delete_module_index_.has_value() && *delete_module_index_ < modules.size()) {
                auto const declaration_count{std::ranges::count_if(
                    document_->declarations(), [&](DeclarationInfo const& declaration) {
                        return declaration.module_index == *delete_module_index_;
                    })};
                ImGui::TextWrapped("Delete module '%s' and its %lld declaration(s)?",
                                   delete_module_name_.c_str(),
                                   static_cast<long long>(declaration_count));
                ImGui::TextWrapped(
                    "Only this module's LispB form is removed. Its source file remains on disk. "
                    "File > Undo restores the module until you save.");
                if (ImGui::Button("Delete", {120.0F, 0.0F})) {
                    if (apply_document_edit(DeleteModule{.module_index = *delete_module_index_})) {
                        delete_module_index_.reset();
                        delete_module_name_.clear();
                        ImGui::CloseCurrentPopup();
                        ImGui::EndPopup();
                        ImGui::End();
                        return;
                    }
                }
                ImGui::SameLine();
            }
            if (ImGui::Button("Cancel", {120.0F, 0.0F})) {
                delete_module_index_.reset();
                delete_module_name_.clear();
                ImGui::CloseCurrentPopup();
            }
            if (!schema_edit_message_.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, {0.95F, 0.45F, 0.35F, 1.0F});
                ImGui::TextWrapped("%s", schema_edit_message_.c_str());
                ImGui::PopStyleColor();
            }
            ImGui::EndPopup();
        }

        if (create_record_in_module.has_value()) {
            auto const& module{
                std::get<codegen::NormalModuleSchema>(modules[*create_record_in_module])};
            auto name{std::string{"Record"}};
            auto suffix{2};
            while (std::ranges::find(module.declarations, name, codegen::declaration_name) !=
                   module.declarations.end()) {
                name = "Record" + std::to_string(suffix++);
            }
            auto const identity{
                TypeIdentity{.origin = TypeOrigin::declaration,
                             .module_name = module.settings.name,
                             .namespace_name = module.settings.namespace_name.value_or(""),
                             .name = name}};
            auto const id{document_->allocate_declaration_id()};
            if (apply_document_edit(
                    CreateRecord{.declaration = id,
                                 .module_index = *create_record_in_module,
                                 .schema = codegen::RecordSchema{.name = name,
                                                                 .members = {},
                                                                 .export_specifier = std::nullopt},
                                 .insertion_index = std::nullopt},
                    identity)) {
                schema_filter_.fill('\0');
                analysis_session_.inputs.selection.field.clear();
                inline_record_rename_ = id;
                open_record_module_ = *create_record_in_module;
                std::snprintf(
                    inline_record_name_.data(), inline_record_name_.size(), "%s", name.c_str());
                focus_inline_record_rename_ = true;
            }
        } else if (rename_record.has_value()) {
            auto const* declaration{document_->declaration(rename_record->first)};
            if (declaration != nullptr) {
                auto identity{declaration->identity};
                identity.name = rename_record->second;
                if (apply_document_edit(RenameDeclaration{.declaration = rename_record->first,
                                                          .new_name = rename_record->second},
                                        identity)) {
                    inline_record_rename_.reset();
                    rename_editor_declaration_.reset();
                }
            }
        }
    }

    if (!load_diagnostics_.empty() && detail::section("Load diagnostics")) {
        draw_diagnostics(load_diagnostics_);
    }
    ImGui::End();
}

void PlannerUi::draw_new_module_dialog() {
    if (open_new_module_dialog_) {
        ImGui::OpenPopup("New editable module");
        open_new_module_dialog_ = false;
    }
    if (!ImGui::BeginPopupModal(
            "New editable module", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    if (!document_.has_value() || document_->source_files().size() < 2) {
        ImGui::TextDisabled("No loaded LispB module source can receive a new module.");
        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return;
    }

    constexpr std::array backends{"Unreal", "Standard library"};
    ImGui::Combo(
        "SoA backend", &new_module_backend_, backends.data(), static_cast<int>(backends.size()));
    if (ImGui::InputText("Module name", new_module_name_.data(), new_module_name_.size()) &&
        new_module_header_.front() == '\0') {
        confirm_unchecked_module_header_ = false;
    }
    auto const module_name{std::string{new_module_name_.data()}};
    auto const default_header{module_name + ".h"};
    if (ImGui::InputTextWithHint("Generated header",
                                 default_header.c_str(),
                                 new_module_header_.data(),
                                 new_module_header_.size())) {
        confirm_unchecked_module_header_ = false;
    }
    ImGui::InputText(
        "Namespace (optional)", new_module_namespace_.data(), new_module_namespace_.size());

    auto const sources{document_->source_files()};
    if (new_module_source_file_index_ == 0 || new_module_source_file_index_ >= sources.size()) {
        new_module_source_file_index_ = 1;
    }
    auto const source_label{sources[new_module_source_file_index_].path.filename().string()};
    if (ImGui::BeginCombo("LispB source", source_label.c_str())) {
        for (std::size_t index{1}; index < sources.size(); ++index) {
            auto const label{sources[index].path.string()};
            if (ImGui::Selectable(label.c_str(), index == new_module_source_file_index_)) {
                new_module_source_file_index_ = index;
            }
        }
        ImGui::EndCombo();
    }
    if (declaration_after_new_module_.has_value()) {
        ImGui::TextDisabled("Creates this module, then opens the requested declaration form.");
    } else {
        ImGui::TextDisabled(
            "Creates a valid empty destination; add declarations with the normal New controls.");
    }

    auto const header{std::filesystem::path{new_module_header_.front() == '\0'
                                                ? default_header
                                                : std::string{new_module_header_.data()}}};
    std::string conflict;
    if (module_name.empty()) {
        conflict = "Enter a module name.";
    } else if (new_module_header_.front() == '\0' &&
               !std::ranges::all_of(module_name, [](unsigned char const character) {
                   return std::isalnum(character) != 0 || character == '_' || character == '-';
               })) {
        conflict = "Enter an explicit header for this module name.";
    } else if (header.is_absolute() || header.has_root_path() ||
               std::ranges::find(header, std::filesystem::path{".."}) != header.end() ||
               header.filename().empty() || header.filename() == ".") {
        conflict = "Generated header must be a relative file path within the output root.";
    }

    if (conflict.empty()) {
        auto const header_key{codegen::output_path_key(header)};
        for (auto const& candidate : document_->manifest().modules) {
            std::visit(
                [&](auto const& module) {
                    if (module.settings.name == module_name) {
                        conflict = "A module with this name already exists.";
                    } else if (codegen::output_path_key(module.settings.header) == header_key ||
                               (module.settings.source.has_value() &&
                                codegen::output_path_key(*module.settings.source) == header_key)) {
                        conflict = "Another module already uses this generated output path.";
                    }
                },
                candidate);
            if (!conflict.empty()) {
                break;
            }
        }
    }

    auto output_location_known{false};
    if (conflict.empty() && project_document_.has_value()) {
        auto const& project{project_document_->project()};
        auto const target{project.targets.find(target_name_)};
        if (target != project.targets.end()) {
            if (auto const* schema{std::get_if<lispb::CppSchemaTarget>(&target->second)}) {
                if (schema->output_root.base == lispb::PathBase::project) {
                    output_location_known = true;
                    std::error_code error;
                    auto const output_path{project.root / schema->output_root.path / header};
                    if (std::filesystem::exists(output_path, error)) {
                        conflict = "A file already exists at " + output_path.string() +
                                   ". Choose another header.";
                    } else if (error) {
                        conflict =
                            "Cannot inspect generated header destination: " + error.message();
                    }
                }
            }
        }
    }
    if (conflict.empty() && !output_location_known) {
        ImGui::TextColored(ImVec4{1.0F, 0.85F, 0.2F, 1.0F},
                           "Output location cannot be checked; check for an existing file first.");
        ImGui::Checkbox("I checked the output path", &confirm_unchecked_module_header_);
    }
    if (!conflict.empty()) {
        ImGui::TextColored(ImVec4{1.0F, 0.85F, 0.2F, 1.0F}, "%s", conflict.c_str());
    }

    auto const ready{
        conflict.empty() && (output_location_known || confirm_unchecked_module_header_) &&
        new_module_backend_ >= 0 && new_module_backend_ < static_cast<int>(backends.size())};
    ImGui::BeginDisabled(!ready);
    if (ImGui::Button("Create")) {
        auto const settings{codegen::ModuleSettings{
            .name = new_module_name_.data(),
            .header = header,
            .source = std::nullopt,
            .header_include = std::nullopt,
            .namespace_name = new_module_namespace_.front() == '\0'
                                ? std::nullopt
                                : std::optional<std::string>{new_module_namespace_.data()},
            .include_order = {},
            .prelude_lines = {}}};
        auto module{codegen::NormalModuleSchema{
            .settings = settings,
            .soa_backend = new_module_backend_ == 0 ? codegen::SoaBackend::unreal
                                                    : codegen::SoaBackend::standard_library}};
        if (apply_document_edit(CreateModule{.source_file_index = new_module_source_file_index_,
                                             .schema = std::move(module)})) {
            schema_warning_message_.clear();
            new_module_name_.fill('\0');
            new_module_header_.fill('\0');
            new_module_namespace_.fill('\0');
            confirm_unchecked_module_header_ = false;
            ImGui::CloseCurrentPopup();
            if (declaration_after_new_module_.has_value()) {
                module_initiated_dialog_ = false;
                switch (*declaration_after_new_module_) {
                    case NewDeclarationDialog::enumeration:
                        open_new_enum_dialog_ = true;
                        break;
                    case NewDeclarationDialog::packed_value:
                        open_new_packed_value_dialog_ = true;
                        break;
                    case NewDeclarationDialog::integer_scalar:
                        open_new_integer_scalar_dialog_ = true;
                        break;
                    case NewDeclarationDialog::quantization:
                        open_new_linear_quantized_dialog_ = true;
                        break;
                    case NewDeclarationDialog::varint:
                        open_new_integer_varint_dialog_ = true;
                        break;
                    case NewDeclarationDialog::fixed_point:
                        open_new_fixed_point_dialog_ = true;
                        break;
                    case NewDeclarationDialog::mini_float:
                        open_new_mini_float_dialog_ = true;
                        break;
                    case NewDeclarationDialog::optional_sentinel:
                        open_new_optional_sentinel_dialog_ = true;
                        break;
                    case NewDeclarationDialog::optional_presence_bit:
                        open_new_optional_presence_bit_dialog_ = true;
                        break;
                    case NewDeclarationDialog::record:
                        open_new_record_dialog_ = true;
                        break;
                    case NewDeclarationDialog::union_value:
                        open_new_union_dialog_ = true;
                        break;
                    case NewDeclarationDialog::tagged_union:
                        open_new_tagged_union_dialog_ = true;
                        break;
                    case NewDeclarationDialog::soa:
                        open_new_soa_dialog_ = true;
                        break;
                }
                declaration_after_new_module_.reset();
            }
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        declaration_after_new_module_.reset();
        new_module_name_.fill('\0');
        new_module_header_.fill('\0');
        new_module_namespace_.fill('\0');
        confirm_unchecked_module_header_ = false;
        ImGui::CloseCurrentPopup();
    }
    if (!schema_edit_message_.empty()) {
        ImGui::TextWrapped("%s", schema_edit_message_.c_str());
    }
    ImGui::EndPopup();
}

} // namespace ioj::layout_planner
