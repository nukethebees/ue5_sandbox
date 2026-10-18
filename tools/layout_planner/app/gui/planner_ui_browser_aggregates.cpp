#include "planner_ui_browser_common.hpp"

#include <codegen/schema/enumerator_schemas.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/module_kind.h>
#include <codegen/schema/module_settings_store.h>
#include <codegen/schema/normal_module_schemas.h>
#include <codegen/schema/record_member_schemas.h>
#include <codegen/schema/record_schemas.h>
#include <codegen/schema/soa_backend.h>
#include <codegen/schema/soa_member_kind.h>
#include <codegen/schema/soa_member_schemas.h>
#include <codegen/schema/soa_schemas.h>
#include <codegen/schema/tagged_union_alternative_schemas.h>
#include <codegen/schema/tagged_union_schemas.h>
#include <codegen/schema/type_refs.h>
#include <codegen/schema/union_alternative_schemas.h>
#include <codegen/schema/union_schemas.h>
#include <lispb/compiler_storage.h>
#include <lispb/indices.h>
#include <lispb/schema/enum_domains.h>
#include <lispb/schema/type_definitions.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <ranges>
#include <string>
#include <utility>

namespace ioj::layout_planner {

void PlannerUi::draw_new_record_dialog() {
    if (open_new_record_dialog_) {
        ImGui::OpenPopup("New record");
        open_new_record_dialog_ = false;
    }
    if (!ImGui::BeginPopupModal("New record", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    if (!document_.has_value()) {
        ImGui::TextDisabled("No editable LispB document is loaded.");
        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return;
    }

    if (pending_soa_record_binding_.has_value()) {
        ImGui::TextWrapped(
            "Create a shared record declaration and bind SoA column '%s' to it. Creation and "
            "binding are separate undoable history steps.",
            text(pending_soa_record_binding_->column_name).c_str());
        ImGui::Separator();
    }

    auto const& modules{document_->manifest().modules};
    auto first_record_module{std::optional<std::size_t>{}};
    for (std::size_t index{}; index < modules.size(); ++index) {
        if ((modules[index].kind() == lispb::codegen::ModuleKind::normal)) {
            first_record_module = index;
            break;
        }
    }
    if (!first_record_module.has_value()) {
        ImGui::TextDisabled("The target has no ordinary module to receive a new declaration.");
    } else {
        if (new_record_module_index_ >= modules.size() ||
            !(modules[new_record_module_index_].kind() == lispb::codegen::ModuleKind::normal)) {
            new_record_module_index_ = *first_record_module;
        }
        auto const& selected_module{modules[new_record_module_index_]};
        if (!module_initiated_dialog_ &&
            ImGui::BeginCombo(
                "Module",
                text(declarations()
                         .module_settings_store.name[declarations().module_settings_store.index(
                             declarations().normal_modules.settings
                                 [declarations().normal_modules.index(selected_module)])])
                    .c_str())) {
            for (std::size_t index{}; index < modules.size(); ++index) {
                auto const module{((modules[index]).kind() == lispb::codegen::ModuleKind::normal
                                       ? std::optional{modules[index]}
                                       : std::nullopt)};
                if (!module.has_value()) {
                    continue;
                }
                if (ImGui::Selectable(
                        text(declarations()
                                 .module_settings_store
                                 .name[declarations().module_settings_store.index(
                                     declarations().normal_modules.settings
                                         [declarations().normal_modules.index(*module)])])
                            .c_str(),
                        index == new_record_module_index_)) {
                    new_record_module_index_ = index;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::InputText("Name", new_record_name_.data(), new_record_name_.size());
        ImGui::InputText(
            "First member type", new_record_member_type_.data(), new_record_member_type_.size());
        ImGui::TextDisabled("The declaration starts with one editable scalar member.");

        auto const ready{new_record_name_.front() != '\0' &&
                         new_record_member_type_.front() != '\0'};
        ImGui::BeginDisabled(!ready);
        auto const create_label{pending_soa_record_binding_.has_value() ? "Create and use"
                                                                        : "Create"};
        if (ImGui::Button(create_label)) {
            auto const& module{modules[new_record_module_index_]};
            auto const name{std::string{new_record_name_.data()}};
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
            auto schema{[&] {
                auto const new_records{declarations().records.append()};
                declarations().records.name[declarations().records.index(new_records)] =
                    strings().intern(name);
                declarations().records.members[declarations().records.index(new_records)] =
                    declarations().range<lispb::codegen::RecordMemberId>({[&] {
                        auto const new_record_members{declarations().record_members.append()};
                        declarations().record_members.name[declarations().record_members.index(
                            new_record_members)] = strings().intern("value");
                        declarations().record_members.type[declarations().record_members.index(
                            new_record_members)] = [&] {
                            auto const new_type_refs{declarations().type_refs.append()};
                            declarations()
                                .type_refs.name[declarations().type_refs.index(new_type_refs)] =
                                strings().intern(new_record_member_type_.data());
                            declarations().type_refs.suffix[declarations().type_refs.index(
                                new_type_refs)] = {};
                            declarations()
                                .type_refs.nested[declarations().type_refs.index(new_type_refs)] =
                                std::nullopt;
                            return new_type_refs;
                        }();
                        declarations().record_members.count[declarations().record_members.index(
                            new_record_members)] = std::nullopt;
                        declarations()
                            .record_members
                            .relationship[declarations().record_members.index(new_record_members)] =
                            std::nullopt;
                        return new_record_members;
                    }()});
                declarations()
                    .records.export_specifier[declarations().records.index(new_records)] = {};
                return new_records;
            }()};
            auto selection{std::optional<TypeIdentity>{identity}};
            if (pending_soa_record_binding_.has_value()) {
                if (auto const soa_info{document_->declarations().find(
                        pending_soa_record_binding_->soa_declaration)};
                    soa_info.has_value()) {
                    selection = document_->declarations()
                                    .identity[document_->declarations().index(*soa_info)];
                }
            }
            if (apply_document_edit(
                    document_->create_declaration(
                        id, new_record_module_index_, std::move(schema), std::nullopt),
                    selection) &&
                bind_new_record_to_soa_column(identity)) {
                new_record_name_.fill('\0');
                std::snprintf(new_record_member_type_.data(),
                              new_record_member_type_.size(),
                              "%s",
                              "std::uint32_t");
                if (!pending_soa_record_binding_.has_value()) {
                    analysis_session_.inputs.selection.field = "value";
                }
                pending_soa_record_binding_.reset();
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
    }
    if (ImGui::Button("Cancel")) {
        pending_soa_record_binding_.reset();
        ImGui::CloseCurrentPopup();
    }
    if (!schema_edit_message_.empty()) {
        ImGui::TextWrapped("%s", schema_edit_message_.c_str());
    }
    ImGui::EndPopup();
}

auto PlannerUi::bind_new_record_to_soa_column(TypeIdentity const& record) -> bool {
    if (!pending_soa_record_binding_.has_value()) {
        return true;
    }
    auto const binding{*pending_soa_record_binding_};
    auto const soa_info{document_->declarations().find(binding.soa_declaration)};
    auto const soa_schema{document_->declaration_handle(binding.soa_declaration,
                                                        lispb::codegen::DeclarationKind::soa)};
    auto const soa_identity{
        !soa_info.has_value()
            ? std::optional<TypeIdentity>{}
            : std::optional{
                  document_->declarations().identity[document_->declarations().index(*soa_info)]}};
    auto rollback_creation = [&](std::string message) {
        auto const rollback{document_->undo()};
        if (rollback.has_value() && *rollback) {
            sync_document_graph(soa_identity);
            schema_edit_message_ = std::move(message) + " The new record was rolled back.";
        } else if (!rollback.has_value()) {
            schema_edit_message_ =
                std::move(message) +
                " The new record could not be rolled back: " + rollback.error().message;
        } else {
            schema_edit_message_ = std::move(message) +
                                   " The new record could not be rolled back because history did "
                                   "not change.";
        }
        return false;
    };

    if (!soa_info.has_value() || !soa_schema.has_value()) {
        return rollback_creation("The SoA declaration is no longer available.");
    }
    auto replacement{document_->storage().declarations.soas.clone(*soa_schema)};
    auto const members{declarations().items(
        document_->storage()
            .declarations.soas.members[document_->storage().declarations.soas.index(replacement)])};
    auto const column{std::ranges::find(members, binding.column_name, [&](auto const id) {
        return declarations().soa_members.name[declarations().soa_members.index(id)];
    })};
    if (column == members.end()) {
        return rollback_creation("The selected SoA column is no longer available.");
    }

    auto const column_index{static_cast<std::size_t>(column - members.begin())};
    auto const reference{
        record.namespace_name.value == 0
            ? record.name
            : strings().intern(text(record.namespace_name) + "::" + text(record.name))};
    document_->storage()
        .declarations.soas.members[document_->storage().declarations.soas.index(replacement)] =
        [&] {
            auto const original_items{declarations().items(
                document_->storage()
                    .declarations.soas
                    .members[document_->storage().declarations.soas.index(replacement)])};
            auto ids{std::vector(original_items.begin(), original_items.end())};
            ids[column_index] = [&] {
                auto value{declarations().soa_members.clone(ids[column_index])};
                declarations().soa_members.type[declarations().soa_members.index(value)] = [&] {
                    auto const new_type_refs{declarations().type_refs.append()};
                    declarations().type_refs.name[declarations().type_refs.index(new_type_refs)] =
                        reference;
                    return new_type_refs;
                }();
                return value;
            }();
            return declarations().range(ids);
        }();
    if (!apply_document_edit(
            document_->replace_declaration(binding.soa_declaration, std::move(replacement)),
            document_->declarations().identity[document_->declarations().index(*soa_info)])) {
        return rollback_creation("The record was valid, but binding the SoA column failed: " +
                                 schema_edit_message_);
    }

    analysis_session_.inputs.selection.field = text(binding.column_name);
    return true;
}

void PlannerUi::draw_new_union_dialog() {
    if (open_new_union_dialog_) {
        ImGui::OpenPopup("New union");
        open_new_union_dialog_ = false;
    }
    if (!ImGui::BeginPopupModal("New union", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    if (!document_.has_value()) {
        ImGui::TextDisabled("No editable LispB document is loaded.");
        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return;
    }

    auto const& modules{document_->manifest().modules};
    auto first_module{std::optional<std::size_t>{}};
    for (std::size_t index{}; index < modules.size(); ++index) {
        if ((modules[index].kind() == lispb::codegen::ModuleKind::normal)) {
            first_module = index;
            break;
        }
    }
    if (!first_module.has_value()) {
        ImGui::TextDisabled("The target has no ordinary module to receive a new declaration.");
    } else {
        if (new_union_module_index_ >= modules.size() ||
            !(modules[new_union_module_index_].kind() == lispb::codegen::ModuleKind::normal)) {
            new_union_module_index_ = *first_module;
        }
        auto const& selected_module{modules[new_union_module_index_]};
        if (!module_initiated_dialog_ &&
            ImGui::BeginCombo(
                "Module",
                text(declarations()
                         .module_settings_store.name[declarations().module_settings_store.index(
                             declarations().normal_modules.settings
                                 [declarations().normal_modules.index(selected_module)])])
                    .c_str())) {
            for (std::size_t index{}; index < modules.size(); ++index) {
                auto const module{((modules[index]).kind() == lispb::codegen::ModuleKind::normal
                                       ? std::optional{modules[index]}
                                       : std::nullopt)};
                if (module.has_value() &&
                    ImGui::Selectable(
                        text(declarations()
                                 .module_settings_store
                                 .name[declarations().module_settings_store.index(
                                     declarations().normal_modules.settings
                                         [declarations().normal_modules.index(*module)])])
                            .c_str(),
                        index == new_union_module_index_)) {
                    new_union_module_index_ = index;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::InputText("Name", new_union_name_.data(), new_union_name_.size());
        ImGui::InputText("First alternative type",
                         new_union_alternative_type_.data(),
                         new_union_alternative_type_.size());
        ImGui::TextDisabled("The declaration starts with one editable scalar alternative.");
        auto const ready{new_union_name_.front() != '\0' &&
                         new_union_alternative_type_.front() != '\0'};
        ImGui::BeginDisabled(!ready);
        if (ImGui::Button("Create")) {
            auto const& module{modules[new_union_module_index_]};
            auto const name{std::string{new_union_name_.data()}};
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
            auto schema{[&] {
                auto const new_unions{declarations().unions.append()};
                declarations().unions.name[declarations().unions.index(new_unions)] =
                    strings().intern(name);
                declarations().unions.alternatives[declarations().unions.index(
                    new_unions)] = declarations().range<lispb::codegen::UnionAlternativeId>({[&] {
                    auto const new_union_alternatives{declarations().union_alternatives.append()};
                    declarations().union_alternatives.name[declarations().union_alternatives.index(
                        new_union_alternatives)] = strings().intern("value");
                    declarations().union_alternatives.type[declarations().union_alternatives.index(
                        new_union_alternatives)] = [&] {
                        auto const new_type_refs{declarations().type_refs.append()};
                        declarations()
                            .type_refs.name[declarations().type_refs.index(new_type_refs)] =
                            strings().intern(new_union_alternative_type_.data());
                        declarations()
                            .type_refs.suffix[declarations().type_refs.index(new_type_refs)] = {};
                        declarations()
                            .type_refs.nested[declarations().type_refs.index(new_type_refs)] =
                            std::nullopt;
                        return new_type_refs;
                    }();
                    declarations().union_alternatives.count[declarations().union_alternatives.index(
                        new_union_alternatives)] = std::nullopt;
                    return new_union_alternatives;
                }()});
                declarations()
                    .unions.export_specifier[declarations().unions.index(new_unions)] = {};
                return new_unions;
            }()};
            if (apply_document_edit(
                    document_->create_declaration(
                        id, new_union_module_index_, std::move(schema), std::nullopt),
                    identity)) {
                new_union_name_.fill('\0');
                std::snprintf(new_union_alternative_type_.data(),
                              new_union_alternative_type_.size(),
                              "%s",
                              "std::uint32_t");
                analysis_session_.inputs.selection.field = "value";
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
    }
    if (ImGui::Button("Cancel")) {
        ImGui::CloseCurrentPopup();
    }
    if (!schema_edit_message_.empty()) {
        ImGui::TextWrapped("%s", schema_edit_message_.c_str());
    }
    ImGui::EndPopup();
}

void PlannerUi::draw_new_tagged_union_dialog() {
    if (open_new_tagged_union_dialog_) {
        ImGui::OpenPopup("New tagged union");
        open_new_tagged_union_dialog_ = false;
    }
    if (!ImGui::BeginPopupModal("New tagged union", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    if (!document_.has_value()) {
        ImGui::TextDisabled("No editable LispB document is loaded.");
        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return;
    }

    auto const& modules{document_->manifest().modules};
    auto first_module{std::optional<std::size_t>{}};
    for (std::size_t index{}; index < modules.size(); ++index) {
        if ((modules[index].kind() == lispb::codegen::ModuleKind::normal)) {
            first_module = index;
            break;
        }
    }
    auto const enum_types{analysis_session_.inputs.workspace.types().types()};
    auto const first_enum{std::ranges::find_if(enum_types, [&](auto const node) {
        return ((analysis_session_.inputs.workspace.types().definition(node)).kind() ==
                lispb::schema::TypeDefinitionKind::enumeration);
    })};
    if (new_tagged_union_discriminant_.front() == '\0' && first_enum != enum_types.end()) {
        std::snprintf(
            new_tagged_union_discriminant_.data(),
            new_tagged_union_discriminant_.size(),
            "%s",
            text(analysis_session_.inputs.workspace.types().cpp_spelling(*first_enum)).c_str());
    }
    auto const selected_enum{std::ranges::find_if(enum_types, [&](auto const& node) {
        return ((analysis_session_.inputs.workspace.types().definition(node)).kind() ==
                lispb::schema::TypeDefinitionKind::enumeration) &&
               text(analysis_session_.inputs.workspace.types().cpp_spelling(node)) ==
                   new_tagged_union_discriminant_.data();
    })};
    if (new_tagged_union_tag_.front() == '\0' && selected_enum != enum_types.end()) {
        auto const& enumeration{
            analysis_session_.inputs.workspace.types().definition(*selected_enum)};
        auto const first_tag{std::ranges::find_if(
            analysis_session_.inputs.workspace.types().enum_domains().values(
                analysis_session_.inputs.workspace.types()
                    .definitions()
                    .enums
                    .domain[analysis_session_.inputs.workspace.types().definitions().enums.index(
                        enumeration)]),
            [&](auto id) {
                auto const value{id};
                return !declarations()
                            .enumerators.sentinel[declarations().enumerators.index(value)] &&
                       !(value ==
                         analysis_session_.inputs.workspace.types().enum_domains().count_value
                             [analysis_session_.inputs.workspace.types().enum_domains().index(
                                 analysis_session_.inputs.workspace.types()
                                     .definitions()
                                     .enums.domain[analysis_session_.inputs.workspace.types()
                                                       .definitions()
                                                       .enums.index(enumeration)])]);
            })};
        if (first_tag !=
            analysis_session_.inputs.workspace.types()
                .enum_domains()
                .values(analysis_session_.inputs.workspace.types().definitions().enums.domain
                            [analysis_session_.inputs.workspace.types().definitions().enums.index(
                                enumeration)])
                .end()) {
            std::snprintf(
                new_tagged_union_tag_.data(),
                new_tagged_union_tag_.size(),
                "%s",
                text(declarations().enumerators.name[declarations().enumerators.index(*first_tag)])
                    .c_str());
        }
    }

    if (!first_module.has_value()) {
        ImGui::TextDisabled("The target has no ordinary module to receive a new declaration.");
    } else if (first_enum == enum_types.end()) {
        ImGui::TextDisabled("The target has no enum to use as a discriminant.");
    } else {
        if (new_tagged_union_module_index_ >= modules.size() ||
            !(modules[new_tagged_union_module_index_].kind() ==
              lispb::codegen::ModuleKind::normal)) {
            new_tagged_union_module_index_ = *first_module;
        }
        auto const& selected_module{modules[new_tagged_union_module_index_]};
        if (!module_initiated_dialog_ &&
            ImGui::BeginCombo(
                "Module",
                text(declarations()
                         .module_settings_store.name[declarations().module_settings_store.index(
                             declarations().normal_modules.settings
                                 [declarations().normal_modules.index(selected_module)])])
                    .c_str())) {
            for (std::size_t index{}; index < modules.size(); ++index) {
                auto const module{((modules[index]).kind() == lispb::codegen::ModuleKind::normal
                                       ? std::optional{modules[index]}
                                       : std::nullopt)};
                if (module.has_value() &&
                    ImGui::Selectable(
                        text(declarations()
                                 .module_settings_store
                                 .name[declarations().module_settings_store.index(
                                     declarations().normal_modules.settings
                                         [declarations().normal_modules.index(*module)])])
                            .c_str(),
                        index == new_tagged_union_module_index_)) {
                    new_tagged_union_module_index_ = index;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::InputText("Name", new_tagged_union_name_.data(), new_tagged_union_name_.size());
        if (ImGui::BeginCombo("Discriminant", new_tagged_union_discriminant_.data())) {
            for (auto const& candidate : enum_types) {
                if (!((analysis_session_.inputs.workspace.types().definition(candidate)).kind() ==
                      lispb::schema::TypeDefinitionKind::enumeration)) {
                    continue;
                }
                auto const selected{text(analysis_session_.inputs.workspace.types().cpp_spelling(
                                        candidate)) == new_tagged_union_discriminant_.data()};
                if (ImGui::Selectable(
                        text(analysis_session_.inputs.workspace.types().cpp_spelling(candidate))
                            .c_str(),
                        selected)) {
                    std::snprintf(
                        new_tagged_union_discriminant_.data(),
                        new_tagged_union_discriminant_.size(),
                        "%s",
                        text(analysis_session_.inputs.workspace.types().cpp_spelling(candidate))
                            .c_str());
                    new_tagged_union_tag_.fill('\0');
                }
            }
            ImGui::EndCombo();
        }
        ImGui::InputText("First alternative type",
                         new_tagged_union_alternative_type_.data(),
                         new_tagged_union_alternative_type_.size());
        auto const current_enum{std::ranges::find_if(enum_types, [&](auto const& node) {
            return ((analysis_session_.inputs.workspace.types().definition(node)).kind() ==
                    lispb::schema::TypeDefinitionKind::enumeration) &&
                   text(analysis_session_.inputs.workspace.types().cpp_spelling(node)) ==
                       new_tagged_union_discriminant_.data();
        })};
        if (current_enum != enum_types.end()) {
            auto const& enumeration{
                analysis_session_.inputs.workspace.types().definition(*current_enum)};
            if (ImGui::BeginCombo("First alternative tag", new_tagged_union_tag_.data())) {
                for (auto const id :
                     analysis_session_.inputs.workspace.types().enum_domains().values(
                         analysis_session_.inputs.workspace.types().definitions().enums.domain
                             [analysis_session_.inputs.workspace.types().definitions().enums.index(
                                 enumeration)])) {
                    auto const value{id};
                    if (declarations()
                            .enumerators.sentinel[declarations().enumerators.index(value)] ||
                        (value ==
                         analysis_session_.inputs.workspace.types().enum_domains().count_value
                             [analysis_session_.inputs.workspace.types().enum_domains().index(
                                 analysis_session_.inputs.workspace.types()
                                     .definitions()
                                     .enums.domain[analysis_session_.inputs.workspace.types()
                                                       .definitions()
                                                       .enums.index(enumeration)])])) {
                        continue;
                    }
                    if (ImGui::Selectable(
                            text(declarations()
                                     .enumerators.name[declarations().enumerators.index(value)])
                                .c_str(),
                            text(declarations()
                                     .enumerators.name[declarations().enumerators.index(value)]) ==
                                new_tagged_union_tag_.data())) {
                        std::snprintf(
                            new_tagged_union_tag_.data(),
                            new_tagged_union_tag_.size(),
                            "%s",
                            text(declarations()
                                     .enumerators.name[declarations().enumerators.index(value)])
                                .c_str());
                    }
                }
                ImGui::EndCombo();
            }
        }
        ImGui::TextDisabled("The declaration starts with one editable payload alternative.");
        auto const ready{new_tagged_union_name_.front() != '\0' &&
                         new_tagged_union_discriminant_.front() != '\0' &&
                         new_tagged_union_alternative_type_.front() != '\0' &&
                         new_tagged_union_tag_.front() != '\0'};
        ImGui::BeginDisabled(!ready);
        if (ImGui::Button("Create")) {
            auto const& module{modules[new_tagged_union_module_index_]};
            auto const name{std::string{new_tagged_union_name_.data()}};
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
            auto const schema{declarations().tagged_unions.append()};

            declarations().tagged_unions.name[declarations().tagged_unions.index(schema)] =
                strings().intern(name);
            declarations().tagged_unions.discriminant[declarations().tagged_unions.index(schema)] =
                [&] {
                    auto const new_type_refs{declarations().type_refs.append()};
                    declarations().type_refs.name[declarations().type_refs.index(new_type_refs)] =
                        strings().intern(new_tagged_union_discriminant_.data());
                    return new_type_refs;
                }();
            auto const alternative{declarations().tagged_union_alternatives.append()};

            declarations()
                .tagged_union_alternatives
                .name[declarations().tagged_union_alternatives.index(alternative)] =
                strings().intern("value");
            declarations()
                .tagged_union_alternatives
                .type[declarations().tagged_union_alternatives.index(alternative)] = [&] {
                auto const new_type_refs{declarations().type_refs.append()};
                declarations().type_refs.name[declarations().type_refs.index(new_type_refs)] =
                    strings().intern(new_tagged_union_alternative_type_.data());
                return new_type_refs;
            }();
            declarations()
                .tagged_union_alternatives
                .tag[declarations().tagged_union_alternatives.index(alternative)] =
                strings().intern(new_tagged_union_tag_.data());
            declarations().tagged_unions.alternatives[declarations().tagged_unions.index(schema)] =
                declarations().range<lispb::codegen::TaggedUnionAlternativeId>({alternative});
            if (apply_document_edit(
                    document_->create_declaration(
                        id, new_tagged_union_module_index_, std::move(schema), std::nullopt),
                    identity)) {
                new_tagged_union_name_.fill('\0');
                std::snprintf(new_tagged_union_alternative_type_.data(),
                              new_tagged_union_alternative_type_.size(),
                              "%s",
                              "std::uint32_t");
                analysis_session_.inputs.selection.field = "value";
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
    }
    if (ImGui::Button("Cancel")) {
        ImGui::CloseCurrentPopup();
    }
    if (!schema_edit_message_.empty()) {
        ImGui::TextWrapped("%s", schema_edit_message_.c_str());
    }
    ImGui::EndPopup();
}

void PlannerUi::draw_new_soa_dialog() {
    if (open_new_soa_dialog_) {
        ImGui::OpenPopup("New SoA");
        open_new_soa_dialog_ = false;
    }
    if (!ImGui::BeginPopupModal("New SoA", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    if (!document_.has_value()) {
        ImGui::TextDisabled("No editable LispB document is loaded.");
        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return;
    }

    auto const& modules{document_->manifest().modules};
    auto first_soa_module{std::optional<std::size_t>{}};
    for (std::size_t index{}; index < modules.size(); ++index) {
        auto const module{((modules[index]).kind() == lispb::codegen::ModuleKind::normal
                               ? std::optional{modules[index]}
                               : std::nullopt)};
        if (module.has_value() &&
            declarations()
                    .normal_modules.soa_backend[declarations().normal_modules.index(*module)] ==
                lispb::codegen::SoaBackend::standard_library) {
            first_soa_module = index;
            break;
        }
    }
    if (!first_soa_module.has_value()) {
        ImGui::TextDisabled("The target has no standard-library SoA module for a new declaration.");
    } else {
        auto const selected_valid{[&] {
            if (new_soa_module_index_ >= modules.size()) {
                return false;
            }
            auto const module{
                ((modules[new_soa_module_index_]).kind() == lispb::codegen::ModuleKind::normal
                     ? std::optional{modules[new_soa_module_index_]}
                     : std::nullopt)};
            return module.has_value() &&
                   declarations().normal_modules.soa_backend[declarations().normal_modules.index(
                       *module)] == lispb::codegen::SoaBackend::standard_library;
        }()};
        if (!selected_valid) {
            new_soa_module_index_ = *first_soa_module;
        }
        auto const& selected_module{modules[new_soa_module_index_]};
        if (!module_initiated_dialog_ &&
            ImGui::BeginCombo(
                "Module",
                text(declarations()
                         .module_settings_store.name[declarations().module_settings_store.index(
                             declarations().normal_modules.settings
                                 [declarations().normal_modules.index(selected_module)])])
                    .c_str())) {
            for (std::size_t index{}; index < modules.size(); ++index) {
                auto const module{((modules[index]).kind() == lispb::codegen::ModuleKind::normal
                                       ? std::optional{modules[index]}
                                       : std::nullopt)};
                if (!module.has_value() ||
                    declarations().normal_modules.soa_backend[declarations().normal_modules.index(
                        *module)] != lispb::codegen::SoaBackend::standard_library) {
                    continue;
                }
                if (ImGui::Selectable(
                        text(declarations()
                                 .module_settings_store
                                 .name[declarations().module_settings_store.index(
                                     declarations().normal_modules.settings
                                         [declarations().normal_modules.index(*module)])])
                            .c_str(),
                        index == new_soa_module_index_)) {
                    new_soa_module_index_ = index;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::InputText("Name", new_soa_name_.data(), new_soa_name_.size());
        ImGui::InputText(
            "First column type", new_soa_member_type_.data(), new_soa_member_type_.size());
        ImGui::TextDisabled("The declaration starts with one editable array column.");

        auto const ready{new_soa_name_.front() != '\0' && new_soa_member_type_.front() != '\0'};
        ImGui::BeginDisabled(!ready);
        if (ImGui::Button("Create")) {
            auto const& module{modules[new_soa_module_index_]};
            auto const name{std::string{new_soa_name_.data()}};
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
            auto const schema{declarations().soas.append()};

            declarations().soas.name[declarations().soas.index(schema)] = strings().intern(name);
            declarations().soas.members[declarations().soas.index(
                schema)] = declarations().range<lispb::codegen::SoaMemberId>({[&] {
                auto const new_soa_members{declarations().soa_members.append()};
                declarations().soa_members.name[declarations().soa_members.index(new_soa_members)] =
                    strings().intern("values");
                declarations().soa_members.kind[declarations().soa_members.index(new_soa_members)] =
                    lispb::codegen::SoaMemberKind::array;
                declarations()
                    .soa_members.type[declarations().soa_members.index(new_soa_members)] = [&] {
                    auto const new_type_refs{declarations().type_refs.append()};
                    declarations().type_refs.name[declarations().type_refs.index(new_type_refs)] =
                        strings().intern(new_soa_member_type_.data());
                    declarations()
                        .type_refs.suffix[declarations().type_refs.index(new_type_refs)] = {};
                    declarations().type_refs.nested[declarations().type_refs.index(new_type_refs)] =
                        std::nullopt;
                    return new_type_refs;
                }();
                declarations()
                    .soa_members.fixed_schema[declarations().soa_members.index(new_soa_members)] =
                    std::nullopt;
                declarations()
                    .soa_members.nested_schema[declarations().soa_members.index(new_soa_members)] =
                    std::nullopt;
                declarations()
                    .soa_members.mask_field[declarations().soa_members.index(new_soa_members)] =
                    false;
                declarations().soa_members.mask_dimensions[declarations().soa_members.index(
                    new_soa_members)] = {};
                declarations()
                    .soa_members.relationship[declarations().soa_members.index(new_soa_members)] =
                    std::nullopt;
                return new_soa_members;
            }()});
            if (apply_document_edit(document_->create_declaration(
                                        id, new_soa_module_index_, std::move(schema), std::nullopt),
                                    identity)) {
                new_soa_name_.fill('\0');
                std::snprintf(new_soa_member_type_.data(),
                              new_soa_member_type_.size(),
                              "%s",
                              "std::uint32_t");
                analysis_session_.inputs.selection.field = "values";
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
    }
    if (ImGui::Button("Cancel")) {
        ImGui::CloseCurrentPopup();
    }
    if (!schema_edit_message_.empty()) {
        ImGui::TextWrapped("%s", schema_edit_message_.c_str());
    }
    ImGui::EndPopup();
}

} // namespace ioj::layout_planner
