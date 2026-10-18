#include "planner_ui_properties_common.hpp"

#include <codegen/schema.h>
#include <codegen/schema/declaration_schema.h>
#include <codegen/schema/fixed_soa_schemas.h>
#include <codegen/schema/function_schemas.h>
#include <codegen/schema/module_settings_store.h>
#include <codegen/schema/normal_module_schemas.h>
#include <codegen/schema/parameter_schemas.h>
#include <codegen/schema/semantic_relation_kind.h>
#include <codegen/schema/semantic_relation_schemas.h>
#include <codegen/schema/soa_mask_dimension_schemas.h>
#include <codegen/schema/soa_member_kind.h>
#include <codegen/schema/soa_member_schemas.h>
#include <codegen/schema/soa_schemas.h>
#include <codegen/schema/storage_operation.h>
#include <codegen/schema/type_refs.h>
#include <lispb/compiler_storage.h>
#include <lispb/indices.h>
#include <lispb/schema/soa_columns.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ranges>
#include <string>
#include <utility>

namespace ioj::layout_planner {

auto PlannerUi::draw_soa_editor(TypeId const node, TypeDefinitionId const& soa) -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const declaration{
        document_->find_declaration(analysis_session_.inputs.workspace.types().identity(node))};
    if (!declaration.has_value()) {
        return false;
    }
    auto const schema{
        document_->declaration_handle(*declaration, lispb::codegen::DeclarationKind::soa)};
    if (!schema.has_value()) {
        return false;
    }

    if (analysis_session_.inputs.selection.field.empty() &&
        !declarations()
             .items(document_->storage()
                        .declarations.soas
                        .members[document_->storage().declarations.soas.index(*schema)])
             .empty()) {
        analysis_session_.inputs.selection.field =
            text(declarations().soa_members.name[declarations().soa_members.index(
                declarations()
                    .items(document_->storage().declarations.soas.members
                               [document_->storage().declarations.soas.index(*schema)])
                    .front())]);
    }
    auto selected{std::ranges::find(
        declarations().items(
            document_->storage()
                .declarations.soas.members[document_->storage().declarations.soas.index(*schema)]),
        strings().intern(analysis_session_.inputs.selection.field),
        [&](auto id) {
            return declarations().soa_members.name[declarations().soa_members.index(id)];
        })};
    if (selected == declarations()
                        .items(document_->storage()
                                   .declarations.soas
                                   .members[document_->storage().declarations.soas.index(*schema)])
                        .end() &&
        !declarations()
             .items(document_->storage()
                        .declarations.soas
                        .members[document_->storage().declarations.soas.index(*schema)])
             .empty()) {
        selected = declarations()
                       .items(document_->storage()
                                  .declarations.soas
                                  .members[document_->storage().declarations.soas.index(*schema)])
                       .begin();
        analysis_session_.inputs.selection.field =
            text(declarations().soa_members.name[declarations().soa_members.index(*selected)]);
    }
    auto const selected_index{
        selected == declarations()
                        .items(document_->storage()
                                   .declarations.soas
                                   .members[document_->storage().declarations.soas.index(*schema)])
                        .end()
            ? std::optional<std::size_t>{}
            : std::optional<std::size_t>{static_cast<std::size_t>(
                  selected - declarations()
                                 .items(document_->storage().declarations.soas.members
                                            [document_->storage().declarations.soas.index(*schema)])
                                 .begin())}};
    auto const selected_is_mask_storage{
        selected != declarations()
                        .items(document_->storage()
                                   .declarations.soas
                                   .members[document_->storage().declarations.soas.index(*schema)])
                        .end() &&
        document_->storage()
            .declarations.soas
            .field_mask_name[document_->storage().declarations.soas.index(*schema)]
            .has_value() &&
        declarations().type_refs.name[declarations().type_refs.index(
            declarations().soa_members.type[declarations().soa_members.index(*selected)])] ==
            *document_->storage()
                 .declarations.soas
                 .field_mask_name[document_->storage().declarations.soas.index(*schema)]};
    auto const selected_is_final_mask_field{
        selected != declarations()
                        .items(document_->storage()
                                   .declarations.soas
                                   .members[document_->storage().declarations.soas.index(*schema)])
                        .end() &&
        declarations().soa_members.mask_field[declarations().soa_members.index(*selected)] &&
        std::ranges::count_if(
            declarations().items(
                document_->storage()
                    .declarations.soas
                    .members[document_->storage().declarations.soas.index(*schema)]),
            [&](auto id) {
                return declarations().soa_members.mask_field[declarations().soa_members.index(id)];
            }) == 1};

    if (soa_editor_declaration_ != declaration ||
        soa_editor_member_ != analysis_session_.inputs.selection.field) {
        soa_editor_declaration_ = declaration;
        soa_editor_member_ = analysis_session_.inputs.selection.field;
        std::snprintf(
            soa_view_name_.data(),
            soa_view_name_.size(),
            "%s",
            (document_->storage().declarations.soas.view_name[document_->storage()
                                                                  .declarations.soas.index(*schema)]
                 ? text(*document_->storage()
                             .declarations.soas
                             .view_name[document_->storage().declarations.soas.index(*schema)])
                 : std::string{})
                .c_str());
        std::snprintf(
            soa_const_view_name_.data(),
            soa_const_view_name_.size(),
            "%s",
            (document_->storage()
                     .declarations.soas
                     .const_view_name[document_->storage().declarations.soas.index(*schema)]
                 ? text(
                       *document_->storage()
                            .declarations.soas
                            .const_view_name[document_->storage().declarations.soas.index(*schema)])
                 : std::string{})
                .c_str());
        std::snprintf(
            soa_equivalent_type_.data(),
            soa_equivalent_type_.size(),
            "%s",
            document_->storage()
                    .declarations.soas
                    .equivalent_type[document_->storage().declarations.soas.index(*schema)]
                    .has_value()
                ? text(declarations().type_refs.name[declarations().type_refs.index(
                           (*document_->storage().declarations.soas.equivalent_type
                                 [document_->storage().declarations.soas.index(*schema)]))])
                      .c_str()
                : "");
        std::snprintf(
            soa_export_specifier_.data(),
            soa_export_specifier_.size(),
            "%s",
            text(document_->storage()
                     .declarations.soas
                     .export_specifier[document_->storage().declarations.soas.index(*schema)])
                .c_str());
        soa_using_declarations_.clear();
        soa_using_declarations_.reserve(
            declarations()
                .items(
                    document_->storage()
                        .declarations.soas
                        .using_declarations[document_->storage().declarations.soas.index(*schema)])
                .size());
        for (auto const using_declaration : declarations().items(
                 document_->storage()
                     .declarations.soas
                     .using_declarations[document_->storage().declarations.soas.index(*schema)])) {
            std::array<char, 256> declaration_text{};
            std::snprintf(declaration_text.data(),
                          declaration_text.size(),
                          "%s",
                          text(using_declaration).c_str());
            soa_using_declarations_.push_back(declaration_text);
        }
        if (declarations()
                .items(
                    document_->storage()
                        .declarations.soas
                        .using_declarations[document_->storage().declarations.soas.index(*schema)])
                .empty()) {
            soa_using_declaration_index_.reset();
        } else if (!soa_using_declaration_index_.has_value() ||
                   *soa_using_declaration_index_ >=
                       declarations()
                           .items(document_->storage().declarations.soas.using_declarations
                                      [document_->storage().declarations.soas.index(*schema)])
                           .size()) {
            soa_using_declaration_index_ = 0;
        }
        soa_function_names_.clear();
        soa_function_return_types_.clear();
        soa_function_names_.reserve(
            declarations()
                .items(document_->storage()
                           .declarations.soas
                           .functions[document_->storage().declarations.soas.index(*schema)])
                .size());
        soa_function_return_types_.reserve(
            declarations()
                .items(document_->storage()
                           .declarations.soas
                           .functions[document_->storage().declarations.soas.index(*schema)])
                .size());
        for (auto const function_id : declarations().items(
                 document_->storage()
                     .declarations.soas
                     .functions[document_->storage().declarations.soas.index(*schema)])) {
            auto const function{function_id};
            std::array<char, 128> name{};
            std::array<char, 128> return_type{};
            std::snprintf(
                name.data(),
                name.size(),
                "%s",
                text(declarations().functions.name[declarations().functions.index(function)])
                    .c_str());
            std::snprintf(
                return_type.data(),
                return_type.size(),
                "%s",
                text(declarations().type_refs.name[declarations().type_refs.index(
                         declarations()
                             .functions.return_type[declarations().functions.index(function)])])
                    .c_str());
            soa_function_names_.push_back(name);
            soa_function_return_types_.push_back(return_type);
        }
        if (declarations()
                .items(document_->storage()
                           .declarations.soas
                           .functions[document_->storage().declarations.soas.index(*schema)])
                .empty()) {
            soa_function_index_.reset();
        } else if (!soa_function_index_.has_value() ||
                   *soa_function_index_ >=
                       declarations()
                           .items(document_->storage().declarations.soas.functions
                                      [document_->storage().declarations.soas.index(*schema)])
                           .size()) {
            soa_function_index_ = 0;
        }
        soa_parameter_names_.clear();
        soa_parameter_types_.clear();
        soa_parameter_defaults_.clear();
        if (soa_function_index_.has_value()) {
            auto const function{declarations().items(
                document_->storage()
                    .declarations.soas.functions[document_->storage().declarations.soas.index(
                        *schema)])[*soa_function_index_]};
            auto const parameters{
                declarations().functions.parameters[declarations().functions.index(function)]};
            soa_parameter_names_.reserve(declarations().items(parameters).size());
            soa_parameter_types_.reserve(declarations().items(parameters).size());
            soa_parameter_defaults_.reserve(declarations().items(parameters).size());
            for (auto const parameter_id : declarations().items(parameters)) {
                auto const parameter{parameter_id};
                std::array<char, 128> name{};
                std::array<char, 128> type{};
                std::array<char, 128> default_value{};
                std::snprintf(
                    name.data(),
                    name.size(),
                    "%s",
                    text(declarations().parameters.name[declarations().parameters.index(parameter)])
                        .c_str());
                std::snprintf(
                    type.data(),
                    type.size(),
                    "%s",
                    text(declarations().type_refs.name[declarations().type_refs.index(
                             declarations()
                                 .parameters.type[declarations().parameters.index(parameter)])])
                        .c_str());
                std::snprintf(
                    default_value.data(),
                    default_value.size(),
                    "%s",
                    (declarations()
                             .parameters.default_value[declarations().parameters.index(parameter)]
                         ? text(*declarations()
                                     .parameters
                                     .default_value[declarations().parameters.index(parameter)])
                         : std::string{})
                        .c_str());
                soa_parameter_names_.push_back(name);
                soa_parameter_types_.push_back(type);
                soa_parameter_defaults_.push_back(default_value);
            }
            if (declarations().items(parameters).empty()) {
                soa_parameter_index_.reset();
            } else if (!soa_parameter_index_.has_value() ||
                       *soa_parameter_index_ >= declarations().items(parameters).size()) {
                soa_parameter_index_ = 0;
            }

            soa_function_body_lines_.clear();
            soa_function_body_lines_.reserve(
                declarations()
                    .items(declarations()
                               .functions.body_lines[declarations().functions.index(function)])
                    .size());
            for (auto const body_line : declarations().items(
                     declarations()
                         .functions.body_lines[declarations().functions.index(function)])) {
                soa_function_body_lines_.push_back(text(body_line));
            }
            if (declarations()
                    .items(declarations()
                               .functions.body_lines[declarations().functions.index(function)])
                    .empty()) {
                soa_function_body_index_.reset();
            } else if (!soa_function_body_index_.has_value() ||
                       *soa_function_body_index_ >=
                           declarations()
                               .items(declarations()
                                          .functions
                                          .body_lines[declarations().functions.index(function)])
                               .size()) {
                soa_function_body_index_ = 0;
            }

            soa_function_dependencies_.clear();
            soa_function_dependencies_.reserve(
                declarations()
                    .items(declarations()
                               .functions.dependencies[declarations().functions.index(function)])
                    .size());
            for (auto const dependency : declarations().items(
                     declarations()
                         .functions.dependencies[declarations().functions.index(function)])) {
                soa_function_dependencies_.push_back(text(dependency));
            }
            if (declarations()
                    .items(declarations()
                               .functions.dependencies[declarations().functions.index(function)])
                    .empty()) {
                soa_function_dependency_index_.reset();
            } else if (!soa_function_dependency_index_.has_value() ||
                       *soa_function_dependency_index_ >=
                           declarations()
                               .items(declarations()
                                          .functions
                                          .dependencies[declarations().functions.index(function)])
                               .size()) {
                soa_function_dependency_index_ = 0;
            }
            soa_function_trailing_return_type_ =
                declarations()
                        .functions.trailing_return_type[declarations().functions.index(function)]
                        .has_value()
                    ? text(declarations().type_refs.name[declarations().type_refs.index(
                          *declarations().functions.trailing_return_type
                               [declarations().functions.index(function)])])
                    : "";
            soa_function_template_parameters_ =
                (declarations()
                         .functions.template_parameters[declarations().functions.index(function)]
                     ? text(*declarations()
                                 .functions
                                 .template_parameters[declarations().functions.index(function)])
                     : std::string{});
            soa_function_requires_clause_ =
                (declarations().functions.requires_clause[declarations().functions.index(function)]
                     ? text(
                           *declarations().functions.requires_clause[declarations().functions.index(
                               function)])
                     : std::string{});
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
        std::snprintf(
            soa_single_allocation_name_.data(),
            soa_single_allocation_name_.size(),
            "%s",
            (document_->storage()
                     .declarations.soas
                     .single_allocation[document_->storage().declarations.soas.index(*schema)]
                 ? text(*document_->storage().declarations.soas.single_allocation
                             [document_->storage().declarations.soas.index(*schema)])
                 : std::string{})
                .c_str());
        soa_fixed_container_names_.clear();
        if (document_->storage()
                .declarations.soas.fixed[document_->storage().declarations.soas.index(*schema)]
                .has_value()) {
            std::snprintf(
                soa_fixed_storage_name_.data(),
                soa_fixed_storage_name_.size(),
                "%s",
                text(declarations().fixed_soas.storage_name[declarations().fixed_soas.index(
                         (*document_->storage().declarations.soas.fixed
                               [document_->storage().declarations.soas.index(*schema)]))])
                    .c_str());
            soa_fixed_container_names_.reserve(
                declarations()
                    .items(declarations().fixed_soas.containers[declarations().fixed_soas.index(
                        (*document_->storage().declarations.soas.fixed
                              [document_->storage().declarations.soas.index(*schema)]))])
                    .size());
            for (auto const container : declarations().items(
                     declarations().fixed_soas.containers[declarations().fixed_soas.index(
                         (*document_->storage().declarations.soas.fixed
                               [document_->storage().declarations.soas.index(*schema)]))])) {
                std::array<char, 128> name{};
                std::snprintf(name.data(), name.size(), "%s", text(container).c_str());
                soa_fixed_container_names_.push_back(name);
            }
            if (declarations()
                    .items(declarations().fixed_soas.containers[declarations().fixed_soas.index(
                        (*document_->storage().declarations.soas.fixed
                              [document_->storage().declarations.soas.index(*schema)]))])
                    .empty()) {
                soa_fixed_container_index_.reset();
            } else if (!soa_fixed_container_index_.has_value() ||
                       *soa_fixed_container_index_ >=
                           declarations()
                               .items(declarations()
                                          .fixed_soas.containers[declarations().fixed_soas.index(
                                              (*document_->storage().declarations.soas.fixed
                                                    [document_->storage().declarations.soas.index(
                                                        *schema)]))])
                               .size()) {
                soa_fixed_container_index_ = 0;
            }
        } else {
            soa_fixed_storage_name_.front() = '\0';
            soa_fixed_container_index_.reset();
        }
        if (selected !=
            declarations()
                .items(document_->storage()
                           .declarations.soas
                           .members[document_->storage().declarations.soas.index(*schema)])
                .end()) {
            std::snprintf(
                soa_member_name_.data(),
                soa_member_name_.size(),
                "%s",
                text(declarations().soa_members.name[declarations().soa_members.index(*selected)])
                    .c_str());
            std::snprintf(
                soa_member_type_.data(),
                soa_member_type_.size(),
                "%s",
                text(declarations().type_refs.name[declarations().type_refs.index(
                         declarations()
                             .soa_members.type[declarations().soa_members.index(*selected)])])
                    .c_str());
            std::snprintf(
                soa_member_fixed_schema_.data(),
                soa_member_fixed_schema_.size(),
                "%s",
                (declarations()
                         .soa_members.fixed_schema[declarations().soa_members.index(*selected)]
                     ? text(*declarations()
                                 .soa_members
                                 .fixed_schema[declarations().soa_members.index(*selected)])
                     : std::string{})
                    .c_str());
            std::snprintf(
                soa_member_nested_schema_.data(),
                soa_member_nested_schema_.size(),
                "%s",
                (declarations()
                         .soa_members.nested_schema[declarations().soa_members.index(*selected)]
                     ? text(*declarations()
                                 .soa_members
                                 .nested_schema[declarations().soa_members.index(*selected)])
                     : std::string{})
                    .c_str());
            std::snprintf(
                soa_relationship_target_.data(),
                soa_relationship_target_.size(),
                "%s",
                declarations()
                        .soa_members.relationship[declarations().soa_members.index(*selected)]
                        .has_value()
                    ? text(declarations().type_refs.name[declarations().type_refs.index(
                               declarations().semantic_relations.target
                                   [declarations().semantic_relations.index(
                                       (*declarations().soa_members.relationship
                                             [declarations().soa_members.index(*selected)]))])])
                          .c_str()
                    : "");
            auto const relationship_kind{
                declarations()
                        .soa_members.relationship[declarations().soa_members.index(*selected)]
                        .has_value()
                    ? std::ranges::find(
                          semantic_relationship_kinds,
                          declarations()
                              .semantic_relations.kind[declarations().semantic_relations.index(
                                  (*declarations().soa_members.relationship
                                        [declarations().soa_members.index(*selected)]))])
                    : semantic_relationship_kinds.begin()};
            soa_relationship_kind_ =
                relationship_kind == semantic_relationship_kinds.end()
                    ? 0
                    : static_cast<int>(relationship_kind - semantic_relationship_kinds.begin());
            auto const relationship_unit{
                declarations()
                            .soa_members.relationship[declarations().soa_members.index(*selected)]
                            .has_value() &&
                        declarations()
                            .semantic_relations
                            .unit[declarations().semantic_relations.index(
                                (*declarations().soa_members.relationship
                                      [declarations().soa_members.index(*selected)]))]
                            .has_value()
                    ? std::ranges::find(
                          semantic_relationship_units,
                          *declarations()
                               .semantic_relations.unit[declarations().semantic_relations.index(
                                   (*declarations().soa_members.relationship
                                         [declarations().soa_members.index(*selected)]))])
                    : semantic_relationship_units.begin()};
            soa_relationship_unit_ =
                relationship_unit == semantic_relationship_units.end()
                    ? 0
                    : static_cast<int>(relationship_unit - semantic_relationship_units.begin());
            soa_mask_dimension_names_.clear();
            soa_mask_dimension_extents_.clear();
            soa_mask_dimension_names_.reserve(
                declarations()
                    .items(
                        declarations().soa_members.mask_dimensions[declarations().soa_members.index(
                            *selected)])
                    .size());
            soa_mask_dimension_extents_.reserve(
                declarations()
                    .items(
                        declarations().soa_members.mask_dimensions[declarations().soa_members.index(
                            *selected)])
                    .size());
            for (auto const dimension_id : declarations().items(
                     declarations().soa_members.mask_dimensions[declarations().soa_members.index(
                         *selected)])) {
                auto const dimension{dimension_id};
                std::array<char, 128> name{};
                std::array<char, 128> extent{};
                std::snprintf(
                    name.data(),
                    name.size(),
                    "%s",
                    text(declarations()
                             .soa_mask_dimensions
                             .index_name[declarations().soa_mask_dimensions.index(dimension)])
                        .c_str());
                std::snprintf(extent.data(),
                              extent.size(),
                              "%s",
                              text(declarations()
                                       .soa_mask_dimensions
                                       .extent[declarations().soa_mask_dimensions.index(dimension)])
                                  .c_str());
                soa_mask_dimension_names_.push_back(name);
                soa_mask_dimension_extents_.push_back(extent);
            }
            if (declarations()
                    .items(
                        declarations().soa_members.mask_dimensions[declarations().soa_members.index(
                            *selected)])
                    .empty()) {
                soa_mask_dimension_index_.reset();
            } else if (!soa_mask_dimension_index_.has_value() ||
                       *soa_mask_dimension_index_ >=
                           declarations()
                               .items(declarations().soa_members.mask_dimensions
                                          [declarations().soa_members.index(*selected)])
                               .size()) {
                soa_mask_dimension_index_ = 0;
            }
        }
    }

    if (ImGui::Button("+ Column")) {
        auto replacement{document_->storage().declarations.soas.clone(*schema)};
        auto name{unique_child_name(
            declarations(),
            declarations().soa_members,
            declarations().soa_members.name,
            declarations().items(
                document_->storage()
                    .declarations.soas
                    .members[document_->storage().declarations.soas.index(replacement)]),
            "column")};
        document_->storage().declarations.soas.members[document_->storage().declarations.soas.index(
            replacement)] = [&] {
            auto const original_items{declarations().items(
                document_->storage()
                    .declarations.soas
                    .members[document_->storage().declarations.soas.index(replacement)])};
            auto rows{std::vector(original_items.begin(), original_items.end())};
            rows.push_back([&] {
                auto const new_soa_members{declarations().soa_members.append()};
                declarations().soa_members.name[declarations().soa_members.index(new_soa_members)] =
                    strings().intern(name);
                declarations().soa_members.kind[declarations().soa_members.index(new_soa_members)] =
                    lispb::codegen::SoaMemberKind::array;
                declarations()
                    .soa_members.type[declarations().soa_members.index(new_soa_members)] = [&] {
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
    ImGui::BeginDisabled(!selected_index.has_value() || selected_is_mask_storage);
    if (ImGui::Button("Duplicate")) {
        auto replacement{document_->storage().declarations.soas.clone(*schema)};
        auto copy{declarations().soa_members.clone(declarations().items(
            document_->storage()
                .declarations.soas.members[document_->storage().declarations.soas.index(
                    replacement)])[*selected_index])};
        declarations().soa_members.name[declarations().soa_members.index(copy)] =
            strings().intern(unique_child_name(
                declarations(),
                declarations().soa_members,
                declarations().soa_members.name,
                declarations().items(
                    document_->storage()
                        .declarations.soas
                        .members[document_->storage().declarations.soas.index(replacement)]),
                text(declarations().soa_members.name[declarations().soa_members.index(copy)]) +
                    "_copy"));
        document_->storage()
            .declarations.soas.members[document_->storage().declarations.soas.index(replacement)] =
            [&] {
                auto const original_items{declarations().items(
                    document_->storage()
                        .declarations.soas
                        .members[document_->storage().declarations.soas.index(replacement)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.insert(rows.begin() + static_cast<std::ptrdiff_t>(*selected_index + 1), copy);
                return declarations().range(rows);
            }();
        if (apply_document_edit(
                document_->replace_declaration(*declaration, std::move(replacement)))) {
            analysis_session_.inputs.selection.field =
                text(declarations().soa_members.name[declarations().soa_members.index(copy)]);
            return true;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() || *selected_index == 0);
    if (ImGui::Button("Move up")) {
        auto replacement{document_->storage().declarations.soas.clone(*schema)};
        document_->storage()
            .declarations.soas.members[document_->storage().declarations.soas.index(replacement)] =
            [&] {
                auto const original_items{declarations().items(
                    document_->storage()
                        .declarations.soas
                        .members[document_->storage().declarations.soas.index(replacement)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                std::swap(rows[*selected_index], rows[*selected_index - 1]);
                return declarations().range(rows);
            }();
        if (apply_document_edit(
                document_->replace_declaration(*declaration, std::move(replacement)))) {
            analysis_session_.inputs.selection.field = soa_editor_member_;
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
                           .declarations.soas
                           .members[document_->storage().declarations.soas.index(*schema)])
                .size());
    if (ImGui::Button("Move down")) {
        auto replacement{document_->storage().declarations.soas.clone(*schema)};
        document_->storage()
            .declarations.soas.members[document_->storage().declarations.soas.index(replacement)] =
            [&] {
                auto const original_items{declarations().items(
                    document_->storage()
                        .declarations.soas
                        .members[document_->storage().declarations.soas.index(replacement)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                std::swap(rows[*selected_index], rows[*selected_index + 1]);
                return declarations().range(rows);
            }();
        if (apply_document_edit(
                document_->replace_declaration(*declaration, std::move(replacement)))) {
            analysis_session_.inputs.selection.field = soa_editor_member_;
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(
        !selected_index.has_value() ||
        declarations()
                .items(document_->storage()
                           .declarations.soas
                           .members[document_->storage().declarations.soas.index(*schema)])
                .size() == 1 ||
        selected_is_mask_storage || selected_is_final_mask_field);
    if (ImGui::Button("Delete")) {
        auto replacement{document_->storage().declarations.soas.clone(*schema)};
        auto const deleted_name{text(
            declarations().soa_members.name[declarations().soa_members.index(declarations().items(
                document_->storage()
                    .declarations.soas.members[document_->storage().declarations.soas.index(
                        replacement)])[*selected_index])])};
        document_->storage()
            .declarations.soas.members[document_->storage().declarations.soas.index(replacement)] =
            [&] {
                auto const original_items{declarations().items(
                    document_->storage()
                        .declarations.soas
                        .members[document_->storage().declarations.soas.index(replacement)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.erase(rows.begin() + static_cast<std::ptrdiff_t>(*selected_index));
                return declarations().range(rows);
            }();
        auto const next_index{std::min(
            *selected_index,
            declarations()
                    .items(document_->storage()
                               .declarations.soas
                               .members[document_->storage().declarations.soas.index(replacement)])
                    .size() -
                1)};
        auto const next_name{text(
            declarations().soa_members.name[declarations().soa_members.index(declarations().items(
                document_->storage().declarations.soas.members
                    [document_->storage().declarations.soas.index(replacement)])[next_index])])};
        if (apply_document_edit(
                document_->replace_declaration(*declaration, std::move(replacement)))) {
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
        for (auto const member_id : declarations().items(
                 document_->storage()
                     .declarations.soas
                     .members[document_->storage().declarations.soas.index(*schema)])) {
            auto const member{member_id};
            analysis_session_.inputs.selection.soa_access_columns.insert_or_assign(
                text(declarations().soa_members.name[declarations().soa_members.index(member)]),
                analysis_session_.inputs.access_operation);
        }
        analysis_session_.inputs.selection.soa_access_set_explicit = true;
    }

    std::optional<lispb::codegen::DeclarationHandle> pending;
    std::optional<TypeId> navigate_to;
    std::optional<std::pair<std::string, std::string>> renamed_member;
    auto selected_after_edit{analysis_session_.inputs.selection.field};
    if (detail::begin_editable_table(
            "soa-schema-members",
            6,
            declarations()
                .items(document_->storage()
                           .declarations.soas
                           .members[document_->storage().declarations.soas.index(*schema)])
                .size())) {
        detail::editable_table_column("", ImGuiTableColumnFlags_WidthFixed);
        detail::editable_table_column("Access", ImGuiTableColumnFlags_WidthFixed);
        detail::editable_table_column("Operation", ImGuiTableColumnFlags_WidthFixed);
        detail::editable_table_column("Name");
        detail::editable_table_column("Semantic type");
        detail::editable_table_column("Kind");
        ImGui::TableHeadersRow();
        for (std::size_t index{};
             index < declarations()
                         .items(document_->storage()
                                    .declarations.soas
                                    .members[document_->storage().declarations.soas.index(*schema)])
                         .size();
             ++index) {
            auto const member{declarations().items(
                document_->storage()
                    .declarations.soas
                    .members[document_->storage().declarations.soas.index(*schema)])[index]};
            auto const row_selected{
                analysis_session_.inputs.selection.field ==
                text(declarations().soa_members.name[declarations().soa_members.index(member)])};
            ImGui::PushID(static_cast<int>(index));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (detail::editable_table_row_handle(row_selected)) {
                analysis_session_.inputs.selection.field =
                    text(declarations().soa_members.name[declarations().soa_members.index(member)]);
                soa_editor_declaration_.reset();
            }
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("SOA_MEMBER_ROW", &index, sizeof(index));
                ImGui::Text(
                    "Move %s",
                    text(declarations().soa_members.name[declarations().soa_members.index(member)])
                        .c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget()) {
                if (auto const* payload{ImGui::AcceptDragDropPayload("SOA_MEMBER_ROW")}) {
                    auto const source_index{*static_cast<std::size_t const*>(payload->Data)};
                    if (source_index <
                            declarations()
                                .items(document_->storage().declarations.soas.members
                                           [document_->storage().declarations.soas.index(*schema)])
                                .size() &&
                        source_index != index) {
                        pending = declarations().soas.clone(*schema);
                        selected_after_edit =
                            text(declarations().soa_members.name[declarations().soa_members.index(
                                declarations().items(
                                    declarations().soas.members[declarations().soas.index(
                                        *pending)])[source_index])]);
                        declarations().soas.members[declarations().soas.index(*pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations().soas.members[declarations().soas.index(*pending)])};
                            auto rows{std::vector(original_items.begin(), original_items.end())};
                            move_element(rows, source_index, index);
                            return declarations().range(rows);
                        }();
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::TableNextColumn();
            auto accessed{
                analysis_session_.inputs.selection.soa_access_set_explicit
                    ? analysis_session_.inputs.selection.soa_access_columns.contains(
                          text(declarations()
                                   .soa_members.name[declarations().soa_members.index(member)]))
                    : analysis_session_.inputs.selection.field ==
                          text(declarations()
                                   .soa_members.name[declarations().soa_members.index(member)])};
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
                        text(declarations()
                                 .soa_members.name[declarations().soa_members.index(member)]),
                        analysis_session_.inputs.access_operation);
                } else {
                    analysis_session_.inputs.selection.soa_access_columns.erase(text(
                        declarations().soa_members.name[declarations().soa_members.index(member)]));
                }
            }

            ImGui::TableNextColumn();
            if (accessed) {
                auto operation{analysis_session_.inputs.access_operation};
                if (analysis_session_.inputs.selection.soa_access_set_explicit) {
                    if (auto const found{analysis_session_.inputs.selection.soa_access_columns.find(
                            text(declarations()
                                     .soa_members.name[declarations().soa_members.index(member)]))};
                        found != analysis_session_.inputs.selection.soa_access_columns.end()) {
                        operation = found->second;
                    }
                }
                auto operation_index{static_cast<int>(operation)};
                ImGui::SetNextItemWidth(105.0F);
                if (ImGui::Combo("##access-operation", &operation_index, "R\0W\0RW\0")) {
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
                        text(declarations()
                                 .soa_members.name[declarations().soa_members.index(member)]),
                        static_cast<AccessOperation>(operation_index));
                }
            } else {
                ImGui::TextDisabled("-");
            }

            ImGui::TableNextColumn();
            auto const row_mask_storage{
                document_->storage()
                    .declarations.soas
                    .field_mask_name[document_->storage().declarations.soas.index(*schema)]
                    .has_value() &&
                declarations().type_refs.name[declarations().type_refs.index(
                    declarations().soa_members.type[declarations().soa_members.index(member)])] ==
                    *document_->storage()
                         .declarations.soas
                         .field_mask_name[document_->storage().declarations.soas.index(*schema)]};
            if (row_selected) {
                ImGui::SetNextItemWidth(-1.0F);
                auto const submitted{ImGui::InputText("##name",
                                                      soa_member_name_.data(),
                                                      soa_member_name_.size(),
                                                      ImGuiInputTextFlags_EnterReturnsTrue)};
                if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                    pending = declarations().soas.clone(*schema);
                    declarations().soas.members[declarations().soas.index(*pending)] = [&] {
                        auto const original_items{declarations().items(
                            declarations().soas.members[declarations().soas.index(*pending)])};
                        auto rows_1{std::vector(original_items.begin(), original_items.end())};
                        rows_1[index] = [&] {
                            auto row_0{declarations().soa_members.clone(rows_1[index])};
                            declarations()
                                .soa_members.name[declarations().soa_members.index(row_0)] =
                                strings().intern(soa_member_name_.data());
                            return row_0;
                        }();
                        return declarations().range(rows_1);
                    }();
                    selected_after_edit = text(
                        declarations()
                            .soa_members.name[declarations().soa_members.index(declarations().items(
                                declarations()
                                    .soas.members[declarations().soas.index(*pending)])[index])]);
                    renamed_member = std::pair{
                        text(declarations()
                                 .soa_members.name[declarations().soa_members.index(member)]),
                        selected_after_edit};
                }
            } else {
                ImGui::TextUnformatted(
                    text(declarations().soa_members.name[declarations().soa_members.index(member)])
                        .c_str());
            }

            ImGui::TableNextColumn();
            if (row_selected && !row_mask_storage) {
                auto const type_controls_inline{detail::prepare_editable_type_input()};
                auto const submitted{ImGui::InputText("##type",
                                                      soa_member_type_.data(),
                                                      soa_member_type_.size(),
                                                      ImGuiInputTextFlags_EnterReturnsTrue)};
                if (!pending.has_value() && (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                    pending = declarations().soas.clone(*schema);
                    declarations().soas.members[declarations().soas.index(*pending)] = [&] {
                        auto const original_items{declarations().items(
                            declarations().soas.members[declarations().soas.index(*pending)])};
                        auto rows_2{std::vector(original_items.begin(), original_items.end())};
                        rows_2[index] = [&] {
                            auto row_1{declarations().soa_members.clone(rows_2[index])};
                            declarations()
                                .soa_members.type[declarations().soa_members.index(row_1)] = [&] {
                                auto row_0{declarations().type_refs.clone(
                                    declarations()
                                        .soa_members
                                        .type[declarations().soa_members.index(row_1)])};
                                declarations()
                                    .type_refs.name[declarations().type_refs.index(row_0)] =
                                    strings().intern(soa_member_type_.data());
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
                    pending = declarations().soas.clone(*schema);
                    declarations().soas.members[declarations().soas.index(*pending)] = [&] {
                        auto const original_items{declarations().items(
                            declarations().soas.members[declarations().soas.index(*pending)])};
                        auto rows_2{std::vector(original_items.begin(), original_items.end())};
                        rows_2[index] = [&] {
                            auto row_1{declarations().soa_members.clone(rows_2[index])};
                            declarations()
                                .soa_members.type[declarations().soa_members.index(row_1)] = [&] {
                                auto row_0{declarations().type_refs.clone(
                                    declarations()
                                        .soa_members
                                        .type[declarations().soa_members.index(row_1)])};
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
                if (index < definitions().soas.columns_of(soa).size() &&
                    detail::semantic_type_navigation_button()) {
                    navigate_to =
                        analysis_session_.inputs.workspace.types()
                            .occurrences()
                            .type[analysis_session_.inputs.workspace.types().occurrences().index(
                                definitions()
                                    .soa_columns.semantic_type[definitions().soa_columns.index(
                                        definitions().soas.columns_of(soa)[index])])];
                }
                detail::editable_table_content_hint(soa_member_type_.data(), 80.0F);
            } else {
                ImGui::TextUnformatted(
                    text(declarations().type_refs.name[declarations().type_refs.index(
                             declarations()
                                 .soa_members.type[declarations().soa_members.index(member)])])
                        .c_str());
            }

            ImGui::TableNextColumn();
            auto const kind_label{
                declarations().soa_members.kind[declarations().soa_members.index(member)] ==
                        lispb::codegen::SoaMemberKind::nested
                    ? "nested"
                    : "array"};
            if (row_selected && ImGui::BeginCombo("##kind", kind_label)) {
                if (ImGui::Selectable(
                        "array",
                        declarations().soa_members.kind[declarations().soa_members.index(member)] ==
                            lispb::codegen::SoaMemberKind::array)) {
                    pending = declarations().soas.clone(*schema);
                    declarations().soas.members[declarations().soas.index(*pending)] = [&] {
                        auto const original_items{declarations().items(
                            declarations().soas.members[declarations().soas.index(*pending)])};
                        auto rows_1{std::vector(original_items.begin(), original_items.end())};
                        rows_1[index] = [&] {
                            auto row_0{declarations().soa_members.clone(rows_1[index])};
                            declarations()
                                .soa_members.kind[declarations().soa_members.index(row_0)] =
                                lispb::codegen::SoaMemberKind::array;
                            return row_0;
                        }();
                        return declarations().range(rows_1);
                    }();
                    declarations().soas.members[declarations().soas.index(*pending)] = [&] {
                        auto const original_items{declarations().items(
                            declarations().soas.members[declarations().soas.index(*pending)])};
                        auto rows_1{std::vector(original_items.begin(), original_items.end())};
                        rows_1[index] = [&] {
                            auto row_0{declarations().soa_members.clone(rows_1[index])};
                            declarations()
                                .soa_members.fixed_schema[declarations().soa_members.index(row_0)]
                                .reset();
                            return row_0;
                        }();
                        return declarations().range(rows_1);
                    }();
                    declarations().soas.members[declarations().soas.index(*pending)] = [&] {
                        auto const original_items{declarations().items(
                            declarations().soas.members[declarations().soas.index(*pending)])};
                        auto rows_1{std::vector(original_items.begin(), original_items.end())};
                        rows_1[index] = [&] {
                            auto row_0{declarations().soa_members.clone(rows_1[index])};
                            declarations()
                                .soa_members.nested_schema[declarations().soa_members.index(row_0)]
                                .reset();
                            return row_0;
                        }();
                        return declarations().range(rows_1);
                    }();
                    soa_member_fixed_schema_.front() = '\0';
                    soa_member_nested_schema_.front() = '\0';
                }
                auto const mask_storage{
                    document_->storage()
                        .declarations.soas
                        .field_mask_name[document_->storage().declarations.soas.index(*schema)]
                        .has_value() &&
                    declarations().type_refs.name[declarations().type_refs.index(
                        declarations()
                            .soa_members.type[declarations().soa_members.index(member)])] ==
                        *document_->storage().declarations.soas.field_mask_name
                             [document_->storage().declarations.soas.index(*schema)]};
                ImGui::BeginDisabled(
                    declarations()
                        .soa_members.mask_field[declarations().soa_members.index(member)] ||
                    mask_storage);
                if (ImGui::Selectable(
                        "nested",
                        declarations().soa_members.kind[declarations().soa_members.index(member)] ==
                            lispb::codegen::SoaMemberKind::nested)) {
                    pending = declarations().soas.clone(*schema);
                    declarations().soas.members[declarations().soas.index(*pending)] = [&] {
                        auto const original_items{declarations().items(
                            declarations().soas.members[declarations().soas.index(*pending)])};
                        auto rows_1{std::vector(original_items.begin(), original_items.end())};
                        rows_1[index] = [&] {
                            auto row_0{declarations().soa_members.clone(rows_1[index])};
                            declarations()
                                .soa_members.kind[declarations().soa_members.index(row_0)] =
                                lispb::codegen::SoaMemberKind::nested;
                            return row_0;
                        }();
                        return declarations().range(rows_1);
                    }();
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

    if (selected_index.has_value() && detail::section("Selected column details")) {
        auto const member{declarations().items(
            document_->storage()
                .declarations.soas
                .members[document_->storage().declarations.soas.index(*schema)])[*selected_index]};
        auto const resolved_column{
            *selected_index < definitions().soas.columns_of(soa).size()
                ? std::optional{definitions().soas.columns_of(soa)[*selected_index]}
                : std::nullopt};
        if (declarations().soa_members.kind[declarations().soa_members.index(member)] ==
            lispb::codegen::SoaMemberKind::nested) {
            detail::prepare_property_input("Fixed schema (optional)");
            auto const fixed_submitted{ImGui::InputText("##Fixed schema (optional)",
                                                        soa_member_fixed_schema_.data(),
                                                        soa_member_fixed_schema_.size(),
                                                        ImGuiInputTextFlags_EnterReturnsTrue)};
            if (fixed_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                if (!pending.has_value()) {
                    pending = declarations().soas.clone(*schema);
                }
                auto value{std::string{soa_member_fixed_schema_.data()}};
                if (value.empty()) {
                    declarations().soas.members[declarations().soas.index(*pending)] = [&] {
                        auto const original_items{declarations().items(
                            declarations().soas.members[declarations().soas.index(*pending)])};
                        auto rows_1{std::vector(original_items.begin(), original_items.end())};
                        rows_1[*selected_index] = [&] {
                            auto row_0{declarations().soa_members.clone(rows_1[*selected_index])};
                            declarations()
                                .soa_members.fixed_schema[declarations().soa_members.index(row_0)]
                                .reset();
                            return row_0;
                        }();
                        return declarations().range(rows_1);
                    }();
                } else {
                    declarations().soas.members[declarations().soas.index(*pending)] = [&] {
                        auto const original_items{declarations().items(
                            declarations().soas.members[declarations().soas.index(*pending)])};
                        auto rows_1{std::vector(original_items.begin(), original_items.end())};
                        rows_1[*selected_index] = [&] {
                            auto row_0{declarations().soa_members.clone(rows_1[*selected_index])};
                            declarations()
                                .soa_members.fixed_schema[declarations().soa_members.index(row_0)] =
                                strings().intern(value);
                            return row_0;
                        }();
                        return declarations().range(rows_1);
                    }();
                }
            }

            detail::prepare_property_input("Nested schema (optional)");
            auto const nested_submitted{ImGui::InputText("##Nested schema (optional)",
                                                         soa_member_nested_schema_.data(),
                                                         soa_member_nested_schema_.size(),
                                                         ImGuiInputTextFlags_EnterReturnsTrue)};
            if (nested_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                if (!pending.has_value()) {
                    pending = declarations().soas.clone(*schema);
                }
                auto value{std::string{soa_member_nested_schema_.data()}};
                if (value.empty()) {
                    declarations().soas.members[declarations().soas.index(*pending)] = [&] {
                        auto const original_items{declarations().items(
                            declarations().soas.members[declarations().soas.index(*pending)])};
                        auto rows_1{std::vector(original_items.begin(), original_items.end())};
                        rows_1[*selected_index] = [&] {
                            auto row_0{declarations().soa_members.clone(rows_1[*selected_index])};
                            declarations()
                                .soa_members.nested_schema[declarations().soa_members.index(row_0)]
                                .reset();
                            return row_0;
                        }();
                        return declarations().range(rows_1);
                    }();
                } else {
                    declarations().soas.members[declarations().soas.index(*pending)] = [&] {
                        auto const original_items{declarations().items(
                            declarations().soas.members[declarations().soas.index(*pending)])};
                        auto rows_1{std::vector(original_items.begin(), original_items.end())};
                        rows_1[*selected_index] = [&] {
                            auto row_0{declarations().soa_members.clone(rows_1[*selected_index])};
                            declarations()
                                .soa_members
                                .nested_schema[declarations().soa_members.index(row_0)] =
                                strings().intern(value);
                            return row_0;
                        }();
                        return declarations().range(rows_1);
                    }();
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
                auto const record_module{
                    ((modules[module_index]).kind() == lispb::codegen::ModuleKind::normal
                         ? std::optional{modules[module_index]}
                         : std::nullopt)};
                if (!record_module.has_value()) {
                    continue;
                }
                if (!fallback_record_module_index.has_value()) {
                    fallback_record_module_index = module_index;
                }
                if (declarations()
                        .module_settings_store
                        .namespace_name[declarations().module_settings_store.index(
                            declarations().normal_modules.settings
                                [declarations().normal_modules.index(*record_module)])]
                        .value_or(lispb::IdentifierId{}) ==
                    analysis_session_.inputs.workspace.types().namespace_name(node)) {
                    record_module_index = module_index;
                    break;
                }
            }
            if (!record_module_index.has_value()) {
                record_module_index = fallback_record_module_index;
            }
            auto const mask_storage{
                document_->storage()
                    .declarations.soas
                    .field_mask_name[document_->storage().declarations.soas.index(*schema)]
                    .has_value() &&
                declarations().type_refs.name[declarations().type_refs.index(
                    declarations().soa_members.type[declarations().soa_members.index(member)])] ==
                    *document_->storage()
                         .declarations.soas
                         .field_mask_name[document_->storage().declarations.soas.index(*schema)]};
            auto const can_create_record{
                record_module_index.has_value() && resolved_column.has_value() && !mask_storage &&
                declarations().soa_members.kind[declarations().soa_members.index(member)] ==
                    lispb::codegen::SoaMemberKind::array};
            ImGui::BeginDisabled(!can_create_record);
            if (ImGui::Button("Create record for selected column...")) {
                new_record_module_index_ = *record_module_index;
                auto const suggested_name{suggested_type_name(
                    text(declarations().soa_members.name[declarations().soa_members.index(member)]),
                    "Record")};

                auto unique_name{suggested_name};
                auto suffix_number{std::size_t{1}};
                while (
                    std::ranges::find(
                        declarations().items(
                            declarations()
                                .normal_modules.declarations[declarations().normal_modules.index(
                                    modules[new_record_module_index_])]),
                        strings().intern(unique_name),
                        [&](auto id) {
                            return lispb::codegen::declaration_name(declarations(), id);
                        }) !=
                    declarations()
                        .items(declarations()
                                   .normal_modules.declarations[declarations().normal_modules.index(
                                       modules[new_record_module_index_])])
                        .end()) {
                    unique_name = suggested_name + std::to_string(suffix_number++);
                }
                std::snprintf(
                    new_record_name_.data(), new_record_name_.size(), "%s", unique_name.c_str());
                auto first_member_type{declarations().type_refs.name[declarations().type_refs.index(
                    declarations().soa_members.type[declarations().soa_members.index(member)])]};
                auto const& current_type{
                    analysis_session_.inputs.workspace.types()
                        .occurrences()
                        .type[analysis_session_.inputs.workspace.types().occurrences().index(
                            definitions().soa_columns.semantic_type[definitions().soa_columns.index(
                                *resolved_column)])]};
                if (!text(first_member_type).starts_with('@') &&
                    analysis_session_.inputs.workspace.types().origin(current_type) ==
                        TypeOrigin::declaration) {
                    first_member_type = strings().intern(
                        analysis_session_.inputs.workspace.types().cpp_spelling(current_type));
                }
                std::snprintf(new_record_member_type_.data(),
                              new_record_member_type_.size(),
                              "%s",
                              text(first_member_type).c_str());
                pending_soa_record_binding_ = PendingSoaRecordBinding{
                    .soa_declaration = *declaration,
                    .column_name =
                        declarations().soa_members.name[declarations().soa_members.index(member)]};
                module_initiated_dialog_ = false;
                open_new_record_dialog_ = true;
            }
            ImGui::EndDisabled();
            if (!record_module_index.has_value()) {
                ImGui::SameLine();
                ImGui::TextDisabled("no ordinary module is available.");
            } else if (!resolved_column) {
                ImGui::SameLine();
                ImGui::TextDisabled("The selected column type is unresolved.");
            } else if (mask_storage ||
                       declarations().soa_members.kind[declarations().soa_members.index(member)] !=
                           lispb::codegen::SoaMemberKind::array) {
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
            if (section_with_tooltip(
                    "Selected column relationship",
                    "Describe how this SoA column relates to another semantic type.")) {
                ImGui::SetNextItemWidth(180.0F);
                if (ImGui::BeginCombo(
                        "Kind##soa-column-relationship",
                        lispb::codegen::semantic_relation_kind_name(current_kind).data())) {
                    for (std::size_t kind_index{}; kind_index < semantic_relationship_kinds.size();
                         ++kind_index) {
                        auto const kind{semantic_relationship_kinds[kind_index]};
                        auto const chosen{soa_relationship_kind_ == static_cast<int>(kind_index)};
                        if (ImGui::Selectable(
                                lispb::codegen::semantic_relation_kind_name(kind).data(), chosen)) {
                            soa_relationship_kind_ = static_cast<int>(kind_index);
                            if (declarations()
                                    .soa_members
                                    .relationship[declarations().soa_members.index(member)]
                                    .has_value()) {
                                auto replacement{
                                    document_->storage().declarations.soas.clone(*schema)};
                                document_->storage()
                                    .declarations.soas
                                    .members[document_->storage().declarations.soas.index(
                                        replacement)] = [&] {
                                    auto const original_items{declarations().items(
                                        document_->storage()
                                            .declarations.soas
                                            .members[document_->storage().declarations.soas.index(
                                                replacement)])};
                                    auto rows_2{
                                        std::vector(original_items.begin(), original_items.end())};
                                    rows_2[*selected_index] = [&] {
                                        auto row_1{declarations().soa_members.clone(
                                            rows_2[*selected_index])};
                                        (*declarations().soa_members.relationship
                                              [declarations().soa_members.index(row_1)]) = [&] {
                                            auto row_0{declarations().semantic_relations.clone(
                                                (*declarations().soa_members.relationship
                                                      [declarations().soa_members.index(row_1)]))};
                                            declarations().semantic_relations.kind
                                                [declarations().semantic_relations.index(row_0)] =
                                                kind;
                                            return row_0;
                                        }();
                                        return row_1;
                                    }();
                                    return declarations().range(rows_2);
                                }();
                                document_->storage()
                                    .declarations.soas
                                    .members[document_->storage().declarations.soas.index(
                                        replacement)] = [&] {
                                    auto const original_items{declarations().items(
                                        document_->storage()
                                            .declarations.soas
                                            .members[document_->storage().declarations.soas.index(
                                                replacement)])};
                                    auto rows_2{
                                        std::vector(original_items.begin(), original_items.end())};
                                    rows_2[*selected_index] = [&] {
                                        auto row_1{declarations().soa_members.clone(
                                            rows_2[*selected_index])};
                                        (*declarations().soa_members.relationship
                                              [declarations().soa_members.index(row_1)]) = [&] {
                                            auto row_0{declarations().semantic_relations.clone(
                                                (*declarations().soa_members.relationship
                                                      [declarations().soa_members.index(row_1)]))};
                                            declarations().semantic_relations.unit
                                                [declarations().semantic_relations.index(row_0)] =
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
                                    soa_editor_declaration_.reset();
                                    analysis_session_.inputs.selection.field =
                                        text(declarations()
                                                 .soa_members
                                                 .name[declarations().soa_members.index(member)]);
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
                            "Unit##soa-column-relationship",
                            lispb::codegen::semantic_relation_unit_name(current_unit).data())) {
                        for (std::size_t unit_index{};
                             unit_index < semantic_relationship_units.size();
                             ++unit_index) {
                            auto const unit{semantic_relationship_units[unit_index]};
                            auto const chosen{soa_relationship_unit_ ==
                                              static_cast<int>(unit_index)};
                            if (ImGui::Selectable(
                                    lispb::codegen::semantic_relation_unit_name(unit).data(),
                                    chosen)) {
                                soa_relationship_unit_ = static_cast<int>(unit_index);
                                if (declarations()
                                        .soa_members
                                        .relationship[declarations().soa_members.index(member)]
                                        .has_value()) {
                                    auto replacement{
                                        document_->storage().declarations.soas.clone(*schema)};
                                    document_->storage()
                                        .declarations.soas
                                        .members[document_->storage().declarations.soas.index(
                                            replacement)] = [&] {
                                        auto const original_items{declarations().items(
                                            document_->storage().declarations.soas.members
                                                [document_->storage().declarations.soas.index(
                                                    replacement)])};
                                        auto rows_2{std::vector(original_items.begin(),
                                                                original_items.end())};
                                        rows_2[*selected_index] = [&] {
                                            auto row_1{declarations().soa_members.clone(
                                                rows_2[*selected_index])};
                                            (*declarations().soa_members.relationship
                                                  [declarations().soa_members.index(row_1)]) = [&] {
                                                auto row_0{declarations().semantic_relations.clone(
                                                    (*declarations().soa_members.relationship
                                                          [declarations().soa_members.index(
                                                              row_1)]))};
                                                declarations()
                                                    .semantic_relations
                                                    .unit[declarations().semantic_relations.index(
                                                        row_0)] = unit;
                                                return row_0;
                                            }();
                                            return row_1;
                                        }();
                                        return declarations().range(rows_2);
                                    }();
                                    if (apply_document_edit(document_->replace_declaration(
                                            *declaration, std::move(replacement)))) {
                                        soa_editor_declaration_.reset();
                                        analysis_session_.inputs.selection.field = text(
                                            declarations()
                                                .soa_members
                                                .name[declarations().soa_members.index(member)]);
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
                if (declarations()
                        .soa_members.relationship[declarations().soa_members.index(member)]
                        .has_value() &&
                    (target_submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                    if (soa_relationship_target_.front() == '\0') {
                        schema_edit_message_ = "Relationship target cannot be empty.";
                    } else {
                        auto replacement{document_->storage().declarations.soas.clone(*schema)};
                        document_->storage()
                            .declarations.soas.members[document_->storage().declarations.soas.index(
                                replacement)] = [&] {
                            auto const original_items{declarations().items(
                                document_->storage().declarations.soas.members
                                    [document_->storage().declarations.soas.index(replacement)])};
                            auto rows_3{std::vector(original_items.begin(), original_items.end())};
                            rows_3[*selected_index] = [&] {
                                auto row_2{
                                    declarations().soa_members.clone(rows_3[*selected_index])};
                                (*declarations()
                                      .soa_members.relationship[declarations().soa_members.index(
                                          row_2)]) = [&] {
                                    auto row_1{declarations().semantic_relations.clone(
                                        (*declarations().soa_members.relationship
                                              [declarations().soa_members.index(row_2)]))};
                                    declarations().semantic_relations.target
                                        [declarations().semantic_relations.index(row_1)] = [&] {
                                        auto row_0{declarations().type_refs.clone(
                                            declarations().semantic_relations.target
                                                [declarations().semantic_relations.index(row_1)])};
                                        declarations()
                                            .type_refs.name[declarations().type_refs.index(row_0)] =
                                            strings().intern(soa_relationship_target_.data());
                                        return row_0;
                                    }();
                                    return row_1;
                                }();
                                return row_2;
                            }();
                            return declarations().range(rows_3);
                        }();
                        if (apply_document_edit(document_->replace_declaration(
                                *declaration, std::move(replacement)))) {
                            soa_editor_declaration_.reset();
                            analysis_session_.inputs.selection.field = text(
                                declarations()
                                    .soa_members.name[declarations().soa_members.index(member)]);
                            return true;
                        }
                    }
                }
                ImGui::SameLine();
                ImGui::PushID("soa-column-relationship-target");
                auto picked_relationship_target{
                    draw_type_picker(analysis_session_.inputs.workspace.types().module_name(node),
                                     analysis_session_.inputs.workspace.types().identity(node))};
                ImGui::PopID();
                if (picked_relationship_target.has_value()) {
                    auto replacement{document_->storage().declarations.soas.clone(*schema)};
                    document_->storage()
                        .declarations.soas
                        .members[document_->storage().declarations.soas.index(replacement)] = [&] {
                        auto const original_items{declarations().items(
                            document_->storage().declarations.soas.members
                                [document_->storage().declarations.soas.index(replacement)])};
                        auto rows_1{std::vector(original_items.begin(), original_items.end())};
                        rows_1[*selected_index] = [&] {
                            auto row_0{declarations().soa_members.clone(rows_1[*selected_index])};
                            declarations()
                                .soa_members
                                .relationship[declarations().soa_members.index(row_0)] = [&] {
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
                                        strings().intern(*picked_relationship_target);
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
                        soa_editor_declaration_.reset();
                        analysis_session_.inputs.selection.field =
                            text(declarations()
                                     .soa_members.name[declarations().soa_members.index(member)]);
                        return true;
                    }
                }
                ImGui::SameLine();
                if (declarations()
                        .soa_members.relationship[declarations().soa_members.index(member)]
                        .has_value()) {
                    if (ImGui::SmallButton("Clear##soa-column-relationship")) {
                        auto replacement{document_->storage().declarations.soas.clone(*schema)};
                        document_->storage()
                            .declarations.soas.members[document_->storage().declarations.soas.index(
                                replacement)] = [&] {
                            auto const original_items{declarations().items(
                                document_->storage().declarations.soas.members
                                    [document_->storage().declarations.soas.index(replacement)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[*selected_index] = [&] {
                                auto row_0{
                                    declarations().soa_members.clone(rows_1[*selected_index])};
                                declarations()
                                    .soa_members
                                    .relationship[declarations().soa_members.index(row_0)]
                                    .reset();
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        if (apply_document_edit(document_->replace_declaration(
                                *declaration, std::move(replacement)))) {
                            soa_editor_declaration_.reset();
                            analysis_session_.inputs.selection.field = text(
                                declarations()
                                    .soa_members.name[declarations().soa_members.index(member)]);
                            return true;
                        }
                    }
                    ImGui::SameLine();
                    if (resolved_column.has_value() &&
                        lispb::schema::is_valid(
                            definitions().soa_columns.relationship[definitions().soa_columns.index(
                                *resolved_column)]) &&
                        ImGui::SmallButton(">##soa-column-relationship")) {
                        navigate_to =
                            analysis_session_.inputs.workspace.types().occurrences().type
                                [analysis_session_.inputs.workspace.types().occurrences().index(
                                    analysis_session_.inputs.workspace.types()
                                        .relationships()
                                        .target[analysis_session_.inputs.workspace.types()
                                                    .relationships()
                                                    .index(definitions().soa_columns.relationship
                                                               [definitions().soa_columns.index(
                                                                   *resolved_column)])])];
                    }
                } else {
                    ImGui::BeginDisabled(soa_relationship_target_.front() == '\0');
                    if (ImGui::SmallButton("Add##soa-column-relationship")) {
                        auto replacement{document_->storage().declarations.soas.clone(*schema)};
                        document_->storage()
                            .declarations.soas.members[document_->storage().declarations.soas.index(
                                replacement)] = [&] {
                            auto const original_items{declarations().items(
                                document_->storage().declarations.soas.members
                                    [document_->storage().declarations.soas.index(replacement)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[*selected_index] = [&] {
                                auto row_0{
                                    declarations().soa_members.clone(rows_1[*selected_index])};
                                declarations()
                                    .soa_members
                                    .relationship[declarations().soa_members.index(row_0)] = [&] {
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
                                        declarations()
                                            .type_refs
                                            .name[declarations().type_refs.index(new_type_refs)] =
                                            strings().intern(soa_relationship_target_.data());
                                        declarations()
                                            .type_refs.suffix[declarations().type_refs.index(
                                                new_type_refs)] = {};
                                        declarations()
                                            .type_refs
                                            .nested[declarations().type_refs.index(new_type_refs)] =
                                            std::nullopt;
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
                        if (apply_document_edit(document_->replace_declaration(
                                *declaration, std::move(replacement)))) {
                            soa_editor_declaration_.reset();
                            analysis_session_.inputs.selection.field = text(
                                declarations()
                                    .soa_members.name[declarations().soa_members.index(member)]);
                            return true;
                        }
                    }
                    ImGui::EndDisabled();
                }
                ImGui::TextDisabled(
                    "The relationship is durable semantic metadata; SoA storage, capacity, and "
                    "allocator placement remain separate physical/session concerns.");
            }
        }

        auto const mask_configured{
            document_->storage()
                .declarations.soas
                .field_mask_name[document_->storage().declarations.soas.index(*schema)]
                .has_value() &&
            document_->storage()
                .declarations.soas
                .field_enum_name[document_->storage().declarations.soas.index(*schema)]
                .has_value()};
        if (!mask_configured) {
            auto const eligible{
                declarations().soa_members.kind[declarations().soa_members.index(member)] ==
                lispb::codegen::SoaMemberKind::array};
            ImGui::BeginDisabled(!eligible);
            if (ImGui::Button("Enable generated field mask for this column")) {
                auto replacement{document_->storage().declarations.soas.clone(*schema)};
                document_->storage()
                    .declarations.soas
                    .field_mask_name[document_->storage().declarations.soas.index(replacement)] =
                    store_identifier(
                        text(document_->storage()
                                 .declarations.soas
                                 .name[document_->storage().declarations.soas.index(*schema)]) +
                        "FieldMask");
                document_->storage()
                    .declarations.soas
                    .field_enum_name[document_->storage().declarations.soas.index(replacement)] =
                    store_identifier(
                        text(document_->storage()
                                 .declarations.soas
                                 .name[document_->storage().declarations.soas.index(*schema)]) +
                        "Field");
                document_->storage()
                    .declarations.soas
                    .members[document_->storage().declarations.soas.index(replacement)] = [&] {
                    auto const original_items{declarations().items(
                        document_->storage()
                            .declarations.soas
                            .members[document_->storage().declarations.soas.index(replacement)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[*selected_index] = [&] {
                        auto row_0{declarations().soa_members.clone(rows_1[*selected_index])};
                        declarations()
                            .soa_members.mask_field[declarations().soa_members.index(row_0)] = true;
                        return row_0;
                    }();
                    return declarations().range(rows_1);
                }();
                auto const storage_name{unique_child_name(
                    declarations(),
                    declarations().soa_members,
                    declarations().soa_members.name,
                    declarations().items(
                        document_->storage()
                            .declarations.soas
                            .members[document_->storage().declarations.soas.index(replacement)]),
                    "field_mask")};
                document_->storage()
                    .declarations.soas
                    .members[document_->storage().declarations.soas.index(replacement)] = [&] {
                    auto const original_items{declarations().items(
                        document_->storage()
                            .declarations.soas
                            .members[document_->storage().declarations.soas.index(replacement)])};
                    auto rows{std::vector(original_items.begin(), original_items.end())};
                    rows.push_back([&] {
                        auto const new_soa_members{declarations().soa_members.append()};
                        declarations()
                            .soa_members.name[declarations().soa_members.index(new_soa_members)] =
                            strings().intern(storage_name);
                        declarations()
                            .soa_members.kind[declarations().soa_members.index(new_soa_members)] =
                            lispb::codegen::SoaMemberKind::array;
                        declarations().soa_members.type[declarations().soa_members.index(
                            new_soa_members)] = [&] {
                            auto const new_type_refs{declarations().type_refs.append()};
                            declarations()
                                .type_refs.name[declarations().type_refs.index(new_type_refs)] =
                                *document_->storage().declarations.soas.field_mask_name
                                     [document_->storage().declarations.soas.index(replacement)];
                            declarations().type_refs.suffix[declarations().type_refs.index(
                                new_type_refs)] = {};
                            declarations()
                                .type_refs.nested[declarations().type_refs.index(new_type_refs)] =
                                std::nullopt;
                            return new_type_refs;
                        }();
                        declarations().soa_members.fixed_schema[declarations().soa_members.index(
                            new_soa_members)] = std::nullopt;
                        declarations().soa_members.nested_schema[declarations().soa_members.index(
                            new_soa_members)] = std::nullopt;
                        declarations().soa_members.mask_field[declarations().soa_members.index(
                            new_soa_members)] = false;
                        declarations().soa_members.mask_dimensions[declarations().soa_members.index(
                            new_soa_members)] = {};
                        declarations().soa_members.relationship[declarations().soa_members.index(
                            new_soa_members)] = std::nullopt;
                        return new_soa_members;
                    }());
                    return declarations().range(rows);
                }();
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
                    analysis_session_.inputs.selection.field = text(
                        declarations().soa_members.name[declarations().soa_members.index(member)]);
                    return true;
                }
            }
            ImGui::EndDisabled();
            if (!eligible) {
                ImGui::TextDisabled("Generated mask fields must be array columns.");
            }
        } else {
            ImGui::Text(
                "Generated mask: %s",
                text(*document_->storage()
                          .declarations.soas
                          .field_mask_name[document_->storage().declarations.soas.index(*schema)])
                    .c_str());
            ImGui::Text(
                "Generated field enum: %s",
                text(*document_->storage()
                          .declarations.soas
                          .field_enum_name[document_->storage().declarations.soas.index(*schema)])
                    .c_str());
            auto const storage_member{
                declarations().type_refs.name[declarations().type_refs.index(
                    declarations().soa_members.type[declarations().soa_members.index(member)])] ==
                *document_->storage()
                     .declarations.soas
                     .field_mask_name[document_->storage().declarations.soas.index(*schema)]};
            auto const mask_field_count{std::ranges::count_if(
                declarations().items(
                    document_->storage()
                        .declarations.soas
                        .members[document_->storage().declarations.soas.index(*schema)]),
                [&](auto id) {
                    return declarations()
                        .soa_members.mask_field[declarations().soa_members.index(id)];
                })};
            auto included{
                declarations().soa_members.mask_field[declarations().soa_members.index(member)] !=
                0};
            auto const can_toggle{
                declarations().soa_members.kind[declarations().soa_members.index(member)] ==
                    lispb::codegen::SoaMemberKind::array &&
                !storage_member && (!included || mask_field_count > 1)};
            ImGui::BeginDisabled(!can_toggle);
            if (ImGui::Checkbox("Included in generated field mask", &included)) {
                auto replacement{document_->storage().declarations.soas.clone(*schema)};
                document_->storage()
                    .declarations.soas
                    .members[document_->storage().declarations.soas.index(replacement)] = [&] {
                    auto const original_items{declarations().items(
                        document_->storage()
                            .declarations.soas
                            .members[document_->storage().declarations.soas.index(replacement)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[*selected_index] = [&] {
                        auto row_0{declarations().soa_members.clone(rows_1[*selected_index])};
                        declarations()
                            .soa_members.mask_field[declarations().soa_members.index(row_0)] =
                            included;
                        return row_0;
                    }();
                    return declarations().range(rows_1);
                }();
                if (!included) {
                    document_->storage()
                        .declarations.soas
                        .members[document_->storage().declarations.soas.index(replacement)] = [&] {
                        auto const original_items{declarations().items(
                            document_->storage().declarations.soas.members
                                [document_->storage().declarations.soas.index(replacement)])};
                        auto rows_1{std::vector(original_items.begin(), original_items.end())};
                        rows_1[*selected_index] = [&] {
                            auto row_0{declarations().soa_members.clone(rows_1[*selected_index])};
                            declarations()
                                .soa_members
                                .mask_dimensions[declarations().soa_members.index(row_0)] = {};
                            return row_0;
                        }();
                        return declarations().range(rows_1);
                    }();
                }
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
                    soa_editor_declaration_.reset();
                    return true;
                }
            }
            ImGui::EndDisabled();
            if (storage_member) {
                ImGui::TextDisabled("This is the generated mask storage column.");
            } else if (declarations().soa_members.kind[declarations().soa_members.index(member)] !=
                       lispb::codegen::SoaMemberKind::array) {
                ImGui::TextDisabled("Generated mask fields must be array columns.");
            } else if (declarations()
                           .soa_members.mask_field[declarations().soa_members.index(member)] &&
                       mask_field_count == 1) {
                ImGui::TextDisabled("At least one generated mask field is required.");
            }

            if (ImGui::Button("Disable generated field mask")) {
                auto replacement{document_->storage().declarations.soas.clone(*schema)};
                auto const mask_type{
                    *document_->storage().declarations.soas.field_mask_name
                         [document_->storage().declarations.soas.index(replacement)]};
                auto selected_after_disable{analysis_session_.inputs.selection.field};
                auto const selected_is_storage{
                    declarations().type_refs.name[declarations().type_refs.index(
                        declarations()
                            .soa_members.type[declarations().soa_members.index(member)])] ==
                    mask_type};
                document_->storage()
                    .declarations.soas
                    .members[document_->storage().declarations.soas.index(replacement)] = [&] {
                    auto const original_items{declarations().items(
                        document_->storage()
                            .declarations.soas
                            .members[document_->storage().declarations.soas.index(replacement)])};
                    auto members{std::vector(original_items.begin(), original_items.end())};
                    std::erase_if(members, [&](auto id) {
                        return declarations().type_refs.name[declarations().type_refs.index(
                                   declarations()
                                       .soa_members.type[declarations().soa_members.index(id)])] ==
                               mask_type;
                    });
                    for (auto& id : members) {
                        id = [&] {
                            auto candidate{declarations().soa_members.clone(id)};
                            declarations().soa_members.mask_field[declarations().soa_members.index(
                                candidate)] = false;
                            declarations()
                                .soa_members
                                .mask_dimensions[declarations().soa_members.index(candidate)] = {};
                            return candidate;
                        }();
                    }
                    return declarations().range(members);
                }();
                document_->storage()
                    .declarations.soas
                    .field_mask_name[document_->storage().declarations.soas.index(replacement)]
                    .reset();
                document_->storage()
                    .declarations.soas
                    .field_enum_name[document_->storage().declarations.soas.index(replacement)]
                    .reset();
                if (selected_is_storage &&
                    !declarations()
                         .items(document_->storage().declarations.soas.members
                                    [document_->storage().declarations.soas.index(replacement)])
                         .empty()) {
                    selected_after_disable =
                        text(declarations().soa_members.name[declarations().soa_members.index(
                            declarations()
                                .items(
                                    document_->storage().declarations.soas.members
                                        [document_->storage().declarations.soas.index(replacement)])
                                .front())]);
                }
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
                    analysis_session_.inputs.selection.field = std::move(selected_after_disable);
                    return true;
                }
            }
        }

        if (declarations().soa_members.mask_field[declarations().soa_members.index(member)] &&
            detail::section("Mask dimensions")) {
            if (ImGui::Button("+ Dimension")) {
                auto replacement{document_->storage().declarations.soas.clone(*schema)};

                auto const name{unique_child_name(
                    declarations(),
                    declarations().soa_mask_dimensions,
                    declarations().soa_mask_dimensions.index_name,
                    declarations().items(
                        declarations().soa_members.mask_dimensions[declarations().soa_members.index(
                            declarations().items(document_->storage().declarations.soas.members
                                                     [document_->storage().declarations.soas.index(
                                                         replacement)])[*selected_index])]),
                    "index")};
                document_->storage()
                    .declarations.soas
                    .members[document_->storage().declarations.soas.index(replacement)] = [&] {
                    auto const original_items{declarations().items(
                        document_->storage()
                            .declarations.soas
                            .members[document_->storage().declarations.soas.index(replacement)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[*selected_index] = [&] {
                        auto row_0{declarations().soa_members.clone(rows_1[*selected_index])};
                        declarations().soa_members.mask_dimensions[declarations().soa_members.index(
                            row_0)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .soa_members
                                    .mask_dimensions[declarations().soa_members.index(row_0)])};
                            auto rows{std::vector(original_items.begin(), original_items.end())};
                            rows.push_back([&] {
                                auto const new_soa_mask_dimensions{
                                    declarations().soa_mask_dimensions.append()};
                                declarations()
                                    .soa_mask_dimensions
                                    .index_name[declarations().soa_mask_dimensions.index(
                                        new_soa_mask_dimensions)] = strings().intern(name);
                                declarations()
                                    .soa_mask_dimensions
                                    .extent[declarations().soa_mask_dimensions.index(
                                        new_soa_mask_dimensions)] = strings().add("1");
                                return new_soa_mask_dimensions;
                            }());
                            return declarations().range(rows);
                        }();
                        return row_0;
                    }();
                    return declarations().range(rows_1);
                }();
                auto const new_index{
                    declarations()
                        .items(declarations()
                                   .soa_members.mask_dimensions[declarations().soa_members.index(
                                       declarations().items(
                                           document_->storage().declarations.soas.members
                                               [document_->storage().declarations.soas.index(
                                                   replacement)])[*selected_index])])
                        .size() -
                    1};
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
                    soa_mask_dimension_index_ = new_index;
                    soa_editor_declaration_.reset();
                    return true;
                }
            }
            ImGui::SameLine();
            auto const dimension_index{soa_mask_dimension_index_};
            auto const has_dimension{
                dimension_index.has_value() &&
                *dimension_index <
                    declarations()
                        .items(declarations()
                                   .soa_members
                                   .mask_dimensions[declarations().soa_members.index(member)])
                        .size()};
            ImGui::BeginDisabled(!has_dimension);
            if (ImGui::Button("Duplicate dimension")) {
                auto replacement{document_->storage().declarations.soas.clone(*schema)};

                auto copy{declarations().soa_mask_dimensions.clone(declarations().items(
                    declarations().soa_members.mask_dimensions[declarations().soa_members.index(
                        declarations().items(
                            document_->storage().declarations.soas.members
                                [document_->storage().declarations.soas.index(
                                    replacement)])[*selected_index])])[*dimension_index])};
                declarations()
                    .soa_mask_dimensions
                    .index_name[declarations().soa_mask_dimensions.index(copy)] =
                    strings().intern(unique_child_name(
                        declarations(),
                        declarations().soa_mask_dimensions,
                        declarations().soa_mask_dimensions.index_name,
                        declarations().items(
                            declarations()
                                .soa_members.mask_dimensions[declarations().soa_members.index(
                                    declarations().items(
                                        document_->storage().declarations.soas.members
                                            [document_->storage().declarations.soas.index(
                                                replacement)])[*selected_index])]),
                        text(declarations()
                                 .soa_mask_dimensions
                                 .index_name[declarations().soa_mask_dimensions.index(copy)]) +
                            "_copy"));
                document_->storage()
                    .declarations.soas
                    .members[document_->storage().declarations.soas.index(replacement)] = [&] {
                    auto const original_items{declarations().items(
                        document_->storage()
                            .declarations.soas
                            .members[document_->storage().declarations.soas.index(replacement)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[*selected_index] = [&] {
                        auto row_0{declarations().soa_members.clone(rows_1[*selected_index])};
                        declarations().soa_members.mask_dimensions[declarations().soa_members.index(
                            row_0)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .soa_members
                                    .mask_dimensions[declarations().soa_members.index(row_0)])};
                            auto rows{std::vector(original_items.begin(), original_items.end())};
                            rows.insert(rows.begin() +
                                            static_cast<std::ptrdiff_t>(*dimension_index + 1),
                                        copy);
                            return declarations().range(rows);
                        }();
                        return row_0;
                    }();
                    return declarations().range(rows_1);
                }();
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
                    soa_mask_dimension_index_ = *dimension_index + 1;
                    soa_editor_declaration_.reset();
                    return true;
                }
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(!has_dimension || *dimension_index == 0);
            if (ImGui::Button("Dimension up")) {
                auto replacement{document_->storage().declarations.soas.clone(*schema)};

                document_->storage()
                    .declarations.soas
                    .members[document_->storage().declarations.soas.index(replacement)] = [&] {
                    auto const original_items{declarations().items(
                        document_->storage()
                            .declarations.soas
                            .members[document_->storage().declarations.soas.index(replacement)])};
                    auto members{std::vector(original_items.begin(), original_items.end())};
                    members[*selected_index] = [&] {
                        auto member{declarations().soa_members.clone(members[*selected_index])};
                        declarations().soa_members.mask_dimensions[declarations().soa_members.index(
                            member)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .soa_members
                                    .mask_dimensions[declarations().soa_members.index(member)])};
                            auto dimensions{
                                std::vector(original_items.begin(), original_items.end())};
                            std::swap(dimensions[*dimension_index],
                                      dimensions[*dimension_index - 1]);
                            return declarations().range(dimensions);
                        }();
                        return member;
                    }();
                    return declarations().range(members);
                }();
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
                    soa_mask_dimension_index_ = *dimension_index - 1;
                    soa_editor_declaration_.reset();
                    return true;
                }
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(
                !has_dimension ||
                *dimension_index + 1 >=
                    declarations()
                        .items(declarations()
                                   .soa_members
                                   .mask_dimensions[declarations().soa_members.index(member)])
                        .size());
            if (ImGui::Button("Dimension down")) {
                auto replacement{document_->storage().declarations.soas.clone(*schema)};

                document_->storage()
                    .declarations.soas
                    .members[document_->storage().declarations.soas.index(replacement)] = [&] {
                    auto const original_items{declarations().items(
                        document_->storage()
                            .declarations.soas
                            .members[document_->storage().declarations.soas.index(replacement)])};
                    auto members{std::vector(original_items.begin(), original_items.end())};
                    members[*selected_index] = [&] {
                        auto member{declarations().soa_members.clone(members[*selected_index])};
                        declarations().soa_members.mask_dimensions[declarations().soa_members.index(
                            member)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .soa_members
                                    .mask_dimensions[declarations().soa_members.index(member)])};
                            auto dimensions{
                                std::vector(original_items.begin(), original_items.end())};
                            std::swap(dimensions[*dimension_index],
                                      dimensions[*dimension_index + 1]);
                            return declarations().range(dimensions);
                        }();
                        return member;
                    }();
                    return declarations().range(members);
                }();
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
                    soa_mask_dimension_index_ = *dimension_index + 1;
                    soa_editor_declaration_.reset();
                    return true;
                }
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Delete dimension")) {
                auto replacement{document_->storage().declarations.soas.clone(*schema)};

                document_->storage()
                    .declarations.soas
                    .members[document_->storage().declarations.soas.index(replacement)] = [&] {
                    auto const original_items{declarations().items(
                        document_->storage()
                            .declarations.soas
                            .members[document_->storage().declarations.soas.index(replacement)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[*selected_index] = [&] {
                        auto row_0{declarations().soa_members.clone(rows_1[*selected_index])};
                        declarations().soa_members.mask_dimensions[declarations().soa_members.index(
                            row_0)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .soa_members
                                    .mask_dimensions[declarations().soa_members.index(row_0)])};
                            auto rows{std::vector(original_items.begin(), original_items.end())};
                            rows.erase(rows.begin() +
                                       static_cast<std::ptrdiff_t>(*dimension_index));
                            return declarations().range(rows);
                        }();
                        return row_0;
                    }();
                    return declarations().range(rows_1);
                }();
                auto const next_index{
                    declarations()
                            .items(declarations().soa_members.mask_dimensions
                                       [declarations().soa_members.index(declarations().items(
                                           document_->storage().declarations.soas.members
                                               [document_->storage().declarations.soas.index(
                                                   replacement)])[*selected_index])])
                            .empty()
                        ? std::optional<std::size_t>{}
                        : std::optional{std::min(
                              *dimension_index,
                              declarations()
                                      .items(
                                          declarations()
                                              .soa_members
                                              .mask_dimensions[declarations().soa_members.index(
                                                  declarations().items(
                                                      document_->storage().declarations.soas.members
                                                          [document_->storage()
                                                               .declarations.soas.index(
                                                                   replacement)])
                                                      [*selected_index])])
                                      .size() -
                                  1)}};
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
                    soa_mask_dimension_index_ = next_index;
                    soa_editor_declaration_.reset();
                    return true;
                }
            }
            ImGui::EndDisabled();

            if (soa_mask_dimension_names_.size() ==
                    declarations()
                        .items(declarations()
                                   .soa_members
                                   .mask_dimensions[declarations().soa_members.index(member)])
                        .size() &&
                soa_mask_dimension_extents_.size() ==
                    declarations()
                        .items(declarations()
                                   .soa_members
                                   .mask_dimensions[declarations().soa_members.index(member)])
                        .size() &&
                detail::begin_editable_table(
                    "soa-mask-dimensions",
                    3,
                    declarations()
                        .items(declarations()
                                   .soa_members
                                   .mask_dimensions[declarations().soa_members.index(member)])
                        .size())) {
                detail::editable_table_column("", ImGuiTableColumnFlags_WidthFixed);
                detail::editable_table_column("Index name");
                detail::editable_table_column("Extent");
                ImGui::TableHeadersRow();
                for (std::size_t index{};
                     index <
                     declarations()
                         .items(declarations()
                                    .soa_members
                                    .mask_dimensions[declarations().soa_members.index(member)])
                         .size();
                     ++index) {
                    ImGui::PushID(static_cast<int>(index));
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    auto const row_selected{soa_mask_dimension_index_ == index};
                    if (detail::editable_table_row_handle(row_selected)) {
                        soa_mask_dimension_index_ = index;
                    }
                    if (ImGui::BeginDragDropSource()) {
                        ImGui::SetDragDropPayload("SOA_MASK_DIMENSION_ROW", &index, sizeof(index));
                        ImGui::Text(
                            "Move %s",
                            text(declarations().soa_mask_dimensions.index_name
                                     [declarations().soa_mask_dimensions.index(declarations().items(
                                         declarations().soa_members.mask_dimensions
                                             [declarations().soa_members.index(member)])[index])])
                                .c_str());
                        ImGui::EndDragDropSource();
                    }
                    if (ImGui::BeginDragDropTarget()) {
                        if (auto const* payload{
                                ImGui::AcceptDragDropPayload("SOA_MASK_DIMENSION_ROW")}) {
                            auto const source_index{
                                *static_cast<std::size_t const*>(payload->Data)};
                            if (source_index <
                                    declarations()
                                        .items(declarations().soa_members.mask_dimensions
                                                   [declarations().soa_members.index(member)])
                                        .size() &&
                                source_index != index) {
                                pending = declarations().soas.clone(*schema);
                                declarations().soas.members[declarations().soas.index(*pending)] =
                                    [&] {
                                        auto const original_items{declarations().items(
                                            declarations().soas.members[declarations().soas.index(
                                                *pending)])};
                                        auto rows_1{std::vector(original_items.begin(),
                                                                original_items.end())};
                                        rows_1[*selected_index] = [&] {
                                            auto row_0{declarations().soa_members.clone(
                                                rows_1[*selected_index])};
                                            declarations().soa_members.mask_dimensions
                                                [declarations().soa_members.index(row_0)] = [&] {
                                                auto const original_items{declarations().items(
                                                    declarations().soa_members.mask_dimensions
                                                        [declarations().soa_members.index(row_0)])};
                                                auto rows{std::vector(original_items.begin(),
                                                                      original_items.end())};
                                                move_element(rows, source_index, index);
                                                return declarations().range(rows);
                                            }();
                                            return row_0;
                                        }();
                                        return declarations().range(rows_1);
                                    }();
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
                            pending = declarations().soas.clone(*schema);
                        }
                        declarations().soas.members[declarations().soas.index(*pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations().soas.members[declarations().soas.index(*pending)])};
                            auto rows_3{std::vector(original_items.begin(), original_items.end())};
                            rows_3[*selected_index] = [&] {
                                auto row_2{
                                    declarations().soa_members.clone(rows_3[*selected_index])};
                                declarations()
                                    .soa_members
                                    .mask_dimensions[declarations().soa_members.index(row_2)] =
                                    [&] {
                                        auto const original_items{declarations().items(
                                            declarations().soa_members.mask_dimensions
                                                [declarations().soa_members.index(row_2)])};
                                        auto rows_1{std::vector(original_items.begin(),
                                                                original_items.end())};
                                        rows_1[index] = [&] {
                                            auto row_0{declarations().soa_mask_dimensions.clone(
                                                rows_1[index])};
                                            declarations().soa_mask_dimensions.index_name
                                                [declarations().soa_mask_dimensions.index(row_0)] =
                                                strings().intern(
                                                    soa_mask_dimension_names_[index].data());
                                            return row_0;
                                        }();
                                        return declarations().range(rows_1);
                                    }();
                                return row_2;
                            }();
                            return declarations().range(rows_3);
                        }();
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
                            pending = declarations().soas.clone(*schema);
                        }
                        declarations().soas.members[declarations().soas.index(*pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations().soas.members[declarations().soas.index(*pending)])};
                            auto rows_3{std::vector(original_items.begin(), original_items.end())};
                            rows_3[*selected_index] = [&] {
                                auto row_2{
                                    declarations().soa_members.clone(rows_3[*selected_index])};
                                declarations()
                                    .soa_members
                                    .mask_dimensions[declarations().soa_members.index(row_2)] =
                                    [&] {
                                        auto const original_items{declarations().items(
                                            declarations().soa_members.mask_dimensions
                                                [declarations().soa_members.index(row_2)])};
                                        auto rows_1{std::vector(original_items.begin(),
                                                                original_items.end())};
                                        rows_1[index] = [&] {
                                            auto row_0{declarations().soa_mask_dimensions.clone(
                                                rows_1[index])};
                                            declarations().soa_mask_dimensions.extent
                                                [declarations().soa_mask_dimensions.index(row_0)] =
                                                strings().add(
                                                    soa_mask_dimension_extents_[index].data());
                                            return row_0;
                                        }();
                                        return declarations().range(rows_1);
                                    }();
                                return row_2;
                            }();
                            return declarations().range(rows_3);
                        }();
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }
    }

    if (detail::section("Storage operations")) {
        auto const all_storage_operations{lispb::codegen::all_storage_operations()};
        auto const all_operations_enabled{
            std::ranges::all_of(all_storage_operations, [&](auto const operation) {
                return has_storage_operation(
                    declarations().items(
                        document_->storage()
                            .declarations.soas
                            .operations[document_->storage().declarations.soas.index(*schema)]),
                    operation);
            })};
        ImGui::BeginDisabled(pending.has_value() || all_operations_enabled);
        auto const enable_all_operations{ImGui::Button("Enable all operations")};
        ImGui::EndDisabled();
        if (enable_all_operations) {
            auto replacement{document_->storage().declarations.soas.clone(*schema)};
            document_->storage()
                .declarations.soas
                .operations[document_->storage().declarations.soas.index(replacement)] =
                declarations().range<lispb::codegen::StorageOperation>(all_storage_operations);
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
                return true;
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(
            pending.has_value() ||
            declarations()
                .items(document_->storage()
                           .declarations.soas
                           .operations[document_->storage().declarations.soas.index(*schema)])
                .empty());
        auto const disable_all_operations{ImGui::Button("Disable all operations")};
        ImGui::EndDisabled();
        if (disable_all_operations) {
            auto replacement{document_->storage().declarations.soas.clone(*schema)};
            document_->storage()
                .declarations.soas
                .operations[document_->storage().declarations.soas.index(replacement)] = {};
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
                return true;
            }
        }
        auto operation_applied{false};
        ImGui::BeginDisabled(pending.has_value());
        if (ImGui::BeginTable("soa-storage-operations", 2, ImGuiTableFlags_SizingStretchSame)) {
            for (auto const operation : all_storage_operations) {
                auto const descriptor{storage_operation_descriptor(operation)};
                ImGui::TableNextColumn();
                auto enabled{has_storage_operation(
                    declarations().items(
                        document_->storage()
                            .declarations.soas
                            .operations[document_->storage().declarations.soas.index(*schema)]),
                    operation)};
                auto const toggled{ImGui::Checkbox(descriptor.source_name, &enabled)};
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", descriptor.description);
                }
                if (toggled) {
                    auto replacement{document_->storage().declarations.soas.clone(*schema)};
                    document_->storage()
                        .declarations.soas
                        .operations[document_->storage().declarations.soas.index(replacement)] =
                        declarations().range(with_storage_operation(
                            declarations().items(
                                document_->storage().declarations.soas.operations
                                    [document_->storage().declarations.soas.index(*schema)]),
                            operation,
                            enabled));
                    if (apply_document_edit(
                            document_->replace_declaration(*declaration, std::move(replacement)))) {
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
    }

    if (detail::section("Generation policy")) {
        detail::prepare_property_input("Export specifier (optional)");
        auto const export_submitted{ImGui::InputText("##Export specifier (optional)",
                                                     soa_export_specifier_.data(),
                                                     soa_export_specifier_.size(),
                                                     ImGuiInputTextFlags_EnterReturnsTrue)};
        if (export_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
            if (!pending.has_value()) {
                pending = declarations().soas.clone(*schema);
            }
            declarations().soas.export_specifier[declarations().soas.index(*pending)] =
                strings().intern(soa_export_specifier_.data());
        }

        ImGui::TextUnformatted("Equivalent row type (optional)");
        ImGui::SetNextItemWidth(std::max(60.0F, ImGui::GetContentRegionAvail().x - 58.0F));
        auto const equivalent_submitted{ImGui::InputText("##soa-equivalent-type",
                                                         soa_equivalent_type_.data(),
                                                         soa_equivalent_type_.size(),
                                                         ImGuiInputTextFlags_EnterReturnsTrue)};
        if (equivalent_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
            if (!pending.has_value()) {
                pending = declarations().soas.clone(*schema);
            }
            if (soa_equivalent_type_.front() == '\0') {
                declarations().soas.equivalent_type[declarations().soas.index(*pending)].reset();
            } else if (declarations()
                           .soas.equivalent_type[declarations().soas.index(*pending)]
                           .has_value()) {
                (*declarations().soas.equivalent_type[declarations().soas.index(*pending)]) = [&] {
                    auto row_0{declarations().type_refs.clone(
                        (*declarations()
                              .soas.equivalent_type[declarations().soas.index(*pending)]))};
                    declarations().type_refs.name[declarations().type_refs.index(row_0)] =
                        strings().intern(soa_equivalent_type_.data());
                    return row_0;
                }();
            } else {
                declarations().soas.equivalent_type[declarations().soas.index(*pending)] = [&] {
                    auto const new_type_refs{declarations().type_refs.append()};
                    declarations().type_refs.name[declarations().type_refs.index(new_type_refs)] =
                        strings().intern(soa_equivalent_type_.data());
                    declarations()
                        .type_refs.suffix[declarations().type_refs.index(new_type_refs)] = {};
                    declarations().type_refs.nested[declarations().type_refs.index(new_type_refs)] =
                        std::nullopt;
                    return new_type_refs;
                }();
            }
        }
        ImGui::SameLine();
        ImGui::PushID("soa-equivalent-type");
        if (auto picked{
                draw_type_picker(analysis_session_.inputs.workspace.types().module_name(node),
                                 analysis_session_.inputs.workspace.types().identity(node))}) {
            if (!pending.has_value()) {
                pending = declarations().soas.clone(*schema);
            }
            if (declarations()
                    .soas.equivalent_type[declarations().soas.index(*pending)]
                    .has_value()) {
                (*declarations().soas.equivalent_type[declarations().soas.index(*pending)]) = [&] {
                    auto row_0{declarations().type_refs.clone(
                        (*declarations()
                              .soas.equivalent_type[declarations().soas.index(*pending)]))};
                    declarations().type_refs.name[declarations().type_refs.index(row_0)] =
                        strings().intern(*picked);
                    return row_0;
                }();
            } else {
                declarations().soas.equivalent_type[declarations().soas.index(*pending)] = [&] {
                    auto const new_type_refs{declarations().type_refs.append()};
                    declarations().type_refs.name[declarations().type_refs.index(new_type_refs)] =
                        strings().intern(*picked);
                    declarations()
                        .type_refs.suffix[declarations().type_refs.index(new_type_refs)] = {};
                    declarations().type_refs.nested[declarations().type_refs.index(new_type_refs)] =
                        std::nullopt;
                    return new_type_refs;
                }();
            }
        }
        ImGui::PopID();
        ImGui::SameLine();
        ImGui::BeginDisabled(
            pending.has_value() ||
            !lispb::schema::is_valid(
                definitions().soas.equivalent_type[definitions().soas.index(soa)]));
        if (ImGui::SmallButton(">##equivalent-type")) {
            navigate_to =
                analysis_session_.inputs.workspace.types()
                    .occurrences()
                    .type[analysis_session_.inputs.workspace.types().occurrences().index(
                        definitions().soas.equivalent_type[definitions().soas.index(soa)])];
        }
        ImGui::EndDisabled();

        auto layout_only{document_->storage()
                             .declarations.soas
                             .layout_only[document_->storage().declarations.soas.index(*schema)] !=
                         0};
        ImGui::BeginDisabled(pending.has_value());
        auto const layout_only_toggled{ImGui::Checkbox("Layout only", &layout_only)};
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "Keep this declaration available as a nested layout without emitting its "
                "standalone storage/view API.");
        }
        ImGui::EndDisabled();
        if (layout_only_toggled) {
            auto replacement{document_->storage().declarations.soas.clone(*schema)};
            document_->storage()
                .declarations.soas
                .layout_only[document_->storage().declarations.soas.index(replacement)] =
                layout_only;
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
                return true;
            }
        }
        auto copy_element_memberwise{
            document_->storage()
                .declarations.soas
                .copy_element_memberwise[document_->storage().declarations.soas.index(*schema)] !=
            0};
        ImGui::BeginDisabled(pending.has_value());
        auto const copy_memberwise_toggled{
            ImGui::Checkbox("Copy elements memberwise", &copy_element_memberwise)};
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "Generate direct member assignment for copy-element; this has no effect "
                "unless the copy-element storage operation is enabled.");
        }
        ImGui::EndDisabled();
        if (copy_memberwise_toggled) {
            auto replacement{document_->storage().declarations.soas.clone(*schema)};
            document_->storage().declarations.soas.copy_element_memberwise
                [document_->storage().declarations.soas.index(replacement)] =
                copy_element_memberwise;
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
                return true;
            }
        }
    }

    if (detail::section("Using declarations")) {
        ImGui::BeginDisabled(pending.has_value());
        auto const add_using_declaration{ImGui::Button("+ Using declaration")};
        ImGui::EndDisabled();
        if (add_using_declaration) {
            auto replacement{document_->storage().declarations.soas.clone(*schema)};
            document_->storage()
                .declarations.soas
                .using_declarations[document_->storage().declarations.soas.index(replacement)] =
                [&] {
                    auto const original_items{declarations().items(
                        document_->storage().declarations.soas.using_declarations
                            [document_->storage().declarations.soas.index(replacement)])};
                    auto rows{std::vector(original_items.begin(), original_items.end())};
                    rows.push_back(strings().add(unique_soa_using_declaration(
                        declarations(),
                        document_->storage().declarations.soas.using_declarations
                            [document_->storage().declarations.soas.index(replacement)])));
                    return declarations().range(rows);
                }();
            auto const new_index{
                declarations()
                    .items(document_->storage().declarations.soas.using_declarations
                               [document_->storage().declarations.soas.index(replacement)])
                    .size() -
                1};
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
                soa_using_declaration_index_ = new_index;
                soa_editor_declaration_.reset();
                return true;
            }
        }
        ImGui::SameLine();
        auto const using_index{soa_using_declaration_index_};
        auto const has_using{
            using_index.has_value() &&
            *using_index < declarations()
                               .items(document_->storage().declarations.soas.using_declarations
                                          [document_->storage().declarations.soas.index(*schema)])
                               .size()};
        ImGui::BeginDisabled(pending.has_value() || !has_using || *using_index == 0);
        auto const move_using_up{ImGui::Button("Using up")};
        ImGui::EndDisabled();
        if (move_using_up) {
            auto replacement{document_->storage().declarations.soas.clone(*schema)};
            document_->storage()
                .declarations.soas
                .using_declarations[document_->storage().declarations.soas.index(replacement)] =
                [&] {
                    auto const original_items{declarations().items(
                        document_->storage().declarations.soas.using_declarations
                            [document_->storage().declarations.soas.index(replacement)])};
                    auto rows{std::vector(original_items.begin(), original_items.end())};
                    std::swap(rows[*using_index], rows[*using_index - 1]);
                    return declarations().range(rows);
                }();
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
                soa_using_declaration_index_ = *using_index - 1;
                soa_editor_declaration_.reset();
                return true;
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(
            pending.has_value() || !has_using ||
            *using_index + 1 >=
                declarations()
                    .items(document_->storage().declarations.soas.using_declarations
                               [document_->storage().declarations.soas.index(*schema)])
                    .size());
        auto const move_using_down{ImGui::Button("Using down")};
        ImGui::EndDisabled();
        if (move_using_down) {
            auto replacement{document_->storage().declarations.soas.clone(*schema)};
            document_->storage()
                .declarations.soas
                .using_declarations[document_->storage().declarations.soas.index(replacement)] =
                [&] {
                    auto const original_items{declarations().items(
                        document_->storage().declarations.soas.using_declarations
                            [document_->storage().declarations.soas.index(replacement)])};
                    auto rows{std::vector(original_items.begin(), original_items.end())};
                    std::swap(rows[*using_index], rows[*using_index + 1]);
                    return declarations().range(rows);
                }();
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
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
            auto replacement{document_->storage().declarations.soas.clone(*schema)};
            document_->storage()
                .declarations.soas
                .using_declarations[document_->storage().declarations.soas.index(replacement)] =
                [&] {
                    auto const original_items{declarations().items(
                        document_->storage().declarations.soas.using_declarations
                            [document_->storage().declarations.soas.index(replacement)])};
                    auto rows{std::vector(original_items.begin(), original_items.end())};
                    rows.erase(rows.begin() + static_cast<std::ptrdiff_t>(*using_index));
                    return declarations().range(rows);
                }();
            auto const next_index{
                declarations()
                        .items(document_->storage().declarations.soas.using_declarations
                                   [document_->storage().declarations.soas.index(replacement)])
                        .empty()
                    ? std::optional<std::size_t>{}
                    : std::optional{std::min(
                          *using_index,
                          declarations()
                                  .items(document_->storage().declarations.soas.using_declarations
                                             [document_->storage().declarations.soas.index(
                                                 replacement)])
                                  .size() -
                              1)}};
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
                soa_using_declaration_index_ = next_index;
                soa_editor_declaration_.reset();
                return true;
            }
        }

        ImGui::BeginDisabled(pending.has_value());
        if (soa_using_declarations_.size() ==
                declarations()
                    .items(document_->storage().declarations.soas.using_declarations
                               [document_->storage().declarations.soas.index(*schema)])
                    .size() &&
            detail::begin_editable_table(
                "soa-using-declarations",
                2,
                declarations()
                    .items(document_->storage().declarations.soas.using_declarations
                               [document_->storage().declarations.soas.index(*schema)])
                    .size())) {
            detail::editable_table_column("", ImGuiTableColumnFlags_WidthFixed);
            detail::editable_table_column("Declaration after 'using'");
            ImGui::TableHeadersRow();
            for (std::size_t index{};
                 index < declarations()
                             .items(document_->storage().declarations.soas.using_declarations
                                        [document_->storage().declarations.soas.index(*schema)])
                             .size();
                 ++index) {
                ImGui::PushID(static_cast<int>(index));
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                auto const row_selected{soa_using_declaration_index_ == index};
                if (detail::editable_table_row_handle(row_selected)) {
                    soa_using_declaration_index_ = index;
                }
                if (ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload("SOA_USING_DECLARATION_ROW", &index, sizeof(index));
                    ImGui::Text(
                        "Move %s",
                        text(
                            declarations().items(
                                document_->storage().declarations.soas.using_declarations
                                    [document_->storage().declarations.soas.index(*schema)])[index])
                            .c_str());
                    ImGui::EndDragDropSource();
                }
                if (ImGui::BeginDragDropTarget()) {
                    if (auto const* payload{
                            ImGui::AcceptDragDropPayload("SOA_USING_DECLARATION_ROW")}) {
                        auto const source_index{*static_cast<std::size_t const*>(payload->Data)};
                        if (source_index <
                                declarations()
                                    .items(
                                        document_->storage().declarations.soas.using_declarations
                                            [document_->storage().declarations.soas.index(*schema)])
                                    .size() &&
                            source_index != index) {
                            pending = declarations().soas.clone(*schema);
                            declarations().soas.using_declarations[declarations().soas.index(
                                *pending)] = [&] {
                                auto const original_items{declarations().items(
                                    declarations()
                                        .soas
                                        .using_declarations[declarations().soas.index(*pending)])};
                                auto rows{
                                    std::vector(original_items.begin(), original_items.end())};
                                move_element(rows, source_index, index);
                                return declarations().range(rows);
                            }();
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
                        pending = declarations().soas.clone(*schema);
                    }
                    declarations().soas.using_declarations[declarations().soas.index(*pending)] =
                        [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .soas.using_declarations[declarations().soas.index(*pending)])};
                            auto rows_0{std::vector(original_items.begin(), original_items.end())};
                            rows_0[index] = strings().add(soa_using_declarations_[index].data());
                            return declarations().range(rows_0);
                        }();
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::EndDisabled();
    }

    if (detail::section("Custom functions")) {
        ImGui::BeginDisabled(pending.has_value());
        auto const add_function{ImGui::Button("+ Function")};
        ImGui::EndDisabled();
        if (add_function) {
            auto replacement{document_->storage().declarations.soas.clone(*schema)};
            auto function{[&] {
                auto const new_functions{declarations().functions.append()};

                return new_functions;
            }()};
            declarations().functions.name[declarations().functions.index(function)] =
                strings().intern(unique_child_name(
                    declarations(),
                    declarations().functions,
                    declarations().functions.name,
                    declarations().items(
                        document_->storage()
                            .declarations.soas
                            .functions[document_->storage().declarations.soas.index(replacement)]),
                    "custom_function"));
            declarations().functions.return_type[declarations().functions.index(function)] = [&] {
                auto const new_type_refs{declarations().type_refs.append()};
                declarations().type_refs.name[declarations().type_refs.index(new_type_refs)] =
                    strings().intern("void");
                declarations().type_refs.suffix[declarations().type_refs.index(new_type_refs)] = {};
                declarations().type_refs.nested[declarations().type_refs.index(new_type_refs)] =
                    std::nullopt;
                return new_type_refs;
            }();
            declarations().functions.is_inline[declarations().functions.index(function)] = true;
            document_->storage()
                .declarations.soas
                .functions[document_->storage().declarations.soas.index(replacement)] = [&] {
                auto const original_items{declarations().items(
                    document_->storage()
                        .declarations.soas
                        .functions[document_->storage().declarations.soas.index(replacement)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back(std::move(function));
                return declarations().range(rows);
            }();
            auto const new_index{
                declarations()
                    .items(
                        document_->storage()
                            .declarations.soas
                            .functions[document_->storage().declarations.soas.index(replacement)])
                    .size() -
                1};
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
                soa_function_index_ = new_index;
                soa_editor_declaration_.reset();
                return true;
            }
        }
        ImGui::SameLine();
        auto const function_index{soa_function_index_};
        auto const has_function{
            function_index.has_value() &&
            *function_index <
                declarations()
                    .items(document_->storage()
                               .declarations.soas
                               .functions[document_->storage().declarations.soas.index(*schema)])
                    .size()};
        ImGui::BeginDisabled(pending.has_value() || !has_function);
        auto const duplicate_function{ImGui::Button("Duplicate function")};
        ImGui::EndDisabled();
        if (duplicate_function) {
            auto replacement{document_->storage().declarations.soas.clone(*schema)};
            auto copy{declarations().functions.clone(declarations().items(
                document_->storage()
                    .declarations.soas.functions[document_->storage().declarations.soas.index(
                        replacement)])[*function_index])};
            declarations().functions.name[declarations().functions.index(copy)] =
                strings().intern(unique_child_name(
                    declarations(),
                    declarations().functions,
                    declarations().functions.name,
                    declarations().items(
                        document_->storage()
                            .declarations.soas
                            .functions[document_->storage().declarations.soas.index(replacement)]),
                    text(declarations().functions.name[declarations().functions.index(copy)]) +
                        "_copy"));
            document_->storage()
                .declarations.soas
                .functions[document_->storage().declarations.soas.index(replacement)] = [&] {
                auto const original_items{declarations().items(
                    document_->storage()
                        .declarations.soas
                        .functions[document_->storage().declarations.soas.index(replacement)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.insert(rows.begin() + static_cast<std::ptrdiff_t>(*function_index + 1),
                            std::move(copy));
                return declarations().range(rows);
            }();
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
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
            auto replacement{document_->storage().declarations.soas.clone(*schema)};
            document_->storage()
                .declarations.soas
                .functions[document_->storage().declarations.soas.index(replacement)] = [&] {
                auto const original_items{declarations().items(
                    document_->storage()
                        .declarations.soas
                        .functions[document_->storage().declarations.soas.index(replacement)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                std::swap(rows[*function_index], rows[*function_index - 1]);
                return declarations().range(rows);
            }();
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
                soa_function_index_ = *function_index - 1;
                soa_editor_declaration_.reset();
                return true;
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(
            pending.has_value() || !has_function ||
            *function_index + 1 >=
                declarations()
                    .items(document_->storage()
                               .declarations.soas
                               .functions[document_->storage().declarations.soas.index(*schema)])
                    .size());
        auto const move_function_down{ImGui::Button("Function down")};
        ImGui::EndDisabled();
        if (move_function_down) {
            auto replacement{document_->storage().declarations.soas.clone(*schema)};
            document_->storage()
                .declarations.soas
                .functions[document_->storage().declarations.soas.index(replacement)] = [&] {
                auto const original_items{declarations().items(
                    document_->storage()
                        .declarations.soas
                        .functions[document_->storage().declarations.soas.index(replacement)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                std::swap(rows[*function_index], rows[*function_index + 1]);
                return declarations().range(rows);
            }();
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
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
            auto replacement{document_->storage().declarations.soas.clone(*schema)};
            document_->storage()
                .declarations.soas
                .functions[document_->storage().declarations.soas.index(replacement)] = [&] {
                auto const original_items{declarations().items(
                    document_->storage()
                        .declarations.soas
                        .functions[document_->storage().declarations.soas.index(replacement)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.erase(rows.begin() + static_cast<std::ptrdiff_t>(*function_index));
                return declarations().range(rows);
            }();
            auto const next_index{
                declarations()
                        .items(document_->storage().declarations.soas.functions
                                   [document_->storage().declarations.soas.index(replacement)])
                        .empty()
                    ? std::optional<std::size_t>{}
                    : std::optional{
                          std::min(*function_index,
                                   declarations()
                                           .items(document_->storage().declarations.soas.functions
                                                      [document_->storage().declarations.soas.index(
                                                          replacement)])
                                           .size() -
                                       1)}};
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
                soa_function_index_ = next_index;
                soa_editor_declaration_.reset();
                return true;
            }
        }

        ImGui::BeginDisabled(pending.has_value());
        if (soa_function_names_.size() ==
                declarations()
                    .items(document_->storage()
                               .declarations.soas
                               .functions[document_->storage().declarations.soas.index(*schema)])
                    .size() &&
            soa_function_return_types_.size() ==
                declarations()
                    .items(document_->storage()
                               .declarations.soas
                               .functions[document_->storage().declarations.soas.index(*schema)])
                    .size() &&
            detail::begin_editable_table(
                "soa-functions",
                8,
                declarations()
                    .items(document_->storage()
                               .declarations.soas
                               .functions[document_->storage().declarations.soas.index(*schema)])
                    .size())) {
            detail::editable_table_column("", ImGuiTableColumnFlags_WidthFixed);
            detail::editable_table_column("Name");
            detail::editable_table_column("Return");
            detail::editable_table_column("const", ImGuiTableColumnFlags_WidthFixed);
            detail::editable_table_column("noexcept", ImGuiTableColumnFlags_WidthFixed);
            detail::editable_table_column("static", ImGuiTableColumnFlags_WidthFixed);
            detail::editable_table_column("inline", ImGuiTableColumnFlags_WidthFixed);
            detail::editable_table_column("source", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableHeadersRow();
            for (std::size_t index{};
                 index <
                 declarations()
                     .items(document_->storage()
                                .declarations.soas
                                .functions[document_->storage().declarations.soas.index(*schema)])
                     .size();
                 ++index) {
                auto const function{declarations().items(
                    document_->storage()
                        .declarations.soas
                        .functions[document_->storage().declarations.soas.index(*schema)])[index]};
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
                    ImGui::Text(
                        "Move %s",
                        text(
                            declarations().functions.name[declarations().functions.index(function)])
                            .c_str());
                    ImGui::EndDragDropSource();
                }
                if (ImGui::BeginDragDropTarget()) {
                    if (auto const* payload{ImGui::AcceptDragDropPayload("SOA_FUNCTION_ROW")}) {
                        auto const source_index{*static_cast<std::size_t const*>(payload->Data)};
                        if (source_index <
                                declarations()
                                    .items(
                                        document_->storage().declarations.soas.functions
                                            [document_->storage().declarations.soas.index(*schema)])
                                    .size() &&
                            source_index != index) {
                            pending = declarations().soas.clone(*schema);
                            declarations().soas.functions[declarations().soas.index(*pending)] =
                                [&] {
                                    auto const original_items{declarations().items(
                                        declarations()
                                            .soas.functions[declarations().soas.index(*pending)])};
                                    auto rows{
                                        std::vector(original_items.begin(), original_items.end())};
                                    move_element(rows, source_index, index);
                                    return declarations().range(rows);
                                }();
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
                        pending = declarations().soas.clone(*schema);
                    }
                    declarations().soas.functions[declarations().soas.index(*pending)] = [&] {
                        auto const original_items{declarations().items(
                            declarations().soas.functions[declarations().soas.index(*pending)])};
                        auto rows_1{std::vector(original_items.begin(), original_items.end())};
                        rows_1[index] = [&] {
                            auto row_0{declarations().functions.clone(rows_1[index])};
                            declarations().functions.name[declarations().functions.index(row_0)] =
                                strings().intern(soa_function_names_[index].data());
                            return row_0;
                        }();
                        return declarations().range(rows_1);
                    }();
                }

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-1.0F);
                auto const return_submitted{
                    ImGui::InputText("##return",
                                     soa_function_return_types_[index].data(),
                                     soa_function_return_types_[index].size(),
                                     ImGuiInputTextFlags_EnterReturnsTrue)};
                if (return_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                    if (!pending.has_value()) {
                        pending = declarations().soas.clone(*schema);
                    }
                    declarations().soas.functions[declarations().soas.index(*pending)] = [&] {
                        auto const original_items{declarations().items(
                            declarations().soas.functions[declarations().soas.index(*pending)])};
                        auto rows_2{std::vector(original_items.begin(), original_items.end())};
                        rows_2[index] = [&] {
                            auto row_1{declarations().functions.clone(rows_2[index])};
                            declarations()
                                .functions.return_type[declarations().functions.index(row_1)] =
                                [&] {
                                    auto row_0{declarations().type_refs.clone(
                                        declarations()
                                            .functions
                                            .return_type[declarations().functions.index(row_1)])};
                                    declarations()
                                        .type_refs.name[declarations().type_refs.index(row_0)] =
                                        strings().intern(soa_function_return_types_[index].data());
                                    return row_0;
                                }();
                            return row_1;
                        }();
                        return declarations().range(rows_2);
                    }();
                }

                auto edit_flag = [&](char const* label, bool const current, auto&& apply) {
                    ImGui::TableNextColumn();
                    auto value{current};
                    if (ImGui::Checkbox(label, &value)) {
                        if (!pending.has_value()) {
                            pending = declarations().soas.clone(*schema);
                        }
                        declarations().soas.functions[declarations().soas.index(*pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .soas.functions[declarations().soas.index(*pending)])};
                            auto functions{
                                std::vector(original_items.begin(), original_items.end())};
                            functions[index] = [&] {
                                auto function{declarations().functions.clone(functions[index])};
                                apply(function, value);
                                return function;
                            }();
                            return declarations().range(functions);
                        }();
                    }
                };
                edit_flag(
                    "##const",
                    declarations().functions.is_const[declarations().functions.index(function)],
                    [&](auto const edited, bool const value) {
                        declarations().functions.is_const[declarations().functions.index(edited)] =
                            value;
                        if (value) {
                            declarations()
                                .functions.is_static[declarations().functions.index(edited)] =
                                false;
                        }
                    });
                edit_flag(
                    "##noexcept",
                    declarations().functions.is_noexcept[declarations().functions.index(function)],
                    [&](auto const edited, bool const value) {
                        declarations()
                            .functions.is_noexcept[declarations().functions.index(edited)] = value;
                    });
                edit_flag(
                    "##static",
                    declarations().functions.is_static[declarations().functions.index(function)],
                    [&](auto const edited, bool const value) {
                        declarations().functions.is_static[declarations().functions.index(edited)] =
                            value;
                        if (value) {
                            declarations()
                                .functions.is_const[declarations().functions.index(edited)] = false;
                        }
                    });
                edit_flag(
                    "##inline",
                    declarations().functions.is_inline[declarations().functions.index(function)],
                    [&](auto const edited, bool const value) {
                        declarations().functions.is_inline[declarations().functions.index(edited)] =
                            value;
                        if (value) {
                            declarations()
                                .functions
                                .definition_in_source[declarations().functions.index(edited)] =
                                false;
                        }
                    });
                edit_flag(
                    "##source",
                    declarations()
                        .functions.definition_in_source[declarations().functions.index(function)],
                    [&](auto const edited, bool const value) {
                        declarations()
                            .functions
                            .definition_in_source[declarations().functions.index(edited)] = value;
                        if (value) {
                            declarations()
                                .functions.is_inline[declarations().functions.index(edited)] =
                                false;
                        }
                    });
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::EndDisabled();

        if (has_function) {
            auto const function{declarations().items(
                document_->storage()
                    .declarations.soas.functions[document_->storage().declarations.soas.index(
                        *schema)])[*function_index]};

            ImGui::TextUnformatted("Parameters");
            ImGui::BeginDisabled(pending.has_value());
            auto const add_parameter{ImGui::Button("+ Parameter")};
            ImGui::EndDisabled();
            if (add_parameter) {
                auto replacement{document_->storage().declarations.soas.clone(*schema)};

                auto parameter{[&] {
                    auto const new_parameters{declarations().parameters.append()};

                    return new_parameters;
                }()};
                declarations().parameters.name[declarations().parameters.index(parameter)] =
                    strings().intern(unique_child_name(
                        declarations(),
                        declarations().parameters,
                        declarations().parameters.name,
                        declarations().items(
                            declarations().functions.parameters[declarations().functions.index(
                                declarations().items(
                                    document_->storage().declarations.soas.functions
                                        [document_->storage().declarations.soas.index(
                                            replacement)])[*function_index])]),
                        "parameter"));
                declarations().parameters.type[declarations().parameters.index(parameter)] = [&] {
                    auto const new_type_refs{declarations().type_refs.append()};
                    declarations().type_refs.name[declarations().type_refs.index(new_type_refs)] =
                        strings().intern("std::uint32_t");
                    declarations()
                        .type_refs.suffix[declarations().type_refs.index(new_type_refs)] = {};
                    declarations().type_refs.nested[declarations().type_refs.index(new_type_refs)] =
                        std::nullopt;
                    return new_type_refs;
                }();
                if (std::ranges::any_of(
                        declarations().items(
                            declarations().functions.parameters[declarations().functions.index(
                                declarations().items(
                                    document_->storage().declarations.soas.functions
                                        [document_->storage().declarations.soas.index(
                                            replacement)])[*function_index])]),
                        [&](auto id) {
                            auto const value{id};
                            return declarations()
                                .parameters.default_value[declarations().parameters.index(value)]
                                .has_value();
                        })) {
                    declarations()
                        .parameters.default_value[declarations().parameters.index(parameter)] =
                        strings().add("{}");
                }
                document_->storage()
                    .declarations.soas
                    .functions[document_->storage().declarations.soas.index(replacement)] = [&] {
                    auto const original_items{declarations().items(
                        document_->storage()
                            .declarations.soas
                            .functions[document_->storage().declarations.soas.index(replacement)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[*function_index] = [&] {
                        auto row_0{declarations().functions.clone(rows_1[*function_index])};
                        declarations()
                            .functions.parameters[declarations().functions.index(row_0)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .functions.parameters[declarations().functions.index(row_0)])};
                            auto rows{std::vector(original_items.begin(), original_items.end())};
                            rows.push_back(std::move(parameter));
                            return declarations().range(rows);
                        }();
                        return row_0;
                    }();
                    return declarations().range(rows_1);
                }();
                auto const new_index{
                    declarations()
                        .items(declarations().functions.parameters[declarations().functions.index(
                            declarations().items(document_->storage().declarations.soas.functions
                                                     [document_->storage().declarations.soas.index(
                                                         replacement)])[*function_index])])
                        .size() -
                    1};
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
                    soa_parameter_index_ = new_index;
                    soa_editor_declaration_.reset();
                    return true;
                }
            }
            ImGui::SameLine();
            auto const parameter_index{soa_parameter_index_};
            auto const has_parameter{
                parameter_index.has_value() &&
                *parameter_index <
                    declarations()
                        .items(declarations()
                                   .functions.parameters[declarations().functions.index(function)])
                        .size()};
            ImGui::BeginDisabled(pending.has_value() || !has_parameter);
            auto const duplicate_parameter{ImGui::Button("Duplicate parameter")};
            ImGui::EndDisabled();
            if (duplicate_parameter) {
                auto replacement{document_->storage().declarations.soas.clone(*schema)};

                auto copy{declarations().parameters.clone(declarations().items(
                    declarations()
                        .functions.parameters[declarations().functions.index(declarations().items(
                            document_->storage().declarations.soas.functions
                                [document_->storage().declarations.soas.index(
                                    replacement)])[*function_index])])[*parameter_index])};
                declarations().parameters.name[declarations().parameters.index(copy)] =
                    strings().intern(unique_child_name(
                        declarations(),
                        declarations().parameters,
                        declarations().parameters.name,
                        declarations().items(
                            declarations().functions.parameters[declarations().functions.index(
                                declarations().items(
                                    document_->storage().declarations.soas.functions
                                        [document_->storage().declarations.soas.index(
                                            replacement)])[*function_index])]),
                        text(
                            declarations().parameters.name[declarations().parameters.index(copy)]) +
                            "_copy"));
                document_->storage()
                    .declarations.soas
                    .functions[document_->storage().declarations.soas.index(replacement)] = [&] {
                    auto const original_items{declarations().items(
                        document_->storage()
                            .declarations.soas
                            .functions[document_->storage().declarations.soas.index(replacement)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[*function_index] = [&] {
                        auto row_0{declarations().functions.clone(rows_1[*function_index])};
                        declarations()
                            .functions.parameters[declarations().functions.index(row_0)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .functions.parameters[declarations().functions.index(row_0)])};
                            auto rows{std::vector(original_items.begin(), original_items.end())};
                            rows.insert(rows.begin() +
                                            static_cast<std::ptrdiff_t>(*parameter_index + 1),
                                        std::move(copy));
                            return declarations().range(rows);
                        }();
                        return row_0;
                    }();
                    return declarations().range(rows_1);
                }();
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
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
                auto replacement{document_->storage().declarations.soas.clone(*schema)};

                document_->storage()
                    .declarations.soas
                    .functions[document_->storage().declarations.soas.index(replacement)] = [&] {
                    auto const original_items{declarations().items(
                        document_->storage()
                            .declarations.soas
                            .functions[document_->storage().declarations.soas.index(replacement)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[*function_index] = [&] {
                        auto row_0{declarations().functions.clone(rows_1[*function_index])};
                        declarations()
                            .functions.parameters[declarations().functions.index(row_0)] = [&] {
                            auto const original_items{declarations().items(
                                declarations().functions.parameters[declarations().functions.index(
                                    declarations().items(
                                        document_->storage().declarations.soas.functions
                                            [document_->storage().declarations.soas.index(
                                                replacement)])[*function_index])])};
                            auto rows{std::vector(original_items.begin(), original_items.end())};
                            std::swap(rows[*parameter_index], rows[*parameter_index - 1]);
                            return declarations().range(rows);
                        }();
                        return row_0;
                    }();
                    return declarations().range(rows_1);
                }();
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
                    soa_parameter_index_ = *parameter_index - 1;
                    soa_editor_declaration_.reset();
                    return true;
                }
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(
                pending.has_value() || !has_parameter ||
                *parameter_index + 1 >=
                    declarations()
                        .items(declarations()
                                   .functions.parameters[declarations().functions.index(function)])
                        .size());
            auto const move_parameter_down{ImGui::Button("Parameter down")};
            ImGui::EndDisabled();
            if (move_parameter_down) {
                auto replacement{document_->storage().declarations.soas.clone(*schema)};

                document_->storage()
                    .declarations.soas
                    .functions[document_->storage().declarations.soas.index(replacement)] = [&] {
                    auto const original_items{declarations().items(
                        document_->storage()
                            .declarations.soas
                            .functions[document_->storage().declarations.soas.index(replacement)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[*function_index] = [&] {
                        auto row_0{declarations().functions.clone(rows_1[*function_index])};
                        declarations()
                            .functions.parameters[declarations().functions.index(row_0)] = [&] {
                            auto const original_items{declarations().items(
                                declarations().functions.parameters[declarations().functions.index(
                                    declarations().items(
                                        document_->storage().declarations.soas.functions
                                            [document_->storage().declarations.soas.index(
                                                replacement)])[*function_index])])};
                            auto rows{std::vector(original_items.begin(), original_items.end())};
                            std::swap(rows[*parameter_index], rows[*parameter_index + 1]);
                            return declarations().range(rows);
                        }();
                        return row_0;
                    }();
                    return declarations().range(rows_1);
                }();
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
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
                auto replacement{document_->storage().declarations.soas.clone(*schema)};

                document_->storage()
                    .declarations.soas
                    .functions[document_->storage().declarations.soas.index(replacement)] = [&] {
                    auto const original_items{declarations().items(
                        document_->storage()
                            .declarations.soas
                            .functions[document_->storage().declarations.soas.index(replacement)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[*function_index] = [&] {
                        auto row_0{declarations().functions.clone(rows_1[*function_index])};
                        declarations()
                            .functions.parameters[declarations().functions.index(row_0)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .functions.parameters[declarations().functions.index(row_0)])};
                            auto rows{std::vector(original_items.begin(), original_items.end())};
                            rows.erase(rows.begin() +
                                       static_cast<std::ptrdiff_t>(*parameter_index));
                            return declarations().range(rows);
                        }();
                        return row_0;
                    }();
                    return declarations().range(rows_1);
                }();
                auto const next_index{
                    declarations()
                            .items(
                                declarations().functions.parameters[declarations().functions.index(
                                    declarations().items(
                                        document_->storage().declarations.soas.functions
                                            [document_->storage().declarations.soas.index(
                                                replacement)])[*function_index])])
                            .empty()
                        ? std::optional<std::size_t>{}
                        : std::optional{std::min(
                              *parameter_index,
                              declarations()
                                      .items(
                                          declarations().functions.parameters
                                              [declarations().functions.index(declarations().items(
                                                  document_->storage().declarations.soas.functions
                                                      [document_->storage().declarations.soas.index(
                                                          replacement)])[*function_index])])
                                      .size() -
                                  1)}};
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
                    soa_parameter_index_ = next_index;
                    soa_editor_declaration_.reset();
                    return true;
                }
            }

            ImGui::BeginDisabled(pending.has_value());
            if (soa_parameter_names_.size() ==
                    declarations()
                        .items(declarations()
                                   .functions.parameters[declarations().functions.index(function)])
                        .size() &&
                soa_parameter_types_.size() ==
                    declarations()
                        .items(declarations()
                                   .functions.parameters[declarations().functions.index(function)])
                        .size() &&
                soa_parameter_defaults_.size() ==
                    declarations()
                        .items(declarations()
                                   .functions.parameters[declarations().functions.index(function)])
                        .size() &&
                detail::begin_editable_table(
                    "soa-function-parameters",
                    5,
                    declarations()
                        .items(declarations()
                                   .functions.parameters[declarations().functions.index(function)])
                        .size())) {
                detail::editable_table_column("", ImGuiTableColumnFlags_WidthFixed);
                detail::editable_table_column("Name");
                detail::editable_table_column("Type");
                detail::editable_table_column("Pick", ImGuiTableColumnFlags_WidthFixed);
                detail::editable_table_column("Default");
                ImGui::TableHeadersRow();
                for (std::size_t index{};
                     index <
                     declarations()
                         .items(declarations()
                                    .functions.parameters[declarations().functions.index(function)])
                         .size();
                     ++index) {
                    auto const parameter{declarations().items(
                        declarations().functions.parameters[declarations().functions.index(
                            function)])[index]};
                    ImGui::PushID(static_cast<int>(index));
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    auto const row_selected{soa_parameter_index_ == index};
                    if (detail::editable_table_row_handle(row_selected)) {
                        soa_parameter_index_ = index;
                    }
                    if (ImGui::BeginDragDropSource()) {
                        ImGui::SetDragDropPayload(
                            "SOA_FUNCTION_PARAMETER_ROW", &index, sizeof(index));
                        ImGui::Text(
                            "Move %s",
                            text(declarations()
                                     .parameters.name[declarations().parameters.index(parameter)])
                                .c_str());
                        ImGui::EndDragDropSource();
                    }
                    if (ImGui::BeginDragDropTarget()) {
                        if (auto const* payload{
                                ImGui::AcceptDragDropPayload("SOA_FUNCTION_PARAMETER_ROW")}) {
                            auto const source_index{
                                *static_cast<std::size_t const*>(payload->Data)};
                            if (source_index <
                                    declarations()
                                        .items(declarations().functions.parameters
                                                   [declarations().functions.index(function)])
                                        .size() &&
                                source_index != index) {
                                pending = declarations().soas.clone(*schema);
                                declarations()
                                    .soas.functions[declarations().soas.index(*pending)] = [&] {
                                    auto const original_items{declarations().items(
                                        declarations()
                                            .soas.functions[declarations().soas.index(*pending)])};
                                    auto rows_1{
                                        std::vector(original_items.begin(), original_items.end())};
                                    rows_1[*function_index] = [&] {
                                        auto row_0{declarations().functions.clone(
                                            rows_1[*function_index])};
                                        declarations()
                                            .functions
                                            .parameters[declarations().functions.index(row_0)] =
                                            [&] {
                                                auto const original_items{declarations().items(
                                                    declarations().functions.parameters
                                                        [declarations().functions.index(row_0)])};
                                                auto rows{std::vector(original_items.begin(),
                                                                      original_items.end())};
                                                move_element(rows, source_index, index);
                                                return declarations().range(rows);
                                            }();
                                        return row_0;
                                    }();
                                    return declarations().range(rows_1);
                                }();
                                soa_parameter_index_ = index;
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }

                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-1.0F);
                    auto const name_submitted{
                        ImGui::InputText("##name",
                                         soa_parameter_names_[index].data(),
                                         soa_parameter_names_[index].size(),
                                         ImGuiInputTextFlags_EnterReturnsTrue)};
                    if (name_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                        if (!pending.has_value()) {
                            pending = declarations().soas.clone(*schema);
                        }
                        declarations().soas.functions[declarations().soas.index(*pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .soas.functions[declarations().soas.index(*pending)])};
                            auto rows_3{std::vector(original_items.begin(), original_items.end())};
                            rows_3[*function_index] = [&] {
                                auto row_2{declarations().functions.clone(rows_3[*function_index])};
                                declarations().functions.parameters[declarations().functions.index(
                                    row_2)] = [&] {
                                    auto const original_items{declarations().items(
                                        declarations()
                                            .functions
                                            .parameters[declarations().functions.index(row_2)])};
                                    auto rows_1{
                                        std::vector(original_items.begin(), original_items.end())};
                                    rows_1[index] = [&] {
                                        auto row_0{declarations().parameters.clone(rows_1[index])};
                                        declarations()
                                            .parameters
                                            .name[declarations().parameters.index(row_0)] =
                                            strings().intern(soa_parameter_names_[index].data());
                                        return row_0;
                                    }();
                                    return declarations().range(rows_1);
                                }();
                                return row_2;
                            }();
                            return declarations().range(rows_3);
                        }();
                    }

                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-1.0F);
                    auto const type_submitted{
                        ImGui::InputText("##type",
                                         soa_parameter_types_[index].data(),
                                         soa_parameter_types_[index].size(),
                                         ImGuiInputTextFlags_EnterReturnsTrue)};
                    if (type_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                        if (!pending.has_value()) {
                            pending = declarations().soas.clone(*schema);
                        }
                        declarations().soas.functions[declarations().soas.index(*pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .soas.functions[declarations().soas.index(*pending)])};
                            auto rows_4{std::vector(original_items.begin(), original_items.end())};
                            rows_4[*function_index] = [&] {
                                auto row_3{declarations().functions.clone(rows_4[*function_index])};
                                declarations().functions.parameters[declarations().functions.index(
                                    row_3)] = [&] {
                                    auto const original_items{declarations().items(
                                        declarations()
                                            .functions
                                            .parameters[declarations().functions.index(row_3)])};
                                    auto rows_2{
                                        std::vector(original_items.begin(), original_items.end())};
                                    rows_2[index] = [&] {
                                        auto row_1{declarations().parameters.clone(rows_2[index])};
                                        declarations()
                                            .parameters
                                            .type[declarations().parameters.index(row_1)] = [&] {
                                            auto row_0{declarations().type_refs.clone(
                                                declarations()
                                                    .parameters
                                                    .type[declarations().parameters.index(row_1)])};
                                            declarations()
                                                .type_refs
                                                .name[declarations().type_refs.index(row_0)] =
                                                strings().intern(
                                                    soa_parameter_types_[index].data());
                                            return row_0;
                                        }();
                                        return row_1;
                                    }();
                                    return declarations().range(rows_2);
                                }();
                                return row_3;
                            }();
                            return declarations().range(rows_4);
                        }();
                    }

                    ImGui::TableNextColumn();
                    ImGui::PushID("type-picker");
                    if (auto picked{draw_type_picker(
                            analysis_session_.inputs.workspace.types().module_name(node),
                            analysis_session_.inputs.workspace.types().identity(node))}) {
                        if (!pending.has_value()) {
                            pending = declarations().soas.clone(*schema);
                        }
                        declarations().soas.functions[declarations().soas.index(*pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .soas.functions[declarations().soas.index(*pending)])};
                            auto rows_4{std::vector(original_items.begin(), original_items.end())};
                            rows_4[*function_index] = [&] {
                                auto row_3{declarations().functions.clone(rows_4[*function_index])};
                                declarations().functions.parameters[declarations().functions.index(
                                    row_3)] = [&] {
                                    auto const original_items{declarations().items(
                                        declarations()
                                            .functions
                                            .parameters[declarations().functions.index(row_3)])};
                                    auto rows_2{
                                        std::vector(original_items.begin(), original_items.end())};
                                    rows_2[index] = [&] {
                                        auto row_1{declarations().parameters.clone(rows_2[index])};
                                        declarations()
                                            .parameters
                                            .type[declarations().parameters.index(row_1)] = [&] {
                                            auto row_0{declarations().type_refs.clone(
                                                declarations()
                                                    .parameters
                                                    .type[declarations().parameters.index(row_1)])};
                                            declarations()
                                                .type_refs
                                                .name[declarations().type_refs.index(row_0)] =
                                                strings().intern(*picked);
                                            return row_0;
                                        }();
                                        return row_1;
                                    }();
                                    return declarations().range(rows_2);
                                }();
                                return row_3;
                            }();
                            return declarations().range(rows_4);
                        }();
                    }
                    ImGui::PopID();

                    ImGui::TableNextColumn();
                    auto has_default{
                        declarations()
                            .parameters.default_value[declarations().parameters.index(parameter)]
                            .has_value()};
                    if (ImGui::Checkbox("##has-default", &has_default)) {
                        if (!pending.has_value()) {
                            pending = declarations().soas.clone(*schema);
                        }

                        declarations().soas.functions[declarations().soas.index(*pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .soas.functions[declarations().soas.index(*pending)])};
                            auto rows_3{std::vector(original_items.begin(), original_items.end())};
                            rows_3[*function_index] = [&] {
                                auto row_2{declarations().functions.clone(rows_3[*function_index])};
                                declarations().functions.parameters[declarations().functions.index(
                                    row_2)] = [&] {
                                    auto const original_items{declarations().items(
                                        declarations()
                                            .functions
                                            .parameters[declarations().functions.index(row_2)])};
                                    auto rows_1{
                                        std::vector(original_items.begin(), original_items.end())};
                                    rows_1[index] = [&] {
                                        auto row_0{declarations().parameters.clone(rows_1[index])};
                                        declarations()
                                            .parameters
                                            .default_value[declarations().parameters.index(row_0)] =
                                            has_default ? std::optional{strings().add("{}")}
                                                        : std::nullopt;
                                        return row_0;
                                    }();
                                    return declarations().range(rows_1);
                                }();
                                return row_2;
                            }();
                            return declarations().range(rows_3);
                        }();
                    }
                    if (declarations()
                            .parameters.default_value[declarations().parameters.index(parameter)]
                            .has_value()) {
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(-1.0F);
                        auto const default_submitted{
                            ImGui::InputText("##default",
                                             soa_parameter_defaults_[index].data(),
                                             soa_parameter_defaults_[index].size(),
                                             ImGuiInputTextFlags_EnterReturnsTrue)};
                        if (default_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                            if (!pending.has_value()) {
                                pending = declarations().soas.clone(*schema);
                            }
                            declarations().soas.functions[declarations().soas.index(*pending)] =
                                [&] {
                                    auto const original_items{declarations().items(
                                        declarations()
                                            .soas.functions[declarations().soas.index(*pending)])};
                                    auto rows_3{
                                        std::vector(original_items.begin(), original_items.end())};
                                    rows_3[*function_index] = [&] {
                                        auto row_2{declarations().functions.clone(
                                            rows_3[*function_index])};
                                        declarations()
                                            .functions
                                            .parameters[declarations().functions.index(row_2)] =
                                            [&] {
                                                auto const original_items{declarations().items(
                                                    declarations().functions.parameters
                                                        [declarations().functions.index(row_2)])};
                                                auto rows_1{std::vector(original_items.begin(),
                                                                        original_items.end())};
                                                rows_1[index] = [&] {
                                                    auto row_0{declarations().parameters.clone(
                                                        rows_1[index])};
                                                    declarations().parameters.default_value
                                                        [declarations().parameters.index(row_0)] =
                                                        strings().add(
                                                            soa_parameter_defaults_[index].data());
                                                    return row_0;
                                                }();
                                                return declarations().range(rows_1);
                                            }();
                                        return row_2;
                                    }();
                                    return declarations().range(rows_3);
                                }();
                        }
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::EndDisabled();

            ImGui::PushID(static_cast<int>(*function_index));
            if (detail::section("Body fragments")) {
                ImGui::BeginDisabled(pending.has_value());
                auto const add_body_line{ImGui::Button("+ Body fragment")};
                ImGui::EndDisabled();
                if (add_body_line) {
                    auto replacement{document_->storage().declarations.soas.clone(*schema)};

                    document_->storage()
                        .declarations.soas.functions[document_->storage().declarations.soas.index(
                            replacement)] = [&] {
                        auto const original_items{declarations().items(
                            document_->storage().declarations.soas.functions
                                [document_->storage().declarations.soas.index(replacement)])};
                        auto rows_1{std::vector(original_items.begin(), original_items.end())};
                        rows_1[*function_index] = [&] {
                            auto row_0{declarations().functions.clone(rows_1[*function_index])};
                            declarations()
                                .functions.body_lines[declarations().functions.index(row_0)] = [&] {
                                auto const original_items{declarations().items(
                                    declarations()
                                        .functions
                                        .body_lines[declarations().functions.index(row_0)])};
                                auto rows{
                                    std::vector(original_items.begin(), original_items.end())};
                                rows.push_back(strings().add("// TODO"));
                                return declarations().range(rows);
                            }();
                            return row_0;
                        }();
                        return declarations().range(rows_1);
                    }();
                    auto const new_index{
                        declarations()
                            .items(
                                declarations().functions.body_lines[declarations().functions.index(
                                    declarations().items(
                                        document_->storage().declarations.soas.functions
                                            [document_->storage().declarations.soas.index(
                                                replacement)])[*function_index])])
                            .size() -
                        1};
                    if (apply_document_edit(
                            document_->replace_declaration(*declaration, std::move(replacement)))) {
                        soa_function_body_index_ = new_index;
                        soa_editor_declaration_.reset();
                        ImGui::PopID();
                        return true;
                    }
                }
                ImGui::SameLine();
                auto const body_index{soa_function_body_index_};
                auto const has_body_line{
                    body_index.has_value() &&
                    *body_index <
                        declarations()
                            .items(
                                declarations()
                                    .functions.body_lines[declarations().functions.index(function)])
                            .size()};
                ImGui::BeginDisabled(pending.has_value() || !has_body_line);
                auto const duplicate_body_line{ImGui::Button("Duplicate body")};
                ImGui::EndDisabled();
                if (duplicate_body_line) {
                    auto replacement{document_->storage().declarations.soas.clone(*schema)};

                    document_->storage()
                        .declarations.soas.functions[document_->storage().declarations.soas.index(
                            replacement)] = [&] {
                        auto const original_items{declarations().items(
                            document_->storage().declarations.soas.functions
                                [document_->storage().declarations.soas.index(replacement)])};
                        auto rows_1{std::vector(original_items.begin(), original_items.end())};
                        rows_1[*function_index] = [&] {
                            auto row_0{declarations().functions.clone(rows_1[*function_index])};
                            declarations()
                                .functions.body_lines[declarations().functions.index(row_0)] = [&] {
                                auto const original_items{declarations().items(
                                    declarations()
                                        .functions
                                        .body_lines[declarations().functions.index(row_0)])};
                                auto rows{
                                    std::vector(original_items.begin(), original_items.end())};
                                rows.insert(
                                    rows.begin() + static_cast<std::ptrdiff_t>(*body_index + 1),
                                    declarations().items(
                                        declarations().functions.body_lines
                                            [declarations().functions.index(declarations().items(
                                                document_->storage().declarations.soas.functions
                                                    [document_->storage().declarations.soas.index(
                                                        replacement)])[*function_index])])
                                        [*body_index]);
                                return declarations().range(rows);
                            }();
                            return row_0;
                        }();
                        return declarations().range(rows_1);
                    }();
                    if (apply_document_edit(
                            document_->replace_declaration(*declaration, std::move(replacement)))) {
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
                    auto replacement{document_->storage().declarations.soas.clone(*schema)};

                    document_->storage()
                        .declarations.soas.functions[document_->storage().declarations.soas.index(
                            replacement)] = [&] {
                        auto const original_items{declarations().items(
                            document_->storage().declarations.soas.functions
                                [document_->storage().declarations.soas.index(replacement)])};
                        auto functions{std::vector(original_items.begin(), original_items.end())};
                        functions[*function_index] = [&] {
                            auto function{
                                declarations().functions.clone(functions[*function_index])};
                            declarations()
                                .functions.body_lines[declarations().functions.index(function)] =
                                [&] {
                                    auto const original_items{declarations().items(
                                        declarations()
                                            .functions
                                            .body_lines[declarations().functions.index(function)])};
                                    auto values{
                                        std::vector(original_items.begin(), original_items.end())};
                                    std::swap(values[*body_index], values[*body_index - 1]);
                                    return declarations().range(values);
                                }();
                            return function;
                        }();
                        return declarations().range(functions);
                    }();
                    if (apply_document_edit(
                            document_->replace_declaration(*declaration, std::move(replacement)))) {
                        soa_function_body_index_ = *body_index - 1;
                        soa_editor_declaration_.reset();
                        ImGui::PopID();
                        return true;
                    }
                }
                ImGui::SameLine();
                ImGui::BeginDisabled(
                    pending.has_value() || !has_body_line ||
                    *body_index + 1 >=
                        declarations()
                            .items(
                                declarations()
                                    .functions.body_lines[declarations().functions.index(function)])
                            .size());
                auto const move_body_down{ImGui::Button("Body down")};
                ImGui::EndDisabled();
                if (move_body_down) {
                    auto replacement{document_->storage().declarations.soas.clone(*schema)};

                    document_->storage()
                        .declarations.soas.functions[document_->storage().declarations.soas.index(
                            replacement)] = [&] {
                        auto const original_items{declarations().items(
                            document_->storage().declarations.soas.functions
                                [document_->storage().declarations.soas.index(replacement)])};
                        auto functions{std::vector(original_items.begin(), original_items.end())};
                        functions[*function_index] = [&] {
                            auto function{
                                declarations().functions.clone(functions[*function_index])};
                            declarations()
                                .functions.body_lines[declarations().functions.index(function)] =
                                [&] {
                                    auto const original_items{declarations().items(
                                        declarations()
                                            .functions
                                            .body_lines[declarations().functions.index(function)])};
                                    auto values{
                                        std::vector(original_items.begin(), original_items.end())};
                                    std::swap(values[*body_index], values[*body_index + 1]);
                                    return declarations().range(values);
                                }();
                            return function;
                        }();
                        return declarations().range(functions);
                    }();
                    if (apply_document_edit(
                            document_->replace_declaration(*declaration, std::move(replacement)))) {
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
                    auto replacement{document_->storage().declarations.soas.clone(*schema)};

                    document_->storage()
                        .declarations.soas.functions[document_->storage().declarations.soas.index(
                            replacement)] = [&] {
                        auto const original_items{declarations().items(
                            document_->storage().declarations.soas.functions
                                [document_->storage().declarations.soas.index(replacement)])};
                        auto rows_1{std::vector(original_items.begin(), original_items.end())};
                        rows_1[*function_index] = [&] {
                            auto row_0{declarations().functions.clone(rows_1[*function_index])};
                            declarations()
                                .functions.body_lines[declarations().functions.index(row_0)] = [&] {
                                auto const original_items{declarations().items(
                                    declarations()
                                        .functions
                                        .body_lines[declarations().functions.index(row_0)])};
                                auto rows{
                                    std::vector(original_items.begin(), original_items.end())};
                                rows.erase(rows.begin() + static_cast<std::ptrdiff_t>(*body_index));
                                return declarations().range(rows);
                            }();
                            return row_0;
                        }();
                        return declarations().range(rows_1);
                    }();
                    auto const next_index{
                        declarations()
                                .items(declarations().functions.body_lines
                                           [declarations().functions.index(declarations().items(
                                               document_->storage().declarations.soas.functions
                                                   [document_->storage().declarations.soas.index(
                                                       replacement)])[*function_index])])
                                .empty()
                            ? std::optional<std::size_t>{}
                            : std::optional{std::min(
                                  *body_index,
                                  declarations()
                                          .items(
                                              declarations()
                                                  .functions
                                                  .body_lines[declarations().functions.index(
                                                      declarations().items(
                                                          document_->storage()
                                                              .declarations.soas
                                                              .functions[document_->storage()
                                                                             .declarations.soas
                                                                             .index(replacement)])
                                                          [*function_index])])
                                          .size() -
                                      1)}};
                    if (apply_document_edit(
                            document_->replace_declaration(*declaration, std::move(replacement)))) {
                        soa_function_body_index_ = next_index;
                        soa_editor_declaration_.reset();
                        ImGui::PopID();
                        return true;
                    }
                }

                ImGui::BeginDisabled(pending.has_value());
                if (soa_function_body_lines_.size() ==
                        declarations()
                            .items(
                                declarations()
                                    .functions.body_lines[declarations().functions.index(function)])
                            .size() &&
                    detail::begin_editable_table(
                        "function-body-lines",
                        2,
                        declarations()
                            .items(
                                declarations()
                                    .functions.body_lines[declarations().functions.index(function)])
                            .size())) {
                    detail::editable_table_column("", ImGuiTableColumnFlags_WidthFixed);
                    detail::editable_table_column("C++ fragment");
                    ImGui::TableHeadersRow();
                    for (std::size_t index{};
                         index <
                         declarations()
                             .items(
                                 declarations().functions.body_lines[declarations().functions.index(
                                     function)])
                             .size();
                         ++index) {
                        ImGui::PushID(static_cast<int>(index));
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        auto const row_selected{soa_function_body_index_ == index};
                        if (detail::editable_table_row_handle(row_selected)) {
                            soa_function_body_index_ = index;
                        }
                        if (ImGui::BeginDragDropSource()) {
                            ImGui::SetDragDropPayload(
                                "SOA_FUNCTION_BODY_ROW", &index, sizeof(index));
                            ImGui::TextUnformatted("Move body fragment");
                            ImGui::EndDragDropSource();
                        }
                        if (ImGui::BeginDragDropTarget()) {
                            if (auto const* payload{
                                    ImGui::AcceptDragDropPayload("SOA_FUNCTION_BODY_ROW")}) {
                                auto const source_index{
                                    *static_cast<std::size_t const*>(payload->Data)};
                                if (source_index <
                                        declarations()
                                            .items(declarations().functions.body_lines
                                                       [declarations().functions.index(function)])
                                            .size() &&
                                    source_index != index) {
                                    pending = declarations().soas.clone(*schema);
                                    declarations()
                                        .soas.functions[declarations().soas.index(*pending)] = [&] {
                                        auto const original_items{declarations().items(
                                            declarations().soas.functions[declarations().soas.index(
                                                *pending)])};
                                        auto rows_1{std::vector(original_items.begin(),
                                                                original_items.end())};
                                        rows_1[*function_index] = [&] {
                                            auto row_0{declarations().functions.clone(
                                                rows_1[*function_index])};
                                            declarations().functions.body_lines
                                                [declarations().functions.index(row_0)] = [&] {
                                                auto const original_items{declarations().items(
                                                    declarations().functions.body_lines
                                                        [declarations().functions.index(row_0)])};
                                                auto rows{std::vector(original_items.begin(),
                                                                      original_items.end())};
                                                move_element(rows, source_index, index);
                                                return declarations().range(rows);
                                            }();
                                            return row_0;
                                        }();
                                        return declarations().range(rows_1);
                                    }();
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
                                pending = declarations().soas.clone(*schema);
                            }
                            declarations().soas.functions[declarations().soas.index(*pending)] =
                                [&] {
                                    auto const original_items{declarations().items(
                                        declarations()
                                            .soas.functions[declarations().soas.index(*pending)])};
                                    auto rows_2{
                                        std::vector(original_items.begin(), original_items.end())};
                                    rows_2[*function_index] = [&] {
                                        auto row_1{declarations().functions.clone(
                                            rows_2[*function_index])};
                                        declarations()
                                            .functions
                                            .body_lines[declarations().functions.index(row_1)] =
                                            [&] {
                                                auto const original_items{declarations().items(
                                                    declarations().functions.body_lines
                                                        [declarations().functions.index(row_1)])};
                                                auto rows_0{std::vector(original_items.begin(),
                                                                        original_items.end())};
                                                rows_0[index] =
                                                    strings().add(soa_function_body_lines_[index]);
                                                return declarations().range(rows_0);
                                            }();
                                        return row_1;
                                    }();
                                    return declarations().range(rows_2);
                                }();
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndTable();
                }
                ImGui::EndDisabled();
            }

            if (detail::section("Declared dependencies")) {
                ImGui::SetNextItemWidth(220.0F);
                ImGui::BeginDisabled(pending.has_value());
                input_text("Dependency key", soa_new_function_dependency_);
                ImGui::SameLine();
                auto const add_dependency{ImGui::Button("+ Dependency")};
                ImGui::EndDisabled();
                if (add_dependency) {
                    if (soa_new_function_dependency_.find_first_not_of(" \t\r\n") ==
                        std::string::npos) {
                        schema_edit_message_ = "SoA function dependency key cannot be empty.";
                        ImGui::PopID();
                        return false;
                    }
                    auto replacement{document_->storage().declarations.soas.clone(*schema)};

                    document_->storage()
                        .declarations.soas.functions[document_->storage().declarations.soas.index(
                            replacement)] = [&] {
                        auto const original_items{declarations().items(
                            document_->storage().declarations.soas.functions
                                [document_->storage().declarations.soas.index(replacement)])};
                        auto rows_1{std::vector(original_items.begin(), original_items.end())};
                        rows_1[*function_index] = [&] {
                            auto row_0{declarations().functions.clone(rows_1[*function_index])};
                            declarations()
                                .functions.dependencies[declarations().functions.index(row_0)] =
                                [&] {
                                    auto const original_items{declarations().items(
                                        declarations()
                                            .functions
                                            .dependencies[declarations().functions.index(row_0)])};
                                    auto rows{
                                        std::vector(original_items.begin(), original_items.end())};
                                    rows.push_back(strings().intern(soa_new_function_dependency_));
                                    return declarations().range(rows);
                                }();
                            return row_0;
                        }();
                        return declarations().range(rows_1);
                    }();
                    auto const new_index{
                        declarations()
                            .items(declarations()
                                       .functions.dependencies[declarations().functions.index(
                                           declarations().items(
                                               document_->storage().declarations.soas.functions
                                                   [document_->storage().declarations.soas.index(
                                                       replacement)])[*function_index])])
                            .size() -
                        1};
                    if (apply_document_edit(
                            document_->replace_declaration(*declaration, std::move(replacement)))) {
                        soa_new_function_dependency_.clear();
                        soa_function_dependency_index_ = new_index;
                        soa_editor_declaration_.reset();
                        ImGui::PopID();
                        return true;
                    }
                }
                auto const dependency_index{soa_function_dependency_index_};
                auto const has_dependency{
                    dependency_index.has_value() &&
                    *dependency_index <
                        declarations()
                            .items(declarations()
                                       .functions
                                       .dependencies[declarations().functions.index(function)])
                            .size()};
                ImGui::BeginDisabled(pending.has_value() || !has_dependency);
                auto const duplicate_dependency{ImGui::Button("Duplicate dependency")};
                ImGui::EndDisabled();
                if (duplicate_dependency) {
                    auto replacement{document_->storage().declarations.soas.clone(*schema)};

                    document_->storage()
                        .declarations.soas.functions[document_->storage().declarations.soas.index(
                            replacement)] = [&] {
                        auto const original_items{declarations().items(
                            document_->storage().declarations.soas.functions
                                [document_->storage().declarations.soas.index(replacement)])};
                        auto rows_1{std::vector(original_items.begin(), original_items.end())};
                        rows_1[*function_index] = [&] {
                            auto row_0{declarations().functions.clone(rows_1[*function_index])};
                            declarations().functions.dependencies[declarations().functions.index(
                                row_0)] = [&] {
                                auto const original_items{declarations().items(
                                    declarations()
                                        .functions
                                        .dependencies[declarations().functions.index(row_0)])};
                                auto rows{
                                    std::vector(original_items.begin(), original_items.end())};
                                rows.insert(
                                    rows.begin() +
                                        static_cast<std::ptrdiff_t>(*dependency_index + 1),
                                    declarations().items(
                                        declarations().functions.dependencies
                                            [declarations().functions.index(declarations().items(
                                                document_->storage().declarations.soas.functions
                                                    [document_->storage().declarations.soas.index(
                                                        replacement)])[*function_index])])
                                        [*dependency_index]);
                                return declarations().range(rows);
                            }();
                            return row_0;
                        }();
                        return declarations().range(rows_1);
                    }();
                    if (apply_document_edit(
                            document_->replace_declaration(*declaration, std::move(replacement)))) {
                        soa_function_dependency_index_ = *dependency_index + 1;
                        soa_editor_declaration_.reset();
                        ImGui::PopID();
                        return true;
                    }
                }
                ImGui::SameLine();
                ImGui::BeginDisabled(pending.has_value() || !has_dependency ||
                                     *dependency_index == 0);
                auto const move_dependency_up{ImGui::Button("Dependency up")};
                ImGui::EndDisabled();
                if (move_dependency_up) {
                    auto replacement{document_->storage().declarations.soas.clone(*schema)};

                    document_->storage()
                        .declarations.soas.functions[document_->storage().declarations.soas.index(
                            replacement)] = [&] {
                        auto const original_items{declarations().items(
                            document_->storage().declarations.soas.functions
                                [document_->storage().declarations.soas.index(replacement)])};
                        auto functions{std::vector(original_items.begin(), original_items.end())};
                        functions[*function_index] = [&] {
                            auto function{
                                declarations().functions.clone(functions[*function_index])};
                            declarations().functions.dependencies[declarations().functions.index(
                                function)] = [&] {
                                auto const original_items{declarations().items(
                                    declarations()
                                        .functions
                                        .dependencies[declarations().functions.index(function)])};
                                auto values{
                                    std::vector(original_items.begin(), original_items.end())};
                                std::swap(values[*dependency_index], values[*dependency_index - 1]);
                                return declarations().range(values);
                            }();
                            return function;
                        }();
                        return declarations().range(functions);
                    }();
                    if (apply_document_edit(
                            document_->replace_declaration(*declaration, std::move(replacement)))) {
                        soa_function_dependency_index_ = *dependency_index - 1;
                        soa_editor_declaration_.reset();
                        ImGui::PopID();
                        return true;
                    }
                }
                ImGui::SameLine();
                ImGui::BeginDisabled(
                    pending.has_value() || !has_dependency ||
                    *dependency_index + 1 >=
                        declarations()
                            .items(declarations()
                                       .functions
                                       .dependencies[declarations().functions.index(function)])
                            .size());
                auto const move_dependency_down{ImGui::Button("Dependency down")};
                ImGui::EndDisabled();
                if (move_dependency_down) {
                    auto replacement{document_->storage().declarations.soas.clone(*schema)};

                    document_->storage()
                        .declarations.soas.functions[document_->storage().declarations.soas.index(
                            replacement)] = [&] {
                        auto const original_items{declarations().items(
                            document_->storage().declarations.soas.functions
                                [document_->storage().declarations.soas.index(replacement)])};
                        auto functions{std::vector(original_items.begin(), original_items.end())};
                        functions[*function_index] = [&] {
                            auto function{
                                declarations().functions.clone(functions[*function_index])};
                            declarations().functions.dependencies[declarations().functions.index(
                                function)] = [&] {
                                auto const original_items{declarations().items(
                                    declarations()
                                        .functions
                                        .dependencies[declarations().functions.index(function)])};
                                auto values{
                                    std::vector(original_items.begin(), original_items.end())};
                                std::swap(values[*dependency_index], values[*dependency_index + 1]);
                                return declarations().range(values);
                            }();
                            return function;
                        }();
                        return declarations().range(functions);
                    }();
                    if (apply_document_edit(
                            document_->replace_declaration(*declaration, std::move(replacement)))) {
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
                    auto replacement{document_->storage().declarations.soas.clone(*schema)};

                    document_->storage()
                        .declarations.soas.functions[document_->storage().declarations.soas.index(
                            replacement)] = [&] {
                        auto const original_items{declarations().items(
                            document_->storage().declarations.soas.functions
                                [document_->storage().declarations.soas.index(replacement)])};
                        auto rows_1{std::vector(original_items.begin(), original_items.end())};
                        rows_1[*function_index] = [&] {
                            auto row_0{declarations().functions.clone(rows_1[*function_index])};
                            declarations()
                                .functions.dependencies[declarations().functions.index(row_0)] =
                                [&] {
                                    auto const original_items{declarations().items(
                                        declarations()
                                            .functions
                                            .dependencies[declarations().functions.index(row_0)])};
                                    auto rows{
                                        std::vector(original_items.begin(), original_items.end())};
                                    rows.erase(rows.begin() +
                                               static_cast<std::ptrdiff_t>(*dependency_index));
                                    return declarations().range(rows);
                                }();
                            return row_0;
                        }();
                        return declarations().range(rows_1);
                    }();
                    auto const next_index{
                        declarations()
                                .items(declarations().functions.dependencies
                                           [declarations().functions.index(declarations().items(
                                               document_->storage().declarations.soas.functions
                                                   [document_->storage().declarations.soas.index(
                                                       replacement)])[*function_index])])
                                .empty()
                            ? std::optional<std::size_t>{}
                            : std::optional{std::min(
                                  *dependency_index,
                                  declarations()
                                          .items(
                                              declarations()
                                                  .functions
                                                  .dependencies[declarations().functions.index(
                                                      declarations().items(
                                                          document_->storage()
                                                              .declarations.soas
                                                              .functions[document_->storage()
                                                                             .declarations.soas
                                                                             .index(replacement)])
                                                          [*function_index])])
                                          .size() -
                                      1)}};
                    if (apply_document_edit(
                            document_->replace_declaration(*declaration, std::move(replacement)))) {
                        soa_function_dependency_index_ = next_index;
                        soa_editor_declaration_.reset();
                        ImGui::PopID();
                        return true;
                    }
                }

                ImGui::BeginDisabled(pending.has_value());
                if (soa_function_dependencies_.size() ==
                        declarations()
                            .items(declarations()
                                       .functions
                                       .dependencies[declarations().functions.index(function)])
                            .size() &&
                    detail::begin_editable_table(
                        "function-dependencies",
                        2,
                        declarations()
                            .items(declarations()
                                       .functions
                                       .dependencies[declarations().functions.index(function)])
                            .size())) {
                    detail::editable_table_column("", ImGuiTableColumnFlags_WidthFixed);
                    detail::editable_table_column("Registered dependency key");
                    ImGui::TableHeadersRow();
                    for (std::size_t index{};
                         index <
                         declarations()
                             .items(declarations()
                                        .functions
                                        .dependencies[declarations().functions.index(function)])
                             .size();
                         ++index) {
                        ImGui::PushID(static_cast<int>(index));
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        auto const row_selected{soa_function_dependency_index_ == index};
                        if (detail::editable_table_row_handle(row_selected)) {
                            soa_function_dependency_index_ = index;
                        }
                        if (ImGui::BeginDragDropSource()) {
                            ImGui::SetDragDropPayload(
                                "SOA_FUNCTION_DEPENDENCY_ROW", &index, sizeof(index));
                            ImGui::Text(
                                "Move %s",
                                text(declarations().items(
                                         declarations()
                                             .functions.dependencies[declarations().functions.index(
                                                 function)])[index])
                                    .c_str());
                            ImGui::EndDragDropSource();
                        }
                        if (ImGui::BeginDragDropTarget()) {
                            if (auto const* payload{
                                    ImGui::AcceptDragDropPayload("SOA_FUNCTION_DEPENDENCY_ROW")}) {
                                auto const source_index{
                                    *static_cast<std::size_t const*>(payload->Data)};
                                if (source_index <
                                        declarations()
                                            .items(declarations().functions.dependencies
                                                       [declarations().functions.index(function)])
                                            .size() &&
                                    source_index != index) {
                                    pending = declarations().soas.clone(*schema);
                                    declarations()
                                        .soas.functions[declarations().soas.index(*pending)] = [&] {
                                        auto const original_items{declarations().items(
                                            declarations().soas.functions[declarations().soas.index(
                                                *pending)])};
                                        auto rows_1{std::vector(original_items.begin(),
                                                                original_items.end())};
                                        rows_1[*function_index] = [&] {
                                            auto row_0{declarations().functions.clone(
                                                rows_1[*function_index])};
                                            declarations().functions.dependencies
                                                [declarations().functions.index(row_0)] = [&] {
                                                auto const original_items{declarations().items(
                                                    declarations().functions.dependencies
                                                        [declarations().functions.index(row_0)])};
                                                auto rows{std::vector(original_items.begin(),
                                                                      original_items.end())};
                                                move_element(rows, source_index, index);
                                                return declarations().range(rows);
                                            }();
                                            return row_0;
                                        }();
                                        return declarations().range(rows_1);
                                    }();
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
                                pending = declarations().soas.clone(*schema);
                            }
                            declarations()
                                .soas.functions[declarations().soas.index(*pending)] = [&] {
                                auto const original_items{declarations().items(
                                    declarations()
                                        .soas.functions[declarations().soas.index(*pending)])};
                                auto rows_2{
                                    std::vector(original_items.begin(), original_items.end())};
                                rows_2[*function_index] = [&] {
                                    auto row_1{
                                        declarations().functions.clone(rows_2[*function_index])};
                                    declarations()
                                        .functions
                                        .dependencies[declarations().functions.index(row_1)] = [&] {
                                        auto const original_items{declarations().items(
                                            declarations().functions.dependencies
                                                [declarations().functions.index(row_1)])};
                                        auto rows_0{std::vector(original_items.begin(),
                                                                original_items.end())};
                                        rows_0[index] =
                                            strings().intern(soa_function_dependencies_[index]);
                                        return declarations().range(rows_0);
                                    }();
                                    return row_1;
                                }();
                                return declarations().range(rows_2);
                            }();
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndTable();
                }
                ImGui::EndDisabled();
            }

            if (detail::section("Advanced signature")) {
                auto has_trailing_return{
                    declarations()
                        .functions.trailing_return_type[declarations().functions.index(function)]
                        .has_value()};
                ImGui::BeginDisabled(pending.has_value());
                auto const trailing_toggled{
                    ImGui::Checkbox("Use trailing return type", &has_trailing_return)};
                ImGui::EndDisabled();
                if (trailing_toggled) {
                    auto replacement{document_->storage().declarations.soas.clone(*schema)};

                    if (has_trailing_return) {
                        document_->storage().declarations.soas.functions
                            [document_->storage().declarations.soas.index(replacement)] = [&] {
                            auto const original_items{declarations().items(
                                document_->storage().declarations.soas.functions
                                    [document_->storage().declarations.soas.index(replacement)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[*function_index] = [&] {
                                auto row_0{declarations().functions.clone(rows_1[*function_index])};
                                declarations().functions.trailing_return_type[declarations().functions.index(row_0)] =
                                                declarations().type_refs.name[declarations().type_refs.index(declarations().functions.return_type[declarations().functions.index(declarations().items(
                                                                            document_->storage().declarations.soas.functions[document_->storage().declarations.soas.index(replacement)])
                                                                            [*function_index])])] == strings().intern("auto")
                                                    ? [&] { auto const new_type_refs{declarations().type_refs.append()};
declarations().type_refs.name[declarations().type_refs.index(new_type_refs)] = strings().intern("void");
return new_type_refs; }()
                                                    : declarations().functions.return_type[declarations().functions.index(declarations().items(
                                                                  document_->storage().declarations.soas.functions[document_->storage().declarations.soas.index(replacement)])[*function_index])];
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        document_->storage().declarations.soas.functions
                            [document_->storage().declarations.soas.index(replacement)] = [&] {
                            auto const original_items{declarations().items(
                                document_->storage().declarations.soas.functions
                                    [document_->storage().declarations.soas.index(replacement)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[*function_index] = [&] {
                                auto row_0{declarations().functions.clone(rows_1[*function_index])};
                                declarations().functions.return_type[declarations().functions.index(
                                    row_0)] = [&] {
                                    auto const new_type_refs{declarations().type_refs.append()};
                                    declarations().type_refs.name[declarations().type_refs.index(
                                        new_type_refs)] = strings().intern("auto");
                                    declarations().type_refs.suffix[declarations().type_refs.index(
                                        new_type_refs)] = {};
                                    declarations().type_refs.nested[declarations().type_refs.index(
                                        new_type_refs)] = std::nullopt;
                                    return new_type_refs;
                                }();
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                    } else {
                        document_->storage().declarations.soas.functions
                            [document_->storage().declarations.soas.index(replacement)] = [&] {
                            auto const original_items{declarations().items(
                                document_->storage().declarations.soas.functions
                                    [document_->storage().declarations.soas.index(replacement)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[*function_index] = [&] {
                                auto row_0{declarations().functions.clone(rows_1[*function_index])};
                                declarations()
                                    .functions.return_type[declarations().functions.index(row_0)] =
                                    *declarations().functions.trailing_return_type
                                         [declarations().functions.index(declarations().items(
                                             document_->storage().declarations.soas.functions
                                                 [document_->storage().declarations.soas.index(
                                                     replacement)])[*function_index])];
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        document_->storage().declarations.soas.functions
                            [document_->storage().declarations.soas.index(replacement)] = [&] {
                            auto const original_items{declarations().items(
                                document_->storage().declarations.soas.functions
                                    [document_->storage().declarations.soas.index(replacement)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[*function_index] = [&] {
                                auto row_0{declarations().functions.clone(rows_1[*function_index])};
                                declarations()
                                    .functions
                                    .trailing_return_type[declarations().functions.index(row_0)]
                                    .reset();
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                    }
                    if (apply_document_edit(
                            document_->replace_declaration(*declaration, std::move(replacement)))) {
                        soa_editor_declaration_.reset();
                        ImGui::PopID();
                        return true;
                    }
                }
                if (declarations()
                        .functions.trailing_return_type[declarations().functions.index(function)]
                        .has_value()) {
                    ImGui::SetNextItemWidth(220.0F);
                    auto const submitted{input_text("Trailing return type",
                                                    soa_function_trailing_return_type_,
                                                    ImGuiInputTextFlags_EnterReturnsTrue)};
                    if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                        if (!pending.has_value()) {
                            pending = declarations().soas.clone(*schema);
                        }
                        declarations().soas.functions[declarations().soas.index(*pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .soas.functions[declarations().soas.index(*pending)])};
                            auto rows_2{std::vector(original_items.begin(), original_items.end())};
                            rows_2[*function_index] = [&] {
                                auto row_1{declarations().functions.clone(rows_2[*function_index])};
                                (*declarations().functions.trailing_return_type
                                      [declarations().functions.index(row_1)]) = [&] {
                                    auto row_0{declarations().type_refs.clone(
                                        (*declarations().functions.trailing_return_type
                                              [declarations().functions.index(row_1)]))};
                                    declarations()
                                        .type_refs.name[declarations().type_refs.index(row_0)] =
                                        strings().intern(soa_function_trailing_return_type_);
                                    return row_0;
                                }();
                                return row_1;
                            }();
                            return declarations().range(rows_2);
                        }();
                    }
                    ImGui::SameLine();
                    ImGui::PushID("trailing-return-picker");
                    if (auto picked{draw_type_picker(
                            analysis_session_.inputs.workspace.types().module_name(node),
                            analysis_session_.inputs.workspace.types().identity(node))}) {
                        if (!pending.has_value()) {
                            pending = declarations().soas.clone(*schema);
                        }
                        declarations().soas.functions[declarations().soas.index(*pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .soas.functions[declarations().soas.index(*pending)])};
                            auto rows_2{std::vector(original_items.begin(), original_items.end())};
                            rows_2[*function_index] = [&] {
                                auto row_1{declarations().functions.clone(rows_2[*function_index])};
                                (*declarations().functions.trailing_return_type
                                      [declarations().functions.index(row_1)]) = [&] {
                                    auto row_0{declarations().type_refs.clone(
                                        (*declarations().functions.trailing_return_type
                                              [declarations().functions.index(row_1)]))};
                                    declarations()
                                        .type_refs.name[declarations().type_refs.index(row_0)] =
                                        strings().intern(*picked);
                                    return row_0;
                                }();
                                return row_1;
                            }();
                            return declarations().range(rows_2);
                        }();
                    }
                    ImGui::PopID();
                }

                auto has_template_parameters{
                    declarations()
                        .functions.template_parameters[declarations().functions.index(function)]
                        .has_value()};
                ImGui::BeginDisabled(pending.has_value());
                auto const template_toggled{
                    ImGui::Checkbox("Template parameters", &has_template_parameters)};
                ImGui::EndDisabled();
                if (template_toggled) {
                    auto replacement{document_->storage().declarations.soas.clone(*schema)};
                    document_->storage()
                        .declarations.soas
                        .functions[document_->storage().declarations.soas.index(replacement)] =
                        [&] {
                            auto const original_items{declarations().items(
                                document_->storage().declarations.soas.functions
                                    [document_->storage().declarations.soas.index(replacement)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[*function_index] = [&] {
                                auto row_0{declarations().functions.clone(rows_1[*function_index])};
                                declarations()
                                    .functions
                                    .template_parameters[declarations().functions.index(row_0)] =
                                    has_template_parameters
                                        ? std::optional{strings().add("typename T")}
                                        : std::nullopt;
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                    if (apply_document_edit(
                            document_->replace_declaration(*declaration, std::move(replacement)))) {
                        soa_editor_declaration_.reset();
                        ImGui::PopID();
                        return true;
                    }
                }
                if (declarations()
                        .functions.template_parameters[declarations().functions.index(function)]
                        .has_value()) {
                    ImGui::SetNextItemWidth(-1.0F);
                    auto const submitted{input_text("Template parameter text",
                                                    soa_function_template_parameters_,
                                                    ImGuiInputTextFlags_EnterReturnsTrue)};
                    if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                        if (!pending.has_value()) {
                            pending = declarations().soas.clone(*schema);
                        }
                        declarations().soas.functions[declarations().soas.index(*pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .soas.functions[declarations().soas.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[*function_index] = [&] {
                                auto row_0{declarations().functions.clone(rows_1[*function_index])};
                                declarations()
                                    .functions
                                    .template_parameters[declarations().functions.index(row_0)] =
                                    strings().add(soa_function_template_parameters_);
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                    }
                }

                auto has_requires_clause{
                    declarations()
                        .functions.requires_clause[declarations().functions.index(function)]
                        .has_value()};
                ImGui::BeginDisabled(pending.has_value());
                auto const requires_toggled{
                    ImGui::Checkbox("Requires clause", &has_requires_clause)};
                ImGui::EndDisabled();
                if (requires_toggled) {
                    auto replacement{document_->storage().declarations.soas.clone(*schema)};
                    document_->storage()
                        .declarations.soas.functions[document_->storage().declarations.soas.index(
                            replacement)] = [&] {
                        auto const original_items{declarations().items(
                            document_->storage().declarations.soas.functions
                                [document_->storage().declarations.soas.index(replacement)])};
                        auto rows_1{std::vector(original_items.begin(), original_items.end())};
                        rows_1[*function_index] = [&] {
                            auto row_0{declarations().functions.clone(rows_1[*function_index])};
                            declarations()
                                .functions.requires_clause[declarations().functions.index(row_0)] =
                                has_requires_clause ? std::optional{strings().add("true")}
                                                    : std::nullopt;
                            return row_0;
                        }();
                        return declarations().range(rows_1);
                    }();
                    if (apply_document_edit(
                            document_->replace_declaration(*declaration, std::move(replacement)))) {
                        soa_editor_declaration_.reset();
                        ImGui::PopID();
                        return true;
                    }
                }
                if (declarations()
                        .functions.requires_clause[declarations().functions.index(function)]
                        .has_value()) {
                    ImGui::SetNextItemWidth(-1.0F);
                    auto const submitted{input_text("Requires expression",
                                                    soa_function_requires_clause_,
                                                    ImGuiInputTextFlags_EnterReturnsTrue)};
                    if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                        if (!pending.has_value()) {
                            pending = declarations().soas.clone(*schema);
                        }
                        declarations().soas.functions[declarations().soas.index(*pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .soas.functions[declarations().soas.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[*function_index] = [&] {
                                auto row_0{declarations().functions.clone(rows_1[*function_index])};
                                declarations()
                                    .functions
                                    .requires_clause[declarations().functions.index(row_0)] =
                                    strings().add(soa_function_requires_clause_);
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                    }
                }
            }
            ImGui::PopID();
        }
    }

    if (detail::section("View types")) {
        auto explicit_view_name{
            document_->storage()
                .declarations.soas.view_name[document_->storage().declarations.soas.index(*schema)]
                .has_value()};
        if (ImGui::Checkbox("Explicit mutable view type", &explicit_view_name)) {
            auto replacement{document_->storage().declarations.soas.clone(*schema)};
            document_->storage()
                .declarations.soas
                .view_name[document_->storage().declarations.soas.index(replacement)] =
                store_identifier(
                    explicit_view_name
                        ? std::optional{text(document_->storage()
                                                 .declarations.soas
                                                 .name[document_->storage().declarations.soas.index(
                                                     *schema)]) +
                                        "View"}
                        : std::nullopt);
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
                soa_editor_declaration_.reset();
                return true;
            }
        }
        if (document_->storage()
                .declarations.soas.view_name[document_->storage().declarations.soas.index(*schema)]
                .has_value()) {
            detail::prepare_property_input("Mutable view type");
            auto const submitted{ImGui::InputText("##Mutable view type",
                                                  soa_view_name_.data(),
                                                  soa_view_name_.size(),
                                                  ImGuiInputTextFlags_EnterReturnsTrue)};
            if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                if (soa_view_name_.front() == '\0') {
                    schema_edit_message_ =
                        "Explicit mutable view type cannot be empty; disable it to "
                        "use the derived name.";
                    return false;
                }
                auto replacement{document_->storage().declarations.soas.clone(*schema)};
                document_->storage()
                    .declarations.soas
                    .view_name[document_->storage().declarations.soas.index(replacement)] =
                    store_identifier(soa_view_name_.data());
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
                    soa_editor_declaration_.reset();
                    return true;
                }
            }
        } else {
            ImGui::TextDisabled(
                "Derived mutable view: %sView",
                text(document_->storage()
                         .declarations.soas
                         .name[document_->storage().declarations.soas.index(*schema)])
                    .c_str());
        }

        auto explicit_const_view_name{
            document_->storage()
                .declarations.soas
                .const_view_name[document_->storage().declarations.soas.index(*schema)]
                .has_value()};
        if (ImGui::Checkbox("Explicit const view type", &explicit_const_view_name)) {
            auto replacement{document_->storage().declarations.soas.clone(*schema)};
            document_->storage()
                .declarations.soas
                .const_view_name[document_->storage().declarations.soas.index(replacement)] =
                store_identifier(
                    explicit_const_view_name
                        ? std::optional{text(document_->storage()
                                                 .declarations.soas
                                                 .name[document_->storage().declarations.soas.index(
                                                     *schema)]) +
                                        "ConstView"}
                        : std::nullopt);
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
                soa_editor_declaration_.reset();
                return true;
            }
        }
        if (document_->storage()
                .declarations.soas
                .const_view_name[document_->storage().declarations.soas.index(*schema)]
                .has_value()) {
            detail::prepare_property_input("Const view type");
            auto const submitted{ImGui::InputText("##Const view type",
                                                  soa_const_view_name_.data(),
                                                  soa_const_view_name_.size(),
                                                  ImGuiInputTextFlags_EnterReturnsTrue)};
            if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                if (soa_const_view_name_.front() == '\0') {
                    schema_edit_message_ = "Explicit const view type cannot be empty; disable it "
                                           "to use the derived name.";
                    return false;
                }
                auto replacement{document_->storage().declarations.soas.clone(*schema)};
                document_->storage()
                    .declarations.soas
                    .const_view_name[document_->storage().declarations.soas.index(replacement)] =
                    store_identifier(soa_const_view_name_.data());
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
                    soa_editor_declaration_.reset();
                    return true;
                }
            }
        } else {
            ImGui::TextDisabled(
                "Derived const view: %sConstView",
                text(document_->storage()
                         .declarations.soas
                         .name[document_->storage().declarations.soas.index(*schema)])
                    .c_str());
        }
    }

    if (detail::section("Fixed layout")) {
        if (!document_->storage()
                 .declarations.soas.fixed[document_->storage().declarations.soas.index(*schema)]
                 .has_value()) {
            if (ImGui::Button("Enable fixed layout")) {
                auto storage_name{document_->unique_soa_generated_type_name(
                    *declaration,
                    text(document_->storage()
                             .declarations.soas
                             .name[document_->storage().declarations.soas.index(*schema)]) +
                        "FixedStorage")};
                if (!storage_name.has_value()) {
                    schema_edit_message_ = storage_name.error().message;
                    return false;
                }
                auto replacement{document_->storage().declarations.soas.clone(*schema)};
                document_->storage()
                    .declarations.soas
                    .fixed[document_->storage().declarations.soas.index(replacement)] = [&] {
                    auto const new_fixed_soas{declarations().fixed_soas.append()};
                    declarations()
                        .fixed_soas.storage_name[declarations().fixed_soas.index(new_fixed_soas)] =
                        strings().intern(std::move(*storage_name));
                    declarations().fixed_soas.containers[declarations().fixed_soas.index(
                        new_fixed_soas)] = {};
                    return new_fixed_soas;
                }();
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
                    soa_editor_declaration_.reset();
                    return true;
                }
            }
        } else {
            detail::prepare_property_input("Storage type");
            auto const storage_submitted{ImGui::InputText("##Storage type",
                                                          soa_fixed_storage_name_.data(),
                                                          soa_fixed_storage_name_.size(),
                                                          ImGuiInputTextFlags_EnterReturnsTrue)};
            if (storage_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                if (soa_fixed_storage_name_.front() == '\0') {
                    schema_edit_message_ = "Fixed-layout storage type cannot be empty.";
                    return false;
                }
                auto replacement{document_->storage().declarations.soas.clone(*schema)};
                (*document_->storage()
                      .declarations.soas
                      .fixed[document_->storage().declarations.soas.index(replacement)]) = [&] {
                    auto row_0{declarations().fixed_soas.clone(
                        (*document_->storage()
                              .declarations.soas
                              .fixed[document_->storage().declarations.soas.index(replacement)]))};
                    declarations().fixed_soas.storage_name[declarations().fixed_soas.index(row_0)] =
                        strings().intern(soa_fixed_storage_name_.data());
                    return row_0;
                }();
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
                    soa_editor_declaration_.reset();
                    return true;
                }
            }

            if (ImGui::Button("+ Fixed container")) {
                auto container_name{document_->unique_soa_generated_type_name(
                    *declaration,
                    text(document_->storage()
                             .declarations.soas
                             .name[document_->storage().declarations.soas.index(*schema)]) +
                        "Fixed")};
                if (!container_name.has_value()) {
                    schema_edit_message_ = container_name.error().message;
                    return false;
                }
                auto replacement{document_->storage().declarations.soas.clone(*schema)};
                (*document_->storage()
                      .declarations.soas
                      .fixed[document_->storage().declarations.soas.index(replacement)]) = [&] {
                    auto row_0{declarations().fixed_soas.clone(
                        (*document_->storage()
                              .declarations.soas
                              .fixed[document_->storage().declarations.soas.index(replacement)]))};
                    declarations()
                        .fixed_soas.containers[declarations().fixed_soas.index(row_0)] = [&] {
                        auto const original_items{declarations().items(
                            declarations()
                                .fixed_soas.containers[declarations().fixed_soas.index(row_0)])};
                        auto rows{std::vector(original_items.begin(), original_items.end())};
                        rows.push_back(strings().intern(std::move(*container_name)));
                        return declarations().range(rows);
                    }();
                    return row_0;
                }();
                auto const new_index{
                    declarations()
                        .items(declarations().fixed_soas.containers[declarations().fixed_soas.index(
                            (*document_->storage().declarations.soas.fixed
                                  [document_->storage().declarations.soas.index(replacement)]))])
                        .size() -
                    1};
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
                    soa_fixed_container_index_ = new_index;
                    soa_editor_declaration_.reset();
                    return true;
                }
            }
            ImGui::SameLine();
            auto const container_index{soa_fixed_container_index_};
            auto const has_container{
                container_index.has_value() &&
                *container_index <
                    declarations()
                        .items(declarations().fixed_soas.containers[declarations().fixed_soas.index(
                            (*document_->storage().declarations.soas.fixed
                                  [document_->storage().declarations.soas.index(*schema)]))])
                        .size()};
            ImGui::BeginDisabled(!has_container);
            if (ImGui::Button("Duplicate fixed container")) {
                auto container_name{document_->unique_soa_generated_type_name(
                    *declaration,
                    text(declarations().items(
                        declarations().fixed_soas.containers[declarations().fixed_soas.index(
                            *document_->storage().declarations.soas.fixed
                                 [document_->storage().declarations.soas.index(
                                     *schema)])])[*container_index]) +
                        "_copy")};
                if (!container_name.has_value()) {
                    schema_edit_message_ = container_name.error().message;
                } else {
                    auto replacement{document_->storage().declarations.soas.clone(*schema)};
                    (*document_->storage()
                          .declarations.soas
                          .fixed[document_->storage().declarations.soas.index(replacement)]) = [&] {
                        auto row_0{declarations().fixed_soas.clone(
                            (*document_->storage().declarations.soas.fixed
                                  [document_->storage().declarations.soas.index(replacement)]))};
                        declarations()
                            .fixed_soas.containers[declarations().fixed_soas.index(row_0)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .fixed_soas
                                    .containers[declarations().fixed_soas.index(row_0)])};
                            auto rows{std::vector(original_items.begin(), original_items.end())};
                            rows.insert(rows.begin() +
                                            static_cast<std::ptrdiff_t>(*container_index + 1),
                                        strings().intern(*container_name));
                            return declarations().range(rows);
                        }();
                        return row_0;
                    }();
                    if (apply_document_edit(
                            document_->replace_declaration(*declaration, std::move(replacement)))) {
                        soa_fixed_container_index_ = *container_index + 1;
                        soa_editor_declaration_.reset();
                        return true;
                    }
                }
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(!has_container || *container_index == 0);
            if (ImGui::Button("Fixed container up")) {
                auto replacement{document_->storage().declarations.soas.clone(*schema)};
                (*document_->storage()
                      .declarations.soas
                      .fixed[document_->storage().declarations.soas.index(replacement)]) = [&] {
                    auto row_0{declarations().fixed_soas.clone(
                        (*document_->storage()
                              .declarations.soas
                              .fixed[document_->storage().declarations.soas.index(replacement)]))};
                    declarations()
                        .fixed_soas.containers[declarations().fixed_soas.index(row_0)] = [&] {
                        auto const original_items{declarations().items(
                            declarations()
                                .fixed_soas.containers[declarations().fixed_soas.index(row_0)])};
                        auto rows{std::vector(original_items.begin(), original_items.end())};
                        std::swap(rows[*container_index], rows[*container_index - 1]);
                        return declarations().range(rows);
                    }();
                    return row_0;
                }();
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
                    soa_fixed_container_index_ = *container_index - 1;
                    soa_editor_declaration_.reset();
                    return true;
                }
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(
                !has_container ||
                *container_index + 1 >=
                    declarations()
                        .items(declarations().fixed_soas.containers[declarations().fixed_soas.index(
                            (*document_->storage().declarations.soas.fixed
                                  [document_->storage().declarations.soas.index(*schema)]))])
                        .size());
            if (ImGui::Button("Fixed container down")) {
                auto replacement{document_->storage().declarations.soas.clone(*schema)};
                (*document_->storage()
                      .declarations.soas
                      .fixed[document_->storage().declarations.soas.index(replacement)]) = [&] {
                    auto row_0{declarations().fixed_soas.clone(
                        (*document_->storage()
                              .declarations.soas
                              .fixed[document_->storage().declarations.soas.index(replacement)]))};
                    declarations()
                        .fixed_soas.containers[declarations().fixed_soas.index(row_0)] = [&] {
                        auto const original_items{declarations().items(
                            declarations()
                                .fixed_soas.containers[declarations().fixed_soas.index(row_0)])};
                        auto rows{std::vector(original_items.begin(), original_items.end())};
                        std::swap(rows[*container_index], rows[*container_index + 1]);
                        return declarations().range(rows);
                    }();
                    return row_0;
                }();
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
                    soa_fixed_container_index_ = *container_index + 1;
                    soa_editor_declaration_.reset();
                    return true;
                }
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Delete fixed container")) {
                auto replacement{document_->storage().declarations.soas.clone(*schema)};
                (*document_->storage()
                      .declarations.soas
                      .fixed[document_->storage().declarations.soas.index(replacement)]) = [&] {
                    auto row_0{declarations().fixed_soas.clone(
                        (*document_->storage()
                              .declarations.soas
                              .fixed[document_->storage().declarations.soas.index(replacement)]))};
                    declarations()
                        .fixed_soas.containers[declarations().fixed_soas.index(row_0)] = [&] {
                        auto const original_items{declarations().items(
                            declarations()
                                .fixed_soas.containers[declarations().fixed_soas.index(row_0)])};
                        auto rows{std::vector(original_items.begin(), original_items.end())};
                        rows.erase(rows.begin() + static_cast<std::ptrdiff_t>(*container_index));
                        return declarations().range(rows);
                    }();
                    return row_0;
                }();
                auto const next_index{
                    declarations()
                            .items(declarations()
                                       .fixed_soas.containers[declarations().fixed_soas.index(
                                           (*document_->storage().declarations.soas.fixed
                                                 [document_->storage().declarations.soas.index(
                                                     replacement)]))])
                            .empty()
                        ? std::optional<std::size_t>{}
                        : std::optional{std::min(
                              *container_index,
                              declarations()
                                      .items(declarations()
                                                 .fixed_soas
                                                 .containers[declarations().fixed_soas.index(
                                                     (*document_->storage().declarations.soas.fixed
                                                           [document_->storage()
                                                                .declarations.soas.index(
                                                                    replacement)]))])
                                      .size() -
                                  1)}};
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
                    soa_fixed_container_index_ = next_index;
                    soa_editor_declaration_.reset();
                    return true;
                }
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Disable fixed layout")) {
                auto replacement{document_->storage().declarations.soas.clone(*schema)};
                document_->storage()
                    .declarations.soas
                    .fixed[document_->storage().declarations.soas.index(replacement)]
                    .reset();
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
                    soa_fixed_container_index_.reset();
                    soa_editor_declaration_.reset();
                    return true;
                }
            }

            if (soa_fixed_container_names_.size() ==
                    declarations()
                        .items(declarations().fixed_soas.containers[declarations().fixed_soas.index(
                            (*document_->storage().declarations.soas.fixed
                                  [document_->storage().declarations.soas.index(*schema)]))])
                        .size() &&
                detail::begin_editable_table(
                    "soa-fixed-containers",
                    2,
                    declarations()
                        .items(declarations().fixed_soas.containers[declarations().fixed_soas.index(
                            (*document_->storage().declarations.soas.fixed
                                  [document_->storage().declarations.soas.index(*schema)]))])
                        .size())) {
                detail::editable_table_column("", ImGuiTableColumnFlags_WidthFixed);
                detail::editable_table_column("Container type");
                ImGui::TableHeadersRow();
                for (std::size_t index{};
                     index <
                     declarations()
                         .items(
                             declarations().fixed_soas.containers[declarations().fixed_soas.index(
                                 (*document_->storage().declarations.soas.fixed
                                       [document_->storage().declarations.soas.index(*schema)]))])
                         .size();
                     ++index) {
                    ImGui::PushID(static_cast<int>(index));
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    auto const row_selected{soa_fixed_container_index_ == index};
                    if (detail::editable_table_row_handle(row_selected)) {
                        soa_fixed_container_index_ = index;
                    }
                    if (ImGui::BeginDragDropSource()) {
                        ImGui::SetDragDropPayload("SOA_FIXED_CONTAINER_ROW", &index, sizeof(index));
                        ImGui::Text(
                            "Move %s",
                            text(declarations().items(
                                     declarations()
                                         .fixed_soas.containers[declarations().fixed_soas.index(
                                             (*document_->storage().declarations.soas.fixed
                                                   [document_->storage().declarations.soas.index(
                                                       *schema)]))])[index])
                                .c_str());
                        ImGui::EndDragDropSource();
                    }
                    if (ImGui::BeginDragDropTarget()) {
                        if (auto const* payload{
                                ImGui::AcceptDragDropPayload("SOA_FIXED_CONTAINER_ROW")}) {
                            auto const source_index{
                                *static_cast<std::size_t const*>(payload->Data)};
                            if (source_index <
                                    declarations()
                                        .items(
                                            declarations()
                                                .fixed_soas
                                                .containers[declarations().fixed_soas.index((
                                                    *document_->storage().declarations.soas.fixed
                                                         [document_->storage()
                                                              .declarations.soas.index(*schema)]))])
                                        .size() &&
                                source_index != index) {
                                pending = declarations().soas.clone(*schema);
                                (*declarations()
                                      .soas.fixed[declarations().soas.index(*pending)]) = [&] {
                                    auto row_0{declarations().fixed_soas.clone(
                                        (*declarations()
                                              .soas.fixed[declarations().soas.index(*pending)]))};
                                    declarations()
                                        .fixed_soas
                                        .containers[declarations().fixed_soas.index(row_0)] = [&] {
                                        auto const original_items{declarations().items(
                                            declarations().fixed_soas.containers
                                                [declarations().fixed_soas.index(row_0)])};
                                        auto rows{std::vector(original_items.begin(),
                                                              original_items.end())};
                                        move_element(rows, source_index, index);
                                        return declarations().range(rows);
                                    }();
                                    return row_0;
                                }();
                                soa_fixed_container_index_ = index;
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-1.0F);
                    auto const name_submitted{
                        ImGui::InputText("##name",
                                         soa_fixed_container_names_[index].data(),
                                         soa_fixed_container_names_[index].size(),
                                         ImGuiInputTextFlags_EnterReturnsTrue)};
                    if (name_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                        if (!pending.has_value()) {
                            pending = declarations().soas.clone(*schema);
                        }
                        (*declarations().soas.fixed[declarations().soas.index(*pending)]) = [&] {
                            auto row_1{declarations().fixed_soas.clone(
                                (*declarations().soas.fixed[declarations().soas.index(*pending)]))};
                            declarations()
                                .fixed_soas.containers[declarations().fixed_soas.index(row_1)] =
                                [&] {
                                    auto const original_items{declarations().items(
                                        declarations()
                                            .fixed_soas
                                            .containers[declarations().fixed_soas.index(row_1)])};
                                    auto rows_0{
                                        std::vector(original_items.begin(), original_items.end())};
                                    rows_0[index] =
                                        strings().intern(soa_fixed_container_names_[index].data());
                                    return declarations().range(rows_0);
                                }();
                            return row_1;
                        }();
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }
    }

    if (detail::section("Single allocation")) {
        if (!document_->storage()
                 .declarations.soas
                 .single_allocation[document_->storage().declarations.soas.index(*schema)]
                 .has_value()) {
            if (ImGui::Button("Enable single allocation")) {
                auto owner_name{document_->unique_soa_storage_owner_name(
                    *declaration,
                    text(document_->storage()
                             .declarations.soas
                             .name[document_->storage().declarations.soas.index(*schema)]) +
                        "Single")};
                if (!owner_name.has_value()) {
                    schema_edit_message_ = owner_name.error().message;
                    return false;
                }
                auto replacement{document_->storage().declarations.soas.clone(*schema)};
                document_->storage()
                    .declarations.soas
                    .single_allocation[document_->storage().declarations.soas.index(replacement)] =
                    store_identifier(std::move(*owner_name));
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
                    soa_editor_declaration_.reset();
                    return true;
                }
            }
        } else {
            detail::prepare_property_input("Owner type");
            auto const submitted{ImGui::InputText("##Owner type",
                                                  soa_single_allocation_name_.data(),
                                                  soa_single_allocation_name_.size(),
                                                  ImGuiInputTextFlags_EnterReturnsTrue)};
            if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                if (soa_single_allocation_name_.front() == '\0') {
                    schema_edit_message_ = "Single-allocation owner type cannot be empty.";
                    return false;
                }
                auto replacement{document_->storage().declarations.soas.clone(*schema)};
                document_->storage()
                    .declarations.soas
                    .single_allocation[document_->storage().declarations.soas.index(replacement)] =
                    store_identifier(soa_single_allocation_name_.data());
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
                    soa_editor_declaration_.reset();
                    return true;
                }
            }
            if (ImGui::Button("Disable single allocation")) {
                auto replacement{document_->storage().declarations.soas.clone(*schema)};
                document_->storage()
                    .declarations.soas
                    .single_allocation[document_->storage().declarations.soas.index(replacement)]
                    .reset();
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
                    soa_editor_declaration_.reset();
                    return true;
                }
            }
        }

        if (navigate_to.has_value()) {
            select_type(*navigate_to);
            analysis_session_.inputs.selection.field.clear();
            analysis_session_.inputs.selection.record_access_members.clear();
            analysis_session_.inputs.selection.record_access_set_explicit = false;
            return true;
        }
    }

    if (pending.has_value()) {
        auto const invalid_member{std::ranges::find_if(
            declarations().items(declarations().soas.members[declarations().soas.index(*pending)]),
            [&](auto id) {
                auto const member{id};
                return text(declarations()
                                .soa_members.name[declarations().soa_members.index(member)])
                           .empty() ||
                       text(declarations().type_refs.name[declarations().type_refs.index(
                                declarations()
                                    .soa_members.type[declarations().soa_members.index(member)])])
                           .empty();
            })};
        if (invalid_member !=
            declarations()
                .items(declarations().soas.members[declarations().soas.index(*pending)])
                .end()) {
            schema_edit_message_ =
                text(declarations()
                         .soa_members.name[declarations().soa_members.index(*invalid_member)])
                        .empty()
                    ? "SoA column name cannot be empty."
                    : "SoA column type cannot be empty.";
            return false;
        }
        if (declarations().soas.fixed[declarations().soas.index(*pending)].has_value() &&
            std::ranges::any_of(
                declarations().items(
                    declarations().fixed_soas.containers[declarations().fixed_soas.index(
                        *declarations().soas.fixed[declarations().soas.index(*pending)])]),
                [&](auto name) { return text(name).empty(); })) {
            schema_edit_message_ = "Fixed-layout container type cannot be empty.";
            return false;
        }
        if (std::ranges::any_of(
                declarations().items(
                    declarations().soas.using_declarations[declarations().soas.index(*pending)]),
                [&](auto declaration) {
                    return text(declaration).find_first_not_of(" \t\r\n") == std::string::npos;
                })) {
            schema_edit_message_ = "SoA using declaration cannot be empty.";
            return false;
        }
        auto const invalid_function{std::ranges::find_if(
            declarations().items(
                declarations().soas.functions[declarations().soas.index(*pending)]),
            [&](auto id) {
                auto const function{id};
                return text(declarations().functions.name[declarations().functions.index(function)])
                           .empty() ||
                       text(declarations().type_refs.name[declarations().type_refs.index(
                                declarations().functions.return_type[declarations().functions.index(
                                    function)])])
                           .empty();
            })};
        if (invalid_function !=
            declarations()
                .items(declarations().soas.functions[declarations().soas.index(*pending)])
                .end()) {
            schema_edit_message_ =
                text(declarations()
                         .functions.name[declarations().functions.index(*invalid_function)])
                        .empty()
                    ? "SoA function name cannot be empty."
                    : "SoA function return type cannot be empty.";
            return false;
        }
        for (auto const function_id : declarations().items(
                 declarations().soas.functions[declarations().soas.index(*pending)])) {
            auto const function{function_id};
            if (declarations()
                    .functions.trailing_return_type[declarations().functions.index(function)]
                    .has_value() &&
                text(declarations().type_refs.name[declarations().type_refs.index(
                         (*declarations().functions.trailing_return_type
                               [declarations().functions.index(function)]))])
                    .empty()) {
                schema_edit_message_ = "SoA function trailing return type cannot be empty.";
                return false;
            }
            if (declarations()
                    .functions.template_parameters[declarations().functions.index(function)]
                    .has_value() &&
                text(*declarations()
                          .functions.template_parameters[declarations().functions.index(function)])
                        .find_first_not_of(" \t\r\n") == std::string::npos) {
                schema_edit_message_ = "SoA function template parameters cannot be empty.";
                return false;
            }
            if (declarations()
                    .functions.requires_clause[declarations().functions.index(function)]
                    .has_value() &&
                text(*declarations()
                          .functions.requires_clause[declarations().functions.index(function)])
                        .find_first_not_of(" \t\r\n") == std::string::npos) {
                schema_edit_message_ = "SoA function requires clause cannot be empty.";
                return false;
            }
            auto const invalid_dependency{std::ranges::find_if(
                declarations().items(
                    declarations()
                        .functions.dependencies[declarations().functions.index(function)]),
                [&](auto dependency) {
                    return text(dependency).find_first_not_of(" \t\r\n") == std::string::npos;
                })};
            if (invalid_dependency !=
                declarations()
                    .items(declarations()
                               .functions.dependencies[declarations().functions.index(function)])
                    .end()) {
                schema_edit_message_ = "SoA function dependency key cannot be empty.";
                return false;
            }
            auto const invalid_parameter{std::ranges::find_if(
                declarations().items(
                    declarations().functions.parameters[declarations().functions.index(function)]),
                [&](auto id) {
                    auto const parameter{id};
                    return text(declarations()
                                    .parameters.name[declarations().parameters.index(parameter)])
                               .empty() ||
                           text(declarations().type_refs.name[declarations().type_refs.index(
                                    declarations().parameters.type[declarations().parameters.index(
                                        parameter)])])
                               .empty() ||
                           (declarations()
                                .parameters
                                .default_value[declarations().parameters.index(parameter)]
                                .has_value() &&
                            text(*declarations()
                                      .parameters
                                      .default_value[declarations().parameters.index(parameter)])
                                    .find_first_not_of(" \t\r\n") == std::string::npos);
                })};
            if (invalid_parameter !=
                declarations()
                    .items(declarations()
                               .functions.parameters[declarations().functions.index(function)])
                    .end()) {
                if (text(declarations()
                             .parameters.name[declarations().parameters.index(*invalid_parameter)])
                        .empty()) {
                    schema_edit_message_ = "SoA function parameter name cannot be empty.";
                } else if (text(declarations().type_refs.name[declarations().type_refs.index(
                                    declarations().parameters.type[declarations().parameters.index(
                                        *invalid_parameter)])])
                               .empty()) {
                    schema_edit_message_ = "SoA function parameter type cannot be empty.";
                } else {
                    schema_edit_message_ = "SoA function parameter default cannot be empty.";
                }
                return false;
            }
        }
        if (apply_document_edit(
                document_->replace_declaration(*declaration, std::move(*pending)))) {
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
