#include "planner_ui_properties_common.hpp"

#include <codegen/schema/declaration_schema.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/module_settings_store.h>
#include <codegen/schema/normal_module_schemas.h>
#include <codegen/schema/packed_bit_order.h>
#include <codegen/schema/packed_byte_order.h>
#include <codegen/schema/packed_field_kind.h>
#include <codegen/schema/packed_field_schemas.h>
#include <codegen/schema/packed_integer_value.h>
#include <codegen/schema/packed_named_code_schemas.h>
#include <codegen/schema/packed_reserved_bits_schemas.h>
#include <codegen/schema/packed_segment_id.h>
#include <codegen/schema/packed_segment_kind.h>
#include <codegen/schema/packed_value_schemas.h>
#include <codegen/schema/semantic_relation_kind.h>
#include <codegen/schema/semantic_relation_schemas.h>
#include <codegen/schema/type_refs.h>
#include <lispb/compiler_storage.h>
#include <lispb/indices.h>
#include <lispb/schema/packed_fields.h>
#include <lispb/schema/packed_reserved_segments.h>
#include <lispb/schema/packed_segment_id.h>
#include <lispb/schema/packed_segment_kind.h>
#include <lispb/schema/type_definitions.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace ioj::layout_planner {

auto PlannerUi::packed_name(lispb::codegen::PackedSegmentId const id) const -> std::string {
    return text(
        id.kind() == lispb::codegen::PackedSegmentKind::field
            ? declarations().packed_fields.name[declarations().packed_fields.index(id)]
            : declarations()
                  .packed_reserved_bits.name[declarations().packed_reserved_bits.index(id)]);
}

auto PlannerUi::packed_name(lispb::schema::PackedSegmentId const id) const -> std::string {
    auto const& store{analysis_session_.inputs.workspace.types().definitions()};
    return text(id.kind() == lispb::schema::PackedSegmentKind::field
                    ? store.packed_fields.name[store.packed_fields.index(id)]
                    : store.packed_reserved_bits.name[store.packed_reserved_bits.index(id)]);
}

auto PlannerUi::packed_bits(lispb::codegen::PackedSegmentId const id) const -> std::optional<int> {
    return id.kind() == lispb::codegen::PackedSegmentKind::field
             ? declarations().packed_fields.bits[declarations().packed_fields.index(id)]
             : declarations()
                   .packed_reserved_bits.bits[declarations().packed_reserved_bits.index(id)];
}

auto PlannerUi::packed_width(lispb::schema::PackedSegmentId const id) const -> std::uint32_t {
    auto const& store{analysis_session_.inputs.workspace.types().definitions()};
    return id.kind() == lispb::schema::PackedSegmentKind::field
             ? store.packed_fields.bit_width[store.packed_fields.index(id)]
             : store.packed_reserved_bits.bit_width[store.packed_reserved_bits.index(id)];
}

auto PlannerUi::rename_packed_segment(lispb::codegen::PackedSegmentId const id,
                                      lispb::IdentifierId const name)
    -> lispb::codegen::PackedSegmentId {
    auto const replacement{declarations().clone(id)};
    if (id.kind() == lispb::codegen::PackedSegmentKind::field) {
        declarations().packed_fields.name[declarations().packed_fields.index(replacement)] = name;
    } else {
        declarations()
            .packed_reserved_bits.name[declarations().packed_reserved_bits.index(replacement)] =
            name;
    }
    return replacement;
}

auto PlannerUi::resize_packed_segment(lispb::codegen::PackedSegmentId const id, int const bits)
    -> lispb::codegen::PackedSegmentId {
    auto const replacement{declarations().clone(id)};
    if (id.kind() == lispb::codegen::PackedSegmentKind::field) {
        declarations().packed_fields.bits[declarations().packed_fields.index(replacement)] = bits;
    } else {
        declarations()
            .packed_reserved_bits.bits[declarations().packed_reserved_bits.index(replacement)] =
            bits;
    }
    return replacement;
}

auto PlannerUi::draw_packed_editor(TypeId const node, TypeDefinitionId const& packed) -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const declaration{
        document_->find_declaration(analysis_session_.inputs.workspace.types().identity(node))};
    if (!declaration.has_value()) {
        return false;
    }
    auto const schema{
        document_->declaration_handle(*declaration, lispb::codegen::DeclarationKind::packed_value)};
    if (!schema.has_value()) {
        return false;
    }

    if (analysis_session_.inputs.selection.field.empty() &&
        !declarations()
             .items(document_->storage()
                        .declarations.packed_values
                        .segments[document_->storage().declarations.packed_values.index(*schema)])
             .empty()) {
        analysis_session_.inputs.selection.field = packed_name(
            declarations()
                .items(
                    document_->storage()
                        .declarations.packed_values
                        .segments[document_->storage().declarations.packed_values.index(*schema)])
                .front());
    }
    auto selected{std::ranges::find_if(
        declarations().items(
            document_->storage()
                .declarations.packed_values
                .segments[document_->storage().declarations.packed_values.index(*schema)]),
        [&](auto const& segment) {
            return packed_name(segment) == analysis_session_.inputs.selection.field;
        })};
    if (selected == declarations()
                        .items(document_->storage().declarations.packed_values.segments
                                   [document_->storage().declarations.packed_values.index(*schema)])
                        .end() &&
        !declarations()
             .items(document_->storage()
                        .declarations.packed_values
                        .segments[document_->storage().declarations.packed_values.index(*schema)])
             .empty()) {
        selected =
            declarations()
                .items(
                    document_->storage()
                        .declarations.packed_values
                        .segments[document_->storage().declarations.packed_values.index(*schema)])
                .begin();
        analysis_session_.inputs.selection.field = packed_name(*selected);
    }
    auto const selected_index{
        selected == declarations()
                        .items(document_->storage().declarations.packed_values.segments
                                   [document_->storage().declarations.packed_values.index(*schema)])
                        .end()
            ? std::optional<std::size_t>{}
            : std::optional<std::size_t>{static_cast<std::size_t>(
                  selected -
                  declarations()
                      .items(document_->storage().declarations.packed_values.segments
                                 [document_->storage().declarations.packed_values.index(*schema)])
                      .begin())}};

    if (packed_editor_declaration_ != declaration ||
        packed_editor_field_ != analysis_session_.inputs.selection.field) {
        packed_editor_declaration_ = declaration;
        packed_editor_field_ = analysis_session_.inputs.selection.field;
        std::snprintf(
            packed_storage_type_.data(),
            packed_storage_type_.size(),
            "%s",
            text(declarations().type_refs.name[declarations().type_refs.index(
                     document_->storage().declarations.packed_values.storage_type
                         [document_->storage().declarations.packed_values.index(*schema)])])
                .c_str());
        std::snprintf(
            packed_invalid_value_.data(),
            packed_invalid_value_.size(),
            "%s",
            document_->storage()
                    .declarations.packed_values
                    .invalid_value[document_->storage().declarations.packed_values.index(*schema)]
                    .has_value()
                ? std::to_string(
                      *document_->storage().declarations.packed_values.invalid_value
                           [document_->storage().declarations.packed_values.index(*schema)])
                      .c_str()
                : "");
        std::snprintf(packed_export_specifier_.data(),
                      packed_export_specifier_.size(),
                      "%s",
                      text(document_->storage().declarations.packed_values.export_specifier
                               [document_->storage().declarations.packed_values.index(*schema)])
                          .c_str());
        if (selected !=
            declarations()
                .items(
                    document_->storage()
                        .declarations.packed_values
                        .segments[document_->storage().declarations.packed_values.index(*schema)])
                .end()) {
            auto const& selected_name{packed_name(*selected)};
            std::snprintf(
                packed_field_name_.data(), packed_field_name_.size(), "%s", selected_name.c_str());
            if (auto const field{((*selected).kind() == lispb::codegen::PackedSegmentKind::field
                                      ? std::optional{*selected}
                                      : std::nullopt)}) {
                std::snprintf(
                    packed_field_default_.data(),
                    packed_field_default_.size(),
                    "%s",
                    declarations()
                        .packed_fields.default_value[declarations().packed_fields.index(*field)]
                        .transform(lispb::codegen::format_packed_integer)
                        .value_or("")
                        .c_str());
                std::snprintf(
                    packed_field_type_.data(),
                    packed_field_type_.size(),
                    "%s",
                    text(declarations().type_refs.name[declarations().type_refs.index(
                             declarations()
                                 .packed_fields.type[declarations().packed_fields.index(*field)])])
                        .c_str());
                std::snprintf(
                    packed_field_minimum_.data(),
                    packed_field_minimum_.size(),
                    "%s",
                    declarations()
                            .packed_fields.minimum_value[declarations().packed_fields.index(*field)]
                            .has_value()
                        ? lispb::codegen::format_packed_integer(
                              *declarations()
                                   .packed_fields
                                   .minimum_value[declarations().packed_fields.index(*field)])
                              .c_str()
                        : "");
                std::snprintf(
                    packed_field_maximum_.data(),
                    packed_field_maximum_.size(),
                    "%s",
                    declarations()
                            .packed_fields.maximum_value[declarations().packed_fields.index(*field)]
                            .has_value()
                        ? lispb::codegen::format_packed_integer(
                              *declarations()
                                   .packed_fields
                                   .maximum_value[declarations().packed_fields.index(*field)])
                              .c_str()
                        : "");
                std::snprintf(
                    packed_relationship_target_.data(),
                    packed_relationship_target_.size(),
                    "%s",
                    declarations()
                            .packed_fields.relationship[declarations().packed_fields.index(*field)]
                            .has_value()
                        ? text(declarations().type_refs.name[declarations().type_refs.index(
                                   declarations().semantic_relations.target
                                       [declarations().semantic_relations.index(
                                           *declarations().packed_fields.relationship
                                                [declarations().packed_fields.index(*field)])])])
                              .c_str()
                        : "");
                auto const relationship_kind{
                    declarations()
                            .packed_fields.relationship[declarations().packed_fields.index(*field)]
                            .has_value()
                        ? std::ranges::find(
                              semantic_relationship_kinds,
                              declarations()
                                  .semantic_relations.kind[declarations().semantic_relations.index(
                                      *declarations().packed_fields.relationship
                                           [declarations().packed_fields.index(*field)])])
                        : semantic_relationship_kinds.end()};
                packed_relationship_kind_ =
                    relationship_kind == semantic_relationship_kinds.end()
                        ? 0
                        : static_cast<int>(relationship_kind - semantic_relationship_kinds.begin());
                auto const relationship_unit{
                    declarations()
                                .packed_fields
                                .relationship[declarations().packed_fields.index(*field)]
                                .has_value() &&
                            declarations()
                                .semantic_relations
                                .unit[declarations().semantic_relations.index(
                                    *declarations().packed_fields.relationship
                                         [declarations().packed_fields.index(*field)])]
                                .has_value()
                        ? std::ranges::find(
                              semantic_relationship_units,
                              *declarations()
                                   .semantic_relations.unit[declarations().semantic_relations.index(
                                       *declarations().packed_fields.relationship
                                            [declarations().packed_fields.index(*field)])])
                        : semantic_relationship_units.end()};
                packed_relationship_unit_ =
                    relationship_unit == semantic_relationship_units.end()
                        ? 0
                        : static_cast<int>(relationship_unit - semantic_relationship_units.begin());
            } else {
                packed_field_type_.front() = '\0';
                packed_field_minimum_.front() = '\0';
                packed_field_maximum_.front() = '\0';
                packed_relationship_target_.front() = '\0';
                packed_relationship_kind_ = 0;
                packed_relationship_unit_ = 0;
            }
            auto const resolved_width{
                selected_index.has_value() &&
                        *selected_index < definitions().packed.segments_of(packed).size()
                    ? packed_width(definitions().packed.segments_of(packed)[*selected_index])
                    : std::uint32_t{1}};
            packed_field_bits_ =
                ((*selected).kind() == lispb::codegen::PackedSegmentKind::field
                     ? std::optional{*selected}
                     : std::nullopt)
                        .has_value()
                    ? declarations()
                          .packed_fields.bits[declarations().packed_fields.index(*selected)]
                          .value_or(static_cast<int>(resolved_width))
                    : static_cast<int>(resolved_width);
        }
    }

    auto const selected_schema_field{
        selected == declarations()
                        .items(document_->storage().declarations.packed_values.segments
                                   [document_->storage().declarations.packed_values.index(*schema)])
                        .end()
            ? std::nullopt
            : ((*selected).kind() == lispb::codegen::PackedSegmentKind::field
                   ? std::optional{*selected}
                   : std::nullopt)};
    auto const selected_resolved_field{
        selected_index.has_value() &&
                *selected_index < definitions().packed.segments_of(packed).size()
            ? ((definitions().packed.segments_of(packed)[*selected_index]).kind() ==
                       lispb::schema::PackedSegmentKind::field
                   ? std::optional{definitions().packed.segments_of(packed)[*selected_index]}
                   : std::nullopt)
            : std::nullopt};
    auto const selected_integer_scalar{
        selected_resolved_field.has_value() && selected_schema_field.has_value()
            ? packed_integer_domain(
                  analysis_session_.inputs.workspace.types(),
                  analysis_session_.inputs.workspace.types()
                      .occurrences()
                      .type[analysis_session_.inputs.workspace.types().occurrences().index(
                          analysis_session_.inputs.workspace.types()
                              .definitions()
                              .packed_fields
                              .semantic_type[analysis_session_.inputs.workspace.types()
                                                 .definitions()
                                                 .packed_fields.index(*selected_resolved_field)])],
                  *selected_schema_field)
            : std::nullopt};
    auto const selected_linear_quantized{
        selected_resolved_field.has_value()
            ? ((analysis_session_.inputs.workspace.types().definition(
                    analysis_session_.inputs.workspace.types()
                        .occurrences()
                        .type[analysis_session_.inputs.workspace.types().occurrences().index(
                            analysis_session_.inputs.workspace.types()
                                .definitions()
                                .packed_fields.semantic_type
                                    [analysis_session_.inputs.workspace.types()
                                         .definitions()
                                         .packed_fields.index(*selected_resolved_field)])]))
                           .kind() == lispb::schema::TypeDefinitionKind::linear_quantized
                   ? std::optional{analysis_session_.inputs.workspace.types().definition(
                         analysis_session_.inputs.workspace.types()
                             .occurrences()
                             .type[analysis_session_.inputs.workspace.types().occurrences().index(
                                 analysis_session_.inputs.workspace.types()
                                     .definitions()
                                     .packed_fields.semantic_type
                                         [analysis_session_.inputs.workspace.types()
                                              .definitions()
                                              .packed_fields.index(*selected_resolved_field)])])}
                   : std::nullopt)
            : std::nullopt};
    auto const selected_fixed_point{
        selected_resolved_field.has_value()
            ? ((analysis_session_.inputs.workspace.types().definition(
                    analysis_session_.inputs.workspace.types()
                        .occurrences()
                        .type[analysis_session_.inputs.workspace.types().occurrences().index(
                            analysis_session_.inputs.workspace.types()
                                .definitions()
                                .packed_fields.semantic_type
                                    [analysis_session_.inputs.workspace.types()
                                         .definitions()
                                         .packed_fields.index(*selected_resolved_field)])]))
                           .kind() == lispb::schema::TypeDefinitionKind::fixed_point
                   ? std::optional{analysis_session_.inputs.workspace.types().definition(
                         analysis_session_.inputs.workspace.types()
                             .occurrences()
                             .type[analysis_session_.inputs.workspace.types().occurrences().index(
                                 analysis_session_.inputs.workspace.types()
                                     .definitions()
                                     .packed_fields.semantic_type
                                         [analysis_session_.inputs.workspace.types()
                                              .definitions()
                                              .packed_fields.index(*selected_resolved_field)])])}
                   : std::nullopt)
            : std::nullopt};
    auto const selected_mini_float{
        selected_resolved_field.has_value()
            ? ((analysis_session_.inputs.workspace.types().definition(
                    analysis_session_.inputs.workspace.types()
                        .occurrences()
                        .type[analysis_session_.inputs.workspace.types().occurrences().index(
                            analysis_session_.inputs.workspace.types()
                                .definitions()
                                .packed_fields.semantic_type
                                    [analysis_session_.inputs.workspace.types()
                                         .definitions()
                                         .packed_fields.index(*selected_resolved_field)])]))
                           .kind() == lispb::schema::TypeDefinitionKind::mini_float
                   ? std::optional{analysis_session_.inputs.workspace.types().definition(
                         analysis_session_.inputs.workspace.types()
                             .occurrences()
                             .type[analysis_session_.inputs.workspace.types().occurrences().index(
                                 analysis_session_.inputs.workspace.types()
                                     .definitions()
                                     .packed_fields.semantic_type
                                         [analysis_session_.inputs.workspace.types()
                                              .definitions()
                                              .packed_fields.index(*selected_resolved_field)])])}
                   : std::nullopt)
            : std::nullopt};
    if (!selected_schema_field.has_value()) {
        selected_packed_code_.clear();
    } else if (std::ranges::find(
                   declarations().items(
                       declarations().packed_fields.named_codes[declarations().packed_fields.index(
                           *selected_schema_field)]),
                   strings().intern(selected_packed_code_),
                   [&](auto id) {
                       return declarations()
                           .packed_named_codes.name[declarations().packed_named_codes.index(id)];
                   }) ==
               declarations()
                   .items(
                       declarations().packed_fields.named_codes[declarations().packed_fields.index(
                           *selected_schema_field)])
                   .end()) {
        selected_packed_code_ =
            declarations()
                    .items(
                        declarations().packed_fields.named_codes[declarations().packed_fields.index(
                            *selected_schema_field)])
                    .empty()
                ? std::string{}
                : text(declarations()
                           .packed_named_codes.name[declarations().packed_named_codes.index(
                               declarations()
                                   .items(declarations().packed_fields.named_codes
                                              [declarations().packed_fields.index(
                                                  *selected_schema_field)])
                                   .front())]);
    }
    auto const selected_code{
        !selected_schema_field.has_value()
            ? std::span<lispb::codegen::PackedNamedCodeId const>::iterator{}
            : std::ranges::find(
                  declarations().items(
                      declarations().packed_fields.named_codes[declarations().packed_fields.index(
                          *selected_schema_field)]),
                  strings().intern(selected_packed_code_),
                  [&](auto id) {
                      return declarations()
                          .packed_named_codes.name[declarations().packed_named_codes.index(id)];
                  })};
    if (packed_code_editor_declaration_ != declaration ||
        packed_code_editor_field_ != analysis_session_.inputs.selection.field ||
        packed_code_editor_name_ != selected_packed_code_) {
        packed_code_editor_declaration_ = declaration;
        packed_code_editor_field_ = analysis_session_.inputs.selection.field;
        packed_code_editor_name_ = selected_packed_code_;
        if (selected_schema_field.has_value() &&
            selected_code !=
                declarations()
                    .items(
                        declarations().packed_fields.named_codes[declarations().packed_fields.index(
                            *selected_schema_field)])
                    .end()) {
            std::snprintf(
                packed_code_name_.data(),
                packed_code_name_.size(),
                "%s",
                text(declarations().packed_named_codes.name[declarations().packed_named_codes.index(
                         *selected_code)])
                    .c_str());
            std::snprintf(
                packed_code_value_.data(),
                packed_code_value_.size(),
                "%s",
                lispb::codegen::format_packed_integer(
                    declarations().packed_named_codes.value[declarations().packed_named_codes.index(
                        *selected_code)])
                    .c_str());
            packed_code_sentinel_ =
                declarations().packed_named_codes.sentinel[declarations().packed_named_codes.index(
                    *selected_code)];
        } else {
            packed_code_name_.front() = '\0';
            packed_code_value_.front() = '\0';
            packed_code_sentinel_ = false;
        }
    }

    detail::prepare_property_input("Export specifier (optional)");
    auto const export_submitted{ImGui::InputText("##Export specifier (optional)##packed",
                                                 packed_export_specifier_.data(),
                                                 packed_export_specifier_.size(),
                                                 ImGuiInputTextFlags_EnterReturnsTrue)};
    if (export_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
        auto const export_specifier{std::string_view{packed_export_specifier_.data()}};
        if (text(document_->storage().declarations.packed_values.export_specifier
                     [document_->storage().declarations.packed_values.index(*schema)]) !=
            export_specifier) {
            auto replacement{document_->storage().declarations.packed_values.clone(*schema)};
            document_->storage().declarations.packed_values.export_specifier
                [document_->storage().declarations.packed_values.index(replacement)] =
                strings().intern(export_specifier);
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
                analysis_session_.inputs.selection.field = packed_editor_field_;
                return true;
            }
        }
    }

    auto mutable_value{
        document_->storage()
            .declarations.packed_values
            .mutable_value[document_->storage().declarations.packed_values.index(*schema)] != 0};
    if (ImGui::Checkbox("Generate mutable field API", &mutable_value)) {
        auto replacement{document_->storage().declarations.packed_values.clone(*schema)};
        document_->storage()
            .declarations.packed_values
            .mutable_value[document_->storage().declarations.packed_values.index(replacement)] =
            mutable_value;
        if (apply_document_edit(
                document_->replace_declaration(*declaration, std::move(replacement)))) {
            analysis_session_.inputs.selection.field = packed_editor_field_;
            return true;
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Generates assert-backed setter APIs; it does not change the "
                          "packed bit layout or semantic value domain.");
    }

    detail::prepare_property_input("Storage type");
    auto const storage_submitted{ImGui::InputText("##Storage type",
                                                  packed_storage_type_.data(),
                                                  packed_storage_type_.size(),
                                                  ImGuiInputTextFlags_EnterReturnsTrue)};
    if (storage_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
        if (packed_storage_type_.front() == '\0') {
            schema_edit_message_ = "Packed storage type cannot be empty.";
        } else {
            auto replacement{document_->storage().declarations.packed_values.clone(*schema)};
            document_->storage()
                .declarations.packed_values
                .storage_type[document_->storage().declarations.packed_values.index(replacement)] =
                [&] {
                    auto row_0{declarations().type_refs.clone(
                        document_->storage().declarations.packed_values.storage_type
                            [document_->storage().declarations.packed_values.index(replacement)])};
                    declarations().type_refs.name[declarations().type_refs.index(row_0)] =
                        strings().intern(packed_storage_type_.data());
                    return row_0;
                }();
            if (apply_document_edit(
                    document_->replace_declaration(*declaration, std::move(replacement)))) {
                analysis_session_.inputs.selection.field = packed_editor_field_;
                return true;
            }
        }
    }

    constexpr std::array byte_order_labels{"Unspecified", "Little endian", "Big endian"};
    auto byte_order_index{
        document_->storage()
                .declarations.packed_values
                .byte_order[document_->storage().declarations.packed_values.index(*schema)]
                .has_value()
            ? (*document_->storage().declarations.packed_values.byte_order
                           [document_->storage().declarations.packed_values.index(*schema)] ==
                       lispb::codegen::PackedByteOrder::little_endian
                   ? 1
                   : 2)
            : 0};
    if (ImGui::Combo("Serialized byte order",
                     &byte_order_index,
                     byte_order_labels.data(),
                     static_cast<int>(byte_order_labels.size()))) {
        auto replacement{document_->storage().declarations.packed_values.clone(*schema)};
        document_->storage()
            .declarations.packed_values
            .byte_order[document_->storage().declarations.packed_values.index(replacement)] =
            byte_order_index == 0
                ? std::nullopt
                : std::optional{byte_order_index == 1
                                    ? lispb::codegen::PackedByteOrder::little_endian
                                    : lispb::codegen::PackedByteOrder::big_endian};
        if (apply_document_edit(
                document_->replace_declaration(*declaration, std::move(replacement)))) {
            analysis_session_.inputs.selection.field = packed_editor_field_;
            return true;
        }
    }

    constexpr std::array bit_order_labels{"Default (LSB-first)", "Explicit LSB-first", "MSB-first"};
    auto bit_order_index{
        document_->storage()
                .declarations.packed_values
                .bit_order[document_->storage().declarations.packed_values.index(*schema)]
                .has_value()
            ? (*document_->storage().declarations.packed_values.bit_order
                           [document_->storage().declarations.packed_values.index(*schema)] ==
                       lispb::codegen::PackedBitOrder::least_significant_first
                   ? 1
                   : 2)
            : 0};
    if (ImGui::Combo("Segment bit order",
                     &bit_order_index,
                     bit_order_labels.data(),
                     static_cast<int>(bit_order_labels.size()))) {
        auto replacement{document_->storage().declarations.packed_values.clone(*schema)};
        document_->storage()
            .declarations.packed_values
            .bit_order[document_->storage().declarations.packed_values.index(replacement)] =
            bit_order_index == 0
                ? std::nullopt
                : std::optional{bit_order_index == 1
                                    ? lispb::codegen::PackedBitOrder::least_significant_first
                                    : lispb::codegen::PackedBitOrder::most_significant_first};
        if (apply_document_edit(
                document_->replace_declaration(*declaration, std::move(replacement)))) {
            analysis_session_.inputs.selection.field = packed_editor_field_;
            return true;
        }
    }
    ImGui::TextDisabled(
        "Byte order describes serialized bytes; segment order controls numeric bit offsets.");

    detail::prepare_property_input("Invalid raw value");
    auto const invalid_submitted{ImGui::InputText("##Invalid raw value",
                                                  packed_invalid_value_.data(),
                                                  packed_invalid_value_.size(),
                                                  ImGuiInputTextFlags_EnterReturnsTrue)};
    if (invalid_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
        auto replacement{document_->storage().declarations.packed_values.clone(*schema)};
        if (packed_invalid_value_.front() == '\0') {
            document_->storage()
                .declarations.packed_values
                .invalid_value[document_->storage().declarations.packed_values.index(replacement)]
                .reset();
        } else if (auto const value{detail::parse_unsigned(packed_invalid_value_.data())}) {
            document_->storage()
                .declarations.packed_values
                .invalid_value[document_->storage().declarations.packed_values.index(replacement)] =
                *value;
        } else {
            schema_edit_message_ = "Invalid raw value must be an unsigned decimal or 0x value.";
            return false;
        }
        if (apply_document_edit(
                document_->replace_declaration(*declaration, std::move(replacement)))) {
            analysis_session_.inputs.selection.field = packed_editor_field_;
            return true;
        }
    }
    ImGui::TextDisabled("Leave invalid raw value empty when every storage code is valid.");

    if (ImGui::Button("+ Field")) {
        auto replacement{document_->storage().declarations.packed_values.clone(*schema)};
        auto name{unique_segment_name(
            declarations(),
            document_->storage()
                .declarations.packed_values
                .segments[document_->storage().declarations.packed_values.index(replacement)],
            "field")};
        document_->storage()
            .declarations.packed_values
            .segments[document_->storage().declarations.packed_values.index(replacement)] = [&] {
            auto const original_items{declarations().items(
                document_->storage()
                    .declarations.packed_values
                    .segments[document_->storage().declarations.packed_values.index(replacement)])};
            auto rows{std::vector(original_items.begin(), original_items.end())};
            rows.push_back([&] {
                auto const new_packed_fields{declarations().packed_fields.append()};
                declarations()
                    .packed_fields.name[declarations().packed_fields.index(new_packed_fields)] =
                    strings().intern(name);
                declarations().packed_fields.type[declarations().packed_fields.index(
                    new_packed_fields)] = [&] {
                    auto const new_type_refs{declarations().type_refs.append()};
                    declarations().type_refs.name[declarations().type_refs.index(new_type_refs)] =
                        strings().intern("std::uint8_t");
                    declarations()
                        .type_refs.suffix[declarations().type_refs.index(new_type_refs)] = {};
                    declarations().type_refs.nested[declarations().type_refs.index(new_type_refs)] =
                        std::nullopt;
                    return new_type_refs;
                }();
                declarations()
                    .packed_fields.bits[declarations().packed_fields.index(new_packed_fields)] = 1;
                declarations()
                    .packed_fields.kind[declarations().packed_fields.index(new_packed_fields)] =
                    lispb::codegen::PackedFieldKind::unsigned_integer;
                declarations().packed_fields.range_helper[declarations().packed_fields.index(
                    new_packed_fields)] = false;
                declarations().packed_fields.minimum_value[declarations().packed_fields.index(
                    new_packed_fields)] = std::nullopt;
                declarations().packed_fields.maximum_value[declarations().packed_fields.index(
                    new_packed_fields)] = std::nullopt;
                declarations().packed_fields.named_codes[declarations().packed_fields.index(
                    new_packed_fields)] = {};
                declarations().packed_fields.relationship[declarations().packed_fields.index(
                    new_packed_fields)] = std::nullopt;
                return new_packed_fields;
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
    if (ImGui::Button("+ Reserved")) {
        auto replacement{document_->storage().declarations.packed_values.clone(*schema)};
        auto name{unique_segment_name(
            declarations(),
            document_->storage()
                .declarations.packed_values
                .segments[document_->storage().declarations.packed_values.index(replacement)],
            "reserved")};
        document_->storage()
            .declarations.packed_values
            .segments[document_->storage().declarations.packed_values.index(replacement)] = [&] {
            auto const original_items{declarations().items(
                document_->storage()
                    .declarations.packed_values
                    .segments[document_->storage().declarations.packed_values.index(replacement)])};
            auto rows{std::vector(original_items.begin(), original_items.end())};
            rows.push_back([&] {
                auto const new_packed_reserved_bits{declarations().packed_reserved_bits.append()};
                declarations().packed_reserved_bits.name[declarations().packed_reserved_bits.index(
                    new_packed_reserved_bits)] = strings().intern(name);
                declarations().packed_reserved_bits.bits[declarations().packed_reserved_bits.index(
                    new_packed_reserved_bits)] = 1;
                return new_packed_reserved_bits;
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
    ImGui::BeginDisabled(!selected_index.has_value());
    if (ImGui::Button("Duplicate")) {
        auto replacement{document_->storage().declarations.packed_values.clone(*schema)};
        auto copy{declarations().items(
            document_->storage()
                .declarations.packed_values
                .segments[document_->storage().declarations.packed_values.index(
                    replacement)])[*selected_index]};
        auto const name{unique_segment_name(
            declarations(),
            document_->storage()
                .declarations.packed_values
                .segments[document_->storage().declarations.packed_values.index(replacement)],
            packed_name(copy) + "_copy")};
        copy = rename_packed_segment(copy, strings().intern(name));
        document_->storage()
            .declarations.packed_values
            .segments[document_->storage().declarations.packed_values.index(replacement)] = [&] {
            auto const original_items{declarations().items(
                document_->storage()
                    .declarations.packed_values
                    .segments[document_->storage().declarations.packed_values.index(replacement)])};
            auto rows{std::vector(original_items.begin(), original_items.end())};
            rows.insert(rows.begin() + static_cast<std::ptrdiff_t>(*selected_index + 1),
                        std::move(copy));
            return declarations().range(rows);
        }();
        if (apply_document_edit(
                document_->replace_declaration(*declaration, std::move(replacement)))) {
            analysis_session_.inputs.selection.field = name;
            ImGui::EndDisabled();
            return true;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() || *selected_index == 0);
    if (ImGui::Button("Move up")) {
        auto replacement{document_->storage().declarations.packed_values.clone(*schema)};
        document_->storage()
            .declarations.packed_values
            .segments[document_->storage().declarations.packed_values.index(replacement)] = [&] {
            auto const original_items{declarations().items(
                document_->storage()
                    .declarations.packed_values
                    .segments[document_->storage().declarations.packed_values.index(replacement)])};
            auto rows{std::vector(original_items.begin(), original_items.end())};
            std::swap(rows[*selected_index], rows[*selected_index - 1]);
            return declarations().range(rows);
        }();
        if (apply_document_edit(
                document_->replace_declaration(*declaration, std::move(replacement)))) {
            analysis_session_.inputs.selection.field = packed_editor_field_;
            ImGui::EndDisabled();
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
                .items(
                    document_->storage()
                        .declarations.packed_values
                        .segments[document_->storage().declarations.packed_values.index(*schema)])
                .size());
    if (ImGui::Button("Move down")) {
        auto replacement{document_->storage().declarations.packed_values.clone(*schema)};
        document_->storage()
            .declarations.packed_values
            .segments[document_->storage().declarations.packed_values.index(replacement)] = [&] {
            auto const original_items{declarations().items(
                document_->storage()
                    .declarations.packed_values
                    .segments[document_->storage().declarations.packed_values.index(replacement)])};
            auto rows{std::vector(original_items.begin(), original_items.end())};
            std::swap(rows[*selected_index], rows[*selected_index + 1]);
            return declarations().range(rows);
        }();
        if (apply_document_edit(
                document_->replace_declaration(*declaration, std::move(replacement)))) {
            analysis_session_.inputs.selection.field = packed_editor_field_;
            ImGui::EndDisabled();
            ImGui::EndDisabled();
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(
        !selected_index.has_value() ||
        declarations()
                .items(
                    document_->storage()
                        .declarations.packed_values
                        .segments[document_->storage().declarations.packed_values.index(*schema)])
                .size() == 1);
    if (ImGui::Button("Delete")) {
        auto replacement{document_->storage().declarations.packed_values.clone(*schema)};
        auto const deleted_name{packed_name(declarations().items(
            document_->storage()
                .declarations.packed_values
                .segments[document_->storage().declarations.packed_values.index(
                    replacement)])[*selected_index])};
        document_->storage()
            .declarations.packed_values
            .segments[document_->storage().declarations.packed_values.index(replacement)] = [&] {
            auto const original_items{declarations().items(
                document_->storage()
                    .declarations.packed_values
                    .segments[document_->storage().declarations.packed_values.index(replacement)])};
            auto rows{std::vector(original_items.begin(), original_items.end())};
            rows.erase(rows.begin() + static_cast<std::ptrdiff_t>(*selected_index));
            return declarations().range(rows);
        }();
        auto const next_index{std::min(
            *selected_index,
            declarations()
                    .items(document_->storage().declarations.packed_values.segments
                               [document_->storage().declarations.packed_values.index(replacement)])
                    .size() -
                1)};
        auto const next_name{packed_name(declarations().items(
            document_->storage().declarations.packed_values.segments
                [document_->storage().declarations.packed_values.index(replacement)])[next_index])};
        if (apply_document_edit(
                document_->replace_declaration(*declaration, std::move(replacement)))) {
            analysis_session_.inputs.selection.packed_access_fields.erase(deleted_name);
            analysis_session_.inputs.selection.field = next_name;
            ImGui::EndDisabled();
            ImGui::EndDisabled();
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Selected only")) {
        analysis_session_.inputs.selection.packed_access_fields.clear();
        analysis_session_.inputs.selection.packed_access_set_explicit = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Access all")) {
        analysis_session_.inputs.selection.packed_access_fields.clear();
        for (auto const segment : declarations().items(
                 document_->storage()
                     .declarations.packed_values
                     .segments[document_->storage().declarations.packed_values.index(*schema)])) {
            if (auto const field{((segment).kind() == lispb::codegen::PackedSegmentKind::field
                                      ? std::optional{segment}
                                      : std::nullopt)}) {
                analysis_session_.inputs.selection.packed_access_fields.insert_or_assign(
                    text(declarations()
                             .packed_fields.name[declarations().packed_fields.index(*field)]),
                    analysis_session_.inputs.access_operation);
            }
        }
        analysis_session_.inputs.selection.packed_access_set_explicit = true;
    }

    std::optional<lispb::codegen::DeclarationHandle> pending;
    std::optional<TypeId> navigate_to;
    std::optional<std::pair<std::string, std::string>> renamed_field;
    auto selected_after_edit{analysis_session_.inputs.selection.field};
    if (detail::begin_editable_table(
            "packed-schema-fields",
            12,
            declarations()
                .items(
                    document_->storage()
                        .declarations.packed_values
                        .segments[document_->storage().declarations.packed_values.index(*schema)])
                .size())) {
        detail::editable_table_column("", ImGuiTableColumnFlags_WidthFixed);
        detail::editable_table_column("Access", ImGuiTableColumnFlags_WidthFixed);
        detail::editable_table_column("Operation", ImGuiTableColumnFlags_WidthFixed);
        detail::editable_table_column("Name");
        detail::editable_table_column("Semantic type");
        detail::editable_table_column("Bits", ImGuiTableColumnFlags_WidthFixed);
        detail::editable_table_column("Auto", ImGuiTableColumnFlags_WidthFixed);
        detail::editable_table_column("Kind");
        detail::editable_table_column("Range", ImGuiTableColumnFlags_WidthFixed);
        detail::editable_table_column("Semantic range");
        detail::editable_table_column("Representable");
        detail::editable_table_column("Default");
        constexpr std::array header_tips{
            "Select a segment or drag to reorder its bit position.",
            "Include this field in the modeled access workload.",
            "R = read, W = write, RW = read and write; affects analysis only.",
            "Field or reserved-region name.",
            "LispB semantic type; use the picker or arrow to inspect its declaration.",
            "Bits occupied in the packed storage integer.",
            "Derive width from the semantic type or field domain.",
            "Encoding kind; semantic types own their encoding where applicable.",
            "Generate an integer range-check helper for this field.",
            "Allowed semantic values, including separately declared sentinel codes.",
            "Values representable by the chosen field width or encoding.",
            "Numeric constructor value (raw/encoded for representations). Blank is implicit zero "
            "when all defaults are blank, unless an invalid sentinel is configured. "
            "Partial explicit defaults disable default construction."};
        ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
        for (int column{}; column < static_cast<int>(header_tips.size()); ++column) {
            ImGui::TableSetColumnIndex(column);
            ImGui::TableHeader(ImGui::TableGetColumnName(column));
            ImGui::SetItemTooltip("%s", header_tips[static_cast<std::size_t>(column)]);
        }
        auto default_gap{false};
        auto const explicit_defaults{std::ranges::any_of(
            declarations().items(
                document_->storage()
                    .declarations.packed_values
                    .segments[document_->storage().declarations.packed_values.index(*schema)]),
            [&](auto const segment) {
                auto const field{((segment).kind() == lispb::codegen::PackedSegmentKind::field
                                      ? std::optional{segment}
                                      : std::nullopt)};
                return field.has_value() &&
                       declarations()
                           .packed_fields.default_value[declarations().packed_fields.index(*field)]
                           .has_value();
            })};
        for (std::size_t index{};
             index <
             declarations()
                 .items(
                     document_->storage()
                         .declarations.packed_values
                         .segments[document_->storage().declarations.packed_values.index(*schema)])
                 .size();
             ++index) {
            auto const segment{declarations().items(
                document_->storage().declarations.packed_values.segments
                    [document_->storage().declarations.packed_values.index(*schema)])[index]};
            auto const field{((segment).kind() == lispb::codegen::PackedSegmentKind::field
                                  ? std::optional{segment}
                                  : std::nullopt)};
            auto const resolved_field{
                index < definitions().packed.segments_of(packed).size()
                    ? ((definitions().packed.segments_of(packed)[index]).kind() ==
                               lispb::schema::PackedSegmentKind::field
                           ? std::optional{definitions().packed.segments_of(packed)[index]}
                           : std::nullopt)
                    : std::nullopt};
            auto const field_uses_integer_scalar{
                resolved_field.has_value() && field.has_value() &&
                packed_integer_domain(
                    analysis_session_.inputs.workspace.types(),
                    analysis_session_.inputs.workspace.types()
                        .occurrences()
                        .type[analysis_session_.inputs.workspace.types().occurrences().index(
                            analysis_session_.inputs.workspace.types()
                                .definitions()
                                .packed_fields
                                .semantic_type[analysis_session_.inputs.workspace.types()
                                                   .definitions()
                                                   .packed_fields.index(*resolved_field)])],
                    *field)
                    .has_value()};
            auto const field_linear_quantized{
                resolved_field.has_value()
                    ? ((analysis_session_.inputs.workspace.types().definition(
                            analysis_session_.inputs.workspace.types().occurrences().type
                                [analysis_session_.inputs.workspace.types().occurrences().index(
                                    analysis_session_.inputs.workspace.types()
                                        .definitions()
                                        .packed_fields.semantic_type
                                            [analysis_session_.inputs.workspace.types()
                                                 .definitions()
                                                 .packed_fields.index(*resolved_field)])]))
                                   .kind() == lispb::schema::TypeDefinitionKind::linear_quantized
                           ? std::optional{analysis_session_.inputs.workspace.types().definition(
                                 analysis_session_.inputs.workspace.types().occurrences().type
                                     [analysis_session_.inputs.workspace.types()
                                          .occurrences()
                                          .index(analysis_session_.inputs.workspace.types()
                                                     .definitions()
                                                     .packed_fields.semantic_type
                                                         [analysis_session_.inputs.workspace.types()
                                                              .definitions()
                                                              .packed_fields.index(
                                                                  *resolved_field)])])}
                           : std::nullopt)
                    : std::nullopt};
            auto const field_fixed_point{
                resolved_field.has_value()
                    ? ((analysis_session_.inputs.workspace.types().definition(
                            analysis_session_.inputs.workspace.types().occurrences().type
                                [analysis_session_.inputs.workspace.types().occurrences().index(
                                    analysis_session_.inputs.workspace.types()
                                        .definitions()
                                        .packed_fields.semantic_type
                                            [analysis_session_.inputs.workspace.types()
                                                 .definitions()
                                                 .packed_fields.index(*resolved_field)])]))
                                   .kind() == lispb::schema::TypeDefinitionKind::fixed_point
                           ? std::optional{analysis_session_.inputs.workspace.types().definition(
                                 analysis_session_.inputs.workspace.types().occurrences().type
                                     [analysis_session_.inputs.workspace.types()
                                          .occurrences()
                                          .index(analysis_session_.inputs.workspace.types()
                                                     .definitions()
                                                     .packed_fields.semantic_type
                                                         [analysis_session_.inputs.workspace.types()
                                                              .definitions()
                                                              .packed_fields.index(
                                                                  *resolved_field)])])}
                           : std::nullopt)
                    : std::nullopt};
            auto const field_mini_float{
                resolved_field.has_value()
                    ? ((analysis_session_.inputs.workspace.types().definition(
                            analysis_session_.inputs.workspace.types().occurrences().type
                                [analysis_session_.inputs.workspace.types().occurrences().index(
                                    analysis_session_.inputs.workspace.types()
                                        .definitions()
                                        .packed_fields.semantic_type
                                            [analysis_session_.inputs.workspace.types()
                                                 .definitions()
                                                 .packed_fields.index(*resolved_field)])]))
                                   .kind() == lispb::schema::TypeDefinitionKind::mini_float
                           ? std::optional{analysis_session_.inputs.workspace.types().definition(
                                 analysis_session_.inputs.workspace.types().occurrences().type
                                     [analysis_session_.inputs.workspace.types()
                                          .occurrences()
                                          .index(analysis_session_.inputs.workspace.types()
                                                     .definitions()
                                                     .packed_fields.semantic_type
                                                         [analysis_session_.inputs.workspace.types()
                                                              .definitions()
                                                              .packed_fields.index(
                                                                  *resolved_field)])])}
                           : std::nullopt)
                    : std::nullopt};
            layout::LinearQuantizedAnalysis const* field_quantization_analysis{};
            layout::FixedPointAnalysis const* field_fixed_point_analysis{};
            layout::MiniFloatAnalysis const* field_mini_float_analysis{};
            if (field.has_value() && analysis_session_.results().active_packed.has_value()) {
                auto const analyzed_field{std::ranges::find(
                    analysis_session_.results().active_packed->fields,
                    text(declarations()
                             .packed_fields.name[declarations().packed_fields.index(*field)]),
                    &layout::PackedFieldAnalysis::name)};
                if (analyzed_field != analysis_session_.results().active_packed->fields.end() &&
                    analyzed_field->linear_quantized.has_value()) {
                    field_quantization_analysis = &*analyzed_field->linear_quantized;
                }
                if (analyzed_field != analysis_session_.results().active_packed->fields.end() &&
                    analyzed_field->fixed_point.has_value()) {
                    field_fixed_point_analysis = &*analyzed_field->fixed_point;
                }
                if (analyzed_field != analysis_session_.results().active_packed->fields.end() &&
                    analyzed_field->mini_float.has_value()) {
                    field_mini_float_analysis = &*analyzed_field->mini_float;
                }
            }
            auto const& segment_name{packed_name(segment)};
            auto const row_selected{analysis_session_.inputs.selection.field == segment_name};
            ImGui::PushID(static_cast<int>(index));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (detail::editable_table_row_handle(row_selected)) {
                analysis_session_.inputs.selection.field = segment_name;
                packed_editor_declaration_.reset();
            }
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("PACKED_FIELD_ROW", &index, sizeof(index));
                ImGui::Text("Move %s", segment_name.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget()) {
                if (auto const* payload{ImGui::AcceptDragDropPayload("PACKED_FIELD_ROW")}) {
                    auto const source_index{*static_cast<std::size_t const*>(payload->Data)};
                    if (source_index <
                            declarations()
                                .items(document_->storage().declarations.packed_values.segments
                                           [document_->storage().declarations.packed_values.index(
                                               *schema)])
                                .size() &&
                        source_index != index) {
                        pending = declarations().packed_values.clone(*schema);
                        selected_after_edit = packed_name(declarations().items(
                            declarations()
                                .packed_values.segments[declarations().packed_values.index(
                                    *pending)])[source_index]);
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows{std::vector(original_items.begin(), original_items.end())};
                            move_element(rows, source_index, index);
                            return declarations().range(rows);
                        }();
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::TableNextColumn();
            if (!field.has_value()) {
                ImGui::TextDisabled("-");
            } else {
                auto accessed{
                    analysis_session_.inputs.selection.packed_access_set_explicit
                        ? analysis_session_.inputs.selection.packed_access_fields.contains(text(
                              declarations()
                                  .packed_fields.name[declarations().packed_fields.index(*field)]))
                        : analysis_session_.inputs.selection.field ==
                              text(declarations()
                                       .packed_fields
                                       .name[declarations().packed_fields.index(*field)])};
                if (ImGui::Checkbox("##access", &accessed)) {
                    if (!analysis_session_.inputs.selection.packed_access_set_explicit) {
                        analysis_session_.inputs.selection.packed_access_fields.clear();
                        for (auto const selected_segment : declarations().items(
                                 document_->storage()
                                     .declarations.packed_values
                                     .segments[document_->storage()
                                                   .declarations.packed_values.index(*schema)])) {
                            auto const selected_schema_field{
                                ((selected_segment).kind() ==
                                         lispb::codegen::PackedSegmentKind::field
                                     ? std::optional{selected_segment}
                                     : std::nullopt)};
                            if (selected_schema_field.has_value() &&
                                text(declarations()
                                         .packed_fields.name[declarations().packed_fields.index(
                                             *selected_schema_field)]) ==
                                    analysis_session_.inputs.selection.field) {
                                analysis_session_.inputs.selection.packed_access_fields
                                    .insert_or_assign(analysis_session_.inputs.selection.field,
                                                      analysis_session_.inputs.access_operation);
                                break;
                            }
                        }
                        analysis_session_.inputs.selection.packed_access_set_explicit = true;
                    }
                    if (accessed) {
                        analysis_session_.inputs.selection.packed_access_fields.insert_or_assign(
                            text(declarations()
                                     .packed_fields
                                     .name[declarations().packed_fields.index(*field)]),
                            analysis_session_.inputs.access_operation);
                    } else {
                        analysis_session_.inputs.selection.packed_access_fields.erase(text(
                            declarations()
                                .packed_fields.name[declarations().packed_fields.index(*field)]));
                    }
                }
            }

            ImGui::TableNextColumn();
            auto const accessed{
                field.has_value() &&
                (analysis_session_.inputs.selection.packed_access_set_explicit
                     ? analysis_session_.inputs.selection.packed_access_fields.contains(text(
                           declarations()
                               .packed_fields.name[declarations().packed_fields.index(*field)]))
                     : analysis_session_.inputs.selection.field ==
                           text(
                               declarations().packed_fields.name[declarations().packed_fields.index(
                                   *field)]))};
            if (accessed) {
                auto operation{analysis_session_.inputs.access_operation};
                if (analysis_session_.inputs.selection.packed_access_set_explicit) {
                    if (auto const found{
                            analysis_session_.inputs.selection.packed_access_fields.find(
                                text(declarations()
                                         .packed_fields
                                         .name[declarations().packed_fields.index(*field)]))};
                        found != analysis_session_.inputs.selection.packed_access_fields.end()) {
                        operation = found->second;
                    }
                }
                auto operation_index{static_cast<int>(operation)};
                ImGui::SetNextItemWidth(-1.0F);
                if (ImGui::Combo("##access-operation", &operation_index, "R\0W\0RW\0")) {
                    if (!analysis_session_.inputs.selection.packed_access_set_explicit) {
                        analysis_session_.inputs.selection.packed_access_fields.clear();
                        analysis_session_.inputs.selection.packed_access_set_explicit = true;
                    }
                    analysis_session_.inputs.selection.packed_access_fields.insert_or_assign(
                        text(declarations()
                                 .packed_fields.name[declarations().packed_fields.index(*field)]),
                        static_cast<AccessOperation>(operation_index));
                }
            } else {
                ImGui::TextDisabled("-");
            }

            detail::editable_table_content_hint("RW", ImGui::GetFrameHeight());

            ImGui::TableNextColumn();
            if (row_selected) {
                ImGui::SetNextItemWidth(-1.0F);
                auto const submitted{ImGui::InputText("##name",
                                                      packed_field_name_.data(),
                                                      packed_field_name_.size(),
                                                      ImGuiInputTextFlags_EnterReturnsTrue)};
                if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                    pending = declarations().packed_values.clone(*schema);
                    declarations()
                        .packed_values.segments[declarations().packed_values.index(*pending)] =
                        [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows{std::vector(original_items.begin(), original_items.end())};
                            rows[index] = rename_packed_segment(
                                rows[index], strings().intern(packed_field_name_.data()));
                            return declarations().range(rows);
                        }();
                    selected_after_edit = packed_field_name_.data();
                    if (field.has_value()) {
                        renamed_field =
                            std::pair{text(declarations()
                                               .packed_fields
                                               .name[declarations().packed_fields.index(*field)]),
                                      selected_after_edit};
                    }
                }
            } else {
                ImGui::TextUnformatted(segment_name.c_str());
            }

            ImGui::TableNextColumn();
            if (!field.has_value()) {
                ImGui::TextDisabled("Reserved");
            } else if (row_selected) {
                auto const type_controls_inline{detail::prepare_editable_type_input()};
                auto const submitted{ImGui::InputText("##type",
                                                      packed_field_type_.data(),
                                                      packed_field_type_.size(),
                                                      ImGuiInputTextFlags_EnterReturnsTrue)};
                if (!pending.has_value() && (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                    pending = declarations().packed_values.clone(*schema);
                    declarations()
                        .packed_values.segments[declarations().packed_values.index(*pending)] =
                        [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_2{std::vector(original_items.begin(), original_items.end())};
                            rows_2[index] = [&] {
                                auto row_1{declarations().packed_fields.clone(rows_2[index])};
                                declarations()
                                    .packed_fields.type[declarations().packed_fields.index(row_1)] =
                                    [&] {
                                        auto row_0{declarations().type_refs.clone(
                                            declarations()
                                                .packed_fields
                                                .type[declarations().packed_fields.index(row_1)])};
                                        declarations()
                                            .type_refs.name[declarations().type_refs.index(row_0)] =
                                            strings().intern(packed_field_type_.data());
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
                    pending = declarations().packed_values.clone(*schema);

                    declarations().packed_values.segments[declarations().packed_values.index(
                        *pending)] = [&] {
                        auto const original_items{declarations().items(
                            declarations()
                                .packed_values
                                .segments[declarations().packed_values.index(*pending)])};
                        auto rows_1{std::vector(original_items.begin(), original_items.end())};
                        rows_1[index] = [&] {
                            auto row_0{declarations().packed_fields.clone(rows_1[index])};
                            declarations().packed_fields.type[declarations().packed_fields.index(
                                row_0)] = [&] {
                                auto const new_type_refs{declarations().type_refs.append()};
                                declarations()
                                    .type_refs.name[declarations().type_refs.index(new_type_refs)] =
                                    strings().intern(*picked);
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
                    auto const candidate_types{analysis_session_.inputs.workspace.types().types()};
                    auto const picked_type{
                        std::ranges::find_if(candidate_types, [&](TypeId const candidate) {
                            return (analysis_session_.inputs.workspace.types().origin(candidate) ==
                                        TypeOrigin::declaration &&
                                    ((analysis_session_.inputs.workspace.types().module_name(
                                          candidate) ==
                                          analysis_session_.inputs.workspace.types().module_name(
                                              node) &&
                                      text(analysis_session_.inputs.workspace.types().name(
                                          candidate)) == *picked) ||
                                     text(analysis_session_.inputs.workspace.types().cpp_spelling(
                                         candidate)) == *picked)) ||
                                   (analysis_session_.inputs.workspace.types().origin(candidate) !=
                                        TypeOrigin::declaration &&
                                    integer_source_reference(
                                        analysis_session_.inputs.workspace.types(), candidate) ==
                                        *picked);
                        })};
                    if (picked_type != candidate_types.end() &&
                        ((analysis_session_.inputs.workspace.types().definition(*picked_type))
                             .kind() == lispb::schema::TypeDefinitionKind::enumeration)) {
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields.kind[declarations().packed_fields.index(row_0)] =
                                    lispb::codegen::PackedFieldKind::enumeration;
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .range_helper[declarations().packed_fields.index(row_0)] =
                                    false;
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .minimum_value[declarations().packed_fields.index(row_0)]
                                    .reset();
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .maximum_value[declarations().packed_fields.index(row_0)]
                                    .reset();
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .named_codes[declarations().packed_fields.index(row_0)] = {};
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                    } else if (picked_type != candidate_types.end() &&
                               ((analysis_session_.inputs.workspace.types().definition(
                                     *picked_type))
                                    .kind() ==
                                lispb::schema::TypeDefinitionKind::linear_quantized)) {
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields.kind[declarations().packed_fields.index(row_0)] =
                                    lispb::codegen::PackedFieldKind::linear_quantized;
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields.bits[declarations().packed_fields.index(row_0)]
                                    .reset();
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .range_helper[declarations().packed_fields.index(row_0)] =
                                    false;
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .minimum_value[declarations().packed_fields.index(row_0)]
                                    .reset();
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .maximum_value[declarations().packed_fields.index(row_0)]
                                    .reset();
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .named_codes[declarations().packed_fields.index(row_0)] = {};
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .relationship[declarations().packed_fields.index(row_0)]
                                    .reset();
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                    } else if (picked_type != candidate_types.end() &&
                               ((analysis_session_.inputs.workspace.types().definition(
                                     *picked_type))
                                    .kind() == lispb::schema::TypeDefinitionKind::fixed_point)) {
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields.kind[declarations().packed_fields.index(row_0)] =
                                    lispb::codegen::PackedFieldKind::fixed_point;
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields.bits[declarations().packed_fields.index(row_0)]
                                    .reset();
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .range_helper[declarations().packed_fields.index(row_0)] =
                                    false;
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .minimum_value[declarations().packed_fields.index(row_0)]
                                    .reset();
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .maximum_value[declarations().packed_fields.index(row_0)]
                                    .reset();
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .named_codes[declarations().packed_fields.index(row_0)] = {};
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .relationship[declarations().packed_fields.index(row_0)]
                                    .reset();
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                    } else if (picked_type != candidate_types.end() &&
                               ((analysis_session_.inputs.workspace.types().definition(
                                     *picked_type))
                                    .kind() == lispb::schema::TypeDefinitionKind::mini_float)) {
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields.kind[declarations().packed_fields.index(row_0)] =
                                    lispb::codegen::PackedFieldKind::mini_float;
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields.bits[declarations().packed_fields.index(row_0)]
                                    .reset();
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .range_helper[declarations().packed_fields.index(row_0)] =
                                    false;
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .minimum_value[declarations().packed_fields.index(row_0)]
                                    .reset();
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .maximum_value[declarations().packed_fields.index(row_0)]
                                    .reset();
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .named_codes[declarations().packed_fields.index(row_0)] = {};
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .relationship[declarations().packed_fields.index(row_0)]
                                    .reset();
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                    } else if (picked_type != candidate_types.end()) {
                        if (auto const scalar{packed_integer_domain(
                                analysis_session_.inputs.workspace.types(),
                                TypeId{static_cast<std::uint32_t>(picked_type -
                                                                  candidate_types.begin())},
                                declarations().items(
                                    declarations().packed_values.segments
                                        [declarations().packed_values.index(*pending)])[index])}) {
                            declarations()
                                .packed_values
                                .segments[declarations().packed_values.index(*pending)] = [&] {
                                auto const original_items{declarations().items(
                                    declarations()
                                        .packed_values
                                        .segments[declarations().packed_values.index(*pending)])};
                                auto rows_1{
                                    std::vector(original_items.begin(), original_items.end())};
                                rows_1[index] = [&] {
                                    auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                    declarations()
                                        .packed_fields
                                        .kind[declarations().packed_fields.index(row_0)] =
                                        analysis_session_.inputs.workspace.types()
                                                .definitions()
                                                .integer_scalars
                                                .signedness[analysis_session_.inputs.workspace
                                                                .types()
                                                                .definitions()
                                                                .integer_scalars.index(*scalar)]
                                            ? lispb::codegen::PackedFieldKind::signed_integer
                                            : lispb::codegen::PackedFieldKind::unsigned_integer;
                                    return row_0;
                                }();
                                return declarations().range(rows_1);
                            }();
                            declarations()
                                .packed_values
                                .segments[declarations().packed_values.index(*pending)] = [&] {
                                auto const original_items{declarations().items(
                                    declarations()
                                        .packed_values
                                        .segments[declarations().packed_values.index(*pending)])};
                                auto rows_1{
                                    std::vector(original_items.begin(), original_items.end())};
                                rows_1[index] = [&] {
                                    auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                    declarations()
                                        .packed_fields
                                        .range_helper[declarations().packed_fields.index(row_0)] =
                                        false;
                                    return row_0;
                                }();
                                return declarations().range(rows_1);
                            }();
                            declarations()
                                .packed_values
                                .segments[declarations().packed_values.index(*pending)] = [&] {
                                auto const original_items{declarations().items(
                                    declarations()
                                        .packed_values
                                        .segments[declarations().packed_values.index(*pending)])};
                                auto rows_1{
                                    std::vector(original_items.begin(), original_items.end())};
                                rows_1[index] = [&] {
                                    auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                    declarations()
                                        .packed_fields
                                        .minimum_value[declarations().packed_fields.index(row_0)]
                                        .reset();
                                    return row_0;
                                }();
                                return declarations().range(rows_1);
                            }();
                            declarations()
                                .packed_values
                                .segments[declarations().packed_values.index(*pending)] = [&] {
                                auto const original_items{declarations().items(
                                    declarations()
                                        .packed_values
                                        .segments[declarations().packed_values.index(*pending)])};
                                auto rows_1{
                                    std::vector(original_items.begin(), original_items.end())};
                                rows_1[index] = [&] {
                                    auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                    declarations()
                                        .packed_fields
                                        .maximum_value[declarations().packed_fields.index(row_0)]
                                        .reset();
                                    return row_0;
                                }();
                                return declarations().range(rows_1);
                            }();
                            declarations()
                                .packed_values
                                .segments[declarations().packed_values.index(*pending)] = [&] {
                                auto const original_items{declarations().items(
                                    declarations()
                                        .packed_values
                                        .segments[declarations().packed_values.index(*pending)])};
                                auto rows_1{
                                    std::vector(original_items.begin(), original_items.end())};
                                rows_1[index] = [&] {
                                    auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                    declarations().packed_fields.named_codes
                                        [declarations().packed_fields.index(row_0)] = {};
                                    return row_0;
                                }();
                                return declarations().range(rows_1);
                            }();
                        }
                    }
                }
                ImGui::SameLine();
                if (index < definitions().packed.segments_of(packed).size() &&
                    detail::semantic_type_navigation_button()) {
                    if (auto const resolved{
                            ((definitions().packed.segments_of(packed)[index]).kind() ==
                                     lispb::schema::PackedSegmentKind::field
                                 ? std::optional{definitions().packed.segments_of(packed)[index]}
                                 : std::nullopt)}) {
                        navigate_to =
                            analysis_session_.inputs.workspace.types().occurrences().type
                                [analysis_session_.inputs.workspace.types().occurrences().index(
                                    analysis_session_.inputs.workspace.types()
                                        .definitions()
                                        .packed_fields
                                        .semantic_type[analysis_session_.inputs.workspace.types()
                                                           .definitions()
                                                           .packed_fields.index(*resolved)])];
                    }
                }
                detail::editable_table_content_hint(packed_field_type_.data(), 80.0F);
            } else {
                ImGui::TextUnformatted(
                    text(declarations().type_refs.name[declarations().type_refs.index(
                             declarations()
                                 .packed_fields.type[declarations().packed_fields.index(*field)])])
                        .c_str());
            }

            ImGui::TableNextColumn();
            if (field.has_value() &&
                !declarations()
                     .packed_fields.bits[declarations().packed_fields.index(*field)]
                     .has_value()) {
                auto const effective{
                    index < definitions().packed.segments_of(packed).size()
                        ? analysis_session_.inputs.workspace.types()
                              .definitions()
                              .packed_fields
                              .bit_width[analysis_session_.inputs.workspace.types()
                                             .definitions()
                                             .packed_fields.index(
                                                 definitions().packed.segments_of(packed)[index])]
                        : 0U};
                ImGui::Text("=> %u", effective);
            } else if (row_selected) {
                ImGui::SetNextItemWidth(72.0F);
                auto const submitted{ImGui::InputInt(
                    "##bits", &packed_field_bits_, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue)};
                if (!pending.has_value() && (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                    pending = declarations().packed_values.clone(*schema);
                    declarations()
                        .packed_values.segments[declarations().packed_values.index(*pending)] =
                        [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows{std::vector(original_items.begin(), original_items.end())};
                            rows[index] = resize_packed_segment(rows[index], packed_field_bits_);
                            return declarations().range(rows);
                        }();
                }
            } else {
                ImGui::Text("%d", *packed_bits(segment));
            }

            ImGui::TableNextColumn();
            if (!field.has_value()) {
                ImGui::TextDisabled("-");
            } else if (row_selected) {
                auto width_auto{!declarations()
                                     .packed_fields.bits[declarations().packed_fields.index(*field)]
                                     .has_value()};
                if (ImGui::Checkbox("##auto-bits", &width_auto)) {
                    pending = declarations().packed_values.clone(*schema);

                    declarations().packed_values.segments[declarations().packed_values.index(
                        *pending)] = [&] {
                        auto const original_items{declarations().items(
                            declarations()
                                .packed_values
                                .segments[declarations().packed_values.index(*pending)])};
                        auto rows_1{std::vector(original_items.begin(), original_items.end())};
                        rows_1[index] = [&] {
                            auto row_0{declarations().packed_fields.clone(rows_1[index])};
                            declarations()
                                .packed_fields.bits[declarations().packed_fields.index(row_0)] =
                                width_auto ? std::nullopt : std::optional<int>{packed_field_bits_};
                            return row_0;
                        }();
                        return declarations().range(rows_1);
                    }();
                }
            } else {
                ImGui::TextUnformatted(
                    declarations()
                            .packed_fields.bits[declarations().packed_fields.index(*field)]
                            .has_value()
                        ? "-"
                        : "yes");
            }

            ImGui::TableNextColumn();
            auto const kind_label{
                !field.has_value() ? "reserved"
                : declarations().packed_fields.kind[declarations().packed_fields.index(*field)] ==
                        lispb::codegen::PackedFieldKind::enumeration
                    ? "enum"
                : declarations().packed_fields.kind[declarations().packed_fields.index(*field)] ==
                        lispb::codegen::PackedFieldKind::linear_quantized
                    ? "linear quantized"
                : declarations().packed_fields.kind[declarations().packed_fields.index(*field)] ==
                        lispb::codegen::PackedFieldKind::fixed_point
                    ? "fixed point"
                : declarations().packed_fields.kind[declarations().packed_fields.index(*field)] ==
                        lispb::codegen::PackedFieldKind::mini_float
                    ? "mini float"
                : declarations().packed_fields.kind[declarations().packed_fields.index(*field)] ==
                        lispb::codegen::PackedFieldKind::signed_integer
                    ? "signed"
                    : "unsigned"};
            if (!field.has_value()) {
                ImGui::TextDisabled("reserved");
            } else if (row_selected) {
                ImGui::BeginDisabled(
                    field_uses_integer_scalar || static_cast<bool>(field_linear_quantized) ||
                    static_cast<bool>(field_fixed_point) || static_cast<bool>(field_mini_float));
                ImGui::SetNextItemWidth(-1.0F);
                if (ImGui::BeginCombo("##kind", kind_label)) {
                    if (ImGui::Selectable(
                            "unsigned",
                            declarations().packed_fields.kind[declarations().packed_fields.index(
                                *field)] == lispb::codegen::PackedFieldKind::unsigned_integer)) {
                        pending = declarations().packed_values.clone(*schema);

                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields.kind[declarations().packed_fields.index(row_0)] =
                                    lispb::codegen::PackedFieldKind::unsigned_integer;
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        if (auto const mapped{matching_unsigned_type(
                                text(declarations().type_refs.name[declarations().type_refs.index(
                                    declarations().packed_fields.type
                                        [declarations().packed_fields.index(declarations().items(
                                            declarations().packed_values.segments
                                                [declarations().packed_values.index(
                                                    *pending)])[index])])]))}) {
                            declarations()
                                .packed_values
                                .segments[declarations().packed_values.index(*pending)] = [&] {
                                auto const original_items{declarations().items(
                                    declarations()
                                        .packed_values
                                        .segments[declarations().packed_values.index(*pending)])};
                                auto rows_2{
                                    std::vector(original_items.begin(), original_items.end())};
                                rows_2[index] = [&] {
                                    auto row_1{declarations().packed_fields.clone(rows_2[index])};
                                    declarations()
                                        .packed_fields
                                        .type[declarations().packed_fields.index(row_1)] = [&] {
                                        auto row_0{declarations().type_refs.clone(
                                            declarations()
                                                .packed_fields
                                                .type[declarations().packed_fields.index(row_1)])};
                                        declarations()
                                            .type_refs.name[declarations().type_refs.index(row_0)] =
                                            strings().intern(*mapped);
                                        return row_0;
                                    }();
                                    return row_1;
                                }();
                                return declarations().range(rows_2);
                            }();
                        }
                        auto const has_negative_code{std::ranges::any_of(
                            declarations().items(
                                declarations()
                                    .packed_fields.named_codes[declarations().packed_fields.index(
                                        declarations().items(
                                            declarations().packed_values.segments
                                                [declarations().packed_values.index(
                                                    *pending)])[index])]),
                            [&](auto id) {
                                return declarations()
                                    .packed_named_codes
                                    .value[declarations().packed_named_codes.index(id)]
                                    .negative;
                            })};
                        if ((declarations()
                                 .packed_fields
                                 .minimum_value[declarations().packed_fields.index(
                                     declarations().items(declarations().packed_values.segments
                                                              [declarations().packed_values.index(
                                                                  *pending)])[index])]
                                 .has_value() &&
                             (*declarations().packed_fields.minimum_value
                                   [declarations().packed_fields.index(declarations().items(
                                       declarations().packed_values.segments
                                           [declarations().packed_values.index(*pending)])[index])])
                                 .negative) ||
                            has_negative_code) {
                            declarations()
                                .packed_values
                                .segments[declarations().packed_values.index(*pending)] = [&] {
                                auto const original_items{declarations().items(
                                    declarations()
                                        .packed_values
                                        .segments[declarations().packed_values.index(*pending)])};
                                auto rows_1{
                                    std::vector(original_items.begin(), original_items.end())};
                                rows_1[index] = [&] {
                                    auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                    declarations()
                                        .packed_fields
                                        .minimum_value[declarations().packed_fields.index(row_0)]
                                        .reset();
                                    return row_0;
                                }();
                                return declarations().range(rows_1);
                            }();
                            declarations()
                                .packed_values
                                .segments[declarations().packed_values.index(*pending)] = [&] {
                                auto const original_items{declarations().items(
                                    declarations()
                                        .packed_values
                                        .segments[declarations().packed_values.index(*pending)])};
                                auto rows_1{
                                    std::vector(original_items.begin(), original_items.end())};
                                rows_1[index] = [&] {
                                    auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                    declarations()
                                        .packed_fields
                                        .maximum_value[declarations().packed_fields.index(row_0)]
                                        .reset();
                                    return row_0;
                                }();
                                return declarations().range(rows_1);
                            }();
                            declarations()
                                .packed_values
                                .segments[declarations().packed_values.index(*pending)] = [&] {
                                auto const original_items{declarations().items(
                                    declarations()
                                        .packed_values
                                        .segments[declarations().packed_values.index(*pending)])};
                                auto rows_1{
                                    std::vector(original_items.begin(), original_items.end())};
                                rows_1[index] = [&] {
                                    auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                    declarations().packed_fields.named_codes
                                        [declarations().packed_fields.index(row_0)] = {};
                                    return row_0;
                                }();
                                return declarations().range(rows_1);
                            }();
                            if (!declarations()
                                     .packed_fields
                                     .bits[declarations().packed_fields.index(declarations().items(
                                         declarations().packed_values.segments
                                             [declarations().packed_values.index(
                                                 *pending)])[index])]
                                     .has_value() &&
                                index < definitions().packed.segments_of(packed).size()) {
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)] = [&] {
                                    auto const original_items{declarations().items(
                                        declarations().packed_values.segments
                                            [declarations().packed_values.index(*pending)])};
                                    auto rows_1{
                                        std::vector(original_items.begin(), original_items.end())};
                                    rows_1[index] = [&] {
                                        auto row_0{
                                            declarations().packed_fields.clone(rows_1[index])};
                                        declarations()
                                            .packed_fields
                                            .bits[declarations().packed_fields.index(row_0)] =
                                            static_cast<int>(
                                                analysis_session_.inputs.workspace.types()
                                                    .definitions()
                                                    .packed_fields.bit_width
                                                        [analysis_session_.inputs.workspace.types()
                                                             .definitions()
                                                             .packed_fields.index(
                                                                 definitions().packed.segments_of(
                                                                     packed)[index])]);
                                        return row_0;
                                    }();
                                    return declarations().range(rows_1);
                                }();
                            }
                        }
                    }
                    if (ImGui::Selectable(
                            "signed",
                            declarations().packed_fields.kind[declarations().packed_fields.index(
                                *field)] == lispb::codegen::PackedFieldKind::signed_integer)) {
                        pending = declarations().packed_values.clone(*schema);

                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields.kind[declarations().packed_fields.index(row_0)] =
                                    lispb::codegen::PackedFieldKind::signed_integer;
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        if (auto const mapped{matching_signed_type(
                                text(declarations().type_refs.name[declarations().type_refs.index(
                                    declarations().packed_fields.type
                                        [declarations().packed_fields.index(declarations().items(
                                            declarations().packed_values.segments
                                                [declarations().packed_values.index(
                                                    *pending)])[index])])]))}) {
                            declarations()
                                .packed_values
                                .segments[declarations().packed_values.index(*pending)] = [&] {
                                auto const original_items{declarations().items(
                                    declarations()
                                        .packed_values
                                        .segments[declarations().packed_values.index(*pending)])};
                                auto rows_2{
                                    std::vector(original_items.begin(), original_items.end())};
                                rows_2[index] = [&] {
                                    auto row_1{declarations().packed_fields.clone(rows_2[index])};
                                    declarations()
                                        .packed_fields
                                        .type[declarations().packed_fields.index(row_1)] = [&] {
                                        auto row_0{declarations().type_refs.clone(
                                            declarations()
                                                .packed_fields
                                                .type[declarations().packed_fields.index(row_1)])};
                                        declarations()
                                            .type_refs.name[declarations().type_refs.index(row_0)] =
                                            strings().intern(*mapped);
                                        return row_0;
                                    }();
                                    return row_1;
                                }();
                                return declarations().range(rows_2);
                            }();
                        }
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .range_helper[declarations().packed_fields.index(row_0)] =
                                    false;
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        auto const transition_width{
                            declarations()
                                    .packed_fields
                                    .bits[declarations().packed_fields.index(declarations().items(
                                        declarations().packed_values.segments
                                            [declarations().packed_values.index(*pending)])[index])]
                                    .has_value()
                                ? static_cast<std::uint32_t>(
                                      *declarations()
                                           .packed_fields.bits[declarations().packed_fields.index(
                                               declarations().items(
                                                   declarations().packed_values.segments
                                                       [declarations().packed_values.index(
                                                           *pending)])[index])])
                            : index < definitions().packed.segments_of(packed).size()
                                ? analysis_session_.inputs.workspace.types()
                                      .definitions()
                                      .packed_fields
                                      .bit_width[analysis_session_.inputs.workspace.types()
                                                     .definitions()
                                                     .packed_fields.index(
                                                         definitions().packed.segments_of(
                                                             packed)[index])]
                                : std::uint32_t{1}};
                        if (declarations()
                                .packed_fields
                                .minimum_value[declarations().packed_fields.index(
                                    declarations().items(
                                        declarations().packed_values.segments
                                            [declarations().packed_values.index(*pending)])[index])]
                                .has_value() &&
                            (!lispb::codegen::packed_integer_fits_signed(
                                 *declarations().packed_fields.minimum_value
                                      [declarations().packed_fields.index(declarations().items(
                                          declarations().packed_values.segments
                                              [declarations().packed_values.index(
                                                  *pending)])[index])],
                                 transition_width) ||
                             !lispb::codegen::packed_integer_fits_signed(
                                 *declarations().packed_fields.maximum_value
                                      [declarations().packed_fields.index(declarations().items(
                                          declarations().packed_values.segments
                                              [declarations().packed_values.index(
                                                  *pending)])[index])],
                                 transition_width))) {
                            declarations()
                                .packed_values
                                .segments[declarations().packed_values.index(*pending)] = [&] {
                                auto const original_items{declarations().items(
                                    declarations()
                                        .packed_values
                                        .segments[declarations().packed_values.index(*pending)])};
                                auto rows_1{
                                    std::vector(original_items.begin(), original_items.end())};
                                rows_1[index] = [&] {
                                    auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                    declarations()
                                        .packed_fields
                                        .minimum_value[declarations().packed_fields.index(row_0)]
                                        .reset();
                                    return row_0;
                                }();
                                return declarations().range(rows_1);
                            }();
                            declarations()
                                .packed_values
                                .segments[declarations().packed_values.index(*pending)] = [&] {
                                auto const original_items{declarations().items(
                                    declarations()
                                        .packed_values
                                        .segments[declarations().packed_values.index(*pending)])};
                                auto rows_1{
                                    std::vector(original_items.begin(), original_items.end())};
                                rows_1[index] = [&] {
                                    auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                    declarations()
                                        .packed_fields
                                        .maximum_value[declarations().packed_fields.index(row_0)]
                                        .reset();
                                    return row_0;
                                }();
                                return declarations().range(rows_1);
                            }();
                            declarations()
                                .packed_values
                                .segments[declarations().packed_values.index(*pending)] = [&] {
                                auto const original_items{declarations().items(
                                    declarations()
                                        .packed_values
                                        .segments[declarations().packed_values.index(*pending)])};
                                auto rows_1{
                                    std::vector(original_items.begin(), original_items.end())};
                                rows_1[index] = [&] {
                                    auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                    declarations().packed_fields.named_codes
                                        [declarations().packed_fields.index(row_0)] = {};
                                    return row_0;
                                }();
                                return declarations().range(rows_1);
                            }();
                            if (!declarations()
                                     .packed_fields
                                     .bits[declarations().packed_fields.index(declarations().items(
                                         declarations().packed_values.segments
                                             [declarations().packed_values.index(
                                                 *pending)])[index])]
                                     .has_value()) {
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)] = [&] {
                                    auto const original_items{declarations().items(
                                        declarations().packed_values.segments
                                            [declarations().packed_values.index(*pending)])};
                                    auto rows_1{
                                        std::vector(original_items.begin(), original_items.end())};
                                    rows_1[index] = [&] {
                                        auto row_0{
                                            declarations().packed_fields.clone(rows_1[index])};
                                        declarations()
                                            .packed_fields
                                            .bits[declarations().packed_fields.index(row_0)] =
                                            static_cast<int>(transition_width);
                                        return row_0;
                                    }();
                                    return declarations().range(rows_1);
                                }();
                            }
                        } else {
                            declarations()
                                .packed_values
                                .segments[declarations().packed_values.index(*pending)] = [&] {
                                auto const original_items{declarations().items(
                                    declarations()
                                        .packed_values
                                        .segments[declarations().packed_values.index(*pending)])};
                                auto segments{
                                    std::vector(original_items.begin(), original_items.end())};
                                segments[index] = [&] {
                                    auto field{declarations().packed_fields.clone(segments[index])};
                                    declarations().packed_fields.named_codes
                                        [declarations().packed_fields.index(field)] = [&] {
                                        auto const original_items{declarations().items(
                                            declarations().packed_fields.named_codes
                                                [declarations().packed_fields.index(field)])};
                                        auto codes{std::vector(original_items.begin(),
                                                               original_items.end())};
                                        std::erase_if(codes, [&](auto code_id) {
                                            auto const code{code_id};
                                            return !lispb::codegen::packed_integer_fits_signed(
                                                declarations().packed_named_codes.value
                                                    [declarations().packed_named_codes.index(code)],
                                                transition_width);
                                        });
                                        return declarations().range(codes);
                                    }();
                                    return field;
                                }();
                                return declarations().range(segments);
                            }();
                        }
                    }
                    if (ImGui::Selectable(
                            "enum",
                            declarations().packed_fields.kind[declarations().packed_fields.index(
                                *field)] == lispb::codegen::PackedFieldKind::enumeration)) {
                        pending = declarations().packed_values.clone(*schema);

                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields.kind[declarations().packed_fields.index(row_0)] =
                                    lispb::codegen::PackedFieldKind::enumeration;
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .range_helper[declarations().packed_fields.index(row_0)] =
                                    false;
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .minimum_value[declarations().packed_fields.index(row_0)]
                                    .reset();
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .maximum_value[declarations().packed_fields.index(row_0)]
                                    .reset();
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .named_codes[declarations().packed_fields.index(row_0)] = {};
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                    }
                    if (ImGui::Selectable(
                            "linear quantized",
                            declarations().packed_fields.kind[declarations().packed_fields.index(
                                *field)] == lispb::codegen::PackedFieldKind::linear_quantized)) {
                        pending = declarations().packed_values.clone(*schema);

                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields.kind[declarations().packed_fields.index(row_0)] =
                                    lispb::codegen::PackedFieldKind::linear_quantized;
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields.bits[declarations().packed_fields.index(row_0)]
                                    .reset();
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .range_helper[declarations().packed_fields.index(row_0)] =
                                    false;
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .minimum_value[declarations().packed_fields.index(row_0)]
                                    .reset();
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .maximum_value[declarations().packed_fields.index(row_0)]
                                    .reset();
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .named_codes[declarations().packed_fields.index(row_0)] = {};
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .relationship[declarations().packed_fields.index(row_0)]
                                    .reset();
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                    }
                    if (ImGui::Selectable(
                            "fixed point",
                            declarations().packed_fields.kind[declarations().packed_fields.index(
                                *field)] == lispb::codegen::PackedFieldKind::fixed_point)) {
                        pending = declarations().packed_values.clone(*schema);

                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields.kind[declarations().packed_fields.index(row_0)] =
                                    lispb::codegen::PackedFieldKind::fixed_point;
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields.bits[declarations().packed_fields.index(row_0)]
                                    .reset();
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .range_helper[declarations().packed_fields.index(row_0)] =
                                    false;
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .minimum_value[declarations().packed_fields.index(row_0)]
                                    .reset();
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .maximum_value[declarations().packed_fields.index(row_0)]
                                    .reset();
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .named_codes[declarations().packed_fields.index(row_0)] = {};
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .relationship[declarations().packed_fields.index(row_0)]
                                    .reset();
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                    }
                    if (ImGui::Selectable(
                            "mini float",
                            declarations().packed_fields.kind[declarations().packed_fields.index(
                                *field)] == lispb::codegen::PackedFieldKind::mini_float)) {
                        pending = declarations().packed_values.clone(*schema);

                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields.kind[declarations().packed_fields.index(row_0)] =
                                    lispb::codegen::PackedFieldKind::mini_float;
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields.bits[declarations().packed_fields.index(row_0)]
                                    .reset();
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .range_helper[declarations().packed_fields.index(row_0)] =
                                    false;
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .minimum_value[declarations().packed_fields.index(row_0)]
                                    .reset();
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .maximum_value[declarations().packed_fields.index(row_0)]
                                    .reset();
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .named_codes[declarations().packed_fields.index(row_0)] = {};
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .relationship[declarations().packed_fields.index(row_0)]
                                    .reset();
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                    }
                    ImGui::EndCombo();
                }
                ImGui::EndDisabled();
            } else if (!row_selected) {
                ImGui::TextUnformatted(kind_label);
            }

            detail::editable_table_content_hint(kind_label, ImGui::GetFrameHeight());

            ImGui::TableNextColumn();
            auto range_helper{
                field.has_value() &&
                declarations()
                    .packed_fields.range_helper[declarations().packed_fields.index(*field)]};
            if (!field.has_value()) {
                ImGui::TextDisabled("-");
            } else if (row_selected) {
                ImGui::BeginDisabled(
                    declarations().packed_fields.kind[declarations().packed_fields.index(*field)] !=
                        lispb::codegen::PackedFieldKind::unsigned_integer ||
                    field_uses_integer_scalar || static_cast<bool>(field_linear_quantized) ||
                    static_cast<bool>(field_fixed_point) || static_cast<bool>(field_mini_float));
                if (ImGui::Checkbox("##range-helper", &range_helper)) {
                    pending = declarations().packed_values.clone(*schema);
                    declarations()
                        .packed_values.segments[declarations().packed_values.index(*pending)] =
                        [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .range_helper[declarations().packed_fields.index(row_0)] =
                                    range_helper;
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                }
                ImGui::EndDisabled();
            } else {
                ImGui::TextUnformatted(range_helper ? "yes" : "-");
            }

            ImGui::TableNextColumn();
            if (!field.has_value()) {
                ImGui::TextDisabled("-");
            } else if (field_uses_integer_scalar &&
                       analysis_session_.inputs.workspace.types()
                           .definitions()
                           .packed_fields
                           .minimum_value[analysis_session_.inputs.workspace.types()
                                              .definitions()
                                              .packed_fields.index(*resolved_field)]
                           .has_value()) {
                auto const minimum{lispb::codegen::format_packed_integer(
                    *analysis_session_.inputs.workspace.types()
                         .definitions()
                         .packed_fields.minimum_value[analysis_session_.inputs.workspace.types()
                                                          .definitions()
                                                          .packed_fields.index(*resolved_field)])};
                auto const maximum{lispb::codegen::format_packed_integer(
                    *analysis_session_.inputs.workspace.types()
                         .definitions()
                         .packed_fields.maximum_value[analysis_session_.inputs.workspace.types()
                                                          .definitions()
                                                          .packed_fields.index(*resolved_field)])};
                ImGui::TextDisabled("%s..%s (shared)", minimum.c_str(), maximum.c_str());
            } else if (field_quantization_analysis != nullptr) {
                auto const minimum{lispb::codegen::format_packed_integer(
                    field_quantization_analysis->source_minimum)};
                auto const maximum{lispb::codegen::format_packed_integer(
                    field_quantization_analysis->source_maximum)};
                ImGui::TextDisabled("%s..%s (quantized)", minimum.c_str(), maximum.c_str());
            } else if (field_fixed_point_analysis != nullptr) {
                ImGui::TextDisabled("%.9Lg..%.9Lg (fixed)",
                                    field_fixed_point_analysis->minimum_value,
                                    field_fixed_point_analysis->maximum_value);
            } else if (field_mini_float_analysis != nullptr) {
                if (field_mini_float_analysis->minimum_finite.has_value() &&
                    field_mini_float_analysis->maximum_finite.has_value()) {
                    ImGui::TextDisabled("%.9Lg..%.9Lg (mini float)",
                                        *field_mini_float_analysis->minimum_finite,
                                        *field_mini_float_analysis->maximum_finite);
                } else {
                    ImGui::TextDisabled("Unknown (mini float)");
                }
            } else if (row_selected) {
                ImGui::SetNextItemWidth(70.0F);
                auto range_submitted{ImGui::InputText("##minimum",
                                                      packed_field_minimum_.data(),
                                                      packed_field_minimum_.size(),
                                                      ImGuiInputTextFlags_EnterReturnsTrue)};
                ImGui::SameLine();
                ImGui::TextUnformatted("..");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(70.0F);
                range_submitted = ImGui::InputText("##maximum",
                                                   packed_field_maximum_.data(),
                                                   packed_field_maximum_.size(),
                                                   ImGuiInputTextFlags_EnterReturnsTrue) ||
                                  range_submitted;
                range_submitted = range_submitted || ImGui::IsItemDeactivatedAfterEdit();
                if (!pending.has_value() && range_submitted) {
                    auto const minimum_text{std::string_view{packed_field_minimum_.data()}};
                    auto const maximum_text{std::string_view{packed_field_maximum_.data()}};
                    pending = declarations().packed_values.clone(*schema);

                    if (minimum_text.empty() && maximum_text.empty()) {
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .minimum_value[declarations().packed_fields.index(row_0)]
                                    .reset();
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                        declarations().packed_values.segments[declarations().packed_values.index(
                            *pending)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)])};
                            auto rows_1{std::vector(original_items.begin(), original_items.end())};
                            rows_1[index] = [&] {
                                auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                declarations()
                                    .packed_fields
                                    .maximum_value[declarations().packed_fields.index(row_0)]
                                    .reset();
                                return row_0;
                            }();
                            return declarations().range(rows_1);
                        }();
                    } else if (auto const minimum{detail::parse_packed_integer(minimum_text)};
                               minimum.has_value()) {
                        if (auto const maximum{detail::parse_packed_integer(maximum_text)};
                            maximum.has_value()) {
                            declarations()
                                .packed_values
                                .segments[declarations().packed_values.index(*pending)] = [&] {
                                auto const original_items{declarations().items(
                                    declarations()
                                        .packed_values
                                        .segments[declarations().packed_values.index(*pending)])};
                                auto rows_1{
                                    std::vector(original_items.begin(), original_items.end())};
                                rows_1[index] = [&] {
                                    auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                    declarations()
                                        .packed_fields
                                        .minimum_value[declarations().packed_fields.index(row_0)] =
                                        *minimum;
                                    return row_0;
                                }();
                                return declarations().range(rows_1);
                            }();
                            declarations()
                                .packed_values
                                .segments[declarations().packed_values.index(*pending)] = [&] {
                                auto const original_items{declarations().items(
                                    declarations()
                                        .packed_values
                                        .segments[declarations().packed_values.index(*pending)])};
                                auto rows_1{
                                    std::vector(original_items.begin(), original_items.end())};
                                rows_1[index] = [&] {
                                    auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                    declarations()
                                        .packed_fields
                                        .maximum_value[declarations().packed_fields.index(row_0)] =
                                        *maximum;
                                    return row_0;
                                }();
                                return declarations().range(rows_1);
                            }();
                        } else {
                            schema_edit_message_ =
                                "Semantic range requires valid integer minimum and maximum.";
                            pending.reset();
                        }
                    } else {
                        schema_edit_message_ =
                            "Semantic range requires valid integer minimum and maximum.";
                        pending.reset();
                    }
                }
            } else if (declarations()
                           .packed_fields.minimum_value[declarations().packed_fields.index(*field)]
                           .has_value()) {
                auto const minimum{lispb::codegen::format_packed_integer(
                    *declarations()
                         .packed_fields.minimum_value[declarations().packed_fields.index(*field)])};
                auto const maximum{lispb::codegen::format_packed_integer(
                    *declarations()
                         .packed_fields.maximum_value[declarations().packed_fields.index(*field)])};
                ImGui::Text("%s..%s", minimum.c_str(), maximum.c_str());
            } else {
                ImGui::TextDisabled("unconstrained");
            }

            ImGui::TableNextColumn();
            if (!field.has_value()) {
                ImGui::TextDisabled("Reserved");
            } else if (field_quantization_analysis != nullptr) {
                auto const maximum{field_quantization_analysis->usable_code_count.two_to_64
                                       ? (std::numeric_limits<std::uint64_t>::max)()
                                       : field_quantization_analysis->usable_code_count.value - 1};
                ImGui::Text("encoded 0..%llu", static_cast<unsigned long long>(maximum));
            } else if (field_fixed_point_analysis != nullptr) {
                auto const minimum{lispb::codegen::format_packed_integer(
                    field_fixed_point_analysis->minimum_raw_value)};
                auto const maximum{lispb::codegen::format_packed_integer(
                    field_fixed_point_analysis->maximum_raw_value)};
                ImGui::Text("raw %s..%s", minimum.c_str(), maximum.c_str());
            } else if (field_mini_float_analysis != nullptr) {
                auto const maximum{
                    field_mini_float_analysis->total_bits == 64
                        ? (std::numeric_limits<std::uint64_t>::max)()
                        : (std::uint64_t{1} << field_mini_float_analysis->total_bits) - 1};
                ImGui::Text("encoded 0..%llu", static_cast<unsigned long long>(maximum));
            } else if (index < definitions().packed.segments_of(packed).size()) {
                auto const resolved{
                    ((definitions().packed.segments_of(packed)[index]).kind() ==
                             lispb::schema::PackedSegmentKind::field
                         ? std::optional{definitions().packed.segments_of(packed)[index]}
                         : std::nullopt)};
                if (resolved.has_value() &&
                    analysis_session_.inputs.workspace.types()
                            .definitions()
                            .packed_fields.kind[analysis_session_.inputs.workspace.types()
                                                    .definitions()
                                                    .packed_fields.index(*resolved)] ==
                        lispb::codegen::PackedFieldKind::signed_integer) {
                    auto const minimum{
                        analysis_session_.inputs.workspace.types()
                                    .definitions()
                                    .packed_fields
                                    .bit_width[analysis_session_.inputs.workspace.types()
                                                   .definitions()
                                                   .packed_fields.index(*resolved)] == 64
                            ? std::numeric_limits<std::int64_t>::min()
                            : -static_cast<std::int64_t>(
                                  std::uint64_t{1}
                                  << (analysis_session_.inputs.workspace.types()
                                          .definitions()
                                          .packed_fields
                                          .bit_width[analysis_session_.inputs.workspace.types()
                                                         .definitions()
                                                         .packed_fields.index(*resolved)] -
                                      1))};
                    auto const maximum{
                        analysis_session_.inputs.workspace.types()
                                    .definitions()
                                    .packed_fields
                                    .bit_width[analysis_session_.inputs.workspace.types()
                                                   .definitions()
                                                   .packed_fields.index(*resolved)] == 64
                            ? std::numeric_limits<std::int64_t>::max()
                            : static_cast<std::int64_t>(
                                  (std::uint64_t{1}
                                   << (analysis_session_.inputs.workspace.types()
                                           .definitions()
                                           .packed_fields
                                           .bit_width[analysis_session_.inputs.workspace.types()
                                                          .definitions()
                                                          .packed_fields.index(*resolved)] -
                                       1)) -
                                  1)};
                    ImGui::Text("%lld..%lld",
                                static_cast<long long>(minimum),
                                static_cast<long long>(maximum));
                } else {
                    auto const maximum{
                        !resolved.has_value() ? std::optional<std::uint64_t>{}
                        : analysis_session_.inputs.workspace.types()
                                    .definitions()
                                    .packed_fields
                                    .bit_width[analysis_session_.inputs.workspace.types()
                                                   .definitions()
                                                   .packed_fields.index(*resolved)] >= 64
                            ? std::optional<
                                  std::uint64_t>{std::numeric_limits<std::uint64_t>::max()}
                            : std::optional<std::uint64_t>{
                                  (std::uint64_t{1}
                                   << analysis_session_.inputs.workspace.types()
                                          .definitions()
                                          .packed_fields
                                          .bit_width[analysis_session_.inputs.workspace.types()
                                                         .definitions()
                                                         .packed_fields.index(*resolved)]) -
                                  1}};
                    ImGui::Text("0..%s", detail::format_number(maximum).c_str());
                }
            } else {
                ImGui::TextDisabled("Unknown");
            }
            ImGui::TableNextColumn();
            if (!field.has_value()) {
                ImGui::TextDisabled("0 (reserved)");
            } else {
                auto const inactive{explicit_defaults && default_gap};
                if (inactive) {
                    ImGui::PushStyleColor(ImGuiCol_Text,
                                          ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                }
                auto const value_text{
                    declarations()
                        .packed_fields.default_value[declarations().packed_fields.index(*field)]
                        .transform(lispb::codegen::format_packed_integer)
                        .value_or("")};
                auto const* hint{
                    explicit_defaults ? "required"
                    : document_->storage()
                            .declarations.packed_values
                            .invalid_value[document_->storage().declarations.packed_values.index(
                                *schema)]
                            .has_value()
                        ? "sentinel"
                        : "0 (implicit)"};
                if (row_selected) {
                    ImGui::SetNextItemWidth(-1.0F);
                    auto const submitted{
                        ImGui::InputTextWithHint("##default",
                                                 hint,
                                                 packed_field_default_.data(),
                                                 packed_field_default_.size(),
                                                 ImGuiInputTextFlags_EnterReturnsTrue)};
                    if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                        auto const default_value{
                            detail::parse_packed_integer(packed_field_default_.data())};
                        if (packed_field_default_.front() == '\0' || default_value.has_value()) {
                            if (!pending.has_value()) {
                                pending = declarations().packed_values.clone(*schema);
                            }
                            declarations()
                                .packed_values
                                .segments[declarations().packed_values.index(*pending)] = [&] {
                                auto const original_items{declarations().items(
                                    declarations()
                                        .packed_values
                                        .segments[declarations().packed_values.index(*pending)])};
                                auto rows_1{
                                    std::vector(original_items.begin(), original_items.end())};
                                rows_1[index] = [&] {
                                    auto row_0{declarations().packed_fields.clone(rows_1[index])};
                                    declarations()
                                        .packed_fields
                                        .default_value[declarations().packed_fields.index(row_0)] =
                                        default_value;
                                    return row_0;
                                }();
                                return declarations().range(rows_1);
                            }();
                        } else {
                            schema_edit_message_ = "Default must be an integer or blank.";
                        }
                    }
                } else {
                    ImGui::TextUnformatted(value_text.empty() ? hint : value_text.c_str());
                }
                if (inactive) {
                    ImGui::PopStyleColor();
                    ImGui::SetItemTooltip("Retained but inactive: fill every field default to "
                                          "enable default construction.");
                }
                detail::editable_table_content_hint(value_text.empty() ? hint : value_text);
                default_gap |=
                    !declarations()
                         .packed_fields.default_value[declarations().packed_fields.index(*field)]
                         .has_value();
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (!pending.has_value() && selected_schema_field.has_value() && selected_index.has_value()) {
        auto const selected_segment_name{analysis_session_.inputs.selection.field};
        auto enum_module_index{std::optional<std::size_t>{}};
        auto fallback_enum_module_index{std::optional<std::size_t>{}};
        auto const& modules{document_->manifest().modules};
        for (std::size_t module_index{}; module_index < modules.size(); ++module_index) {
            auto const enum_module{
                ((modules[module_index]).kind() == lispb::codegen::ModuleKind::normal
                     ? std::optional{modules[module_index]}
                     : std::nullopt)};
            if (!enum_module.has_value()) {
                continue;
            }
            if (!fallback_enum_module_index.has_value()) {
                fallback_enum_module_index = module_index;
            }
            if (declarations()
                    .module_settings_store
                    .namespace_name[declarations().module_settings_store.index(
                        declarations().normal_modules.settings[declarations().normal_modules.index(
                            *enum_module)])]
                    .value_or(lispb::IdentifierId{}) ==
                analysis_session_.inputs.workspace.types().namespace_name(node)) {
                enum_module_index = module_index;
                break;
            }
        }
        if (!enum_module_index.has_value()) {
            enum_module_index = fallback_enum_module_index;
        }
        ImGui::BeginDisabled(!enum_module_index.has_value() ||
                             !selected_resolved_field.has_value());
        if (ImGui::Button("Create enum for selected field...")) {
            new_enum_module_index_ = *enum_module_index;
            auto const suggested_name{suggested_type_name(
                text(declarations().packed_fields.name[declarations().packed_fields.index(
                    *selected_schema_field)]),
                "Type")};
            std::snprintf(
                new_enum_name_.data(), new_enum_name_.size(), "%s", suggested_name.c_str());

            auto unique_name{std::string{new_enum_name_.data()}};
            auto suffix_number{std::size_t{1}};
            while (std::ranges::find(
                       declarations().items(
                           declarations()
                               .normal_modules.declarations[declarations().normal_modules.index(
                                   modules[new_enum_module_index_])]),
                       strings().intern(unique_name),
                       [&](auto id) {
                           return lispb::codegen::declaration_name(declarations(), id);
                       }) !=
                   declarations()
                       .items(declarations()
                                  .normal_modules.declarations[declarations().normal_modules.index(
                                      modules[new_enum_module_index_])])
                       .end()) {
                unique_name = std::string{new_enum_name_.data()} + std::to_string(suffix_number++);
            }
            std::snprintf(new_enum_name_.data(), new_enum_name_.size(), "%s", unique_name.c_str());
            new_enum_backing_auto_ = true;
            new_enum_width_auto_ = false;
            new_enum_bit_width_ =
                analysis_session_.inputs.workspace.types().definitions().packed_fields.bit_width
                    [analysis_session_.inputs.workspace.types().definitions().packed_fields.index(
                        *selected_resolved_field)];
            new_enum_signedness_ = 1;
            pending_packed_enum_binding_ =
                PendingPackedBinding{.packed_declaration = *declaration,
                                     .field_name = strings().intern(selected_segment_name)};
            module_initiated_dialog_ = false;
            open_new_enum_dialog_ = true;
        }
        ImGui::EndDisabled();
        if (!enum_module_index.has_value()) {
            ImGui::SameLine();
            ImGui::TextDisabled("no ordinary module is available.");
        } else if (!selected_resolved_field.has_value()) {
            ImGui::SameLine();
            ImGui::TextDisabled("The selected field width is unresolved.");
        } else {
            ImGui::SameLine();
            ImGui::TextDisabled("Creates a shared enum declaration, then binds this field.");
        }

        auto scalar_module_index{std::optional<std::size_t>{}};
        auto fallback_scalar_module_index{std::optional<std::size_t>{}};
        for (std::size_t module_index{}; module_index < modules.size(); ++module_index) {
            auto const scalar_module{
                ((modules[module_index]).kind() == lispb::codegen::ModuleKind::normal
                     ? std::optional{modules[module_index]}
                     : std::nullopt)};
            if (!scalar_module.has_value()) {
                continue;
            }
            if (!fallback_scalar_module_index.has_value()) {
                fallback_scalar_module_index = module_index;
            }
            if (declarations()
                    .module_settings_store
                    .namespace_name[declarations().module_settings_store.index(
                        declarations().normal_modules.settings[declarations().normal_modules.index(
                            *scalar_module)])]
                    .value_or(lispb::IdentifierId{}) ==
                analysis_session_.inputs.workspace.types().namespace_name(node)) {
                scalar_module_index = module_index;
                break;
            }
        }
        if (!scalar_module_index.has_value()) {
            scalar_module_index = fallback_scalar_module_index;
        }
        auto const scalar_compatible{
            selected_resolved_field.has_value() && !selected_integer_scalar &&
            (declarations().packed_fields.kind[declarations().packed_fields.index(
                 *selected_schema_field)] == lispb::codegen::PackedFieldKind::signed_integer ||
             declarations().packed_fields.kind[declarations().packed_fields.index(
                 *selected_schema_field)] == lispb::codegen::PackedFieldKind::unsigned_integer)};
        ImGui::BeginDisabled(!scalar_module_index.has_value() || !scalar_compatible);
        if (ImGui::Button("Create integer scalar for selected field...")) {
            new_integer_scalar_module_index_ = *scalar_module_index;
            auto const scalar_module{modules[new_integer_scalar_module_index_]};
            auto const scalar_namespace{
                (declarations()
                         .module_settings_store
                         .namespace_name[declarations().module_settings_store.index(
                             declarations().normal_modules.settings
                                 [declarations().normal_modules.index(scalar_module)])]
                     ? text(*declarations()
                                 .module_settings_store
                                 .namespace_name[declarations().module_settings_store.index(
                                     declarations().normal_modules.settings
                                         [declarations().normal_modules.index(scalar_module)])])
                     : std::string{})};
            auto const suggested_name{suggested_type_name(
                text(declarations().packed_fields.name[declarations().packed_fields.index(
                    *selected_schema_field)]),
                "Value")};
            auto unique_name{suggested_name};
            auto suffix_number{std::size_t{1}};
            while (std::ranges::any_of(document_->types().types(), [&](auto const& candidate) {
                return text(document_->types().namespace_name(candidate)) == scalar_namespace &&
                       text(document_->types().name(candidate)) == unique_name;
            })) {
                unique_name = suggested_name + std::to_string(suffix_number++);
            }
            std::snprintf(new_integer_scalar_name_.data(),
                          new_integer_scalar_name_.size(),
                          "%s",
                          unique_name.c_str());

            new_integer_scalar_signed_ =
                declarations().packed_fields.kind[declarations().packed_fields.index(
                    *selected_schema_field)] == lispb::codegen::PackedFieldKind::signed_integer;
            new_integer_scalar_width_auto_ =
                !declarations()
                     .packed_fields.bits[declarations().packed_fields.index(*selected_schema_field)]
                     .has_value();
            new_integer_scalar_bit_width_ =
                analysis_session_.inputs.workspace.types().definitions().packed_fields.bit_width
                    [analysis_session_.inputs.workspace.types().definitions().packed_fields.index(
                        *selected_resolved_field)];

            auto minimum{lispb::codegen::PackedIntegerValue{0}};
            auto maximum{lispb::codegen::PackedIntegerValue{0}};
            if (declarations()
                    .packed_fields
                    .minimum_value[declarations().packed_fields.index(*selected_schema_field)]
                    .has_value() &&
                declarations()
                    .packed_fields
                    .maximum_value[declarations().packed_fields.index(*selected_schema_field)]
                    .has_value()) {
                minimum =
                    *declarations().packed_fields.minimum_value[declarations().packed_fields.index(
                        *selected_schema_field)];
                maximum =
                    *declarations().packed_fields.maximum_value[declarations().packed_fields.index(
                        *selected_schema_field)];
            } else if (new_integer_scalar_signed_) {
                auto const negative_limit{
                    analysis_session_.inputs.workspace.types()
                                .definitions()
                                .packed_fields
                                .bit_width[analysis_session_.inputs.workspace.types()
                                               .definitions()
                                               .packed_fields.index(*selected_resolved_field)] == 64
                        ? std::uint64_t{1} << 63
                        : std::uint64_t{1}
                              << (analysis_session_.inputs.workspace.types()
                                      .definitions()
                                      .packed_fields
                                      .bit_width[analysis_session_.inputs.workspace.types()
                                                     .definitions()
                                                     .packed_fields.index(
                                                         *selected_resolved_field)] -
                                  1)};
                minimum = lispb::codegen::PackedIntegerValue::from_parts(true, negative_limit);
                maximum = lispb::codegen::PackedIntegerValue{
                    static_cast<std::int64_t>(negative_limit - 1)};
            } else {
                maximum = lispb::codegen::PackedIntegerValue{
                    analysis_session_.inputs.workspace.types()
                                .definitions()
                                .packed_fields
                                .bit_width[analysis_session_.inputs.workspace.types()
                                               .definitions()
                                               .packed_fields.index(*selected_resolved_field)] == 64
                        ? (std::numeric_limits<std::uint64_t>::max)()
                        : (std::uint64_t{1}
                           << analysis_session_.inputs.workspace.types()
                                  .definitions()
                                  .packed_fields
                                  .bit_width[analysis_session_.inputs.workspace.types()
                                                 .definitions()
                                                 .packed_fields.index(*selected_resolved_field)]) -
                              1};
            }
            auto const minimum_text{lispb::codegen::format_packed_integer(minimum)};
            auto const maximum_text{lispb::codegen::format_packed_integer(maximum)};
            std::snprintf(new_integer_scalar_minimum_.data(),
                          new_integer_scalar_minimum_.size(),
                          "%s",
                          minimum_text.c_str());
            std::snprintf(new_integer_scalar_maximum_.data(),
                          new_integer_scalar_maximum_.size(),
                          "%s",
                          maximum_text.c_str());
            pending_packed_integer_scalar_binding_ =
                PendingPackedBinding{.packed_declaration = *declaration,
                                     .field_name = strings().intern(selected_segment_name)};
            module_initiated_dialog_ = false;
            open_new_integer_scalar_dialog_ = true;
        }
        ImGui::EndDisabled();
        if (!scalar_module_index.has_value()) {
            ImGui::SameLine();
            ImGui::TextDisabled("no ordinary module is available.");
        } else if (static_cast<bool>(selected_integer_scalar)) {
            ImGui::SameLine();
            ImGui::TextDisabled("This field already uses a shared integer scalar.");
        } else if (!scalar_compatible) {
            ImGui::SameLine();
            ImGui::TextDisabled("Requires a resolved signed or unsigned integer field.");
        } else {
            ImGui::SameLine();
            ImGui::TextDisabled("Moves this field's domain into a shared scalar declaration.");
        }

        auto representation_module_index{std::optional<std::size_t>{}};
        auto fallback_representation_module_index{std::optional<std::size_t>{}};
        for (std::size_t module_index{}; module_index < modules.size(); ++module_index) {
            auto const representation_module{
                ((modules[module_index]).kind() == lispb::codegen::ModuleKind::normal
                     ? std::optional{modules[module_index]}
                     : std::nullopt)};
            if (!representation_module.has_value()) {
                continue;
            }
            if (!fallback_representation_module_index.has_value()) {
                fallback_representation_module_index = module_index;
            }
            if (declarations()
                    .module_settings_store
                    .namespace_name[declarations().module_settings_store.index(
                        declarations().normal_modules.settings[declarations().normal_modules.index(
                            *representation_module)])]
                    .value_or(lispb::IdentifierId{}) ==
                analysis_session_.inputs.workspace.types().namespace_name(node)) {
                representation_module_index = module_index;
                break;
            }
        }
        if (!representation_module_index.has_value()) {
            representation_module_index = fallback_representation_module_index;
        }
        auto const plain_integer_field{
            selected_resolved_field.has_value() && !selected_integer_scalar &&
            (declarations().packed_fields.kind[declarations().packed_fields.index(
                 *selected_schema_field)] == lispb::codegen::PackedFieldKind::signed_integer ||
             declarations().packed_fields.kind[declarations().packed_fields.index(
                 *selected_schema_field)] == lispb::codegen::PackedFieldKind::unsigned_integer) &&
            !declarations()
                 .packed_fields
                 .minimum_value[declarations().packed_fields.index(*selected_schema_field)]
                 .has_value() &&
            !declarations()
                 .packed_fields
                 .maximum_value[declarations().packed_fields.index(*selected_schema_field)]
                 .has_value() &&
            declarations()
                .items(declarations().packed_fields.named_codes[declarations().packed_fields.index(
                    *selected_schema_field)])
                .empty() &&
            !declarations()
                 .packed_fields
                 .relationship[declarations().packed_fields.index(*selected_schema_field)]
                 .has_value() &&
            !declarations().packed_fields.range_helper[declarations().packed_fields.index(
                *selected_schema_field)]};
        ImGui::BeginDisabled(!representation_module_index.has_value() || !plain_integer_field);
        if (ImGui::Button("Create fixed point for selected field...")) {
            new_fixed_point_module_index_ = *representation_module_index;
            auto const module{modules[new_fixed_point_module_index_]};
            auto const namespace_name{
                (declarations()
                         .module_settings_store
                         .namespace_name[declarations().module_settings_store.index(
                             declarations().normal_modules.settings
                                 [declarations().normal_modules.index(module)])]
                     ? text(*declarations()
                                 .module_settings_store
                                 .namespace_name[declarations().module_settings_store.index(
                                     declarations().normal_modules.settings
                                         [declarations().normal_modules.index(module)])])
                     : std::string{})};
            auto const suggested_name{suggested_type_name(
                text(declarations().packed_fields.name[declarations().packed_fields.index(
                    *selected_schema_field)]),
                "Fixed")};
            auto unique_name{suggested_name};
            auto suffix_number{std::size_t{1}};
            while (std::ranges::any_of(document_->types().types(), [&](auto const& candidate) {
                return text(document_->types().namespace_name(candidate)) == namespace_name &&
                       text(document_->types().name(candidate)) == unique_name;
            })) {
                unique_name = suggested_name + std::to_string(suffix_number++);
            }
            std::snprintf(new_fixed_point_name_.data(),
                          new_fixed_point_name_.size(),
                          "%s",
                          unique_name.c_str());
            new_fixed_point_signed_ =
                declarations().packed_fields.kind[declarations().packed_fields.index(
                    *selected_schema_field)] == lispb::codegen::PackedFieldKind::signed_integer;
            new_fixed_point_total_bits_ =
                analysis_session_.inputs.workspace.types().definitions().packed_fields.bit_width
                    [analysis_session_.inputs.workspace.types().definitions().packed_fields.index(
                        *selected_resolved_field)];
            auto const available_magnitude_bits{new_fixed_point_total_bits_ -
                                                (new_fixed_point_signed_ ? 1U : 0U)};
            new_fixed_point_fractional_bits_ =
                std::min(new_fixed_point_total_bits_ / 2, available_magnitude_bits);
            new_fixed_point_rounding_ = 0;
            pending_packed_fixed_point_binding_ =
                PendingPackedBinding{.packed_declaration = *declaration,
                                     .field_name = strings().intern(selected_segment_name)};
            module_initiated_dialog_ = false;
            open_new_fixed_point_dialog_ = true;
        }
        ImGui::EndDisabled();
        if (!representation_module_index.has_value()) {
            ImGui::SameLine();
            ImGui::TextDisabled("no ordinary module is available.");
        } else if (!plain_integer_field) {
            ImGui::SameLine();
            ImGui::TextDisabled("Requires a plain integer field without local semantic metadata.");
        } else {
            ImGui::SameLine();
            ImGui::TextDisabled("Creates a shared scaled-integer representation for this width.");
        }

        auto const mini_float_compatible{
            plain_integer_field &&
            analysis_session_.inputs.workspace.types().definitions().packed_fields.bit_width
                    [analysis_session_.inputs.workspace.types().definitions().packed_fields.index(
                        *selected_resolved_field)] >=
                (declarations().packed_fields.kind[declarations().packed_fields.index(
                     *selected_schema_field)] == lispb::codegen::PackedFieldKind::signed_integer
                     ? 3U
                     : 2U)};
        ImGui::BeginDisabled(!representation_module_index.has_value() || !mini_float_compatible);
        if (ImGui::Button("Create mini float for selected field...")) {
            new_mini_float_module_index_ = *representation_module_index;
            auto const module{modules[new_mini_float_module_index_]};
            auto const namespace_name{
                (declarations()
                         .module_settings_store
                         .namespace_name[declarations().module_settings_store.index(
                             declarations().normal_modules.settings
                                 [declarations().normal_modules.index(module)])]
                     ? text(*declarations()
                                 .module_settings_store
                                 .namespace_name[declarations().module_settings_store.index(
                                     declarations().normal_modules.settings
                                         [declarations().normal_modules.index(module)])])
                     : std::string{})};
            auto const suggested_name{suggested_type_name(
                text(declarations().packed_fields.name[declarations().packed_fields.index(
                    *selected_schema_field)]),
                "Float")};
            auto unique_name{suggested_name};
            auto suffix_number{std::size_t{1}};
            while (std::ranges::any_of(document_->types().types(), [&](auto const& candidate) {
                return text(document_->types().namespace_name(candidate)) == namespace_name &&
                       text(document_->types().name(candidate)) == unique_name;
            })) {
                unique_name = suggested_name + std::to_string(suffix_number++);
            }
            std::snprintf(new_mini_float_name_.data(),
                          new_mini_float_name_.size(),
                          "%s",
                          unique_name.c_str());
            new_mini_float_sign_bits_ =
                declarations().packed_fields.kind[declarations().packed_fields.index(
                    *selected_schema_field)] == lispb::codegen::PackedFieldKind::signed_integer
                    ? 1U
                    : 0U;
            auto const available_bits{
                analysis_session_.inputs.workspace.types().definitions().packed_fields.bit_width
                    [analysis_session_.inputs.workspace.types().definitions().packed_fields.index(
                        *selected_resolved_field)] -
                new_mini_float_sign_bits_};
            new_mini_float_exponent_bits_ = std::min(5U, available_bits);
            new_mini_float_significand_bits_ = available_bits - new_mini_float_exponent_bits_;
            new_mini_float_exponent_bias_ =
                static_cast<std::int32_t>((1U << (new_mini_float_exponent_bits_ - 1)) - 1);
            pending_packed_mini_float_binding_ =
                PendingPackedBinding{.packed_declaration = *declaration,
                                     .field_name = strings().intern(selected_segment_name)};
            module_initiated_dialog_ = false;
            open_new_mini_float_dialog_ = true;
        }
        ImGui::EndDisabled();
        if (!representation_module_index.has_value()) {
            ImGui::SameLine();
            ImGui::TextDisabled("no ordinary module is available.");
        } else if (!mini_float_compatible) {
            ImGui::SameLine();
            ImGui::TextDisabled("Requires a plain integer field with at least two encoding bits.");
        } else {
            ImGui::SameLine();
            ImGui::TextDisabled("Creates a shared floating-like encoding for this width.");
        }

        if (section_with_tooltip(
                "Semantic relationship",
                "Describe how the selected field relates to another semantic type.")) {
            if (static_cast<bool>(selected_linear_quantized)) {
                ImGui::TextDisabled("This placement inherits semantic meaning through the "
                                    "quantizer's source scalar.");
            } else if (static_cast<bool>(selected_fixed_point)) {
                ImGui::TextDisabled(
                    "This fixed-point placement owns its scaled numerical representation.");
            } else if (static_cast<bool>(selected_mini_float)) {
                ImGui::TextDisabled("This mini-float placement owns its numerical encoding.");
            }
            ImGui::BeginDisabled(static_cast<bool>(selected_linear_quantized) ||
                                 static_cast<bool>(selected_fixed_point) ||
                                 static_cast<bool>(selected_mini_float));
            ImGui::SetNextItemWidth(180.0F);
            auto const current_kind{semantic_relationship_kinds[static_cast<std::size_t>(
                std::clamp(packed_relationship_kind_,
                           0,
                           static_cast<int>(semantic_relationship_kinds.size() - 1)))]};
            auto const current_unit{semantic_relationship_units[static_cast<std::size_t>(
                std::clamp(packed_relationship_unit_,
                           0,
                           static_cast<int>(semantic_relationship_units.size() - 1)))]};
            if (ImGui::BeginCombo(
                    "Kind", lispb::codegen::semantic_relation_kind_name(current_kind).data())) {
                for (std::size_t kind_index{}; kind_index < semantic_relationship_kinds.size();
                     ++kind_index) {
                    auto const kind{semantic_relationship_kinds[kind_index]};
                    auto const chosen{packed_relationship_kind_ == static_cast<int>(kind_index)};
                    if (ImGui::Selectable(lispb::codegen::semantic_relation_kind_name(kind).data(),
                                          chosen)) {
                        packed_relationship_kind_ = static_cast<int>(kind_index);
                        if (declarations()
                                .packed_fields
                                .relationship[declarations().packed_fields.index(
                                    *selected_schema_field)]
                                .has_value()) {
                            auto replacement{
                                document_->storage().declarations.packed_values.clone(*schema)};
                            document_->storage()
                                .declarations.packed_values
                                .segments[document_->storage().declarations.packed_values.index(
                                    replacement)] = [&] {
                                auto const original_items{declarations().items(
                                    document_->storage().declarations.packed_values.segments
                                        [document_->storage().declarations.packed_values.index(
                                            replacement)])};
                                auto segments{
                                    std::vector(original_items.begin(), original_items.end())};
                                segments[*selected_index] = [&] {
                                    auto field{declarations().packed_fields.clone(
                                        segments[*selected_index])};
                                    declarations().packed_fields.relationship
                                        [declarations().packed_fields.index(field)] = [&] {
                                        auto relationship{declarations().semantic_relations.clone(
                                            *declarations().packed_fields.relationship
                                                 [declarations().packed_fields.index(field)])};
                                        declarations()
                                            .semantic_relations
                                            .kind[declarations().semantic_relations.index(
                                                relationship)] = kind;
                                        declarations()
                                            .semantic_relations
                                            .unit[declarations().semantic_relations.index(
                                                relationship)] =
                                            kind == lispb::codegen::SemanticRelationKind::
                                                        offset_into
                                                ? std::optional{current_unit}
                                                : std::nullopt;
                                        return relationship;
                                    }();
                                    return field;
                                }();
                                return declarations().range(segments);
                            }();
                            if (apply_document_edit(document_->replace_declaration(
                                    *declaration, std::move(replacement)))) {
                                analysis_session_.inputs.selection.field = selected_segment_name;
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
                        "Unit", lispb::codegen::semantic_relation_unit_name(current_unit).data())) {
                    for (std::size_t unit_index{}; unit_index < semantic_relationship_units.size();
                         ++unit_index) {
                        auto const unit{semantic_relationship_units[unit_index]};
                        auto const chosen{packed_relationship_unit_ ==
                                          static_cast<int>(unit_index)};
                        if (ImGui::Selectable(
                                lispb::codegen::semantic_relation_unit_name(unit).data(), chosen)) {
                            packed_relationship_unit_ = static_cast<int>(unit_index);
                            if (declarations()
                                    .packed_fields
                                    .relationship[declarations().packed_fields.index(
                                        *selected_schema_field)]
                                    .has_value()) {
                                auto replacement{
                                    document_->storage().declarations.packed_values.clone(*schema)};
                                document_->storage()
                                    .declarations.packed_values
                                    .segments[document_->storage().declarations.packed_values.index(
                                        replacement)] = [&] {
                                    auto const original_items{declarations().items(
                                        document_->storage().declarations.packed_values.segments
                                            [document_->storage().declarations.packed_values.index(
                                                replacement)])};
                                    auto rows_2{
                                        std::vector(original_items.begin(), original_items.end())};
                                    rows_2[*selected_index] = [&] {
                                        auto row_1{declarations().packed_fields.clone(
                                            rows_2[*selected_index])};
                                        (*declarations().packed_fields.relationship
                                              [declarations().packed_fields.index(row_1)]) = [&] {
                                            auto row_0{declarations().semantic_relations.clone((
                                                *declarations().packed_fields.relationship
                                                     [declarations().packed_fields.index(row_1)]))};
                                            declarations().semantic_relations.unit
                                                [declarations().semantic_relations.index(row_0)] =
                                                unit;
                                            return row_0;
                                        }();
                                        return row_1;
                                    }();
                                    return declarations().range(rows_2);
                                }();
                                if (apply_document_edit(document_->replace_declaration(
                                        *declaration, std::move(replacement)))) {
                                    analysis_session_.inputs.selection.field =
                                        selected_segment_name;
                                    return true;
                                }
                            }
                        }
                    }
                    ImGui::EndCombo();
                }
            }

            ImGui::SetNextItemWidth(std::max(80.0F, ImGui::GetContentRegionAvail().x - 132.0F));
            auto const target_submitted{ImGui::InputText("Target",
                                                         packed_relationship_target_.data(),
                                                         packed_relationship_target_.size(),
                                                         ImGuiInputTextFlags_EnterReturnsTrue)};
            if (declarations()
                    .packed_fields
                    .relationship[declarations().packed_fields.index(*selected_schema_field)]
                    .has_value() &&
                (target_submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                if (packed_relationship_target_.front() == '\0') {
                    schema_edit_message_ = "Relationship target cannot be empty.";
                } else {
                    auto replacement{
                        document_->storage().declarations.packed_values.clone(*schema)};
                    document_->storage().declarations.packed_values.segments
                        [document_->storage().declarations.packed_values.index(replacement)] = [&] {
                        auto const original_items{declarations().items(
                            document_->storage()
                                .declarations.packed_values
                                .segments[document_->storage().declarations.packed_values.index(
                                    replacement)])};
                        auto rows_3{std::vector(original_items.begin(), original_items.end())};
                        rows_3[*selected_index] = [&] {
                            auto row_2{declarations().packed_fields.clone(rows_3[*selected_index])};
                            (*declarations()
                                  .packed_fields
                                  .relationship[declarations().packed_fields.index(row_2)]) = [&] {
                                auto row_1{declarations().semantic_relations.clone(
                                    (*declarations().packed_fields.relationship
                                          [declarations().packed_fields.index(row_2)]))};
                                declarations()
                                    .semantic_relations
                                    .target[declarations().semantic_relations.index(row_1)] = [&] {
                                    auto row_0{declarations().type_refs.clone(
                                        declarations().semantic_relations.target
                                            [declarations().semantic_relations.index(row_1)])};
                                    declarations()
                                        .type_refs.name[declarations().type_refs.index(row_0)] =
                                        strings().intern(packed_relationship_target_.data());
                                    return row_0;
                                }();
                                return row_1;
                            }();
                            return row_2;
                        }();
                        return declarations().range(rows_3);
                    }();
                    if (apply_document_edit(
                            document_->replace_declaration(*declaration, std::move(replacement)))) {
                        analysis_session_.inputs.selection.field = selected_segment_name;
                        return true;
                    }
                }
            }
            ImGui::SameLine();
            ImGui::PushID("relationship-target");
            auto picked_relationship_target{
                draw_type_picker(analysis_session_.inputs.workspace.types().module_name(node),
                                 analysis_session_.inputs.workspace.types().identity(node))};
            ImGui::PopID();
            if (picked_relationship_target.has_value()) {
                std::snprintf(packed_relationship_target_.data(),
                              packed_relationship_target_.size(),
                              "%s",
                              picked_relationship_target->c_str());
                auto replacement{document_->storage().declarations.packed_values.clone(*schema)};

                document_->storage().declarations.packed_values.segments
                    [document_->storage().declarations.packed_values.index(replacement)] = [&] {
                    auto const original_items{declarations().items(
                        document_->storage().declarations.packed_values.segments
                            [document_->storage().declarations.packed_values.index(replacement)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[*selected_index] = [&] {
                        auto row_0{declarations().packed_fields.clone(rows_1[*selected_index])};
                        declarations()
                            .packed_fields
                            .relationship[declarations().packed_fields.index(row_0)] = [&] {
                            auto const new_semantic_relations{
                                declarations().semantic_relations.append()};
                            declarations()
                                .semantic_relations.kind[declarations().semantic_relations.index(
                                    new_semantic_relations)] = current_kind;
                            declarations()
                                .semantic_relations.target[declarations().semantic_relations.index(
                                    new_semantic_relations)] = [&] {
                                auto const new_type_refs{declarations().type_refs.append()};
                                declarations()
                                    .type_refs.name[declarations().type_refs.index(new_type_refs)] =
                                    strings().intern(*picked_relationship_target);
                                declarations().type_refs.suffix[declarations().type_refs.index(
                                    new_type_refs)] = {};
                                declarations().type_refs.nested[declarations().type_refs.index(
                                    new_type_refs)] = std::nullopt;
                                return new_type_refs;
                            }();
                            declarations()
                                .semantic_relations.unit[declarations().semantic_relations.index(
                                    new_semantic_relations)] =
                                current_kind == lispb::codegen::SemanticRelationKind::offset_into
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
                    analysis_session_.inputs.selection.field = selected_segment_name;
                    return true;
                }
            }
            ImGui::SameLine();
            if (declarations()
                    .packed_fields
                    .relationship[declarations().packed_fields.index(*selected_schema_field)]
                    .has_value()) {
                if (ImGui::SmallButton("Clear")) {
                    auto replacement{
                        document_->storage().declarations.packed_values.clone(*schema)};
                    document_->storage().declarations.packed_values.segments
                        [document_->storage().declarations.packed_values.index(replacement)] = [&] {
                        auto const original_items{declarations().items(
                            document_->storage()
                                .declarations.packed_values
                                .segments[document_->storage().declarations.packed_values.index(
                                    replacement)])};
                        auto rows_1{std::vector(original_items.begin(), original_items.end())};
                        rows_1[*selected_index] = [&] {
                            auto row_0{declarations().packed_fields.clone(rows_1[*selected_index])};
                            declarations()
                                .packed_fields
                                .relationship[declarations().packed_fields.index(row_0)]
                                .reset();
                            return row_0;
                        }();
                        return declarations().range(rows_1);
                    }();
                    if (apply_document_edit(
                            document_->replace_declaration(*declaration, std::move(replacement)))) {
                        analysis_session_.inputs.selection.field = selected_segment_name;
                        return true;
                    }
                }
                ImGui::SameLine();
                if (selected_resolved_field.has_value() &&
                    lispb::schema::is_valid(
                        analysis_session_.inputs.workspace.types()
                            .definitions()
                            .packed_fields
                            .relationship[analysis_session_.inputs.workspace.types()
                                              .definitions()
                                              .packed_fields.index(*selected_resolved_field)]) &&
                    detail::semantic_type_navigation_button()) {
                    select_type(
                        analysis_session_.inputs.workspace.types()
                            .occurrences()
                            .type[analysis_session_.inputs.workspace.types().occurrences().index(
                                analysis_session_.inputs.workspace.types().relationships().target
                                    [analysis_session_.inputs.workspace.types()
                                         .relationships()
                                         .index(analysis_session_.inputs.workspace.types()
                                                    .definitions()
                                                    .packed_fields.relationship
                                                        [analysis_session_.inputs.workspace.types()
                                                             .definitions()
                                                             .packed_fields.index(
                                                                 *selected_resolved_field)])])]);
                    analysis_session_.inputs.selection.field.clear();
                    analysis_session_.inputs.selection.packed_access_fields.clear();
                    analysis_session_.inputs.selection.packed_access_set_explicit = false;
                    analysis_session_.inputs.selection.record_access_members.clear();
                    analysis_session_.inputs.selection.record_access_set_explicit = false;
                    return true;
                }
            } else {
                ImGui::BeginDisabled(packed_relationship_target_.front() == '\0');
                if (ImGui::SmallButton("Add")) {
                    auto replacement{
                        document_->storage().declarations.packed_values.clone(*schema)};
                    document_->storage().declarations.packed_values.segments
                        [document_->storage().declarations.packed_values.index(replacement)] = [&] {
                        auto const original_items{declarations().items(
                            document_->storage()
                                .declarations.packed_values
                                .segments[document_->storage().declarations.packed_values.index(
                                    replacement)])};
                        auto rows_1{std::vector(original_items.begin(), original_items.end())};
                        rows_1[*selected_index] = [&] {
                            auto row_0{declarations().packed_fields.clone(rows_1[*selected_index])};
                            declarations()
                                .packed_fields
                                .relationship[declarations().packed_fields.index(row_0)] = [&] {
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
                                        strings().intern(packed_relationship_target_.data());
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
                        analysis_session_.inputs.selection.field = selected_segment_name;
                        return true;
                    }
                }
                ImGui::EndDisabled();
            }
            ImGui::EndDisabled();
            ImGui::TextDisabled(
                "Relationships are semantic graph edges; session capacity analysis does not change "
                "durable source widths.");
            layout::PackedFieldAnalysis const* analysis_field{};
            if (analysis_session_.results().active_packed.has_value()) {
                auto const found{
                    std::ranges::find(analysis_session_.results().active_packed->fields,
                                      selected_segment_name,
                                      &layout::PackedFieldAnalysis::name)};
                if (found != analysis_session_.results().active_packed->fields.end()) {
                    analysis_field = &*found;
                }
            }
            if (analysis_field != nullptr &&
                analysis_field->relationship_target_extent.has_value()) {
                auto const kind{*analysis_field->relationship_kind};
                auto const term{detail::relationship_extent_term(kind)};
                auto const unit{
                    detail::relationship_extent_unit(kind, analysis_field->relationship_unit)};
                auto const heading{"Session " + std::string{term} + " requirement"};
                if (detail::section(heading.c_str())) {
                    ImGui::Text("Target %s: %llu %s",
                                term.data(),
                                static_cast<unsigned long long>(
                                    *analysis_field->relationship_target_extent),
                                unit.data());
                    ImGui::Text("Live values: %s",
                                analysis_field->relationship_live_value_count.has_value()
                                    ? detail::format_code_count(
                                          *analysis_field->relationship_live_value_count)
                                          .c_str()
                                    : "Unknown");
                    auto const required_codes{
                        analysis_field->relationship_required_code_count.has_value()
                            ? detail::format_code_count(
                                  *analysis_field->relationship_required_code_count)
                        : analysis_field->relationship_minimum_required_bits.value_or(0) > 64
                            ? std::string{"> 2^64"}
                            : std::string{"Unknown"}};
                    ImGui::Text("Required codes: %s", required_codes.c_str());
                    ImGui::Text(
                        "Minimum width: %s",
                        analysis_field->relationship_minimum_required_bits.has_value()
                            ? (std::to_string(*analysis_field->relationship_minimum_required_bits) +
                               " bits")
                                  .c_str()
                            : "Unknown");
                    ImGui::Text(
                        "Current planning width fits: %s",
                        analysis_field->relationship_width_sufficient.has_value()
                            ? (*analysis_field->relationship_width_sufficient ? "Yes" : "No")
                            : "Unknown");
                    ImGui::Text("Code-space %s limit: %s",
                                term.data(),
                                detail::format_number(
                                    analysis_field->relationship_code_space_capacity_limit)
                                    .c_str());
                    ImGui::Text(
                        "Code-space %s headroom: %s",
                        term.data(),
                        detail::format_number(analysis_field->relationship_capacity_headroom)
                            .c_str());
                    ImGui::Text(
                        "Semantic-range %s limit: %s",
                        term.data(),
                        detail::format_number(analysis_field->relationship_semantic_capacity_limit)
                            .c_str());
                    ImGui::Text(
                        "Sentinel-placement %s limit: %s",
                        term.data(),
                        detail::format_number(analysis_field->relationship_sentinel_capacity_limit)
                            .c_str());
                    ImGui::Text(
                        "Effective valid %s limit: %s",
                        term.data(),
                        detail::format_number(analysis_field->relationship_effective_capacity_limit)
                            .c_str());
                    ImGui::Text("Effective valid %s headroom: %s",
                                term.data(),
                                detail::format_number(
                                    analysis_field->relationship_effective_capacity_headroom)
                                    .c_str());
                    if (kind == lispb::codegen::SemanticRelationKind::count_of) {
                        ImGui::TextDisabled("Live counts include 0 through capacity; named "
                                            "sentinels add code states.");
                    } else if (kind == lispb::codegen::SemanticRelationKind::offset_into) {
                        ImGui::TextDisabled("Live offsets span 0 through extent-1 in the declared "
                                            "unit; named sentinels "
                                            "add code states.");
                    } else {
                        ImGui::TextDisabled("Live indices span 0 through capacity-1; named "
                                            "sentinels add code states.");
                    }
                }
            }
        }
    }

    auto selected_code_after_edit{selected_packed_code_};
    if (!pending.has_value() && selected_schema_field.has_value() &&
        static_cast<bool>(selected_integer_scalar) && selected_resolved_field.has_value()) {
        if (detail::section("Shared scalar domain")) {
            ImGui::TextDisabled("Range, signedness, and named codes are authored on the referenced "
                                "integer scalar.");
            for (auto const id : declarations().items(
                     analysis_session_.inputs.workspace.types()
                         .definitions()
                         .packed_fields
                         .named_codes[analysis_session_.inputs.workspace.types()
                                          .definitions()
                                          .packed_fields.index(*selected_resolved_field)])) {
                auto const code{id};
                auto const value{lispb::codegen::format_packed_integer(
                    declarations()
                        .packed_named_codes.value[declarations().packed_named_codes.index(code)])};
                ImGui::BulletText(
                    "%s = %s%s",
                    text(
                        declarations()
                            .packed_named_codes.name[declarations().packed_named_codes.index(code)])
                        .c_str(),
                    value.c_str(),
                    declarations()
                            .packed_named_codes
                            .sentinel[declarations().packed_named_codes.index(code)]
                        ? " (sentinel)"
                        : "");
            }
        }
    } else if (!pending.has_value() && selected_schema_field.has_value() &&
               static_cast<bool>(selected_linear_quantized)) {
        if (detail::section("Linear quantization")) {
            layout::LinearQuantizedAnalysis const* quantization{};
            if (analysis_session_.results().active_packed.has_value()) {
                auto const analyzed_field{
                    std::ranges::find(analysis_session_.results().active_packed->fields,
                                      analysis_session_.inputs.selection.field,
                                      &layout::PackedFieldAnalysis::name)};
                if (analyzed_field != analysis_session_.results().active_packed->fields.end() &&
                    analyzed_field->linear_quantized.has_value()) {
                    quantization = &*analyzed_field->linear_quantized;
                }
            }
            if (quantization != nullptr) {
                auto const source_minimum{
                    lispb::codegen::format_packed_integer(quantization->source_minimum)};
                auto const source_maximum{
                    lispb::codegen::format_packed_integer(quantization->source_maximum)};
                ImGui::Text("Source range: %s..%s", source_minimum.c_str(), source_maximum.c_str());
                ImGui::Text("Encoded width: %u bits", quantization->encoded_storage_bits);
                ImGui::Text("Usable codes: %s",
                            detail::format_code_count(quantization->usable_code_count).c_str());
                ImGui::Text("Reserved codes: %llu",
                            static_cast<unsigned long long>(quantization->reserved_code_count));
                ImGui::Text("Resolution: %.9Lg", quantization->resolution);
                ImGui::Text("Maximum rounding error: %.9Lg", quantization->maximum_rounding_error);
                ImGui::Text(
                    "Clipping: %s",
                    lispb::codegen::quantization_clipping_name(quantization->clipping).data());
            } else {
                ImGui::TextDisabled("Quantization analysis is unavailable.");
            }
            ImGui::TextDisabled("Generated packed APIs expose encoded codes; decoding remains "
                                "representation policy.");
        }
    } else if (!pending.has_value() && selected_schema_field.has_value() &&
               static_cast<bool>(selected_fixed_point)) {
        if (detail::section("Fixed point")) {
            layout::FixedPointAnalysis const* fixed{};
            if (analysis_session_.results().active_packed.has_value()) {
                auto const analyzed_field{
                    std::ranges::find(analysis_session_.results().active_packed->fields,
                                      analysis_session_.inputs.selection.field,
                                      &layout::PackedFieldAnalysis::name)};
                if (analyzed_field != analysis_session_.results().active_packed->fields.end() &&
                    analyzed_field->fixed_point.has_value()) {
                    fixed = &*analyzed_field->fixed_point;
                }
            }
            if (fixed != nullptr) {
                auto const minimum_raw{
                    lispb::codegen::format_packed_integer(fixed->minimum_raw_value)};
                auto const maximum_raw{
                    lispb::codegen::format_packed_integer(fixed->maximum_raw_value)};
                ImGui::Text("Raw range: %s..%s", minimum_raw.c_str(), maximum_raw.c_str());
                ImGui::Text(
                    "Numerical range: %.9Lg..%.9Lg", fixed->minimum_value, fixed->maximum_value);
                ImGui::Text("Allowed range: %.9Lg..%.9Lg",
                            fixed->minimum_allowed_value,
                            fixed->maximum_allowed_value);
                ImGui::Text("Width: %u total / %u whole / %u fractional bits",
                            fixed->total_bits,
                            fixed->whole_bits,
                            fixed->fractional_bits);
                ImGui::Text("Scale: %.9Lg", fixed->scale);
                ImGui::Text("Resolution: %.9Lg", fixed->resolution);
                ImGui::Text("Maximum rounding error: %.9Lg", fixed->maximum_rounding_error);
                ImGui::Text("Rounding: %s",
                            lispb::codegen::fixed_point_rounding_name(fixed->rounding).data());
            } else {
                ImGui::TextDisabled("Fixed-point analysis is unavailable.");
            }
            ImGui::TextDisabled(
                "Generated packed APIs expose raw codes and checked double conversion.");
        }
    } else if (!pending.has_value() && selected_schema_field.has_value() &&
               static_cast<bool>(selected_mini_float)) {
        if (detail::section("Mini float")) {
            layout::MiniFloatAnalysis const* mini{};
            if (analysis_session_.results().active_packed.has_value()) {
                auto const analyzed_field{
                    std::ranges::find(analysis_session_.results().active_packed->fields,
                                      analysis_session_.inputs.selection.field,
                                      &layout::PackedFieldAnalysis::name)};
                if (analyzed_field != analysis_session_.results().active_packed->fields.end() &&
                    analyzed_field->mini_float.has_value()) {
                    mini = &*analyzed_field->mini_float;
                }
            }
            if (mini != nullptr) {
                ImGui::Text("Width: %u sign / %u exponent / %u significand bits",
                            mini->sign_bits,
                            mini->exponent_bits,
                            mini->significand_bits);
                ImGui::Text("Exponent bias: %d", mini->exponent_bias);
                if (mini->minimum_finite.has_value() && mini->maximum_finite.has_value()) {
                    ImGui::Text(
                        "Finite range: %.9Lg..%.9Lg", *mini->minimum_finite, *mini->maximum_finite);
                } else {
                    ImGui::TextDisabled("Finite range: Unknown");
                }
                ImGui::Text("Zero / infinity / NaN codes: %llu / %llu / %llu",
                            static_cast<unsigned long long>(mini->zero_code_count),
                            static_cast<unsigned long long>(mini->infinity_code_count),
                            static_cast<unsigned long long>(mini->nan_code_count));
            } else {
                ImGui::TextDisabled("Mini-float analysis is unavailable.");
            }
            ImGui::TextDisabled("Generated packed APIs expose encoded bits, not decoded values.");
        }
    } else if (!pending.has_value() && selected_schema_field.has_value() &&
               selected_index.has_value() && !selected_linear_quantized && !selected_fixed_point &&
               !selected_mini_float) {
        auto const selected_segment_name{analysis_session_.inputs.selection.field};
        auto const effective_width{
            selected_resolved_field.has_value()
                ? analysis_session_.inputs.workspace.types().definitions().packed_fields.bit_width
                      [analysis_session_.inputs.workspace.types().definitions().packed_fields.index(
                          *selected_resolved_field)]
                : 1U};
        auto const named_value{
            first_available_code(declarations(), *selected_schema_field, effective_width, false)};
        auto const sentinel_value{
            first_available_code(declarations(), *selected_schema_field, effective_width, true)};
        auto const current_code_index{
            selected_code ==
                    declarations()
                        .items(declarations()
                                   .packed_fields.named_codes[declarations().packed_fields.index(
                                       *selected_schema_field)])
                        .end()
                ? std::optional<std::size_t>{}
                : std::optional<std::size_t>{static_cast<std::size_t>(
                      selected_code -
                      declarations()
                          .items(declarations().packed_fields.named_codes
                                     [declarations().packed_fields.index(*selected_schema_field)])
                          .begin())}};

        if (detail::section("Named codes")) {
            ImGui::TextDisabled("Named codes remain ordinary integer values; sentinels sit outside "
                                "the live range.");
            ImGui::BeginDisabled(!named_value.has_value());
            if (ImGui::Button("+ Named code")) {
                auto replacement{document_->storage().declarations.packed_values.clone(*schema)};

                auto name{unique_child_name(
                    declarations(),
                    declarations().packed_named_codes,
                    declarations().packed_named_codes.name,
                    declarations().items(
                        declarations().packed_fields.named_codes[declarations().packed_fields.index(
                            declarations().items(
                                document_->storage().declarations.packed_values.segments
                                    [document_->storage().declarations.packed_values.index(
                                        replacement)])[*selected_index])]),
                    "Code")};
                document_->storage().declarations.packed_values.segments
                    [document_->storage().declarations.packed_values.index(replacement)] = [&] {
                    auto const original_items{declarations().items(
                        document_->storage().declarations.packed_values.segments
                            [document_->storage().declarations.packed_values.index(replacement)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[*selected_index] = [&] {
                        auto row_0{declarations().packed_fields.clone(rows_1[*selected_index])};
                        declarations().packed_fields.named_codes[declarations().packed_fields.index(
                            row_0)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_fields
                                    .named_codes[declarations().packed_fields.index(row_0)])};
                            auto rows{std::vector(original_items.begin(), original_items.end())};
                            rows.push_back([&] {
                                auto const new_packed_named_codes{
                                    declarations().packed_named_codes.append()};
                                declarations()
                                    .packed_named_codes
                                    .name[declarations().packed_named_codes.index(
                                        new_packed_named_codes)] = strings().intern(name);
                                declarations()
                                    .packed_named_codes
                                    .value[declarations().packed_named_codes.index(
                                        new_packed_named_codes)] = *named_value;
                                declarations()
                                    .packed_named_codes
                                    .sentinel[declarations().packed_named_codes.index(
                                        new_packed_named_codes)] = false;
                                return new_packed_named_codes;
                            }());
                            return declarations().range(rows);
                        }();
                        return row_0;
                    }();
                    return declarations().range(rows_1);
                }();
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
                    analysis_session_.inputs.selection.field = selected_segment_name;
                    selected_packed_code_ = std::move(name);
                    ImGui::EndDisabled();
                    return true;
                }
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(!sentinel_value.has_value());
            if (ImGui::Button("+ Sentinel")) {
                auto replacement{document_->storage().declarations.packed_values.clone(*schema)};

                auto name{unique_child_name(
                    declarations(),
                    declarations().packed_named_codes,
                    declarations().packed_named_codes.name,
                    declarations().items(
                        declarations().packed_fields.named_codes[declarations().packed_fields.index(
                            declarations().items(
                                document_->storage().declarations.packed_values.segments
                                    [document_->storage().declarations.packed_values.index(
                                        replacement)])[*selected_index])]),
                    "Invalid")};
                document_->storage().declarations.packed_values.segments
                    [document_->storage().declarations.packed_values.index(replacement)] = [&] {
                    auto const original_items{declarations().items(
                        document_->storage().declarations.packed_values.segments
                            [document_->storage().declarations.packed_values.index(replacement)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[*selected_index] = [&] {
                        auto row_0{declarations().packed_fields.clone(rows_1[*selected_index])};
                        declarations().packed_fields.named_codes[declarations().packed_fields.index(
                            row_0)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_fields
                                    .named_codes[declarations().packed_fields.index(row_0)])};
                            auto rows{std::vector(original_items.begin(), original_items.end())};
                            rows.push_back([&] {
                                auto const new_packed_named_codes{
                                    declarations().packed_named_codes.append()};
                                declarations()
                                    .packed_named_codes
                                    .name[declarations().packed_named_codes.index(
                                        new_packed_named_codes)] = strings().intern(name);
                                declarations()
                                    .packed_named_codes
                                    .value[declarations().packed_named_codes.index(
                                        new_packed_named_codes)] = *sentinel_value;
                                declarations()
                                    .packed_named_codes
                                    .sentinel[declarations().packed_named_codes.index(
                                        new_packed_named_codes)] = true;
                                return new_packed_named_codes;
                            }());
                            return declarations().range(rows);
                        }();
                        return row_0;
                    }();
                    return declarations().range(rows_1);
                }();
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
                    analysis_session_.inputs.selection.field = selected_segment_name;
                    selected_packed_code_ = std::move(name);
                    ImGui::EndDisabled();
                    return true;
                }
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            auto const duplicate_value{
                current_code_index.has_value()
                    ? first_available_code(
                          declarations(),
                          *selected_schema_field,
                          effective_width,
                          declarations().packed_named_codes.sentinel
                              [declarations().packed_named_codes.index(declarations().items(
                                  declarations()
                                      .packed_fields.named_codes[declarations().packed_fields.index(
                                          *selected_schema_field)])[*current_code_index])])
                    : std::nullopt};
            ImGui::BeginDisabled(!current_code_index.has_value() || !duplicate_value.has_value());
            if (ImGui::Button("Duplicate code")) {
                auto replacement{document_->storage().declarations.packed_values.clone(*schema)};

                auto copy{declarations().packed_named_codes.clone(declarations().items(
                    declarations().packed_fields.named_codes[declarations().packed_fields.index(
                        declarations().items(
                            document_->storage().declarations.packed_values.segments
                                [document_->storage().declarations.packed_values.index(
                                    replacement)])[*selected_index])])[*current_code_index])};
                declarations()
                    .packed_named_codes.name[declarations().packed_named_codes.index(copy)] =
                    strings().intern(unique_child_name(
                        declarations(),
                        declarations().packed_named_codes,
                        declarations().packed_named_codes.name,
                        declarations().items(
                            declarations()
                                .packed_fields.named_codes[declarations().packed_fields.index(
                                    declarations().items(
                                        document_->storage().declarations.packed_values.segments
                                            [document_->storage().declarations.packed_values.index(
                                                replacement)])[*selected_index])]),
                        text(declarations()
                                 .packed_named_codes
                                 .name[declarations().packed_named_codes.index(copy)]) +
                            "_copy"));
                declarations()
                    .packed_named_codes.value[declarations().packed_named_codes.index(copy)] =
                    *duplicate_value;
                document_->storage().declarations.packed_values.segments
                    [document_->storage().declarations.packed_values.index(replacement)] = [&] {
                    auto const original_items{declarations().items(
                        document_->storage().declarations.packed_values.segments
                            [document_->storage().declarations.packed_values.index(replacement)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[*selected_index] = [&] {
                        auto row_0{declarations().packed_fields.clone(rows_1[*selected_index])};
                        declarations().packed_fields.named_codes[declarations().packed_fields.index(
                            row_0)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_fields
                                    .named_codes[declarations().packed_fields.index(row_0)])};
                            auto rows{std::vector(original_items.begin(), original_items.end())};
                            rows.insert(rows.begin() +
                                            static_cast<std::ptrdiff_t>(*current_code_index + 1),
                                        copy);
                            return declarations().range(rows);
                        }();
                        return row_0;
                    }();
                    return declarations().range(rows_1);
                }();
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
                    analysis_session_.inputs.selection.field = selected_segment_name;
                    selected_packed_code_ =
                        text(declarations()
                                 .packed_named_codes
                                 .name[declarations().packed_named_codes.index(copy)]);
                    ImGui::EndDisabled();
                    return true;
                }
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(!current_code_index.has_value() || *current_code_index == 0);
            if (ImGui::Button("Code up")) {
                auto const code_name{text(
                    declarations().packed_named_codes.name[declarations().packed_named_codes.index(
                        declarations().items(
                            declarations()
                                .packed_fields.named_codes[declarations().packed_fields.index(
                                    *selected_schema_field)])[*current_code_index])])};
                auto replacement{document_->storage().declarations.packed_values.clone(*schema)};

                document_->storage().declarations.packed_values.segments
                    [document_->storage().declarations.packed_values.index(replacement)] = [&] {
                    auto const original_items{declarations().items(
                        document_->storage().declarations.packed_values.segments
                            [document_->storage().declarations.packed_values.index(replacement)])};
                    auto segments{std::vector(original_items.begin(), original_items.end())};
                    segments[*selected_index] = [&] {
                        auto field{declarations().packed_fields.clone(segments[*selected_index])};
                        declarations().packed_fields.named_codes[declarations().packed_fields.index(
                            field)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_fields
                                    .named_codes[declarations().packed_fields.index(field)])};
                            auto codes{std::vector(original_items.begin(), original_items.end())};
                            std::swap(codes[*current_code_index], codes[*current_code_index - 1]);
                            return declarations().range(codes);
                        }();
                        return field;
                    }();
                    return declarations().range(segments);
                }();
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
                    analysis_session_.inputs.selection.field = selected_segment_name;
                    selected_packed_code_ = code_name;
                    ImGui::EndDisabled();
                    return true;
                }
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(
                !current_code_index.has_value() ||
                *current_code_index + 1 >=
                    declarations()
                        .items(declarations()
                                   .packed_fields.named_codes[declarations().packed_fields.index(
                                       *selected_schema_field)])
                        .size());
            if (ImGui::Button("Code down")) {
                auto const code_name{text(
                    declarations().packed_named_codes.name[declarations().packed_named_codes.index(
                        declarations().items(
                            declarations()
                                .packed_fields.named_codes[declarations().packed_fields.index(
                                    *selected_schema_field)])[*current_code_index])])};
                auto replacement{document_->storage().declarations.packed_values.clone(*schema)};

                document_->storage().declarations.packed_values.segments
                    [document_->storage().declarations.packed_values.index(replacement)] = [&] {
                    auto const original_items{declarations().items(
                        document_->storage().declarations.packed_values.segments
                            [document_->storage().declarations.packed_values.index(replacement)])};
                    auto segments{std::vector(original_items.begin(), original_items.end())};
                    segments[*selected_index] = [&] {
                        auto field{declarations().packed_fields.clone(segments[*selected_index])};
                        declarations().packed_fields.named_codes[declarations().packed_fields.index(
                            field)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_fields
                                    .named_codes[declarations().packed_fields.index(field)])};
                            auto codes{std::vector(original_items.begin(), original_items.end())};
                            std::swap(codes[*current_code_index], codes[*current_code_index + 1]);
                            return declarations().range(codes);
                        }();
                        return field;
                    }();
                    return declarations().range(segments);
                }();
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
                    analysis_session_.inputs.selection.field = selected_segment_name;
                    selected_packed_code_ = code_name;
                    ImGui::EndDisabled();
                    return true;
                }
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(!current_code_index.has_value());
            if (ImGui::Button("Delete code")) {
                auto replacement{document_->storage().declarations.packed_values.clone(*schema)};

                document_->storage().declarations.packed_values.segments
                    [document_->storage().declarations.packed_values.index(replacement)] = [&] {
                    auto const original_items{declarations().items(
                        document_->storage().declarations.packed_values.segments
                            [document_->storage().declarations.packed_values.index(replacement)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[*selected_index] = [&] {
                        auto row_0{declarations().packed_fields.clone(rows_1[*selected_index])};
                        declarations().packed_fields.named_codes[declarations().packed_fields.index(
                            row_0)] = [&] {
                            auto const original_items{declarations().items(
                                declarations()
                                    .packed_fields
                                    .named_codes[declarations().packed_fields.index(row_0)])};
                            auto rows{std::vector(original_items.begin(), original_items.end())};
                            rows.erase(rows.begin() +
                                       static_cast<std::ptrdiff_t>(*current_code_index));
                            return declarations().range(rows);
                        }();
                        return row_0;
                    }();
                    return declarations().range(rows_1);
                }();
                auto const next_code{
                    declarations()
                            .items(
                                declarations().packed_fields.named_codes
                                    [declarations().packed_fields.index(declarations().items(
                                        document_->storage().declarations.packed_values.segments
                                            [document_->storage().declarations.packed_values.index(
                                                replacement)])[*selected_index])])
                            .empty()
                        ? std::string{}
                        : text(
                              declarations()
                                  .packed_named_codes.name[declarations().packed_named_codes.index(
                                      declarations().items(
                                          declarations().packed_fields.named_codes
                                              [declarations().packed_fields.index(
                                                  declarations().items(
                                                      document_->storage()
                                                          .declarations.packed_values
                                                          .segments[document_->storage()
                                                                        .declarations.packed_values
                                                                        .index(replacement)])
                                                      [*selected_index])])
                                          [std::min(
                                              *current_code_index,
                                              declarations()
                                                      .items(
                                                          declarations().packed_fields.named_codes
                                                              [declarations().packed_fields.index(
                                                                  declarations().items(
                                                                      document_->storage()
                                                                          .declarations
                                                                          .packed_values.segments
                                                                              [document_->storage()
                                                                                   .declarations
                                                                                   .packed_values
                                                                                   .index(
                                                                                       replacement)])
                                                                      [*selected_index])])
                                                      .size() -
                                                  1)])])};
                if (apply_document_edit(
                        document_->replace_declaration(*declaration, std::move(replacement)))) {
                    analysis_session_.inputs.selection.field = selected_segment_name;
                    selected_packed_code_ = next_code;
                    ImGui::EndDisabled();
                    return true;
                }
            }
            ImGui::EndDisabled();

            if (!declarations()
                     .packed_fields
                     .minimum_value[declarations().packed_fields.index(*selected_schema_field)]
                     .has_value()) {
                ImGui::TextDisabled("Set a semantic range before adding sentinel codes.");
            } else if (!sentinel_value.has_value()) {
                ImGui::TextDisabled("No unused code outside the live range fits this field width.");
            }

            if (detail::begin_editable_table(
                    "packed-named-codes",
                    4,
                    declarations()
                        .items(declarations()
                                   .packed_fields.named_codes[declarations().packed_fields.index(
                                       *selected_schema_field)])
                        .size())) {
                detail::editable_table_column("", ImGuiTableColumnFlags_WidthFixed);
                detail::editable_table_column("Name");
                detail::editable_table_column("Value");
                detail::editable_table_column("Sentinel", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableHeadersRow();
                for (std::size_t code_index{};
                     code_index <
                     declarations()
                         .items(declarations()
                                    .packed_fields.named_codes[declarations().packed_fields.index(
                                        *selected_schema_field)])
                         .size();
                     ++code_index) {
                    auto const code{declarations().items(
                        declarations().packed_fields.named_codes[declarations().packed_fields.index(
                            *selected_schema_field)])[code_index]};
                    auto const row_selected{
                        selected_packed_code_ ==
                        text(declarations()
                                 .packed_named_codes
                                 .name[declarations().packed_named_codes.index(code)])};
                    ImGui::PushID(static_cast<int>(code_index));
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    if (detail::editable_table_row_handle(row_selected)) {
                        selected_packed_code_ =
                            text(declarations()
                                     .packed_named_codes
                                     .name[declarations().packed_named_codes.index(code)]);
                        packed_code_editor_declaration_.reset();
                    }
                    if (ImGui::BeginDragDropSource()) {
                        ImGui::SetDragDropPayload(
                            "PACKED_CODE_ROW", &code_index, sizeof(code_index));
                        ImGui::Text("Move %s",
                                    text(declarations()
                                             .packed_named_codes
                                             .name[declarations().packed_named_codes.index(code)])
                                        .c_str());
                        ImGui::EndDragDropSource();
                    }
                    if (ImGui::BeginDragDropTarget()) {
                        if (auto const* payload{ImGui::AcceptDragDropPayload("PACKED_CODE_ROW")}) {
                            auto const source_index{
                                *static_cast<std::size_t const*>(payload->Data)};
                            if (source_index <
                                    declarations()
                                        .items(declarations()
                                                   .packed_fields
                                                   .named_codes[declarations().packed_fields.index(
                                                       *selected_schema_field)])
                                        .size() &&
                                source_index != code_index) {
                                pending = declarations().packed_values.clone(*schema);

                                selected_code_after_edit = text(
                                    declarations()
                                        .packed_named_codes
                                        .name[declarations().packed_named_codes.index(
                                            declarations().items(
                                                declarations().packed_fields.named_codes
                                                    [declarations().packed_fields.index(
                                                        declarations().items(
                                                            declarations().packed_values.segments
                                                                [declarations().packed_values.index(
                                                                    *pending)])[*selected_index])])
                                                [source_index])]);
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)] = [&] {
                                    auto const original_items{declarations().items(
                                        declarations().packed_values.segments
                                            [declarations().packed_values.index(*pending)])};
                                    auto segments{
                                        std::vector(original_items.begin(), original_items.end())};
                                    segments[*selected_index] = [&] {
                                        auto field{declarations().packed_fields.clone(
                                            segments[*selected_index])};
                                        declarations().packed_fields.named_codes
                                            [declarations().packed_fields.index(field)] = [&] {
                                            auto const original_items{declarations().items(
                                                declarations().packed_fields.named_codes
                                                    [declarations().packed_fields.index(field)])};
                                            auto codes{std::vector(original_items.begin(),
                                                                   original_items.end())};
                                            move_element(codes, source_index, code_index);
                                            return declarations().range(codes);
                                        }();
                                        return field;
                                    }();
                                    return declarations().range(segments);
                                }();
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }

                    ImGui::TableNextColumn();
                    if (row_selected) {
                        ImGui::SetNextItemWidth(-1.0F);
                        auto const submitted{
                            ImGui::InputText("##code-name",
                                             packed_code_name_.data(),
                                             packed_code_name_.size(),
                                             ImGuiInputTextFlags_EnterReturnsTrue)};
                        if (!pending.has_value() &&
                            (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                            pending = declarations().packed_values.clone(*schema);

                            declarations()
                                .packed_values
                                .segments[declarations().packed_values.index(*pending)] = [&] {
                                auto const original_items{declarations().items(
                                    declarations()
                                        .packed_values
                                        .segments[declarations().packed_values.index(*pending)])};
                                auto rows_3{
                                    std::vector(original_items.begin(), original_items.end())};
                                rows_3[*selected_index] = [&] {
                                    auto row_2{declarations().packed_fields.clone(
                                        rows_3[*selected_index])};
                                    declarations()
                                        .packed_fields
                                        .named_codes[declarations().packed_fields.index(row_2)] =
                                        [&] {
                                            auto const original_items{declarations().items(
                                                declarations().packed_fields.named_codes
                                                    [declarations().packed_fields.index(row_2)])};
                                            auto rows_1{std::vector(original_items.begin(),
                                                                    original_items.end())};
                                            rows_1[code_index] = [&] {
                                                auto row_0{declarations().packed_named_codes.clone(
                                                    rows_1[code_index])};
                                                declarations()
                                                    .packed_named_codes
                                                    .name[declarations().packed_named_codes.index(
                                                        row_0)] =
                                                    strings().intern(packed_code_name_.data());
                                                return row_0;
                                            }();
                                            return declarations().range(rows_1);
                                        }();
                                    return row_2;
                                }();
                                return declarations().range(rows_3);
                            }();
                            selected_code_after_edit = text(
                                declarations().packed_named_codes.name
                                    [declarations().packed_named_codes.index(declarations().items(
                                        declarations().packed_fields.named_codes
                                            [declarations().packed_fields.index(
                                                declarations().items(
                                                    declarations().packed_values.segments
                                                        [declarations().packed_values.index(
                                                            *pending)])[*selected_index])])
                                                                                 [code_index])]);
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
                        auto const submitted{
                            ImGui::InputText("##code-value",
                                             packed_code_value_.data(),
                                             packed_code_value_.size(),
                                             ImGuiInputTextFlags_EnterReturnsTrue)};
                        if (!pending.has_value() &&
                            (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                            if (auto const value{
                                    detail::parse_packed_integer(packed_code_value_.data())}) {
                                pending = declarations().packed_values.clone(*schema);
                                declarations()
                                    .packed_values
                                    .segments[declarations().packed_values.index(*pending)] = [&] {
                                    auto const original_items{declarations().items(
                                        declarations().packed_values.segments
                                            [declarations().packed_values.index(*pending)])};
                                    auto rows_3{
                                        std::vector(original_items.begin(), original_items.end())};
                                    rows_3[*selected_index] = [&] {
                                        auto row_2{declarations().packed_fields.clone(
                                            rows_3[*selected_index])};
                                        declarations().packed_fields.named_codes
                                            [declarations().packed_fields.index(row_2)] = [&] {
                                            auto const original_items{declarations().items(
                                                declarations().packed_fields.named_codes
                                                    [declarations().packed_fields.index(row_2)])};
                                            auto rows_1{std::vector(original_items.begin(),
                                                                    original_items.end())};
                                            rows_1[code_index] = [&] {
                                                auto row_0{declarations().packed_named_codes.clone(
                                                    rows_1[code_index])};
                                                declarations()
                                                    .packed_named_codes
                                                    .value[declarations().packed_named_codes.index(
                                                        row_0)] = *value;
                                                return row_0;
                                            }();
                                            return declarations().range(rows_1);
                                        }();
                                        return row_2;
                                    }();
                                    return declarations().range(rows_3);
                                }();
                            } else {
                                schema_edit_message_ = "Named code value must be a signed decimal "
                                                       "or hexadecimal integer.";
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
                        auto sentinel{packed_code_sentinel_};
                        if (ImGui::Checkbox("##code-sentinel", &sentinel)) {
                            pending = declarations().packed_values.clone(*schema);
                            declarations()
                                .packed_values
                                .segments[declarations().packed_values.index(*pending)] = [&] {
                                auto const original_items{declarations().items(
                                    declarations()
                                        .packed_values
                                        .segments[declarations().packed_values.index(*pending)])};
                                auto rows_3{
                                    std::vector(original_items.begin(), original_items.end())};
                                rows_3[*selected_index] = [&] {
                                    auto row_2{declarations().packed_fields.clone(
                                        rows_3[*selected_index])};
                                    declarations().packed_fields.named_codes
                                        [declarations().packed_fields.index(row_2)] = [&] {
                                        auto const original_items{declarations().items(
                                            declarations().packed_fields.named_codes
                                                [declarations().packed_fields.index(row_2)])};
                                        auto rows_1{std::vector(original_items.begin(),
                                                                original_items.end())};
                                        rows_1[code_index] = [&] {
                                            auto row_0{declarations().packed_named_codes.clone(
                                                rows_1[code_index])};
                                            declarations().packed_named_codes.sentinel
                                                [declarations().packed_named_codes.index(row_0)] =
                                                sentinel;
                                            return row_0;
                                        }();
                                        return declarations().range(rows_1);
                                    }();
                                    return row_2;
                                }();
                                return declarations().range(rows_3);
                            }();
                        }
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
        }
    }

    if (navigate_to.has_value()) {
        select_type(*navigate_to);
        analysis_session_.inputs.selection.field.clear();
        analysis_session_.inputs.selection.packed_access_fields.clear();
        analysis_session_.inputs.selection.packed_access_set_explicit = false;
        analysis_session_.inputs.selection.record_access_members.clear();
        analysis_session_.inputs.selection.record_access_set_explicit = false;
        return true;
    }

    if (pending.has_value()) {
        auto const segment{declarations().items(
            declarations().packed_values.segments[declarations().packed_values.index(
                *pending)])[*selected_index]};
        if (packed_name(segment).empty()) {
            schema_edit_message_ = "Packed segment name cannot be empty.";
            return false;
        }
        if (auto const field{((segment).kind() == lispb::codegen::PackedSegmentKind::field
                                  ? std::optional{segment}
                                  : std::nullopt)};
            field.has_value() &&
            text(
                declarations().type_refs.name[declarations().type_refs.index(
                    declarations().packed_fields.type[declarations().packed_fields.index(*field)])])
                .empty()) {
            schema_edit_message_ = "Packed field semantic type cannot be empty.";
            return false;
        }
        if (apply_document_edit(
                document_->replace_declaration(*declaration, std::move(*pending)))) {
            if (analysis_session_.inputs.selection.packed_access_set_explicit &&
                renamed_field.has_value()) {
                auto const existing{analysis_session_.inputs.selection.packed_access_fields.find(
                    renamed_field->first)};
                if (existing != analysis_session_.inputs.selection.packed_access_fields.end()) {
                    auto const operation{existing->second};
                    analysis_session_.inputs.selection.packed_access_fields.erase(existing);
                    analysis_session_.inputs.selection.packed_access_fields.insert_or_assign(
                        renamed_field->second, operation);
                }
            }
            analysis_session_.inputs.selection.field = std::move(selected_after_edit);
            selected_packed_code_ = std::move(selected_code_after_edit);
            return true;
        }
    }
    return false;
}

} // namespace ioj::layout_planner
