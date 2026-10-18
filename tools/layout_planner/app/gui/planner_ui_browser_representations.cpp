#include "planner_ui_browser_common.hpp"

#include <codegen/schema/enum_reflection.h>
#include <codegen/schema/enum_schemas.h>
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
#include <codegen/schema/module_kind.h>
#include <codegen/schema/module_settings_store.h>
#include <codegen/schema/normal_module_schemas.h>
#include <codegen/schema/optional_presence_bit_schemas.h>
#include <codegen/schema/optional_sentinel_schemas.h>
#include <codegen/schema/packed_bit_order.h>
#include <codegen/schema/packed_byte_order.h>
#include <codegen/schema/packed_field_kind.h>
#include <codegen/schema/packed_field_schemas.h>
#include <codegen/schema/packed_named_code_schemas.h>
#include <codegen/schema/packed_segment_id.h>
#include <codegen/schema/packed_value_schemas.h>
#include <codegen/schema/quantization_clipping.h>
#include <codegen/schema/schema_range.h>
#include <codegen/schema/type_refs.h>
#include <lispb/compiler_storage.h>
#include <lispb/indices.h>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace ioj::layout_planner {

auto PlannerUi::bind_new_type_to_packed_field(TypeIdentity const& source,
                                              std::optional<PendingPackedBinding> const& pending)
    -> bool {
    if (!pending) {
        return true;
    }
    auto const binding{*pending};
    auto const packed_info{document_->declarations().find(binding.packed_declaration)};
    auto const schema{document_->declaration_handle(binding.packed_declaration,
                                                    lispb::codegen::DeclarationKind::packed_value)};
    auto const selection{
        packed_info ? std::optional{document_->declarations()
                                        .identity[document_->declarations().index(*packed_info)]}
                    : std::nullopt};
    auto rollback_creation = [&](std::string message) {
        auto const rollback{document_->undo()};
        if (rollback && *rollback) {
            sync_document_graph(selection);
            schema_edit_message_ = std::move(message) + " The new declaration was rolled back.";
        } else if (!rollback) {
            schema_edit_message_ =
                std::move(message) +
                " The new declaration could not be rolled back: " + rollback.error().message;
        } else {
            schema_edit_message_ =
                std::move(message) +
                " The new declaration could not be rolled back because history did not change.";
        }
        return false;
    };
    if (!schema || !packed_info) {
        return rollback_creation("The packed declaration is no longer available.");
    }

    auto const segments{declarations().items(
        document_->storage()
            .declarations.packed_values
            .segments[document_->storage().declarations.packed_values.index(*schema)])};
    auto const segment{std::ranges::find_if(segments, [&](auto handle) {
        auto const field{((handle).kind() == lispb::codegen::PackedSegmentKind::field
                              ? std::optional{handle}
                              : std::nullopt)};
        return field &&
               declarations().packed_fields.name[declarations().packed_fields.index(*field)] ==
                   binding.field_name;
    })};
    if (segment == segments.end()) {
        return rollback_creation("The selected packed field is no longer available.");
    }
    auto const index{static_cast<std::size_t>(segment - segments.begin())};
    auto const& types{analysis_session_.inputs.workspace.types()};
    auto const type{types.find(source)};
    assert(type);
    auto const definition{types.definition(*type)};
    auto kind{lispb::codegen::PackedFieldKind::enumeration};
    if (auto const scalar{((definition).kind() == lispb::schema::TypeDefinitionKind::integer_scalar
                               ? std::optional{definition}
                               : std::nullopt)}) {
        kind =
            types.definitions()
                    .integer_scalars.signedness[types.definitions().integer_scalars.index(*scalar)]
                ? lispb::codegen::PackedFieldKind::signed_integer
                : lispb::codegen::PackedFieldKind::unsigned_integer;
    } else if (((definition).kind() == lispb::schema::TypeDefinitionKind::fixed_point)) {
        kind = lispb::codegen::PackedFieldKind::fixed_point;
    } else if (((definition).kind() == lispb::schema::TypeDefinitionKind::mini_float)) {
        kind = lispb::codegen::PackedFieldKind::mini_float;
    } else {
        assert(((definition).kind() == lispb::schema::TypeDefinitionKind::enumeration));
    }

    auto const reference{
        source.namespace_name.value == 0
            ? source.name
            : strings().intern(text(source.namespace_name) + "::" + text(source.name))};
    auto replacement{document_->storage().declarations.packed_values.clone(*schema)};
    document_->storage()
        .declarations.packed_values
        .segments[document_->storage().declarations.packed_values.index(replacement)] = [&] {
        auto const original_items{declarations().items(
            document_->storage()
                .declarations.packed_values
                .segments[document_->storage().declarations.packed_values.index(replacement)])};
        auto rows{std::vector(original_items.begin(), original_items.end())};
        rows[index] = [&] {
            auto field{declarations().packed_fields.clone(rows[index])};
            declarations().packed_fields.type[declarations().packed_fields.index(field)] = [&] {
                auto const new_type_refs{declarations().type_refs.append()};
                declarations().type_refs.name[declarations().type_refs.index(new_type_refs)] =
                    reference;
                return new_type_refs;
            }();
            declarations().packed_fields.kind[declarations().packed_fields.index(field)] = kind;
            declarations().packed_fields.range_helper[declarations().packed_fields.index(field)] =
                false;
            declarations()
                .packed_fields.minimum_value[declarations().packed_fields.index(field)]
                .reset();
            declarations()
                .packed_fields.maximum_value[declarations().packed_fields.index(field)]
                .reset();
            declarations()
                .packed_fields.named_codes[declarations().packed_fields.index(field)] = {};
            if (kind == lispb::codegen::PackedFieldKind::fixed_point ||
                kind == lispb::codegen::PackedFieldKind::mini_float) {
                declarations()
                    .packed_fields.bits[declarations().packed_fields.index(field)]
                    .reset();
                declarations()
                    .packed_fields.relationship[declarations().packed_fields.index(field)]
                    .reset();
            }
            return field;
        }();
        return declarations().range(rows);
    }();
    if (!apply_document_edit(
            document_->replace_declaration(binding.packed_declaration, replacement), selection)) {
        return rollback_creation(
            "The declaration was valid, but binding the packed field failed: " +
            schema_edit_message_);
    }
    analysis_session_.inputs.selection.field = text(binding.field_name);
    return true;
}

void PlannerUi::draw_new_enum_dialog() {
    if (open_new_enum_dialog_) {
        ImGui::OpenPopup("New enum");
        open_new_enum_dialog_ = false;
    }
    if (!ImGui::BeginPopupModal("New enum", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
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

    if (pending_packed_enum_binding_.has_value()) {
        ImGui::TextWrapped(
            "Create a shared enum declaration and bind packed field '%s' to it. Creation and "
            "binding are separate undoable history steps.",
            text(pending_packed_enum_binding_->field_name).c_str());
        ImGui::Separator();
    }

    auto const& modules{document_->manifest().modules};
    auto first_enum_module{std::optional<std::size_t>{}};
    for (std::size_t index{}; index < modules.size(); ++index) {
        if ((modules[index].kind() == lispb::codegen::ModuleKind::normal)) {
            first_enum_module = index;
            break;
        }
    }
    if (!first_enum_module.has_value()) {
        ImGui::TextDisabled("The target has no ordinary module to receive a new declaration.");
    } else {
        if (new_enum_module_index_ >= modules.size() ||
            !(modules[new_enum_module_index_].kind() == lispb::codegen::ModuleKind::normal)) {
            new_enum_module_index_ = *first_enum_module;
        }
        auto const& selected_module{modules[new_enum_module_index_]};
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
                        index == new_enum_module_index_)) {
                    new_enum_module_index_ = index;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::InputText("Name", new_enum_name_.data(), new_enum_name_.size());
        ImGui::Checkbox("Auto C++ backing", &new_enum_backing_auto_);
        if (!new_enum_backing_auto_) {
            ImGui::InputText("C++ backing type",
                             new_enum_underlying_type_.data(),
                             new_enum_underlying_type_.size());
            ImGui::TextDisabled("Use a registered name such as @native_uint8 or a C++ spelling.");
        } else {
            ImGui::TextDisabled(
                "A legal fixed-width C++ backing is derived from the semantic domain.");
        }
        ImGui::Checkbox("Auto semantic width", &new_enum_width_auto_);
        if (!new_enum_width_auto_) {
            ImGui::InputScalar(
                "Semantic width", ImGuiDataType_U32, &new_enum_bit_width_, nullptr, nullptr, "%u");
        }
        ImGui::TextDisabled(
            "Semantic width is independent of the C++ type used by the current lowering target.");
        constexpr std::array signedness_labels{"Auto / inferred", "Unsigned", "Signed"};
        ImGui::Combo("Semantic signedness",
                     &new_enum_signedness_,
                     signedness_labels.data(),
                     static_cast<int>(signedness_labels.size()));

        auto const ready{
            new_enum_name_.front() != '\0' &&
            (new_enum_backing_auto_ || new_enum_underlying_type_.front() != '\0') &&
            (new_enum_width_auto_ || (new_enum_bit_width_ >= 1 && new_enum_bit_width_ <= 64))};
        ImGui::BeginDisabled(!ready);
        auto const create_label{pending_packed_enum_binding_.has_value() ? "Create and use"
                                                                         : "Create"};
        if (ImGui::Button(create_label)) {
            auto const& module{modules[new_enum_module_index_]};
            auto const name{std::string{new_enum_name_.data()}};
            auto const bit_width{new_enum_width_auto_ ? std::optional<std::uint8_t>{}
                                                      : std::optional{static_cast<std::uint8_t>(
                                                            new_enum_bit_width_)}};
            auto const signedness{new_enum_signedness_ == 0   ? EnumSignedness::unspecified
                                  : new_enum_signedness_ == 2 ? EnumSignedness::signed_
                                                              : EnumSignedness::unsigned_};
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
            auto selection{std::optional<TypeIdentity>{identity}};
            if (pending_packed_enum_binding_.has_value()) {
                if (auto const packed_info{document_->declarations().find(
                        pending_packed_enum_binding_->packed_declaration)};
                    packed_info.has_value()) {
                    selection = document_->declarations()
                                    .identity[document_->declarations().index(*packed_info)];
                }
            }
            if (apply_document_edit(
                    document_->create_declaration(
                        id,
                        new_enum_module_index_,
                        [&] {
                            auto const new_enums{declarations().enums.append()};
                            declarations().enums.name[declarations().enums.index(new_enums)] =
                                strings().intern(name);
                            declarations()
                                .enums.underlying_type[declarations().enums.index(new_enums)] =
                                new_enum_backing_auto_
                                    ? std::optional<lispb::codegen::TypeRefId>{}
                                    : std::optional{[&] {
                                          auto const new_type_refs{
                                              declarations().type_refs.append()};
                                          declarations()
                                              .type_refs
                                              .name[declarations().type_refs.index(new_type_refs)] =
                                              strings().intern(new_enum_underlying_type_.data());
                                          return new_type_refs;
                                      }()};
                            declarations().enums.values[declarations().enums.index(new_enums)] =
                                declarations().range<lispb::codegen::EnumeratorId>({[&] {
                                    auto const new_enumerators{declarations().enumerators.append()};
                                    declarations()
                                        .enumerators
                                        .name[declarations().enumerators.index(new_enumerators)] =
                                        strings().intern("Value0");
                                    declarations()
                                        .enumerators.initializer[declarations().enumerators.index(
                                            new_enumerators)] = strings().add("0");
                                    declarations()
                                        .enumerators.display_name[declarations().enumerators.index(
                                            new_enumerators)] = std::nullopt;
                                    declarations()
                                        .enumerators
                                        .hidden[declarations().enumerators.index(new_enumerators)] =
                                        false;
                                    declarations().enumerators.serialized_name
                                        [declarations().enumerators.index(new_enumerators)] =
                                        std::nullopt;
                                    return new_enumerators;
                                }()});
                            declarations().enums.count[declarations().enums.index(new_enums)] =
                                std::nullopt;
                            declarations()
                                .enums.conversions[declarations().enums.index(new_enums)] = {};
                            declarations()
                                .enums.export_specifier[declarations().enums.index(new_enums)] = {};
                            declarations()
                                .enums.unreal_projection[declarations().enums.index(new_enums)] =
                                std::nullopt;
                            declarations().enums.bit_width[declarations().enums.index(new_enums)] =
                                bit_width;
                            declarations().enums.reflection[declarations().enums.index(new_enums)] =
                                lispb::codegen::EnumReflection::none;
                            declarations().enums.signedness[declarations().enums.index(new_enums)] =
                                signedness;
                            declarations().enums.enum_array[declarations().enums.index(new_enums)] =
                                false;
                            declarations().enums.native_api[declarations().enums.index(new_enums)] =
                                false;
                            return new_enums;
                        }(),
                        std::nullopt),
                    selection) &&
                bind_new_type_to_packed_field(identity, pending_packed_enum_binding_)) {
                new_enum_name_.fill('\0');
                std::snprintf(new_enum_underlying_type_.data(),
                              new_enum_underlying_type_.size(),
                              "%s",
                              "std::uint8_t");
                new_enum_width_auto_ = true;
                new_enum_bit_width_ = 1;
                new_enum_signedness_ = 0;
                new_enum_backing_auto_ = true;
                selected_enumerator_ = "Value0";
                pending_packed_enum_binding_.reset();
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
    }
    if (ImGui::Button("Cancel")) {
        pending_packed_enum_binding_.reset();
        ImGui::CloseCurrentPopup();
    }
    if (!schema_edit_message_.empty()) {
        ImGui::TextWrapped("%s", schema_edit_message_.c_str());
    }
    ImGui::EndPopup();
}

void PlannerUi::draw_new_packed_value_dialog() {
    if (open_new_packed_value_dialog_) {
        ImGui::OpenPopup("New packed value");
        open_new_packed_value_dialog_ = false;
    }
    if (!ImGui::BeginPopupModal("New packed value", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
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
    auto first_packed_module{std::optional<std::size_t>{}};
    for (std::size_t index{}; index < modules.size(); ++index) {
        if ((modules[index].kind() == lispb::codegen::ModuleKind::normal)) {
            first_packed_module = index;
            break;
        }
    }
    if (!first_packed_module.has_value()) {
        ImGui::TextDisabled("The target has no ordinary module to receive a new declaration.");
    } else {
        if (new_packed_module_index_ >= modules.size() ||
            !(modules[new_packed_module_index_].kind() == lispb::codegen::ModuleKind::normal)) {
            new_packed_module_index_ = *first_packed_module;
        }
        auto const& selected_module{modules[new_packed_module_index_]};
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
                        index == new_packed_module_index_)) {
                    new_packed_module_index_ = index;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::InputText("Name", new_packed_value_name_.data(), new_packed_value_name_.size());
        ImGui::InputText(
            "Storage type", new_packed_storage_type_.data(), new_packed_storage_type_.size());
        constexpr std::array byte_order_labels{"Unspecified", "Little endian", "Big endian"};
        ImGui::Combo("Serialized byte order",
                     &new_packed_byte_order_,
                     byte_order_labels.data(),
                     static_cast<int>(byte_order_labels.size()));
        constexpr std::array bit_order_labels{
            "Default (LSB-first)", "Explicit LSB-first", "MSB-first"};
        ImGui::Combo("Segment bit order",
                     &new_packed_bit_order_,
                     bit_order_labels.data(),
                     static_cast<int>(bit_order_labels.size()));
        ImGui::TextDisabled("The declaration starts with one editable 1-bit uint8 field.");
        ImGui::TextDisabled(
            "Byte order describes serialized bytes; it does not change the host integer ABI.");

        auto const ready{new_packed_value_name_.front() != '\0' &&
                         new_packed_storage_type_.front() != '\0'};
        ImGui::BeginDisabled(!ready);
        if (ImGui::Button("Create")) {
            auto const& module{modules[new_packed_module_index_]};
            auto const name{std::string{new_packed_value_name_.data()}};
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
                        new_packed_module_index_,
                        [&] {
                            auto const new_packed_values{declarations().packed_values.append()};
                            declarations().packed_values.name[declarations().packed_values.index(
                                new_packed_values)] = strings().intern(name);
                            declarations()
                                .packed_values.storage_type[declarations().packed_values.index(
                                    new_packed_values)] = [&] {
                                auto const new_type_refs{declarations().type_refs.append()};
                                declarations()
                                    .type_refs.name[declarations().type_refs.index(new_type_refs)] =
                                    strings().intern(new_packed_storage_type_.data());
                                declarations().type_refs.suffix[declarations().type_refs.index(
                                    new_type_refs)] = {};
                                declarations().type_refs.nested[declarations().type_refs.index(
                                    new_type_refs)] = std::nullopt;
                                return new_type_refs;
                            }();
                            declarations()
                                .packed_values
                                .segments[declarations().packed_values.index(new_packed_values)] =
                                declarations().range<lispb::codegen::PackedSegmentId>({[&] {
                                    auto const new_packed_fields{
                                        declarations().packed_fields.append()};
                                    declarations()
                                        .packed_fields.name[declarations().packed_fields.index(
                                            new_packed_fields)] = strings().intern("value");
                                    declarations()
                                        .packed_fields.type[declarations().packed_fields.index(
                                            new_packed_fields)] = [&] {
                                        auto const new_type_refs{declarations().type_refs.append()};
                                        declarations()
                                            .type_refs
                                            .name[declarations().type_refs.index(new_type_refs)] =
                                            strings().intern("std::uint8_t");
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
                                        .packed_fields.bits[declarations().packed_fields.index(
                                            new_packed_fields)] = 1;
                                    declarations()
                                        .packed_fields.kind[declarations().packed_fields.index(
                                            new_packed_fields)] =
                                        lispb::codegen::PackedFieldKind::unsigned_integer;
                                    declarations().packed_fields.range_helper
                                        [declarations().packed_fields.index(new_packed_fields)] =
                                        false;
                                    declarations().packed_fields.minimum_value
                                        [declarations().packed_fields.index(new_packed_fields)] =
                                        std::nullopt;
                                    declarations().packed_fields.maximum_value
                                        [declarations().packed_fields.index(new_packed_fields)] =
                                        std::nullopt;
                                    declarations()
                                        .packed_fields
                                        .named_codes[declarations().packed_fields.index(
                                            new_packed_fields)] = {};
                                    declarations().packed_fields.relationship
                                        [declarations().packed_fields.index(new_packed_fields)] =
                                        std::nullopt;
                                    return new_packed_fields;
                                }()});
                            declarations()
                                .packed_values.invalid_value[declarations().packed_values.index(
                                    new_packed_values)] = std::nullopt;
                            declarations()
                                .packed_values.export_specifier[declarations().packed_values.index(
                                    new_packed_values)] = {};
                            declarations()
                                .packed_values
                                .byte_order[declarations().packed_values.index(new_packed_values)] =
                                new_packed_byte_order_ == 0
                                    ? std::nullopt
                                    : std::optional{
                                          new_packed_byte_order_ == 1
                                              ? lispb::codegen::PackedByteOrder::little_endian
                                              : lispb::codegen::PackedByteOrder::big_endian};
                            declarations()
                                .packed_values
                                .bit_order[declarations().packed_values.index(new_packed_values)] =
                                new_packed_bit_order_ == 0
                                    ? std::nullopt
                                    : std::optional{new_packed_bit_order_ == 1
                                                        ? lispb::codegen::PackedBitOrder::
                                                              least_significant_first
                                                        : lispb::codegen::PackedBitOrder::
                                                              most_significant_first};
                            return new_packed_values;
                        }(),
                        std::nullopt),
                    identity)) {
                new_packed_value_name_.fill('\0');
                std::snprintf(new_packed_storage_type_.data(),
                              new_packed_storage_type_.size(),
                              "%s",
                              "std::uint32_t");
                new_packed_byte_order_ = 0;
                new_packed_bit_order_ = 0;
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

void PlannerUi::draw_new_integer_scalar_dialog() {
    if (open_new_integer_scalar_dialog_) {
        ImGui::OpenPopup("New integer scalar");
        open_new_integer_scalar_dialog_ = false;
    }
    if (!ImGui::BeginPopupModal("New integer scalar", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
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

    if (pending_packed_integer_scalar_binding_.has_value()) {
        ImGui::TextWrapped(
            "Create a shared integer-scalar domain and bind packed field '%s' to it. Creation and "
            "binding are separate undoable history steps.",
            text(pending_packed_integer_scalar_binding_->field_name).c_str());
        ImGui::Separator();
    }

    auto const& modules{document_->manifest().modules};
    auto first_scalar_module{std::optional<std::size_t>{}};
    for (std::size_t index{}; index < modules.size(); ++index) {
        if ((modules[index].kind() == lispb::codegen::ModuleKind::normal)) {
            first_scalar_module = index;
            break;
        }
    }
    if (!first_scalar_module.has_value()) {
        ImGui::TextDisabled("The target has no ordinary module to receive a new declaration.");
    } else {
        if (new_integer_scalar_module_index_ >= modules.size() ||
            !(modules[new_integer_scalar_module_index_].kind() ==
              lispb::codegen::ModuleKind::normal)) {
            new_integer_scalar_module_index_ = *first_scalar_module;
        }
        auto const& selected_module{modules[new_integer_scalar_module_index_]};
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
                        index == new_integer_scalar_module_index_)) {
                    new_integer_scalar_module_index_ = index;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::InputText("Name", new_integer_scalar_name_.data(), new_integer_scalar_name_.size());
        ImGui::Checkbox("Signed domain", &new_integer_scalar_signed_);
        ImGui::InputText(
            "Minimum", new_integer_scalar_minimum_.data(), new_integer_scalar_minimum_.size());
        ImGui::InputText(
            "Maximum", new_integer_scalar_maximum_.data(), new_integer_scalar_maximum_.size());
        ImGui::Checkbox("Auto bit width", &new_integer_scalar_width_auto_);
        ImGui::BeginDisabled(new_integer_scalar_width_auto_);
        ImGui::InputScalar(
            "Bit width", ImGuiDataType_U32, &new_integer_scalar_bit_width_, nullptr, nullptr);
        ImGui::EndDisabled();
        ImGui::TextDisabled(
            "This declares semantic meaning only; it does not choose a physical C++ type.");

        auto const minimum{parse_integer_literal(new_integer_scalar_minimum_.data())};
        auto const maximum{parse_integer_literal(new_integer_scalar_maximum_.data())};
        auto const ready{new_integer_scalar_name_.front() != '\0' && minimum.has_value() &&
                         maximum.has_value() &&
                         (new_integer_scalar_width_auto_ || (new_integer_scalar_bit_width_ >= 1 &&
                                                             new_integer_scalar_bit_width_ <= 64))};
        ImGui::BeginDisabled(!ready);
        auto const create_label{
            pending_packed_integer_scalar_binding_.has_value() ? "Create and use" : "Create"};
        if (ImGui::Button(create_label)) {
            auto const& module{modules[new_integer_scalar_module_index_]};
            auto const name{std::string{new_integer_scalar_name_.data()}};
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
            auto selection{std::optional<TypeIdentity>{identity}};
            lispb::codegen::SchemaRange<lispb::codegen::PackedNamedCodeId> named_codes{};
            if (pending_packed_integer_scalar_binding_.has_value()) {
                auto const& binding{*pending_packed_integer_scalar_binding_};
                if (auto const packed_info{
                        document_->declarations().find(binding.packed_declaration)};
                    packed_info.has_value()) {
                    selection = document_->declarations()
                                    .identity[document_->declarations().index(*packed_info)];
                }
                if (auto const packed_schema{document_->declaration_handle(
                        binding.packed_declaration, lispb::codegen::DeclarationKind::packed_value)};
                    packed_schema.has_value()) {
                    auto const segment{std::ranges::find_if(
                        declarations().items(
                            document_->storage()
                                .declarations.packed_values
                                .segments[document_->storage().declarations.packed_values.index(
                                    *packed_schema)]),
                        [&](auto const& candidate) {
                            return packed_name(candidate) == text(binding.field_name);
                        })};
                    if (segment !=
                        declarations()
                            .items(
                                document_->storage()
                                    .declarations.packed_values
                                    .segments[document_->storage().declarations.packed_values.index(
                                        *packed_schema)])
                            .end()) {
                        if (auto const field{
                                ((*segment).kind() == lispb::codegen::PackedSegmentKind::field
                                     ? std::optional{*segment}
                                     : std::nullopt)}) {
                            named_codes =
                                declarations()
                                    .packed_fields
                                    .named_codes[declarations().packed_fields.index(*field)];
                        }
                    }
                }
            }
            if (apply_document_edit(
                    document_->create_declaration(
                        id,
                        new_integer_scalar_module_index_,
                        [&] {
                            auto const new_integer_scalars{declarations().integer_scalars.append()};
                            declarations()
                                .integer_scalars
                                .name[declarations().integer_scalars.index(new_integer_scalars)] =
                                strings().intern(name);
                            declarations()
                                .integer_scalars.signedness[declarations().integer_scalars.index(
                                    new_integer_scalars)] = new_integer_scalar_signed_;
                            declarations()
                                .integer_scalars.minimum_value[declarations().integer_scalars.index(
                                    new_integer_scalars)] = *minimum;
                            declarations()
                                .integer_scalars.maximum_value[declarations().integer_scalars.index(
                                    new_integer_scalars)] = *maximum;
                            declarations()
                                .integer_scalars.bit_width[declarations().integer_scalars.index(
                                    new_integer_scalars)] =
                                new_integer_scalar_width_auto_
                                    ? std::nullopt
                                    : std::optional{new_integer_scalar_bit_width_};
                            declarations()
                                .integer_scalars.named_codes[declarations().integer_scalars.index(
                                    new_integer_scalars)] = named_codes;
                            declarations()
                                .integer_scalars.relationship[declarations().integer_scalars.index(
                                    new_integer_scalars)] = std::nullopt;
                            declarations()
                                .integer_scalars.cpp_emission[declarations().integer_scalars.index(
                                    new_integer_scalars)] =
                                lispb::codegen::IntegerScalarCppEmission::none;
                            declarations()
                                .integer_scalars.cpp_type[declarations().integer_scalars.index(
                                    new_integer_scalars)] = std::nullopt;
                            return new_integer_scalars;
                        }(),
                        std::nullopt),
                    selection) &&
                bind_new_type_to_packed_field(identity, pending_packed_integer_scalar_binding_)) {
                new_integer_scalar_name_.fill('\0');
                std::snprintf(new_integer_scalar_minimum_.data(),
                              new_integer_scalar_minimum_.size(),
                              "%s",
                              "0");
                std::snprintf(new_integer_scalar_maximum_.data(),
                              new_integer_scalar_maximum_.size(),
                              "%s",
                              "255");
                new_integer_scalar_signed_ = false;
                new_integer_scalar_width_auto_ = true;
                new_integer_scalar_bit_width_ = 8;
                selected_integer_scalar_code_.clear();
                pending_packed_integer_scalar_binding_.reset();
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (!minimum.has_value() || !maximum.has_value()) {
            ImGui::TextDisabled(
                "Minimum and maximum must be signed decimal or hexadecimal values.");
        }
    }
    if (ImGui::Button("Cancel")) {
        pending_packed_integer_scalar_binding_.reset();
        ImGui::CloseCurrentPopup();
    }
    if (!schema_edit_message_.empty()) {
        ImGui::TextWrapped("%s", schema_edit_message_.c_str());
    }
    ImGui::EndPopup();
}

void PlannerUi::draw_new_linear_quantized_dialog() {
    if (open_new_linear_quantized_dialog_) {
        ImGui::OpenPopup("New linear quantization");
        open_new_linear_quantized_dialog_ = false;
    }
    if (!ImGui::BeginPopupModal(
            "New linear quantization", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
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
        if (new_linear_quantized_module_index_ >= modules.size() ||
            !(modules[new_linear_quantized_module_index_].kind() ==
              lispb::codegen::ModuleKind::normal)) {
            new_linear_quantized_module_index_ = *first_module;
        }
        auto const& selected_module{modules[new_linear_quantized_module_index_]};
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
                        index == new_linear_quantized_module_index_)) {
                    new_linear_quantized_module_index_ = index;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::InputText(
            "Name", new_linear_quantized_name_.data(), new_linear_quantized_name_.size());
        if (new_linear_quantized_source_.front() == '\0') {
            auto const candidate_types{document_->types().types()};
            for (std::size_t type_index{}, type_count{candidate_types.size()};
                 type_index < type_count;
                 ++type_index) {
                auto const& node{candidate_types[type_index]};
                if (integer_domain(document_->types(),
                                   TypeId{static_cast<std::uint32_t>(type_index)})
                        .has_value()) {
                    std::snprintf(
                        new_linear_quantized_source_.data(),
                        new_linear_quantized_source_.size(),
                        "%s",
                        integer_source_reference(analysis_session_.inputs.workspace.types(), node)
                            .c_str());
                    break;
                }
            }
        }
        auto const source_label{new_linear_quantized_source_.front() == '\0'
                                    ? "Select integer scalar"
                                    : new_linear_quantized_source_.data()};
        if (ImGui::BeginCombo("Semantic source", source_label)) {
            auto const candidate_types{document_->types().types()};
            for (std::size_t type_index{}, type_count{candidate_types.size()};
                 type_index < type_count;
                 ++type_index) {
                auto const& node{candidate_types[type_index]};
                if (!integer_domain(document_->types(),
                                    TypeId{static_cast<std::uint32_t>(type_index)})
                         .has_value()) {
                    continue;
                }
                auto const selected{
                    integer_source_reference(analysis_session_.inputs.workspace.types(), node) ==
                    new_linear_quantized_source_.data()};
                if (ImGui::Selectable(
                        integer_source_reference(analysis_session_.inputs.workspace.types(), node)
                            .c_str(),
                        selected)) {
                    std::snprintf(
                        new_linear_quantized_source_.data(),
                        new_linear_quantized_source_.size(),
                        "%s",
                        integer_source_reference(analysis_session_.inputs.workspace.types(), node)
                            .c_str());
                }
            }
            ImGui::EndCombo();
        }
        ImGui::InputScalar("Encoded bit width",
                           ImGuiDataType_U32,
                           &new_linear_quantized_bit_width_,
                           nullptr,
                           nullptr,
                           "%u");
        ImGui::InputScalar("Reserved codes",
                           ImGuiDataType_U64,
                           &new_linear_quantized_reserved_codes_,
                           nullptr,
                           nullptr,
                           "%llu");
        constexpr std::array clipping_labels{"Reject", "Clamp"};
        ImGui::Combo("Out-of-range values",
                     &new_linear_quantized_clipping_,
                     clipping_labels.data(),
                     static_cast<int>(clipping_labels.size()));
        ImGui::TextDisabled(
            "Linear endpoint mapping; encoded width is not a standalone ABI sizeof.");

        auto maximum_reserved{std::optional<std::uint64_t>{}};
        if (new_linear_quantized_bit_width_ >= 1 && new_linear_quantized_bit_width_ <= 64) {
            maximum_reserved = new_linear_quantized_bit_width_ == 64
                                 ? (std::numeric_limits<std::uint64_t>::max)() - 1
                                 : (std::uint64_t{1} << new_linear_quantized_bit_width_) - 2;
        }
        auto const ready{new_linear_quantized_name_.front() != '\0' &&
                         new_linear_quantized_source_.front() != '\0' &&
                         maximum_reserved.has_value() &&
                         new_linear_quantized_reserved_codes_ <= *maximum_reserved};
        ImGui::BeginDisabled(!ready);
        if (ImGui::Button("Create")) {
            auto const name{std::string{new_linear_quantized_name_.data()}};
            auto const identity{TypeIdentity{
                .origin = TypeOrigin::declaration,
                .module_name =
                    declarations()
                        .module_settings_store.name[declarations().module_settings_store.index(
                            declarations().normal_modules.settings
                                [declarations().normal_modules.index(selected_module)])],
                .namespace_name =
                    declarations()
                        .module_settings_store
                        .namespace_name[declarations().module_settings_store.index(
                            declarations().normal_modules.settings
                                [declarations().normal_modules.index(selected_module)])]
                        .value_or(lispb::IdentifierId{}),
                .name = strings().intern(name)}};
            auto const id{document_->allocate_declaration_id()};
            if (apply_document_edit(
                    document_->create_declaration(
                        id,
                        new_linear_quantized_module_index_,
                        [&] {
                            auto const new_linear_quantizeds{
                                declarations().linear_quantizeds.append()};
                            declarations()
                                .linear_quantizeds.name[declarations().linear_quantizeds.index(
                                    new_linear_quantizeds)] = strings().intern(name);
                            declarations()
                                .linear_quantizeds.source[declarations().linear_quantizeds.index(
                                    new_linear_quantizeds)] = [&] {
                                auto const new_type_refs{declarations().type_refs.append()};
                                declarations()
                                    .type_refs.name[declarations().type_refs.index(new_type_refs)] =
                                    strings().intern(new_linear_quantized_source_.data());
                                declarations().type_refs.suffix[declarations().type_refs.index(
                                    new_type_refs)] = {};
                                declarations().type_refs.nested[declarations().type_refs.index(
                                    new_type_refs)] = std::nullopt;
                                return new_type_refs;
                            }();
                            declarations()
                                .linear_quantizeds.bit_width[declarations().linear_quantizeds.index(
                                    new_linear_quantizeds)] = new_linear_quantized_bit_width_;
                            declarations().linear_quantizeds.reserved_codes
                                [declarations().linear_quantizeds.index(new_linear_quantizeds)] =
                                new_linear_quantized_reserved_codes_;
                            declarations()
                                .linear_quantizeds.clipping[declarations().linear_quantizeds.index(
                                    new_linear_quantizeds)] =
                                new_linear_quantized_clipping_ == 0
                                    ? lispb::codegen::QuantizationClipping::reject
                                    : lispb::codegen::QuantizationClipping::clamp;
                            return new_linear_quantizeds;
                        }(),
                        std::nullopt),
                    identity)) {
                new_linear_quantized_name_.fill('\0');
                new_linear_quantized_bit_width_ = 8;
                new_linear_quantized_reserved_codes_ = 0;
                new_linear_quantized_clipping_ = 0;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (maximum_reserved.has_value() &&
            new_linear_quantized_reserved_codes_ > *maximum_reserved) {
            ImGui::TextDisabled("At least two usable codes are required.");
        }
    }
    if (ImGui::Button("Cancel")) {
        ImGui::CloseCurrentPopup();
    }
    if (!schema_edit_message_.empty()) {
        ImGui::TextWrapped("%s", schema_edit_message_.c_str());
    }
    ImGui::EndPopup();
}

void PlannerUi::draw_new_integer_varint_dialog() {
    if (open_new_integer_varint_dialog_) {
        ImGui::OpenPopup("New integer varint");
        open_new_integer_varint_dialog_ = false;
    }
    if (!ImGui::BeginPopupModal("New integer varint", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
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
        if (new_integer_varint_module_index_ >= modules.size() ||
            !(modules[new_integer_varint_module_index_].kind() ==
              lispb::codegen::ModuleKind::normal)) {
            new_integer_varint_module_index_ = *first_module;
        }
        auto const& selected_module{modules[new_integer_varint_module_index_]};
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
                        index == new_integer_varint_module_index_)) {
                    new_integer_varint_module_index_ = index;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::InputText("Name", new_integer_varint_name_.data(), new_integer_varint_name_.size());
        if (new_integer_varint_source_.front() == '\0') {
            auto const candidate_types{document_->types().types()};
            for (std::size_t type_index{}, type_count{candidate_types.size()};
                 type_index < type_count;
                 ++type_index) {
                auto const& node{candidate_types[type_index]};
                if (auto const scalar{integer_domain(
                        document_->types(), TypeId{static_cast<std::uint32_t>(type_index)})}) {
                    std::snprintf(
                        new_integer_varint_source_.data(),
                        new_integer_varint_source_.size(),
                        "%s",
                        integer_source_reference(analysis_session_.inputs.workspace.types(), node)
                            .c_str());
                    new_integer_varint_encoding_ =
                        document_->types().definitions().integer_scalars.signedness
                                [document_->types().definitions().integer_scalars.index(*scalar)]
                            ? 2
                            : 0;
                    break;
                }
            }
        }
        auto source_signed{std::optional<bool>{}};
        auto const candidate_types{document_->types().types()};
        for (std::size_t type_index{}, type_count{candidate_types.size()}; type_index < type_count;
             ++type_index) {
            auto const& node{candidate_types[type_index]};
            if (integer_source_reference(analysis_session_.inputs.workspace.types(), node) ==
                new_integer_varint_source_.data()) {
                if (auto const scalar{integer_domain(
                        document_->types(), TypeId{static_cast<std::uint32_t>(type_index)})}) {
                    source_signed =
                        document_->types().definitions().integer_scalars.signedness
                            [document_->types().definitions().integer_scalars.index(*scalar)];
                }
                break;
            }
        }
        auto const source_label{new_integer_varint_source_.front() == '\0'
                                    ? "Select integer scalar"
                                    : new_integer_varint_source_.data()};
        if (ImGui::BeginCombo("Semantic source", source_label)) {
            auto const candidate_types{document_->types().types()};
            for (std::size_t type_index{}, type_count{candidate_types.size()};
                 type_index < type_count;
                 ++type_index) {
                auto const& node{candidate_types[type_index]};
                auto const scalar{integer_domain(document_->types(),
                                                 TypeId{static_cast<std::uint32_t>(type_index)})};
                if (!scalar) {
                    continue;
                }
                auto const selected{
                    integer_source_reference(analysis_session_.inputs.workspace.types(), node) ==
                    new_integer_varint_source_.data()};
                if (ImGui::Selectable(
                        integer_source_reference(analysis_session_.inputs.workspace.types(), node)
                            .c_str(),
                        selected)) {
                    std::snprintf(
                        new_integer_varint_source_.data(),
                        new_integer_varint_source_.size(),
                        "%s",
                        integer_source_reference(analysis_session_.inputs.workspace.types(), node)
                            .c_str());
                    source_signed =
                        document_->types().definitions().integer_scalars.signedness
                            [document_->types().definitions().integer_scalars.index(*scalar)];
                    new_integer_varint_encoding_ =
                        document_->types().definitions().integer_scalars.signedness
                                [document_->types().definitions().integer_scalars.index(*scalar)]
                            ? 2
                            : 0;
                }
            }
            ImGui::EndCombo();
        }
        constexpr std::array signed_encoding_labels{"Signed varint", "ZigZag varint"};
        if (source_signed.value_or(false)) {
            auto signed_encoding{new_integer_varint_encoding_ == 1 ? 0 : 1};
            ImGui::Combo("Encoding",
                         &signed_encoding,
                         signed_encoding_labels.data(),
                         static_cast<int>(signed_encoding_labels.size()));
            new_integer_varint_encoding_ = signed_encoding == 0 ? 1 : 2;
        } else {
            new_integer_varint_encoding_ = 0;
            ImGui::TextUnformatted("Encoding: unsigned varint");
        }
        ImGui::TextDisabled(
            "Encoded size is variable; no fixed ABI sizeof or expected size is inferred.");

        auto const ready{new_integer_varint_name_.front() != '\0' &&
                         new_integer_varint_source_.front() != '\0' && source_signed.has_value()};
        ImGui::BeginDisabled(!ready);
        if (ImGui::Button("Create")) {
            auto const name{std::string{new_integer_varint_name_.data()}};
            auto const identity{TypeIdentity{
                .origin = TypeOrigin::declaration,
                .module_name =
                    declarations()
                        .module_settings_store.name[declarations().module_settings_store.index(
                            declarations().normal_modules.settings
                                [declarations().normal_modules.index(selected_module)])],
                .namespace_name =
                    declarations()
                        .module_settings_store
                        .namespace_name[declarations().module_settings_store.index(
                            declarations().normal_modules.settings
                                [declarations().normal_modules.index(selected_module)])]
                        .value_or(lispb::IdentifierId{}),
                .name = strings().intern(name)}};
            auto const encoding{new_integer_varint_encoding_ == 0
                                    ? lispb::codegen::IntegerVarintEncoding::unsigned_varint
                                : new_integer_varint_encoding_ == 1
                                    ? lispb::codegen::IntegerVarintEncoding::signed_varint
                                    : lispb::codegen::IntegerVarintEncoding::zigzag_varint};
            auto const id{document_->allocate_declaration_id()};
            if (apply_document_edit(
                    document_->create_declaration(
                        id,
                        new_integer_varint_module_index_,
                        [&] {
                            auto const new_integer_varints{declarations().integer_varints.append()};
                            declarations()
                                .integer_varints
                                .name[declarations().integer_varints.index(new_integer_varints)] =
                                strings().intern(name);
                            declarations()
                                .integer_varints.source[declarations().integer_varints.index(
                                    new_integer_varints)] = [&] {
                                auto const new_type_refs{declarations().type_refs.append()};
                                declarations()
                                    .type_refs.name[declarations().type_refs.index(new_type_refs)] =
                                    strings().intern(new_integer_varint_source_.data());
                                declarations().type_refs.suffix[declarations().type_refs.index(
                                    new_type_refs)] = {};
                                declarations().type_refs.nested[declarations().type_refs.index(
                                    new_type_refs)] = std::nullopt;
                                return new_type_refs;
                            }();
                            declarations()
                                .integer_varints.encoding[declarations().integer_varints.index(
                                    new_integer_varints)] = encoding;
                            return new_integer_varints;
                        }(),
                        std::nullopt),
                    identity)) {
                new_integer_varint_name_.fill('\0');
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndDisabled();
    }
    if (ImGui::Button("Cancel")) {
        ImGui::CloseCurrentPopup();
    }
    if (!schema_edit_message_.empty()) {
        ImGui::TextWrapped("%s", schema_edit_message_.c_str());
    }
    ImGui::EndPopup();
}

void PlannerUi::draw_new_fixed_point_dialog() {
    if (open_new_fixed_point_dialog_) {
        ImGui::OpenPopup("New fixed point");
        open_new_fixed_point_dialog_ = false;
    }
    if (!ImGui::BeginPopupModal("New fixed point", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    if (!document_.has_value()) {
        ImGui::TextDisabled("No editable LispB document is loaded.");
        if (ImGui::Button("Close")) {
            pending_packed_fixed_point_binding_.reset();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return;
    }

    if (pending_packed_fixed_point_binding_.has_value()) {
        ImGui::TextWrapped("Create a shared fixed-point representation and bind packed field '%s' "
                           "to it. Creation and binding are separate undoable history steps.",
                           text(pending_packed_fixed_point_binding_->field_name).c_str());
        ImGui::Separator();
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
        if (new_fixed_point_module_index_ >= modules.size() ||
            !(modules[new_fixed_point_module_index_].kind() ==
              lispb::codegen::ModuleKind::normal)) {
            new_fixed_point_module_index_ = *first_module;
        }
        auto const& selected_module{modules[new_fixed_point_module_index_]};
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
                        index == new_fixed_point_module_index_)) {
                    new_fixed_point_module_index_ = index;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::InputText("Name", new_fixed_point_name_.data(), new_fixed_point_name_.size());
        ImGui::Checkbox("Signed", &new_fixed_point_signed_);
        ImGui::InputScalar(
            "Total width", ImGuiDataType_U32, &new_fixed_point_total_bits_, nullptr, nullptr, "%u");
        ImGui::InputScalar("Fractional width",
                           ImGuiDataType_U32,
                           &new_fixed_point_fractional_bits_,
                           nullptr,
                           nullptr,
                           "%u");
        constexpr std::array rounding_labels{"Nearest even", "Toward zero"};
        ImGui::Combo("Rounding",
                     &new_fixed_point_rounding_,
                     rounding_labels.data(),
                     static_cast<int>(rounding_labels.size()));
        ImGui::InputText("Minimum value (optional)",
                         new_fixed_point_minimum_.data(),
                         new_fixed_point_minimum_.size());
        ImGui::InputText("Maximum value (optional)",
                         new_fixed_point_maximum_.data(),
                         new_fixed_point_maximum_.size());

        auto const total_valid{new_fixed_point_total_bits_ >= 1 &&
                               new_fixed_point_total_bits_ <= 64};
        auto const maximum_fractional{
            total_valid ? new_fixed_point_total_bits_ - (new_fixed_point_signed_ ? 1U : 0U) : 0U};
        auto const widths_valid{total_valid &&
                                new_fixed_point_fractional_bits_ <= maximum_fractional};
        if (!widths_valid) {
            ImGui::TextDisabled(new_fixed_point_signed_
                                    ? "Fractional width must leave one sign bit."
                                    : "Fractional width cannot exceed total width.");
        }
        ImGui::TextDisabled(
            "Encoded bits are representation facts; no standalone ABI sizeof is inferred.");

        auto const ready{new_fixed_point_name_.front() != '\0' && widths_valid};
        ImGui::BeginDisabled(!ready);
        if (ImGui::Button(pending_packed_fixed_point_binding_.has_value() ? "Create and use"
                                                                          : "Create")) {
            auto const name{std::string{new_fixed_point_name_.data()}};
            auto const identity{TypeIdentity{
                .origin = TypeOrigin::declaration,
                .module_name =
                    declarations()
                        .module_settings_store.name[declarations().module_settings_store.index(
                            declarations().normal_modules.settings
                                [declarations().normal_modules.index(selected_module)])],
                .namespace_name =
                    declarations()
                        .module_settings_store
                        .namespace_name[declarations().module_settings_store.index(
                            declarations().normal_modules.settings
                                [declarations().normal_modules.index(selected_module)])]
                        .value_or(lispb::IdentifierId{}),
                .name = strings().intern(name)}};
            auto const id{document_->allocate_declaration_id()};
            auto selection{std::optional<TypeIdentity>{identity}};
            if (pending_packed_fixed_point_binding_.has_value()) {
                if (auto const packed_info{document_->declarations().find(
                        pending_packed_fixed_point_binding_->packed_declaration)};
                    packed_info.has_value()) {
                    selection = document_->declarations()
                                    .identity[document_->declarations().index(*packed_info)];
                }
            }
            if (apply_document_edit(
                    document_->create_declaration(
                        id,
                        new_fixed_point_module_index_,
                        [&] {
                            auto const new_fixed_points{declarations().fixed_points.append()};
                            declarations().fixed_points.name[declarations().fixed_points.index(
                                new_fixed_points)] = strings().intern(name);
                            declarations()
                                .fixed_points
                                .signedness[declarations().fixed_points.index(new_fixed_points)] =
                                new_fixed_point_signed_;
                            declarations()
                                .fixed_points
                                .total_bits[declarations().fixed_points.index(new_fixed_points)] =
                                new_fixed_point_total_bits_;
                            declarations()
                                .fixed_points.fractional_bits[declarations().fixed_points.index(
                                    new_fixed_points)] = new_fixed_point_fractional_bits_;
                            declarations().fixed_points.rounding[declarations().fixed_points.index(
                                new_fixed_points)] =
                                new_fixed_point_rounding_ == 0
                                    ? lispb::codegen::FixedPointRounding::nearest_even
                                    : lispb::codegen::FixedPointRounding::toward_zero;
                            declarations()
                                .fixed_points.minimum_value[declarations().fixed_points.index(
                                    new_fixed_points)] =
                                store_text(new_fixed_point_minimum_.front() == '\0'
                                               ? std::nullopt
                                               : std::optional{
                                                     std::string{new_fixed_point_minimum_.data()}});
                            declarations()
                                .fixed_points.maximum_value[declarations().fixed_points.index(
                                    new_fixed_points)] =
                                store_text(new_fixed_point_maximum_.front() == '\0'
                                               ? std::nullopt
                                               : std::optional{
                                                     std::string{new_fixed_point_maximum_.data()}});
                            return new_fixed_points;
                        }(),
                        std::nullopt),
                    selection) &&
                bind_new_type_to_packed_field(identity, pending_packed_fixed_point_binding_)) {
                new_fixed_point_name_.fill('\0');
                new_fixed_point_signed_ = true;
                new_fixed_point_total_bits_ = 16;
                new_fixed_point_fractional_bits_ = 8;
                new_fixed_point_rounding_ = 0;
                new_fixed_point_minimum_.fill('\0');
                new_fixed_point_maximum_.fill('\0');
                pending_packed_fixed_point_binding_.reset();
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndDisabled();
    }
    if (ImGui::Button("Cancel")) {
        pending_packed_fixed_point_binding_.reset();
        ImGui::CloseCurrentPopup();
    }
    if (!schema_edit_message_.empty()) {
        ImGui::TextWrapped("%s", schema_edit_message_.c_str());
    }
    ImGui::EndPopup();
}

void PlannerUi::draw_new_mini_float_dialog() {
    if (open_new_mini_float_dialog_) {
        ImGui::OpenPopup("New mini float");
        open_new_mini_float_dialog_ = false;
    }
    if (!ImGui::BeginPopupModal("New mini float", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    if (!document_.has_value()) {
        ImGui::TextDisabled("No editable LispB document is loaded.");
        if (ImGui::Button("Close")) {
            pending_packed_mini_float_binding_.reset();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return;
    }

    if (pending_packed_mini_float_binding_.has_value()) {
        ImGui::TextWrapped("Create a shared mini-float representation and bind packed field '%s' "
                           "to it. Creation and binding are separate undoable history steps.",
                           text(pending_packed_mini_float_binding_->field_name).c_str());
        ImGui::Separator();
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
        if (new_mini_float_module_index_ >= modules.size() ||
            !(modules[new_mini_float_module_index_].kind() == lispb::codegen::ModuleKind::normal)) {
            new_mini_float_module_index_ = *first_module;
        }
        auto const& selected_module{modules[new_mini_float_module_index_]};
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
                        index == new_mini_float_module_index_)) {
                    new_mini_float_module_index_ = index;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::InputText("Name", new_mini_float_name_.data(), new_mini_float_name_.size());
        ImGui::InputScalar(
            "Sign bits", ImGuiDataType_U32, &new_mini_float_sign_bits_, nullptr, nullptr, "%u");
        ImGui::InputScalar("Exponent bits",
                           ImGuiDataType_U32,
                           &new_mini_float_exponent_bits_,
                           nullptr,
                           nullptr,
                           "%u");
        ImGui::InputScalar("Significand bits",
                           ImGuiDataType_U32,
                           &new_mini_float_significand_bits_,
                           nullptr,
                           nullptr,
                           "%u");
        ImGui::InputScalar("Exponent bias",
                           ImGuiDataType_S32,
                           &new_mini_float_exponent_bias_,
                           nullptr,
                           nullptr,
                           "%d");

        auto const widths_valid{
            new_mini_float_sign_bits_ <= 1 && new_mini_float_exponent_bits_ >= 2 &&
            new_mini_float_exponent_bits_ <= 15 && new_mini_float_significand_bits_ <= 62 &&
            static_cast<std::uint64_t>(new_mini_float_sign_bits_) + new_mini_float_exponent_bits_ +
                    new_mini_float_significand_bits_ <=
                64};
        auto const bias_valid{new_mini_float_exponent_bias_ >= -32'768 &&
                              new_mini_float_exponent_bias_ <= 32'767};
        if (!widths_valid) {
            ImGui::TextDisabled(
                "Sign must be 0..1, exponent 2..15, significand 0..62, and total at most 64.");
        }
        if (!bias_valid) {
            ImGui::TextDisabled("Exponent bias must be in the range -32768..32767.");
        }
        ImGui::TextDisabled(
            "All-zero/all-one exponents encode zero/subnormal and infinity/NaN roles.");
        ImGui::TextDisabled(
            "Encoded bits do not imply native arithmetic, byte order, or an ABI sizeof.");

        auto const ready{new_mini_float_name_.front() != '\0' && widths_valid && bias_valid};
        ImGui::BeginDisabled(!ready);
        if (ImGui::Button(pending_packed_mini_float_binding_.has_value() ? "Create and use"
                                                                         : "Create")) {
            auto const name{std::string{new_mini_float_name_.data()}};
            auto const identity{TypeIdentity{
                .origin = TypeOrigin::declaration,
                .module_name =
                    declarations()
                        .module_settings_store.name[declarations().module_settings_store.index(
                            declarations().normal_modules.settings
                                [declarations().normal_modules.index(selected_module)])],
                .namespace_name =
                    declarations()
                        .module_settings_store
                        .namespace_name[declarations().module_settings_store.index(
                            declarations().normal_modules.settings
                                [declarations().normal_modules.index(selected_module)])]
                        .value_or(lispb::IdentifierId{}),
                .name = strings().intern(name)}};
            auto const id{document_->allocate_declaration_id()};
            auto selection{std::optional<TypeIdentity>{identity}};
            if (pending_packed_mini_float_binding_.has_value()) {
                if (auto const packed_info{document_->declarations().find(
                        pending_packed_mini_float_binding_->packed_declaration)};
                    packed_info.has_value()) {
                    selection = document_->declarations()
                                    .identity[document_->declarations().index(*packed_info)];
                }
            }
            if (apply_document_edit(
                    document_->create_declaration(
                        id,
                        new_mini_float_module_index_,
                        [&] {
                            auto const new_mini_floats{declarations().mini_floats.append()};
                            declarations().mini_floats.name[declarations().mini_floats.index(
                                new_mini_floats)] = strings().intern(name);
                            declarations().mini_floats.sign_bits[declarations().mini_floats.index(
                                new_mini_floats)] = new_mini_float_sign_bits_;
                            declarations()
                                .mini_floats
                                .exponent_bits[declarations().mini_floats.index(new_mini_floats)] =
                                new_mini_float_exponent_bits_;
                            declarations()
                                .mini_floats.significand_bits[declarations().mini_floats.index(
                                    new_mini_floats)] = new_mini_float_significand_bits_;
                            declarations()
                                .mini_floats
                                .exponent_bias[declarations().mini_floats.index(new_mini_floats)] =
                                new_mini_float_exponent_bias_;
                            return new_mini_floats;
                        }(),
                        std::nullopt),
                    selection) &&
                bind_new_type_to_packed_field(identity, pending_packed_mini_float_binding_)) {
                new_mini_float_name_.fill('\0');
                new_mini_float_sign_bits_ = 1;
                new_mini_float_exponent_bits_ = 5;
                new_mini_float_significand_bits_ = 10;
                new_mini_float_exponent_bias_ = 15;
                pending_packed_mini_float_binding_.reset();
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndDisabled();
    }
    if (ImGui::Button("Cancel")) {
        pending_packed_mini_float_binding_.reset();
        ImGui::CloseCurrentPopup();
    }
    if (!schema_edit_message_.empty()) {
        ImGui::TextWrapped("%s", schema_edit_message_.c_str());
    }
    ImGui::EndPopup();
}

void PlannerUi::draw_new_optional_sentinel_dialog() {
    if (open_new_optional_sentinel_dialog_) {
        ImGui::OpenPopup("New sentinel-encoded optional");
        open_new_optional_sentinel_dialog_ = false;
    }
    if (!ImGui::BeginPopupModal(
            "New sentinel-encoded optional", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
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

    std::vector<TypeId> sources;
    auto const types{analysis_session_.inputs.workspace.types().types()};
    for (std::size_t index{}; index < types.size(); ++index) {
        auto const scalar{integer_domain(analysis_session_.inputs.workspace.types(),
                                         TypeId{static_cast<std::uint32_t>(index)})};
        if (static_cast<bool>(scalar) &&
            std::ranges::any_of(
                declarations().items(
                    analysis_session_.inputs.workspace.types()
                        .definitions()
                        .integer_scalars.named_codes[analysis_session_.inputs.workspace.types()
                                                         .definitions()
                                                         .integer_scalars.index(*scalar)]),
                [&](auto id) {
                    return declarations()
                        .packed_named_codes.sentinel[declarations().packed_named_codes.index(id)];
                })) {
            sources.push_back(TypeId{static_cast<std::uint32_t>(index)});
        }
    }

    if (!first_module.has_value()) {
        ImGui::TextDisabled("The target has no ordinary module to receive a new declaration.");
    } else if (sources.empty()) {
        ImGui::TextDisabled(
            "No integer scalar has a named sentinel code. Add one before creating this encoding.");
    } else {
        if (new_optional_sentinel_module_index_ >= modules.size() ||
            !(modules[new_optional_sentinel_module_index_].kind() ==
              lispb::codegen::ModuleKind::normal)) {
            new_optional_sentinel_module_index_ = *first_module;
        }
        auto const& selected_module{modules[new_optional_sentinel_module_index_]};
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
                        index == new_optional_sentinel_module_index_)) {
                    new_optional_sentinel_module_index_ = index;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::InputText(
            "Name", new_optional_sentinel_name_.data(), new_optional_sentinel_name_.size());
        auto selected_source{std::ranges::find_if(sources, [&](TypeId const type) {
            return integer_source_reference(analysis_session_.inputs.workspace.types(),

                                            type) == new_optional_sentinel_source_.data();
        })};
        if (selected_source == sources.end()) {
            selected_source = sources.begin();
            auto const& node{*selected_source};
            std::snprintf(
                new_optional_sentinel_source_.data(),
                new_optional_sentinel_source_.size(),
                "%s",
                integer_source_reference(analysis_session_.inputs.workspace.types(), node).c_str());
            auto const scalar{
                *integer_domain(analysis_session_.inputs.workspace.types(), *selected_source)};
            auto const sentinel{std::ranges::find_if(
                declarations().items(
                    analysis_session_.inputs.workspace.types()
                        .definitions()
                        .integer_scalars.named_codes[analysis_session_.inputs.workspace.types()
                                                         .definitions()
                                                         .integer_scalars.index(scalar)]),
                [&](auto id) {
                    return declarations()
                        .packed_named_codes.sentinel[declarations().packed_named_codes.index(id)];
                })};
            std::snprintf(
                new_optional_sentinel_code_.data(),
                new_optional_sentinel_code_.size(),
                "%s",
                text(declarations().packed_named_codes.name[declarations().packed_named_codes.index(
                         *sentinel)])
                    .c_str());
        }

        auto selected_source_type{*selected_source};
        if (ImGui::BeginCombo("Semantic source",
                              integer_source_reference(analysis_session_.inputs.workspace.types(),
                                                       selected_source_type)
                                  .c_str())) {
            for (auto const type : sources) {
                auto const& candidate{type};
                auto const selected{type == selected_source_type};
                if (ImGui::Selectable(integer_source_reference(
                                          analysis_session_.inputs.workspace.types(), candidate)
                                          .c_str(),
                                      selected)) {
                    selected_source_type = type;
                    std::snprintf(new_optional_sentinel_source_.data(),
                                  new_optional_sentinel_source_.size(),
                                  "%s",
                                  integer_source_reference(
                                      analysis_session_.inputs.workspace.types(), candidate)
                                      .c_str());
                    auto const scalar{
                        *integer_domain(analysis_session_.inputs.workspace.types(), type)};
                    auto const sentinel{std::ranges::find_if(
                        declarations().items(
                            analysis_session_.inputs.workspace.types()
                                .definitions()
                                .integer_scalars
                                .named_codes[analysis_session_.inputs.workspace.types()
                                                 .definitions()
                                                 .integer_scalars.index(scalar)]),
                        [&](auto id) {
                            return declarations()
                                .packed_named_codes
                                .sentinel[declarations().packed_named_codes.index(id)];
                        })};
                    std::snprintf(
                        new_optional_sentinel_code_.data(),
                        new_optional_sentinel_code_.size(),
                        "%s",
                        text(declarations()
                                 .packed_named_codes
                                 .name[declarations().packed_named_codes.index(*sentinel)])
                            .c_str());
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        auto const source_scalar{
            *integer_domain(analysis_session_.inputs.workspace.types(), selected_source_type)};
        auto selected_code{std::ranges::find_if(
            declarations().items(
                analysis_session_.inputs.workspace.types().definitions().integer_scalars.named_codes
                    [analysis_session_.inputs.workspace.types().definitions().integer_scalars.index(
                        source_scalar)]),
            [&](auto id) {
                auto const code{id};
                return declarations()
                           .packed_named_codes
                           .sentinel[declarations().packed_named_codes.index(code)] &&
                       text(declarations()
                                .packed_named_codes
                                .name[declarations().packed_named_codes.index(code)]) ==
                           new_optional_sentinel_code_.data();
            })};
        if (selected_code ==
            declarations()
                .items(analysis_session_.inputs.workspace.types()
                           .definitions()
                           .integer_scalars.named_codes[analysis_session_.inputs.workspace.types()
                                                            .definitions()
                                                            .integer_scalars.index(source_scalar)])
                .end()) {
            selected_code = std::ranges::find_if(
                declarations().items(
                    analysis_session_.inputs.workspace.types()
                        .definitions()
                        .integer_scalars.named_codes[analysis_session_.inputs.workspace.types()
                                                         .definitions()
                                                         .integer_scalars.index(source_scalar)]),
                [&](auto id) {
                    return declarations()
                        .packed_named_codes.sentinel[declarations().packed_named_codes.index(id)];
                });
            std::snprintf(
                new_optional_sentinel_code_.data(),
                new_optional_sentinel_code_.size(),
                "%s",
                text(declarations().packed_named_codes.name[declarations().packed_named_codes.index(
                         *selected_code)])
                    .c_str());
        }
        if (ImGui::BeginCombo(
                "Absence sentinel",
                text(declarations().packed_named_codes.name[declarations().packed_named_codes.index(
                         *selected_code)])
                    .c_str())) {
            for (auto const id : declarations().items(
                     analysis_session_.inputs.workspace.types()
                         .definitions()
                         .integer_scalars.named_codes[analysis_session_.inputs.workspace.types()
                                                          .definitions()
                                                          .integer_scalars.index(source_scalar)])) {
                auto const code{id};
                if (!declarations()
                         .packed_named_codes
                         .sentinel[declarations().packed_named_codes.index(code)]) {
                    continue;
                }
                auto const selected{
                    declarations()
                        .packed_named_codes.name[declarations().packed_named_codes.index(code)] ==
                    declarations().packed_named_codes.name[declarations().packed_named_codes.index(
                        *selected_code)]};
                if (ImGui::Selectable(text(declarations()
                                               .packed_named_codes
                                               .name[declarations().packed_named_codes.index(code)])
                                          .c_str(),
                                      selected)) {
                    std::snprintf(new_optional_sentinel_code_.data(),
                                  new_optional_sentinel_code_.size(),
                                  "%s",
                                  text(declarations()
                                           .packed_named_codes
                                           .name[declarations().packed_named_codes.index(code)])
                                      .c_str());
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::TextDisabled(
            "The source scalar owns the live domain and code values; this declaration selects one "
            "named sentinel as the absence state.");

        auto const ready{new_optional_sentinel_name_.front() != '\0'};
        ImGui::BeginDisabled(!ready);
        if (ImGui::Button("Create")) {
            auto const name{std::string{new_optional_sentinel_name_.data()}};
            auto const identity{TypeIdentity{
                .origin = TypeOrigin::declaration,
                .module_name =
                    declarations()
                        .module_settings_store.name[declarations().module_settings_store.index(
                            declarations().normal_modules.settings
                                [declarations().normal_modules.index(selected_module)])],
                .namespace_name =
                    declarations()
                        .module_settings_store
                        .namespace_name[declarations().module_settings_store.index(
                            declarations().normal_modules.settings
                                [declarations().normal_modules.index(selected_module)])]
                        .value_or(lispb::IdentifierId{}),
                .name = strings().intern(name)}};
            auto const id{document_->allocate_declaration_id()};
            if (apply_document_edit(
                    document_->create_declaration(
                        id,
                        new_optional_sentinel_module_index_,
                        [&] {
                            auto const new_optional_sentinels{
                                declarations().optional_sentinels.append()};
                            declarations()
                                .optional_sentinels.name[declarations().optional_sentinels.index(
                                    new_optional_sentinels)] = strings().intern(name);
                            declarations()
                                .optional_sentinels.source[declarations().optional_sentinels.index(
                                    new_optional_sentinels)] = [&] {
                                auto const new_type_refs{declarations().type_refs.append()};
                                declarations()
                                    .type_refs.name[declarations().type_refs.index(new_type_refs)] =
                                    strings().intern(new_optional_sentinel_source_.data());
                                declarations().type_refs.suffix[declarations().type_refs.index(
                                    new_type_refs)] = {};
                                declarations().type_refs.nested[declarations().type_refs.index(
                                    new_type_refs)] = std::nullopt;
                                return new_type_refs;
                            }();
                            declarations().optional_sentinels.sentinel
                                [declarations().optional_sentinels.index(new_optional_sentinels)] =
                                strings().intern(new_optional_sentinel_code_.data());
                            return new_optional_sentinels;
                        }(),
                        std::nullopt),
                    identity)) {
                new_optional_sentinel_name_.fill('\0');
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndDisabled();
    }
    if (ImGui::Button("Cancel")) {
        ImGui::CloseCurrentPopup();
    }
    if (!schema_edit_message_.empty()) {
        ImGui::TextWrapped("%s", schema_edit_message_.c_str());
    }
    ImGui::EndPopup();
}

void PlannerUi::draw_new_optional_presence_bit_dialog() {
    if (open_new_optional_presence_bit_dialog_) {
        ImGui::OpenPopup("New presence-bit optional");
        open_new_optional_presence_bit_dialog_ = false;
    }
    if (!ImGui::BeginPopupModal(
            "New presence-bit optional", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
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

    std::vector<TypeId> sources;
    auto const types{analysis_session_.inputs.workspace.types().types()};
    for (std::size_t index{}; index < types.size(); ++index) {
        if (integer_domain(analysis_session_.inputs.workspace.types(),
                           TypeId{static_cast<std::uint32_t>(index)})
                .has_value()) {
            sources.push_back(TypeId{static_cast<std::uint32_t>(index)});
        }
    }

    if (!first_module.has_value()) {
        ImGui::TextDisabled("The target has no ordinary module to receive a new declaration.");
    } else if (sources.empty()) {
        ImGui::TextDisabled(
            "No integer scalar is available. Add a semantic scalar before creating this encoding.");
    } else {
        if (new_optional_presence_bit_module_index_ >= modules.size() ||
            !(modules[new_optional_presence_bit_module_index_].kind() ==
              lispb::codegen::ModuleKind::normal)) {
            new_optional_presence_bit_module_index_ = *first_module;
        }
        auto const& selected_module{modules[new_optional_presence_bit_module_index_]};
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
                        index == new_optional_presence_bit_module_index_)) {
                    new_optional_presence_bit_module_index_ = index;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::InputText(
            "Name", new_optional_presence_bit_name_.data(), new_optional_presence_bit_name_.size());
        auto selected_source{std::ranges::find_if(sources, [&](TypeId const type) {
            return integer_source_reference(analysis_session_.inputs.workspace.types(),

                                            type) == new_optional_presence_bit_source_.data();
        })};
        if (selected_source == sources.end()) {
            selected_source = sources.begin();
            auto const& node{*selected_source};
            std::snprintf(
                new_optional_presence_bit_source_.data(),
                new_optional_presence_bit_source_.size(),
                "%s",
                integer_source_reference(analysis_session_.inputs.workspace.types(), node).c_str());
        }

        auto selected_source_type{*selected_source};
        if (ImGui::BeginCombo("Semantic source",
                              integer_source_reference(analysis_session_.inputs.workspace.types(),
                                                       selected_source_type)
                                  .c_str())) {
            for (auto const type : sources) {
                auto const& candidate{type};
                auto const selected{type == selected_source_type};
                if (ImGui::Selectable(integer_source_reference(
                                          analysis_session_.inputs.workspace.types(), candidate)
                                          .c_str(),
                                      selected)) {
                    selected_source_type = type;
                    std::snprintf(new_optional_presence_bit_source_.data(),
                                  new_optional_presence_bit_source_.size(),
                                  "%s",
                                  integer_source_reference(
                                      analysis_session_.inputs.workspace.types(), candidate)
                                      .c_str());
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        auto const source_scalar{
            *integer_domain(analysis_session_.inputs.workspace.types(), selected_source_type)};
        ImGui::Text(
            "Encoding: 1 presence bit + %u payload bits = %u bits/value",
            analysis_session_.inputs.workspace.types().definitions().integer_scalars.bit_width
                [analysis_session_.inputs.workspace.types().definitions().integer_scalars.index(
                    source_scalar)],
            analysis_session_.inputs.workspace.types().definitions().integer_scalars.bit_width
                    [analysis_session_.inputs.workspace.types().definitions().integer_scalars.index(
                        source_scalar)] +
                1);
        ImGui::TextDisabled(
            "Concrete byte packing and presence-bit ordering remain unspecified until placement.");

        auto const ready{new_optional_presence_bit_name_.front() != '\0'};
        ImGui::BeginDisabled(!ready);
        if (ImGui::Button("Create")) {
            auto const name{std::string{new_optional_presence_bit_name_.data()}};
            auto const identity{TypeIdentity{
                .origin = TypeOrigin::declaration,
                .module_name =
                    declarations()
                        .module_settings_store.name[declarations().module_settings_store.index(
                            declarations().normal_modules.settings
                                [declarations().normal_modules.index(selected_module)])],
                .namespace_name =
                    declarations()
                        .module_settings_store
                        .namespace_name[declarations().module_settings_store.index(
                            declarations().normal_modules.settings
                                [declarations().normal_modules.index(selected_module)])]
                        .value_or(lispb::IdentifierId{}),
                .name = strings().intern(name)}};
            auto const id{document_->allocate_declaration_id()};
            if (apply_document_edit(
                    document_->create_declaration(
                        id,
                        new_optional_presence_bit_module_index_,
                        [&] {
                            auto const new_optional_presence_bits{
                                declarations().optional_presence_bits.append()};
                            declarations()
                                .optional_presence_bits
                                .name[declarations().optional_presence_bits.index(
                                    new_optional_presence_bits)] = strings().intern(name);
                            declarations()
                                .optional_presence_bits
                                .source[declarations().optional_presence_bits.index(
                                    new_optional_presence_bits)] = [&] {
                                auto const new_type_refs{declarations().type_refs.append()};
                                declarations()
                                    .type_refs.name[declarations().type_refs.index(new_type_refs)] =
                                    strings().intern(new_optional_presence_bit_source_.data());
                                declarations().type_refs.suffix[declarations().type_refs.index(
                                    new_type_refs)] = {};
                                declarations().type_refs.nested[declarations().type_refs.index(
                                    new_type_refs)] = std::nullopt;
                                return new_type_refs;
                            }();
                            return new_optional_presence_bits;
                        }(),
                        std::nullopt),
                    identity)) {
                new_optional_presence_bit_name_.fill('\0');
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndDisabled();
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
