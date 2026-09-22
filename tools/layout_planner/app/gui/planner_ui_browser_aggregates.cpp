#include "planner_ui_browser_common.hpp"

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
            pending_soa_record_binding_->column_name.c_str());
        ImGui::Separator();
    }

    auto const& modules{document_->manifest().modules};
    auto first_record_module{std::optional<std::size_t>{}};
    for (std::size_t index{}; index < modules.size(); ++index) {
        if (std::holds_alternative<codegen::NormalModuleSchema>(modules[index])) {
            first_record_module = index;
            break;
        }
    }
    if (!first_record_module.has_value()) {
        ImGui::TextDisabled("The target has no ordinary module to receive a new declaration.");
    } else {
        if (new_record_module_index_ >= modules.size() ||
            !std::holds_alternative<codegen::NormalModuleSchema>(
                modules[new_record_module_index_])) {
            new_record_module_index_ = *first_record_module;
        }
        auto const& selected_module{
            std::get<codegen::NormalModuleSchema>(modules[new_record_module_index_])};
        if (!module_initiated_dialog_ &&
            ImGui::BeginCombo("Module", selected_module.settings.name.c_str())) {
            for (std::size_t index{}; index < modules.size(); ++index) {
                auto const* module{std::get_if<codegen::NormalModuleSchema>(&modules[index])};
                if (module == nullptr) {
                    continue;
                }
                if (ImGui::Selectable(module->settings.name.c_str(),
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
            auto const& module{
                std::get<codegen::NormalModuleSchema>(modules[new_record_module_index_])};
            auto const name{std::string{new_record_name_.data()}};
            auto const identity{
                TypeIdentity{.origin = TypeOrigin::declaration,
                             .module_name = module.settings.name,
                             .namespace_name = module.settings.namespace_name.value_or(""),
                             .name = name}};
            auto const id{document_->allocate_declaration_id()};
            auto schema{codegen::RecordSchema{
                .name = name,
                .members = {{.name = "value",
                             .type = codegen::TypeRef{.name = new_record_member_type_.data(),
                                                      .suffix = {},
                                                      .nested = std::nullopt},
                             .count = std::nullopt,
                             .relationship = std::nullopt}},
                .export_specifier = std::nullopt}};
            auto selection{std::optional<TypeIdentity>{identity}};
            if (pending_soa_record_binding_.has_value()) {
                if (auto const* soa_info{
                        document_->declaration(pending_soa_record_binding_->soa_declaration)};
                    soa_info != nullptr) {
                    selection = soa_info->identity;
                }
            }
            if (apply_document_edit(CreateRecord{.declaration = id,
                                                 .module_index = new_record_module_index_,
                                                 .schema = std::move(schema),
                                                 .insertion_index = std::nullopt},
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
    auto const* soa_info{document_->declaration(binding.soa_declaration)};
    auto const* soa_schema{document_->soa_schema(binding.soa_declaration)};
    auto const soa_identity{soa_info == nullptr ? std::optional<TypeIdentity>{}
                                                : std::optional{soa_info->identity}};
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

    if (soa_info == nullptr || soa_schema == nullptr) {
        return rollback_creation("The SoA declaration is no longer available.");
    }
    auto replacement{*soa_schema};
    auto const column{std::ranges::find(
        replacement.members, binding.column_name, &codegen::SoaMemberSchema::name)};
    if (column == replacement.members.end()) {
        return rollback_creation("The selected SoA column is no longer available.");
    }

    column->type = codegen::TypeRef{.name = record.namespace_name.empty()
                                              ? record.name
                                              : record.namespace_name + "::" + record.name,
                                    .suffix = {},
                                    .nested = std::nullopt};
    if (!apply_document_edit(
            ReplaceSoa{.declaration = binding.soa_declaration, .schema = std::move(replacement)},
            soa_info->identity)) {
        return rollback_creation("The record was valid, but binding the SoA column failed: " +
                                 schema_edit_message_);
    }

    analysis_session_.inputs.selection.field = binding.column_name;
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
        if (std::holds_alternative<codegen::NormalModuleSchema>(modules[index])) {
            first_module = index;
            break;
        }
    }
    if (!first_module.has_value()) {
        ImGui::TextDisabled("The target has no ordinary module to receive a new declaration.");
    } else {
        if (new_union_module_index_ >= modules.size() ||
            !std::holds_alternative<codegen::NormalModuleSchema>(
                modules[new_union_module_index_])) {
            new_union_module_index_ = *first_module;
        }
        auto const& selected_module{
            std::get<codegen::NormalModuleSchema>(modules[new_union_module_index_])};
        if (!module_initiated_dialog_ &&
            ImGui::BeginCombo("Module", selected_module.settings.name.c_str())) {
            for (std::size_t index{}; index < modules.size(); ++index) {
                auto const* module{std::get_if<codegen::NormalModuleSchema>(&modules[index])};
                if (module != nullptr && ImGui::Selectable(module->settings.name.c_str(),
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
            auto const& module{
                std::get<codegen::NormalModuleSchema>(modules[new_union_module_index_])};
            auto const name{std::string{new_union_name_.data()}};
            auto const identity{
                TypeIdentity{.origin = TypeOrigin::declaration,
                             .module_name = module.settings.name,
                             .namespace_name = module.settings.namespace_name.value_or(""),
                             .name = name}};
            auto const id{document_->allocate_declaration_id()};
            auto schema{codegen::UnionSchema{
                .name = name,
                .alternatives = {{.name = "value",
                                  .type =
                                      codegen::TypeRef{.name = new_union_alternative_type_.data(),
                                                       .suffix = {},
                                                       .nested = std::nullopt},
                                  .count = std::nullopt}},
                .export_specifier = std::nullopt}};
            if (apply_document_edit(CreateUnion{.declaration = id,
                                                .module_index = new_union_module_index_,
                                                .schema = std::move(schema),
                                                .insertion_index = std::nullopt},
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
        if (std::holds_alternative<codegen::NormalModuleSchema>(modules[index])) {
            first_module = index;
            break;
        }
    }
    auto const enum_types{analysis_session_.inputs.workspace.types().types()};
    auto const first_enum{std::ranges::find_if(enum_types, [](auto const& node) {
        return std::holds_alternative<EnumType>(node.definition);
    })};
    if (new_tagged_union_discriminant_.front() == '\0' && first_enum != enum_types.end()) {
        std::snprintf(new_tagged_union_discriminant_.data(),
                      new_tagged_union_discriminant_.size(),
                      "%s",
                      first_enum->cpp_spelling.c_str());
    }
    auto const selected_enum{std::ranges::find_if(enum_types, [&](auto const& node) {
        return std::holds_alternative<EnumType>(node.definition) &&
               node.cpp_spelling == new_tagged_union_discriminant_.data();
    })};
    if (new_tagged_union_tag_.front() == '\0' && selected_enum != enum_types.end()) {
        auto const& enumeration{std::get<EnumType>(selected_enum->definition)};
        auto const first_tag{std::ranges::find_if(enumeration.enumerators, [](auto const& value) {
            return !value.sentinel && !value.count_sentinel;
        })};
        if (first_tag != enumeration.enumerators.end()) {
            std::snprintf(new_tagged_union_tag_.data(),
                          new_tagged_union_tag_.size(),
                          "%s",
                          first_tag->name.c_str());
        }
    }

    if (!first_module.has_value()) {
        ImGui::TextDisabled("The target has no ordinary module to receive a new declaration.");
    } else if (first_enum == enum_types.end()) {
        ImGui::TextDisabled("The target has no enum to use as a discriminant.");
    } else {
        if (new_tagged_union_module_index_ >= modules.size() ||
            !std::holds_alternative<codegen::NormalModuleSchema>(
                modules[new_tagged_union_module_index_])) {
            new_tagged_union_module_index_ = *first_module;
        }
        auto const& selected_module{
            std::get<codegen::NormalModuleSchema>(modules[new_tagged_union_module_index_])};
        if (!module_initiated_dialog_ &&
            ImGui::BeginCombo("Module", selected_module.settings.name.c_str())) {
            for (std::size_t index{}; index < modules.size(); ++index) {
                auto const* module{std::get_if<codegen::NormalModuleSchema>(&modules[index])};
                if (module != nullptr &&
                    ImGui::Selectable(module->settings.name.c_str(),
                                      index == new_tagged_union_module_index_)) {
                    new_tagged_union_module_index_ = index;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::InputText("Name", new_tagged_union_name_.data(), new_tagged_union_name_.size());
        if (ImGui::BeginCombo("Discriminant", new_tagged_union_discriminant_.data())) {
            for (auto const& candidate : enum_types) {
                if (!std::holds_alternative<EnumType>(candidate.definition)) {
                    continue;
                }
                auto const selected{candidate.cpp_spelling ==
                                    new_tagged_union_discriminant_.data()};
                if (ImGui::Selectable(candidate.cpp_spelling.c_str(), selected)) {
                    std::snprintf(new_tagged_union_discriminant_.data(),
                                  new_tagged_union_discriminant_.size(),
                                  "%s",
                                  candidate.cpp_spelling.c_str());
                    new_tagged_union_tag_.fill('\0');
                }
            }
            ImGui::EndCombo();
        }
        ImGui::InputText("First alternative type",
                         new_tagged_union_alternative_type_.data(),
                         new_tagged_union_alternative_type_.size());
        auto const current_enum{std::ranges::find_if(enum_types, [&](auto const& node) {
            return std::holds_alternative<EnumType>(node.definition) &&
                   node.cpp_spelling == new_tagged_union_discriminant_.data();
        })};
        if (current_enum != enum_types.end()) {
            auto const& enumeration{std::get<EnumType>(current_enum->definition)};
            if (ImGui::BeginCombo("First alternative tag", new_tagged_union_tag_.data())) {
                for (auto const& value : enumeration.enumerators) {
                    if (value.sentinel || value.count_sentinel) {
                        continue;
                    }
                    if (ImGui::Selectable(value.name.c_str(),
                                          value.name == new_tagged_union_tag_.data())) {
                        std::snprintf(new_tagged_union_tag_.data(),
                                      new_tagged_union_tag_.size(),
                                      "%s",
                                      value.name.c_str());
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
            auto const& module{
                std::get<codegen::NormalModuleSchema>(modules[new_tagged_union_module_index_])};
            auto const name{std::string{new_tagged_union_name_.data()}};
            auto const identity{
                TypeIdentity{.origin = TypeOrigin::declaration,
                             .module_name = module.settings.name,
                             .namespace_name = module.settings.namespace_name.value_or(""),
                             .name = name}};
            auto const id{document_->allocate_declaration_id()};
            codegen::TaggedUnionSchema schema{};
            schema.name = name;
            schema.discriminant.name = new_tagged_union_discriminant_.data();
            codegen::TaggedUnionAlternativeSchema alternative{};
            alternative.name = "value";
            alternative.type.name = new_tagged_union_alternative_type_.data();
            alternative.tag = new_tagged_union_tag_.data();
            schema.alternatives.push_back(std::move(alternative));
            if (apply_document_edit(
                    CreateTaggedUnion{.declaration = id,
                                      .module_index = new_tagged_union_module_index_,
                                      .schema = std::move(schema),
                                      .insertion_index = std::nullopt},
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
        auto const* module{std::get_if<codegen::NormalModuleSchema>(&modules[index])};
        if (module != nullptr && module->soa_backend == codegen::SoaBackend::standard_library) {
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
            auto const* module{
                std::get_if<codegen::NormalModuleSchema>(&modules[new_soa_module_index_])};
            return module != nullptr &&
                   module->soa_backend == codegen::SoaBackend::standard_library;
        }()};
        if (!selected_valid) {
            new_soa_module_index_ = *first_soa_module;
        }
        auto const& selected_module{
            std::get<codegen::NormalModuleSchema>(modules[new_soa_module_index_])};
        if (!module_initiated_dialog_ &&
            ImGui::BeginCombo("Module", selected_module.settings.name.c_str())) {
            for (std::size_t index{}; index < modules.size(); ++index) {
                auto const* module{std::get_if<codegen::NormalModuleSchema>(&modules[index])};
                if (module == nullptr ||
                    module->soa_backend != codegen::SoaBackend::standard_library) {
                    continue;
                }
                if (ImGui::Selectable(module->settings.name.c_str(),
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
            auto const& module{
                std::get<codegen::NormalModuleSchema>(modules[new_soa_module_index_])};
            auto const name{std::string{new_soa_name_.data()}};
            auto const identity{
                TypeIdentity{.origin = TypeOrigin::declaration,
                             .module_name = module.settings.name,
                             .namespace_name = module.settings.namespace_name.value_or(""),
                             .name = name}};
            auto const id{document_->allocate_declaration_id()};
            codegen::SoaSchema schema{};
            schema.name = name;
            schema.members = {codegen::SoaMemberSchema{
                .name = "values",
                .kind = codegen::SoaMemberKind::array,
                .type = codegen::TypeRef{.name = new_soa_member_type_.data(),
                                         .suffix = {},
                                         .nested = std::nullopt},
                .fixed_schema = std::nullopt,
                .nested_schema = std::nullopt,
                .mask_field = false,
                .mask_dimensions = {},
                .relationship = std::nullopt}};
            if (apply_document_edit(CreateSoa{.declaration = id,
                                              .module_index = new_soa_module_index_,
                                              .schema = std::move(schema),
                                              .insertion_index = std::nullopt},
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
