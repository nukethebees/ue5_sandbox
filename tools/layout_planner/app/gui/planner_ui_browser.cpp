#include "planner_ui_browser_common.hpp"

#include <codegen/schema/declaration_schema.h>
#include <codegen/schema/facade_method_schemas.h>
#include <codegen/schema/facade_schemas.h>
#include <codegen/schema/homogeneous_layout_schemas.h>
#include <codegen/schema/homogeneous_value_schemas.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/module_handle.h>
#include <codegen/schema/module_kind.h>
#include <codegen/schema/module_settings_store.h>
#include <codegen/schema/normal_module_schemas.h>
#include <codegen/schema/record_schemas.h>
#include <codegen/schema/soa_backend.h>
#include <codegen/schema/static_table_column_schemas.h>
#include <codegen/schema/static_table_row_schemas.h>
#include <codegen/schema/static_table_schemas.h>
#include <codegen/schema/type_refs.h>
#include <codegen/sexpr/indices.h>
#include <codegen/sexpr/source_file_kind.h>
#include <lispb/compiler_storage.h>
#include <lispb/indices.h>
#include <lispb/path_base.h>
#include <lispb/project_edit_kind.h>
#include <lispb/project_edit_request.h>
#include <lispb/schema/type_definitions.h>
#include <lispb/target_kind.h>

#include <array>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ioj::layout_planner {

void PlannerUi::draw_project_panel() {
    if (!project_view_open_) {
        return;
    }
    auto const was_open{project_view_open_};
    ImGui::Begin("Project / Schema",
                 &project_view_open_,
                 ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_MenuBar);
    persist_view_visibility(was_open, project_view_open_);
    detail::pane_section_menu();
    if (!project_path_.empty()) {
        ImGui::PushTextWrapPos(0.0F);
        ImGui::TextDisabled("%s", project_path_.string().c_str());
        ImGui::PopTextWrapPos();
    }
    ImGui::BeginDisabled(project_path_.empty());
    if (ImGui::Button("Refresh project")) {
        if (has_dirty_changes()) {
            ImGui::OpenPopup("Discard edits and refresh?");
        } else {
            refresh_project();
        }
    }
    ImGui::EndDisabled();
    ImGui::SetItemTooltip("Reload the project and its LispB sources after external edits.");
    if (ImGui::BeginPopupModal(
            "Discard edits and refresh?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Refreshing discards unsaved schema and project edits.");
        if (ImGui::Button("Discard and refresh")) {
            refresh_project();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (project_document_.has_value()) {
        if (detail::section("Project sources")) {
            ImGui::PushTextWrapPos(0.0F);
            ImGui::TextDisabled("Target: %s", target_name_.c_str());
            ImGui::PopTextWrapPos();
            std::optional<std::filesystem::path> unregister_source;
            std::optional<lispb::ProjectEditRequest> rename_source;
            auto const& project{project_document_->project()};
            auto const project_target{project.find_target(target_name_)};
            if (project_target && project_target->kind() == lispb::TargetKind::cpp_schema) {
                for (auto const& source : project.sources(*project_target)) {
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
                    if (rename_project_source_ == source) {
                        ImGui::BeginDisabled(pending || schema_dirty);
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
                                rename_source = {.kind = lispb::ProjectEditKind::rename_source,
                                                 .target_name = target_name_,
                                                 .source = source,
                                                 .destination = rename_project_source_path_.data()};
                            }
                        } else if (deactivated) {
                            rename_project_source_.reset();
                        }
                        ImGui::EndDisabled();
                    } else {
                        auto const source_path{
                            (project_document_->project().root / source).lexically_normal()};
                        auto const selected{selected_source_view_path_ == source_path};
                        if (ImGui::Selectable(source_display.c_str(), selected)) {
                            selected_source_view_path_ = source_path;
                            auto const source_was_open{source_view_open_};
                            source_view_open_ = true;
                            reveal_source_view_ = true;
                            persist_view_visibility(source_was_open, source_view_open_);
                        }
                        ImGui::SetItemTooltip(
                            "%s",
                            pending ? "Click to view the pending source. Save before renaming."
                            : schema_dirty
                                ? "Click to view source. Save or undo schema edits before renaming."
                                : "Click to view source. Right-click or F2 to rename.");
                        auto begin_rename{ImGui::IsItemFocused() &&
                                          ImGui::IsKeyPressed(ImGuiKey_F2, false)};
                        if (ImGui::BeginPopupContextItem("source-actions")) {
                            if (ImGui::MenuItem("Rename", "F2", false, !pending && !schema_dirty)) {
                                begin_rename = true;
                            }
                            ImGui::EndPopup();
                        }
                        if (begin_rename && !pending && !schema_dirty) {
                            rename_project_source_ = source;
                            std::snprintf(rename_project_source_path_.data(),
                                          rename_project_source_path_.size(),
                                          "%s",
                                          source_label.c_str());
                            schema_edit_message_.clear();
                            focus_rename_project_source_ = true;
                        }
                    }
                    ImGui::PopID();
                }
            }
            if (unregister_source.has_value() && apply_project_edit(lispb::ProjectEditRequest{
                                                     .kind = lispb::ProjectEditKind::remove_source,
                                                     .target_name = target_name_,
                                                     .source = *unregister_source})) {
                schema_edit_message_ =
                    "Staged source unregistration. The source file will remain on disk after Save.";
            }
            if (unregister_source.has_value()) {
                rename_project_source_.reset();
            } else if (rename_source.has_value()) {
                if (apply_project_edit(*rename_source)) {
                    auto const& root{project_document_->project().root};
                    if (selected_source_view_path_ ==
                        (root / rename_source->source).lexically_normal()) {
                        selected_source_view_path_ =
                            (root / rename_source->destination).lexically_normal();
                    }
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
                if (apply_project_edit(
                        lispb::ProjectEditRequest{.kind = lispb::ProjectEditKind::add_source,
                                                  .target_name = target_name_,
                                                  .source = new_project_source_path_.data()})) {
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
                        lispb::ProjectEditRequest{.kind = lispb::ProjectEditKind::create_source,
                                                  .target_name = target_name_,
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

    detail::WrappingButtonRow module_expansion;
    if (module_expansion.button("Expand all modules")) {
        detail::request_expansion(detail::ExpansionDomain::modules, true);
    }
    if (module_expansion.button("Collapse all modules")) {
        detail::request_expansion(detail::ExpansionDomain::modules, false);
    }
    if (detail::section("Schema declarations", false, true)) {
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

        auto view{schema_file_view_ ? 1 : 0};
        ImGui::SetNextItemWidth(-1.0F);
        if (ImGui::Combo("##schema-browser-view", &view, "Modules\0Files\0")) {
            schema_file_view_ = view == 1;
        }
        auto const filter{std::string_view{schema_filter_.data()}};
        auto const modules{document_.has_value() ? std::span{document_->manifest().modules}
                                                 : std::span<lispb::codegen::ModuleHandle const>{}};
        std::optional<std::size_t> create_record_in_module;
        std::optional<std::size_t> requested_delete_module;
        std::optional<std::pair<DeclarationId, std::string>> rename_record;
        std::vector<std::size_t> module_order;
        auto source_label = [&](std::size_t const index) {
            auto const source{document_->module_source_file(index)};
            return source.has_value() ? document_->storage()
                                            .sources.filesystem_path(*source)
                                            .lexically_relative(project_path_.parent_path())
                                            .generic_string()
                                      : std::string{"Unsaved modules"};
        };
        for (std::size_t index{}; index < modules.size(); ++index) {
            if ((modules[index].kind() == lispb::codegen::ModuleKind::normal)) {
                module_order.push_back(index);
            }
        }
        std::ranges::sort(module_order, [&](auto const left, auto const right) {
            auto const left_source{schema_file_view_ ? source_label(left) : std::string{}};
            auto const right_source{schema_file_view_ ? source_label(right) : std::string{}};
            if (left_source != right_source) {
                return left_source < right_source;
            }
            return text(declarations()
                            .module_settings_store.name[declarations().module_settings_store.index(
                                declarations().module_settings(modules[left]))]) <
                   text(declarations()
                            .module_settings_store.name[declarations().module_settings_store.index(
                                declarations().module_settings(modules[right]))]);
        });
        std::optional<std::string> current_source;
        auto source_open{false};
        for (auto const module_index : module_order) {
            auto const& module{modules[module_index]};
            auto const normal{((module).kind() == lispb::codegen::ModuleKind::normal
                                   ? std::optional{module}
                                   : std::nullopt)};
            if (!normal.has_value()) {
                continue;
            }
            std::vector<DeclarationId> declaration_rows;
            for (auto const& declaration : document_->declarations().ordered) {
                if (document_->declarations()
                        .module_index[document_->declarations().index(declaration)] !=
                    module_index) {
                    continue;
                }
                auto const type{analysis_session_.inputs.workspace.types().find(
                    document_->declarations()
                        .identity[document_->declarations().index(declaration)])};
                if (type.has_value()) {
                    auto const& node{*type};
                    if (matches_filter(analysis_session_.inputs.workspace.types(), node, filter)) {
                        declaration_rows.push_back(declaration);
                    }
                } else {
                    auto const& schema{declarations().items(
                        declarations()
                            .normal_modules.declarations[declarations().normal_modules.index(
                                *normal)])[document_->declarations().declaration_index
                                               [document_->declarations().index(declaration)]]};
                    auto const search_text{
                        text(document_->declarations()
                                 .identity[document_->declarations().index(declaration)]
                                 .module_name) +
                        " " +
                        text(document_->declarations()
                                 .identity[document_->declarations().index(declaration)]
                                 .namespace_name) +
                        " " +
                        text(document_->declarations()
                                 .identity[document_->declarations().index(declaration)]
                                 .name) +
                        " " + std::string{lispb::codegen::declaration_head(schema)}};
                    if (detail::lowercase(search_text).find(detail::lowercase(filter)) !=
                        std::string::npos) {
                        declaration_rows.push_back(declaration);
                    }
                }
            }
            if (declaration_rows.empty() && !filter.empty()) {
                continue;
            }
            std::ranges::sort(declaration_rows, {}, [&](DeclarationId declaration) {
                return text(document_->declarations()
                                .identity[document_->declarations().index(declaration)]
                                .name);
            });
            if (schema_file_view_) {
                auto const source{source_label(module_index)};
                if (current_source != source) {
                    if (source_open) {
                        ImGui::TreePop();
                    }
                    current_source = source;
                    detail::prepare_expansion(source.c_str(), detail::ExpansionDomain::modules);
                    source_open = ImGui::TreeNodeEx(source.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
                    ImGui::SetItemTooltip("%s", source.c_str());
                }
                if (!source_open) {
                    continue;
                }
            }

            auto const label{module_label(declarations(), modules[module_index])};
            ImGui::PushID(
                text(declarations()
                         .module_settings_store.name[declarations().module_settings_store.index(
                             declarations().normal_modules.settings
                                 [declarations().normal_modules.index(*normal)])])
                    .c_str());
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
                auto const supports_soa{
                    declarations()
                        .normal_modules.soa_backend[declarations().normal_modules.index(*normal)] ==
                    lispb::codegen::SoaBackend::standard_library};
                ImGui::BeginDisabled(!supports_soa);
                add_declaration("SoA", new_soa_module_index_, open_new_soa_dialog_);
                ImGui::EndDisabled();
                if (!supports_soa && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                    ImGui::SetTooltip("SoA authoring requires a standard-library backend module.");
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Static table")) {
                    structural_creation_module_ = module_index;
                    structural_creation_draft_ = [&] {
                        auto const new_static_tables{declarations().static_tables.append()};
                        declarations().static_tables.name[declarations().static_tables.index(
                            new_static_tables)] = strings().intern("Table");
                        declarations().static_tables.rows[declarations().static_tables.index(
                            new_static_tables)] =
                            declarations().range<lispb::codegen::StaticTableRowId>({[&] {
                                auto const new_static_table_rows{
                                    declarations().static_table_rows.append()};
                                declarations()
                                    .static_table_rows.name[declarations().static_table_rows.index(
                                        new_static_table_rows)] = strings().intern("first");
                                return new_static_table_rows;
                            }()});
                        declarations().static_tables.columns[declarations().static_tables.index(
                            new_static_tables)] =
                            declarations().range<lispb::codegen::StaticTableColumnId>({[&] {
                                auto const new_static_table_columns{
                                    declarations().static_table_columns.append()};
                                declarations()
                                    .static_table_columns
                                    .name[declarations().static_table_columns.index(
                                        new_static_table_columns)] = strings().intern("value");
                                declarations()
                                    .static_table_columns
                                    .type[declarations().static_table_columns.index(
                                        new_static_table_columns)] = [&] {
                                    auto const new_type_refs{declarations().type_refs.append()};
                                    declarations().type_refs.name[declarations().type_refs.index(
                                        new_type_refs)] = strings().intern("int");
                                    return new_type_refs;
                                }();
                                return new_static_table_columns;
                            }()});
                        return new_static_tables;
                    }();
                    open_new_structural_dialog_ = true;
                }
                if (ImGui::MenuItem("Facade")) {
                    structural_creation_module_ = module_index;
                    structural_creation_draft_ = [&] {
                        auto const new_facades{declarations().facades.append()};
                        declarations().facades.name[declarations().facades.index(new_facades)] =
                            strings().intern("Facade");
                        declarations()
                            .facades.target_type[declarations().facades.index(new_facades)] = [&] {
                            auto const new_type_refs{declarations().type_refs.append()};

                            return new_type_refs;
                        }();
                        declarations()
                            .facades.target_member_name[declarations().facades.index(new_facades)] =
                            strings().intern("target_");
                        declarations().facades.methods[declarations().facades.index(new_facades)] =
                            declarations().range<lispb::codegen::FacadeMethodId>({[&] {
                                auto const new_facade_methods{
                                    declarations().facade_methods.append()};
                                declarations()
                                    .facade_methods.return_type[declarations().facade_methods.index(
                                        new_facade_methods)] = [&] {
                                    auto const new_type_refs{declarations().type_refs.append()};
                                    declarations().type_refs.name[declarations().type_refs.index(
                                        new_type_refs)] = strings().intern("void");
                                    return new_type_refs;
                                }();
                                return new_facade_methods;
                            }()});
                        return new_facades;
                    }();
                    open_new_structural_dialog_ = true;
                }
                if (ImGui::MenuItem("Homogeneous layout",
                                    nullptr,
                                    false,
                                    declarations()
                                        .module_settings_store
                                        .source[declarations().module_settings_store.index(
                                            declarations().normal_modules.settings
                                                [declarations().normal_modules.index(*normal)])]
                                        .has_value())) {
                    structural_creation_module_ = module_index;
                    structural_creation_draft_ = [&] {
                        auto const new_homogeneous_layouts{
                            declarations().homogeneous_layouts.append()};
                        declarations()
                            .homogeneous_layouts.name[declarations().homogeneous_layouts.index(
                                new_homogeneous_layouts)] = strings().intern("Layout");
                        declarations().homogeneous_layouts.components
                            [declarations().homogeneous_layouts.index(new_homogeneous_layouts)] =
                            declarations().range<lispb::IdentifierId>({strings().intern("x")});
                        declarations().homogeneous_layouts.value_types
                            [declarations().homogeneous_layouts.index(new_homogeneous_layouts)] =
                            declarations().range<lispb::codegen::HomogeneousValueId>({[&] {
                                auto const new_homogeneous_values{
                                    declarations().homogeneous_values.append()};
                                declarations()
                                    .homogeneous_values
                                    .type[declarations().homogeneous_values.index(
                                        new_homogeneous_values)] = [&] {
                                    auto const new_type_refs{declarations().type_refs.append()};
                                    declarations().type_refs.name[declarations().type_refs.index(
                                        new_type_refs)] = strings().intern("float");
                                    return new_type_refs;
                                }();
                                declarations()
                                    .homogeneous_values
                                    .suffix[declarations().homogeneous_values.index(
                                        new_homogeneous_values)] = strings().add("Float");
                                return new_homogeneous_values;
                            }()});
                        return new_homogeneous_layouts;
                    }();
                    open_new_structural_dialog_ = true;
                }
                if (!declarations()
                         .module_settings_store
                         .source[declarations().module_settings_store.index(
                             declarations().normal_modules.settings
                                 [declarations().normal_modules.index(*normal)])]
                         .has_value() &&
                    ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                    ImGui::SetTooltip(
                        "Homogeneous layouts require a module with a C++ source output.");
                }
                ImGui::MenuItem("Vector SoA", nullptr, false, false);
                ImGui::EndDisabled();
                ImGui::EndPopup();
            }
            ImGui::SameLine();
            detail::prepare_expansion(label.c_str(), detail::ExpansionDomain::modules);
            if (open_record_module_ == module_index) {
                ImGui::SetNextItemOpen(true, ImGuiCond_Always);
                open_record_module_.reset();
            }
            auto const open{ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_NoTreePushOnOpen)};
            if (ImGui::IsItemHovered() && !declaration_rows.empty() &&
                document_->declarations()
                    .source[document_->declarations().index(declaration_rows.front())]
                    .has_value()) {
                auto const file{
                    document_
                        ->source_span(
                            *document_->declarations()
                                 .source[document_->declarations().index(declaration_rows.front())])
                        .file};
                ImGui::SetTooltip(
                    "%s", document_->storage().sources.filesystem_path(file).string().c_str());
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
                if (declaration_rows.empty()) {
                    ImGui::PushTextWrapPos(0.0F);
                    ImGui::TextDisabled("Empty module; use its + button to add a declaration.");
                    ImGui::PopTextWrapPos();
                }
                for (auto const declaration : declaration_rows) {
                    auto const resolved_type{analysis_session_.inputs.workspace.types().find(
                        document_->declarations()
                            .identity[document_->declarations().index(declaration)])};
                    auto const& schema{declarations().items(
                        declarations()
                            .normal_modules.declarations[declarations().normal_modules.index(
                                *normal)])[document_->declarations().declaration_index
                                               [document_->declarations().index(declaration)]]};
                    auto const node{resolved_type.has_value() ? std::optional{*resolved_type}
                                                              : std::nullopt};
                    auto const capabilities{
                        node.has_value() ? declaration_capabilities(
                                               analysis_session_.inputs.workspace.types(), *node)
                                         : declaration_capabilities(schema)};
                    if (!capabilities.inspectable) {
                        ImGui::TextDisabled(
                            "%s  [%s, read-only]",
                            text(document_->declarations()
                                     .identity[document_->declarations().index(declaration)]
                                     .name)
                                .c_str(),
                            std::string{lispb::codegen::declaration_head(schema)}.c_str());
                        ImGui::SetItemTooltip(
                            "This declaration could not be resolved for planner inspection.");
                        continue;
                    }
                    auto const kind_label{declaration_kind_label(capabilities.kind)};
                    auto const item_label{
                        text(document_->declarations()
                                 .identity[document_->declarations().index(declaration)]
                                 .name) +
                        "  [" + kind_label + (capabilities.editable ? "]" : ", view only]")};
                    auto const selected{
                        node.has_value()
                            ? analysis_session_.inputs.selection.type == resolved_type
                            : analysis_session_.inputs.selection.declaration == declaration ||
                                  (analysis_session_.inputs.selection.type.has_value() &&
                                   analysis_session_.inputs.workspace.types().source_declaration(
                                       *analysis_session_.inputs.selection.type) == schema)};
                    ImGui::PushID(static_cast<int>(declaration.value));
                    auto const editing_record{inline_record_rename_ == declaration};
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
                            } else if (node.has_value() &&
                                       text(analysis_session_.inputs.workspace.types().name(
                                           *node)) == inline_record_name_.data()) {
                                inline_record_rename_.reset();
                            } else {
                                rename_record = {declaration, inline_record_name_.data()};
                            }
                        }
                    } else if (ImGui::Selectable(item_label.c_str(), selected)) {
                        if (resolved_type.has_value()) {
                            select_type(*resolved_type);
                        } else {
                            select_declaration(declaration);
                        }
                        analysis_session_.inputs.selection.field.clear();
                        analysis_session_.inputs.selection.packed_access_fields.clear();
                        analysis_session_.inputs.selection.packed_access_set_explicit = false;
                        analysis_session_.inputs.selection.record_access_members.clear();
                        analysis_session_.inputs.selection.record_access_set_explicit = false;
                        packed_dragged_divider_.reset();
                        packed_dragged_variant_id_.reset();
                        if (node.has_value() &&
                            ((analysis_session_.inputs.workspace.types().definition(*node))
                                 .kind() == lispb::schema::TypeDefinitionKind::record)) {
                            inline_record_rename_ = declaration;
                            std::snprintf(
                                inline_record_name_.data(),
                                inline_record_name_.size(),
                                "%s",
                                text(analysis_session_.inputs.workspace.types().name(*node))
                                    .c_str());
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

        if (source_open) {
            ImGui::TreePop();
        }

        if (requested_delete_module.has_value()) {
            delete_module_index_ = *requested_delete_module;
            delete_module_name_ =
                text(declarations()
                         .module_settings_store.name[declarations().module_settings_store.index(
                             declarations().module_settings(modules[*delete_module_index_]))]);
            schema_edit_message_.clear();
            ImGui::OpenPopup("Delete module?");
        }
        if (ImGui::BeginPopupModal("Delete module?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            if (delete_module_index_.has_value() && *delete_module_index_ < modules.size()) {
                auto const declaration_count{std::ranges::count_if(
                    document_->declarations().ordered, [&](DeclarationId const declaration) {
                        return document_->declarations()
                                   .module_index[document_->declarations().index(declaration)] ==
                             *delete_module_index_;
                    })};
                ImGui::TextWrapped("Delete module '%s' and its %lld declaration(s)?",
                                   delete_module_name_.c_str(),
                                   static_cast<long long>(declaration_count));
                ImGui::TextWrapped(
                    "Only this module's LispB form is removed. Its source file remains on disk. "
                    "File > Undo restores the module until you save.");
                if (ImGui::Button("Delete", {120.0F, 0.0F})) {
                    if (apply_document_edit(document_->delete_module(*delete_module_index_))) {
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
            auto const& module{modules[*create_record_in_module]};
            auto name{std::string{"Record"}};
            auto suffix{2};
            while (std::ranges::find(
                       declarations().items(
                           declarations()
                               .normal_modules
                               .declarations[declarations().normal_modules.index(module)]),
                       strings().intern(name),
                       [&](auto id) {
                           return lispb::codegen::declaration_name(declarations(), id);
                       }) !=
                   declarations()
                       .items(declarations()
                                  .normal_modules
                                  .declarations[declarations().normal_modules.index(module)])
                       .end()) {
                name = "Record" + std::to_string(suffix++);
            }
            auto const identity{TypeIdentity{
                .origin = TypeOrigin::declaration,
                .module_name =
                    declarations()
                        .module_settings_store.name[declarations().module_settings_store.index(
                            declarations().normal_modules.settings
                                [declarations().normal_modules.index(module)])],
                .namespace_name = declarations()
                                      .module_settings_store
                                      .namespace_name[declarations().module_settings_store.index(
                                          declarations().normal_modules.settings
                                              [declarations().normal_modules.index(module)])]
                                      .value_or(lispb::IdentifierId{}),
                .name = strings().intern(name)}};
            auto const id{document_->allocate_declaration_id()};
            if (apply_document_edit(
                    document_->create_declaration(
                        id,
                        *create_record_in_module,
                        [&] {
                            auto const new_records{declarations().records.append()};
                            declarations().records.name[declarations().records.index(new_records)] =
                                strings().intern(name);
                            return new_records;
                        }(),
                        std::nullopt),
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
            auto const declaration{document_->declarations().find(rename_record->first)};
            if (declaration.has_value()) {
                auto identity{document_->declarations()
                                  .identity[document_->declarations().index(*declaration)]};
                identity.name = strings().intern(rename_record->second);
                if (apply_document_edit(
                        document_->rename_declaration(rename_record->first, rename_record->second),
                        identity)) {
                    inline_record_rename_.reset();
                    rename_editor_declaration_.reset();
                }
            }
        }
    }

    draw_new_structural_dialog();
    auto const dependencies{external_dependencies(analysis_session_.inputs.workspace.types(),
                                                  &analysis_session_.primary_abi())};
    auto const external_label{"External dependencies (" + std::to_string(dependencies.size()) +
                              ")###external-dependencies"};
    if (detail::section(external_label.c_str())) {
        ImGui::InputTextWithHint("##external-filter",
                                 "Filter types or registrations",
                                 external_filter_.data(),
                                 external_filter_.size());
        ImGui::Checkbox("Include described types", &external_show_described_);
        ImGui::Checkbox("Include types with no LispB references", &external_show_unreferenced_);
        if (ImGui::BeginCombo("Used by module",
                              external_module_filter_.empty() ? "All modules"
                                                              : external_module_filter_.c_str())) {
            if (ImGui::Selectable("All modules", external_module_filter_.empty())) {
                external_module_filter_.clear();
            }
            if (document_.has_value()) {
                for (auto const& module : document_->manifest().modules) {
                    auto const& name{text(
                        declarations()
                            .module_settings_store.name[declarations().module_settings_store.index(
                                declarations().module_settings(module))])};
                    if (ImGui::Selectable(name.c_str(), name == external_module_filter_)) {
                        external_module_filter_ = name;
                    }
                }
            }
            ImGui::EndCombo();
        }
        auto const filter{detail::lowercase(external_filter_.data())};
        std::size_t shown{};
        for (auto const& entry : dependencies) {
            if ((!external_show_described_ && entry.semantics_known) ||
                (!external_show_unreferenced_ && entry.uses.empty()) ||
                (!external_module_filter_.empty() &&
                 std::ranges::find(entry.modules, external_module_filter_) ==
                     entry.modules.end())) {
                continue;
            }
            auto searchable{entry.cpp_spelling};
            for (auto const& name : entry.registered_names) {
                searchable += " @" + name;
            }
            if (!filter.empty() &&
                detail::lowercase(searchable).find(filter) == std::string::npos) {
                continue;
            }
            ++shown;
            auto const selected{
                analysis_session_.inputs.selection.type.has_value() &&
                std::ranges::find(entry.types, *analysis_session_.inputs.selection.type) !=
                    entry.types.end()};
            auto const label{entry.cpp_spelling +
                             (entry.semantics_known ? " [described]" : " [opaque]") + " — " +
                             std::to_string(entry.declarations.size()) + " declarations"};
            ImGui::PushID(entry.cpp_spelling.c_str());
            if (ImGui::Selectable(label.c_str(), selected)) {
                select_type(entry.types.front());
            }
            auto const layout_known{entry.physical_facts.has_value()};
            auto tooltip{searchable +
                         "\nTarget size/alignment: " + (layout_known ? "known" : "unknown")};
            if (entry.uses.empty()) {
                tooltip += "\nNo LispB references found.";
            }
            for (auto const& module : entry.modules) {
                tooltip += "\nUsed by module: " + module;
            }
            ImGui::SetItemTooltip("%s", tooltip.c_str());
            ImGui::PopID();
        }
        if (shown == 0) {
            ImGui::TextDisabled("No external types match these filters.");
        }
        ImGui::TextWrapped(
            "External types can remain external. Opaque types have no declared semantic "
            "description. Usage covers explicit LispB references, not arbitrary C++ code.");
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
    if (!document_.has_value() ||
        std::ranges::none_of(document_->source_files(), [&](auto const source) {
            return document_->storage().sources.kind(source) ==
                   lispb::codegen::sexpr::SourceFileKind::module;
        })) {
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
    if (new_module_source_file_index_ >= sources.size() ||
        document_->storage().sources.kind(sources[new_module_source_file_index_]) !=
            lispb::codegen::sexpr::SourceFileKind::module) {
        new_module_source_file_index_ = static_cast<std::size_t>(
            std::ranges::find(sources,
                              lispb::codegen::sexpr::SourceFileKind::module,
                              [&](auto file) { return document_->storage().sources.kind(file); }) -
            sources.begin());
    }
    auto const source_label{document_->storage()
                                .sources.filesystem_path(sources[new_module_source_file_index_])
                                .filename()
                                .string()};
    if (ImGui::BeginCombo("LispB source", source_label.c_str())) {
        for (std::size_t index{}; index < sources.size(); ++index) {
            if (document_->storage().sources.kind(sources[index]) !=
                lispb::codegen::sexpr::SourceFileKind::module) {
                continue;
            }
            auto const label{document_->storage().sources.filesystem_path(sources[index]).string()};
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
        auto const header_key{lispb::codegen::output_path_key(header)};
        for (auto const& candidate : document_->manifest().modules) {
            auto const settings{declarations().module_settings(candidate)};
            if (strings().view(declarations()
                                   .module_settings_store
                                   .name[declarations().module_settings_store.index(settings)]) ==
                module_name) {
                conflict = "A module with this name already exists.";
            } else if (lispb::codegen::output_path_key(text(
                           declarations()
                               .module_settings_store
                               .header[declarations().module_settings_store.index(settings)])) ==
                           header_key ||
                       (declarations()
                            .module_settings_store
                            .source[declarations().module_settings_store.index(settings)] &&
                        lispb::codegen::output_path_key(text(
                            *declarations()
                                 .module_settings_store
                                 .source[declarations().module_settings_store.index(settings)])) ==
                            header_key)) {
                conflict = "Another module already uses this generated output path.";
            }
            if (!conflict.empty()) {
                break;
            }
        }
    }

    auto output_location_known{false};
    if (conflict.empty() && project_document_.has_value()) {
        auto const& project{project_document_->project()};
        auto const target{project.find_target(target_name_)};
        if (target && target->kind() == lispb::TargetKind::cpp_schema) {
            auto const output_root{project.output_path(*target)};
            if (output_root.base == lispb::PathBase::project) {
                output_location_known = true;
                std::error_code error;
                auto const output_path{project.root / output_root.path / header};
                if (std::filesystem::exists(output_path, error)) {
                    conflict = "A file already exists at " + output_path.string() +
                               ". Choose another header.";
                } else if (error) {
                    conflict = "Cannot inspect generated header destination: " + error.message();
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
        auto const settings{[&] {
            auto const new_module_settings_store{declarations().module_settings_store.append()};
            declarations().module_settings_store.name[declarations().module_settings_store.index(
                new_module_settings_store)] = strings().intern(new_module_name_.data());
            declarations().module_settings_store.header[declarations().module_settings_store.index(
                new_module_settings_store)] = strings().add(header.generic_string());
            declarations().module_settings_store.source[declarations().module_settings_store.index(
                new_module_settings_store)] = std::nullopt;
            declarations()
                .module_settings_store.header_include[declarations().module_settings_store.index(
                    new_module_settings_store)] = std::nullopt;
            declarations()
                .module_settings_store.namespace_name[declarations().module_settings_store.index(
                    new_module_settings_store)] =
                new_module_namespace_.front() == '\0'
                    ? std::nullopt
                    : std::optional{strings().intern(new_module_namespace_.data())};
            declarations()
                .module_settings_store.include_order[declarations().module_settings_store.index(
                    new_module_settings_store)] = {};
            declarations()
                .module_settings_store.prelude_lines[declarations().module_settings_store.index(
                    new_module_settings_store)] = {};
            return new_module_settings_store;
        }()};
        auto module{[&] {
            auto const new_normal_modules{declarations().normal_modules.append()};
            declarations()
                .normal_modules.settings[declarations().normal_modules.index(new_normal_modules)] =
                settings;
            declarations().normal_modules.soa_backend[declarations().normal_modules.index(
                new_normal_modules)] = new_module_backend_ == 0
                                         ? lispb::codegen::SoaBackend::unreal
                                         : lispb::codegen::SoaBackend::standard_library;
            return new_normal_modules;
        }()};
        if (apply_document_edit(
                document_->create_module(sources[new_module_source_file_index_], module))) {
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
