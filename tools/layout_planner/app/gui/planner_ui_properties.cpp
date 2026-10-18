#include "planner_ui_properties_common.hpp"

#include <codegen/ast/cpp_types.h>
#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/declaration_kind.h>
#include <codegen/schema/enum_schemas.h>
#include <codegen/schema/facade_schemas.h>
#include <codegen/schema/fixed_point_schemas.h>
#include <codegen/schema/floating_point_format_info.h>
#include <codegen/schema/homogeneous_layout_schemas.h>
#include <codegen/schema/integer_scalar_schemas.h>
#include <codegen/schema/integer_varint_schemas.h>
#include <codegen/schema/linear_quantized_schemas.h>
#include <codegen/schema/mini_float_schemas.h>
#include <codegen/schema/module_settings_store.h>
#include <codegen/schema/optional_presence_bit_schemas.h>
#include <codegen/schema/optional_sentinel_schemas.h>
#include <codegen/schema/packed_named_code_schemas.h>
#include <codegen/schema/packed_value_schemas.h>
#include <codegen/schema/record_schemas.h>
#include <codegen/schema/soa_backend.h>
#include <codegen/schema/soa_member_kind.h>
#include <codegen/schema/soa_schemas.h>
#include <codegen/schema/static_table_schemas.h>
#include <codegen/schema/tagged_union_schemas.h>
#include <codegen/schema/union_schemas.h>
#include <codegen/schema/vector_soa_schemas.h>
#include <lispb/compiler_storage.h>
#include <lispb/indices.h>
#include <lispb/schema/external_types.h>
#include <lispb/schema/soa_columns.h>
#include <lispb/schema/tagged_union_alternatives.h>
#include <lispb/schema/type_definitions.h>
#include <lispb/schema/union_alternatives.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace ioj::layout_planner {

void PlannerUi::draw_declaration_dependencies(TypeIdentity const& identity) {
    if (!detail::section("Declaration dependencies")) {
        return;
    }
    auto const& types{analysis_session_.inputs.workspace.types()};
    auto const declaration{document_->find_declaration(identity)};
    auto const payload{declaration ? document_->declaration_handle(*declaration) : std::nullopt};
    std::size_t count{};
    for (auto const& use : types.type_uses()) {
        if (!payload || types.declaration(use) != *payload) {
            continue;
        }
        ImGui::PushID(static_cast<int>(count++));
        auto const label{
            lispb::codegen::type_reference_role_text(types.cpp_types().strings(), types.role(use)) +
            ": " +
            std::string{analysis_session_.inputs.workspace.types().cpp_types().spelling(
                types.occurrences().cpp_type[types.occurrences().index(types.target(use))])}};
        if (ImGui::Selectable(label.c_str())) {
            select_type(types.occurrences().type[types.occurrences().index(types.target(use))]);
        }
        ImGui::PopID();
    }
    if (count == 0) {
        ImGui::TextDisabled("No explicit type references.");
    }
}

void PlannerUi::draw_type_users(TypeId const type) {
    if (!detail::section("LispB references")) {
        return;
    }
    auto const& types{analysis_session_.inputs.workspace.types()};
    auto const& selected{type};
    auto const external{
        ((types.definition(selected)).kind() == lispb::schema::TypeDefinitionKind::external)};
    std::size_t count{};
    for (auto const& use : types.type_uses()) {
        auto const& target{types.occurrences().type[types.occurrences().index(types.target(use))]};
        auto const matches{
            types.occurrences().type[types.occurrences().index(types.target(use))] == type ||
            (external &&
             ((types.definition(target)).kind() == lispb::schema::TypeDefinitionKind::external) &&
             lispb::codegen::native_spelling(text(types.cpp_spelling(target))) ==
                 lispb::codegen::native_spelling(text(types.cpp_spelling(selected))))};
        if (!matches) {
            continue;
        }
        ImGui::PushID(static_cast<int>(count++));
        auto const declaration{document_.has_value() && types.declaration(use).value != 0
                                   ? document_->find_declaration(types.declaration(use))
                                   : std::optional<DeclarationId>{}};
        auto const label{
            text(declarations()
                     .module_settings_store.name[declarations().module_settings_store.index(
                         declarations().module_settings(types.module(use)))]) +
            " / " +
            (declaration ? text(document_->declarations()
                                    .identity[document_->declarations().index(*declaration)]
                                    .name) +
                               " / "
                         : "") +
            lispb::codegen::type_reference_role_text(strings(), types.role(use))};
        if (declaration.has_value()) {
            if (ImGui::Selectable(label.c_str())) {
                select_declaration(*declaration);
            }
        } else {
            ImGui::TextWrapped("%s", label.c_str());
        }
        ImGui::PopID();
    }
    if (count == 0) {
        ImGui::TextDisabled("No LispB references found.");
    }
}

auto PlannerUi::draw_type_picker(lispb::IdentifierId const module_name, TypeIdentity const& owner)
    -> std::optional<std::string> {
    if (ImGui::SmallButton("...")) {
        type_picker_filter_.fill('\0');
        ImGui::OpenPopup("semantic-type-picker");
    }
    ImGui::SetItemTooltip("Choose a semantic or physical type.");
    if (!ImGui::BeginPopup("semantic-type-picker")) {
        return std::nullopt;
    }

    ImGui::SetNextItemWidth(420.0F);
    ImGui::InputTextWithHint("##type-filter",
                             "Filter semantic or physical types",
                             type_picker_filter_.data(),
                             type_picker_filter_.size());
    auto lowercase{[](std::string text) {
        std::ranges::transform(text, text.begin(), [](unsigned char const character) {
            return static_cast<char>(std::tolower(character));
        });
        return text;
    }};
    auto const filter{lowercase(type_picker_filter_.data())};
    std::set<std::string, std::less<>> seen;
    std::optional<std::string> selected;
    auto draw_candidate = [&](std::string reference, std::string const& description) {
        if (!seen.insert(reference).second) {
            return;
        }
        auto const label{reference + "  [" + description + "]"};
        if (!filter.empty() && lowercase(label).find(filter) == std::string::npos) {
            return;
        }
        if (ImGui::Selectable(label.c_str())) {
            selected = std::move(reference);
            ImGui::CloseCurrentPopup();
        }
    };

    ImGui::BeginChild(
        "type-candidates", {420.0F, 260.0F}, true, ImGuiWindowFlags_HorizontalScrollbar);
    std::map<lispb::IdentifierId, std::size_t> declaration_spelling_counts;
    auto const owner_id{document_->find_declaration(owner)};
    auto const owner_payload{owner_id ? document_->declaration_handle(*owner_id) : std::nullopt};
    for (auto const& node : analysis_session_.inputs.workspace.types().types()) {
        if (analysis_session_.inputs.workspace.types().origin(node) == TypeOrigin::declaration) {
            ++declaration_spelling_counts[strings().intern(
                analysis_session_.inputs.workspace.types().cpp_spelling(node))];
        }
    }

    if (detail::section("Declared semantic types")) {
        for (auto const& node : analysis_session_.inputs.workspace.types().types()) {
            if (analysis_session_.inputs.workspace.types().origin(node) !=
                    TypeOrigin::declaration ||
                (owner_payload && analysis_session_.inputs.workspace.types().source_declaration(
                                      node) == *owner_payload)) {
                continue;
            }
            auto const same_module{analysis_session_.inputs.workspace.types().module_name(node) ==
                                   module_name};
            if (!same_module &&
                declaration_spelling_counts[strings().intern(
                    analysis_session_.inputs.workspace.types().cpp_spelling(node))] != 1) {
                auto const label{
                    text(analysis_session_.inputs.workspace.types().cpp_spelling(node)) +
                    "  [ambiguous across declaration modules]"};
                if (filter.empty() || lowercase(label).find(filter) != std::string::npos) {
                    ImGui::TextDisabled("%s", label.c_str());
                }
                continue;
            }
            auto reference{
                same_module ? text(analysis_session_.inputs.workspace.types().name(node))
                            : text(analysis_session_.inputs.workspace.types().cpp_spelling(node))};
            draw_candidate(std::move(reference),
                           "declared in " +
                               text(analysis_session_.inputs.workspace.types().module_name(node)));
        }
    }
    if (detail::section("Registered semantic types")) {
        auto const owner_type{analysis_session_.inputs.workspace.types().find(owner)};
        for (auto const& [name_id, registered] : document_->manifest().types.entries) {
            auto const name{std::string{
                analysis_session_.inputs.workspace.types().cpp_types().strings().view(name_id)}};
            auto const type{analysis_session_.inputs.workspace.types().find_registered(name_id)};
            if (type.has_value() && type != owner_type) {
                draw_candidate(
                    "@" + name,
                    std::string{analysis_session_.inputs.workspace.types().cpp_types().spelling(
                        document_->manifest()
                            .types.registered
                            .cpp_type[document_->manifest().types.registered.index(registered)])});
            }
        }
    }
    if (detail::section("Target physical types")) {
        for (auto const& [spelling, facts] : analysis_session_.primary_abi().types()) {
            static_cast<void>(facts);
            draw_candidate(spelling, "target ABI type");
        }
    }
    ImGui::EndChild();
    ImGui::EndPopup();
    return selected;
}

void PlannerUi::draw_properties_panel() {
    if (!properties_view_open_) {
        return;
    }
    auto const was_open{properties_view_open_};
    ImGui::Begin("Properties",
                 &properties_view_open_,
                 ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_MenuBar);
    persist_view_visibility(was_open, properties_view_open_);
    detail::pane_section_menu();
    if (auto const selected{analysis_session_.inputs.selection.type};
        selected.has_value() && document_.has_value()) {
        auto const& node{*selected};
        if (((analysis_session_.inputs.workspace.types().definition(node)).kind() ==
             lispb::schema::TypeDefinitionKind::static_table) ||
            ((analysis_session_.inputs.workspace.types().definition(node)).kind() ==
             lispb::schema::TypeDefinitionKind::facade) ||
            ((analysis_session_.inputs.workspace.types().definition(node)).kind() ==
             lispb::schema::TypeDefinitionKind::homogeneous_storage)) {
            auto const declaration{document_->find_declaration(
                analysis_session_.inputs.workspace.types().source_declaration(node))};
            if (declaration.has_value()) {
                static_cast<void>(draw_structural_declaration(*declaration));
            }
            ImGui::End();
            return;
        }
    }
    if (!analysis_session_.inputs.selection.type.has_value()) {
        auto const declaration{analysis_session_.inputs.selection.declaration};
        auto const info{document_.has_value() && declaration.has_value()
                            ? document_->declarations().find(*declaration)
                            : std::nullopt};
        if (!info.has_value()) {
            ImGui::TextDisabled("No selection.");
            ImGui::End();
            return;
        }
        if (draw_structural_declaration(*info)) {
            ImGui::End();
            return;
        }
        ImGui::TextWrapped("No structured inspector is available for this declaration.");
        ImGui::End();
        return;
    }

    auto const revision_before_draw{analysis_session_.inputs.workspace.revision()};
    auto const scroll_before_draw{ImGui::GetScrollY()};
    auto const content_bottom_before_draw{ImGui::GetScrollMaxY() + ImGui::GetWindowHeight()};
    auto end_panel = [&] {
        if (analysis_session_.inputs.workspace.revision() != revision_before_draw &&
            scroll_before_draw > 0.0F) {
            // Keep the old content extent for a frame when a graph rebuild ends drawing early.
            ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY(), content_bottom_before_draw));
            ImGui::Dummy({0.0F, 1.0F});
            ImGui::SetScrollY(scroll_before_draw);
        }
        ImGui::End();
    };

    auto const selected{*analysis_session_.inputs.selection.type};
    auto const& node{selected};
    auto const capabilities{
        declaration_capabilities(analysis_session_.inputs.workspace.types(), node)};
    ImGui::Text("%s", text(analysis_session_.inputs.workspace.types().name(node)).c_str());
    ImGui::TextDisabled("%s",
                        text(analysis_session_.inputs.workspace.types().module_name(node)).c_str());
    ImGui::TextDisabled(
        "%s", text(analysis_session_.inputs.workspace.types().cpp_spelling(node)).c_str());
    auto const selected_declaration{
        document_.has_value()
            ? document_->find_declaration(analysis_session_.inputs.workspace.types().identity(node))
            : std::optional<DeclarationId>{}};
    if (capabilities.editable && selected_declaration.has_value() &&
        ImGui::Button("Duplicate declaration")) {
        if (duplicate_selected_declaration(node)) {
            end_panel();
            return;
        }
    }
    if (selected_declaration.has_value()) {
        auto const declaration_info{document_->declarations().find(*selected_declaration)};
        auto const editable{
            document_
                ->declaration_handle(*selected_declaration,
                                     lispb::codegen::DeclarationKind::enumeration)
                .has_value() ||
            document_
                ->declaration_handle(*selected_declaration,
                                     lispb::codegen::DeclarationKind::packed_value)
                .has_value() ||
            document_
                ->declaration_handle(*selected_declaration,
                                     lispb::codegen::DeclarationKind::integer_scalar)
                .has_value() ||
            document_
                ->declaration_handle(*selected_declaration,
                                     lispb::codegen::DeclarationKind::linear_quantized)
                .has_value() ||
            document_
                ->declaration_handle(*selected_declaration,
                                     lispb::codegen::DeclarationKind::integer_varint)
                .has_value() ||
            document_
                ->declaration_handle(*selected_declaration,
                                     lispb::codegen::DeclarationKind::fixed_point)
                .has_value() ||
            document_
                ->declaration_handle(*selected_declaration,
                                     lispb::codegen::DeclarationKind::optional_sentinel)
                .has_value() ||
            document_
                ->declaration_handle(*selected_declaration,
                                     lispb::codegen::DeclarationKind::optional_presence_bit)
                .has_value() ||
            document_
                ->declaration_handle(*selected_declaration,
                                     lispb::codegen::DeclarationKind::mini_float)
                .has_value() ||
            document_
                ->declaration_handle(*selected_declaration, lispb::codegen::DeclarationKind::record)
                .has_value() ||
            document_
                ->declaration_handle(*selected_declaration,
                                     lispb::codegen::DeclarationKind::union_type)
                .has_value() ||
            document_
                ->declaration_handle(*selected_declaration,
                                     lispb::codegen::DeclarationKind::tagged_union)
                .has_value() ||
            document_
                ->declaration_handle(*selected_declaration, lispb::codegen::DeclarationKind::soa)
                .has_value()};
        if (declaration_info.has_value() && editable && capabilities.editable) {
            auto const& modules{document_->manifest().modules};
            auto const& source_module{
                modules[document_->declarations()
                            .module_index[document_->declarations().index(*declaration_info)]]};
            auto const& source_settings{declarations().module_settings(source_module)};
            std::vector<std::size_t> destinations;
            for (std::size_t index{}; index < modules.size(); ++index) {
                if (index ==
                        document_->declarations()
                            .module_index[document_->declarations().index(*declaration_info)] ||
                    modules[index].kind() != source_module.kind()) {
                    continue;
                }
                destinations.push_back(index);
            }
            if (!destinations.empty()) {
                std::optional<std::size_t> requested_destination;
                ImGui::SetNextItemWidth(std::max(120.0F, ImGui::GetContentRegionAvail().x));
                if (ImGui::BeginCombo(
                        "Module",
                        text(declarations()
                                 .module_settings_store
                                 .name[declarations().module_settings_store.index(source_settings)])
                            .c_str())) {
                    for (auto const index : destinations) {
                        auto const& settings{declarations().module_settings(modules[index])};
                        if (ImGui::Selectable(
                                text(
                                    declarations()
                                        .module_settings_store
                                        .name[declarations().module_settings_store.index(settings)])
                                    .c_str())) {
                            requested_destination = index;
                        }
                    }
                    ImGui::EndCombo();
                }
                if (requested_destination.has_value()) {
                    auto const& destination_settings{
                        declarations().module_settings(modules[*requested_destination])};
                    auto selection{analysis_session_.inputs.workspace.types().identity(node)};
                    selection.module_name =
                        declarations()
                            .module_settings_store
                            .name[declarations().module_settings_store.index(destination_settings)];
                    if (apply_document_edit(document_->move_declaration(*selected_declaration,
                                                                        *requested_destination,
                                                                        std::nullopt),
                                            selection)) {
                        end_panel();
                        return;
                    }
                }
                ImGui::TextDisabled(
                    "Moving repairs semantic references when the namespace changes.");
            }
        }
    }
    auto const rename_supported{
        capabilities.editable && selected_declaration.has_value() &&
        (!((analysis_session_.inputs.workspace.types().definition(node)).kind() ==
           lispb::schema::TypeDefinitionKind::soa) ||
         document_->declaration_handle(*selected_declaration, lispb::codegen::DeclarationKind::soa)
             .has_value())};
    if (rename_supported) {
        if (rename_editor_declaration_ != selected_declaration) {
            rename_editor_declaration_ = selected_declaration;
            std::snprintf(declaration_name_.data(),
                          declaration_name_.size(),
                          "%s",
                          text(analysis_session_.inputs.workspace.types().name(node)).c_str());
        }
        ImGui::SetNextItemWidth(std::max(120.0F, ImGui::GetContentRegionAvail().x - 90.0F));
        auto const submitted{ImGui::InputText("##declaration-name",
                                              declaration_name_.data(),
                                              declaration_name_.size(),
                                              ImGuiInputTextFlags_EnterReturnsTrue)};
        ImGui::SameLine();
        auto const rename_clicked{ImGui::Button("Rename")};
        auto const user_count{analysis_session_.inputs.workspace.types().users_of(selected).size()};
        ImGui::TextDisabled("Renaming repairs %llu direct semantic user%s.",
                            static_cast<unsigned long long>(user_count),
                            user_count == 1 ? "" : "s");
        if (submitted || rename_clicked) {
            if (declaration_name_.front() == '\0') {
                schema_edit_message_ = "Declaration name cannot be empty.";
            } else if (text(analysis_session_.inputs.workspace.types().name(node)) !=
                       declaration_name_.data()) {
                auto selection{analysis_session_.inputs.workspace.types().identity(node)};
                selection.name = strings().intern(declaration_name_.data());
                if (apply_document_edit(document_->rename_declaration(*selected_declaration,
                                                                      declaration_name_.data()),
                                        selection)) {
                    rename_editor_declaration_.reset();
                    end_panel();
                    return;
                }
            }
        }
    }
    if (capabilities.editable && selected_declaration.has_value()) {
        auto const declaration_info{document_->declarations().find(*selected_declaration)};
        if (declaration_info.has_value()) {
            auto const user_count{
                analysis_session_.inputs.workspace.types().users_of(selected).size()};
            ImGui::BeginDisabled(user_count != 0);
            if (ImGui::Button("Delete declaration")) {
                delete_declaration_ = *selected_declaration;
                delete_declaration_name_ =
                    text(analysis_session_.inputs.workspace.types().name(node));
                ImGui::OpenPopup("Delete declaration?");
            }
            ImGui::EndDisabled();
            if (user_count != 0) {
                ImGui::SameLine();
                ImGui::TextDisabled("Remove %llu direct user%s first.",
                                    static_cast<unsigned long long>(user_count),
                                    user_count == 1 ? "" : "s");
            }
        }
    }
    if (ImGui::BeginPopupModal("Delete declaration?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Delete '%s' from this schema?", delete_declaration_name_.c_str());
        ImGui::TextDisabled("This is a semantic edit; File > Undo restores the declaration.");
        if (ImGui::Button("Delete", {120.0F, 0.0F})) {
            if (delete_declaration_.has_value() && delete_declaration(*delete_declaration_)) {
                delete_declaration_.reset();
                delete_declaration_name_.clear();
                ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
                end_panel();
                return;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", {120.0F, 0.0F})) {
            delete_declaration_.reset();
            delete_declaration_name_.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (auto const soa{
            ((analysis_session_.inputs.workspace.types().definition(node)).kind() ==
                     lispb::schema::TypeDefinitionKind::soa
                 ? std::optional{analysis_session_.inputs.workspace.types().definition(node)}
                 : std::nullopt)};
        static_cast<bool>(soa) && !capabilities.physical_analysis_available) {
        if (detail::section("Semantic SoA")) {
            ImGui::Text(
                "Backend: %s",
                analysis_session_.inputs.workspace.types().definitions().soas.backend
                            [analysis_session_.inputs.workspace.types().definitions().soas.index(
                                *soa)] == lispb::codegen::SoaBackend::unreal
                    ? "Unreal"
                    : "standard library");
            ImGui::Text(
                "Source kind: %s",
                analysis_session_.inputs.workspace.types().definitions().soas.source_kind
                            [analysis_session_.inputs.workspace.types().definitions().soas.index(
                                *soa)] == SoaSourceKind::vector
                    ? "vector-soa"
                    : "struct");
            if (lispb::schema::is_valid(
                    analysis_session_.inputs.workspace.types().definitions().soas.equivalent_type
                        [analysis_session_.inputs.workspace.types().definitions().soas.index(
                            *soa)])) {
                ImGui::Text(
                    "Equivalent type: %s",
                    std::string{
                        analysis_session_.inputs.workspace.types().cpp_types().spelling(
                            analysis_session_.inputs.workspace.types().occurrences().cpp_type
                                [analysis_session_.inputs.workspace.types().occurrences().index(
                                    analysis_session_.inputs.workspace.types()
                                        .definitions()
                                        .soas
                                        .equivalent_type[analysis_session_.inputs.workspace.types()
                                                             .definitions()
                                                             .soas.index(*soa)])])}
                        .c_str());
            }
            if (analysis_session_.inputs.workspace.types()
                    .definitions()
                    .soas
                    .related_storage_name
                        [analysis_session_.inputs.workspace.types().definitions().soas.index(*soa)]
                    .has_value()) {
                ImGui::Text(
                    "Related storage: %s",
                    std::string{
                        analysis_session_.inputs.workspace.types().cpp_types().strings().view(
                            *analysis_session_.inputs.workspace.types()
                                 .definitions()
                                 .soas
                                 .related_storage_name[analysis_session_.inputs.workspace.types()
                                                           .definitions()
                                                           .soas.index(*soa)])}
                        .c_str());
            }
            for (auto const& component :
                 analysis_session_.inputs.workspace.types().definitions().soas.vector_components_of(
                     *soa)) {
                ImGui::BulletText("Component: %s", text(component).c_str());
            }
            for (auto const column_id :
                 analysis_session_.inputs.workspace.types().definitions().soas.columns_of(*soa)) {
                auto const column{column_id};
                auto const* kind{
                    definitions().soa_columns.kind[definitions().soa_columns.index(column)] ==
                            lispb::codegen::SoaMemberKind::nested
                        ? "nested SoA"
                        : "array"};
                ImGui::BulletText(
                    "%s [%s]: %s",
                    text(definitions().soa_columns.name[definitions().soa_columns.index(column)])
                        .c_str(),
                    kind,
                    std::string{
                        analysis_session_.inputs.workspace.types().cpp_types().spelling(
                            analysis_session_.inputs.workspace.types().occurrences().cpp_type
                                [analysis_session_.inputs.workspace.types().occurrences().index(
                                    definitions().soa_columns.semantic_type
                                        [definitions().soa_columns.index(column)])])}
                        .c_str());
                if (definitions()
                        .soa_columns.nested_type[definitions().soa_columns.index(column)]
                        .has_value()) {
                    auto const& nested{
                        *definitions()
                             .soa_columns.nested_type[definitions().soa_columns.index(column)]};
                    ImGui::SameLine();
                    ImGui::PushID(
                        text(
                            definitions().soa_columns.name[definitions().soa_columns.index(column)])
                            .c_str());
                    if (ImGui::SmallButton(
                            text(analysis_session_.inputs.workspace.types().cpp_spelling(nested))
                                .c_str())) {
                        select_type(
                            *definitions()
                                 .soa_columns.nested_type[definitions().soa_columns.index(column)]);
                    }
                    ImGui::PopID();
                }
            }
            ImGui::TextWrapped(
                "Physical layout analysis is not available for the Unreal SoA backend.");
        }
    } else if (auto const enumeration{
                   ((analysis_session_.inputs.workspace.types().definition(node)).kind() ==
                            lispb::schema::TypeDefinitionKind::enumeration
                        ? std::optional{analysis_session_.inputs.workspace.types().definition(node)}
                        : std::nullopt)}) {
        if (detail::section("Enum")) {
            if (lispb::schema::is_valid(
                    analysis_session_.inputs.workspace.types().definitions().enums.underlying_type
                        [analysis_session_.inputs.workspace.types().definitions().enums.index(
                            *enumeration)])) {
                auto const& underlying{
                    analysis_session_.inputs.workspace.types()
                        .occurrences()
                        .type[analysis_session_.inputs.workspace.types().occurrences().index(
                            analysis_session_.inputs.workspace.types()
                                .definitions()
                                .enums.underlying_type[analysis_session_.inputs.workspace.types()
                                                           .definitions()
                                                           .enums.index(*enumeration)])]};
                ImGui::TextUnformatted("Explicit C++ backing");
                ImGui::SameLine();
                if (ImGui::SmallButton(
                        text(analysis_session_.inputs.workspace.types().cpp_spelling(underlying))
                            .c_str())) {
                    select_type(
                        analysis_session_.inputs.workspace.types()
                            .occurrences()
                            .type[analysis_session_.inputs.workspace.types().occurrences().index(
                                analysis_session_.inputs.workspace.types()
                                    .definitions()
                                    .enums
                                    .underlying_type[analysis_session_.inputs.workspace.types()
                                                         .definitions()
                                                         .enums.index(*enumeration)])]);
                    analysis_session_.inputs.selection.field.clear();
                    analysis_session_.inputs.selection.record_access_members.clear();
                    analysis_session_.inputs.selection.record_access_set_explicit = false;
                }
            } else if (analysis_session_.results().enum_domain.has_value()) {
                ImGui::Text("Derived C++ backing: %s",
                            analysis_session_.results().enum_domain->backing_type.c_str());
            } else {
                ImGui::TextDisabled("Derived C++ backing: Unknown");
            }
            if (analysis_session_.inputs.workspace.types()
                    .definitions()
                    .enums
                    .count[analysis_session_.inputs.workspace.types().definitions().enums.index(
                        *enumeration)]
                    .has_value()) {
                ImGui::Text(
                    "Count sentinel: %s",
                    std::string{
                        analysis_session_.inputs.workspace.types().cpp_types().strings().view(
                            *analysis_session_.inputs.workspace.types()
                                 .definitions()
                                 .enums.count[analysis_session_.inputs.workspace.types()
                                                  .definitions()
                                                  .enums.index(*enumeration)])}
                        .c_str());
            }
            if (analysis_session_.results().enum_domain.has_value()) {
                auto const& domain{*analysis_session_.results().enum_domain};
                if (detail::section("Value domain")) {
                    ImGui::Text("Live symbols: %llu",
                                static_cast<unsigned long long>(domain.live_value_count));
                    ImGui::Text("Reserved/sentinel symbols: %llu",
                                static_cast<unsigned long long>(domain.reserved_value_count));
                    if (domain.minimum_value.has_value() && domain.maximum_value.has_value()) {
                        auto const minimum{lispb::schema::format_enum_code(*domain.minimum_value)};
                        auto const maximum{lispb::schema::format_enum_code(*domain.maximum_value)};
                        ImGui::Text("Known range: %s .. %s", minimum.c_str(), maximum.c_str());
                    } else {
                        ImGui::TextDisabled("Known range: Unknown");
                    }
                    if (domain.signed_domain.has_value()) {
                        ImGui::Text("Effective domain: %s",
                                    *domain.signed_domain ? "signed" : "unsigned");
                    } else {
                        ImGui::TextDisabled("Effective domain: Unknown");
                    }
                    if (domain.declared_signedness.has_value()) {
                        ImGui::Text("Declared signedness: %s",
                                    *domain.declared_signedness ? "signed" : "unsigned");
                    } else {
                        ImGui::TextUnformatted("Declared signedness: auto / inferred");
                    }
                    if (domain.minimum_required_bits.has_value()) {
                        ImGui::Text("Minimum semantic width: %u bits",
                                    *domain.minimum_required_bits);
                    } else {
                        ImGui::TextDisabled("Minimum semantic width: Unknown");
                    }
                    if (domain.declared_bit_width.has_value()) {
                        ImGui::Text("Declared semantic width: %u bits", *domain.declared_bit_width);
                    } else {
                        ImGui::TextUnformatted("Declared semantic width: auto");
                    }
                    if (domain.effective_bit_width.has_value()) {
                        ImGui::Text("Effective semantic width: %u bits",
                                    *domain.effective_bit_width);
                    } else {
                        ImGui::TextDisabled("Effective semantic width: Unknown");
                    }
                    if (domain.semantic_width_can_represent_domain.has_value()) {
                        ImGui::Text("Semantic width fit: %s",
                                    *domain.semantic_width_can_represent_domain ? "yes" : "NO");
                    } else {
                        ImGui::TextDisabled("Semantic width fit: Unknown");
                    }
                    if (domain.unused_semantic_codes.has_value()) {
                        ImGui::Text("Unused semantic codes: %llu",
                                    static_cast<unsigned long long>(*domain.unused_semantic_codes));
                    } else {
                        ImGui::TextDisabled("Unused semantic codes: Unknown");
                    }
                    if (domain.backing_bits.has_value()) {
                        ImGui::Text("Current physical backing: %s (%llu bits)",
                                    domain.backing_type.c_str(),
                                    static_cast<unsigned long long>(*domain.backing_bits));
                    } else {
                        ImGui::Text("Current physical backing: %s (Unknown size)",
                                    domain.backing_type.c_str());
                    }
                    if (domain.backing_can_represent_domain.has_value()) {
                        ImGui::Text("Backing fit: %s",
                                    *domain.backing_can_represent_domain ? "yes" : "NO");
                    } else {
                        ImGui::TextDisabled("Backing fit: Unknown");
                    }
                    if (domain.unused_backing_codes.has_value()) {
                        ImGui::Text("Unused backing codes: %llu",
                                    static_cast<unsigned long long>(*domain.unused_backing_codes));
                        ImGui::TextDisabled("Code-space inefficiency; not allocated byte waste.");
                    } else {
                        ImGui::TextDisabled("Unused backing codes: Unknown");
                    }
                    draw_diagnostics(domain.diagnostics);
                }
            }
            auto const revision_before_edit{analysis_session_.inputs.workspace.revision()};
            draw_enum_editor(node, *enumeration);
            if (analysis_session_.inputs.workspace.revision() != revision_before_edit) {
                end_panel();
                return;
            }
        }
    } else if (auto const scalar{
                   ((analysis_session_.inputs.workspace.types().definition(node)).kind() ==
                            lispb::schema::TypeDefinitionKind::integer_scalar
                        ? std::optional{analysis_session_.inputs.workspace.types().definition(node)}
                        : std::nullopt)}) {
        if (section_with_tooltip("Semantic integer domain",
                                 "The allowed values and named codes of this integer type, "
                                 "independent of storage.")) {
            if (analysis_session_.results().integer_scalar_analysis.has_value()) {
                auto const& analysis{*analysis_session_.results().integer_scalar_analysis};
                auto const minimum{lispb::codegen::format_packed_integer(analysis.minimum_value)};
                auto const maximum{lispb::codegen::format_packed_integer(analysis.maximum_value)};
                ImGui::Text("Signedness: %s", analysis.signedness ? "signed" : "unsigned");
                ImGui::Text("Live range: %s .. %s", minimum.c_str(), maximum.c_str());
                ImGui::Text("Live values: %s",
                            detail::format_number(analysis.live_value_count).c_str());
                ImGui::Text("Sentinel codes: %llu",
                            static_cast<unsigned long long>(analysis.sentinel_code_count));
                ImGui::Text("Required codes: %s",
                            detail::format_number(analysis.required_code_count).c_str());
                ImGui::Text("Minimum width: %u bits", analysis.minimum_required_bits);
                if (analysis.declared_bit_width.has_value()) {
                    ImGui::Text("Declared width: %u bits", *analysis.declared_bit_width);
                } else {
                    ImGui::TextUnformatted("Declared width: auto");
                }
                ImGui::Text("Effective width: %u bits", analysis.effective_bit_width);
                ImGui::Text("Unused codes: %s",
                            detail::format_number(analysis.unused_codes).c_str());
                ImGui::TextDisabled("Code-space inefficiency; this semantic declaration allocates "
                                    "no bytes itself.");
            }
            if (draw_integer_scalar_editor(node, *scalar)) {
                end_panel();
                return;
            }
        }
    } else if (auto const quantized{
                   ((analysis_session_.inputs.workspace.types().definition(node)).kind() ==
                            lispb::schema::TypeDefinitionKind::linear_quantized
                        ? std::optional{analysis_session_.inputs.workspace.types().definition(node)}
                        : std::nullopt)}) {
        if (detail::section("Linear quantized representation")) {
            auto const& source{
                analysis_session_.inputs.workspace.types()
                    .occurrences()
                    .type[analysis_session_.inputs.workspace.types().occurrences().index(
                        analysis_session_.inputs.workspace.types()
                            .definitions()
                            .linear_quantized.source[analysis_session_.inputs.workspace.types()
                                                         .definitions()
                                                         .linear_quantized.index(*quantized)])]};
            ImGui::TextUnformatted("Semantic source");
            ImGui::SameLine();
            if (ImGui::SmallButton(
                    text(analysis_session_.inputs.workspace.types().cpp_spelling(source))
                        .c_str())) {
                select_type(
                    analysis_session_.inputs.workspace.types()
                        .occurrences()
                        .type[analysis_session_.inputs.workspace.types().occurrences().index(
                            analysis_session_.inputs.workspace.types()
                                .definitions()
                                .linear_quantized
                                .source[analysis_session_.inputs.workspace.types()
                                            .definitions()
                                            .linear_quantized.index(*quantized)])]);
                analysis_session_.inputs.selection.field.clear();
                analysis_session_.inputs.selection.record_access_members.clear();
                analysis_session_.inputs.selection.record_access_set_explicit = false;
            }
            if (analysis_session_.results().linear_quantized_analysis.has_value()) {
                auto const& analysis{*analysis_session_.results().linear_quantized_analysis};
                auto const minimum{lispb::codegen::format_packed_integer(analysis.source_minimum)};
                auto const maximum{lispb::codegen::format_packed_integer(analysis.source_maximum)};
                ImGui::Text("Source range: %s .. %s", minimum.c_str(), maximum.c_str());
                ImGui::Text("Source span: %s", detail::format_number(analysis.source_span).c_str());
                ImGui::Text("Encoded width: %u bits", analysis.encoded_storage_bits);
                ImGui::Text("Total codes: %s",
                            detail::format_code_count(analysis.total_code_count).c_str());
                ImGui::Text("Reserved codes: %llu",
                            static_cast<unsigned long long>(analysis.reserved_code_count));
                ImGui::Text("Usable codes: %s",
                            detail::format_code_count(analysis.usable_code_count).c_str());
                ImGui::Text("Resolution: %.12g", static_cast<double>(analysis.resolution));
                ImGui::Text("Maximum rounding error: %.12g",
                            static_cast<double>(analysis.maximum_rounding_error));
                ImGui::Text("Endpoint mapping: minimum %s, maximum %s",
                            analysis.minimum_endpoint_exact ? "exact" : "inexact",
                            analysis.maximum_endpoint_exact ? "exact" : "inexact");
                ImGui::Text("Out-of-range policy: %s",
                            lispb::codegen::quantization_clipping_name(analysis.clipping).data());
                ImGui::TextDisabled("Resolution/error are numeric encoding facts; no performance "
                                    "estimate is made.");
            }
            if (draw_linear_quantized_editor(node, *quantized)) {
                end_panel();
                return;
            }
        }
    } else if (auto const optional{
                   ((analysis_session_.inputs.workspace.types().definition(node)).kind() ==
                            lispb::schema::TypeDefinitionKind::optional_sentinel
                        ? std::optional{analysis_session_.inputs.workspace.types().definition(node)}
                        : std::nullopt)}) {
        if (detail::section("Sentinel-encoded optional representation")) {
            auto const& source{
                analysis_session_.inputs.workspace.types()
                    .occurrences()
                    .type[analysis_session_.inputs.workspace.types().occurrences().index(
                        analysis_session_.inputs.workspace.types()
                            .definitions()
                            .optional_sentinels.source[analysis_session_.inputs.workspace.types()
                                                           .definitions()
                                                           .optional_sentinels.index(*optional)])]};
            ImGui::TextUnformatted("Semantic source");
            ImGui::SameLine();
            if (ImGui::SmallButton(
                    text(analysis_session_.inputs.workspace.types().cpp_spelling(source))
                        .c_str())) {
                select_type(
                    analysis_session_.inputs.workspace.types()
                        .occurrences()
                        .type[analysis_session_.inputs.workspace.types().occurrences().index(
                            analysis_session_.inputs.workspace.types()
                                .definitions()
                                .optional_sentinels
                                .source[analysis_session_.inputs.workspace.types()
                                            .definitions()
                                            .optional_sentinels.index(*optional)])]);
                analysis_session_.inputs.selection.field.clear();
                analysis_session_.inputs.selection.record_access_members.clear();
                analysis_session_.inputs.selection.record_access_set_explicit = false;
            }
            if (analysis_session_.results().optional_sentinel_analysis.has_value()) {
                auto const& analysis{*analysis_session_.results().optional_sentinel_analysis};
                auto const minimum{lispb::codegen::format_packed_integer(analysis.source_minimum)};
                auto const maximum{lispb::codegen::format_packed_integer(analysis.source_maximum)};
                auto const sentinel{lispb::codegen::format_packed_integer(analysis.sentinel_value)};
                ImGui::Text("Present range: %s .. %s", minimum.c_str(), maximum.c_str());
                ImGui::Text("Present values: %s",
                            detail::format_number(analysis.present_value_count).c_str());
                ImGui::Text(
                    "Absence sentinel: %s = %s", analysis.sentinel_name.c_str(), sentinel.c_str());
                ImGui::Text("Other named sentinel codes: %llu",
                            static_cast<unsigned long long>(analysis.other_sentinel_code_count));
                ImGui::Text("Unused codes: %s",
                            detail::format_number(analysis.unused_code_count).c_str());
                ImGui::Text("Encoded width: %u bits", analysis.encoded_storage_bits);
                ImGui::Text("Total code space: %s",
                            detail::format_code_count(analysis.total_code_count).c_str());
                ImGui::Text("Encoded payload at %llu values: %s bits",
                            static_cast<unsigned long long>(analysis.element_count),
                            detail::format_number(analysis.total_encoded_bits).c_str());
                ImGui::TextDisabled(
                    "Sentinel and unused codes are code-space roles, not allocated byte waste.");
                draw_diagnostics(analysis.diagnostics);
            }
            if (draw_optional_sentinel_editor(node, *optional)) {
                end_panel();
                return;
            }
        }
    } else if (auto const optional{
                   ((analysis_session_.inputs.workspace.types().definition(node)).kind() ==
                            lispb::schema::TypeDefinitionKind::optional_presence_bit
                        ? std::optional{analysis_session_.inputs.workspace.types().definition(node)}
                        : std::nullopt)}) {
        if (detail::section("Presence-bit optional representation")) {
            auto const& source{
                analysis_session_.inputs.workspace.types()
                    .occurrences()
                    .type[analysis_session_.inputs.workspace.types().occurrences().index(
                        analysis_session_.inputs.workspace.types()
                            .definitions()
                            .optional_presence_bits
                            .source[analysis_session_.inputs.workspace.types()
                                        .definitions()
                                        .optional_presence_bits.index(*optional)])]};
            ImGui::TextUnformatted("Semantic source");
            ImGui::SameLine();
            if (ImGui::SmallButton(
                    text(analysis_session_.inputs.workspace.types().cpp_spelling(source))
                        .c_str())) {
                select_type(
                    analysis_session_.inputs.workspace.types()
                        .occurrences()
                        .type[analysis_session_.inputs.workspace.types().occurrences().index(
                            analysis_session_.inputs.workspace.types()
                                .definitions()
                                .optional_presence_bits
                                .source[analysis_session_.inputs.workspace.types()
                                            .definitions()
                                            .optional_presence_bits.index(*optional)])]);
                analysis_session_.inputs.selection.field.clear();
                analysis_session_.inputs.selection.record_access_members.clear();
                analysis_session_.inputs.selection.record_access_set_explicit = false;
            }
            if (analysis_session_.results().optional_presence_bit_analysis.has_value()) {
                auto const& analysis{*analysis_session_.results().optional_presence_bit_analysis};
                auto const minimum{lispb::codegen::format_packed_integer(analysis.source_minimum)};
                auto const maximum{lispb::codegen::format_packed_integer(analysis.source_maximum)};
                ImGui::Text("Present range: %s .. %s", minimum.c_str(), maximum.c_str());
                ImGui::Text("Present values: %s",
                            detail::format_number(analysis.present_value_count).c_str());
                ImGui::Text("Bit allocation: %u presence + %u payload = %u bits/value",
                            analysis.presence_bits,
                            analysis.payload_bits,
                            analysis.encoded_storage_bits);
                ImGui::Text(
                    "Canonical absence states: %llu",
                    static_cast<unsigned long long>(analysis.canonical_absence_state_count));
                ImGui::Text("Source named sentinel codes: %llu",
                            static_cast<unsigned long long>(analysis.source_sentinel_code_count));
                ImGui::Text("Source unused payload codes: %s",
                            detail::format_number(analysis.source_unused_payload_codes).c_str());
                ImGui::Text("Ignored-payload absence patterns: %s noncanonical",
                            detail::format_number(analysis.noncanonical_absence_patterns).c_str());
                ImGui::Text("Encoded payload at %llu values: %s bits",
                            static_cast<unsigned long long>(analysis.element_count),
                            detail::format_number(analysis.total_encoded_bits).c_str());
                ImGui::TextDisabled(
                    "The one canonical absence state is semantic; other absent payload patterns "
                    "are "
                    "redundant physical encodings, not extra states or byte padding.");
                draw_diagnostics(analysis.diagnostics);
            }
            if (draw_optional_presence_bit_editor(node, *optional)) {
                end_panel();
                return;
            }
        }
    } else if (auto const fixed{
                   ((analysis_session_.inputs.workspace.types().definition(node)).kind() ==
                            lispb::schema::TypeDefinitionKind::fixed_point
                        ? std::optional{analysis_session_.inputs.workspace.types().definition(node)}
                        : std::nullopt)}) {
        if (detail::section("Fixed-point representation")) {
            if (analysis_session_.results().fixed_point_analysis.has_value()) {
                auto const& analysis{*analysis_session_.results().fixed_point_analysis};
                auto const minimum_raw{
                    lispb::codegen::format_packed_integer(analysis.minimum_raw_value)};
                auto const maximum_raw{
                    lispb::codegen::format_packed_integer(analysis.maximum_raw_value)};
                ImGui::Text("Signedness: %s", analysis.signedness ? "signed" : "unsigned");
                ImGui::Text("Bit allocation: %u whole + %u fractional%s",
                            analysis.whole_bits,
                            analysis.fractional_bits,
                            analysis.signedness ? " + 1 sign" : "");
                ImGui::Text("Total encoded width: %u bits", analysis.total_bits);
                ImGui::Text("Raw range: %s .. %s", minimum_raw.c_str(), maximum_raw.c_str());
                ImGui::Text("Scale: %.12g raw codes / unit", static_cast<double>(analysis.scale));
                ImGui::Text("Resolution: %.12g", static_cast<double>(analysis.resolution));
                ImGui::Text("Representable range: %.12g .. %.12g",
                            static_cast<double>(analysis.minimum_value),
                            static_cast<double>(analysis.maximum_value));
                ImGui::Text("Allowed range: %.12g .. %.12g",
                            static_cast<double>(analysis.minimum_allowed_value),
                            static_cast<double>(analysis.maximum_allowed_value));
                ImGui::Text("Rounding: %s",
                            lispb::codegen::fixed_point_rounding_name(analysis.rounding).data());
                ImGui::Text("Maximum rounding error: %.12g",
                            static_cast<double>(analysis.maximum_rounding_error));
                ImGui::Text("Encoded payload at %llu values: %s bits",
                            static_cast<unsigned long long>(analysis.element_count),
                            detail::format_number(analysis.total_encoded_bits).c_str());
                ImGui::TextDisabled("Payload bits are not an allocated byte size until "
                                    "placement/lowering is chosen.");
                draw_diagnostics(analysis.diagnostics);
            }
            if (draw_fixed_point_editor(node, *fixed)) {
                end_panel();
                return;
            }
        }
    } else if (auto const mini_float{
                   ((analysis_session_.inputs.workspace.types().definition(node)).kind() ==
                            lispb::schema::TypeDefinitionKind::mini_float
                        ? std::optional{analysis_session_.inputs.workspace.types().definition(node)}
                        : std::nullopt)}) {
        if (detail::section("Mini-float representation")) {
            if (analysis_session_.results().mini_float_analysis.has_value()) {
                auto const& analysis{*analysis_session_.results().mini_float_analysis};
                ImGui::Text("Bit allocation: %u sign + %u exponent + %u significand = %u bits",
                            analysis.sign_bits,
                            analysis.exponent_bits,
                            analysis.significand_bits,
                            analysis.total_bits);
                ImGui::Text("Exponent bias: %d", analysis.exponent_bias);
                ImGui::Text("Normal exponent range: %d .. %d",
                            analysis.minimum_normal_exponent,
                            analysis.maximum_normal_exponent);
                ImGui::Text("Total code space: %s",
                            detail::format_code_count(analysis.total_code_count).c_str());
                ImGui::Text("Code roles: %llu zero, %llu infinity, %llu NaN, %llu subnormal",
                            static_cast<unsigned long long>(analysis.zero_code_count),
                            static_cast<unsigned long long>(analysis.infinity_code_count),
                            static_cast<unsigned long long>(analysis.nan_code_count),
                            static_cast<unsigned long long>(analysis.nonzero_subnormal_code_count));
                auto draw_numeric = [](char const* label, std::optional<long double> const value) {
                    if (value.has_value()) {
                        ImGui::Text("%s: %.12g", label, static_cast<double>(*value));
                    } else {
                        ImGui::TextDisabled("%s: Unknown", label);
                    }
                };
                draw_numeric("Minimum positive subnormal", analysis.minimum_positive_subnormal);
                draw_numeric("Minimum positive normal", analysis.minimum_positive_normal);
                draw_numeric("Minimum finite", analysis.minimum_finite);
                draw_numeric("Maximum finite", analysis.maximum_finite);
                draw_numeric("Resolution around 1.0", analysis.unit_interval_resolution);
                ImGui::Text("Maximum relative rounding error: %.12g",
                            static_cast<double>(analysis.maximum_relative_rounding_error));
                ImGui::Text("Encoded payload at %llu values: %s bits",
                            static_cast<unsigned long long>(analysis.element_count),
                            detail::format_number(analysis.total_encoded_bits).c_str());
                ImGui::TextDisabled(
                    "IEEE-style code roles are encoding facts; arithmetic conformance, byte order, "
                    "ABI placement, and native compiler support are unspecified.");
                draw_diagnostics(analysis.diagnostics);
            }
            if (draw_mini_float_editor(node, *mini_float)) {
                end_panel();
                return;
            }
        }
    } else if (auto const varint{
                   ((analysis_session_.inputs.workspace.types().definition(node)).kind() ==
                            lispb::schema::TypeDefinitionKind::integer_varint
                        ? std::optional{analysis_session_.inputs.workspace.types().definition(node)}
                        : std::nullopt)}) {
        auto const& source{
            analysis_session_.inputs.workspace.types()
                .occurrences()
                .type[analysis_session_.inputs.workspace.types().occurrences().index(
                    analysis_session_.inputs.workspace.types()
                        .definitions()
                        .integer_varints.source[analysis_session_.inputs.workspace.types()
                                                    .definitions()
                                                    .integer_varints.index(*varint)])]};
        if (detail::section("Variable-length integer representation")) {
            ImGui::TextUnformatted("Semantic source");
            ImGui::SameLine();
            if (ImGui::SmallButton(
                    text(analysis_session_.inputs.workspace.types().cpp_spelling(source))
                        .c_str())) {
                select_type(
                    analysis_session_.inputs.workspace.types()
                        .occurrences()
                        .type[analysis_session_.inputs.workspace.types().occurrences().index(
                            analysis_session_.inputs.workspace.types()
                                .definitions()
                                .integer_varints.source[analysis_session_.inputs.workspace.types()
                                                            .definitions()
                                                            .integer_varints.index(*varint)])]);
                analysis_session_.inputs.selection.field.clear();
                analysis_session_.inputs.selection.record_access_members.clear();
                analysis_session_.inputs.selection.record_access_set_explicit = false;
            }
            if (analysis_session_.results().integer_varint_analysis.has_value()) {
                auto const& analysis{*analysis_session_.results().integer_varint_analysis};
                ImGui::Text("Encoding: %s",
                            lispb::codegen::integer_varint_encoding_name(analysis.encoding).data());
                ImGui::Text("Encoded size: %u .. %u bytes/value",
                            analysis.minimum_encoded_bytes,
                            analysis.maximum_encoded_bytes);
                ImGui::Text("At %llu values: %s .. %s",
                            static_cast<unsigned long long>(analysis.element_count),
                            detail::format_bytes(analysis.minimum_total_bytes).c_str(),
                            detail::format_bytes(analysis.maximum_total_bytes).c_str());
                ImGui::TextDisabled(
                    "Expected encoded size is Unknown until a value distribution is supplied.");
                draw_diagnostics(analysis.diagnostics);
            }
        }
        if (detail::section("Session value distribution")) {
            auto& rows{
                varint_distributions_[analysis_session_.inputs.workspace.types().identity(source)]};
            auto remove_index{std::optional<std::size_t>{}};
            if (ImGui::BeginTable("varint-distribution",
                                  3,
                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("Semantic value");
                ImGui::TableSetupColumn("Weight");
                ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableHeadersRow();
                for (std::size_t index{}; index < rows.size(); ++index) {
                    ImGui::PushID(static_cast<int>(index));
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-1.0F);
                    ImGui::InputText("##value", rows[index].value.data(), rows[index].value.size());
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-1.0F);
                    ImGui::InputScalar("##weight",
                                       ImGuiDataType_U64,
                                       &rows[index].weight,
                                       nullptr,
                                       nullptr,
                                       "%llu");
                    ImGui::TableNextColumn();
                    if (ImGui::SmallButton("Delete")) {
                        remove_index = index;
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            if (remove_index.has_value()) {
                rows.erase(rows.begin() + static_cast<std::ptrdiff_t>(*remove_index));
            }

            ImGui::SetNextItemWidth(150.0F);
            ImGui::InputText("Value##new-varint-distribution",
                             new_varint_distribution_value_.data(),
                             new_varint_distribution_value_.size());
            ImGui::SameLine();
            ImGui::SetNextItemWidth(130.0F);
            ImGui::InputScalar("Weight##new-varint-distribution",
                               ImGuiDataType_U64,
                               &new_varint_distribution_weight_,
                               nullptr,
                               nullptr,
                               "%llu");
            ImGui::SameLine();
            auto const new_value{
                detail::parse_packed_integer(new_varint_distribution_value_.data())};
            ImGui::BeginDisabled(!new_value.has_value());
            if (ImGui::Button("Add##varint-distribution")) {
                VarintDistributionRow row{.value = {}, .weight = new_varint_distribution_weight_};
                std::snprintf(row.value.data(),
                              row.value.size(),
                              "%s",
                              new_varint_distribution_value_.data());
                rows.push_back(row);
            }
            ImGui::EndDisabled();

            auto invalid_literal_count{std::size_t{}};
            for (auto const& row : rows) {
                if (!detail::parse_packed_integer(row.value.data()).has_value()) {
                    ++invalid_literal_count;
                }
            }
            refresh_analysis();
            auto const& distribution{*analysis_session_.results().integer_varint_distribution};
            if (invalid_literal_count != 0) {
                ImGui::TextColored(detail::diagnostic_color(DiagnosticSeverity::error),
                                   "%llu distribution row%s contain an invalid integer literal.",
                                   static_cast<unsigned long long>(invalid_literal_count),
                                   invalid_literal_count == 1 ? "" : "s");
            }
            ImGui::Text("Exact sample weight: %s",
                        detail::format_number(distribution.total_weight).c_str());
            ImGui::Text("Exact sample bytes: %s",
                        detail::format_bytes(distribution.total_encoded_bytes).c_str());
            if (!distribution.entries.empty() &&
                ImGui::BeginTable("varint-distribution-breakdown",
                                  4,
                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("Value");
                ImGui::TableSetupColumn("Weight");
                ImGui::TableSetupColumn("Bytes / value");
                ImGui::TableSetupColumn("Weighted bytes");
                ImGui::TableHeadersRow();
                for (auto const& entry : distribution.entries) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(
                        lispb::codegen::format_packed_integer(entry.value).c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%llu", static_cast<unsigned long long>(entry.weight));
                    ImGui::TableNextColumn();
                    ImGui::Text("%u", entry.encoded_bytes);
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(
                        detail::format_bytes(entry.weighted_encoded_bytes).c_str());
                }
                ImGui::EndTable();
            }
            if (distribution.expected_bytes_per_value.has_value()) {
                ImGui::Text("Expected bytes / value: %.8g",
                            static_cast<double>(*distribution.expected_bytes_per_value));
                ImGui::Text("Expected bytes at %llu values: %.8g",
                            static_cast<unsigned long long>(distribution.selected_element_count),
                            static_cast<double>(*distribution.expected_selected_bytes));
            } else {
                ImGui::TextDisabled("Expected bytes / value: Unknown");
            }
            ImGui::TextDisabled("Session workload only; weights are not written into the LispB "
                                "semantic declaration.");
            draw_diagnostics(distribution.diagnostics);
            if (draw_integer_varint_editor(node, *varint)) {
                end_panel();
                return;
            }
        }
    } else if (auto const record{
                   ((analysis_session_.inputs.workspace.types().definition(node)).kind() ==
                            lispb::schema::TypeDefinitionKind::record
                        ? std::optional{analysis_session_.inputs.workspace.types().definition(node)}
                        : std::nullopt)}) {
        if (detail::section("LispB record declaration")) {
            if (draw_record_editor(node, *record)) {
                end_panel();
                return;
            }
        }
    } else if (auto const union_type{
                   ((analysis_session_.inputs.workspace.types().definition(node)).kind() ==
                            lispb::schema::TypeDefinitionKind::union_type
                        ? std::optional{analysis_session_.inputs.workspace.types().definition(node)}
                        : std::nullopt)}) {
        if (detail::section("Raw union layout")) {
            auto const& analysis{*analysis_session_.results().union_analysis};
            ImGui::Text("Alternatives: %llu",
                        static_cast<unsigned long long>(analysis_session_.inputs.workspace.types()
                                                            .definitions()
                                                            .unions.alternatives_of(*union_type)
                                                            .size()));
            ImGui::Text("Size: %s", detail::format_bytes(analysis.size_bytes).c_str());
            ImGui::Text("Alignment: %s", detail::format_bytes(analysis.alignment_bytes).c_str());
            ImGui::Text("Largest alternative: %s",
                        detail::format_bytes(analysis.largest_alternative_bytes).c_str());
            ImGui::Text("Tail padding: %s",
                        detail::format_bytes(analysis.tail_padding_bytes).c_str());
            if (selected_declaration.has_value()) {
                if (detail::section("Session alternative workload")) {
                    if (ImGui::BeginTable("raw-union-workload-weights",
                                          2,
                                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                              ImGuiTableFlags_SizingStretchProp)) {
                        ImGui::TableSetupColumn("Alternative");
                        ImGui::TableSetupColumn("Weight");
                        ImGui::TableHeadersRow();
                        for (auto const alternative_id : analysis_session_.inputs.workspace.types()
                                                             .definitions()
                                                             .unions.alternatives_of(*union_type)) {
                            auto const alternative{alternative_id};
                            ImGui::PushID(
                                text(definitions()
                                         .union_alternatives
                                         .name[definitions().union_alternatives.index(alternative)])
                                    .c_str());
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(
                                text(definitions()
                                         .union_alternatives
                                         .name[definitions().union_alternatives.index(alternative)])
                                    .c_str());
                            ImGui::TableNextColumn();
                            auto weight{std::uint64_t{0}};
                            if (auto const* weights{
                                    analysis_session_.union_distribution(*selected_declaration)};
                                weights != nullptr) {
                                if (auto const found{weights->find(
                                        text(definitions()
                                                 .union_alternatives
                                                 .name[definitions().union_alternatives.index(
                                                     alternative)]))};
                                    found != weights->end()) {
                                    weight = found->second;
                                }
                            }
                            ImGui::SetNextItemWidth(-1.0F);
                            if (ImGui::InputScalar("##weight", ImGuiDataType_U64, &weight)) {
                                analysis_session_.set_union_distribution_weight(
                                    *selected_declaration,
                                    text(definitions().union_alternatives.name
                                             [definitions().union_alternatives.index(alternative)]),
                                    weight);
                            }
                            ImGui::PopID();
                        }
                        ImGui::EndTable();
                    }
                    if (ImGui::SmallButton("Clear alternative workload")) {
                        analysis_session_.clear_union_distribution(*selected_declaration);
                    }
                    if (analysis_session_.results().union_distribution_analysis.has_value()) {
                        auto const& distribution{
                            *analysis_session_.results().union_distribution_analysis};
                        ImGui::Text("Sample weight: %s",
                                    detail::format_number(distribution.total_weight).c_str());
                        ImGui::Text("Sample active extent: %s",
                                    detail::format_bytes(distribution.total_extent_bytes).c_str());
                        ImGui::Text("Sample conditional slack: %s",
                                    detail::format_bytes(distribution.total_slack_bytes).c_str());
                        if (distribution.expected_extent_bytes_per_value.has_value()) {
                            ImGui::Text(
                                "Expected active extent / value: %.8g bytes",
                                static_cast<double>(*distribution.expected_extent_bytes_per_value));
                            ImGui::Text(
                                "Expected conditional slack / value: %.8g bytes",
                                static_cast<double>(*distribution.expected_slack_bytes_per_value));
                            ImGui::Text(
                                "Expected active extent at %llu values: %.8g bytes",
                                static_cast<unsigned long long>(
                                    distribution.selected_element_count),
                                static_cast<double>(*distribution.expected_selected_extent_bytes));
                            ImGui::Text(
                                "Expected conditional slack at %llu values: %.8g bytes",
                                static_cast<unsigned long long>(
                                    distribution.selected_element_count),
                                static_cast<double>(*distribution.expected_selected_slack_bytes));
                        } else {
                            ImGui::TextDisabled("Expected alternative usage: Unknown");
                        }
                        if (ImGui::BeginTable("raw-union-workload-analysis",
                                              6,
                                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                                  ImGuiTableFlags_SizingStretchProp)) {
                            ImGui::TableSetupColumn("Alternative");
                            ImGui::TableSetupColumn("Weight");
                            ImGui::TableSetupColumn("Extent");
                            ImGui::TableSetupColumn("Slack");
                            ImGui::TableSetupColumn("Weighted extent");
                            ImGui::TableSetupColumn("Weighted slack");
                            ImGui::TableHeadersRow();
                            for (auto const& entry : distribution.entries) {
                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                ImGui::TextUnformatted(entry.alternative_name.c_str());
                                ImGui::TableNextColumn();
                                ImGui::Text("%llu", static_cast<unsigned long long>(entry.weight));
                                ImGui::TableNextColumn();
                                ImGui::TextUnformatted(
                                    detail::format_bytes(entry.extent_bytes).c_str());
                                ImGui::TableNextColumn();
                                ImGui::TextUnformatted(
                                    detail::format_bytes(entry.slack_bytes).c_str());
                                ImGui::TableNextColumn();
                                ImGui::TextUnformatted(
                                    detail::format_bytes(entry.weighted_extent_bytes).c_str());
                                ImGui::TableNextColumn();
                                ImGui::TextUnformatted(
                                    detail::format_bytes(entry.weighted_slack_bytes).c_str());
                            }
                            ImGui::EndTable();
                        }
                        draw_diagnostics(distribution.diagnostics);
                    }
                    ImGui::TextDisabled("Session-only conditional workload; weights are not LispB "
                                        "semantics and do not "
                                        "imply a stored discriminant or a performance result.");
                }
            }
            ImGui::TextDisabled(
                "A raw union has no stored discriminant; tagged unions are separate.");
            draw_diagnostics(analysis.diagnostics);
        }
        if (detail::section("LispB union declaration")) {
            if (draw_union_editor(node, *union_type)) {
                end_panel();
                return;
            }
        }
    } else if (auto const tagged{
                   ((analysis_session_.inputs.workspace.types().definition(node)).kind() ==
                            lispb::schema::TypeDefinitionKind::tagged_union
                        ? std::optional{analysis_session_.inputs.workspace.types().definition(node)}
                        : std::nullopt)}) {
        if (detail::section("Tagged union semantics")) {
            auto const& analysis{*analysis_session_.results().tagged_union_analysis};
            auto const& discriminant{
                analysis_session_.inputs.workspace.types()
                    .occurrences()
                    .type[analysis_session_.inputs.workspace.types().occurrences().index(
                        analysis_session_.inputs.workspace.types()
                            .definitions()
                            .tagged_unions.discriminant[analysis_session_.inputs.workspace.types()
                                                            .definitions()
                                                            .tagged_unions.index(*tagged)])]};
            ImGui::Text(
                "Discriminant: %s",
                text(analysis_session_.inputs.workspace.types().name(discriminant)).c_str());
            ImGui::Text("Size: %s", detail::format_bytes(analysis.size_bytes).c_str());
            ImGui::Text("Alignment: %s", detail::format_bytes(analysis.alignment_bytes).c_str());
            ImGui::Text("Payload offset: %s",
                        detail::format_bytes(analysis.payload_offset_bytes).c_str());
            ImGui::Text("Padding: %s",
                        detail::format_bytes(analysis.internal_padding_bytes).c_str());
            ImGui::Text("Tail padding: %s",
                        detail::format_bytes(analysis.tail_padding_bytes).c_str());
            ImGui::Text("Tag coverage: %llu mapped, %llu unmapped, %llu named sentinel%s",
                        static_cast<unsigned long long>(analysis.mapped_live_tags.size()),
                        static_cast<unsigned long long>(analysis.unmapped_live_tags.size()),
                        static_cast<unsigned long long>(analysis.sentinel_tags.size()),
                        analysis.count_sentinel_tag.has_value() ? ", count sentinel present" : "");
            if (ImGui::SmallButton("Go to discriminant")) {
                select_type(
                    analysis_session_.inputs.workspace.types()
                        .occurrences()
                        .type[analysis_session_.inputs.workspace.types().occurrences().index(
                            analysis_session_.inputs.workspace.types()
                                .definitions()
                                .tagged_unions
                                .discriminant[analysis_session_.inputs.workspace.types()
                                                  .definitions()
                                                  .tagged_unions.index(*tagged)])]);
                analysis_session_.inputs.selection.field.clear();
                end_panel();
                return;
            }
            if (ImGui::BeginTable("tagged-union-properties",
                                  4,
                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("Tag");
                ImGui::TableSetupColumn("Alternative");
                ImGui::TableSetupColumn("Type");
                ImGui::TableSetupColumn("Count");
                ImGui::TableHeadersRow();
                for (auto const id : analysis_session_.inputs.workspace.types()
                                         .definitions()
                                         .tagged_unions.alternatives_of(*tagged)) {
                    auto const alternative{id};
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(
                        text(definitions()
                                 .tagged_union_alternatives
                                 .tag[definitions().tagged_union_alternatives.index(alternative)])
                            .c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(
                        text(definitions()
                                 .tagged_union_alternatives
                                 .name[definitions().tagged_union_alternatives.index(alternative)])
                            .c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(
                        text(
                            analysis_session_.inputs.workspace.types().cpp_spelling(
                                analysis_session_.inputs.workspace.types().occurrences().type
                                    [analysis_session_.inputs.workspace.types().occurrences().index(
                                        definitions().tagged_union_alternatives.semantic_type
                                            [definitions().tagged_union_alternatives.index(
                                                alternative)])]))
                            .c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text(
                        "%llu",
                        static_cast<unsigned long long>(
                            definitions()
                                .tagged_union_alternatives
                                .count[definitions().tagged_union_alternatives.index(alternative)]
                                .value_or(1)));
                }
                ImGui::EndTable();
            }
            if (selected_declaration.has_value()) {
                if (detail::section("Session tag workload")) {
                    if (ImGui::BeginTable("tagged-union-workload-weights",
                                          3,
                                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                              ImGuiTableFlags_SizingStretchProp)) {
                        ImGui::TableSetupColumn("Tag");
                        ImGui::TableSetupColumn("Alternative");
                        ImGui::TableSetupColumn("Weight");
                        ImGui::TableHeadersRow();
                        for (auto const id : analysis_session_.inputs.workspace.types()
                                                 .definitions()
                                                 .tagged_unions.alternatives_of(*tagged)) {
                            auto const alternative{id};
                            ImGui::PushID(
                                text(definitions()
                                         .tagged_union_alternatives
                                         .tag[definitions().tagged_union_alternatives.index(
                                             alternative)])
                                    .c_str());
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(
                                text(definitions()
                                         .tagged_union_alternatives
                                         .tag[definitions().tagged_union_alternatives.index(
                                             alternative)])
                                    .c_str());
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(
                                text(definitions()
                                         .tagged_union_alternatives
                                         .name[definitions().tagged_union_alternatives.index(
                                             alternative)])
                                    .c_str());
                            ImGui::TableNextColumn();
                            auto weight{std::uint64_t{0}};
                            if (auto const* weights{analysis_session_.tagged_union_distribution(
                                    *selected_declaration)};
                                weights != nullptr) {
                                if (auto const found{weights->find(
                                        text(definitions()
                                                 .tagged_union_alternatives
                                                 .tag[definitions().tagged_union_alternatives.index(
                                                     alternative)]))};
                                    found != weights->end()) {
                                    weight = found->second;
                                }
                            }
                            ImGui::SetNextItemWidth(-1.0F);
                            if (ImGui::InputScalar("##weight", ImGuiDataType_U64, &weight)) {
                                analysis_session_.set_tagged_union_distribution_weight(
                                    *selected_declaration,
                                    text(definitions()
                                             .tagged_union_alternatives
                                             .tag[definitions().tagged_union_alternatives.index(
                                                 alternative)]),
                                    weight);
                            }
                            ImGui::PopID();
                        }
                        ImGui::EndTable();
                    }
                    if (ImGui::SmallButton("Clear tag workload")) {
                        analysis_session_.clear_tagged_union_distribution(*selected_declaration);
                    }
                    if (analysis_session_.results()
                            .tagged_union_distribution_analysis.has_value()) {
                        auto const& distribution{
                            *analysis_session_.results().tagged_union_distribution_analysis};
                        ImGui::Text("Sample weight: %s",
                                    detail::format_number(distribution.total_weight).c_str());
                        ImGui::Text(
                            "Sample active payload extent: %s",
                            detail::format_bytes(distribution.total_payload_extent_bytes).c_str());
                        ImGui::Text(
                            "Sample payload slack: %s",
                            detail::format_bytes(distribution.total_payload_slack_bytes).c_str());
                        if (distribution.expected_payload_extent_bytes_per_value.has_value()) {
                            ImGui::Text("Expected active payload / value: %.8g bytes",
                                        static_cast<double>(
                                            *distribution.expected_payload_extent_bytes_per_value));
                            ImGui::Text("Expected payload slack / value: %.8g bytes",
                                        static_cast<double>(
                                            *distribution.expected_payload_slack_bytes_per_value));
                            ImGui::Text("Expected active payload at %llu values: %.8g bytes",
                                        static_cast<unsigned long long>(
                                            distribution.selected_element_count),
                                        static_cast<double>(
                                            *distribution.expected_selected_payload_extent_bytes));
                            ImGui::Text("Expected payload slack at %llu values: %.8g bytes",
                                        static_cast<unsigned long long>(
                                            distribution.selected_element_count),
                                        static_cast<double>(
                                            *distribution.expected_selected_payload_slack_bytes));
                        }
                        if (ImGui::BeginTable("tagged-union-workload-analysis",
                                              6,
                                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                                  ImGuiTableFlags_SizingStretchProp)) {
                            ImGui::TableSetupColumn("Tag");
                            ImGui::TableSetupColumn("Weight");
                            ImGui::TableSetupColumn("Extent");
                            ImGui::TableSetupColumn("Slack");
                            ImGui::TableSetupColumn("Weighted extent");
                            ImGui::TableSetupColumn("Weighted slack");
                            ImGui::TableHeadersRow();
                            for (auto const& entry : distribution.entries) {
                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                ImGui::TextUnformatted(entry.tag.c_str());
                                ImGui::TableNextColumn();
                                ImGui::Text("%llu", static_cast<unsigned long long>(entry.weight));
                                ImGui::TableNextColumn();
                                ImGui::TextUnformatted(
                                    detail::format_bytes(entry.payload_extent_bytes).c_str());
                                ImGui::TableNextColumn();
                                ImGui::TextUnformatted(
                                    detail::format_bytes(entry.payload_slack_bytes).c_str());
                                ImGui::TableNextColumn();
                                ImGui::TextUnformatted(
                                    detail::format_bytes(entry.weighted_payload_extent_bytes)
                                        .c_str());
                                ImGui::TableNextColumn();
                                ImGui::TextUnformatted(
                                    detail::format_bytes(entry.weighted_payload_slack_bytes)
                                        .c_str());
                            }
                            ImGui::EndTable();
                        }
                        draw_diagnostics(distribution.diagnostics);
                    } else {
                        ImGui::TextDisabled(
                            "Assign a positive weight to calculate workload expectations.");
                    }
                    ImGui::TextDisabled(
                        "Session workload only; weights are not written into LispB.");
                }
            }
            draw_diagnostics(analysis.diagnostics);
        }
        if (detail::section("LispB tagged-union declaration")) {
            if (draw_tagged_union_editor(node, *tagged)) {
                end_panel();
                return;
            }
        }
    } else if (auto const external{
                   ((analysis_session_.inputs.workspace.types().definition(node)).kind() ==
                            lispb::schema::TypeDefinitionKind::external
                        ? std::optional{analysis_session_.inputs.workspace.types().definition(node)}
                        : std::nullopt)}) {
        if (detail::section("External type")) {
            ImGui::Text(
                "C++ spelling: %s",
                std::string{analysis_session_.inputs.workspace.types().cpp_types().spelling(
                                analysis_session_.inputs.workspace.types()
                                    .definitions()
                                    .externals.cpp_type[analysis_session_.inputs.workspace.types()
                                                            .definitions()
                                                            .externals.index(*external)])}
                    .c_str());
            if (!analysis_session_.inputs.workspace.types()
                     .definitions()
                     .externals.registered_names_of(*external)
                     .empty()) {
                ImGui::TextUnformatted("Registered as");
                for (auto const& name : analysis_session_.inputs.workspace.types()
                                            .definitions()
                                            .externals.registered_names_of(*external)) {
                    ImGui::BulletText("@%s", text(name).c_str());
                    if (document_.has_value()) {
                        if (auto const source{document_->registered_type_source(text(name))}) {
                            auto const& path{document_->storage().sources.filesystem_path(
                                document_->source_span(*source).file)};
                            ImGui::TextWrapped("Source: %s:%llu",
                                               path.string().c_str(),
                                               static_cast<unsigned long long>(
                                                   document_->source_location(*source).line));
                            ImGui::PushID(text(name).c_str());
                            if (ImGui::SmallButton("View source")) {
                                selected_source_view_path_ = path.lexically_normal();
                                source_view_open_ = true;
                                focus_source_view_ = true;
                            }
                            ImGui::PopID();
                        }
                    }
                }
            }
            if (auto const scalar{analysis_session_.inputs.workspace.types()
                                      .definitions()
                                      .externals
                                      .integer_semantics[analysis_session_.inputs.workspace.types()
                                                             .definitions()
                                                             .externals.index(*external)]
                                      .transform([&](TypeDefinitionId id) { return id; })}) {
                ImGui::Text(
                    "Integer domain: %s, %u bits",
                    analysis_session_.inputs.workspace.types()
                            .definitions()
                            .integer_scalars.signedness[analysis_session_.inputs.workspace.types()
                                                            .definitions()
                                                            .integer_scalars.index(*scalar)]
                        ? "signed"
                        : "unsigned",
                    analysis_session_.inputs.workspace.types()
                        .definitions()
                        .integer_scalars.bit_width[analysis_session_.inputs.workspace.types()
                                                       .definitions()
                                                       .integer_scalars.index(*scalar)]);
                ImGui::Text("Live range: %s .. %s",
                            lispb::codegen::format_packed_integer(
                                analysis_session_.inputs.workspace.types()
                                    .definitions()
                                    .integer_scalars
                                    .minimum_value[analysis_session_.inputs.workspace.types()
                                                       .definitions()
                                                       .integer_scalars.index(*scalar)])
                                .c_str(),
                            lispb::codegen::format_packed_integer(
                                analysis_session_.inputs.workspace.types()
                                    .definitions()
                                    .integer_scalars
                                    .maximum_value[analysis_session_.inputs.workspace.types()
                                                       .definitions()
                                                       .integer_scalars.index(*scalar)])
                                .c_str());
                for (auto const id : declarations().items(
                         analysis_session_.inputs.workspace.types()
                             .definitions()
                             .integer_scalars.named_codes[analysis_session_.inputs.workspace.types()
                                                              .definitions()
                                                              .integer_scalars.index(*scalar)])) {
                    auto const code{id};
                    ImGui::BulletText(
                        "%s = %s%s",
                        text(declarations()
                                 .packed_named_codes
                                 .name[declarations().packed_named_codes.index(code)])
                            .c_str(),
                        lispb::codegen::format_packed_integer(
                            declarations()
                                .packed_named_codes
                                .value[declarations().packed_named_codes.index(code)])
                            .c_str(),
                        declarations()
                                .packed_named_codes
                                .sentinel[declarations().packed_named_codes.index(code)]
                            ? " [sentinel]"
                            : "");
                }
            } else if (auto const format{
                           analysis_session_.inputs.workspace.types()
                               .definitions()
                               .externals
                               .floating_semantics[analysis_session_.inputs.workspace.types()
                                                       .definitions()
                                                       .externals.index(*external)]}) {
                auto const info{lispb::codegen::floating_point_format_info(*format)};
                ImGui::Text("Floating-point format: %s",
                            lispb::codegen::floating_point_format_name(*format).data());
                ImGui::Text("Precision: %u binary digits", info.fraction_bits + 1);
                ImGui::Text("Sign / exponent / fraction: 1 / %u / %u bits",
                            info.exponent_bits,
                            info.fraction_bits);
                ImGui::Text("Normal exponent range: %d .. %d",
                            1 - info.exponent_bias,
                            static_cast<int>((std::uint32_t{1} << info.exponent_bits) - 2) -
                                info.exponent_bias);
                ImGui::TextUnformatted("Supports signed zero, subnormal values, infinity and NaN.");
            } else {
                ImGui::TextDisabled("Scalar semantics are not declared in LispB.");
            }
            draw_external_facts(node);
        }
    } else {
        if (auto const packed{
                ((analysis_session_.inputs.workspace.types().definition(node)).kind() ==
                         lispb::schema::TypeDefinitionKind::packed
                     ? std::optional{analysis_session_.inputs.workspace.types().definition(node)}
                     : std::nullopt)}) {
            if (detail::section("LispB packed declaration")) {
                if (draw_packed_editor(node, *packed)) {
                    end_panel();
                    return;
                }
            }
        } else if (auto const soa{
                       ((analysis_session_.inputs.workspace.types().definition(node)).kind() ==
                                lispb::schema::TypeDefinitionKind::soa
                            ? std::optional{analysis_session_.inputs.workspace.types().definition(
                                  node)}
                            : std::nullopt)}) {
            if (detail::section("LispB SoA declaration")) {
                if (draw_soa_editor(node, *soa)) {
                    end_panel();
                    return;
                }
            }
        }

        auto const editable{analysis_session_.inputs.workspace.active_variant_id() !=
                            LayoutWorkspace::baseline_variant_id};
        if (!editable) {
            if (detail::section("Baseline")) {
                if (capabilities.supports_variant_overrides) {
                    ImGui::TextDisabled("Planning overrides are read only on the baseline.");
                    ImGui::TextWrapped(
                        "Edit the LispB declaration above, or create an experiment for "
                        "session-only physical overrides.");
                } else {
                    ImGui::TextDisabled("Loaded from LispB — read only.");
                    ImGui::TextWrapped(
                        "Experiments are session-only variants. The production schema "
                        "is never modified.");
                }
                ImGui::TextDisabled(
                    "Use + Add variant in the Layout panel to create an experiment.");
                if (((analysis_session_.inputs.workspace.types().definition(node)).kind() ==
                     lispb::schema::TypeDefinitionKind::soa)) {
                    ImGui::TextDisabled("Capacity uses the planner default of %llu.",
                                        static_cast<unsigned long long>(
                                            analysis_session_.inputs.workspace.default_capacity()));
                }
            }
        } else {
            if (detail::section("Experiment")) {
                ImGui::Text("Editing %s",
                            analysis_session_.inputs.workspace.active_variant().name.c_str());
            }
        }

        ImGui::BeginDisabled(!editable);
        if (auto const packed{
                ((analysis_session_.inputs.workspace.types().definition(node)).kind() ==
                         lispb::schema::TypeDefinitionKind::packed
                     ? std::optional{analysis_session_.inputs.workspace.types().definition(node)}
                     : std::nullopt)}) {
            auto const& analysis{*analysis_session_.results().active_packed};
            if (detail::section("Packed storage")) {
                ImGui::Text("Schema storage: %s",
                            analysis_session_.results().active_packed->schema_storage_type.c_str());
                if (ImGui::BeginCombo("Planning storage", analysis.storage_type.c_str())) {
                    if (ImGui::Selectable("Schema storage", !analysis.storage_overridden)) {
                        analysis_session_.inputs.workspace.set_packed_storage_type(selected,
                                                                                   std::nullopt);
                    }
                    for (auto const& [type, facts] : analysis_session_.primary_abi().types()) {
                        if (facts.unsigned_value_bits.has_value() &&
                            ImGui::Selectable(type.c_str(), analysis.storage_type == type)) {
                            analysis_session_.inputs.workspace.set_packed_storage_type(selected,
                                                                                       type);
                        }
                    }
                    ImGui::EndCombo();
                }
                draw_override_note(analysis.storage_overridden);
                if (analysis.storage_overridden) {
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Reset storage")) {
                        analysis_session_.inputs.workspace.set_packed_storage_type(selected,
                                                                                   std::nullopt);
                    }
                }

                if (analysis_session_.inputs.selection.field.empty() &&
                    !analysis_session_.inputs.workspace.types()
                         .definitions()
                         .packed.segments_of(*packed)
                         .empty()) {
                    analysis_session_.inputs.selection.field =
                        packed_name(analysis_session_.inputs.workspace.types()
                                        .definitions()
                                        .packed.segments_of(*packed)
                                        .front());
                }
            }
            if (detail::section("Fields")) {
                if (detail::begin_editable_table("packed-fields", 4, analysis.fields.size())) {
                    detail::editable_table_column("Field");
                    detail::editable_table_column("Semantic type");
                    detail::editable_table_column("Schema");
                    detail::editable_table_column("Planning");
                    ImGui::TableHeadersRow();
                    for (auto const& field : analysis.fields) {
                        ImGui::PushID(field.name.c_str());
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        if (ImGui::Selectable(field.name.c_str(),
                                              analysis_session_.inputs.selection.field ==
                                                  field.name)) {
                            analysis_session_.inputs.selection.field = field.name;
                        }
                        ImGui::TableNextColumn();
                        if (field.semantic_type.has_value()) {
                            auto const& semantic_type{

                                *field.semantic_type};
                            if (ImGui::SmallButton(
                                    text(analysis_session_.inputs.workspace.types().cpp_spelling(
                                             semantic_type))
                                        .c_str())) {
                                select_type(*field.semantic_type);
                                analysis_session_.inputs.selection.field.clear();
                                analysis_session_.inputs.selection.record_access_members.clear();
                                analysis_session_.inputs.selection.record_access_set_explicit =
                                    false;
                            }
                        } else {
                            ImGui::TextDisabled("Reserved");
                        }
                        ImGui::TableNextColumn();
                        if (field.schema_bit_width_auto) {
                            ImGui::Text("auto => %u", field.schema_bit_width);
                        } else {
                            ImGui::Text("%u bits", field.schema_bit_width);
                        }
                        ImGui::TableNextColumn();
                        if (field.reserved) {
                            ImGui::TextDisabled("Durable");
                        } else {
                            auto width{field.bit_width};
                            ImGui::SetNextItemWidth(-1.0F);
                            if (ImGui::InputScalar("##planning-width", ImGuiDataType_U32, &width)) {
                                width = std::clamp(width, std::uint32_t{1}, std::uint32_t{64});
                                analysis_session_.inputs.workspace.set_packed_field_width(
                                    selected, field.name, width);
                            }
                            if (field.overridden && ImGui::SmallButton("Reset")) {
                                analysis_session_.inputs.workspace.set_packed_field_width(
                                    selected, field.name, std::nullopt);
                            }
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndTable();
                }
            }
        } else if (auto const soa{
                       ((analysis_session_.inputs.workspace.types().definition(node)).kind() ==
                                lispb::schema::TypeDefinitionKind::soa
                            ? std::optional{analysis_session_.inputs.workspace.types().definition(
                                  node)}
                            : std::nullopt)}) {
            auto const& analysis{*analysis_session_.results().active_soa};
            if (detail::section("Planner capacity")) {
                auto capacity{analysis.capacity};
                if (ImGui::InputScalar("Capacity", ImGuiDataType_U64, &capacity)) {
                    analysis_session_.inputs.workspace.set_capacity(selected, capacity);
                }
                draw_override_note(analysis.capacity_overridden);
                if (analysis.capacity_overridden) {
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Reset capacity")) {
                        analysis_session_.inputs.workspace.set_capacity(selected, std::nullopt);
                    }
                }

                if (analysis_session_.inputs.selection.field.empty() &&
                    !analysis_session_.inputs.workspace.types()
                         .definitions()
                         .soas.columns_of(*soa)
                         .empty()) {
                    analysis_session_.inputs.selection.field =
                        text(definitions().soa_columns.name[definitions().soa_columns.index(
                            analysis_session_.inputs.workspace.types()
                                .definitions()
                                .soas.columns_of(*soa)
                                .front())]);
                }
            }
            if (detail::section("Columns")) {
                if (detail::begin_editable_table(
                        "soa-columns-properties", 3, analysis.columns.size())) {
                    detail::editable_table_column("Column");
                    detail::editable_table_column("Semantic type");
                    detail::editable_table_column("Planning");
                    ImGui::TableHeadersRow();
                    for (auto const& column : analysis.columns) {
                        ImGui::PushID(column.name.c_str());
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        if (ImGui::Selectable(column.name.c_str(),
                                              analysis_session_.inputs.selection.field ==
                                                  column.name)) {
                            analysis_session_.inputs.selection.field = column.name;
                        }
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(column.schema_type.c_str());
                        ImGui::TableNextColumn();
                        ImGui::SetNextItemWidth(-1.0F);
                        if (ImGui::BeginCombo("##planning-type", column.physical_type.c_str())) {
                            if (ImGui::Selectable("Schema type", !column.overridden)) {
                                analysis_session_.inputs.workspace.set_soa_column_type(
                                    selected, column.name, std::nullopt);
                            }
                            for (auto const& [type, facts] :
                                 analysis_session_.primary_abi().types()) {
                                static_cast<void>(facts);
                                if (ImGui::Selectable(type.c_str(), column.physical_type == type)) {
                                    analysis_session_.inputs.workspace.set_soa_column_type(
                                        selected, column.name, type);
                                }
                            }
                            ImGui::EndCombo();
                        }
                        if (column.overridden && ImGui::SmallButton("Reset")) {
                            analysis_session_.inputs.workspace.set_soa_column_type(
                                selected, column.name, std::nullopt);
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndTable();
                }
            }
        }
        ImGui::EndDisabled();
    }

    auto draw_links = [&](char const* heading, std::span<TypeId const> const links) {
        if (detail::section(heading)) {
            if (links.empty()) {
                ImGui::TextDisabled("None");
            }
            for (auto const linked : links) {
                auto const& linked_node{linked};
                ImGui::PushID(static_cast<int>(linked.value));
                if (ImGui::SmallButton(
                        text(analysis_session_.inputs.workspace.types().cpp_spelling(linked_node))
                            .c_str())) {
                    select_type(linked);
                    analysis_session_.inputs.selection.field.clear();
                    analysis_session_.inputs.selection.record_access_members.clear();
                    analysis_session_.inputs.selection.record_access_set_explicit = false;
                }
                ImGui::PopID();
            }
        }
    };
    draw_links("Depends on", analysis_session_.inputs.workspace.types().dependencies_of(selected));
    draw_links("Used by", analysis_session_.inputs.workspace.types().users_of(selected));
    draw_declaration_dependencies(analysis_session_.inputs.workspace.types().identity(node));
    draw_type_users(selected);
    end_panel();
}

auto PlannerUi::duplicate_selected_declaration(TypeId const node) -> bool {
    if (!document_) {
        return false;
    }
    auto const declaration{document_->find_declaration(
        analysis_session_.inputs.workspace.types().source_declaration(node))};
    if (!declaration) {
        return false;
    }
    auto const& info{*declaration};
    auto const source{*document_->declaration_handle(*declaration)};
    auto selection{document_->declarations().identity[document_->declarations().index(info)]};
    lispb::codegen::DeclarationHandle copy;
    if (source.kind() == lispb::codegen::DeclarationKind::soa) {
        auto prepared{document_->prepare_soa_duplicate(*declaration)};
        if (!prepared) {
            schema_edit_message_ = prepared.error().message;
            return false;
        }
        selection.name = declarations().soas.name[declarations().soas.index(*prepared)];
        copy = *prepared;
    } else {
        auto const base{text(selection.name) + "_copy"};
        selection.name = strings().intern(base);
        for (auto suffix{2U};
             document_->find_declaration(selection) ||
             document_->types().find_declared(
                 analysis_session_.inputs.workspace.types().module(node), selection.name);
             ++suffix) {
            selection.name = strings().intern(base + std::to_string(suffix));
        }
        copy = declarations().clone(source);
        switch (copy.kind()) {
            case lispb::codegen::DeclarationKind::enumeration:
                declarations().enums.name[declarations().enums.index(copy)] = selection.name;
                break;
            case lispb::codegen::DeclarationKind::facade:
                declarations().facades.name[declarations().facades.index(copy)] = selection.name;
                break;
            case lispb::codegen::DeclarationKind::fixed_point:
                declarations().fixed_points.name[declarations().fixed_points.index(copy)] =
                    selection.name;
                break;
            case lispb::codegen::DeclarationKind::homogeneous_layout:
                declarations()
                    .homogeneous_layouts.name[declarations().homogeneous_layouts.index(copy)] =
                    selection.name;
                break;
            case lispb::codegen::DeclarationKind::integer_scalar:
                declarations().integer_scalars.name[declarations().integer_scalars.index(copy)] =
                    selection.name;
                break;
            case lispb::codegen::DeclarationKind::integer_varint:
                declarations().integer_varints.name[declarations().integer_varints.index(copy)] =
                    selection.name;
                break;
            case lispb::codegen::DeclarationKind::linear_quantized:
                declarations()
                    .linear_quantizeds.name[declarations().linear_quantizeds.index(copy)] =
                    selection.name;
                break;
            case lispb::codegen::DeclarationKind::mini_float:
                declarations().mini_floats.name[declarations().mini_floats.index(copy)] =
                    selection.name;
                break;
            case lispb::codegen::DeclarationKind::optional_presence_bit:
                declarations()
                    .optional_presence_bits
                    .name[declarations().optional_presence_bits.index(copy)] = selection.name;
                break;
            case lispb::codegen::DeclarationKind::optional_sentinel:
                declarations()
                    .optional_sentinels.name[declarations().optional_sentinels.index(copy)] =
                    selection.name;
                break;
            case lispb::codegen::DeclarationKind::packed_value:
                declarations().packed_values.name[declarations().packed_values.index(copy)] =
                    selection.name;
                break;
            case lispb::codegen::DeclarationKind::record:
                declarations().records.name[declarations().records.index(copy)] = selection.name;
                break;
            case lispb::codegen::DeclarationKind::soa:
                declarations().soas.name[declarations().soas.index(copy)] = selection.name;
                break;
            case lispb::codegen::DeclarationKind::static_table:
                declarations().static_tables.name[declarations().static_tables.index(copy)] =
                    selection.name;
                break;
            case lispb::codegen::DeclarationKind::tagged_union:
                declarations().tagged_unions.name[declarations().tagged_unions.index(copy)] =
                    selection.name;
                break;
            case lispb::codegen::DeclarationKind::union_type:
                declarations().unions.name[declarations().unions.index(copy)] = selection.name;
                break;
            case lispb::codegen::DeclarationKind::vector_soa:
                declarations().vector_soas.name[declarations().vector_soas.index(copy)] =
                    selection.name;
                break;
            default:
                std::unreachable();
        }
    }
    return apply_document_edit(
        document_->create_declaration(
            document_->allocate_declaration_id(),
            document_->declarations().module_index[document_->declarations().index(info)],
            copy),
        selection);
}

auto PlannerUi::delete_declaration(DeclarationId const declaration) -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const info{document_->declarations().find(declaration)};
    if (!info.has_value()) {
        schema_edit_message_ = "The declaration no longer exists.";
        return false;
    }

    if (!apply_document_edit(document_->delete_declaration(declaration))) {
        return false;
    }
    analysis_session_.inputs.selection.record_access_members.clear();
    analysis_session_.inputs.selection.record_access_set_explicit = false;
    return true;
}

void PlannerUi::draw_variants_panel() {
    if (!variants_view_open_) {
        return;
    }
    auto const was_open{variants_view_open_};
    ImGui::Begin("Variants", &variants_view_open_, ImGuiWindowFlags_HorizontalScrollbar);
    persist_view_visibility(was_open, variants_view_open_);
    auto const selected_type{analysis_session_.inputs.selection.type};
    if (!selected_type.has_value() ||
        !declaration_capabilities(analysis_session_.inputs.workspace.types(), *selected_type)
             .physical_analysis_available) {
        ImGui::TextWrapped("Physical variants are unavailable for this selection.");
        ImGui::End();
        return;
    }
    auto const baseline_before_actions{analysis_session_.inputs.workspace.active_variant_id() ==
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
            auto const name{analysis_session_.inputs.workspace.active_variant().name + " copy"};
            analysis_session_.inputs.workspace.duplicate_variant(
                analysis_session_.inputs.workspace.active_variant_id(), name);
            sync_variant_name();
        }
        ImGui::TableNextColumn();
        ImGui::BeginDisabled(baseline_before_actions);
        if (ImGui::Button("Reset all overrides", {-1.0F, 0.0F})) {
            analysis_session_.inputs.workspace.reset_variant(
                analysis_session_.inputs.workspace.active_variant_id());
        }
        ImGui::TableNextColumn();
        if (ImGui::Button("Delete", {-1.0F, 0.0F})) {
            analysis_session_.inputs.workspace.delete_variant(
                analysis_session_.inputs.workspace.active_variant_id());
            sync_variant_name();
            packed_dragged_divider_.reset();
            packed_dragged_variant_id_.reset();
        }
        ImGui::EndDisabled();
        ImGui::EndTable();
    }

    auto const baseline{analysis_session_.inputs.workspace.active_variant_id() ==
                        LayoutWorkspace::baseline_variant_id};
    ImGui::BeginDisabled(baseline);
    if (variant_name_id_ != analysis_session_.inputs.workspace.active_variant_id()) {
        sync_variant_name();
    }
    if (ImGui::InputText("Name",
                         variant_name_.data(),
                         variant_name_.size(),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        analysis_session_.inputs.workspace.rename_variant(
            analysis_session_.inputs.workspace.active_variant_id(), variant_name_.data());
        sync_variant_name();
    }
    ImGui::EndDisabled();

    ImGui::Separator();
    if (ImGui::BeginChild(
            "variant-list", {0.0F, 0.0F}, false, ImGuiWindowFlags_HorizontalScrollbar)) {
        for (auto const& variant : analysis_session_.inputs.workspace.variants()) {
            auto const selected{analysis_session_.inputs.workspace.active_variant_id() ==
                                variant.id};
            auto const label{variant.id == LayoutWorkspace::baseline_variant_id
                                 ? "Baseline"
                                 : variant.name + " (" +
                                       std::to_string(detail::override_count(variant.overrides)) +
                                       " overrides)"};
            if (ImGui::Selectable(label.c_str(), selected)) {
                analysis_session_.inputs.workspace.select_variant(variant.id);
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
