#include "planner_ui_properties_common.hpp"

namespace ioj::layout_planner {

auto PlannerUi::draw_packed_editor(TypeNode const& node, PackedType const& packed) -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const declaration{document_->find_declaration(node.identity)};
    if (!declaration.has_value()) {
        return false;
    }
    auto const* schema{document_->packed_value_schema(*declaration)};
    if (schema == nullptr) {
        return false;
    }

    if (analysis_session_.inputs.selection.field.empty() && !schema->segments.empty()) {
        analysis_session_.inputs.selection.field =
            codegen::packed_segment_name(schema->segments.front());
    }
    auto selected{std::ranges::find_if(schema->segments, [&](auto const& segment) {
        return codegen::packed_segment_name(segment) == analysis_session_.inputs.selection.field;
    })};
    if (selected == schema->segments.end() && !schema->segments.empty()) {
        selected = schema->segments.begin();
        analysis_session_.inputs.selection.field = codegen::packed_segment_name(*selected);
    }
    auto const selected_index{selected == schema->segments.end()
                                  ? std::optional<std::size_t>{}
                                  : std::optional<std::size_t>{static_cast<std::size_t>(
                                        selected - schema->segments.begin())}};

    if (packed_editor_declaration_ != declaration ||
        packed_editor_field_ != analysis_session_.inputs.selection.field) {
        packed_editor_declaration_ = declaration;
        packed_editor_field_ = analysis_session_.inputs.selection.field;
        std::snprintf(packed_storage_type_.data(),
                      packed_storage_type_.size(),
                      "%s",
                      schema->storage_type.name.c_str());
        std::snprintf(packed_invalid_value_.data(),
                      packed_invalid_value_.size(),
                      "%s",
                      schema->invalid_value.has_value()
                          ? std::to_string(*schema->invalid_value).c_str()
                          : "");
        std::snprintf(packed_export_specifier_.data(),
                      packed_export_specifier_.size(),
                      "%s",
                      schema->export_specifier.value_or("").c_str());
        if (selected != schema->segments.end()) {
            auto const& selected_name{codegen::packed_segment_name(*selected)};
            std::snprintf(
                packed_field_name_.data(), packed_field_name_.size(), "%s", selected_name.c_str());
            if (auto const* field{std::get_if<codegen::PackedFieldSchema>(&*selected)}) {
                std::snprintf(packed_field_type_.data(),
                              packed_field_type_.size(),
                              "%s",
                              field->type.name.c_str());
                std::snprintf(packed_field_minimum_.data(),
                              packed_field_minimum_.size(),
                              "%s",
                              field->minimum_value.has_value()
                                  ? codegen::format_packed_integer(*field->minimum_value).c_str()
                                  : "");
                std::snprintf(packed_field_maximum_.data(),
                              packed_field_maximum_.size(),
                              "%s",
                              field->maximum_value.has_value()
                                  ? codegen::format_packed_integer(*field->maximum_value).c_str()
                                  : "");
                std::snprintf(packed_relationship_target_.data(),
                              packed_relationship_target_.size(),
                              "%s",
                              field->relationship.has_value()
                                  ? field->relationship->target.name.c_str()
                                  : "");
                auto const relationship_kind{
                    field->relationship.has_value()
                        ? std::ranges::find(semantic_relationship_kinds, field->relationship->kind)
                        : semantic_relationship_kinds.end()};
                packed_relationship_kind_ =
                    relationship_kind == semantic_relationship_kinds.end()
                        ? 0
                        : static_cast<int>(relationship_kind - semantic_relationship_kinds.begin());
                auto const relationship_unit{
                    field->relationship.has_value() && field->relationship->unit.has_value()
                        ? std::ranges::find(semantic_relationship_units, *field->relationship->unit)
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
                selected_index.has_value() && *selected_index < packed.segments.size()
                    ? std::visit([](auto const& value) { return value.bit_width; },
                                 packed.segments[*selected_index])
                    : std::uint32_t{1}};
            packed_field_bits_ = std::get_if<codegen::PackedFieldSchema>(&*selected) != nullptr
                                   ? std::get<codegen::PackedFieldSchema>(*selected).bits.value_or(
                                         static_cast<int>(resolved_width))
                                   : static_cast<int>(resolved_width);
        }
    }

    auto const* selected_schema_field{selected == schema->segments.end()
                                          ? nullptr
                                          : std::get_if<codegen::PackedFieldSchema>(&*selected)};
    auto const* selected_resolved_field{
        selected_index.has_value() && *selected_index < packed.segments.size()
            ? std::get_if<lispb::schema::PackedField>(&packed.segments[*selected_index])
            : nullptr};
    auto const* selected_integer_scalar{
        selected_resolved_field != nullptr
            ? std::get_if<IntegerScalarType>(&analysis_session_.inputs.workspace.types()
                                                  .type(selected_resolved_field->semantic_type.type)
                                                  .definition)
            : nullptr};
    auto const* selected_linear_quantized{
        selected_resolved_field != nullptr
            ? std::get_if<LinearQuantizedType>(
                  &analysis_session_.inputs.workspace.types()
                       .type(selected_resolved_field->semantic_type.type)
                       .definition)
            : nullptr};
    auto const* selected_fixed_point{
        selected_resolved_field != nullptr
            ? std::get_if<FixedPointType>(&analysis_session_.inputs.workspace.types()
                                               .type(selected_resolved_field->semantic_type.type)
                                               .definition)
            : nullptr};
    auto const* selected_mini_float{
        selected_resolved_field != nullptr
            ? std::get_if<MiniFloatType>(&analysis_session_.inputs.workspace.types()
                                              .type(selected_resolved_field->semantic_type.type)
                                              .definition)
            : nullptr};
    if (selected_schema_field == nullptr) {
        selected_packed_code_.clear();
    } else if (std::ranges::find(selected_schema_field->named_codes,
                                 selected_packed_code_,
                                 &codegen::PackedNamedCodeSchema::name) ==
               selected_schema_field->named_codes.end()) {
        selected_packed_code_ = selected_schema_field->named_codes.empty()
                                  ? std::string{}
                                  : selected_schema_field->named_codes.front().name;
    }
    auto const selected_code{selected_schema_field == nullptr
                                 ? std::vector<codegen::PackedNamedCodeSchema>::const_iterator{}
                                 : std::ranges::find(selected_schema_field->named_codes,
                                                     selected_packed_code_,
                                                     &codegen::PackedNamedCodeSchema::name)};
    if (packed_code_editor_declaration_ != declaration ||
        packed_code_editor_field_ != analysis_session_.inputs.selection.field ||
        packed_code_editor_name_ != selected_packed_code_) {
        packed_code_editor_declaration_ = declaration;
        packed_code_editor_field_ = analysis_session_.inputs.selection.field;
        packed_code_editor_name_ = selected_packed_code_;
        if (selected_schema_field != nullptr &&
            selected_code != selected_schema_field->named_codes.end()) {
            std::snprintf(packed_code_name_.data(),
                          packed_code_name_.size(),
                          "%s",
                          selected_code->name.c_str());
            std::snprintf(packed_code_value_.data(),
                          packed_code_value_.size(),
                          "%s",
                          codegen::format_packed_integer(selected_code->value).c_str());
            packed_code_sentinel_ = selected_code->sentinel;
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
        auto const export_specifier{optional_text(packed_export_specifier_)};
        if (schema->export_specifier != export_specifier) {
            auto replacement{*schema};
            replacement.export_specifier = export_specifier;
            if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                       .schema = std::move(replacement)})) {
                analysis_session_.inputs.selection.field = packed_editor_field_;
                return true;
            }
        }
    }

    auto mutable_value{schema->mutable_value};
    if (ImGui::Checkbox("Generate mutable field API", &mutable_value)) {
        auto replacement{*schema};
        replacement.mutable_value = mutable_value;
        if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
            analysis_session_.inputs.selection.field = packed_editor_field_;
            return true;
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Generates fallible try_make/try_set and checked setter APIs; it does not change the "
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
            auto replacement{*schema};
            replacement.storage_type.name = packed_storage_type_.data();
            if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                       .schema = std::move(replacement)})) {
                analysis_session_.inputs.selection.field = packed_editor_field_;
                return true;
            }
        }
    }

    constexpr std::array byte_order_labels{"Unspecified", "Little endian", "Big endian"};
    auto byte_order_index{
        schema->byte_order.has_value()
            ? (*schema->byte_order == codegen::PackedByteOrder::little_endian ? 1 : 2)
            : 0};
    if (ImGui::Combo("Serialized byte order",
                     &byte_order_index,
                     byte_order_labels.data(),
                     static_cast<int>(byte_order_labels.size()))) {
        auto replacement{*schema};
        replacement.byte_order =
            byte_order_index == 0
                ? std::nullopt
                : std::optional{byte_order_index == 1 ? codegen::PackedByteOrder::little_endian
                                                      : codegen::PackedByteOrder::big_endian};
        if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
            analysis_session_.inputs.selection.field = packed_editor_field_;
            return true;
        }
    }

    constexpr std::array bit_order_labels{"Default (LSB-first)", "Explicit LSB-first", "MSB-first"};
    auto bit_order_index{
        schema->bit_order.has_value()
            ? (*schema->bit_order == codegen::PackedBitOrder::least_significant_first ? 1 : 2)
            : 0};
    if (ImGui::Combo("Segment bit order",
                     &bit_order_index,
                     bit_order_labels.data(),
                     static_cast<int>(bit_order_labels.size()))) {
        auto replacement{*schema};
        replacement.bit_order =
            bit_order_index == 0
                ? std::nullopt
                : std::optional{bit_order_index == 1
                                    ? codegen::PackedBitOrder::least_significant_first
                                    : codegen::PackedBitOrder::most_significant_first};
        if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
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
        auto replacement{*schema};
        if (packed_invalid_value_.front() == '\0') {
            replacement.invalid_value.reset();
        } else if (auto const value{detail::parse_unsigned(packed_invalid_value_.data())}) {
            replacement.invalid_value = *value;
        } else {
            schema_edit_message_ = "Invalid raw value must be an unsigned decimal or 0x value.";
            return false;
        }
        if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
            analysis_session_.inputs.selection.field = packed_editor_field_;
            return true;
        }
    }
    ImGui::TextDisabled("Leave invalid raw value empty when every storage code is valid.");

    if (ImGui::Button("+ Field")) {
        auto replacement{*schema};
        auto name{unique_segment_name(replacement.segments, "field")};
        replacement.segments.emplace_back(codegen::PackedFieldSchema{
            .name = name,
            .type = codegen::TypeRef{.name = "std::uint8_t", .suffix = {}, .nested = std::nullopt},
            .bits = 1,
            .kind = codegen::PackedFieldKind::unsigned_integer,
            .range_helper = false,
            .minimum_value = std::nullopt,
            .maximum_value = std::nullopt,
            .named_codes = {},
            .relationship = std::nullopt});
        if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
            analysis_session_.inputs.selection.field = std::move(name);
            return true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Reserved")) {
        auto replacement{*schema};
        auto name{unique_segment_name(replacement.segments, "reserved")};
        replacement.segments.emplace_back(
            codegen::PackedReservedBitsSchema{.name = name, .bits = 1});
        if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
            analysis_session_.inputs.selection.field = std::move(name);
            return true;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value());
    if (ImGui::Button("Duplicate")) {
        auto replacement{*schema};
        auto copy{replacement.segments[*selected_index]};
        auto const name{unique_segment_name(replacement.segments,
                                            codegen::packed_segment_name(copy) + "_copy")};
        std::visit([&](auto& value) { value.name = name; }, copy);
        replacement.segments.insert(replacement.segments.begin() +
                                        static_cast<std::ptrdiff_t>(*selected_index + 1),
                                    std::move(copy));
        if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
            analysis_session_.inputs.selection.field = name;
            ImGui::EndDisabled();
            return true;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() || *selected_index == 0);
    if (ImGui::Button("Move up")) {
        auto replacement{*schema};
        std::swap(replacement.segments[*selected_index], replacement.segments[*selected_index - 1]);
        if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
            analysis_session_.inputs.selection.field = packed_editor_field_;
            ImGui::EndDisabled();
            ImGui::EndDisabled();
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() ||
                         *selected_index + 1 >= schema->segments.size());
    if (ImGui::Button("Move down")) {
        auto replacement{*schema};
        std::swap(replacement.segments[*selected_index], replacement.segments[*selected_index + 1]);
        if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
            analysis_session_.inputs.selection.field = packed_editor_field_;
            ImGui::EndDisabled();
            ImGui::EndDisabled();
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() || schema->segments.size() == 1);
    if (ImGui::Button("Delete")) {
        auto replacement{*schema};
        auto const deleted_name{
            codegen::packed_segment_name(replacement.segments[*selected_index])};
        replacement.segments.erase(replacement.segments.begin() +
                                   static_cast<std::ptrdiff_t>(*selected_index));
        auto const next_index{std::min(*selected_index, replacement.segments.size() - 1)};
        auto const next_name{codegen::packed_segment_name(replacement.segments[next_index])};
        if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                   .schema = std::move(replacement)})) {
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
        for (auto const& segment : schema->segments) {
            if (auto const* field{std::get_if<codegen::PackedFieldSchema>(&segment)}) {
                analysis_session_.inputs.selection.packed_access_fields.insert_or_assign(
                    field->name, analysis_session_.inputs.access_operation);
            }
        }
        analysis_session_.inputs.selection.packed_access_set_explicit = true;
    }

    std::optional<codegen::PackedValueSchema> pending;
    std::optional<TypeId> navigate_to;
    std::optional<std::pair<std::string, std::string>> renamed_field;
    auto selected_after_edit{analysis_session_.inputs.selection.field};
    if (detail::begin_editable_table("packed-schema-fields", 11, schema->segments.size())) {
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
        ImGui::TableHeadersRow();
        for (std::size_t index{}; index < schema->segments.size(); ++index) {
            auto const& segment{schema->segments[index]};
            auto const* field{std::get_if<codegen::PackedFieldSchema>(&segment)};
            auto const* resolved_field{
                index < packed.segments.size()
                    ? std::get_if<lispb::schema::PackedField>(&packed.segments[index])
                    : nullptr};
            auto const field_uses_integer_scalar{resolved_field != nullptr &&
                                                 std::holds_alternative<IntegerScalarType>(
                                                     analysis_session_.inputs.workspace.types()
                                                         .type(resolved_field->semantic_type.type)
                                                         .definition)};
            auto const* field_linear_quantized{
                resolved_field != nullptr ? std::get_if<LinearQuantizedType>(
                                                &analysis_session_.inputs.workspace.types()
                                                     .type(resolved_field->semantic_type.type)
                                                     .definition)
                                          : nullptr};
            auto const* field_fixed_point{
                resolved_field != nullptr
                    ? std::get_if<FixedPointType>(&analysis_session_.inputs.workspace.types()
                                                       .type(resolved_field->semantic_type.type)
                                                       .definition)
                    : nullptr};
            auto const* field_mini_float{
                resolved_field != nullptr
                    ? std::get_if<MiniFloatType>(&analysis_session_.inputs.workspace.types()
                                                      .type(resolved_field->semantic_type.type)
                                                      .definition)
                    : nullptr};
            layout::LinearQuantizedAnalysis const* field_quantization_analysis{};
            layout::FixedPointAnalysis const* field_fixed_point_analysis{};
            layout::MiniFloatAnalysis const* field_mini_float_analysis{};
            if (field != nullptr && analysis_session_.results().active_packed.has_value()) {
                auto const analyzed_field{
                    std::ranges::find(analysis_session_.results().active_packed->fields,
                                      field->name,
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
            auto const& segment_name{codegen::packed_segment_name(segment)};
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
                    if (source_index < schema->segments.size() && source_index != index) {
                        pending = *schema;
                        selected_after_edit =
                            codegen::packed_segment_name(pending->segments[source_index]);
                        move_element(pending->segments, source_index, index);
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::TableNextColumn();
            if (field == nullptr) {
                ImGui::TextDisabled("-");
            } else {
                auto accessed{
                    analysis_session_.inputs.selection.packed_access_set_explicit
                        ? analysis_session_.inputs.selection.packed_access_fields.contains(
                              field->name)
                        : analysis_session_.inputs.selection.field == field->name};
                if (ImGui::Checkbox("##access", &accessed)) {
                    if (!analysis_session_.inputs.selection.packed_access_set_explicit) {
                        analysis_session_.inputs.selection.packed_access_fields.clear();
                        for (auto const& selected_segment : schema->segments) {
                            auto const* selected_schema_field{
                                std::get_if<codegen::PackedFieldSchema>(&selected_segment)};
                            if (selected_schema_field != nullptr &&
                                selected_schema_field->name ==
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
                            field->name, analysis_session_.inputs.access_operation);
                    } else {
                        analysis_session_.inputs.selection.packed_access_fields.erase(field->name);
                    }
                }
            }

            ImGui::TableNextColumn();
            auto const accessed{
                field != nullptr &&
                (analysis_session_.inputs.selection.packed_access_set_explicit
                     ? analysis_session_.inputs.selection.packed_access_fields.contains(field->name)
                     : analysis_session_.inputs.selection.field == field->name)};
            if (accessed) {
                auto operation{analysis_session_.inputs.access_operation};
                if (analysis_session_.inputs.selection.packed_access_set_explicit) {
                    if (auto const found{
                            analysis_session_.inputs.selection.packed_access_fields.find(
                                field->name)};
                        found != analysis_session_.inputs.selection.packed_access_fields.end()) {
                        operation = found->second;
                    }
                }
                auto operation_index{static_cast<int>(operation)};
                ImGui::SetNextItemWidth(105.0F);
                if (ImGui::Combo(
                        "##access-operation", &operation_index, "Read\0Write\0Read + write\0")) {
                    if (!analysis_session_.inputs.selection.packed_access_set_explicit) {
                        analysis_session_.inputs.selection.packed_access_fields.clear();
                        analysis_session_.inputs.selection.packed_access_set_explicit = true;
                    }
                    analysis_session_.inputs.selection.packed_access_fields.insert_or_assign(
                        field->name, static_cast<AccessOperation>(operation_index));
                }
            } else {
                ImGui::TextDisabled("-");
            }

            ImGui::TableNextColumn();
            if (row_selected) {
                ImGui::SetNextItemWidth(-1.0F);
                auto const submitted{ImGui::InputText("##name",
                                                      packed_field_name_.data(),
                                                      packed_field_name_.size(),
                                                      ImGuiInputTextFlags_EnterReturnsTrue)};
                if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
                    pending = *schema;
                    std::visit([&](auto& value) { value.name = packed_field_name_.data(); },
                               pending->segments[index]);
                    selected_after_edit = packed_field_name_.data();
                    if (field != nullptr) {
                        renamed_field = std::pair{field->name, selected_after_edit};
                    }
                }
            } else {
                ImGui::TextUnformatted(segment_name.c_str());
            }

            ImGui::TableNextColumn();
            if (field == nullptr) {
                ImGui::TextDisabled("Reserved");
            } else if (row_selected) {
                auto const type_controls_inline{detail::prepare_editable_type_input()};
                auto const submitted{ImGui::InputText("##type",
                                                      packed_field_type_.data(),
                                                      packed_field_type_.size(),
                                                      ImGuiInputTextFlags_EnterReturnsTrue)};
                if (!pending.has_value() && (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                    pending = *schema;
                    std::get<codegen::PackedFieldSchema>(pending->segments[index]).type.name =
                        packed_field_type_.data();
                }
                if (type_controls_inline) {
                    ImGui::SameLine();
                }
                if (auto picked{draw_type_picker(node.identity.module_name, node.identity)}) {
                    pending = *schema;
                    auto& pending_field{
                        std::get<codegen::PackedFieldSchema>(pending->segments[index])};
                    pending_field.type =
                        codegen::TypeRef{.name = *picked, .suffix = {}, .nested = std::nullopt};
                    auto const picked_type{std::ranges::find_if(
                        analysis_session_.inputs.workspace.types().types(),
                        [&](TypeNode const& candidate) {
                            return candidate.identity.origin == TypeOrigin::declaration &&
                                   ((candidate.identity.module_name == node.identity.module_name &&
                                     candidate.identity.name == *picked) ||
                                    candidate.cpp_spelling == *picked);
                        })};
                    if (picked_type != analysis_session_.inputs.workspace.types().types().end() &&
                        std::holds_alternative<EnumType>(picked_type->definition)) {
                        pending_field.kind = codegen::PackedFieldKind::enumeration;
                        pending_field.range_helper = false;
                        pending_field.minimum_value.reset();
                        pending_field.maximum_value.reset();
                        pending_field.named_codes.clear();
                    } else if (picked_type !=
                                   analysis_session_.inputs.workspace.types().types().end() &&
                               std::holds_alternative<LinearQuantizedType>(
                                   picked_type->definition)) {
                        pending_field.kind = codegen::PackedFieldKind::linear_quantized;
                        pending_field.bits.reset();
                        pending_field.range_helper = false;
                        pending_field.minimum_value.reset();
                        pending_field.maximum_value.reset();
                        pending_field.named_codes.clear();
                        pending_field.relationship.reset();
                    } else if (picked_type !=
                                   analysis_session_.inputs.workspace.types().types().end() &&
                               std::holds_alternative<FixedPointType>(picked_type->definition)) {
                        pending_field.kind = codegen::PackedFieldKind::fixed_point;
                        pending_field.bits.reset();
                        pending_field.range_helper = false;
                        pending_field.minimum_value.reset();
                        pending_field.maximum_value.reset();
                        pending_field.named_codes.clear();
                        pending_field.relationship.reset();
                    } else if (picked_type !=
                                   analysis_session_.inputs.workspace.types().types().end() &&
                               std::holds_alternative<MiniFloatType>(picked_type->definition)) {
                        pending_field.kind = codegen::PackedFieldKind::mini_float;
                        pending_field.bits.reset();
                        pending_field.range_helper = false;
                        pending_field.minimum_value.reset();
                        pending_field.maximum_value.reset();
                        pending_field.named_codes.clear();
                        pending_field.relationship.reset();
                    } else if (picked_type !=
                               analysis_session_.inputs.workspace.types().types().end()) {
                        if (auto const* scalar{
                                std::get_if<IntegerScalarType>(&picked_type->definition)}) {
                            pending_field.kind = scalar->signedness
                                                   ? codegen::PackedFieldKind::signed_integer
                                                   : codegen::PackedFieldKind::unsigned_integer;
                            pending_field.range_helper = false;
                            pending_field.minimum_value.reset();
                            pending_field.maximum_value.reset();
                            pending_field.named_codes.clear();
                        }
                    }
                }
                ImGui::SameLine();
                if (index < packed.segments.size() && detail::semantic_type_navigation_button()) {
                    if (auto const* resolved{
                            std::get_if<lispb::schema::PackedField>(&packed.segments[index])}) {
                        navigate_to = resolved->semantic_type.type;
                    }
                }
                detail::editable_table_content_hint(packed_field_type_.data(), 80.0F);
            } else {
                ImGui::TextUnformatted(field->type.name.c_str());
            }

            ImGui::TableNextColumn();
            if (field != nullptr && !field->bits.has_value()) {
                auto const effective{
                    index < packed.segments.size()
                        ? std::get<lispb::schema::PackedField>(packed.segments[index]).bit_width
                        : 0U};
                ImGui::Text("=> %u", effective);
            } else if (row_selected) {
                ImGui::SetNextItemWidth(72.0F);
                auto const submitted{ImGui::InputInt(
                    "##bits", &packed_field_bits_, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue)};
                if (!pending.has_value() && (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                    pending = *schema;
                    std::visit([&](auto& value) { value.bits = packed_field_bits_; },
                               pending->segments[index]);
                }
            } else {
                ImGui::Text("%d", *codegen::packed_segment_bits(segment));
            }

            ImGui::TableNextColumn();
            if (field == nullptr) {
                ImGui::TextDisabled("-");
            } else if (row_selected) {
                auto width_auto{!field->bits.has_value()};
                if (ImGui::Checkbox("##auto-bits", &width_auto)) {
                    pending = *schema;
                    auto& pending_field{
                        std::get<codegen::PackedFieldSchema>(pending->segments[index])};
                    pending_field.bits =
                        width_auto ? std::nullopt : std::optional<int>{packed_field_bits_};
                }
            } else {
                ImGui::TextUnformatted(field->bits.has_value() ? "-" : "yes");
            }

            ImGui::TableNextColumn();
            auto const kind_label{
                field == nullptr                                            ? "reserved"
                : field->kind == codegen::PackedFieldKind::enumeration      ? "enum"
                : field->kind == codegen::PackedFieldKind::linear_quantized ? "linear quantized"
                : field->kind == codegen::PackedFieldKind::fixed_point      ? "fixed point"
                : field->kind == codegen::PackedFieldKind::mini_float       ? "mini float"
                : field->kind == codegen::PackedFieldKind::signed_integer   ? "signed"
                                                                            : "unsigned"};
            if (field == nullptr) {
                ImGui::TextDisabled("reserved");
            } else if (row_selected) {
                ImGui::BeginDisabled(field_uses_integer_scalar ||
                                     field_linear_quantized != nullptr ||
                                     field_fixed_point != nullptr || field_mini_float != nullptr);
                if (ImGui::BeginCombo("##kind", kind_label)) {
                    if (ImGui::Selectable("unsigned",
                                          field->kind ==
                                              codegen::PackedFieldKind::unsigned_integer)) {
                        pending = *schema;
                        auto& pending_field{
                            std::get<codegen::PackedFieldSchema>(pending->segments[index])};
                        pending_field.kind = codegen::PackedFieldKind::unsigned_integer;
                        if (auto const mapped{matching_unsigned_type(pending_field.type.name)}) {
                            pending_field.type.name = *mapped;
                        }
                        auto const has_negative_code{
                            std::ranges::any_of(pending_field.named_codes, [](auto const& code) {
                                return code.value.negative;
                            })};
                        if ((pending_field.minimum_value.has_value() &&
                             pending_field.minimum_value->negative) ||
                            has_negative_code) {
                            pending_field.minimum_value.reset();
                            pending_field.maximum_value.reset();
                            pending_field.named_codes.clear();
                            if (!pending_field.bits.has_value() && index < packed.segments.size()) {
                                pending_field.bits = static_cast<int>(
                                    std::get<lispb::schema::PackedField>(packed.segments[index])
                                        .bit_width);
                            }
                        }
                    }
                    if (ImGui::Selectable(
                            "signed", field->kind == codegen::PackedFieldKind::signed_integer)) {
                        pending = *schema;
                        auto& pending_field{
                            std::get<codegen::PackedFieldSchema>(pending->segments[index])};
                        pending_field.kind = codegen::PackedFieldKind::signed_integer;
                        if (auto const mapped{matching_signed_type(pending_field.type.name)}) {
                            pending_field.type.name = *mapped;
                        }
                        pending_field.range_helper = false;
                        auto const transition_width{
                            pending_field.bits.has_value()
                                ? static_cast<std::uint32_t>(*pending_field.bits)
                            : index < packed.segments.size()
                                ? std::get<lispb::schema::PackedField>(packed.segments[index])
                                      .bit_width
                                : std::uint32_t{1}};
                        if (pending_field.minimum_value.has_value() &&
                            (!codegen::packed_integer_fits_signed(*pending_field.minimum_value,
                                                                  transition_width) ||
                             !codegen::packed_integer_fits_signed(*pending_field.maximum_value,
                                                                  transition_width))) {
                            pending_field.minimum_value.reset();
                            pending_field.maximum_value.reset();
                            pending_field.named_codes.clear();
                            if (!pending_field.bits.has_value()) {
                                pending_field.bits = static_cast<int>(transition_width);
                            }
                        } else {
                            std::erase_if(pending_field.named_codes, [&](auto const& code) {
                                return !codegen::packed_integer_fits_signed(code.value,
                                                                            transition_width);
                            });
                        }
                    }
                    if (ImGui::Selectable("enum",
                                          field->kind == codegen::PackedFieldKind::enumeration)) {
                        pending = *schema;
                        auto& pending_field{
                            std::get<codegen::PackedFieldSchema>(pending->segments[index])};
                        pending_field.kind = codegen::PackedFieldKind::enumeration;
                        pending_field.range_helper = false;
                        pending_field.minimum_value.reset();
                        pending_field.maximum_value.reset();
                        pending_field.named_codes.clear();
                    }
                    if (ImGui::Selectable("linear quantized",
                                          field->kind ==
                                              codegen::PackedFieldKind::linear_quantized)) {
                        pending = *schema;
                        auto& pending_field{
                            std::get<codegen::PackedFieldSchema>(pending->segments[index])};
                        pending_field.kind = codegen::PackedFieldKind::linear_quantized;
                        pending_field.bits.reset();
                        pending_field.range_helper = false;
                        pending_field.minimum_value.reset();
                        pending_field.maximum_value.reset();
                        pending_field.named_codes.clear();
                        pending_field.relationship.reset();
                    }
                    if (ImGui::Selectable("fixed point",
                                          field->kind == codegen::PackedFieldKind::fixed_point)) {
                        pending = *schema;
                        auto& pending_field{
                            std::get<codegen::PackedFieldSchema>(pending->segments[index])};
                        pending_field.kind = codegen::PackedFieldKind::fixed_point;
                        pending_field.bits.reset();
                        pending_field.range_helper = false;
                        pending_field.minimum_value.reset();
                        pending_field.maximum_value.reset();
                        pending_field.named_codes.clear();
                        pending_field.relationship.reset();
                    }
                    if (ImGui::Selectable("mini float",
                                          field->kind == codegen::PackedFieldKind::mini_float)) {
                        pending = *schema;
                        auto& pending_field{
                            std::get<codegen::PackedFieldSchema>(pending->segments[index])};
                        pending_field.kind = codegen::PackedFieldKind::mini_float;
                        pending_field.bits.reset();
                        pending_field.range_helper = false;
                        pending_field.minimum_value.reset();
                        pending_field.maximum_value.reset();
                        pending_field.named_codes.clear();
                        pending_field.relationship.reset();
                    }
                    ImGui::EndCombo();
                }
                ImGui::EndDisabled();
            } else if (!row_selected) {
                ImGui::TextUnformatted(kind_label);
            }

            ImGui::TableNextColumn();
            auto range_helper{field != nullptr && field->range_helper};
            if (field == nullptr) {
                ImGui::TextDisabled("-");
            } else if (row_selected) {
                ImGui::BeginDisabled(field->kind != codegen::PackedFieldKind::unsigned_integer ||
                                     field_uses_integer_scalar ||
                                     field_linear_quantized != nullptr ||
                                     field_fixed_point != nullptr || field_mini_float != nullptr);
                if (ImGui::Checkbox("##range-helper", &range_helper)) {
                    pending = *schema;
                    std::get<codegen::PackedFieldSchema>(pending->segments[index]).range_helper =
                        range_helper;
                }
                ImGui::EndDisabled();
            } else {
                ImGui::TextUnformatted(range_helper ? "yes" : "-");
            }

            ImGui::TableNextColumn();
            if (field == nullptr) {
                ImGui::TextDisabled("-");
            } else if (field_uses_integer_scalar && resolved_field->minimum_value.has_value()) {
                auto const minimum{codegen::format_packed_integer(*resolved_field->minimum_value)};
                auto const maximum{codegen::format_packed_integer(*resolved_field->maximum_value)};
                ImGui::TextDisabled("%s..%s (shared)", minimum.c_str(), maximum.c_str());
            } else if (field_quantization_analysis != nullptr) {
                auto const minimum{
                    codegen::format_packed_integer(field_quantization_analysis->source_minimum)};
                auto const maximum{
                    codegen::format_packed_integer(field_quantization_analysis->source_maximum)};
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
                    pending = *schema;
                    auto& pending_field{
                        std::get<codegen::PackedFieldSchema>(pending->segments[index])};
                    if (minimum_text.empty() && maximum_text.empty()) {
                        pending_field.minimum_value.reset();
                        pending_field.maximum_value.reset();
                    } else if (auto const minimum{detail::parse_packed_integer(minimum_text)};
                               minimum.has_value()) {
                        if (auto const maximum{detail::parse_packed_integer(maximum_text)};
                            maximum.has_value()) {
                            pending_field.minimum_value = *minimum;
                            pending_field.maximum_value = *maximum;
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
            } else if (field->minimum_value.has_value()) {
                auto const minimum{codegen::format_packed_integer(*field->minimum_value)};
                auto const maximum{codegen::format_packed_integer(*field->maximum_value)};
                ImGui::Text("%s..%s", minimum.c_str(), maximum.c_str());
            } else {
                ImGui::TextDisabled("unconstrained");
            }

            ImGui::TableNextColumn();
            if (field == nullptr) {
                ImGui::TextDisabled("Reserved");
            } else if (field_quantization_analysis != nullptr) {
                auto const maximum{field_quantization_analysis->usable_code_count.two_to_64
                                       ? (std::numeric_limits<std::uint64_t>::max)()
                                       : field_quantization_analysis->usable_code_count.value - 1};
                ImGui::Text("encoded 0..%llu", static_cast<unsigned long long>(maximum));
            } else if (field_fixed_point_analysis != nullptr) {
                auto const minimum{
                    codegen::format_packed_integer(field_fixed_point_analysis->minimum_raw_value)};
                auto const maximum{
                    codegen::format_packed_integer(field_fixed_point_analysis->maximum_raw_value)};
                ImGui::Text("raw %s..%s", minimum.c_str(), maximum.c_str());
            } else if (field_mini_float_analysis != nullptr) {
                auto const maximum{
                    field_mini_float_analysis->total_bits == 64
                        ? (std::numeric_limits<std::uint64_t>::max)()
                        : (std::uint64_t{1} << field_mini_float_analysis->total_bits) - 1};
                ImGui::Text("encoded 0..%llu", static_cast<unsigned long long>(maximum));
            } else if (index < packed.segments.size()) {
                auto const* resolved{
                    std::get_if<lispb::schema::PackedField>(&packed.segments[index])};
                if (resolved != nullptr &&
                    resolved->kind == codegen::PackedFieldKind::signed_integer) {
                    auto const minimum{resolved->bit_width == 64
                                           ? std::numeric_limits<std::int64_t>::min()
                                           : -static_cast<std::int64_t>(
                                                 std::uint64_t{1} << (resolved->bit_width - 1))};
                    auto const maximum{
                        resolved->bit_width == 64
                            ? std::numeric_limits<std::int64_t>::max()
                            : static_cast<std::int64_t>(
                                  (std::uint64_t{1} << (resolved->bit_width - 1)) - 1)};
                    ImGui::Text("%lld..%lld",
                                static_cast<long long>(minimum),
                                static_cast<long long>(maximum));
                } else {
                    auto const maximum{resolved == nullptr ? std::optional<std::uint64_t>{}
                                       : resolved->bit_width >= 64
                                           ? std::optional<std::uint64_t>{std::numeric_limits<
                                                 std::uint64_t>::max()}
                                           : std::optional<std::uint64_t>{
                                                 (std::uint64_t{1} << resolved->bit_width) - 1}};
                    ImGui::Text("0..%s", detail::format_number(maximum).c_str());
                }
            } else {
                ImGui::TextDisabled("Unknown");
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (!pending.has_value() && selected_schema_field != nullptr && selected_index.has_value()) {
        auto const selected_segment_name{analysis_session_.inputs.selection.field};
        auto enum_module_index{std::optional<std::size_t>{}};
        auto fallback_enum_module_index{std::optional<std::size_t>{}};
        auto const& modules{document_->manifest().modules};
        for (std::size_t module_index{}; module_index < modules.size(); ++module_index) {
            auto const* enum_module{
                std::get_if<codegen::NormalModuleSchema>(&modules[module_index])};
            if (enum_module == nullptr) {
                continue;
            }
            if (!fallback_enum_module_index.has_value()) {
                fallback_enum_module_index = module_index;
            }
            if (enum_module->settings.namespace_name.value_or("") == node.identity.namespace_name) {
                enum_module_index = module_index;
                break;
            }
        }
        if (!enum_module_index.has_value()) {
            enum_module_index = fallback_enum_module_index;
        }
        ImGui::BeginDisabled(!enum_module_index.has_value() || selected_resolved_field == nullptr);
        if (ImGui::Button("Create enum for selected field...")) {
            new_enum_module_index_ = *enum_module_index;
            auto const suggested_name{suggested_type_name(selected_schema_field->name, "Type")};
            std::snprintf(
                new_enum_name_.data(), new_enum_name_.size(), "%s", suggested_name.c_str());
            auto& enum_module{
                std::get<codegen::NormalModuleSchema>(modules[new_enum_module_index_])};
            auto unique_name{std::string{new_enum_name_.data()}};
            auto suffix_number{std::size_t{1}};
            while (std::ranges::find(enum_module.declarations,
                                     unique_name,
                                     codegen::declaration_name) != enum_module.declarations.end()) {
                unique_name = std::string{new_enum_name_.data()} + std::to_string(suffix_number++);
            }
            std::snprintf(new_enum_name_.data(), new_enum_name_.size(), "%s", unique_name.c_str());
            new_enum_backing_auto_ = true;
            new_enum_width_auto_ = false;
            new_enum_bit_width_ = selected_resolved_field->bit_width;
            new_enum_signedness_ = 1;
            pending_packed_enum_binding_ = PendingPackedEnumBinding{
                .packed_declaration = *declaration, .field_name = selected_segment_name};
            module_initiated_dialog_ = false;
            open_new_enum_dialog_ = true;
        }
        ImGui::EndDisabled();
        if (!enum_module_index.has_value()) {
            ImGui::SameLine();
            ImGui::TextDisabled("no ordinary module is available.");
        } else if (selected_resolved_field == nullptr) {
            ImGui::SameLine();
            ImGui::TextDisabled("The selected field width is unresolved.");
        } else {
            ImGui::SameLine();
            ImGui::TextDisabled("Creates a shared enum declaration, then binds this field.");
        }

        auto scalar_module_index{std::optional<std::size_t>{}};
        auto fallback_scalar_module_index{std::optional<std::size_t>{}};
        for (std::size_t module_index{}; module_index < modules.size(); ++module_index) {
            auto const* scalar_module{
                std::get_if<codegen::NormalModuleSchema>(&modules[module_index])};
            if (scalar_module == nullptr) {
                continue;
            }
            if (!fallback_scalar_module_index.has_value()) {
                fallback_scalar_module_index = module_index;
            }
            if (scalar_module->settings.namespace_name.value_or("") ==
                node.identity.namespace_name) {
                scalar_module_index = module_index;
                break;
            }
        }
        if (!scalar_module_index.has_value()) {
            scalar_module_index = fallback_scalar_module_index;
        }
        auto const scalar_compatible{
            selected_resolved_field != nullptr && selected_integer_scalar == nullptr &&
            (selected_schema_field->kind == codegen::PackedFieldKind::signed_integer ||
             selected_schema_field->kind == codegen::PackedFieldKind::unsigned_integer)};
        ImGui::BeginDisabled(!scalar_module_index.has_value() || !scalar_compatible);
        if (ImGui::Button("Create integer scalar for selected field...")) {
            new_integer_scalar_module_index_ = *scalar_module_index;
            auto const& scalar_module{
                std::get<codegen::NormalModuleSchema>(modules[new_integer_scalar_module_index_])};
            auto const scalar_namespace{scalar_module.settings.namespace_name.value_or("")};
            auto const suggested_name{suggested_type_name(selected_schema_field->name, "Value")};
            auto unique_name{suggested_name};
            auto suffix_number{std::size_t{1}};
            while (std::ranges::any_of(document_->types().types(), [&](auto const& candidate) {
                return candidate.identity.namespace_name == scalar_namespace &&
                       candidate.identity.name == unique_name;
            })) {
                unique_name = suggested_name + std::to_string(suffix_number++);
            }
            std::snprintf(new_integer_scalar_name_.data(),
                          new_integer_scalar_name_.size(),
                          "%s",
                          unique_name.c_str());

            new_integer_scalar_signed_ =
                selected_schema_field->kind == codegen::PackedFieldKind::signed_integer;
            new_integer_scalar_width_auto_ = !selected_schema_field->bits.has_value();
            new_integer_scalar_bit_width_ = selected_resolved_field->bit_width;

            auto minimum{codegen::PackedIntegerValue{0}};
            auto maximum{codegen::PackedIntegerValue{0}};
            if (selected_schema_field->minimum_value.has_value() &&
                selected_schema_field->maximum_value.has_value()) {
                minimum = *selected_schema_field->minimum_value;
                maximum = *selected_schema_field->maximum_value;
            } else if (new_integer_scalar_signed_) {
                auto const negative_limit{selected_resolved_field->bit_width == 64
                                              ? std::uint64_t{1} << 63
                                              : std::uint64_t{1}
                                                    << (selected_resolved_field->bit_width - 1)};
                minimum = codegen::PackedIntegerValue::from_parts(true, negative_limit);
                maximum =
                    codegen::PackedIntegerValue{static_cast<std::int64_t>(negative_limit - 1)};
            } else {
                maximum = codegen::PackedIntegerValue{
                    selected_resolved_field->bit_width == 64
                        ? (std::numeric_limits<std::uint64_t>::max)()
                        : (std::uint64_t{1} << selected_resolved_field->bit_width) - 1};
            }
            auto const minimum_text{codegen::format_packed_integer(minimum)};
            auto const maximum_text{codegen::format_packed_integer(maximum)};
            std::snprintf(new_integer_scalar_minimum_.data(),
                          new_integer_scalar_minimum_.size(),
                          "%s",
                          minimum_text.c_str());
            std::snprintf(new_integer_scalar_maximum_.data(),
                          new_integer_scalar_maximum_.size(),
                          "%s",
                          maximum_text.c_str());
            pending_packed_integer_scalar_binding_ = PendingPackedIntegerScalarBinding{
                .packed_declaration = *declaration, .field_name = selected_segment_name};
            module_initiated_dialog_ = false;
            open_new_integer_scalar_dialog_ = true;
        }
        ImGui::EndDisabled();
        if (!scalar_module_index.has_value()) {
            ImGui::SameLine();
            ImGui::TextDisabled("no ordinary module is available.");
        } else if (selected_integer_scalar != nullptr) {
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
            auto const* representation_module{
                std::get_if<codegen::NormalModuleSchema>(&modules[module_index])};
            if (representation_module == nullptr) {
                continue;
            }
            if (!fallback_representation_module_index.has_value()) {
                fallback_representation_module_index = module_index;
            }
            if (representation_module->settings.namespace_name.value_or("") ==
                node.identity.namespace_name) {
                representation_module_index = module_index;
                break;
            }
        }
        if (!representation_module_index.has_value()) {
            representation_module_index = fallback_representation_module_index;
        }
        auto const plain_integer_field{
            selected_resolved_field != nullptr && selected_integer_scalar == nullptr &&
            (selected_schema_field->kind == codegen::PackedFieldKind::signed_integer ||
             selected_schema_field->kind == codegen::PackedFieldKind::unsigned_integer) &&
            !selected_schema_field->minimum_value.has_value() &&
            !selected_schema_field->maximum_value.has_value() &&
            selected_schema_field->named_codes.empty() &&
            !selected_schema_field->relationship.has_value() &&
            !selected_schema_field->range_helper};
        ImGui::BeginDisabled(!representation_module_index.has_value() || !plain_integer_field);
        if (ImGui::Button("Create fixed point for selected field...")) {
            new_fixed_point_module_index_ = *representation_module_index;
            auto const& module{
                std::get<codegen::NormalModuleSchema>(modules[new_fixed_point_module_index_])};
            auto const namespace_name{module.settings.namespace_name.value_or("")};
            auto const suggested_name{suggested_type_name(selected_schema_field->name, "Fixed")};
            auto unique_name{suggested_name};
            auto suffix_number{std::size_t{1}};
            while (std::ranges::any_of(document_->types().types(), [&](auto const& candidate) {
                return candidate.identity.namespace_name == namespace_name &&
                       candidate.identity.name == unique_name;
            })) {
                unique_name = suggested_name + std::to_string(suffix_number++);
            }
            std::snprintf(new_fixed_point_name_.data(),
                          new_fixed_point_name_.size(),
                          "%s",
                          unique_name.c_str());
            new_fixed_point_signed_ =
                selected_schema_field->kind == codegen::PackedFieldKind::signed_integer;
            new_fixed_point_total_bits_ = selected_resolved_field->bit_width;
            auto const available_magnitude_bits{new_fixed_point_total_bits_ -
                                                (new_fixed_point_signed_ ? 1U : 0U)};
            new_fixed_point_fractional_bits_ =
                std::min(new_fixed_point_total_bits_ / 2, available_magnitude_bits);
            new_fixed_point_rounding_ = 0;
            pending_packed_fixed_point_binding_ = PendingPackedFixedPointBinding{
                .packed_declaration = *declaration, .field_name = selected_segment_name};
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
            selected_resolved_field->bit_width >=
                (selected_schema_field->kind == codegen::PackedFieldKind::signed_integer ? 3U
                                                                                         : 2U)};
        ImGui::BeginDisabled(!representation_module_index.has_value() || !mini_float_compatible);
        if (ImGui::Button("Create mini float for selected field...")) {
            new_mini_float_module_index_ = *representation_module_index;
            auto const& module{
                std::get<codegen::NormalModuleSchema>(modules[new_mini_float_module_index_])};
            auto const namespace_name{module.settings.namespace_name.value_or("")};
            auto const suggested_name{suggested_type_name(selected_schema_field->name, "Float")};
            auto unique_name{suggested_name};
            auto suffix_number{std::size_t{1}};
            while (std::ranges::any_of(document_->types().types(), [&](auto const& candidate) {
                return candidate.identity.namespace_name == namespace_name &&
                       candidate.identity.name == unique_name;
            })) {
                unique_name = suggested_name + std::to_string(suffix_number++);
            }
            std::snprintf(new_mini_float_name_.data(),
                          new_mini_float_name_.size(),
                          "%s",
                          unique_name.c_str());
            new_mini_float_sign_bits_ =
                selected_schema_field->kind == codegen::PackedFieldKind::signed_integer ? 1U : 0U;
            auto const available_bits{selected_resolved_field->bit_width -
                                      new_mini_float_sign_bits_};
            new_mini_float_exponent_bits_ = std::min(5U, available_bits);
            new_mini_float_significand_bits_ = available_bits - new_mini_float_exponent_bits_;
            new_mini_float_exponent_bias_ =
                static_cast<std::int32_t>((1U << (new_mini_float_exponent_bits_ - 1)) - 1);
            pending_packed_mini_float_binding_ = PendingPackedMiniFloatBinding{
                .packed_declaration = *declaration, .field_name = selected_segment_name};
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
            if (selected_linear_quantized != nullptr) {
                ImGui::TextDisabled("This placement inherits semantic meaning through the "
                                    "quantizer's source scalar.");
            } else if (selected_fixed_point != nullptr) {
                ImGui::TextDisabled(
                    "This fixed-point placement owns its scaled numerical representation.");
            } else if (selected_mini_float != nullptr) {
                ImGui::TextDisabled("This mini-float placement owns its numerical encoding.");
            }
            ImGui::BeginDisabled(selected_linear_quantized != nullptr ||
                                 selected_fixed_point != nullptr || selected_mini_float != nullptr);
            ImGui::SetNextItemWidth(180.0F);
            auto const current_kind{semantic_relationship_kinds[static_cast<std::size_t>(
                std::clamp(packed_relationship_kind_,
                           0,
                           static_cast<int>(semantic_relationship_kinds.size() - 1)))]};
            auto const current_unit{semantic_relationship_units[static_cast<std::size_t>(
                std::clamp(packed_relationship_unit_,
                           0,
                           static_cast<int>(semantic_relationship_units.size() - 1)))]};
            if (ImGui::BeginCombo("Kind",
                                  codegen::semantic_relation_kind_name(current_kind).data())) {
                for (std::size_t kind_index{}; kind_index < semantic_relationship_kinds.size();
                     ++kind_index) {
                    auto const kind{semantic_relationship_kinds[kind_index]};
                    auto const chosen{packed_relationship_kind_ == static_cast<int>(kind_index)};
                    if (ImGui::Selectable(codegen::semantic_relation_kind_name(kind).data(),
                                          chosen)) {
                        packed_relationship_kind_ = static_cast<int>(kind_index);
                        if (selected_schema_field->relationship.has_value()) {
                            auto replacement{*schema};
                            auto& relationship{*std::get<codegen::PackedFieldSchema>(
                                                    replacement.segments[*selected_index])
                                                    .relationship};
                            relationship.kind = kind;
                            relationship.unit = kind == codegen::SemanticRelationKind::offset_into
                                                  ? std::optional{current_unit}
                                                  : std::nullopt;
                            if (apply_document_edit(
                                    ReplacePackedValue{.declaration = *declaration,
                                                       .schema = std::move(replacement)})) {
                                analysis_session_.inputs.selection.field = selected_segment_name;
                                return true;
                            }
                        }
                    }
                }
                ImGui::EndCombo();
            }

            if (current_kind == codegen::SemanticRelationKind::offset_into) {
                ImGui::SetNextItemWidth(180.0F);
                if (ImGui::BeginCombo("Unit",
                                      codegen::semantic_relation_unit_name(current_unit).data())) {
                    for (std::size_t unit_index{}; unit_index < semantic_relationship_units.size();
                         ++unit_index) {
                        auto const unit{semantic_relationship_units[unit_index]};
                        auto const chosen{packed_relationship_unit_ ==
                                          static_cast<int>(unit_index)};
                        if (ImGui::Selectable(codegen::semantic_relation_unit_name(unit).data(),
                                              chosen)) {
                            packed_relationship_unit_ = static_cast<int>(unit_index);
                            if (selected_schema_field->relationship.has_value()) {
                                auto replacement{*schema};
                                std::get<codegen::PackedFieldSchema>(
                                    replacement.segments[*selected_index])
                                    .relationship->unit = unit;
                                if (apply_document_edit(
                                        ReplacePackedValue{.declaration = *declaration,
                                                           .schema = std::move(replacement)})) {
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
            if (selected_schema_field->relationship.has_value() &&
                (target_submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                if (packed_relationship_target_.front() == '\0') {
                    schema_edit_message_ = "Relationship target cannot be empty.";
                } else {
                    auto replacement{*schema};
                    std::get<codegen::PackedFieldSchema>(replacement.segments[*selected_index])
                        .relationship->target.name = packed_relationship_target_.data();
                    if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                               .schema = std::move(replacement)})) {
                        analysis_session_.inputs.selection.field = selected_segment_name;
                        return true;
                    }
                }
            }
            ImGui::SameLine();
            ImGui::PushID("relationship-target");
            auto picked_relationship_target{
                draw_type_picker(node.identity.module_name, node.identity)};
            ImGui::PopID();
            if (picked_relationship_target.has_value()) {
                std::snprintf(packed_relationship_target_.data(),
                              packed_relationship_target_.size(),
                              "%s",
                              picked_relationship_target->c_str());
                auto replacement{*schema};
                auto& relationship{
                    std::get<codegen::PackedFieldSchema>(replacement.segments[*selected_index])
                        .relationship};
                relationship = codegen::SemanticRelationSchema{
                    .kind = current_kind,
                    .target = codegen::TypeRef{.name = *picked_relationship_target,
                                               .suffix = {},
                                               .nested = std::nullopt},
                    .unit = current_kind == codegen::SemanticRelationKind::offset_into
                              ? std::optional{current_unit}
                              : std::nullopt};
                if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                           .schema = std::move(replacement)})) {
                    analysis_session_.inputs.selection.field = selected_segment_name;
                    return true;
                }
            }
            ImGui::SameLine();
            if (selected_schema_field->relationship.has_value()) {
                if (ImGui::SmallButton("Clear")) {
                    auto replacement{*schema};
                    std::get<codegen::PackedFieldSchema>(replacement.segments[*selected_index])
                        .relationship.reset();
                    if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                               .schema = std::move(replacement)})) {
                        analysis_session_.inputs.selection.field = selected_segment_name;
                        return true;
                    }
                }
                ImGui::SameLine();
                if (selected_resolved_field != nullptr &&
                    selected_resolved_field->relationship.has_value() &&
                    detail::semantic_type_navigation_button()) {
                    select_type(selected_resolved_field->relationship->target.type);
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
                    auto replacement{*schema};
                    std::get<codegen::PackedFieldSchema>(replacement.segments[*selected_index])
                        .relationship = codegen::SemanticRelationSchema{
                        .kind = current_kind,
                        .target = codegen::TypeRef{.name = packed_relationship_target_.data(),
                                                   .suffix = {},
                                                   .nested = std::nullopt},
                        .unit = current_kind == codegen::SemanticRelationKind::offset_into
                                  ? std::optional{current_unit}
                                  : std::nullopt};
                    if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                               .schema = std::move(replacement)})) {
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
                    if (kind == codegen::SemanticRelationKind::count_of) {
                        ImGui::TextDisabled("Live counts include 0 through capacity; named "
                                            "sentinels add code states.");
                    } else if (kind == codegen::SemanticRelationKind::offset_into) {
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
    if (!pending.has_value() && selected_schema_field != nullptr &&
        selected_integer_scalar != nullptr && selected_resolved_field != nullptr) {
        if (detail::section("Shared scalar domain")) {
            ImGui::TextDisabled("Range, signedness, and named codes are authored on the referenced "
                                "integer scalar.");
            for (auto const& code : selected_resolved_field->named_codes) {
                auto const value{codegen::format_packed_integer(code.value)};
                ImGui::BulletText("%s = %s%s",
                                  code.name.c_str(),
                                  value.c_str(),
                                  code.sentinel ? " (sentinel)" : "");
            }
        }
    } else if (!pending.has_value() && selected_schema_field != nullptr &&
               selected_linear_quantized != nullptr) {
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
                    codegen::format_packed_integer(quantization->source_minimum)};
                auto const source_maximum{
                    codegen::format_packed_integer(quantization->source_maximum)};
                ImGui::Text("Source range: %s..%s", source_minimum.c_str(), source_maximum.c_str());
                ImGui::Text("Encoded width: %u bits", quantization->encoded_storage_bits);
                ImGui::Text("Usable codes: %s",
                            detail::format_code_count(quantization->usable_code_count).c_str());
                ImGui::Text("Reserved codes: %llu",
                            static_cast<unsigned long long>(quantization->reserved_code_count));
                ImGui::Text("Resolution: %.9Lg", quantization->resolution);
                ImGui::Text("Maximum rounding error: %.9Lg", quantization->maximum_rounding_error);
                ImGui::Text("Clipping: %s",
                            codegen::quantization_clipping_name(quantization->clipping).data());
            } else {
                ImGui::TextDisabled("Quantization analysis is unavailable.");
            }
            ImGui::TextDisabled("Generated packed APIs expose encoded codes; decoding remains "
                                "representation policy.");
        }
    } else if (!pending.has_value() && selected_schema_field != nullptr &&
               selected_fixed_point != nullptr) {
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
                auto const minimum_raw{codegen::format_packed_integer(fixed->minimum_raw_value)};
                auto const maximum_raw{codegen::format_packed_integer(fixed->maximum_raw_value)};
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
                            codegen::fixed_point_rounding_name(fixed->rounding).data());
            } else {
                ImGui::TextDisabled("Fixed-point analysis is unavailable.");
            }
            ImGui::TextDisabled(
                "Generated packed APIs expose raw codes and checked double conversion.");
        }
    } else if (!pending.has_value() && selected_schema_field != nullptr &&
               selected_mini_float != nullptr) {
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
    } else if (!pending.has_value() && selected_schema_field != nullptr &&
               selected_index.has_value() && selected_linear_quantized == nullptr &&
               selected_fixed_point == nullptr && selected_mini_float == nullptr) {
        auto const selected_segment_name{analysis_session_.inputs.selection.field};
        auto const effective_width{
            selected_resolved_field != nullptr ? selected_resolved_field->bit_width : 1U};
        auto const named_value{
            first_available_code(*selected_schema_field, effective_width, false)};
        auto const sentinel_value{
            first_available_code(*selected_schema_field, effective_width, true)};
        auto const current_code_index{
            selected_code == selected_schema_field->named_codes.end()
                ? std::optional<std::size_t>{}
                : std::optional<std::size_t>{static_cast<std::size_t>(
                      selected_code - selected_schema_field->named_codes.begin())}};

        if (detail::section("Named codes")) {
            ImGui::TextDisabled("Named codes remain ordinary integer values; sentinels sit outside "
                                "the live range.");
            ImGui::BeginDisabled(!named_value.has_value());
            if (ImGui::Button("+ Named code")) {
                auto replacement{*schema};
                auto& field{
                    std::get<codegen::PackedFieldSchema>(replacement.segments[*selected_index])};
                auto name{unique_code_name(field.named_codes, "Code")};
                field.named_codes.push_back(
                    {.name = name, .value = *named_value, .sentinel = false});
                if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                           .schema = std::move(replacement)})) {
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
                auto replacement{*schema};
                auto& field{
                    std::get<codegen::PackedFieldSchema>(replacement.segments[*selected_index])};
                auto name{unique_code_name(field.named_codes, "Invalid")};
                field.named_codes.push_back(
                    {.name = name, .value = *sentinel_value, .sentinel = true});
                if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                           .schema = std::move(replacement)})) {
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
                          *selected_schema_field,
                          effective_width,
                          selected_schema_field->named_codes[*current_code_index].sentinel)
                    : std::nullopt};
            ImGui::BeginDisabled(!current_code_index.has_value() || !duplicate_value.has_value());
            if (ImGui::Button("Duplicate code")) {
                auto replacement{*schema};
                auto& field{
                    std::get<codegen::PackedFieldSchema>(replacement.segments[*selected_index])};
                auto copy{field.named_codes[*current_code_index]};
                copy.name = unique_code_name(field.named_codes, copy.name + "_copy");
                copy.value = *duplicate_value;
                field.named_codes.insert(field.named_codes.begin() +
                                             static_cast<std::ptrdiff_t>(*current_code_index + 1),
                                         copy);
                if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                           .schema = std::move(replacement)})) {
                    analysis_session_.inputs.selection.field = selected_segment_name;
                    selected_packed_code_ = std::move(copy.name);
                    ImGui::EndDisabled();
                    return true;
                }
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(!current_code_index.has_value() || *current_code_index == 0);
            if (ImGui::Button("Code up")) {
                auto const code_name{selected_schema_field->named_codes[*current_code_index].name};
                auto replacement{*schema};
                auto& codes{
                    std::get<codegen::PackedFieldSchema>(replacement.segments[*selected_index])
                        .named_codes};
                std::swap(codes[*current_code_index], codes[*current_code_index - 1]);
                if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                           .schema = std::move(replacement)})) {
                    analysis_session_.inputs.selection.field = selected_segment_name;
                    selected_packed_code_ = code_name;
                    ImGui::EndDisabled();
                    return true;
                }
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(!current_code_index.has_value() ||
                                 *current_code_index + 1 >=
                                     selected_schema_field->named_codes.size());
            if (ImGui::Button("Code down")) {
                auto const code_name{selected_schema_field->named_codes[*current_code_index].name};
                auto replacement{*schema};
                auto& codes{
                    std::get<codegen::PackedFieldSchema>(replacement.segments[*selected_index])
                        .named_codes};
                std::swap(codes[*current_code_index], codes[*current_code_index + 1]);
                if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                           .schema = std::move(replacement)})) {
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
                auto replacement{*schema};
                auto& codes{
                    std::get<codegen::PackedFieldSchema>(replacement.segments[*selected_index])
                        .named_codes};
                codes.erase(codes.begin() + static_cast<std::ptrdiff_t>(*current_code_index));
                auto const next_code{
                    codes.empty() ? std::string{}
                                  : codes[std::min(*current_code_index, codes.size() - 1)].name};
                if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                           .schema = std::move(replacement)})) {
                    analysis_session_.inputs.selection.field = selected_segment_name;
                    selected_packed_code_ = next_code;
                    ImGui::EndDisabled();
                    return true;
                }
            }
            ImGui::EndDisabled();

            if (!selected_schema_field->minimum_value.has_value()) {
                ImGui::TextDisabled("Set a semantic range before adding sentinel codes.");
            } else if (!sentinel_value.has_value()) {
                ImGui::TextDisabled("No unused code outside the live range fits this field width.");
            }

            if (detail::begin_editable_table(
                    "packed-named-codes", 4, selected_schema_field->named_codes.size())) {
                detail::editable_table_column("", ImGuiTableColumnFlags_WidthFixed);
                detail::editable_table_column("Name");
                detail::editable_table_column("Value");
                detail::editable_table_column("Sentinel", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableHeadersRow();
                for (std::size_t code_index{};
                     code_index < selected_schema_field->named_codes.size();
                     ++code_index) {
                    auto const& code{selected_schema_field->named_codes[code_index]};
                    auto const row_selected{selected_packed_code_ == code.name};
                    ImGui::PushID(static_cast<int>(code_index));
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    if (detail::editable_table_row_handle(row_selected)) {
                        selected_packed_code_ = code.name;
                        packed_code_editor_declaration_.reset();
                    }
                    if (ImGui::BeginDragDropSource()) {
                        ImGui::SetDragDropPayload(
                            "PACKED_CODE_ROW", &code_index, sizeof(code_index));
                        ImGui::Text("Move %s", code.name.c_str());
                        ImGui::EndDragDropSource();
                    }
                    if (ImGui::BeginDragDropTarget()) {
                        if (auto const* payload{ImGui::AcceptDragDropPayload("PACKED_CODE_ROW")}) {
                            auto const source_index{
                                *static_cast<std::size_t const*>(payload->Data)};
                            if (source_index < selected_schema_field->named_codes.size() &&
                                source_index != code_index) {
                                pending = *schema;
                                auto& codes{std::get<codegen::PackedFieldSchema>(
                                                pending->segments[*selected_index])
                                                .named_codes};
                                selected_code_after_edit = codes[source_index].name;
                                move_element(codes, source_index, code_index);
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
                            pending = *schema;
                            auto& edited{std::get<codegen::PackedFieldSchema>(
                                             pending->segments[*selected_index])
                                             .named_codes[code_index]};
                            edited.name = packed_code_name_.data();
                            selected_code_after_edit = edited.name;
                        }
                    } else {
                        ImGui::TextUnformatted(code.name.c_str());
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
                                pending = *schema;
                                std::get<codegen::PackedFieldSchema>(
                                    pending->segments[*selected_index])
                                    .named_codes[code_index]
                                    .value = *value;
                            } else {
                                schema_edit_message_ = "Named code value must be a signed decimal "
                                                       "or hexadecimal integer.";
                            }
                        }
                    } else {
                        auto const value{codegen::format_packed_integer(code.value)};
                        ImGui::TextUnformatted(value.c_str());
                    }

                    ImGui::TableNextColumn();
                    if (row_selected) {
                        auto sentinel{packed_code_sentinel_};
                        if (ImGui::Checkbox("##code-sentinel", &sentinel)) {
                            pending = *schema;
                            std::get<codegen::PackedFieldSchema>(pending->segments[*selected_index])
                                .named_codes[code_index]
                                .sentinel = sentinel;
                        }
                    } else {
                        ImGui::TextUnformatted(code.sentinel ? "yes" : "-");
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
        auto const& segment{pending->segments[*selected_index]};
        if (codegen::packed_segment_name(segment).empty()) {
            schema_edit_message_ = "Packed segment name cannot be empty.";
            return false;
        }
        if (auto const* field{std::get_if<codegen::PackedFieldSchema>(&segment)};
            field != nullptr && field->type.name.empty()) {
            schema_edit_message_ = "Packed field semantic type cannot be empty.";
            return false;
        }
        if (apply_document_edit(
                ReplacePackedValue{.declaration = *declaration, .schema = std::move(*pending)})) {
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
