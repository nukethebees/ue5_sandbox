#include "planner_ui_properties_common.hpp"

#include <codegen/schema/enum_schemas.h>
#include <codegen/schema/enum_unreal_projections.h>
#include <codegen/schema/enumerator_schemas.h>
#include <codegen/schema/fixed_point_rounding.h>
#include <codegen/schema/fixed_point_schemas.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/integer_scalar_cpp_emission.h>
#include <codegen/schema/integer_scalar_schemas.h>
#include <codegen/schema/integer_varint_encoding.h>
#include <codegen/schema/integer_varint_schemas.h>
#include <codegen/schema/linear_quantized_schemas.h>
#include <codegen/schema/mini_float_schemas.h>
#include <codegen/schema/optional_presence_bit_schemas.h>
#include <codegen/schema/optional_sentinel_schemas.h>
#include <codegen/schema/packed_named_code_schemas.h>
#include <codegen/schema/quantization_clipping.h>
#include <codegen/schema/semantic_relation_kind.h>
#include <codegen/schema/semantic_relation_schemas.h>
#include <codegen/schema/type_refs.h>
#include <lispb/compiler_storage.h>
#include <lispb/indices.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>

namespace ioj::layout_planner {

void PlannerUi::draw_enum_editor(TypeId const node, TypeDefinitionId const&) {
    if (!document_.has_value()) {
        return;
    }
    auto const declaration{
        document_->find_declaration(analysis_session_.inputs.workspace.types().identity(node))};
    if (!declaration.has_value()) {
        return;
    }
    auto const schema{
        document_->declaration_handle(*declaration, lispb::codegen::DeclarationKind::enumeration)};
    if (!schema.has_value()) {
        return;
    }

    if (selected_enumerator_.empty() &&
        !declarations()
             .items(document_->storage()
                        .declarations.enums
                        .values[document_->storage().declarations.enums.index(*schema)])
             .empty()) {
        selected_enumerator_ =
            text(declarations().enumerators.name[declarations().enumerators.index(
                declarations()
                    .items(document_->storage().declarations.enums.values
                               [document_->storage().declarations.enums.index(*schema)])
                    .front())]);
    }
    auto selected{std::ranges::find(
        declarations().items(
            document_->storage()
                .declarations.enums.values[document_->storage().declarations.enums.index(*schema)]),
        strings().intern(selected_enumerator_),
        [&](auto id) {
            return declarations().enumerators.name[declarations().enumerators.index(id)];
        })};
    if (selected == declarations()
                        .items(document_->storage()
                                   .declarations.enums
                                   .values[document_->storage().declarations.enums.index(*schema)])
                        .end() &&
        !declarations()
             .items(document_->storage()
                        .declarations.enums
                        .values[document_->storage().declarations.enums.index(*schema)])
             .empty()) {
        selected = declarations()
                       .items(document_->storage()
                                  .declarations.enums
                                  .values[document_->storage().declarations.enums.index(*schema)])
                       .begin();
        selected_enumerator_ =
            text(declarations().enumerators.name[declarations().enumerators.index(*selected)]);
    }
    auto const selected_index{
        selected == declarations()
                        .items(document_->storage()
                                   .declarations.enums
                                   .values[document_->storage().declarations.enums.index(*schema)])
                        .end()
            ? std::optional<std::size_t>{}
            : std::optional<std::size_t>{static_cast<std::size_t>(
                  selected -
                  declarations()
                      .items(document_->storage()
                                 .declarations.enums
                                 .values[document_->storage().declarations.enums.index(*schema)])
                      .begin())}};
    auto const enum_declaration_changed{enum_editor_declaration_ != declaration};
    if (enum_declaration_changed || enum_editor_value_ != selected_enumerator_) {
        enum_editor_declaration_ = declaration;
        enum_editor_value_ = selected_enumerator_;
        std::snprintf(
            enum_underlying_type_.data(),
            enum_underlying_type_.size(),
            "%s",
            document_->storage()
                    .declarations.enums
                    .underlying_type[document_->storage().declarations.enums.index(*schema)]
                    .has_value()
                ? text(declarations().type_refs.name[declarations().type_refs.index(
                           (*document_->storage().declarations.enums.underlying_type
                                 [document_->storage().declarations.enums.index(*schema)]))])
                      .c_str()
                : "std::uint8_t");
        std::snprintf(
            enum_export_specifier_.data(),
            enum_export_specifier_.size(),
            "%s",
            text(document_->storage()
                     .declarations.enums
                     .export_specifier[document_->storage().declarations.enums.index(*schema)])
                .c_str());
        if (enum_declaration_changed) {
            auto const projection{
                document_->storage()
                        .declarations.enums
                        .unreal_projection[document_->storage().declarations.enums.index(*schema)]
                    ? std::optional{*document_->storage().declarations.enums.unreal_projection
                                         [document_->storage().declarations.enums.index(*schema)]}
                    : std::nullopt};
            std::snprintf(
                enum_projection_name_.data(),
                enum_projection_name_.size(),
                "%s",
                projection.has_value()
                    ? text(declarations()
                               .enum_unreal_projections
                               .name[declarations().enum_unreal_projections.index(*projection)])
                          .c_str()
                    : "");
            std::snprintf(
                enum_projection_header_.data(),
                enum_projection_header_.size(),
                "%s",
                projection.has_value()
                    ? text(declarations()
                               .enum_unreal_projections
                               .header[declarations().enum_unreal_projections.index(*projection)])
                          .c_str()
                    : "");
            std::snprintf(
                enum_projection_header_include_.data(),
                enum_projection_header_include_.size(),
                "%s",
                projection.has_value()
                    ? text(declarations().enum_unreal_projections.header_include
                               [declarations().enum_unreal_projections.index(*projection)])
                          .c_str()
                    : "");
            std::snprintf(
                enum_projection_conversion_header_.data(),
                enum_projection_conversion_header_.size(),
                "%s",
                projection.has_value()
                    ? text(declarations().enum_unreal_projections.conversion_header
                               [declarations().enum_unreal_projections.index(*projection)])
                          .c_str()
                    : "");
            std::snprintf(
                enum_projection_native_header_include_.data(),
                enum_projection_native_header_include_.size(),
                "%s",
                projection.has_value()
                    ? text(declarations().enum_unreal_projections.native_header_include
                               [declarations().enum_unreal_projections.index(*projection)])
                          .c_str()
                    : "");
            auto const reflection{
                projection.has_value()
                    ? std::ranges::find(
                          enum_projection_reflections,
                          declarations().enum_unreal_projections.reflection
                              [declarations().enum_unreal_projections.index(*projection)])
                    : enum_projection_reflections.begin()};
            enum_projection_reflection_ =
                reflection == enum_projection_reflections.end()
                    ? 0
                    : static_cast<int>(reflection - enum_projection_reflections.begin());
        }
        if (selected !=
            declarations()
                .items(document_->storage()
                           .declarations.enums
                           .values[document_->storage().declarations.enums.index(*schema)])
                .end()) {
            std::snprintf(
                enum_value_name_.data(),
                enum_value_name_.size(),
                "%s",
                text(declarations().enumerators.name[declarations().enumerators.index(*selected)])
                    .c_str());
            set_buffer(
                enum_value_initializer_,
                read_optional_text(
                    declarations()
                        .enumerators.initializer[declarations().enumerators.index(*selected)]));
            set_buffer(
                enum_value_display_name_,
                read_optional_text(
                    declarations()
                        .enumerators.display_name[declarations().enumerators.index(*selected)]));
            set_buffer(
                enum_value_serialized_name_,
                read_optional_text(
                    declarations()
                        .enumerators.serialized_name[declarations().enumerators.index(*selected)]));
        }
    }

    auto automatic_backing{
        !document_->storage()
             .declarations.enums
             .underlying_type[document_->storage().declarations.enums.index(*schema)]
             .has_value()};
    if (ImGui::Checkbox("Auto C++ backing", &automatic_backing)) {
        auto replacement{document_->storage().declarations.enums.clone(*schema)};
        document_->storage()
            .declarations.enums
            .underlying_type[document_->storage().declarations.enums.index(replacement)] =
            automatic_backing ? std::optional<lispb::codegen::TypeRefId>{} : std::optional{[&] {
                auto const new_type_refs{declarations().type_refs.append()};
                declarations().type_refs.name[declarations().type_refs.index(new_type_refs)] =
                    strings().intern(enum_underlying_type_.data());
                return new_type_refs;
            }()};
        static_cast<void>(apply_document_edit(
            document_->replace_declaration(*declaration, std::move(replacement))));
        return;
    }
    if (document_->storage()
            .declarations.enums
            .underlying_type[document_->storage().declarations.enums.index(*schema)]
            .has_value()) {
        auto const submitted{ImGui::InputText("C++ backing type",
                                              enum_underlying_type_.data(),
                                              enum_underlying_type_.size(),
                                              ImGuiInputTextFlags_EnterReturnsTrue)};
        if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
            if (enum_underlying_type_.front() == '\0') {
                schema_edit_message_ = "Explicit enum C++ backing type cannot be empty.";
            } else if (text(declarations().type_refs.name[declarations().type_refs.index(
                           *document_->storage().declarations.enums.underlying_type
                                [document_->storage().declarations.enums.index(*schema)])]) !=
                       enum_underlying_type_.data()) {
                auto replacement{document_->storage().declarations.enums.clone(*schema)};
                document_->storage().declarations.enums.underlying_type
                    [document_->storage().declarations.enums.index(replacement)] = [&] {
                    auto const new_type_refs{declarations().type_refs.append()};
                    declarations().type_refs.name[declarations().type_refs.index(new_type_refs)] =
                        strings().intern(enum_underlying_type_.data());
                    declarations()
                        .type_refs.suffix[declarations().type_refs.index(new_type_refs)] = {};
                    declarations().type_refs.nested[declarations().type_refs.index(new_type_refs)] =
                        std::nullopt;
                    return new_type_refs;
                }();
                static_cast<void>(apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement))));
            }
            return;
        }
    } else if (analysis_session_.results().enum_domain.has_value()) {
        ImGui::TextDisabled("Derived for C++: %s",
                            analysis_session_.results().enum_domain->backing_type.c_str());
    }

    auto automatic_width{
        !document_->storage()
             .declarations.enums.bit_width[document_->storage().declarations.enums.index(*schema)]
             .has_value()};
    if (ImGui::Checkbox("Auto semantic width", &automatic_width)) {
        auto replacement{document_->storage().declarations.enums.clone(*schema)};
        if (automatic_width) {
            document_->storage()
                .declarations.enums
                .bit_width[document_->storage().declarations.enums.index(replacement)]
                .reset();
        } else {
            document_->storage()
                .declarations.enums
                .bit_width[document_->storage().declarations.enums.index(replacement)] =
                analysis_session_.results().enum_domain.has_value()
                    ? static_cast<std::uint8_t>(
                          analysis_session_.results().enum_domain->minimum_required_bits.value_or(
                              1))
                    : std::uint8_t{1};
        }
        static_cast<void>(apply_document_edit(
            document_->replace_declaration(*declaration, std::move(replacement))));
        return;
    }
    if (document_->storage()
            .declarations.enums.bit_width[document_->storage().declarations.enums.index(*schema)]
            .has_value()) {
        auto bit_width{static_cast<std::uint32_t>(
            *document_->storage()
                 .declarations.enums
                 .bit_width[document_->storage().declarations.enums.index(*schema)])};
        detail::prepare_property_input("Semantic width");
        auto const submitted{ImGui::InputScalar("##Semantic width",
                                                ImGuiDataType_U32,
                                                &bit_width,
                                                nullptr,
                                                nullptr,
                                                "%u",
                                                ImGuiInputTextFlags_EnterReturnsTrue)};
        if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
            if (bit_width == 0 || bit_width > 64) {
                schema_edit_message_ = "Enum semantic width must be between 1 and 64 bits.";
            } else {
                auto replacement{document_->storage().declarations.enums.clone(*schema)};
                document_->storage()
                    .declarations.enums
                    .bit_width[document_->storage().declarations.enums.index(replacement)] =
                    static_cast<std::uint8_t>(bit_width);
                static_cast<void>(apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement))));
            }
            return;
        }
    }
    auto signedness_mode{
        document_->storage()
                    .declarations.enums
                    .signedness[document_->storage().declarations.enums.index(*schema)] ==
                EnumSignedness::unspecified
            ? 0
            : (document_->storage()
                           .declarations.enums
                           .signedness[document_->storage().declarations.enums.index(*schema)] ==
                       EnumSignedness::signed_
                   ? 2
                   : 1)};
    constexpr std::array signedness_labels{"Auto / inferred", "Unsigned", "Signed"};
    detail::prepare_property_input("Semantic signedness");
    if (ImGui::Combo("##Semantic signedness",
                     &signedness_mode,
                     signedness_labels.data(),
                     static_cast<int>(signedness_labels.size()))) {
        auto replacement{document_->storage().declarations.enums.clone(*schema)};
        document_->storage()
            .declarations.enums
            .signedness[document_->storage().declarations.enums.index(replacement)] =
            signedness_mode == 0   ? EnumSignedness::unspecified
            : signedness_mode == 2 ? EnumSignedness::signed_
                                   : EnumSignedness::unsigned_;
        static_cast<void>(apply_document_edit(
            document_->replace_declaration(*declaration, std::move(replacement))));
        return;
    }
    ImGui::PushTextWrapPos(0.0F);
    ImGui::TextDisabled("Semantic width describes the value domain; C++ backing storage is a "
                        "separate lowering choice.");
    ImGui::PopTextWrapPos();

    if (detail::section("Generation policy")) {
        auto reflection_index{static_cast<int>(
            std::ranges::find(
                enum_reflections,
                document_->storage()
                    .declarations.enums
                    .reflection[document_->storage().declarations.enums.index(*schema)]) -
            enum_reflections.begin())};
        if (reflection_index < 0 || reflection_index >= static_cast<int>(enum_reflections.size())) {
            reflection_index = 0;
        }
        detail::prepare_property_input("Reflection");
        if (ImGui::BeginCombo("##Reflection",
                              lispb::codegen::enum_reflection_name(
                                  enum_reflections[static_cast<std::size_t>(reflection_index)])
                                  .data())) {
            for (std::size_t index{}; index < enum_reflections.size(); ++index) {
                auto const reflection{enum_reflections[index]};
                auto const selected_reflection{index == static_cast<std::size_t>(reflection_index)};
                if (ImGui::Selectable(lispb::codegen::enum_reflection_name(reflection).data(),
                                      selected_reflection)) {
                    auto replacement{document_->storage().declarations.enums.clone(*schema)};
                    document_->storage()
                        .declarations.enums
                        .reflection[document_->storage().declarations.enums.index(replacement)] =
                        reflection;
                    ImGui::EndCombo();
                    static_cast<void>(apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement))));
                    return;
                }
                if (selected_reflection) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        auto enum_array{document_->storage()
                            .declarations.enums
                            .enum_array[document_->storage().declarations.enums.index(*schema)] !=
                        0};
        if (ImGui::Checkbox("Generate enum-array helpers", &enum_array)) {
            auto replacement{document_->storage().declarations.enums.clone(*schema)};
            document_->storage()
                .declarations.enums
                .enum_array[document_->storage().declarations.enums.index(replacement)] =
                enum_array;
            static_cast<void>(apply_document_edit(
                document_->replace_declaration(*declaration, std::move(replacement))));
            return;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "Requires a valid final count-sentinel enumerator and implicit values.");
        }

        auto native_api{document_->storage()
                            .declarations.enums
                            .native_api[document_->storage().declarations.enums.index(*schema)] !=
                        0};
        if (ImGui::Checkbox("Use native enum API", &native_api)) {
            auto replacement{document_->storage().declarations.enums.clone(*schema)};
            document_->storage()
                .declarations.enums
                .native_api[document_->storage().declarations.enums.index(replacement)] =
                native_api;
            static_cast<void>(apply_document_edit(
                document_->replace_declaration(*declaration, std::move(replacement))));
            return;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "Native API mode is incompatible with reflection, enum arrays, conversions, and an "
                "export specifier.");
        }

        detail::prepare_property_input("Export specifier (optional)");
        auto const export_submitted{ImGui::InputText("##Export specifier (optional)##enum",
                                                     enum_export_specifier_.data(),
                                                     enum_export_specifier_.size(),
                                                     ImGuiInputTextFlags_EnterReturnsTrue)};
        if (export_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
            auto const export_specifier{std::string_view{enum_export_specifier_.data()}};
            if (text(document_->storage().declarations.enums.export_specifier
                         [document_->storage().declarations.enums.index(*schema)]) !=
                export_specifier) {
                auto replacement{document_->storage().declarations.enums.clone(*schema)};
                document_->storage()
                    .declarations.enums
                    .export_specifier[document_->storage().declarations.enums.index(replacement)] =
                    strings().intern(export_specifier);
                static_cast<void>(apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement))));
            }
            return;
        }
    }
    if (detail::section("Unreal projection", false)) {
        std::optional<lispb::codegen::DeclarationHandle> projection_edit;
        auto const projection_exists{
            document_->storage()
                .declarations.enums
                .unreal_projection[document_->storage().declarations.enums.index(*schema)]
                .has_value()};
        auto edit_projection_text = [&](char const* label, auto& buffer, auto&& assign) {
            detail::prepare_property_input(label);
            auto const input_id{"##" + std::string{label}};
            auto const submitted{ImGui::InputText(input_id.c_str(),
                                                  buffer.data(),
                                                  buffer.size(),
                                                  ImGuiInputTextFlags_EnterReturnsTrue)};
            if (projection_exists && (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                if (!projection_edit.has_value()) {
                    projection_edit = declarations().enums.clone(*schema);
                }
                declarations()
                    .enums.unreal_projection[declarations().enums.index(*projection_edit)] = [&] {
                    auto projection{declarations().enum_unreal_projections.clone(
                        *declarations().enums.unreal_projection[declarations().enums.index(
                            *projection_edit)])};
                    assign(projection, buffer.data());
                    return projection;
                }();
            }
        };
        edit_projection_text(
            "Projected enum name", enum_projection_name_, [&](auto& projection, char const* value) {
                declarations()
                    .enum_unreal_projections
                    .name[declarations().enum_unreal_projections.index(projection)] =
                    strings().intern(value);
            });
        edit_projection_text(
            "Generated header path",
            enum_projection_header_,
            [&](auto& projection, char const* value) {
                declarations()
                    .enum_unreal_projections
                    .header[declarations().enum_unreal_projections.index(projection)] =
                    strings().add(value);
            });
        edit_projection_text(
            "Generated header include",
            enum_projection_header_include_,
            [&](auto& projection, char const* value) {
                declarations()
                    .enum_unreal_projections
                    .header_include[declarations().enum_unreal_projections.index(projection)] =
                    strings().add(value);
            });
        edit_projection_text(
            "Conversion header path",
            enum_projection_conversion_header_,
            [&](auto& projection, char const* value) {
                declarations()
                    .enum_unreal_projections
                    .conversion_header[declarations().enum_unreal_projections.index(projection)] =
                    strings().add(value);
            });
        edit_projection_text("Native header include",
                             enum_projection_native_header_include_,
                             [&](auto& projection, char const* value) {
                                 declarations().enum_unreal_projections.native_header_include
                                     [declarations().enum_unreal_projections.index(projection)] =
                                     strings().add(value);
                             });

        enum_projection_reflection_ =
            std::clamp(enum_projection_reflection_,
                       0,
                       static_cast<int>(enum_projection_reflections.size() - 1));
        auto const selected_projection_reflection{
            enum_projection_reflections[static_cast<std::size_t>(enum_projection_reflection_)]};
        detail::prepare_property_input("Projection reflection");
        if (ImGui::BeginCombo(
                "##Projection reflection",
                lispb::codegen::enum_reflection_name(selected_projection_reflection).data())) {
            for (std::size_t index{}; index < enum_projection_reflections.size(); ++index) {
                auto const reflection{enum_projection_reflections[index]};
                auto const selected_reflection{enum_projection_reflection_ ==
                                               static_cast<int>(index)};
                if (ImGui::Selectable(lispb::codegen::enum_reflection_name(reflection).data(),
                                      selected_reflection)) {
                    enum_projection_reflection_ = static_cast<int>(index);
                    if (projection_exists) {
                        projection_edit = declarations().enums.clone(*schema);
                        (*declarations().enums.unreal_projection[declarations().enums.index(
                            *projection_edit)]) = [&] {
                            auto row_0{declarations().enum_unreal_projections.clone(
                                (*declarations().enums.unreal_projection[declarations().enums.index(
                                    *projection_edit)]))};
                            declarations()
                                .enum_unreal_projections
                                .reflection[declarations().enum_unreal_projections.index(row_0)] =
                                reflection;
                            return row_0;
                        }();
                    }
                }
                if (selected_reflection) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        if (projection_edit.has_value()) {
            static_cast<void>(apply_document_edit(
                document_->replace_declaration(*declaration, std::move(*projection_edit))));
            return;
        }

        if (projection_exists) {
            if (ImGui::Button("Remove Unreal projection")) {
                auto replacement{document_->storage().declarations.enums.clone(*schema)};
                document_->storage()
                    .declarations.enums
                    .unreal_projection[document_->storage().declarations.enums.index(replacement)]
                    .reset();
                static_cast<void>(apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement))));
                return;
            }
        } else {
            auto const fields_complete{enum_projection_name_.front() != '\0' &&
                                       enum_projection_header_.front() != '\0' &&
                                       enum_projection_header_include_.front() != '\0' &&
                                       enum_projection_conversion_header_.front() != '\0' &&
                                       enum_projection_native_header_include_.front() != '\0'};
            ImGui::BeginDisabled(
                !document_->storage()
                     .declarations.enums
                     .native_api[document_->storage().declarations.enums.index(*schema)] ||
                !fields_complete);
            if (ImGui::Button("Add Unreal projection")) {
                auto replacement{document_->storage().declarations.enums.clone(*schema)};
                document_->storage().declarations.enums.unreal_projection
                    [document_->storage().declarations.enums.index(replacement)] = [&] {
                    auto const new_enum_unreal_projections{
                        declarations().enum_unreal_projections.append()};
                    declarations()
                        .enum_unreal_projections.name[declarations().enum_unreal_projections.index(
                            new_enum_unreal_projections)] =
                        strings().intern(enum_projection_name_.data());
                    declarations()
                        .enum_unreal_projections
                        .header[declarations().enum_unreal_projections.index(
                            new_enum_unreal_projections)] =
                        strings().add(enum_projection_header_.data());
                    declarations()
                        .enum_unreal_projections
                        .header_include[declarations().enum_unreal_projections.index(
                            new_enum_unreal_projections)] =
                        strings().add(enum_projection_header_include_.data());
                    declarations()
                        .enum_unreal_projections
                        .conversion_header[declarations().enum_unreal_projections.index(
                            new_enum_unreal_projections)] =
                        strings().add(enum_projection_conversion_header_.data());
                    declarations()
                        .enum_unreal_projections
                        .native_header_include[declarations().enum_unreal_projections.index(
                            new_enum_unreal_projections)] =
                        strings().add(enum_projection_native_header_include_.data());
                    declarations()
                        .enum_unreal_projections
                        .reflection[declarations().enum_unreal_projections.index(
                            new_enum_unreal_projections)] = selected_projection_reflection;
                    return new_enum_unreal_projections;
                }();
                static_cast<void>(apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement))));
                ImGui::EndDisabled();
                return;
            }
            ImGui::EndDisabled();
            if (!document_->storage()
                     .declarations.enums
                     .native_api[document_->storage().declarations.enums.index(*schema)]) {
                ImGui::TextDisabled("A projection requires native enum API mode.");
            } else if (!fields_complete) {
                ImGui::TextDisabled("Fill every projection field before adding it.");
            }
        }
        ImGui::PushTextWrapPos(0.0F);
        ImGui::TextDisabled(
            "Projection files are generated consumer outputs; semantic enum width and "
            "target layout remain unchanged.");
        ImGui::PopTextWrapPos();
    }

    if (detail::section("Generated conversions", false)) {
        if (ImGui::BeginTable("enum-conversions", 2, ImGuiTableFlags_SizingStretchSame)) {
            for (auto const conversion : enum_conversions) {
                auto const descriptor{enum_conversion_descriptor(conversion)};
                ImGui::TableNextColumn();
                auto enabled{has_enum_conversion(
                    declarations().items(
                        document_->storage()
                            .declarations.enums
                            .conversions[document_->storage().declarations.enums.index(*schema)]),
                    conversion)};
                if (ImGui::Checkbox(descriptor.source_name.data(), &enabled)) {
                    auto replacement{document_->storage().declarations.enums.clone(*schema)};
                    document_->storage()
                        .declarations.enums
                        .conversions[document_->storage().declarations.enums.index(replacement)] =
                        declarations().range(with_enum_conversion(
                            declarations().items(
                                document_->storage().declarations.enums.conversions
                                    [document_->storage().declarations.enums.index(*schema)]),
                            conversion,
                            enabled));
                    ImGui::EndTable();
                    static_cast<void>(apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement))));
                    return;
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", descriptor.description);
                }
            }
            ImGui::EndTable();
        }
    }
    if (detail::section("Enumerators")) {
        if (ImGui::Button("+ Enumerator")) {
            auto replacement{document_->storage().declarations.enums.clone(*schema)};
            auto suffix{
                declarations()
                    .items(document_->storage()
                               .declarations.enums
                               .values[document_->storage().declarations.enums.index(replacement)])
                    .size()};
            std::string name;
            do {
                name = "Value" + std::to_string(suffix++);
            } while (
                std::ranges::find(
                    declarations().items(
                        document_->storage()
                            .declarations.enums
                            .values[document_->storage().declarations.enums.index(replacement)]),
                    strings().intern(name),
                    [&](auto id) {
                        return declarations()
                            .enumerators.name[declarations().enumerators.index(id)];
                    }) !=
                declarations()
                    .items(document_->storage()
                               .declarations.enums
                               .values[document_->storage().declarations.enums.index(replacement)])
                    .end());
            auto insertion_index{
                declarations()
                    .items(document_->storage()
                               .declarations.enums
                               .values[document_->storage().declarations.enums.index(replacement)])
                    .size()};
            if (document_->storage()
                    .declarations.enums
                    .count[document_->storage().declarations.enums.index(replacement)]
                    .has_value()) {
                auto const count{std::ranges::find(
                    declarations().items(
                        document_->storage()
                            .declarations.enums
                            .values[document_->storage().declarations.enums.index(replacement)]),
                    *document_->storage()
                         .declarations.enums
                         .count[document_->storage().declarations.enums.index(replacement)],
                    [&](auto id) {
                        return declarations()
                            .enumerators.name[declarations().enumerators.index(id)];
                    })};
                if (count !=
                    declarations()
                        .items(
                            document_->storage()
                                .declarations.enums
                                .values[document_->storage().declarations.enums.index(replacement)])
                        .end()) {
                    insertion_index = static_cast<std::size_t>(
                        count -
                        declarations()
                            .items(document_->storage().declarations.enums.values
                                       [document_->storage().declarations.enums.index(replacement)])
                            .begin());
                }
            }
            document_->storage()
                .declarations.enums
                .values[document_->storage().declarations.enums.index(replacement)] = [&] {
                auto const original_items{declarations().items(
                    document_->storage()
                        .declarations.enums
                        .values[document_->storage().declarations.enums.index(replacement)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.insert(rows.begin() + static_cast<std::ptrdiff_t>(insertion_index), [&] {
                    auto const new_enumerators{declarations().enumerators.append()};
                    declarations()
                        .enumerators.name[declarations().enumerators.index(new_enumerators)] =
                        strings().intern(name);
                    declarations().enumerators.initializer[declarations().enumerators.index(
                        new_enumerators)] = std::nullopt;
                    declarations().enumerators.display_name[declarations().enumerators.index(
                        new_enumerators)] = std::nullopt;
                    declarations()
                        .enumerators.hidden[declarations().enumerators.index(new_enumerators)] =
                        false;
                    declarations().enumerators.serialized_name[declarations().enumerators.index(
                        new_enumerators)] = std::nullopt;
                    return new_enumerators;
                }());
                return declarations().range(rows);
            }();
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
                selected_enumerator_ = std::move(name);
                return;
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!selected_index.has_value());
        if (ImGui::Button("Duplicate")) {
            auto replacement{document_->storage().declarations.enums.clone(*schema)};
            auto copy{declarations().enumerators.clone(declarations().items(
                document_->storage()
                    .declarations.enums.values[document_->storage().declarations.enums.index(
                        replacement)])[*selected_index])};
            auto suffix{std::size_t{1}};
            auto const stem{
                text(declarations().enumerators.name[declarations().enumerators.index(copy)]) +
                "_copy"};
            declarations().enumerators.name[declarations().enumerators.index(copy)] =
                strings().intern(stem);
            while (
                std::ranges::find(
                    declarations().items(
                        document_->storage()
                            .declarations.enums
                            .values[document_->storage().declarations.enums.index(replacement)]),
                    declarations().enumerators.name[declarations().enumerators.index(copy)],
                    [&](auto id) {
                        return declarations()
                            .enumerators.name[declarations().enumerators.index(id)];
                    }) !=
                declarations()
                    .items(document_->storage()
                               .declarations.enums
                               .values[document_->storage().declarations.enums.index(replacement)])
                    .end()) {
                declarations().enumerators.name[declarations().enumerators.index(copy)] =
                    strings().intern(stem + std::to_string(suffix++));
            }
            auto const insertion_index{
                document_->storage()
                            .declarations.enums
                            .count[document_->storage().declarations.enums.index(*schema)] ==
                        declarations().enumerators.name[declarations().enumerators.index(
                            declarations().items(document_->storage().declarations.enums.values
                                                     [document_->storage().declarations.enums.index(
                                                         replacement)])[*selected_index])]
                    ? *selected_index
                    : *selected_index + 1};
            document_->storage()
                .declarations.enums
                .values[document_->storage().declarations.enums.index(replacement)] = [&] {
                auto const original_items{declarations().items(
                    document_->storage()
                        .declarations.enums
                        .values[document_->storage().declarations.enums.index(replacement)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.insert(rows.begin() + static_cast<std::ptrdiff_t>(insertion_index), copy);
                return declarations().range(rows);
            }();
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
                selected_enumerator_ =
                    text(declarations().enumerators.name[declarations().enumerators.index(copy)]);
                ImGui::EndDisabled();
                return;
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!selected_index.has_value() || *selected_index == 0);
        if (ImGui::Button("Move up")) {
            auto replacement{document_->storage().declarations.enums.clone(*schema)};
            document_->storage()
                .declarations.enums
                .values[document_->storage().declarations.enums.index(replacement)] = [&] {
                auto const original_items{declarations().items(
                    document_->storage()
                        .declarations.enums
                        .values[document_->storage().declarations.enums.index(replacement)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                std::swap(rows[*selected_index], rows[*selected_index - 1]);
                return declarations().range(rows);
            }();
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
                selected_enumerator_ = enum_editor_value_;
                ImGui::EndDisabled();
                ImGui::EndDisabled();
                return;
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(
            !selected_index.has_value() ||
            *selected_index + 1 >=
                declarations()
                    .items(document_->storage()
                               .declarations.enums
                               .values[document_->storage().declarations.enums.index(*schema)])
                    .size());
        if (ImGui::Button("Move down")) {
            auto replacement{document_->storage().declarations.enums.clone(*schema)};
            document_->storage()
                .declarations.enums
                .values[document_->storage().declarations.enums.index(replacement)] = [&] {
                auto const original_items{declarations().items(
                    document_->storage()
                        .declarations.enums
                        .values[document_->storage().declarations.enums.index(replacement)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                std::swap(rows[*selected_index], rows[*selected_index + 1]);
                return declarations().range(rows);
            }();
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
                selected_enumerator_ = enum_editor_value_;
                ImGui::EndDisabled();
                ImGui::EndDisabled();
                return;
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(
            !selected_index.has_value() ||
            declarations()
                    .items(document_->storage()
                               .declarations.enums
                               .values[document_->storage().declarations.enums.index(*schema)])
                    .size() == 1);
        if (ImGui::Button("Delete")) {
            auto replacement{document_->storage().declarations.enums.clone(*schema)};
            auto const deleted_name{
                declarations().enumerators.name[declarations().enumerators.index(
                    declarations().items(document_->storage().declarations.enums.values
                                             [document_->storage().declarations.enums.index(
                                                 replacement)])[*selected_index])]};
            document_->storage()
                .declarations.enums
                .values[document_->storage().declarations.enums.index(replacement)] = [&] {
                auto const original_items{declarations().items(
                    document_->storage()
                        .declarations.enums
                        .values[document_->storage().declarations.enums.index(replacement)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.erase(rows.begin() + static_cast<std::ptrdiff_t>(*selected_index));
                return declarations().range(rows);
            }();
            if (document_->storage()
                    .declarations.enums
                    .count[document_->storage().declarations.enums.index(replacement)] ==
                deleted_name) {
                document_->storage()
                    .declarations.enums
                    .count[document_->storage().declarations.enums.index(replacement)]
                    .reset();
            }
            auto const next_index{std::min(
                *selected_index,
                declarations()
                        .items(
                            document_->storage()
                                .declarations.enums
                                .values[document_->storage().declarations.enums.index(replacement)])
                        .size() -
                    1)};
            auto const next_name{
                text(declarations().enumerators.name[declarations().enumerators.index(
                    declarations().items(document_->storage().declarations.enums.values
                                             [document_->storage().declarations.enums.index(
                                                 replacement)])[next_index])])};
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
                selected_enumerator_ = next_name;
                ImGui::EndDisabled();
                ImGui::EndDisabled();
                return;
            }
        }
        ImGui::EndDisabled();
        ImGui::EndDisabled();

        std::optional<lispb::codegen::DeclarationHandle> pending;
        auto selected_after_edit{selected_enumerator_};
        if (detail::begin_editable_table(
                "enumerators",
                9,
                declarations()
                    .items(document_->storage()
                               .declarations.enums
                               .values[document_->storage().declarations.enums.index(*schema)])
                    .size())) {
            detail::editable_table_column("", ImGuiTableColumnFlags_WidthFixed);
            detail::editable_table_column("Name");
            detail::editable_table_column("Initializer");
            detail::editable_table_column("Derived code");
            detail::editable_table_column("Display");
            detail::editable_table_column("Serialized");
            detail::editable_table_column("Hidden", ImGuiTableColumnFlags_WidthFixed);
            detail::editable_table_column("Sentinel", ImGuiTableColumnFlags_WidthFixed);
            detail::editable_table_column("Count", ImGuiTableColumnFlags_WidthFixed);
            constexpr std::array<char const*, 9> header_tooltips{
                "Select an enumerator or drag it to change its order.",
                "C++ identifier for this enumerator.",
                "Optional C++ value expression. Leave blank to use the next implicit value.",
                "Numeric value derived from the initializers; read only.",
                "Optional human-readable name for display-string conversions. Defaults to Name.",
                "Stable text token for serialization and parsing, distinct from Display and the "
                "numeric value. Serialized conversions require one for every non-count value.",
                "Mark this value hidden in Unreal reflection (UMETA Hidden).",
                "Reserve this value as a sentinel rather than a live enum value.",
                "Use this final value as the enum's count sentinel.",
            };
            ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
            for (std::size_t index{}; index < header_tooltips.size(); ++index) {
                auto const column{static_cast<int>(index)};
                if (!ImGui::TableSetColumnIndex(column)) {
                    continue;
                }
                ImGui::PushID(column);
                ImGui::TableHeader(ImGui::TableGetColumnName(column));
                ImGui::SetItemTooltip("%s", header_tooltips[index]);
                ImGui::PopID();
            }
            for (std::size_t index{};
                 index <
                 declarations()
                     .items(document_->storage()
                                .declarations.enums
                                .values[document_->storage().declarations.enums.index(*schema)])
                     .size();
                 ++index) {
                auto const value{declarations().items(
                    document_->storage()
                        .declarations.enums
                        .values[document_->storage().declarations.enums.index(*schema)])[index]};
                auto const row_selected{
                    selected_enumerator_ ==
                    text(declarations().enumerators.name[declarations().enumerators.index(value)])};
                ImGui::PushID(static_cast<int>(index));
                auto select_cell = [&](char const* id, char const* label) {
                    ImGui::PushID(id);
                    if (ImGui::Selectable(label)) {
                        selected_enumerator_ =
                            text(declarations()
                                     .enumerators.name[declarations().enumerators.index(value)]);
                        enum_editor_declaration_.reset();
                    }
                    ImGui::PopID();
                };
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                if (detail::editable_table_row_handle(row_selected)) {
                    selected_enumerator_ = text(
                        declarations().enumerators.name[declarations().enumerators.index(value)]);
                    enum_editor_declaration_.reset();
                }
                if (ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload("ENUMERATOR_ROW", &index, sizeof(index));
                    ImGui::Text("Move %s",
                                text(declarations()
                                         .enumerators.name[declarations().enumerators.index(value)])
                                    .c_str());
                    ImGui::EndDragDropSource();
                }
                if (ImGui::BeginDragDropTarget()) {
                    if (auto const* payload{ImGui::AcceptDragDropPayload("ENUMERATOR_ROW")}) {
                        auto const source_index{*static_cast<std::size_t const*>(payload->Data)};
                        if (source_index <
                                declarations()
                                    .items(
                                        document_->storage()
                                            .declarations.enums
                                            .values[document_->storage().declarations.enums.index(
                                                *schema)])
                                    .size() &&
                            source_index != index) {
                            pending = declarations().enums.clone(*schema);
                            selected_after_edit = text(
                                declarations().enumerators.name[declarations().enumerators.index(
                                    declarations().items(
                                        declarations().enums.values[declarations().enums.index(
                                            *pending)])[source_index])]);
                            declarations().enums.values[declarations().enums.index(*pending)] =
                                [&] {
                                    auto const original_items{declarations().items(
                                        declarations()
                                            .enums.values[declarations().enums.index(*pending)])};
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
                if (row_selected) {
                    ImGui::SetNextItemWidth(-1.0F);
                    auto const submitted{ImGui::InputText("##name",
                                                          enum_value_name_.data(),
                                                          enum_value_name_.size(),
                                                          ImGuiInputTextFlags_EnterReturnsTrue)};
                    if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                        pending = declarations().enums.clone(*schema);

                        auto const previous_name{
                            declarations().enumerators.name[declarations().enumerators.index(
                                declarations().items(
                                    declarations().enums.values[declarations().enums.index(
                                        *pending)])[index])]};
                        declarations().enums.values[declarations().enums.index(*pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations().enums.values[declarations().enums.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().enumerators.clone(rows_1[index])};
                                declarations()
                                    .enumerators.name[declarations().enumerators.index(row_0)] =
                                    strings().intern(enum_value_name_.data());
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        if (declarations().enums.count[declarations().enums.index(*pending)] ==
                            previous_name) {
                            declarations().enums.count[declarations().enums.index(*pending)] =
                                declarations().enumerators.name[declarations().enumerators.index(
                                    declarations().items(
                                        declarations().enums.values[declarations().enums.index(
                                            *pending)])[index])];
                        }
                        selected_after_edit =
                            text(declarations().enumerators.name[declarations().enumerators.index(
                                declarations().items(
                                    declarations().enums.values[declarations().enums.index(
                                        *pending)])[index])]);
                    }
                } else {
                    select_cell("name",
                                text(declarations()
                                         .enumerators.name[declarations().enumerators.index(value)])
                                    .c_str());
                }

                auto draw_optional_editor = [&](char const* label, auto& buffer, auto& column) {
                    ImGui::TableNextColumn();
                    if (row_selected) {
                        ImGui::SetNextItemWidth(-1.0F);
                        auto const submitted{
                            ImGui::InputText(label,
                                             buffer.data(),
                                             buffer.size(),
                                             ImGuiInputTextFlags_EnterReturnsTrue)};
                        if (!pending.has_value() &&
                            (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                            pending = declarations().enums.clone(*schema);
                            declarations().enums.values[declarations().enums.index(*pending)] =
                                [&] {
                                    auto const original_items{declarations().items(
                                        declarations()
                                            .enums.values[declarations().enums.index(*pending)])};
                                    auto values{
                                        std::vector(original_items.begin(), original_items.end())};
                                    values[index] = [&] {
                                        auto value{declarations().enumerators.clone(values[index])};
                                        column[declarations().enumerators.index(value)] =
                                            store_text(optional_text(buffer));
                                        return value;
                                    }();
                                    return declarations().range(values);
                                }();
                        }
                    } else {
                        auto const content{column[declarations().enumerators.index(value)]};
                        select_cell(label, content ? text(*content).c_str() : "-");
                    }
                };
                draw_optional_editor(
                    "##value", enum_value_initializer_, declarations().enumerators.initializer);
                ImGui::TableNextColumn();
                if (analysis_session_.results().enum_domain.has_value() &&
                    index < analysis_session_.results().enum_domain->enumerators.size() &&
                    analysis_session_.results().enum_domain->enumerators[index].code.has_value()) {
                    auto const code{lispb::schema::format_enum_code(
                        *analysis_session_.results().enum_domain->enumerators[index].code)};
                    if (row_selected) {
                        ImGui::TextUnformatted(code.c_str());
                    } else {
                        select_cell("derived_code", code.c_str());
                    }
                } else {
                    if (row_selected) {
                        ImGui::TextDisabled("Unknown");
                    } else {
                        select_cell("derived_code", "Unknown");
                    }
                }
                draw_optional_editor(
                    "##display", enum_value_display_name_, declarations().enumerators.display_name);
                draw_optional_editor("##serialized",
                                     enum_value_serialized_name_,
                                     declarations().enumerators.serialized_name);

                ImGui::TableNextColumn();
                auto hidden{
                    declarations().enumerators.hidden[declarations().enumerators.index(value)] !=
                    0};
                if (row_selected) {
                    if (ImGui::Checkbox("##hidden", &hidden)) {
                        pending = declarations().enums.clone(*schema);
                        declarations().enums.values[declarations().enums.index(*pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations().enums.values[declarations().enums.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().enumerators.clone(rows_1[index])};
                                declarations()
                                    .enumerators.hidden[declarations().enumerators.index(row_0)] =
                                    hidden;
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                    }
                } else {
                    select_cell("hidden", hidden ? "yes" : "-");
                }

                ImGui::TableNextColumn();
                auto sentinel{
                    declarations().enumerators.sentinel[declarations().enumerators.index(value)] !=
                    0};
                if (row_selected) {
                    if (ImGui::Checkbox("##sentinel", &sentinel)) {
                        pending = declarations().enums.clone(*schema);
                        declarations().enums.values[declarations().enums.index(*pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations().enums.values[declarations().enums.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().enumerators.clone(rows_1[index])};
                                declarations()
                                    .enumerators.sentinel[declarations().enumerators.index(row_0)] =
                                    sentinel;
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                    }
                } else {
                    select_cell("sentinel", sentinel ? "yes" : "-");
                }

                ImGui::TableNextColumn();
                auto count_sentinel{
                    document_->storage()
                        .declarations.enums
                        .count[document_->storage().declarations.enums.index(*schema)] ==
                    declarations().enumerators.name[declarations().enumerators.index(value)]};
                if (row_selected) {
                    if (ImGui::Checkbox("##count", &count_sentinel)) {
                        pending = declarations().enums.clone(*schema);
                        if (count_sentinel) {
                            declarations().enums.count[declarations().enums.index(*pending)] =
                                declarations()
                                    .enumerators.name[declarations().enumerators.index(value)];
                        } else if (declarations()
                                       .enums.count[declarations().enums.index(*pending)] ==
                                   declarations()
                                       .enumerators.name[declarations().enumerators.index(value)]) {
                            declarations()
                                .enums.count[declarations().enums.index(*pending)]
                                .reset();
                        }
                    }
                } else {
                    select_cell("count", count_sentinel ? "yes" : "-");
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }

        if (pending.has_value()) {
            if (text(
                    declarations().enumerators.name[declarations().enumerators.index(
                        declarations().items(declarations().enums.values[declarations().enums.index(
                            *pending)])[*selected_index])])
                    .empty()) {
                schema_edit_message_ = "Enumerator name cannot be empty.";
                return;
            }
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(*pending)))) {
                selected_enumerator_ = std::move(selected_after_edit);
            }
        }
    }
}

auto PlannerUi::draw_integer_scalar_editor(TypeId const node, TypeDefinitionId const& scalar)
    -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const declaration{
        document_->find_declaration(analysis_session_.inputs.workspace.types().identity(node))};
    if (!declaration.has_value()) {
        return false;
    }
    auto const schema{document_->declaration_handle(
        *declaration, lispb::codegen::DeclarationKind::integer_scalar)};
    if (!schema.has_value()) {
        return false;
    }

    if (selected_integer_scalar_code_.empty() &&
        !declarations()
             .items(
                 document_->storage()
                     .declarations.integer_scalars
                     .named_codes[document_->storage().declarations.integer_scalars.index(*schema)])
             .empty()) {
        selected_integer_scalar_code_ =
            text(declarations().packed_named_codes.name[declarations().packed_named_codes.index(
                declarations()
                    .items(document_->storage().declarations.integer_scalars.named_codes
                               [document_->storage().declarations.integer_scalars.index(*schema)])
                    .front())]);
    }
    auto selected{std::ranges::find(
        declarations().items(
            document_->storage()
                .declarations.integer_scalars
                .named_codes[document_->storage().declarations.integer_scalars.index(*schema)]),
        strings().intern(selected_integer_scalar_code_),
        [&](auto id) {
            return declarations()
                .packed_named_codes.name[declarations().packed_named_codes.index(id)];
        })};
    if (selected ==
            declarations()
                .items(document_->storage().declarations.integer_scalars.named_codes
                           [document_->storage().declarations.integer_scalars.index(*schema)])
                .end() &&
        !declarations()
             .items(
                 document_->storage()
                     .declarations.integer_scalars
                     .named_codes[document_->storage().declarations.integer_scalars.index(*schema)])
             .empty()) {
        selected =
            declarations()
                .items(document_->storage().declarations.integer_scalars.named_codes
                           [document_->storage().declarations.integer_scalars.index(*schema)])
                .begin();
        selected_integer_scalar_code_ =
            text(declarations()
                     .packed_named_codes.name[declarations().packed_named_codes.index(*selected)]);
    }
    auto const selected_index{
        selected ==
                declarations()
                    .items(document_->storage().declarations.integer_scalars.named_codes
                               [document_->storage().declarations.integer_scalars.index(*schema)])
                    .end()
            ? std::optional<std::size_t>{}
            : std::optional<std::size_t>{static_cast<std::size_t>(
                  selected -
                  declarations()
                      .items(document_->storage().declarations.integer_scalars.named_codes
                                 [document_->storage().declarations.integer_scalars.index(*schema)])
                      .begin())}};
    if (integer_scalar_editor_declaration_ != declaration) {
        integer_scalar_editor_declaration_ = declaration;
        integer_scalar_editor_code_.clear();
        std::snprintf(integer_scalar_minimum_.data(),
                      integer_scalar_minimum_.size(),
                      "%s",
                      lispb::codegen::format_packed_integer(
                          document_->storage().declarations.integer_scalars.minimum_value
                              [document_->storage().declarations.integer_scalars.index(*schema)])
                          .c_str());
        std::snprintf(integer_scalar_maximum_.data(),
                      integer_scalar_maximum_.size(),
                      "%s",
                      lispb::codegen::format_packed_integer(
                          document_->storage().declarations.integer_scalars.maximum_value
                              [document_->storage().declarations.integer_scalars.index(*schema)])
                          .c_str());
        std::snprintf(
            integer_scalar_cpp_type_.data(),
            integer_scalar_cpp_type_.size(),
            "%s",
            document_->storage()
                    .declarations.integer_scalars
                    .cpp_type[document_->storage().declarations.integer_scalars.index(*schema)]
                    .has_value()
                ? text(declarations().type_refs.name[declarations().type_refs.index(
                           (*document_->storage().declarations.integer_scalars.cpp_type
                                 [document_->storage().declarations.integer_scalars.index(
                                     *schema)]))])
                      .c_str()
                : "");
        if (document_->storage()
                .declarations.integer_scalars
                .relationship[document_->storage().declarations.integer_scalars.index(*schema)]
                .has_value()) {
            std::snprintf(
                integer_scalar_relationship_target_.data(),
                integer_scalar_relationship_target_.size(),
                "%s",
                text(declarations().type_refs.name[declarations().type_refs.index(
                         declarations()
                             .semantic_relations.target[declarations().semantic_relations.index(
                                 (*document_->storage().declarations.integer_scalars.relationship
                                       [document_->storage().declarations.integer_scalars.index(
                                           *schema)]))])])
                    .c_str());
            auto const kind{std::ranges::find(
                semantic_relationship_kinds,
                declarations().semantic_relations.kind[declarations().semantic_relations.index(
                    (*document_->storage().declarations.integer_scalars.relationship
                          [document_->storage().declarations.integer_scalars.index(*schema)]))])};
            integer_scalar_relationship_kind_ =
                kind == semantic_relationship_kinds.end()
                    ? 0
                    : static_cast<int>(kind - semantic_relationship_kinds.begin());
            auto const unit{
                declarations()
                        .semantic_relations
                        .unit[declarations().semantic_relations.index(
                            (*document_->storage().declarations.integer_scalars.relationship
                                  [document_->storage().declarations.integer_scalars.index(
                                      *schema)]))]
                        .has_value()
                    ? std::ranges::find(
                          semantic_relationship_units,
                          *declarations()
                               .semantic_relations.unit[declarations().semantic_relations.index(
                                   (*document_->storage().declarations.integer_scalars.relationship
                                         [document_->storage().declarations.integer_scalars.index(
                                             *schema)]))])
                    : semantic_relationship_units.end()};
            integer_scalar_relationship_unit_ =
                unit == semantic_relationship_units.end()
                    ? 0
                    : static_cast<int>(unit - semantic_relationship_units.begin());
        } else {
            integer_scalar_relationship_target_.front() = '\0';
            integer_scalar_relationship_kind_ = 0;
            integer_scalar_relationship_unit_ = 0;
        }
    }
    if (integer_scalar_editor_code_ != selected_integer_scalar_code_) {
        integer_scalar_editor_code_ = selected_integer_scalar_code_;
        if (selected !=
            declarations()
                .items(document_->storage().declarations.integer_scalars.named_codes
                           [document_->storage().declarations.integer_scalars.index(*schema)])
                .end()) {
            std::snprintf(
                integer_scalar_code_name_.data(),
                integer_scalar_code_name_.size(),
                "%s",
                text(declarations().packed_named_codes.name[declarations().packed_named_codes.index(
                         *selected)])
                    .c_str());
            std::snprintf(
                integer_scalar_code_value_.data(),
                integer_scalar_code_value_.size(),
                "%s",
                lispb::codegen::format_packed_integer(
                    declarations().packed_named_codes.value[declarations().packed_named_codes.index(
                        *selected)])
                    .c_str());
            integer_scalar_code_sentinel_ =
                declarations().packed_named_codes.sentinel[declarations().packed_named_codes.index(
                    *selected)];
        }
    }

    auto signedness{
        document_->storage()
            .declarations.integer_scalars
            .signedness[document_->storage().declarations.integer_scalars.index(*schema)] != 0};
    if (ImGui::Checkbox("Signed domain", &signedness)) {
        auto replacement{document_->storage().declarations.integer_scalars.clone(*schema)};
        document_->storage()
            .declarations.integer_scalars
            .signedness[document_->storage().declarations.integer_scalars.index(replacement)] =
            signedness;
        if (apply_document_edit(
                document_->replace_declaration(*declaration, std::move(replacement)))) {
            integer_scalar_editor_declaration_.reset();
            return true;
        }
    }
    auto automatic_width{
        !document_->storage()
             .declarations.integer_scalars
             .bit_width[document_->storage().declarations.integer_scalars.index(*schema)]
             .has_value()};
    if (ImGui::Checkbox("Auto bit width", &automatic_width)) {
        auto replacement{document_->storage().declarations.integer_scalars.clone(*schema)};
        document_->storage()
            .declarations.integer_scalars
            .bit_width[document_->storage().declarations.integer_scalars.index(replacement)] =
            automatic_width
                ? std::nullopt
                : std::optional{analysis_session_.results().integer_scalar_analysis.has_value()
                                    ? analysis_session_.results()
                                          .integer_scalar_analysis->minimum_required_bits
                                    : std::uint32_t{1}};
        if (apply_document_edit(
                document_->replace_declaration(*declaration, std::move(replacement)))) {
            integer_scalar_editor_declaration_.reset();
            return true;
        }
    }
    if (document_->storage()
            .declarations.integer_scalars
            .bit_width[document_->storage().declarations.integer_scalars.index(*schema)]
            .has_value()) {
        auto bit_width{
            *document_->storage()
                 .declarations.integer_scalars
                 .bit_width[document_->storage().declarations.integer_scalars.index(*schema)]};
        ImGui::SetNextItemWidth(140.0F);
        auto const submitted{ImGui::InputScalar("Bit width",
                                                ImGuiDataType_U32,
                                                &bit_width,
                                                nullptr,
                                                nullptr,
                                                "%u",
                                                ImGuiInputTextFlags_EnterReturnsTrue)};
        if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
            if (bit_width == 0 || bit_width > 64) {
                schema_edit_message_ = "Integer scalar width must be between 1 and 64 bits.";
            } else {
                auto replacement{document_->storage().declarations.integer_scalars.clone(*schema)};
                document_->storage().declarations.integer_scalars.bit_width
                    [document_->storage().declarations.integer_scalars.index(replacement)] =
                    static_cast<std::uint8_t>(bit_width);
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
                    integer_scalar_editor_declaration_.reset();
                    return true;
                }
            }
        }
    }

    ImGui::SetNextItemWidth(130.0F);
    auto range_submitted{ImGui::InputText("Minimum",
                                          integer_scalar_minimum_.data(),
                                          integer_scalar_minimum_.size(),
                                          ImGuiInputTextFlags_EnterReturnsTrue)};
    range_submitted = range_submitted || ImGui::IsItemDeactivatedAfterEdit();
    ImGui::SetNextItemWidth(130.0F);
    auto const maximum_submitted{ImGui::InputText("Maximum",
                                                  integer_scalar_maximum_.data(),
                                                  integer_scalar_maximum_.size(),
                                                  ImGuiInputTextFlags_EnterReturnsTrue)};
    range_submitted = range_submitted || maximum_submitted || ImGui::IsItemDeactivatedAfterEdit();
    if (range_submitted) {
        auto const minimum{detail::parse_packed_integer(integer_scalar_minimum_.data())};
        auto const maximum{detail::parse_packed_integer(integer_scalar_maximum_.data())};
        if (!minimum.has_value() || !maximum.has_value()) {
            schema_edit_message_ =
                "Scalar bounds must be signed decimal or hexadecimal integer values.";
        } else {
            auto replacement{document_->storage().declarations.integer_scalars.clone(*schema)};
            document_->storage().declarations.integer_scalars.minimum_value
                [document_->storage().declarations.integer_scalars.index(replacement)] = *minimum;
            document_->storage().declarations.integer_scalars.maximum_value
                [document_->storage().declarations.integer_scalars.index(replacement)] = *maximum;
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
                integer_scalar_editor_declaration_.reset();
                return true;
            }
        }
    }
    text_disabled_wrapped(
        "This is a semantic domain. Packed fields choose their storage independently; "
        "alias emission supplies a C++ representation for ordinary layout.");

    if (detail::section("C++ output policy", false)) {
        auto emission{static_cast<int>(
            document_->storage()
                .declarations.integer_scalars
                .cpp_emission[document_->storage().declarations.integer_scalars.index(*schema)])};
        ImGui::SetNextItemWidth(-1.0F);
        if (ImGui::Combo(
                "##cpp-emission", &emission, "None\0Constants\0Constants with names\0Alias\0")) {
            auto replacement{document_->storage().declarations.integer_scalars.clone(*schema)};
            document_->storage().declarations.integer_scalars.cpp_emission
                [document_->storage().declarations.integer_scalars.index(replacement)] =
                static_cast<lispb::codegen::IntegerScalarCppEmission>(emission);
            if (document_->storage().declarations.integer_scalars.cpp_emission
                    [document_->storage().declarations.integer_scalars.index(replacement)] ==
                lispb::codegen::IntegerScalarCppEmission::none) {
                document_->storage()
                    .declarations.integer_scalars
                    .cpp_type[document_->storage().declarations.integer_scalars.index(replacement)]
                    .reset();
            } else if (!document_->storage()
                            .declarations.integer_scalars
                            .cpp_type[document_->storage().declarations.integer_scalars.index(
                                replacement)]
                            .has_value()) {
                if (integer_scalar_cpp_type_.front() == '\0') {
                    schema_edit_message_ = "Enter a C++ output type before enabling emission.";
                } else {
                    document_->storage()
                        .declarations.integer_scalars
                        .cpp_type[document_->storage().declarations.integer_scalars.index(
                            replacement)] = [&] {
                        auto const new_type_refs{declarations().type_refs.append()};
                        declarations()
                            .type_refs.name[declarations().type_refs.index(new_type_refs)] =
                            strings().intern(integer_scalar_cpp_type_.data());
                        declarations()
                            .type_refs.suffix[declarations().type_refs.index(new_type_refs)] = {};
                        declarations()
                            .type_refs.nested[declarations().type_refs.index(new_type_refs)] =
                            std::nullopt;
                        return new_type_refs;
                    }();
                }
            }
            if (document_->storage().declarations.integer_scalars.cpp_emission
                        [document_->storage().declarations.integer_scalars.index(replacement)] ==
                    lispb::codegen::IntegerScalarCppEmission::none ||
                document_->storage()
                    .declarations.integer_scalars
                    .cpp_type[document_->storage().declarations.integer_scalars.index(replacement)]
                    .has_value()) {
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
                    integer_scalar_editor_declaration_.reset();
                    return true;
                }
            }
        }
        auto const* cpp_type_label{
            document_->storage().declarations.integer_scalars.cpp_emission
                        [document_->storage().declarations.integer_scalars.index(*schema)] ==
                    lispb::codegen::IntegerScalarCppEmission::alias
                ? "C++ representation"
            : document_->storage().declarations.integer_scalars.cpp_emission
                        [document_->storage().declarations.integer_scalars.index(*schema)] ==
                    lispb::codegen::IntegerScalarCppEmission::none
                ? "C++ output type"
                : "Constants type"};
        detail::prepare_property_input(cpp_type_label);
        auto const submitted{ImGui::InputText("##scalar-cpp-representation",
                                              integer_scalar_cpp_type_.data(),
                                              integer_scalar_cpp_type_.size(),
                                              ImGuiInputTextFlags_EnterReturnsTrue)};
        if ((submitted || ImGui::IsItemDeactivatedAfterEdit()) &&
            document_->storage().declarations.integer_scalars.cpp_emission
                    [document_->storage().declarations.integer_scalars.index(*schema)] !=
                lispb::codegen::IntegerScalarCppEmission::none) {
            auto replacement{document_->storage().declarations.integer_scalars.clone(*schema)};
            document_->storage().declarations.integer_scalars.cpp_type
                [document_->storage().declarations.integer_scalars.index(replacement)] = [&] {
                auto const new_type_refs{declarations().type_refs.append()};
                declarations().type_refs.name[declarations().type_refs.index(new_type_refs)] =
                    strings().intern(integer_scalar_cpp_type_.data());
                declarations().type_refs.suffix[declarations().type_refs.index(new_type_refs)] = {};
                declarations().type_refs.nested[declarations().type_refs.index(new_type_refs)] =
                    std::nullopt;
                return new_type_refs;
            }();
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
                integer_scalar_editor_declaration_.reset();
                return true;
            }
        }
        text_disabled_wrapped(
            "Alias emits using Scalar = Representation; it does not create a distinct C++ type. "
            "Constants modes type only the emitted constants and do not establish scalar layout. "
            "They require named codes. LispB scalar identity is unchanged.");
    }
    if (detail::section("Semantic relationship", false)) {
        auto const current_kind{semantic_relationship_kinds[static_cast<std::size_t>(
            std::clamp(integer_scalar_relationship_kind_,
                       0,
                       static_cast<int>(semantic_relationship_kinds.size() - 1)))]};
        auto const current_unit{semantic_relationship_units[static_cast<std::size_t>(
            std::clamp(integer_scalar_relationship_unit_,
                       0,
                       static_cast<int>(semantic_relationship_units.size() - 1)))]};
        ImGui::TextUnformatted("Kind");
        ImGui::SetItemTooltip(
            "Whether this value is an index, count, offset, or another semantic link.");
        ImGui::SetNextItemWidth(-1.0F);
        if (ImGui::BeginCombo("##relationship-kind",
                              lispb::codegen::semantic_relation_kind_name(current_kind).data())) {
            for (std::size_t kind_index{}; kind_index < semantic_relationship_kinds.size();
                 ++kind_index) {
                auto const kind{semantic_relationship_kinds[kind_index]};
                auto const chosen{integer_scalar_relationship_kind_ ==
                                  static_cast<int>(kind_index)};
                if (ImGui::Selectable(lispb::codegen::semantic_relation_kind_name(kind).data(),
                                      chosen)) {
                    integer_scalar_relationship_kind_ = static_cast<int>(kind_index);
                    if (document_->storage()
                            .declarations.integer_scalars
                            .relationship[document_->storage().declarations.integer_scalars.index(
                                *schema)]
                            .has_value()) {
                        auto replacement{
                            document_->storage().declarations.integer_scalars.clone(*schema)};
                        (*document_->storage()
                              .declarations.integer_scalars
                              .relationship[document_->storage().declarations.integer_scalars.index(
                                  replacement)]) = [&] {
                            auto row_0{declarations().semantic_relations.clone(
                                (*document_->storage().declarations.integer_scalars.relationship
                                      [document_->storage().declarations.integer_scalars.index(
                                          replacement)]))};
                            declarations()
                                .semantic_relations
                                .kind[declarations().semantic_relations.index(row_0)] = kind;
                            return row_0;
                        }();
                        (*document_->storage()
                              .declarations.integer_scalars
                              .relationship[document_->storage().declarations.integer_scalars.index(
                                  replacement)]) = [&] {
                            auto row_0{declarations().semantic_relations.clone(
                                (*document_->storage().declarations.integer_scalars.relationship
                                      [document_->storage().declarations.integer_scalars.index(
                                          replacement)]))};
                            declarations()
                                .semantic_relations
                                .unit[declarations().semantic_relations.index(row_0)] =
                                kind == lispb::codegen::SemanticRelationKind::offset_into
                                    ? std::optional{current_unit}
                                    : std::nullopt;
                            return row_0;
                        }();
                        if (apply_document_edit(document_->replace_declaration(
                                *declaration, std::move(replacement)))) {
                            integer_scalar_editor_declaration_.reset();
                            ImGui::EndCombo();
                            return true;
                        }
                    }
                }
            }
            ImGui::EndCombo();
        }
        if (current_kind == lispb::codegen::SemanticRelationKind::offset_into) {
            ImGui::TextUnformatted("Unit");
            ImGui::SetItemTooltip("The unit used to measure an offset.");
            ImGui::SetNextItemWidth(-1.0F);
            if (ImGui::BeginCombo(
                    "##relationship-unit",
                    lispb::codegen::semantic_relation_unit_name(current_unit).data())) {
                for (std::size_t unit_index{}; unit_index < semantic_relationship_units.size();
                     ++unit_index) {
                    auto const unit{semantic_relationship_units[unit_index]};
                    auto const chosen{integer_scalar_relationship_unit_ ==
                                      static_cast<int>(unit_index)};
                    if (ImGui::Selectable(lispb::codegen::semantic_relation_unit_name(unit).data(),
                                          chosen)) {
                        integer_scalar_relationship_unit_ = static_cast<int>(unit_index);
                        if (document_->storage()
                                .declarations.integer_scalars
                                .relationship[document_->storage()
                                                  .declarations.integer_scalars.index(*schema)]
                                .has_value()) {
                            auto replacement{
                                document_->storage().declarations.integer_scalars.clone(*schema)};
                            (*document_->storage().declarations.integer_scalars.relationship
                                  [document_->storage().declarations.integer_scalars.index(
                                      replacement)]) = [&] {
                                auto row_0{declarations().semantic_relations.clone(
                                    (*document_->storage().declarations.integer_scalars.relationship
                                          [document_->storage().declarations.integer_scalars.index(
                                              replacement)]))};
                                declarations()
                                    .semantic_relations
                                    .unit[declarations().semantic_relations.index(row_0)] = unit;
                                return row_0;
                            }();
                            if (apply_document_edit(document_->replace_declaration(
                                    *declaration, std::move(replacement)))) {
                                integer_scalar_editor_declaration_.reset();
                                ImGui::EndCombo();
                                return true;
                            }
                        }
                    }
                }
                ImGui::EndCombo();
            }
        }

        ImGui::TextUnformatted("Target");
        ImGui::SetItemTooltip("The semantic type this scalar refers to.");
        ImGui::SetNextItemWidth(-1.0F);
        auto const target_submitted{ImGui::InputText("##relationship-target",
                                                     integer_scalar_relationship_target_.data(),
                                                     integer_scalar_relationship_target_.size(),
                                                     ImGuiInputTextFlags_EnterReturnsTrue)};
        if (document_->storage()
                .declarations.integer_scalars
                .relationship[document_->storage().declarations.integer_scalars.index(*schema)]
                .has_value() &&
            (target_submitted || ImGui::IsItemDeactivatedAfterEdit())) {
            if (integer_scalar_relationship_target_.front() == '\0') {
                schema_edit_message_ = "Relationship target cannot be empty.";
            } else {
                auto replacement{document_->storage().declarations.integer_scalars.clone(*schema)};
                (*document_->storage()
                      .declarations.integer_scalars
                      .relationship[document_->storage().declarations.integer_scalars.index(
                          replacement)]) = [&] {
                    auto row_1{declarations().semantic_relations.clone(
                        (*document_->storage()
                              .declarations.integer_scalars
                              .relationship[document_->storage().declarations.integer_scalars.index(
                                  replacement)]))};
                    declarations()
                        .semantic_relations.target[declarations().semantic_relations.index(row_1)] =
                        [&] {
                            auto row_0{declarations().type_refs.clone(
                                declarations()
                                    .semantic_relations
                                    .target[declarations().semantic_relations.index(row_1)])};
                            declarations().type_refs.name[declarations().type_refs.index(row_0)] =
                                strings().intern(integer_scalar_relationship_target_.data());
                            return row_0;
                        }();
                    return row_1;
                }();
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
                    integer_scalar_editor_declaration_.reset();
                    return true;
                }
            }
        }
        ImGui::PushID("integer-scalar-relationship-target");
        auto picked_relationship_target{
            draw_type_picker(analysis_session_.inputs.workspace.types().module_name(node),
                             analysis_session_.inputs.workspace.types().identity(node))};
        ImGui::PopID();
        if (picked_relationship_target.has_value()) {
            std::snprintf(integer_scalar_relationship_target_.data(),
                          integer_scalar_relationship_target_.size(),
                          "%s",
                          picked_relationship_target->c_str());
            auto replacement{document_->storage().declarations.integer_scalars.clone(*schema)};
            document_->storage().declarations.integer_scalars.relationship
                [document_->storage().declarations.integer_scalars.index(replacement)] = [&] {
                auto const new_semantic_relations{declarations().semantic_relations.append()};
                declarations().semantic_relations.kind[declarations().semantic_relations.index(
                    new_semantic_relations)] = current_kind;
                declarations().semantic_relations.target[declarations().semantic_relations.index(
                    new_semantic_relations)] = [&] {
                    auto const new_type_refs{declarations().type_refs.append()};
                    declarations().type_refs.name[declarations().type_refs.index(new_type_refs)] =
                        strings().intern(*picked_relationship_target);
                    declarations()
                        .type_refs.suffix[declarations().type_refs.index(new_type_refs)] = {};
                    declarations().type_refs.nested[declarations().type_refs.index(new_type_refs)] =
                        std::nullopt;
                    return new_type_refs;
                }();
                declarations().semantic_relations.unit[declarations().semantic_relations.index(
                    new_semantic_relations)] =
                    current_kind == lispb::codegen::SemanticRelationKind::offset_into
                        ? std::optional{current_unit}
                        : std::nullopt;
                return new_semantic_relations;
            }();
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
                integer_scalar_editor_declaration_.reset();
                return true;
            }
        }
        ImGui::SameLine();
        if (document_->storage()
                .declarations.integer_scalars
                .relationship[document_->storage().declarations.integer_scalars.index(*schema)]
                .has_value()) {
            auto const clear_clicked{ImGui::SmallButton("Clear")};
            ImGui::SetItemTooltip("Remove this semantic relationship.");
            if (clear_clicked) {
                auto replacement{document_->storage().declarations.integer_scalars.clone(*schema)};
                document_->storage()
                    .declarations.integer_scalars
                    .relationship[document_->storage().declarations.integer_scalars.index(
                        replacement)]
                    .reset();
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
                    integer_scalar_editor_declaration_.reset();
                    return true;
                }
            }
            ImGui::SameLine();
            if (lispb::schema::is_valid(
                    definitions().integer_scalars.relationship[definitions().integer_scalars.index(
                        scalar)]) &&
                detail::semantic_type_navigation_button()) {
                select_type(
                    analysis_session_.inputs.workspace.types()
                        .occurrences()
                        .type[analysis_session_.inputs.workspace.types().occurrences().index(
                            analysis_session_.inputs.workspace.types().relationships().target
                                [analysis_session_.inputs.workspace.types().relationships().index(
                                    definitions().integer_scalars.relationship
                                        [definitions().integer_scalars.index(scalar)])])]);
                analysis_session_.inputs.selection.field.clear();
                return true;
            }
            if (lispb::schema::is_valid(
                    definitions().integer_scalars.relationship[definitions().integer_scalars.index(
                        scalar)])) {
                ImGui::SetItemTooltip("Open the relationship target.");
            }
        } else {
            ImGui::BeginDisabled(integer_scalar_relationship_target_.front() == '\0');
            auto const add_clicked{ImGui::SmallButton("Add")};
            ImGui::SetItemTooltip("Add a relationship using this kind and target.");
            if (add_clicked) {
                auto replacement{document_->storage().declarations.integer_scalars.clone(*schema)};
                document_->storage().declarations.integer_scalars.relationship
                    [document_->storage().declarations.integer_scalars.index(replacement)] = [&] {
                    auto const new_semantic_relations{declarations().semantic_relations.append()};
                    declarations().semantic_relations.kind[declarations().semantic_relations.index(
                        new_semantic_relations)] = current_kind;
                    declarations()
                        .semantic_relations.target[declarations().semantic_relations.index(
                            new_semantic_relations)] = [&] {
                        auto const new_type_refs{declarations().type_refs.append()};
                        declarations()
                            .type_refs.name[declarations().type_refs.index(new_type_refs)] =
                            strings().intern(integer_scalar_relationship_target_.data());
                        declarations()
                            .type_refs.suffix[declarations().type_refs.index(new_type_refs)] = {};
                        declarations()
                            .type_refs.nested[declarations().type_refs.index(new_type_refs)] =
                            std::nullopt;
                        return new_type_refs;
                    }();
                    declarations().semantic_relations.unit[declarations().semantic_relations.index(
                        new_semantic_relations)] =
                        current_kind == lispb::codegen::SemanticRelationKind::offset_into
                            ? std::optional{current_unit}
                            : std::nullopt;
                    return new_semantic_relations;
                }();
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
                    integer_scalar_editor_declaration_.reset();
                    return true;
                }
            }
            ImGui::EndDisabled();
        }
        text_disabled_wrapped(
            "The relationship is durable semantic metadata; session capacity does not rewrite this "
            "domain or bit width.");
        if (analysis_session_.results().integer_scalar_analysis.has_value() &&
            analysis_session_.results()
                .integer_scalar_analysis->relationship_target_extent.has_value()) {
            auto const& analysis{*analysis_session_.results().integer_scalar_analysis};
            auto const kind{*analysis.relationship_kind};
            auto const term{detail::relationship_extent_term(kind)};
            auto const unit{detail::relationship_extent_unit(kind, analysis.relationship_unit)};
            auto const heading{"Session " + std::string{term} + " requirement"};
            if (section_with_tooltip(heading.c_str(),
                                     "Shows how the current target extent affects the required "
                                     "integer range and width.")) {
                ImGui::PushTextWrapPos(0.0F);
                ImGui::Text("Target %s: %llu %s",
                            term.data(),
                            static_cast<unsigned long long>(*analysis.relationship_target_extent),
                            unit.data());
                ImGui::Text(
                    "Live values: %s",
                    analysis.relationship_live_value_count.has_value()
                        ? detail::format_code_count(*analysis.relationship_live_value_count).c_str()
                        : "Unknown");
                auto const required_codes{
                    analysis.relationship_required_code_count.has_value()
                        ? detail::format_code_count(*analysis.relationship_required_code_count)
                    : analysis.relationship_minimum_required_bits.value_or(0) > 64
                        ? std::string{"> 2^64"}
                        : std::string{"Unknown"}};
                ImGui::Text("Required codes: %s", required_codes.c_str());
                ImGui::Text("Minimum width: %u bits", *analysis.relationship_minimum_required_bits);
                ImGui::Text("Current semantic width fits: %s",
                            *analysis.relationship_width_sufficient ? "Yes" : "No");
                ImGui::Text(
                    "Code-space %s limit: %s",
                    term.data(),
                    detail::format_number(analysis.relationship_code_space_capacity_limit).c_str());
                ImGui::Text("Code-space %s headroom: %s",
                            term.data(),
                            detail::format_number(analysis.relationship_capacity_headroom).c_str());
                ImGui::Text(
                    "Semantic-range %s limit: %s",
                    term.data(),
                    detail::format_number(analysis.relationship_semantic_capacity_limit).c_str());
                ImGui::Text(
                    "Sentinel-placement %s limit: %s",
                    term.data(),
                    detail::format_number(analysis.relationship_sentinel_capacity_limit).c_str());
                ImGui::Text(
                    "Effective valid %s limit: %s",
                    term.data(),
                    detail::format_number(analysis.relationship_effective_capacity_limit).c_str());
                ImGui::Text("Effective valid %s headroom: %s",
                            term.data(),
                            detail::format_number(analysis.relationship_effective_capacity_headroom)
                                .c_str());
                if (kind == lispb::codegen::SemanticRelationKind::count_of) {
                    text_disabled_wrapped(
                        "Live counts include 0 through capacity; named sentinels add code states.");
                } else if (kind == lispb::codegen::SemanticRelationKind::offset_into) {
                    text_disabled_wrapped(
                        "Live offsets span 0 through extent-1 in the declared unit; "
                        "named sentinels add "
                        "code states.");
                } else {
                    text_disabled_wrapped(
                        "Live indices span 0 through capacity-1; named sentinels add code states.");
                }
                ImGui::PopTextWrapPos();
            }
        }
    }
    if (detail::section("Named codes")) {
        auto const code_width{
            document_->storage()
                .declarations.integer_scalars
                .bit_width[document_->storage().declarations.integer_scalars.index(*schema)]
                .value_or(64)};
        auto const named_value{
            first_available_scalar_code(declarations(), *schema, code_width, false)};
        auto const sentinel_value{
            first_available_scalar_code(declarations(), *schema, code_width, true)};
        ImGui::BeginDisabled(!named_value.has_value());
        if (ImGui::Button("+ Named code")) {
            auto replacement{document_->storage().declarations.integer_scalars.clone(*schema)};
            auto name{unique_child_name(
                declarations(),
                declarations().packed_named_codes,
                declarations().packed_named_codes.name,
                declarations().items(
                    document_->storage().declarations.integer_scalars.named_codes
                        [document_->storage().declarations.integer_scalars.index(replacement)]),
                "Code")};
            document_->storage().declarations.integer_scalars.named_codes
                [document_->storage().declarations.integer_scalars.index(replacement)] = [&] {
                auto const original_items{declarations().items(
                    document_->storage().declarations.integer_scalars.named_codes
                        [document_->storage().declarations.integer_scalars.index(replacement)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back([&] {
                    auto const new_packed_named_codes{declarations().packed_named_codes.append()};
                    declarations().packed_named_codes.name[declarations().packed_named_codes.index(
                        new_packed_named_codes)] = strings().intern(name);
                    declarations().packed_named_codes.value[declarations().packed_named_codes.index(
                        new_packed_named_codes)] = *named_value;
                    declarations()
                        .packed_named_codes
                        .sentinel[declarations().packed_named_codes.index(new_packed_named_codes)] =
                        false;
                    return new_packed_named_codes;
                }());
                return declarations().range(rows);
            }();
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
                selected_integer_scalar_code_ = std::move(name);
                integer_scalar_editor_declaration_.reset();
                ImGui::EndDisabled();
                return true;
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!sentinel_value.has_value());
        if (ImGui::Button("+ Sentinel")) {
            auto replacement{document_->storage().declarations.integer_scalars.clone(*schema)};
            auto name{unique_child_name(
                declarations(),
                declarations().packed_named_codes,
                declarations().packed_named_codes.name,
                declarations().items(
                    document_->storage().declarations.integer_scalars.named_codes
                        [document_->storage().declarations.integer_scalars.index(replacement)]),
                "Invalid")};
            document_->storage().declarations.integer_scalars.named_codes
                [document_->storage().declarations.integer_scalars.index(replacement)] = [&] {
                auto const original_items{declarations().items(
                    document_->storage().declarations.integer_scalars.named_codes
                        [document_->storage().declarations.integer_scalars.index(replacement)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back([&] {
                    auto const new_packed_named_codes{declarations().packed_named_codes.append()};
                    declarations().packed_named_codes.name[declarations().packed_named_codes.index(
                        new_packed_named_codes)] = strings().intern(name);
                    declarations().packed_named_codes.value[declarations().packed_named_codes.index(
                        new_packed_named_codes)] = *sentinel_value;
                    declarations()
                        .packed_named_codes
                        .sentinel[declarations().packed_named_codes.index(new_packed_named_codes)] =
                        true;
                    return new_packed_named_codes;
                }());
                return declarations().range(rows);
            }();
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
                selected_integer_scalar_code_ = std::move(name);
                integer_scalar_editor_declaration_.reset();
                ImGui::EndDisabled();
                return true;
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        auto const duplicate_value{
            selected_index.has_value()
                ? first_available_scalar_code(
                      declarations(),
                      *schema,
                      code_width,
                      declarations()
                          .packed_named_codes
                          .sentinel[declarations().packed_named_codes.index(declarations().items(
                              document_->storage().declarations.integer_scalars.named_codes
                                  [document_->storage().declarations.integer_scalars.index(
                                      *schema)])[*selected_index])])
                : std::nullopt};
        ImGui::BeginDisabled(!selected_index.has_value() || !duplicate_value.has_value());
        if (ImGui::Button("Duplicate")) {
            auto replacement{document_->storage().declarations.integer_scalars.clone(*schema)};
            auto copy{declarations().packed_named_codes.clone(declarations().items(
                document_->storage()
                    .declarations.integer_scalars
                    .named_codes[document_->storage().declarations.integer_scalars.index(
                        replacement)])[*selected_index])};
            declarations().packed_named_codes.name[declarations().packed_named_codes.index(copy)] =
                strings().intern(unique_child_name(
                    declarations(),
                    declarations().packed_named_codes,
                    declarations().packed_named_codes.name,
                    declarations().items(
                        document_->storage().declarations.integer_scalars.named_codes
                            [document_->storage().declarations.integer_scalars.index(replacement)]),
                    text(declarations()
                             .packed_named_codes
                             .name[declarations().packed_named_codes.index(copy)]) +
                        "_copy"));
            declarations().packed_named_codes.value[declarations().packed_named_codes.index(copy)] =
                *duplicate_value;
            document_->storage().declarations.integer_scalars.named_codes
                [document_->storage().declarations.integer_scalars.index(replacement)] = [&] {
                auto const original_items{declarations().items(
                    document_->storage().declarations.integer_scalars.named_codes
                        [document_->storage().declarations.integer_scalars.index(replacement)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.insert(rows.begin() + static_cast<std::ptrdiff_t>(*selected_index + 1), copy);
                return declarations().range(rows);
            }();
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
                selected_integer_scalar_code_ = text(
                    declarations()
                        .packed_named_codes.name[declarations().packed_named_codes.index(copy)]);
                integer_scalar_editor_declaration_.reset();
                ImGui::EndDisabled();
                return true;
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!selected_index.has_value() || *selected_index == 0);
        if (ImGui::Button("Move up")) {
            auto replacement{document_->storage().declarations.integer_scalars.clone(*schema)};
            document_->storage().declarations.integer_scalars.named_codes
                [document_->storage().declarations.integer_scalars.index(replacement)] = [&] {
                auto const original_items{declarations().items(
                    document_->storage().declarations.integer_scalars.named_codes
                        [document_->storage().declarations.integer_scalars.index(replacement)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                std::swap(rows[*selected_index], rows[*selected_index - 1]);
                return declarations().range(rows);
            }();
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
                integer_scalar_editor_declaration_.reset();
                ImGui::EndDisabled();
                return true;
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(
            !selected_index.has_value() ||
            *selected_index + 1 >=
                declarations()
                    .items(document_->storage().declarations.integer_scalars.named_codes
                               [document_->storage().declarations.integer_scalars.index(*schema)])
                    .size());
        if (ImGui::Button("Move down")) {
            auto replacement{document_->storage().declarations.integer_scalars.clone(*schema)};
            document_->storage().declarations.integer_scalars.named_codes
                [document_->storage().declarations.integer_scalars.index(replacement)] = [&] {
                auto const original_items{declarations().items(
                    document_->storage().declarations.integer_scalars.named_codes
                        [document_->storage().declarations.integer_scalars.index(replacement)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                std::swap(rows[*selected_index], rows[*selected_index + 1]);
                return declarations().range(rows);
            }();
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
                integer_scalar_editor_declaration_.reset();
                ImGui::EndDisabled();
                return true;
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!selected_index.has_value());
        if (ImGui::Button("Delete")) {
            auto replacement{document_->storage().declarations.integer_scalars.clone(*schema)};
            document_->storage().declarations.integer_scalars.named_codes
                [document_->storage().declarations.integer_scalars.index(replacement)] = [&] {
                auto const original_items{declarations().items(
                    document_->storage().declarations.integer_scalars.named_codes
                        [document_->storage().declarations.integer_scalars.index(replacement)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.erase(rows.begin() + static_cast<std::ptrdiff_t>(*selected_index));
                return declarations().range(rows);
            }();
            auto const next_code{
                declarations()
                        .items(document_->storage().declarations.integer_scalars.named_codes
                                   [document_->storage().declarations.integer_scalars.index(
                                       replacement)])
                        .empty()
                    ? std::string{}
                    : text(declarations()
                               .packed_named_codes.name[declarations().packed_named_codes.index(
                                   declarations().items(
                                       document_->storage().declarations.integer_scalars.named_codes
                                           [document_->storage().declarations.integer_scalars.index(
                                               replacement)])
                                       [std::min(
                                           *selected_index,
                                           declarations()
                                                   .items(document_->storage()
                                                              .declarations.integer_scalars
                                                              .named_codes[document_->storage()
                                                                               .declarations
                                                                               .integer_scalars
                                                                               .index(replacement)])
                                                   .size() -
                                               1)])])};
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
                selected_integer_scalar_code_ = next_code;
                integer_scalar_editor_declaration_.reset();
                ImGui::EndDisabled();
                return true;
            }
        }
        ImGui::EndDisabled();

        std::optional<lispb::codegen::DeclarationHandle> pending;
        auto selected_after_edit{selected_integer_scalar_code_};
        if (detail::begin_editable_table(
                "integer-scalar-codes",
                4,
                declarations()
                    .items(document_->storage().declarations.integer_scalars.named_codes
                               [document_->storage().declarations.integer_scalars.index(*schema)])
                    .size())) {
            detail::editable_table_column("", ImGuiTableColumnFlags_WidthFixed);
            detail::editable_table_column("Name");
            detail::editable_table_column("Value");
            detail::editable_table_column("Sentinel", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableHeadersRow();
            for (std::size_t index{};
                 index <
                 declarations()
                     .items(document_->storage().declarations.integer_scalars.named_codes
                                [document_->storage().declarations.integer_scalars.index(*schema)])
                     .size();
                 ++index) {
                auto const code{declarations().items(
                    document_->storage().declarations.integer_scalars.named_codes
                        [document_->storage().declarations.integer_scalars.index(*schema)])[index]};
                auto const row_selected{
                    selected_integer_scalar_code_ ==
                    text(declarations()
                             .packed_named_codes
                             .name[declarations().packed_named_codes.index(code)])};
                ImGui::PushID(static_cast<int>(index));
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                if (detail::editable_table_row_handle(row_selected)) {
                    selected_integer_scalar_code_ =
                        text(declarations()
                                 .packed_named_codes
                                 .name[declarations().packed_named_codes.index(code)]);
                    integer_scalar_editor_code_.clear();
                }
                if (ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload("INTEGER_SCALAR_CODE_ROW", &index, sizeof(index));
                    ImGui::Text("Move %s",
                                text(declarations()
                                         .packed_named_codes
                                         .name[declarations().packed_named_codes.index(code)])
                                    .c_str());
                    ImGui::EndDragDropSource();
                }
                if (ImGui::BeginDragDropTarget()) {
                    if (auto const* payload{
                            ImGui::AcceptDragDropPayload("INTEGER_SCALAR_CODE_ROW")}) {
                        auto const source_index{*static_cast<std::size_t const*>(payload->Data)};
                        if (source_index <
                                declarations()
                                    .items(document_->storage()
                                               .declarations.integer_scalars
                                               .named_codes[document_->storage()
                                                                .declarations.integer_scalars.index(
                                                                    *schema)])
                                    .size() &&
                            source_index != index) {
                            pending = declarations().integer_scalars.clone(*schema);
                            selected_after_edit = text(
                                declarations().packed_named_codes.name
                                    [declarations().packed_named_codes.index(declarations().items(
                                        declarations().integer_scalars.named_codes
                                            [declarations().integer_scalars.index(
                                                *pending)])[source_index])]);
                            declarations()
                                .integer_scalars
                                .named_codes[declarations().integer_scalars.index(*pending)] = [&] {
                                auto const original_items{declarations().items(
                                    declarations().integer_scalars.named_codes
                                        [declarations().integer_scalars.index(*pending)])};
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
                if (row_selected) {
                    ImGui::SetNextItemWidth(-1.0F);
                    auto const submitted{ImGui::InputText("##name",
                                                          integer_scalar_code_name_.data(),
                                                          integer_scalar_code_name_.size(),
                                                          ImGuiInputTextFlags_EnterReturnsTrue)};
                    if (!pending.has_value() &&
                        (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                        pending = declarations().integer_scalars.clone(*schema);
                        declarations()
                            .integer_scalars
                            .named_codes[declarations().integer_scalars.index(*pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .integer_scalars
                                    .named_codes[declarations().integer_scalars.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_named_codes.clone(rows_1[index])};
                                declarations()
                                    .packed_named_codes
                                    .name[declarations().packed_named_codes.index(row_0)] =
                                    strings().intern(integer_scalar_code_name_.data());
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        selected_after_edit = text(
                            declarations()
                                .packed_named_codes
                                .name[declarations().packed_named_codes.index(declarations().items(
                                    declarations().integer_scalars.named_codes
                                        [declarations().integer_scalars.index(*pending)])[index])]);
                    }
                } else {
                    ImGui::TextUnformatted(
                        text(declarations()
                                 .packed_named_codes
                                 .name[declarations().packed_named_codes.index(code)])
                            .c_str());
                }

                ImGui::TableNextColumn();
                if (row_selected) {
                    ImGui::SetNextItemWidth(-1.0F);
                    auto const submitted{ImGui::InputText("##value",
                                                          integer_scalar_code_value_.data(),
                                                          integer_scalar_code_value_.size(),
                                                          ImGuiInputTextFlags_EnterReturnsTrue)};
                    if (!pending.has_value() &&
                        (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                        if (auto const value{
                                detail::parse_packed_integer(integer_scalar_code_value_.data())}) {
                            pending = declarations().integer_scalars.clone(*schema);
                            declarations()
                                .integer_scalars
                                .named_codes[declarations().integer_scalars.index(*pending)] = [&] {
                                auto const original_items{declarations().items(
                                    declarations().integer_scalars.named_codes
                                        [declarations().integer_scalars.index(*pending)])};
                                auto rows_1{
                                    std::vector(original_items.begin(), original_items.end())};
                                rows_1[index] = [&] {
                                    auto row_0{
                                        declarations().packed_named_codes.clone(rows_1[index])};
                                    declarations()
                                        .packed_named_codes
                                        .value[declarations().packed_named_codes.index(row_0)] =
                                        *value;
                                    return row_0;
                                }();
                                return declarations().range(rows_1);
                            }();
                        } else {
                            schema_edit_message_ =
                                "Named code value must be a signed decimal or hexadecimal integer.";
                        }
                    }
                } else {
                    auto const value{lispb::codegen::format_packed_integer(
                        declarations()
                            .packed_named_codes
                            .value[declarations().packed_named_codes.index(code)])};
                    ImGui::TextUnformatted(value.c_str());
                }

                ImGui::TableNextColumn();
                if (row_selected) {
                    auto sentinel{integer_scalar_code_sentinel_};
                    auto const alternate_value{first_available_scalar_code(
                        declarations(),
                        *schema,
                        code_width,
                        !declarations()
                             .packed_named_codes
                             .sentinel[declarations().packed_named_codes.index(code)])};
                    ImGui::BeginDisabled(!alternate_value.has_value());
                    if (ImGui::Checkbox("##sentinel", &sentinel)) {
                        pending = declarations().integer_scalars.clone(*schema);
                        declarations()
                            .integer_scalars
                            .named_codes[declarations().integer_scalars.index(*pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .integer_scalars
                                    .named_codes[declarations().integer_scalars.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_named_codes.clone(rows_1[index])};
                                declarations()
                                    .packed_named_codes
                                    .sentinel[declarations().packed_named_codes.index(row_0)] =
                                    sentinel;
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations()
                            .integer_scalars
                            .named_codes[declarations().integer_scalars.index(*pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .integer_scalars
                                    .named_codes[declarations().integer_scalars.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_named_codes.clone(rows_1[index])};
                                declarations()
                                    .packed_named_codes
                                    .value[declarations().packed_named_codes.index(row_0)] =
                                    *alternate_value;
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                    }
                    ImGui::EndDisabled();
                } else {
                    ImGui::TextUnformatted(
                        declarations()
                                .packed_named_codes
                                .sentinel[declarations().packed_named_codes.index(code)]
                            ? "yes"
                            : "-");
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }

        if (pending.has_value()) {
            if (std::ranges::any_of(
                    declarations().items(
                        declarations()
                            .integer_scalars
                            .named_codes[declarations().integer_scalars.index(*pending)]),
                    [&](auto id) {
                        return text(declarations()
                                        .packed_named_codes
                                        .name[declarations().packed_named_codes.index(id)])
                            .empty();
                    })) {
                schema_edit_message_ = "Named code name cannot be empty.";
                return false;
            }
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(*pending)))) {
                selected_integer_scalar_code_ = std::move(selected_after_edit);
                integer_scalar_editor_declaration_.reset();
                return true;
            }
        }
    }
    return false;
}

auto PlannerUi::draw_linear_quantized_editor(TypeId const node, TypeDefinitionId const&) -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const declaration{
        document_->find_declaration(analysis_session_.inputs.workspace.types().identity(node))};
    if (!declaration.has_value()) {
        return false;
    }
    auto const schema{document_->declaration_handle(
        *declaration, lispb::codegen::DeclarationKind::linear_quantized)};
    if (!schema.has_value()) {
        return false;
    }
    linear_quantized_editor_declaration_ = declaration;

    if (ImGui::BeginCombo(
            "Semantic source",
            text(declarations().type_refs.name[declarations().type_refs.index(
                     document_->storage().declarations.linear_quantizeds.source
                         [document_->storage().declarations.linear_quantizeds.index(*schema)])])
                .c_str())) {
        auto const candidate_types{document_->types().types()};
        for (std::size_t type_index{}, type_count{candidate_types.size()}; type_index < type_count;
             ++type_index) {
            auto const& candidate{candidate_types[type_index]};
            if (!integer_domain(document_->types(), TypeId{static_cast<std::uint32_t>(type_index)})
                     .has_value()) {
                continue;
            }
            auto const selected{
                integer_source_reference(analysis_session_.inputs.workspace.types(), candidate) ==
                text(declarations().type_refs.name[declarations().type_refs.index(
                    document_->storage().declarations.linear_quantizeds.source
                        [document_->storage().declarations.linear_quantizeds.index(*schema)])])};
            if (ImGui::Selectable(
                    integer_source_reference(analysis_session_.inputs.workspace.types(), candidate)
                        .c_str(),
                    selected)) {
                auto replacement{
                    document_->storage().declarations.linear_quantizeds.clone(*schema)};
                document_->storage().declarations.linear_quantizeds.source
                    [document_->storage().declarations.linear_quantizeds.index(replacement)] = [&] {
                    auto const new_type_refs{declarations().type_refs.append()};
                    declarations().type_refs.name[declarations().type_refs.index(new_type_refs)] =
                        strings().intern(integer_source_reference(
                            analysis_session_.inputs.workspace.types(), candidate));
                    declarations()
                        .type_refs.suffix[declarations().type_refs.index(new_type_refs)] = {};
                    declarations().type_refs.nested[declarations().type_refs.index(new_type_refs)] =
                        std::nullopt;
                    return new_type_refs;
                }();
                ImGui::EndCombo();
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
                    linear_quantized_editor_declaration_.reset();
                    return true;
                }
                return false;
            }
        }
        ImGui::EndCombo();
    }

    auto bit_width{
        document_->storage()
            .declarations.linear_quantizeds
            .bit_width[document_->storage().declarations.linear_quantizeds.index(*schema)]};
    ImGui::SetNextItemWidth(150.0F);
    auto const width_submitted{ImGui::InputScalar("Encoded bit width",
                                                  ImGuiDataType_U32,
                                                  &bit_width,
                                                  nullptr,
                                                  nullptr,
                                                  "%u",
                                                  ImGuiInputTextFlags_EnterReturnsTrue)};
    if (width_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
        if (bit_width == 0 || bit_width > 64) {
            schema_edit_message_ = "Encoded width must be between 1 and 64 bits.";
        } else {
            auto replacement{document_->storage().declarations.linear_quantizeds.clone(*schema)};
            document_->storage()
                .declarations.linear_quantizeds
                .bit_width[document_->storage().declarations.linear_quantizeds.index(replacement)] =
                static_cast<std::uint8_t>(bit_width);
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
                linear_quantized_editor_declaration_.reset();
                return true;
            }
        }
    }

    auto reserved_codes{
        document_->storage()
            .declarations.linear_quantizeds
            .reserved_codes[document_->storage().declarations.linear_quantizeds.index(*schema)]};
    ImGui::SetNextItemWidth(180.0F);
    auto const reserved_submitted{ImGui::InputScalar("Reserved codes",
                                                     ImGuiDataType_U64,
                                                     &reserved_codes,
                                                     nullptr,
                                                     nullptr,
                                                     "%llu",
                                                     ImGuiInputTextFlags_EnterReturnsTrue)};
    if (reserved_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
        auto const maximum_reserved{
            document_->storage().declarations.linear_quantizeds.bit_width
                        [document_->storage().declarations.linear_quantizeds.index(*schema)] == 64
                ? (std::numeric_limits<std::uint64_t>::max)() - 1
                : (std::uint64_t{1}
                   << document_->storage().declarations.linear_quantizeds.bit_width
                          [document_->storage().declarations.linear_quantizeds.index(*schema)]) -
                      2};
        if (reserved_codes > maximum_reserved) {
            schema_edit_message_ = "At least two usable encoded codes are required.";
        } else {
            auto replacement{document_->storage().declarations.linear_quantizeds.clone(*schema)};
            document_->storage().declarations.linear_quantizeds.reserved_codes
                [document_->storage().declarations.linear_quantizeds.index(replacement)] =
                reserved_codes;
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
                linear_quantized_editor_declaration_.reset();
                return true;
            }
        }
    }

    auto clipping{
        document_->storage()
                    .declarations.linear_quantizeds
                    .clipping[document_->storage().declarations.linear_quantizeds.index(*schema)] ==
                lispb::codegen::QuantizationClipping::reject
            ? 0
            : 1};
    constexpr std::array clipping_labels{"Reject", "Clamp"};
    if (ImGui::Combo("Out-of-range values",
                     &clipping,
                     clipping_labels.data(),
                     static_cast<int>(clipping_labels.size()))) {
        auto replacement{document_->storage().declarations.linear_quantizeds.clone(*schema)};
        document_->storage()
            .declarations.linear_quantizeds
            .clipping[document_->storage().declarations.linear_quantizeds.index(replacement)] =
            clipping == 0 ? lispb::codegen::QuantizationClipping::reject
                          : lispb::codegen::QuantizationClipping::clamp;
        if (apply_document_edit(
                document_->replace_declaration(*declaration, std::move(replacement)))) {
            linear_quantized_editor_declaration_.reset();
            return true;
        }
    }
    return false;
}

auto PlannerUi::draw_integer_varint_editor(TypeId const node, TypeDefinitionId const& varint)
    -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const declaration{
        document_->find_declaration(analysis_session_.inputs.workspace.types().identity(node))};
    if (!declaration.has_value()) {
        return false;
    }
    auto const schema{document_->declaration_handle(
        *declaration, lispb::codegen::DeclarationKind::integer_varint)};
    if (!schema.has_value()) {
        return false;
    }
    integer_varint_editor_declaration_ = declaration;

    if (ImGui::BeginCombo(
            "Semantic source",
            text(declarations().type_refs.name[declarations().type_refs.index(
                     document_->storage().declarations.integer_varints.source
                         [document_->storage().declarations.integer_varints.index(*schema)])])
                .c_str())) {
        auto const candidate_types{document_->types().types()};
        for (std::size_t type_index{}, type_count{candidate_types.size()}; type_index < type_count;
             ++type_index) {
            auto const& candidate{candidate_types[type_index]};
            auto const scalar{
                integer_domain(document_->types(), TypeId{static_cast<std::uint32_t>(type_index)})};
            if (!scalar) {
                continue;
            }
            auto const selected{
                integer_source_reference(analysis_session_.inputs.workspace.types(), candidate) ==
                text(declarations().type_refs.name[declarations().type_refs.index(
                    document_->storage().declarations.integer_varints.source
                        [document_->storage().declarations.integer_varints.index(*schema)])])};
            if (ImGui::Selectable(
                    integer_source_reference(analysis_session_.inputs.workspace.types(), candidate)
                        .c_str(),
                    selected)) {
                auto replacement{document_->storage().declarations.integer_varints.clone(*schema)};
                document_->storage().declarations.integer_varints.source
                    [document_->storage().declarations.integer_varints.index(replacement)] = [&] {
                    auto const new_type_refs{declarations().type_refs.append()};
                    declarations().type_refs.name[declarations().type_refs.index(new_type_refs)] =
                        strings().intern(integer_source_reference(
                            analysis_session_.inputs.workspace.types(), candidate));
                    declarations()
                        .type_refs.suffix[declarations().type_refs.index(new_type_refs)] = {};
                    declarations().type_refs.nested[declarations().type_refs.index(new_type_refs)] =
                        std::nullopt;
                    return new_type_refs;
                }();
                if (document_->types().definitions().integer_scalars.signedness
                        [document_->types().definitions().integer_scalars.index(*scalar)] &&
                    document_->storage()
                            .declarations.integer_varints
                            .encoding[document_->storage().declarations.integer_varints.index(
                                replacement)] ==
                        lispb::codegen::IntegerVarintEncoding::unsigned_varint) {
                    document_->storage().declarations.integer_varints.encoding
                        [document_->storage().declarations.integer_varints.index(replacement)] =
                        lispb::codegen::IntegerVarintEncoding::zigzag_varint;
                } else if (!document_->types().definitions().integer_scalars.signedness
                                [document_->types().definitions().integer_scalars.index(*scalar)]) {
                    document_->storage().declarations.integer_varints.encoding
                        [document_->storage().declarations.integer_varints.index(replacement)] =
                        lispb::codegen::IntegerVarintEncoding::unsigned_varint;
                }
                ImGui::EndCombo();
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
                    integer_varint_editor_declaration_.reset();
                    return true;
                }
                return false;
            }
        }
        ImGui::EndCombo();
    }

    auto const source_scalar{*integer_domain(
        analysis_session_.inputs.workspace.types(),
        analysis_session_.inputs.workspace.types()
            .occurrences()
            .type[analysis_session_.inputs.workspace.types().occurrences().index(
                definitions()
                    .integer_varints.source[definitions().integer_varints.index(varint)])])};
    auto const current_label{lispb::codegen::integer_varint_encoding_name(
        document_->storage()
            .declarations.integer_varints
            .encoding[document_->storage().declarations.integer_varints.index(*schema)])};
    if (ImGui::BeginCombo("Encoding", current_label.data())) {
        constexpr std::array encodings{lispb::codegen::IntegerVarintEncoding::unsigned_varint,
                                       lispb::codegen::IntegerVarintEncoding::signed_varint,
                                       lispb::codegen::IntegerVarintEncoding::zigzag_varint};
        for (auto const encoding : encodings) {
            auto const compatible{
                analysis_session_.inputs.workspace.types().definitions().integer_scalars.signedness
                    [analysis_session_.inputs.workspace.types().definitions().integer_scalars.index(
                        source_scalar)] ==
                (encoding != lispb::codegen::IntegerVarintEncoding::unsigned_varint)};
            if (!compatible) {
                continue;
            }
            auto const label{lispb::codegen::integer_varint_encoding_name(encoding)};
            auto const selected{
                encoding ==
                document_->storage()
                    .declarations.integer_varints
                    .encoding[document_->storage().declarations.integer_varints.index(*schema)]};
            if (ImGui::Selectable(label.data(), selected)) {
                auto replacement{document_->storage().declarations.integer_varints.clone(*schema)};
                document_->storage().declarations.integer_varints.encoding
                    [document_->storage().declarations.integer_varints.index(replacement)] =
                    encoding;
                ImGui::EndCombo();
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
                    integer_varint_editor_declaration_.reset();
                    return true;
                }
                return false;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    return false;
}

auto PlannerUi::draw_optional_sentinel_editor(TypeId const node, TypeDefinitionId const& optional)
    -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const declaration{
        document_->find_declaration(analysis_session_.inputs.workspace.types().identity(node))};
    if (!declaration.has_value()) {
        return false;
    }
    auto const schema{document_->declaration_handle(
        *declaration, lispb::codegen::DeclarationKind::optional_sentinel)};
    if (!schema.has_value()) {
        return false;
    }
    optional_sentinel_editor_declaration_ = declaration;

    auto const& current_source{
        analysis_session_.inputs.workspace.types()
            .occurrences()
            .type[analysis_session_.inputs.workspace.types().occurrences().index(
                definitions()
                    .optional_sentinels.source[definitions().optional_sentinels.index(optional)])]};
    if (ImGui::BeginCombo(
            "Source scalar",
            integer_source_reference(analysis_session_.inputs.workspace.types(), current_source)
                .c_str())) {
        auto const types{analysis_session_.inputs.workspace.types().types()};
        for (std::size_t index{}; index < types.size(); ++index) {
            auto const candidate{integer_domain(analysis_session_.inputs.workspace.types(),
                                                TypeId{static_cast<std::uint32_t>(index)})};
            if (!candidate ||
                !std::ranges::any_of(
                    declarations().items(
                        analysis_session_.inputs.workspace.types()
                            .definitions()
                            .integer_scalars.named_codes[analysis_session_.inputs.workspace.types()
                                                             .definitions()
                                                             .integer_scalars.index(*candidate)]),
                    [&](auto id) {
                        return declarations()
                            .packed_named_codes
                            .sentinel[declarations().packed_named_codes.index(id)];
                    })) {
                continue;
            }
            auto const type{TypeId{static_cast<std::uint32_t>(index)}};
            auto const selected{
                type == analysis_session_.inputs.workspace.types()
                            .occurrences()
                            .type[analysis_session_.inputs.workspace.types().occurrences().index(
                                definitions().optional_sentinels.source
                                    [definitions().optional_sentinels.index(optional)])]};
            if (ImGui::Selectable(integer_source_reference(
                                      analysis_session_.inputs.workspace.types(), types[index])
                                      .c_str(),
                                  selected)) {
                auto replacement{
                    document_->storage().declarations.optional_sentinels.clone(*schema)};
                document_->storage()
                    .declarations.optional_sentinels
                    .source[document_->storage().declarations.optional_sentinels.index(
                        replacement)] = [&] {
                    auto const new_type_refs{declarations().type_refs.append()};
                    declarations().type_refs.name[declarations().type_refs.index(new_type_refs)] =
                        strings().intern(integer_source_reference(
                            analysis_session_.inputs.workspace.types(), types[index]));
                    declarations()
                        .type_refs.suffix[declarations().type_refs.index(new_type_refs)] = {};
                    declarations().type_refs.nested[declarations().type_refs.index(new_type_refs)] =
                        std::nullopt;
                    return new_type_refs;
                }();
                auto const codes{declarations().items(
                    analysis_session_.inputs.workspace.types()
                        .definitions()
                        .integer_scalars.named_codes[analysis_session_.inputs.workspace.types()
                                                         .definitions()
                                                         .integer_scalars.index(*candidate)])};
                auto const existing_code{std::ranges::find(
                    codes,
                    document_->storage().declarations.optional_sentinels.sentinel
                        [document_->storage().declarations.optional_sentinels.index(replacement)],
                    [&](auto id) {
                        return declarations()
                            .packed_named_codes.name[declarations().packed_named_codes.index(id)];
                    })};
                if (existing_code == codes.end() ||
                    !declarations()
                         .packed_named_codes
                         .sentinel[declarations().packed_named_codes.index(*existing_code)]) {
                    auto const sentinel{std::ranges::find_if(codes, [&](auto id) {
                        return declarations()
                            .packed_named_codes
                            .sentinel[declarations().packed_named_codes.index(id)];
                    })};
                    document_->storage().declarations.optional_sentinels.sentinel
                        [document_->storage().declarations.optional_sentinels.index(replacement)] =
                        declarations()
                            .packed_named_codes
                            .name[declarations().packed_named_codes.index(*sentinel)];
                }
                ImGui::EndCombo();
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
                    optional_sentinel_editor_declaration_.reset();
                    return true;
                }
                return false;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    auto const scalar{*integer_domain(
        analysis_session_.inputs.workspace.types(),
        analysis_session_.inputs.workspace.types()
            .occurrences()
            .type[analysis_session_.inputs.workspace.types().occurrences().index(
                definitions().optional_sentinels.source[definitions().optional_sentinels.index(
                    optional)])])};
    if (ImGui::BeginCombo(
            "Absence sentinel",
            text(document_->storage()
                     .declarations.optional_sentinels
                     .sentinel[document_->storage().declarations.optional_sentinels.index(*schema)])
                .c_str())) {
        for (auto const id : declarations().items(
                 analysis_session_.inputs.workspace.types()
                     .definitions()
                     .integer_scalars.named_codes[analysis_session_.inputs.workspace.types()
                                                      .definitions()
                                                      .integer_scalars.index(scalar)])) {
            auto const code{id};
            if (!declarations()
                     .packed_named_codes.sentinel[declarations().packed_named_codes.index(code)]) {
                continue;
            }
            auto const selected{
                declarations()
                    .packed_named_codes.name[declarations().packed_named_codes.index(code)] ==
                document_->storage()
                    .declarations.optional_sentinels
                    .sentinel[document_->storage().declarations.optional_sentinels.index(*schema)]};
            auto const value{lispb::codegen::format_packed_integer(
                declarations()
                    .packed_named_codes.value[declarations().packed_named_codes.index(code)])};
            auto const label{
                text(declarations()
                         .packed_named_codes.name[declarations().packed_named_codes.index(code)]) +
                " = " + value};
            if (ImGui::Selectable(label.c_str(), selected)) {
                auto replacement{
                    document_->storage().declarations.optional_sentinels.clone(*schema)};
                document_->storage().declarations.optional_sentinels.sentinel
                    [document_->storage().declarations.optional_sentinels.index(replacement)] =
                    declarations()
                        .packed_named_codes.name[declarations().packed_named_codes.index(code)];
                ImGui::EndCombo();
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
                    optional_sentinel_editor_declaration_.reset();
                    return true;
                }
                return false;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    return false;
}

auto PlannerUi::draw_optional_presence_bit_editor(TypeId const node,
                                                  TypeDefinitionId const& optional) -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const declaration{
        document_->find_declaration(analysis_session_.inputs.workspace.types().identity(node))};
    if (!declaration.has_value()) {
        return false;
    }
    auto const schema{document_->declaration_handle(
        *declaration, lispb::codegen::DeclarationKind::optional_presence_bit)};
    if (!schema.has_value()) {
        return false;
    }
    optional_presence_bit_editor_declaration_ = declaration;

    auto const& current_source{
        analysis_session_.inputs.workspace.types()
            .occurrences()
            .type[analysis_session_.inputs.workspace.types().occurrences().index(
                definitions().optional_presence_bits.source
                    [definitions().optional_presence_bits.index(optional)])]};
    if (ImGui::BeginCombo(
            "Source scalar",
            integer_source_reference(analysis_session_.inputs.workspace.types(), current_source)
                .c_str())) {
        auto const types{analysis_session_.inputs.workspace.types().types()};
        for (std::size_t index{}; index < types.size(); ++index) {
            if (!integer_domain(analysis_session_.inputs.workspace.types(),
                                TypeId{static_cast<std::uint32_t>(index)})
                     .has_value()) {
                continue;
            }
            auto const type{TypeId{static_cast<std::uint32_t>(index)}};
            auto const selected{
                type == analysis_session_.inputs.workspace.types()
                            .occurrences()
                            .type[analysis_session_.inputs.workspace.types().occurrences().index(
                                definitions().optional_presence_bits.source
                                    [definitions().optional_presence_bits.index(optional)])]};
            if (ImGui::Selectable(integer_source_reference(
                                      analysis_session_.inputs.workspace.types(), types[index])
                                      .c_str(),
                                  selected)) {
                auto replacement{
                    document_->storage().declarations.optional_presence_bits.clone(*schema)};
                document_->storage()
                    .declarations.optional_presence_bits
                    .source[document_->storage().declarations.optional_presence_bits.index(
                        replacement)] = [&] {
                    auto const new_type_refs{declarations().type_refs.append()};
                    declarations().type_refs.name[declarations().type_refs.index(new_type_refs)] =
                        strings().intern(integer_source_reference(
                            analysis_session_.inputs.workspace.types(), types[index]));
                    declarations()
                        .type_refs.suffix[declarations().type_refs.index(new_type_refs)] = {};
                    declarations().type_refs.nested[declarations().type_refs.index(new_type_refs)] =
                        std::nullopt;
                    return new_type_refs;
                }();
                ImGui::EndCombo();
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
                    optional_presence_bit_editor_declaration_.reset();
                    return true;
                }
                return false;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    return false;
}

auto PlannerUi::draw_fixed_point_editor(TypeId const node, TypeDefinitionId const&) -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const declaration{
        document_->find_declaration(analysis_session_.inputs.workspace.types().identity(node))};
    if (!declaration.has_value()) {
        return false;
    }
    auto const schema{
        document_->declaration_handle(*declaration, lispb::codegen::DeclarationKind::fixed_point)};
    if (!schema.has_value()) {
        return false;
    }
    if (fixed_point_editor_declaration_ != declaration) {
        std::snprintf(
            fixed_point_minimum_.data(),
            fixed_point_minimum_.size(),
            "%s",
            (document_->storage()
                     .declarations.fixed_points
                     .minimum_value[document_->storage().declarations.fixed_points.index(*schema)]
                 ? text(*document_->storage().declarations.fixed_points.minimum_value
                             [document_->storage().declarations.fixed_points.index(*schema)])
                 : std::string{})
                .c_str());
        std::snprintf(
            fixed_point_maximum_.data(),
            fixed_point_maximum_.size(),
            "%s",
            (document_->storage()
                     .declarations.fixed_points
                     .maximum_value[document_->storage().declarations.fixed_points.index(*schema)]
                 ? text(*document_->storage().declarations.fixed_points.maximum_value
                             [document_->storage().declarations.fixed_points.index(*schema)])
                 : std::string{})
                .c_str());
        fixed_point_editor_declaration_ = declaration;
    }

    auto signedness{
        document_->storage()
            .declarations.fixed_points
            .signedness[document_->storage().declarations.fixed_points.index(*schema)] != 0};
    if (ImGui::Checkbox("Signed representation", &signedness)) {
        auto replacement{document_->storage().declarations.fixed_points.clone(*schema)};
        document_->storage()
            .declarations.fixed_points
            .signedness[document_->storage().declarations.fixed_points.index(replacement)] =
            signedness;
        if (signedness &&
            document_->storage().declarations.fixed_points.fractional_bits
                    [document_->storage().declarations.fixed_points.index(replacement)] ==
                document_->storage().declarations.fixed_points.total_bits
                    [document_->storage().declarations.fixed_points.index(replacement)]) {
            --document_->storage().declarations.fixed_points.fractional_bits
                  [document_->storage().declarations.fixed_points.index(replacement)];
        }
        if (apply_document_edit(
                document_->replace_declaration(*declaration, std::move(replacement)))) {
            fixed_point_editor_declaration_.reset();
            return true;
        }
    }

    auto total_bits{document_->storage()
                        .declarations.fixed_points
                        .total_bits[document_->storage().declarations.fixed_points.index(*schema)]};
    ImGui::SetNextItemWidth(150.0F);
    auto const total_submitted{ImGui::InputScalar("Total bit width",
                                                  ImGuiDataType_U32,
                                                  &total_bits,
                                                  nullptr,
                                                  nullptr,
                                                  "%u",
                                                  ImGuiInputTextFlags_EnterReturnsTrue)};
    if (total_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
        auto const minimum_total{
            document_->storage()
                .declarations.fixed_points
                .fractional_bits[document_->storage().declarations.fixed_points.index(*schema)] +
            (document_->storage()
                     .declarations.fixed_points
                     .signedness[document_->storage().declarations.fixed_points.index(*schema)]
                 ? 1U
                 : 0U)};
        if (total_bits == 0 || total_bits > 64 || total_bits < minimum_total) {
            schema_edit_message_ =
                document_->storage()
                        .declarations.fixed_points
                        .signedness[document_->storage().declarations.fixed_points.index(*schema)]
                    ? "Total width must be 1..64 and leave the sign plus all "
                      "fractional bits."
                    : "Total width must be 1..64 and contain all fractional "
                      "bits.";
        } else {
            auto replacement{document_->storage().declarations.fixed_points.clone(*schema)};
            document_->storage()
                .declarations.fixed_points
                .total_bits[document_->storage().declarations.fixed_points.index(replacement)] =
                total_bits;
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
                fixed_point_editor_declaration_.reset();
                return true;
            }
        }
    }

    auto fractional_bits{
        document_->storage()
            .declarations.fixed_points
            .fractional_bits[document_->storage().declarations.fixed_points.index(*schema)]};
    ImGui::SetNextItemWidth(150.0F);
    auto const fractional_submitted{ImGui::InputScalar("Fractional bit width",
                                                       ImGuiDataType_U32,
                                                       &fractional_bits,
                                                       nullptr,
                                                       nullptr,
                                                       "%u",
                                                       ImGuiInputTextFlags_EnterReturnsTrue)};
    if (fractional_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
        auto const maximum_fractional{
            document_->storage()
                .declarations.fixed_points
                .total_bits[document_->storage().declarations.fixed_points.index(*schema)] -
            (document_->storage()
                     .declarations.fixed_points
                     .signedness[document_->storage().declarations.fixed_points.index(*schema)]
                 ? 1U
                 : 0U)};
        if (fractional_bits > maximum_fractional) {
            schema_edit_message_ =
                document_->storage()
                        .declarations.fixed_points
                        .signedness[document_->storage().declarations.fixed_points.index(*schema)]
                    ? "Fractional width must leave one sign bit."
                    : "Fractional width cannot exceed total width.";
        } else {
            auto replacement{document_->storage().declarations.fixed_points.clone(*schema)};
            document_->storage().declarations.fixed_points.fractional_bits
                [document_->storage().declarations.fixed_points.index(replacement)] =
                fractional_bits;
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
                fixed_point_editor_declaration_.reset();
                return true;
            }
        }
    }

    auto rounding{
        document_->storage()
                    .declarations.fixed_points
                    .rounding[document_->storage().declarations.fixed_points.index(*schema)] ==
                lispb::codegen::FixedPointRounding::nearest_even
            ? 0
            : 1};
    constexpr std::array rounding_labels{"Nearest even", "Toward zero"};
    if (ImGui::Combo("Rounding policy",
                     &rounding,
                     rounding_labels.data(),
                     static_cast<int>(rounding_labels.size()))) {
        auto replacement{document_->storage().declarations.fixed_points.clone(*schema)};
        document_->storage()
            .declarations.fixed_points
            .rounding[document_->storage().declarations.fixed_points.index(replacement)] =
            rounding == 0 ? lispb::codegen::FixedPointRounding::nearest_even
                          : lispb::codegen::FixedPointRounding::toward_zero;
        if (apply_document_edit(
                document_->replace_declaration(*declaration, std::move(replacement)))) {
            fixed_point_editor_declaration_.reset();
            return true;
        }
    }

    auto edit_bound = [&](char const* label, std::array<char, 128>& buffer, bool const minimum) {
        ImGui::SetNextItemWidth(220.0F);
        auto const submitted{ImGui::InputText(
            label, buffer.data(), buffer.size(), ImGuiInputTextFlags_EnterReturnsTrue)};
        if (!submitted && !ImGui::IsItemDeactivatedAfterEdit()) {
            return false;
        }
        auto replacement{document_->storage().declarations.fixed_points.clone(*schema)};
        auto const value{buffer.front() == '\0' ? std::nullopt
                                                : std::optional{std::string{buffer.data()}}};
        if (minimum) {
            document_->storage()
                .declarations.fixed_points
                .minimum_value[document_->storage().declarations.fixed_points.index(replacement)] =
                store_text(value);
        } else {
            document_->storage()
                .declarations.fixed_points
                .maximum_value[document_->storage().declarations.fixed_points.index(replacement)] =
                store_text(value);
        }
        if (apply_document_edit(
                document_->replace_declaration(*declaration, std::move(replacement)))) {
            fixed_point_editor_declaration_.reset();
            return true;
        }
        return false;
    };
    if (edit_bound("Minimum value", fixed_point_minimum_, true) ||
        edit_bound("Maximum value", fixed_point_maximum_, false)) {
        return true;
    }
    ImGui::TextDisabled("Leave a bound empty to use the full representable endpoint.");
    return false;
}

auto PlannerUi::draw_mini_float_editor(TypeId const node, TypeDefinitionId const&) -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const declaration{
        document_->find_declaration(analysis_session_.inputs.workspace.types().identity(node))};
    if (!declaration.has_value()) {
        return false;
    }
    auto const schema{
        document_->declaration_handle(*declaration, lispb::codegen::DeclarationKind::mini_float)};
    if (!schema.has_value()) {
        return false;
    }

    auto has_sign{document_->storage()
                      .declarations.mini_floats
                      .sign_bits[document_->storage().declarations.mini_floats.index(*schema)] ==
                  1};
    if (ImGui::Checkbox("Sign bit", &has_sign)) {
        auto replacement{document_->storage().declarations.mini_floats.clone(*schema)};
        document_->storage()
            .declarations.mini_floats
            .sign_bits[document_->storage().declarations.mini_floats.index(replacement)] =
            has_sign ? 1U : 0U;
        auto const total{
            static_cast<std::uint64_t>(
                document_->storage()
                    .declarations.mini_floats
                    .sign_bits[document_->storage().declarations.mini_floats.index(replacement)]) +
            document_->storage()
                .declarations.mini_floats
                .exponent_bits[document_->storage().declarations.mini_floats.index(replacement)] +
            document_->storage().declarations.mini_floats.significand_bits
                [document_->storage().declarations.mini_floats.index(replacement)]};
        if (total > 64) {
            schema_edit_message_ = "Mini-float total width must not exceed 64 bits.";
        } else if (apply_document_edit(
                       document_->replace_declaration(*declaration, std::move(replacement)))) {
            return true;
        }
    }

    auto exponent_bits{
        document_->storage()
            .declarations.mini_floats
            .exponent_bits[document_->storage().declarations.mini_floats.index(*schema)]};
    ImGui::SetNextItemWidth(150.0F);
    auto const exponent_submitted{ImGui::InputScalar("Exponent bit width",
                                                     ImGuiDataType_U32,
                                                     &exponent_bits,
                                                     nullptr,
                                                     nullptr,
                                                     "%u",
                                                     ImGuiInputTextFlags_EnterReturnsTrue)};
    if (exponent_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
        auto const total{
            static_cast<std::uint64_t>(
                document_->storage()
                    .declarations.mini_floats
                    .sign_bits[document_->storage().declarations.mini_floats.index(*schema)]) +
            exponent_bits +
            document_->storage()
                .declarations.mini_floats
                .significand_bits[document_->storage().declarations.mini_floats.index(*schema)]};
        if (exponent_bits < 2 || exponent_bits > 15) {
            schema_edit_message_ = "Exponent width must be in the range 2..15.";
        } else if (total > 64) {
            schema_edit_message_ = "Mini-float total width must not exceed 64 bits.";
        } else {
            auto replacement{document_->storage().declarations.mini_floats.clone(*schema)};
            document_->storage()
                .declarations.mini_floats
                .exponent_bits[document_->storage().declarations.mini_floats.index(replacement)] =
                exponent_bits;
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
                return true;
            }
        }
    }

    auto significand_bits{
        document_->storage()
            .declarations.mini_floats
            .significand_bits[document_->storage().declarations.mini_floats.index(*schema)]};
    ImGui::SetNextItemWidth(150.0F);
    auto const significand_submitted{ImGui::InputScalar("Significand bit width",
                                                        ImGuiDataType_U32,
                                                        &significand_bits,
                                                        nullptr,
                                                        nullptr,
                                                        "%u",
                                                        ImGuiInputTextFlags_EnterReturnsTrue)};
    if (significand_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
        auto const total{
            static_cast<std::uint64_t>(
                document_->storage()
                    .declarations.mini_floats
                    .sign_bits[document_->storage().declarations.mini_floats.index(*schema)]) +
            document_->storage()
                .declarations.mini_floats
                .exponent_bits[document_->storage().declarations.mini_floats.index(*schema)] +
            significand_bits};
        if (significand_bits > 62) {
            schema_edit_message_ = "Significand width must be in the range 0..62.";
        } else if (total > 64) {
            schema_edit_message_ = "Mini-float total width must not exceed 64 bits.";
        } else {
            auto replacement{document_->storage().declarations.mini_floats.clone(*schema)};
            document_->storage().declarations.mini_floats.significand_bits
                [document_->storage().declarations.mini_floats.index(replacement)] =
                significand_bits;
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
                return true;
            }
        }
    }

    auto exponent_bias{
        document_->storage()
            .declarations.mini_floats
            .exponent_bias[document_->storage().declarations.mini_floats.index(*schema)]};
    ImGui::SetNextItemWidth(150.0F);
    auto const bias_submitted{ImGui::InputScalar("Exponent bias",
                                                 ImGuiDataType_S32,
                                                 &exponent_bias,
                                                 nullptr,
                                                 nullptr,
                                                 "%d",
                                                 ImGuiInputTextFlags_EnterReturnsTrue)};
    if (bias_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
        if (exponent_bias < -32'768 || exponent_bias > 32'767) {
            schema_edit_message_ = "Exponent bias must be in the range -32768..32767.";
        } else {
            auto replacement{document_->storage().declarations.mini_floats.clone(*schema)};
            document_->storage()
                .declarations.mini_floats
                .exponent_bias[document_->storage().declarations.mini_floats.index(replacement)] =
                exponent_bias;
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
                return true;
            }
        }
    }
    return false;
}

} // namespace ioj::layout_planner
