#include "planner_ui_properties_common.hpp"

#include <misc/cpp/imgui_stdlib.h>

namespace ioj::layout_planner {
namespace {

void edit_optional_text(char const* label, std::optional<std::string>& value) {
    ImGui::PushID(label);
    auto enabled{value.has_value()};
    if (ImGui::Checkbox("##enabled", &enabled)) {
        value = enabled ? std::optional{std::string{}} : std::nullopt;
    }
    ImGui::SameLine();
    if (value.has_value()) {
        ImGui::InputText(label, &*value);
    } else {
        ImGui::TextUnformatted(label);
    }
    ImGui::PopID();
}

template <typename T, typename Draw>
void edit_items(char const* label, std::vector<T>& items, Draw const& draw) {
    if (!detail::section(label)) {
        return;
    }
    ImGui::PushID(label);
    std::optional<std::size_t> erase;
    std::optional<std::size_t> move_up;
    std::optional<std::size_t> move_down;
    auto const count{items.size()};
    for (std::size_t index{}; index < count; ++index) {
        ImGui::PushID(static_cast<int>(index));
        ImGui::Separator();
        ImGui::Text("%llu", static_cast<unsigned long long>(index + 1));
        ImGui::SameLine();
        if (ImGui::SmallButton("Remove")) {
            erase = index;
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(index == 0);
        if (ImGui::SmallButton("Up")) {
            move_up = index;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(index + 1 == count);
        if (ImGui::SmallButton("Down")) {
            move_down = index;
        }
        ImGui::EndDisabled();
        draw(items[index]);
        ImGui::PopID();
    }
    if (erase.has_value()) {
        items.erase(items.begin() + static_cast<std::ptrdiff_t>(*erase));
    } else if (move_up.has_value()) {
        std::swap(items[*move_up], items[*move_up - 1]);
    } else if (move_down.has_value()) {
        std::swap(items[*move_down], items[*move_down + 1]);
    }
    if (ImGui::SmallButton("Add")) {
        items.emplace_back();
    }
    ImGui::PopID();
}

void edit_strings(char const* label, std::vector<std::string>& values) {
    edit_items(label, values, [](auto& value) { ImGui::InputText("Value", &value); });
}

} // namespace

void PlannerUi::draw_structural_fields(codegen::DeclarationSchema& draft,
                                       TypeIdentity const& identity) {
    auto type = [&](char const* label, codegen::TypeRef& reference) {
        ImGui::PushID(label);
        ImGui::InputText(label, &reference.name);
        ImGui::SameLine();
        if (auto const picked{draw_type_picker(identity.module_name, identity)}) {
            reference.name = *picked;
        }
        ImGui::PopID();
    };
    auto optional_type = [&](char const* label, std::optional<codegen::TypeRef>& reference) {
        ImGui::PushID(label);
        auto enabled{reference.has_value()};
        if (ImGui::Checkbox("Enabled", &enabled)) {
            reference = enabled ? std::optional{codegen::TypeRef{}} : std::nullopt;
        }
        if (reference.has_value()) {
            type(label, *reference);
        } else {
            ImGui::SameLine();
            ImGui::TextUnformatted(label);
        }
        ImGui::PopID();
    };
    std::visit(
        [&](auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, codegen::StaticTableSchema>) {
                edit_optional_text("Export specifier", value.export_specifier);
                edit_items(
                    "Rows", value.rows, [](auto& row) { ImGui::InputText("Name", &row.name); });
                edit_items("Columns", value.columns, [&](auto& column) {
                    ImGui::InputText("Name", &column.name);
                    type("Type", column.type);
                });
                edit_items("Groups", value.groups, [&](auto& group) {
                    ImGui::InputText("Name", &group.name);
                    type("Result type", group.type);
                    edit_strings("Columns", group.columns);
                });
            } else if constexpr (std::is_same_v<T, codegen::HomogeneousLayoutSchema>) {
                edit_optional_text("Export specifier", value.export_specifier);
                edit_strings("Components", value.components);
                edit_strings("Input members", value.input_members);
                edit_items("Value types", value.value_types, [&](auto& item) {
                    ImGui::InputText("Suffix", &item.suffix);
                    type("Element type", item.type);
                    optional_type("Equivalent type", item.equivalent_type);
                    edit_items(
                        "Input types", item.input_types, [&](auto& input) { type("Type", input); });
                });
            } else if constexpr (std::is_same_v<T, codegen::FacadeSchema>) {
                type("Target type", value.target_type);
                ImGui::InputText("Target member", &value.target_member_name);
                ImGui::Checkbox("Reference binding", &value.reference_target);
                ImGui::Checkbox("Definitions in source", &value.definitions_in_source);
                edit_optional_text("Export specifier", value.export_specifier);
                auto access = [](char const* label, std::string& selection) {
                    if (ImGui::BeginCombo(label, selection.c_str())) {
                        for (auto const* choice : {"public", "private"}) {
                            if (ImGui::Selectable(choice, selection == choice)) {
                                selection = choice;
                            }
                        }
                        ImGui::EndCombo();
                    }
                };
                access("Binding access", value.bind_access);
                access("Method access", value.method_access);
                ImGui::InputText("Friend kind", &value.friend_kind);
                edit_strings("Friends", value.friends);
                edit_strings("Validation lines", value.validation_lines);
                edit_strings("Validation dependencies", value.validation_dependencies);
                edit_items("Methods", value.methods, [&](auto& method) {
                    ImGui::InputText("Name", &method.name);
                    type("Return type", method.return_type);
                    edit_optional_text("Target method", method.target_name);
                    ImGui::Checkbox("Const", &method.is_const);
                    ImGui::Checkbox("Noexcept", &method.is_noexcept);
                    edit_items("Parameters", method.parameters, [&](auto& parameter) {
                        ImGui::InputText("Name", &parameter.name);
                        type("Type", parameter.type);
                        edit_optional_text("Default value", parameter.default_value);
                    });
                });
            }
        },
        draft);
}

void PlannerUi::draw_new_structural_dialog() {
    if (std::exchange(open_new_structural_dialog_, false)) {
        schema_edit_message_.clear();
        ImGui::OpenPopup("New declaration");
    }
    ImGui::SetNextWindowSize({640.0F, 600.0F}, ImGuiCond_FirstUseEver);
    if (!ImGui::BeginPopupModal("New declaration")) {
        return;
    }
    if (!document_.has_value() || !structural_creation_module_.has_value() ||
        *structural_creation_module_ >= document_->manifest().modules.size() ||
        !structural_creation_draft_.has_value()) {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }
    auto const& module{std::get<codegen::NormalModuleSchema>(
        document_->manifest().modules[*structural_creation_module_])};
    auto& name{std::visit([](auto& value) -> std::string& { return value.name; },
                          *structural_creation_draft_)};
    TypeIdentity identity{.module_name = module.settings.name,
                          .namespace_name = module.settings.namespace_name.value_or(""),
                          .name = name};
    ImGui::Text("Module: %s", identity.module_name.c_str());
    ImGui::InputText("Name", &name);
    identity.name = name;
    ImGui::BeginDisabled(project_history_active());
    draw_structural_fields(*structural_creation_draft_, identity);
    if (ImGui::Button("Create")) {
        auto const id{document_->allocate_declaration_id()};
        if (apply_document_edit(CreateDeclaration{.declaration = id,
                                                  .module_index = *structural_creation_module_,
                                                  .schema = *structural_creation_draft_},
                                identity)) {
            open_record_module_ = *structural_creation_module_;
            structural_creation_draft_.reset();
            structural_creation_module_.reset();
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        structural_creation_draft_.reset();
        structural_creation_module_.reset();
        ImGui::CloseCurrentPopup();
    }
    if (!schema_edit_message_.empty()) {
        ImGui::TextWrapped("%s", schema_edit_message_.c_str());
    }
    ImGui::EndPopup();
}

auto PlannerUi::draw_structural_declaration(DeclarationInfo const& info) -> bool {
    auto const& module{
        std::get<codegen::NormalModuleSchema>(document_->manifest().modules[info.module_index])};
    auto const& schema{module.declarations[info.declaration_index]};
    if (!std::holds_alternative<codegen::StaticTableSchema>(schema) &&
        !std::holds_alternative<codegen::FacadeSchema>(schema) &&
        !std::holds_alternative<codegen::HomogeneousLayoutSchema>(schema)) {
        return false;
    }
    auto const revision{document_->revision()};
    if (structural_draft_identity_ != info.identity || structural_draft_revision_ != revision ||
        !structural_draft_.has_value()) {
        structural_draft_ = schema;
        structural_draft_identity_ = info.identity;
        structural_draft_revision_ = revision;
    }

    ImGui::Text("%s [%s]",
                info.identity.name.c_str(),
                std::string{codegen::declaration_head(schema)}.c_str());
    ImGui::TextUnformatted(info.identity.module_name.c_str());
    if (info.source.has_value()) {
        auto const& path{document_->source_files()[info.source->source_file_index].path};
        ImGui::TextWrapped("Source: %s:%llu",
                           path.string().c_str(),
                           static_cast<unsigned long long>(info.source->line));
        if (ImGui::SmallButton("View source")) {
            selected_source_view_path_ = path.lexically_normal();
            source_view_open_ = true;
            focus_source_view_ = true;
        }
    }
    if (!schema_edit_message_.empty()) {
        ImGui::TextWrapped("%s", schema_edit_message_.c_str());
    }
    auto const& types{analysis_session_.inputs.workspace.types()};
    if (auto const selected{analysis_session_.inputs.selection.type};
        selected.has_value() && detail::section("Semantic structure")) {
        auto const& node{types.type(*selected)};
        ImGui::TextUnformatted(node.cpp_spelling.c_str());
        if (auto const* table{std::get_if<StaticTableType>(&node.definition)}) {
            ImGui::Text("Rows: %llu", static_cast<unsigned long long>(table->rows.size()));
            ImGui::TextUnformatted("Backend: Unreal TStaticArray columns");
            for (auto const& column : table->columns) {
                ImGui::BulletText(
                    "%s: %s", column.name.c_str(), column.semantic_type.cpp_type.spelling.c_str());
            }
            for (auto const& group : table->groups) {
                ImGui::BulletText("Group %s returns %s",
                                  group.name.c_str(),
                                  group.result_type.cpp_type.spelling.c_str());
            }
        } else if (auto const* facade{std::get_if<FacadeType>(&node.definition)}) {
            ImGui::Text("Target: %s (%s)",
                        facade->target.cpp_type.spelling.c_str(),
                        facade->reference_target ? "reference" : "pointer");
            ImGui::Text("Target member: %s", facade->target_member_name.c_str());
            for (auto const& method : facade->methods) {
                ImGui::BulletText("%s returns %s%s%s (forwards to %s)",
                                  method.name.c_str(),
                                  method.return_type.cpp_type.spelling.c_str(),
                                  method.is_const ? " const" : "",
                                  method.is_noexcept ? " noexcept" : "",
                                  method.target_name.c_str());
                for (auto const& parameter : method.parameters) {
                    ImGui::Text("    %s: %s",
                                parameter.name.c_str(),
                                parameter.type.cpp_type.spelling.c_str());
                }
            }
        } else if (auto const* storage{std::get_if<HomogeneousStorageType>(&node.definition)}) {
            ImGui::TextUnformatted("Backend: Unreal TArray storage");
            ImGui::Text("Element type: %s", storage->value_type.cpp_type.spelling.c_str());
            for (auto const& component : storage->components) {
                ImGui::BulletText("Component: %s", component.c_str());
            }
            if (storage->equivalent_type.has_value()) {
                ImGui::Text("Equivalent type: %s",
                            storage->equivalent_type->cpp_type.spelling.c_str());
            }
            for (auto const& input : storage->input_types) {
                ImGui::BulletText("Input type: %s", input.cpp_type.spelling.c_str());
            }
        }
    }
    if (detail::section("Generated types")) {
        auto const generated{types.types_for_declaration(info.identity)};
        for (auto const id : generated) {
            auto const& node{types.type(id)};
            if (ImGui::Selectable(node.cpp_spelling.c_str(),
                                  analysis_session_.inputs.selection.type == id)) {
                select_type(id);
            }
        }
        if (auto const* layout{std::get_if<codegen::HomogeneousLayoutSchema>(&schema)}) {
            ImGui::Text("View template: T%sView", layout->name.c_str());
            if (!layout->value_types.empty() &&
                layout->value_types.front().equivalent_type.has_value()) {
                ImGui::Text("Equivalent-type trait: T%sEquivalentType", layout->name.c_str());
            }
            ImGui::TextWrapped("Select a concrete storage type to inspect its graph relationships. "
                               "View templates have no single physical layout.");
        }
    }
    draw_declaration_dependencies(info.identity);
    if (auto const selected{analysis_session_.inputs.selection.type}) {
        draw_type_users(*selected);
    }
    ImGui::TextWrapped(
        "Physical layout analysis is unavailable for this generated backend representation.");

    ImGui::BeginDisabled(project_history_active());
    if (detail::section("Declaration actions")) {
        if (rename_editor_declaration_ != info.id) {
            rename_editor_declaration_ = info.id;
            std::snprintf(declaration_name_.data(),
                          declaration_name_.size(),
                          "%s",
                          info.identity.name.c_str());
        }
        ImGui::InputText("Name", declaration_name_.data(), declaration_name_.size());
        if (ImGui::Button("Rename")) {
            auto identity{info.identity};
            identity.name = declaration_name_.data();
            static_cast<void>(
                apply_document_edit(RenameDeclaration{info.id, identity.name}, identity));
            ImGui::EndDisabled();
            return true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Duplicate")) {
            auto copy{schema};
            auto name{info.identity.name + "Copy"};
            for (std::size_t suffix{2};
                 std::ranges::find(module.declarations, name, codegen::declaration_name) !=
                 module.declarations.end();
                 ++suffix) {
                name = info.identity.name + "Copy" + std::to_string(suffix);
            }
            std::visit([&](auto& value) { value.name = name; }, copy);
            auto identity{info.identity};
            identity.name = name;
            static_cast<void>(apply_document_edit(
                CreateDeclaration{.declaration = document_->allocate_declaration_id(),
                                  .module_index = info.module_index,
                                  .schema = std::move(copy)},
                identity));
            ImGui::EndDisabled();
            return true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete...")) {
            ImGui::OpenPopup("Delete structural declaration?");
        }
        if (ImGui::BeginPopupModal(
                "Delete structural declaration?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Delete %s? File > Undo restores applied deletions.",
                        info.identity.name.c_str());
            if (ImGui::Button("Delete")) {
                auto const applied{apply_document_edit(DeleteDeclaration{info.id})};
                if (applied) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
                ImGui::EndDisabled();
                return true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
            }
            if (!schema_edit_message_.empty()) {
                ImGui::TextWrapped("%s", schema_edit_message_.c_str());
            }
            ImGui::EndPopup();
        }
        std::optional<std::size_t> destination;
        if (ImGui::BeginCombo("Module", module.settings.name.c_str())) {
            auto const& modules{document_->manifest().modules};
            for (std::size_t index{}; index < modules.size(); ++index) {
                if (auto const* normal{std::get_if<codegen::NormalModuleSchema>(&modules[index])};
                    normal != nullptr && index != info.module_index) {
                    if (ImGui::Selectable(normal->settings.name.c_str())) {
                        destination = index;
                    }
                }
            }
            ImGui::EndCombo();
        }
        if (destination.has_value()) {
            auto const& settings{
                std::get<codegen::NormalModuleSchema>(document_->manifest().modules[*destination])
                    .settings};
            auto identity{info.identity};
            identity.module_name = settings.name;
            identity.namespace_name = settings.namespace_name.value_or("");
            static_cast<void>(apply_document_edit(
                MoveDeclaration{.declaration = info.id, .module_index = *destination}, identity));
            ImGui::EndDisabled();
            return true;
        }
    }
    if (detail::section("Edit declaration")) {
        draw_structural_fields(*structural_draft_, info.identity);
        if (ImGui::Button("Apply declaration changes")) {
            auto const applied{apply_document_edit(
                ReplaceDeclaration{.declaration = info.id, .schema = *structural_draft_},
                info.identity)};
            if (applied) {
                structural_draft_.reset();
            }
            ImGui::EndDisabled();
            return true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset draft")) {
            structural_draft_ = schema;
            schema_edit_message_.clear();
        }
        ImGui::TextWrapped("Apply validates the complete declaration and records one undoable "
                           "edit. Save writes applied changes to LispB.");
    }
    ImGui::EndDisabled();
    return true;
}

} // namespace ioj::layout_planner
