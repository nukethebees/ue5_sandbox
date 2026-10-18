#include "planner_ui_properties_common.hpp"

#include <codegen/ast/access.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/record_kind.h>
#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/declaration_kind.h>
#include <codegen/schema/declaration_schema.h>
#include <codegen/schema/facade_method_schemas.h>
#include <codegen/schema/facade_schemas.h>
#include <codegen/schema/homogeneous_layout_schemas.h>
#include <codegen/schema/homogeneous_value_schemas.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/module_kind.h>
#include <codegen/schema/module_settings_store.h>
#include <codegen/schema/normal_module_schemas.h>
#include <codegen/schema/parameter_schemas.h>
#include <codegen/schema/schema_range.h>
#include <codegen/schema/static_table_column_schemas.h>
#include <codegen/schema/static_table_group_schemas.h>
#include <codegen/schema/static_table_row_schemas.h>
#include <codegen/schema/static_table_schemas.h>
#include <codegen/schema/type_refs.h>
#include <lispb/compiler_storage.h>
#include <lispb/indices.h>
#include <lispb/schema/facade_types.h>
#include <lispb/schema/homogeneous_storage_types.h>
#include <lispb/schema/static_table_types.h>
#include <lispb/strings.h>

#include <misc/cpp/imgui_stdlib.h>

#include <cstddef>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace ioj::layout_planner {
namespace {

auto edit_text(lispb::Strings& strings, char const* label, lispb::IdentifierId& id) -> bool {
    auto value{std::string{strings.view(id)}};
    if (!ImGui::InputText(label, &value)) {
        return false;
    }
    id = strings.intern(value);
    return true;
}

auto edit_text(lispb::Strings& strings, char const* label, lispb::StringId& id) -> bool {
    auto value{std::string{strings.view(id)}};
    if (!ImGui::InputText(label, &value)) {
        return false;
    }
    id = strings.add(value);
    return true;
}

template <typename Id>
auto edit_optional_text(lispb::Strings& strings, char const* label, std::optional<Id>& value)
    -> bool {
    ImGui::PushID(label);
    auto enabled{value.has_value()};
    auto changed{ImGui::Checkbox("##enabled", &enabled)};
    if (changed) {
        value = enabled ? std::optional{Id{}} : std::nullopt;
    }
    ImGui::SameLine();
    if (value) {
        changed |= edit_text(strings, label, *value);
    } else {
        ImGui::TextUnformatted(label);
    }
    ImGui::PopID();
    return changed;
}

template <typename Id, typename Draw, typename Create>
auto edit_items(lispb::codegen::Declarations& store,
                char const* label,
                lispb::codegen::SchemaRange<Id>& range,
                Draw const& draw,
                Create const& create) -> bool {
    if (!detail::section(label)) {
        return false;
    }
    ImGui::PushID(label);
    auto const source{store.items(range)};
    std::vector<Id> items{source.begin(), source.end()};
    bool changed{};
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
        changed |= draw(items[index]);
        ImGui::PopID();
    }
    if (erase) {
        items.erase(items.begin() + static_cast<std::ptrdiff_t>(*erase));
        changed = true;
    } else if (move_up) {
        std::swap(items[*move_up], items[*move_up - 1]);
        changed = true;
    } else if (move_down) {
        std::swap(items[*move_down], items[*move_down + 1]);
        changed = true;
    }
    if (ImGui::SmallButton("Add")) {
        items.push_back(create());
        changed = true;
    }
    if (changed) {
        range = store.range(items);
    }
    ImGui::PopID();
    return changed;
}

template <typename Id, typename Field, typename Edit>
auto edit_column(auto& columns, Id& id, std::vector<Field>& column, Edit const& edit) -> bool {
    auto value{column[columns.index(id)]};
    if (!edit(value)) {
        return false;
    }
    auto const replacement{columns.clone(id)};
    column[columns.index(replacement)] = std::move(value);
    id = replacement;
    return true;
}

auto edit_flag(char const* label, std::uint8_t& value) -> bool {
    auto enabled{value != 0};
    if (!ImGui::Checkbox(label, &enabled)) {
        return false;
    }
    value = enabled;
    return true;
}

template <typename Id, typename Draw>
auto edit_records(lispb::codegen::Declarations& store,
                  auto& columns,
                  char const* label,
                  lispb::codegen::SchemaRange<Id>& range,
                  Draw const& draw) -> bool {
    return edit_items(
        store,
        label,
        range,
        [&](Id& id) {
            auto record{id};
            if (!draw(record)) {
                return false;
            }
            id = record;
            return true;
        },
        [&] { return columns.append(); });
}

template <typename Id>
auto edit_strings(lispb::codegen::Declarations& store,
                  char const* label,
                  lispb::codegen::SchemaRange<Id>& range) -> bool {
    return edit_items(
        store,
        label,
        range,
        [&](Id& id) { return edit_text(store.strings(), "Value", id); },
        [] { return Id{}; });
}

auto renamed_structural(lispb::codegen::Declarations& store,
                        lispb::codegen::DeclarationHandle const declaration,
                        lispb::IdentifierId const name) -> lispb::codegen::DeclarationHandle {
    switch (declaration.kind()) {
        case lispb::codegen::DeclarationKind::static_table: {
            auto value{store.static_tables.clone(declaration)};
            store.static_tables.name[store.static_tables.index(value)] = name;
            return value;
        }
        case lispb::codegen::DeclarationKind::homogeneous_layout: {
            auto value{store.homogeneous_layouts.clone(declaration)};
            store.homogeneous_layouts.name[store.homogeneous_layouts.index(value)] = name;
            return value;
        }
        case lispb::codegen::DeclarationKind::facade: {
            auto value{store.facades.clone(declaration)};
            store.facades.name[store.facades.index(value)] = name;
            return value;
        }
        default:
            std::unreachable();
    }
}

} // namespace

void PlannerUi::draw_structural_fields(lispb::codegen::DeclarationHandle& draft,
                                       TypeIdentity const& identity) {
    auto& store{declarations()};
    auto type = [&](char const* label, lispb::codegen::TypeRefId& id) {
        ImGui::PushID(label);
        auto name{id.value == 0 ? lispb::IdentifierId{}
                                : store.type_refs.name[store.type_refs.index(id)]};
        auto changed{edit_text(strings(), label, name)};
        ImGui::SameLine();
        if (auto const picked{draw_type_picker(identity.module_name, identity)}) {
            name = strings().intern(*picked);
            changed = true;
        }
        if (changed) {
            auto const replacement{id.value == 0 ? store.type_refs.append()
                                                 : store.type_refs.clone(id)};
            store.type_refs.name[store.type_refs.index(replacement)] = name;
            id = replacement;
        }
        ImGui::PopID();
        return changed;
    };
    auto optional_type = [&](char const* label,
                             std::optional<lispb::codegen::TypeRefId>& reference) {
        ImGui::PushID(label);
        auto enabled{reference.has_value()};
        auto changed{ImGui::Checkbox("Enabled", &enabled)};
        if (changed) {
            reference = enabled ? std::optional{[&] {
                auto const new_type_refs{store.type_refs.append()};

                return new_type_refs;
            }()}
                                : std::nullopt;
        }
        if (reference) {
            changed |= type(label, *reference);
        } else {
            ImGui::SameLine();
            ImGui::TextUnformatted(label);
        }
        ImGui::PopID();
        return changed;
    };
    if (auto value{((draft).kind() == lispb::codegen::DeclarationKind::static_table
                        ? std::optional{draft}
                        : std::nullopt)}) {
        auto changed{
            edit_text(strings(),
                      "Export specifier",
                      store.static_tables.export_specifier[store.static_tables.index(*value)])};
        changed |= edit_records(store,
                                store.static_table_rows,
                                "Rows",
                                store.static_tables.rows[store.static_tables.index(*value)],
                                [&](auto& row) {
                                    return edit_column(store.static_table_rows,
                                                       row,
                                                       store.static_table_rows.name,
                                                       [&](auto& changed_value) {
                                                           return edit_text(
                                                               strings(), "Name", changed_value);
                                                       });
                                });
        changed |= edit_records(
            store,
            store.static_table_columns,
            "Columns",
            store.static_tables.columns[store.static_tables.index(*value)],
            [&](auto& column) {
                auto edited{edit_column(store.static_table_columns,
                                        column,
                                        store.static_table_columns.name,
                                        [&](auto& changed_value) {
                                            return edit_text(strings(), "Name", changed_value);
                                        })};
                edited |=
                    edit_column(store.static_table_columns,
                                column,
                                store.static_table_columns.type,
                                [&](auto& changed_value) { return type("Type", changed_value); });
                return edited;
            });
        changed |= edit_records(
            store,
            store.static_table_groups,
            "Groups",
            store.static_tables.groups[store.static_tables.index(*value)],
            [&](auto& group) {
                auto edited{edit_column(store.static_table_groups,
                                        group,
                                        store.static_table_groups.name,
                                        [&](auto& changed_value) {
                                            return edit_text(strings(), "Name", changed_value);
                                        })};
                edited |= edit_column(
                    store.static_table_groups,
                    group,
                    store.static_table_groups.type,
                    [&](auto& changed_value) { return type("Result type", changed_value); });
                edited |= edit_column(store.static_table_groups,
                                      group,
                                      store.static_table_groups.columns,
                                      [&](auto& changed_value) {
                                          return edit_strings(store, "Columns", changed_value);
                                      });
                return edited;
            });
        if (changed) {
            draft = *value;
        }
    } else if (auto layout{((draft).kind() == lispb::codegen::DeclarationKind::homogeneous_layout
                                ? std::optional{draft}
                                : std::nullopt)}) {
        auto changed{edit_text(
            strings(),
            "Export specifier",
            store.homogeneous_layouts.export_specifier[store.homogeneous_layouts.index(*layout)])};
        changed |= edit_strings(
            store,
            "Components",
            store.homogeneous_layouts.components[store.homogeneous_layouts.index(*layout)]);
        changed |= edit_strings(
            store,
            "Input members",
            store.homogeneous_layouts.input_members[store.homogeneous_layouts.index(*layout)]);
        changed |= edit_records(
            store,
            store.homogeneous_values,
            "Value types",
            store.homogeneous_layouts.value_types[store.homogeneous_layouts.index(*layout)],
            [&](auto& item) {
                auto edited{edit_column(store.homogeneous_values,
                                        item,
                                        store.homogeneous_values.suffix,
                                        [&](auto& changed_value) {
                                            return edit_text(strings(), "Suffix", changed_value);
                                        })};
                edited |= edit_column(
                    store.homogeneous_values,
                    item,
                    store.homogeneous_values.type,
                    [&](auto& changed_value) { return type("Element type", changed_value); });
                edited |= edit_column(store.homogeneous_values,
                                      item,
                                      store.homogeneous_values.equivalent_type,
                                      [&](auto& changed_value) {
                                          return optional_type("Equivalent type", changed_value);
                                      });
                edited |=
                    edit_column(store.homogeneous_values,
                                item,
                                store.homogeneous_values.input_types,
                                [&](auto& changed_value) {
                                    return edit_items(
                                        store,
                                        "Input types",
                                        changed_value,
                                        [&](auto& input) { return type("Type", input); },
                                        [&] {
                                            return [&] {
                                                auto const new_type_refs{store.type_refs.append()};

                                                return new_type_refs;
                                            }();
                                        });
                                });
                return edited;
            });
        if (changed) {
            draft = *layout;
        }
    } else if (auto facade{((draft).kind() == lispb::codegen::DeclarationKind::facade
                                ? std::optional{draft}
                                : std::nullopt)}) {
        auto changed{type("Target type", store.facades.target_type[store.facades.index(*facade)])};
        changed |= edit_text(strings(),
                             "Target member",
                             store.facades.target_member_name[store.facades.index(*facade)]);
        changed |= edit_flag("Reference binding",
                             store.facades.reference_target[store.facades.index(*facade)]);
        changed |= edit_flag("Definitions in source",
                             store.facades.definitions_in_source[store.facades.index(*facade)]);
        changed |= edit_text(strings(),
                             "Export specifier",
                             store.facades.export_specifier[store.facades.index(*facade)]);
        auto access = [](char const* label, lispb::codegen::Access& value) {
            auto selection{value == lispb::codegen::Access::public_ ? 0 : 1};
            if (!ImGui::Combo(label, &selection, "public\0private\0")) {
                return false;
            }
            value =
                selection == 0 ? lispb::codegen::Access::public_ : lispb::codegen::Access::private_;
            return true;
        };
        changed |=
            access("Binding access", store.facades.bind_access[store.facades.index(*facade)]);
        changed |=
            access("Method access", store.facades.method_access[store.facades.index(*facade)]);
        auto friend_kind{store.facades.friend_kind[store.facades.index(*facade)] ==
                                 lispb::codegen::RecordKind::class_
                             ? 0
                             : 1};
        if (ImGui::Combo("Friend kind", &friend_kind, "class\0struct\0")) {
            store.facades.friend_kind[store.facades.index(*facade)] =
                friend_kind == 0 ? lispb::codegen::RecordKind::class_
                                 : lispb::codegen::RecordKind::struct_;
            changed = true;
        }
        changed |=
            edit_strings(store, "Friends", store.facades.friends[store.facades.index(*facade)]);
        changed |= edit_strings(store,
                                "Validation lines",
                                store.facades.validation_lines[store.facades.index(*facade)]);
        changed |=
            edit_strings(store,
                         "Validation dependencies",
                         store.facades.validation_dependencies[store.facades.index(*facade)]);
        changed |= edit_records(
            store,
            store.facade_methods,
            "Methods",
            store.facades.methods[store.facades.index(*facade)],
            [&](auto& method) {
                auto edited{edit_column(store.facade_methods,
                                        method,
                                        store.facade_methods.name,
                                        [&](auto& changed_value) {
                                            return edit_text(strings(), "Name", changed_value);
                                        })};
                edited |= edit_column(
                    store.facade_methods,
                    method,
                    store.facade_methods.return_type,
                    [&](auto& changed_value) { return type("Return type", changed_value); });
                edited |= edit_column(store.facade_methods,
                                      method,
                                      store.facade_methods.target_name,
                                      [&](auto& changed_value) {
                                          return edit_optional_text(
                                              strings(), "Target method", changed_value);
                                      });
                edited |= edit_column(
                    store.facade_methods,
                    method,
                    store.facade_methods.is_const,
                    [&](auto& changed_value) { return edit_flag("Const", changed_value); });
                edited |= edit_column(
                    store.facade_methods,
                    method,
                    store.facade_methods.is_noexcept,
                    [&](auto& changed_value) { return edit_flag("Noexcept", changed_value); });
                edited |= edit_column(
                    store.facade_methods,
                    method,
                    store.facade_methods.parameters,
                    [&](auto& changed_value) {
                        return edit_records(
                            store,
                            store.parameters,
                            "Parameters",
                            changed_value,
                            [&](auto& parameter) {
                                auto parameter_edited{edit_column(
                                    store.parameters,
                                    parameter,
                                    store.parameters.name,
                                    [&](auto& changed_value) {
                                        return edit_text(strings(), "Name", changed_value);
                                    })};
                                parameter_edited |=
                                    edit_column(store.parameters,
                                                parameter,
                                                store.parameters.type,
                                                [&](auto& changed_value) {
                                                    return type("Type", changed_value);
                                                });
                                parameter_edited |=
                                    edit_column(store.parameters,
                                                parameter,
                                                store.parameters.default_value,
                                                [&](auto& changed_value) {
                                                    return edit_optional_text(
                                                        strings(), "Default value", changed_value);
                                                });
                                return parameter_edited;
                            });
                    });
                return edited;
            });
        if (changed) {
            draft = *facade;
        }
    }
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
    auto const settings{declarations().module_settings(
        document_->manifest().modules[*structural_creation_module_])};
    auto name{lispb::codegen::declaration_name(declarations(), *structural_creation_draft_)};
    TypeIdentity identity{
        .module_name =
            declarations()
                .module_settings_store.name[declarations().module_settings_store.index(settings)],
        .namespace_name = declarations()
                              .module_settings_store
                              .namespace_name[declarations().module_settings_store.index(settings)]
                              .value_or(lispb::IdentifierId{}),
        .name = name};
    ImGui::Text("Module: %s", text(identity.module_name).c_str());
    if (edit_text(strings(), "Name", name)) {
        *structural_creation_draft_ =
            renamed_structural(declarations(), *structural_creation_draft_, name);
    }
    identity.name = name;
    ImGui::BeginDisabled(project_history_active());
    draw_structural_fields(*structural_creation_draft_, identity);
    if (ImGui::Button("Create")) {
        auto const id{document_->allocate_declaration_id()};
        if (apply_document_edit(document_->create_declaration(
                                    id, *structural_creation_module_, *structural_creation_draft_),
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

auto PlannerUi::draw_structural_declaration(DeclarationId const info) -> bool {
    auto const module{
        document_->manifest().modules[document_->declarations()
                                          .module_index[document_->declarations().index(info)]]};
    auto const schema{
        declarations().items(
            declarations().normal_modules.declarations[declarations().normal_modules.index(module)])
            [document_->declarations().declaration_index[document_->declarations().index(info)]]};
    if (schema.kind() != lispb::codegen::DeclarationKind::static_table &&
        schema.kind() != lispb::codegen::DeclarationKind::facade &&
        schema.kind() != lispb::codegen::DeclarationKind::homogeneous_layout) {
        return false;
    }
    auto const revision{document_->revision()};
    if (structural_draft_identity_ !=
            document_->declarations().identity[document_->declarations().index(info)] ||
        structural_draft_revision_ != revision || !structural_draft_.has_value()) {
        structural_draft_ = declarations().clone(schema);
        structural_draft_identity_ =
            document_->declarations().identity[document_->declarations().index(info)];
        structural_draft_revision_ = revision;
    }

    ImGui::Text("%s [%s]",
                text(document_->declarations().identity[document_->declarations().index(info)].name)
                    .c_str(),
                std::string{lispb::codegen::declaration_head(schema)}.c_str());
    ImGui::TextUnformatted(
        text(document_->declarations().identity[document_->declarations().index(info)].module_name)
            .c_str());
    if (document_->declarations().source[document_->declarations().index(info)].has_value()) {
        auto const& path{document_->storage().sources.filesystem_path(
            document_
                ->source_span(
                    *document_->declarations().source[document_->declarations().index(info)])
                .file)};
        ImGui::TextWrapped(
            "Source: %s:%llu",
            path.string().c_str(),
            static_cast<unsigned long long>(
                document_
                    ->source_location(
                        *document_->declarations().source[document_->declarations().index(info)])
                    .line));
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
        auto const& node{*selected};
        ImGui::TextUnformatted(text(types.cpp_spelling(node)).c_str());
        auto const& authored{types.declarations()};
        auto const module{types.module(node)};
        auto const resolved_spelling{[&](lispb::codegen::TypeRefId const reference) {
            auto const occurrence{types.resolved(module, reference)};
            auto const row{types.occurrences().index(occurrence)};
            return std::string{types.cpp_types().spelling(types.occurrences().cpp_type[row])};
        }};
        auto const definition{types.definition(node)};
        if (definition.kind() == lispb::schema::TypeDefinitionKind::static_table) {
            auto const table{
                types.definitions()
                    .static_tables.source[types.definitions().static_tables.index(definition)]};
            auto const table_row{authored.static_tables.index(table)};
            ImGui::Text(
                "Rows: %llu",
                static_cast<unsigned long long>(authored.static_tables.rows[table_row].size()));
            ImGui::TextUnformatted("Backend: Unreal TStaticArray columns");
            for (auto const column : authored.items(authored.static_tables.columns[table_row])) {
                auto const row{authored.static_table_columns.index(column)};
                ImGui::BulletText(
                    "%s: %s",
                    text(authored.static_table_columns.name[row]).c_str(),
                    resolved_spelling(authored.static_table_columns.type[row]).c_str());
            }
            for (auto const group : authored.items(authored.static_tables.groups[table_row])) {
                auto const row{authored.static_table_groups.index(group)};
                ImGui::BulletText(
                    "Group %s returns %s",
                    text(authored.static_table_groups.name[row]).c_str(),
                    resolved_spelling(authored.static_table_groups.type[row]).c_str());
            }
        } else if (definition.kind() == lispb::schema::TypeDefinitionKind::facade) {
            auto const facade{
                types.definitions().facades.source[types.definitions().facades.index(definition)]};
            auto const facade_row{authored.facades.index(facade)};
            ImGui::Text("Target: %s (%s)",
                        resolved_spelling(authored.facades.target_type[facade_row]).c_str(),
                        authored.facades.reference_target[facade_row] ? "reference" : "pointer");
            ImGui::Text("Target member: %s",
                        text(authored.facades.target_member_name[facade_row]).c_str());
            for (auto const method : authored.items(authored.facades.methods[facade_row])) {
                auto const row{authored.facade_methods.index(method)};
                ImGui::BulletText(
                    "%s returns %s%s%s (forwards to %s)",
                    text(authored.facade_methods.name[row]).c_str(),
                    resolved_spelling(authored.facade_methods.return_type[row]).c_str(),
                    authored.facade_methods.is_const[row] ? " const" : "",
                    authored.facade_methods.is_noexcept[row] ? " noexcept" : "",
                    text(authored.facade_methods.target_name[row].value_or(
                             authored.facade_methods.name[row]))
                        .c_str());
                for (auto const parameter :
                     authored.items(authored.facade_methods.parameters[row])) {
                    auto const parameter_row{authored.parameters.index(parameter)};
                    ImGui::Text("    %s: %s",
                                text(authored.parameters.name[parameter_row]).c_str(),
                                resolved_spelling(authored.parameters.type[parameter_row]).c_str());
                }
            }
        } else if (auto const storage{
                       ((types.definition(node)).kind() ==
                                lispb::schema::TypeDefinitionKind::homogeneous_storage
                            ? std::optional{types.definition(node)}
                            : std::nullopt)}) {
            ImGui::TextUnformatted("Backend: Unreal TArray storage");
            ImGui::Text(
                "Element type: %s",
                std::string{
                    analysis_session_.inputs.workspace.types().cpp_types().spelling(
                        analysis_session_.inputs.workspace.types().occurrences().cpp_type
                            [analysis_session_.inputs.workspace.types().occurrences().index(
                                analysis_session_.inputs.workspace.types()
                                    .definitions()
                                    .homogeneous_storage
                                    .value_type[analysis_session_.inputs.workspace.types()
                                                    .definitions()
                                                    .homogeneous_storage.index(*storage)])])}
                    .c_str());
            for (auto const& component : analysis_session_.inputs.workspace.types()
                                             .definitions()
                                             .homogeneous_storage.components_of(*storage)) {
                ImGui::BulletText("Component: %s", text(component).c_str());
            }
            if (lispb::schema::is_valid(
                    analysis_session_.inputs.workspace.types()
                        .definitions()
                        .homogeneous_storage
                        .equivalent_type[analysis_session_.inputs.workspace.types()
                                             .definitions()
                                             .homogeneous_storage.index(*storage)])) {
                ImGui::Text(
                    "Equivalent type: %s",
                    std::string{
                        analysis_session_.inputs.workspace.types().cpp_types().spelling(
                            analysis_session_.inputs.workspace.types().occurrences().cpp_type
                                [analysis_session_.inputs.workspace.types().occurrences().index(
                                    analysis_session_.inputs.workspace.types()
                                        .definitions()
                                        .homogeneous_storage.equivalent_type
                                            [analysis_session_.inputs.workspace.types()
                                                 .definitions()
                                                 .homogeneous_storage.index(*storage)])])}
                        .c_str());
            }
            for (auto const& input : analysis_session_.inputs.workspace.types()
                                         .definitions()
                                         .homogeneous_storage.input_types_of(*storage)) {
                ImGui::BulletText(
                    "Input type: %s",
                    std::string{analysis_session_.inputs.workspace.types().cpp_types().spelling(
                                    types.occurrences().cpp_type[types.occurrences().index(input)])}
                        .c_str());
            }
        }
    }
    if (detail::section("Generated types")) {
        auto const generated{types.types_for_declaration(schema)};
        for (auto const id : generated) {
            auto const& node{id};
            if (ImGui::Selectable(text(types.cpp_spelling(node)).c_str(),
                                  analysis_session_.inputs.selection.type == id)) {
                select_type(id);
            }
        }
        if (auto const layout{
                ((schema).kind() == lispb::codegen::DeclarationKind::homogeneous_layout
                     ? std::optional{schema}
                     : std::nullopt)}) {
            ImGui::Text("View template: T%sView",
                        text(declarations()
                                 .homogeneous_layouts
                                 .name[declarations().homogeneous_layouts.index(*layout)])
                            .c_str());
            auto const value_types{declarations().items(
                declarations()
                    .homogeneous_layouts
                    .value_types[declarations().homogeneous_layouts.index(*layout)])};
            if (!value_types.empty() &&
                declarations()
                    .homogeneous_values
                    .equivalent_type[declarations().homogeneous_values.index(value_types.front())]
                    .has_value()) {
                ImGui::Text("Equivalent-type trait: T%sEquivalentType",
                            text(declarations()
                                     .homogeneous_layouts
                                     .name[declarations().homogeneous_layouts.index(*layout)])
                                .c_str());
            }
            ImGui::TextWrapped("Select a concrete storage type to inspect its graph relationships. "
                               "View templates have no single physical layout.");
        }
    }
    draw_declaration_dependencies(
        document_->declarations().identity[document_->declarations().index(info)]);
    if (auto const selected{analysis_session_.inputs.selection.type}) {
        draw_type_users(*selected);
    }
    ImGui::TextWrapped(
        "Physical layout analysis is unavailable for this generated backend representation.");

    ImGui::BeginDisabled(project_history_active());
    if (detail::section("Declaration actions")) {
        if (rename_editor_declaration_ != info) {
            rename_editor_declaration_ = info;
            std::snprintf(
                declaration_name_.data(),
                declaration_name_.size(),
                "%s",
                text(document_->declarations().identity[document_->declarations().index(info)].name)
                    .c_str());
        }
        ImGui::InputText("Name", declaration_name_.data(), declaration_name_.size());
        if (ImGui::Button("Rename")) {
            auto identity{
                document_->declarations().identity[document_->declarations().index(info)]};
            identity.name = strings().intern(declaration_name_.data());
            static_cast<void>(apply_document_edit(
                document_->rename_declaration(info, text(identity.name)), identity));
            ImGui::EndDisabled();
            return true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Duplicate")) {
            auto name{text(document_->declarations()
                               .identity[document_->declarations().index(info)]
                               .name) +
                      "Copy"};
            for (std::size_t suffix{2}; std::ranges::any_of(
                     declarations().items(
                         declarations()
                             .normal_modules
                             .declarations[declarations().normal_modules.index(module)]),
                     [&](auto const id) {
                         return text(lispb::codegen::declaration_name(declarations(), id)) == name;
                     });
                 ++suffix) {
                name = text(document_->declarations()
                                .identity[document_->declarations().index(info)]
                                .name) +
                       "Copy" + std::to_string(suffix);
            }
            auto identity{
                document_->declarations().identity[document_->declarations().index(info)]};
            identity.name = strings().intern(name);
            auto const copy{renamed_structural(declarations(), schema, identity.name)};
            static_cast<void>(apply_document_edit(
                document_->create_declaration(
                    document_->allocate_declaration_id(),
                    document_->declarations().module_index[document_->declarations().index(info)],
                    copy),
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
            ImGui::Text(
                "Delete %s? File > Undo restores applied deletions.",
                text(document_->declarations().identity[document_->declarations().index(info)].name)
                    .c_str());
            if (ImGui::Button("Delete")) {
                auto const applied{apply_document_edit(document_->delete_declaration(info))};
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
        if (ImGui::BeginCombo(
                "Module",
                text(declarations()
                         .module_settings_store.name[declarations().module_settings_store.index(
                             declarations().normal_modules.settings
                                 [declarations().normal_modules.index(module)])])
                    .c_str())) {
            auto const& modules{document_->manifest().modules};
            for (std::size_t index{}; index < modules.size(); ++index) {
                if (modules[index].kind() == lispb::codegen::ModuleKind::normal &&
                    index != document_->declarations()
                                 .module_index[document_->declarations().index(info)]) {
                    if (ImGui::Selectable(
                            text(declarations()
                                     .module_settings_store
                                     .name[declarations().module_settings_store.index(
                                         declarations().module_settings(modules[index]))])
                                .c_str())) {
                        destination = index;
                    }
                }
            }
            ImGui::EndCombo();
        }
        if (destination.has_value()) {
            auto const settings{
                declarations().module_settings(document_->manifest().modules[*destination])};
            auto identity{
                document_->declarations().identity[document_->declarations().index(info)]};
            identity.module_name = declarations()
                                       .module_settings_store
                                       .name[declarations().module_settings_store.index(settings)];
            identity.namespace_name =
                declarations()
                    .module_settings_store
                    .namespace_name[declarations().module_settings_store.index(settings)]
                    .value_or(lispb::IdentifierId{});
            static_cast<void>(
                apply_document_edit(document_->move_declaration(info, *destination), identity));
            ImGui::EndDisabled();
            return true;
        }
    }
    if (detail::section("Edit declaration")) {
        draw_structural_fields(
            *structural_draft_,
            document_->declarations().identity[document_->declarations().index(info)]);
        if (ImGui::Button("Apply declaration changes")) {
            auto const applied{apply_document_edit(
                document_->replace_declaration(info, *structural_draft_),
                document_->declarations().identity[document_->declarations().index(info)])};
            if (applied) {
                structural_draft_.reset();
            }
            ImGui::EndDisabled();
            return true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset draft")) {
            structural_draft_ = declarations().clone(schema);
            schema_edit_message_.clear();
        }
        ImGui::TextWrapped("Apply validates the complete declaration and records one undoable "
                           "edit. Save writes applied changes to LispB.");
    }
    ImGui::EndDisabled();
    return true;
}

} // namespace ioj::layout_planner
