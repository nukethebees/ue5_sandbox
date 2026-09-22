#include "planner_ui_browser_common.hpp"

namespace ioj::layout_planner {

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
            pending_packed_enum_binding_->field_name.c_str());
        ImGui::Separator();
    }

    auto const& modules{document_->manifest().modules};
    auto first_enum_module{std::optional<std::size_t>{}};
    for (std::size_t index{}; index < modules.size(); ++index) {
        if (std::holds_alternative<codegen::NormalModuleSchema>(modules[index])) {
            first_enum_module = index;
            break;
        }
    }
    if (!first_enum_module.has_value()) {
        ImGui::TextDisabled("The target has no ordinary module to receive a new declaration.");
    } else {
        if (new_enum_module_index_ >= modules.size() ||
            !std::holds_alternative<codegen::NormalModuleSchema>(modules[new_enum_module_index_])) {
            new_enum_module_index_ = *first_enum_module;
        }
        auto const& selected_module{
            std::get<codegen::NormalModuleSchema>(modules[new_enum_module_index_])};
        if (!module_initiated_dialog_ &&
            ImGui::BeginCombo("Module", selected_module.settings.name.c_str())) {
            for (std::size_t index{}; index < modules.size(); ++index) {
                auto const* module{std::get_if<codegen::NormalModuleSchema>(&modules[index])};
                if (module == nullptr) {
                    continue;
                }
                if (ImGui::Selectable(module->settings.name.c_str(),
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
            auto const& module{
                std::get<codegen::NormalModuleSchema>(modules[new_enum_module_index_])};
            auto const name{std::string{new_enum_name_.data()}};
            auto const bit_width{new_enum_width_auto_ ? std::optional<std::uint32_t>{}
                                                      : std::optional{new_enum_bit_width_}};
            auto const signedness{new_enum_signedness_ == 0   ? std::optional<bool>{}
                                  : new_enum_signedness_ == 2 ? std::optional<bool>{true}
                                                              : std::optional<bool>{false}};
            auto const identity{
                TypeIdentity{.origin = TypeOrigin::declaration,
                             .module_name = module.settings.name,
                             .namespace_name = module.settings.namespace_name.value_or(""),
                             .name = name}};
            auto const id{document_->allocate_declaration_id()};
            auto selection{std::optional<TypeIdentity>{identity}};
            if (pending_packed_enum_binding_.has_value()) {
                if (auto const* packed_info{
                        document_->declaration(pending_packed_enum_binding_->packed_declaration)};
                    packed_info != nullptr) {
                    selection = packed_info->identity;
                }
            }
            if (apply_document_edit(
                    CreateEnum{
                        .declaration = id,
                        .module_index = new_enum_module_index_,
                        .schema =
                            codegen::EnumSchema{
                                .name = name,
                                .underlying_type = new_enum_backing_auto_
                                                     ? std::optional<codegen::TypeRef>{}
                                                     : std::optional{codegen::TypeRef{
                                                           .name = new_enum_underlying_type_.data(),
                                                           .suffix = {},
                                                           .nested = std::nullopt}},
                                .bit_width = bit_width,
                                .signedness = signedness,
                                .reflection = codegen::EnumReflection::none,
                                .values = {{.name = "Value0",
                                            .initializer = "0",
                                            .display_name = std::nullopt,
                                            .hidden = false,
                                            .serialized_name = std::nullopt}},
                                .enum_array = false,
                                .count = std::nullopt,
                                .conversions = {},
                                .export_specifier = std::nullopt,
                                .native_api = false,
                                .unreal_projection = std::nullopt},
                        .insertion_index = std::nullopt},
                    selection) &&
                bind_new_enum_to_packed_field(identity)) {
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

auto PlannerUi::bind_new_enum_to_packed_field(TypeIdentity const& enumeration) -> bool {
    if (!pending_packed_enum_binding_.has_value()) {
        return true;
    }
    auto const binding{*pending_packed_enum_binding_};
    auto const* packed_info{document_->declaration(binding.packed_declaration)};
    auto const* packed_schema{document_->packed_value_schema(binding.packed_declaration)};
    auto const packed_identity{packed_info == nullptr ? std::optional<TypeIdentity>{}
                                                      : std::optional{packed_info->identity}};
    auto rollback_creation = [&](std::string message) {
        auto const rollback{document_->undo()};
        if (rollback.has_value() && *rollback) {
            sync_document_graph(packed_identity);
            schema_edit_message_ = std::move(message) + " The new enum was rolled back.";
        } else if (!rollback.has_value()) {
            schema_edit_message_ = std::move(message) + " The new enum could not be rolled back: " +
                                   rollback.error().message;
        } else {
            schema_edit_message_ = std::move(message) +
                                   " The new enum could not be rolled back because history did "
                                   "not change.";
        }
        return false;
    };

    if (packed_info == nullptr || packed_schema == nullptr) {
        return rollback_creation("The packed declaration is no longer available.");
    }
    auto replacement{*packed_schema};
    auto const segment{std::ranges::find_if(replacement.segments, [&](auto const& candidate) {
        return codegen::packed_segment_name(candidate) == binding.field_name;
    })};
    if (segment == replacement.segments.end()) {
        return rollback_creation("The selected packed field is no longer available.");
    }
    auto* field{std::get_if<codegen::PackedFieldSchema>(&*segment)};
    if (field == nullptr) {
        return rollback_creation("The selected packed segment is no longer a field.");
    }

    field->type = codegen::TypeRef{.name = enumeration.namespace_name.empty()
                                             ? enumeration.name
                                             : enumeration.namespace_name + "::" + enumeration.name,
                                   .suffix = {},
                                   .nested = std::nullopt};
    field->kind = codegen::PackedFieldKind::enumeration;
    field->range_helper = false;
    field->minimum_value.reset();
    field->maximum_value.reset();
    field->named_codes.clear();
    if (!apply_document_edit(ReplacePackedValue{.declaration = binding.packed_declaration,
                                                .schema = std::move(replacement)},
                             packed_info->identity)) {
        return rollback_creation("The enum was valid, but binding the packed field failed: " +
                                 schema_edit_message_);
    }

    analysis_session_.inputs.selection.field = binding.field_name;
    return true;
}

auto PlannerUi::bind_new_integer_scalar_to_packed_field(TypeIdentity const& scalar) -> bool {
    if (!pending_packed_integer_scalar_binding_.has_value()) {
        return true;
    }
    auto const binding{*pending_packed_integer_scalar_binding_};
    auto const* packed_info{document_->declaration(binding.packed_declaration)};
    auto const* packed_schema{document_->packed_value_schema(binding.packed_declaration)};
    auto const packed_identity{packed_info == nullptr ? std::optional<TypeIdentity>{}
                                                      : std::optional{packed_info->identity}};
    auto rollback_creation = [&](std::string message) {
        auto const rollback{document_->undo()};
        if (rollback.has_value() && *rollback) {
            sync_document_graph(packed_identity);
            schema_edit_message_ = std::move(message) + " The new integer scalar was rolled back.";
        } else if (!rollback.has_value()) {
            schema_edit_message_ =
                std::move(message) +
                " The new integer scalar could not be rolled back: " + rollback.error().message;
        } else {
            schema_edit_message_ =
                std::move(message) +
                " The new integer scalar could not be rolled back because history did not change.";
        }
        return false;
    };

    if (packed_info == nullptr || packed_schema == nullptr) {
        return rollback_creation("The packed declaration is no longer available.");
    }
    auto const scalar_declaration{document_->find_declaration(scalar)};
    auto const* scalar_schema{scalar_declaration.has_value()
                                  ? document_->integer_scalar_schema(*scalar_declaration)
                                  : nullptr};
    if (scalar_schema == nullptr) {
        return rollback_creation("The new integer scalar is no longer available.");
    }

    auto replacement{*packed_schema};
    auto const segment{std::ranges::find_if(replacement.segments, [&](auto const& candidate) {
        return codegen::packed_segment_name(candidate) == binding.field_name;
    })};
    if (segment == replacement.segments.end()) {
        return rollback_creation("The selected packed field is no longer available.");
    }
    auto* field{std::get_if<codegen::PackedFieldSchema>(&*segment)};
    if (field == nullptr) {
        return rollback_creation("The selected packed segment is no longer a field.");
    }

    field->type = codegen::TypeRef{.name = scalar.namespace_name.empty()
                                             ? scalar.name
                                             : scalar.namespace_name + "::" + scalar.name,
                                   .suffix = {},
                                   .nested = std::nullopt};
    field->kind = scalar_schema->signedness ? codegen::PackedFieldKind::signed_integer
                                            : codegen::PackedFieldKind::unsigned_integer;
    field->range_helper = false;
    field->minimum_value.reset();
    field->maximum_value.reset();
    field->named_codes.clear();
    if (!apply_document_edit(ReplacePackedValue{.declaration = binding.packed_declaration,
                                                .schema = std::move(replacement)},
                             packed_info->identity)) {
        return rollback_creation("The integer scalar was valid, but binding the packed field "
                                 "failed: " +
                                 schema_edit_message_);
    }

    analysis_session_.inputs.selection.field = binding.field_name;
    return true;
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
        if (std::holds_alternative<codegen::NormalModuleSchema>(modules[index])) {
            first_packed_module = index;
            break;
        }
    }
    if (!first_packed_module.has_value()) {
        ImGui::TextDisabled("The target has no ordinary module to receive a new declaration.");
    } else {
        if (new_packed_module_index_ >= modules.size() ||
            !std::holds_alternative<codegen::NormalModuleSchema>(
                modules[new_packed_module_index_])) {
            new_packed_module_index_ = *first_packed_module;
        }
        auto const& selected_module{
            std::get<codegen::NormalModuleSchema>(modules[new_packed_module_index_])};
        if (!module_initiated_dialog_ &&
            ImGui::BeginCombo("Module", selected_module.settings.name.c_str())) {
            for (std::size_t index{}; index < modules.size(); ++index) {
                auto const* module{std::get_if<codegen::NormalModuleSchema>(&modules[index])};
                if (module == nullptr) {
                    continue;
                }
                if (ImGui::Selectable(module->settings.name.c_str(),
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
            auto const& module{
                std::get<codegen::NormalModuleSchema>(modules[new_packed_module_index_])};
            auto const name{std::string{new_packed_value_name_.data()}};
            auto const identity{
                TypeIdentity{.origin = TypeOrigin::declaration,
                             .module_name = module.settings.name,
                             .namespace_name = module.settings.namespace_name.value_or(""),
                             .name = name}};
            auto const id{document_->allocate_declaration_id()};
            if (apply_document_edit(
                    CreatePackedValue{
                        .declaration = id,
                        .module_index = new_packed_module_index_,
                        .schema =
                            codegen::PackedValueSchema{
                                .name = name,
                                .storage_type =
                                    codegen::TypeRef{.name = new_packed_storage_type_.data(),
                                                     .suffix = {},
                                                     .nested = std::nullopt},
                                .segments = {codegen::PackedFieldSchema{
                                    .name = "value",
                                    .type = codegen::TypeRef{.name = "std::uint8_t",
                                                             .suffix = {},
                                                             .nested = std::nullopt},
                                    .bits = 1,
                                    .kind = codegen::PackedFieldKind::unsigned_integer,
                                    .range_helper = false,
                                    .minimum_value = std::nullopt,
                                    .maximum_value = std::nullopt,
                                    .named_codes = {},
                                    .relationship = std::nullopt}},
                                .invalid_value = std::nullopt,
                                .export_specifier = std::nullopt,
                                .byte_order =
                                    new_packed_byte_order_ == 0
                                        ? std::nullopt
                                        : std::optional{new_packed_byte_order_ == 1
                                                            ? codegen::PackedByteOrder::
                                                                  little_endian
                                                            : codegen::PackedByteOrder::big_endian},
                                .bit_order =
                                    new_packed_bit_order_ == 0
                                        ? std::nullopt
                                        : std::
                                              optional{new_packed_bit_order_ == 1
                                                           ? codegen::PackedBitOrder::
                                                                 least_significant_first
                                                           : codegen::PackedBitOrder::
                                                                 most_significant_first}},
                        .insertion_index = std::nullopt},
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
            pending_packed_integer_scalar_binding_->field_name.c_str());
        ImGui::Separator();
    }

    auto const& modules{document_->manifest().modules};
    auto first_scalar_module{std::optional<std::size_t>{}};
    for (std::size_t index{}; index < modules.size(); ++index) {
        if (std::holds_alternative<codegen::NormalModuleSchema>(modules[index])) {
            first_scalar_module = index;
            break;
        }
    }
    if (!first_scalar_module.has_value()) {
        ImGui::TextDisabled("The target has no ordinary module to receive a new declaration.");
    } else {
        if (new_integer_scalar_module_index_ >= modules.size() ||
            !std::holds_alternative<codegen::NormalModuleSchema>(
                modules[new_integer_scalar_module_index_])) {
            new_integer_scalar_module_index_ = *first_scalar_module;
        }
        auto const& selected_module{
            std::get<codegen::NormalModuleSchema>(modules[new_integer_scalar_module_index_])};
        if (!module_initiated_dialog_ &&
            ImGui::BeginCombo("Module", selected_module.settings.name.c_str())) {
            for (std::size_t index{}; index < modules.size(); ++index) {
                auto const* module{std::get_if<codegen::NormalModuleSchema>(&modules[index])};
                if (module == nullptr) {
                    continue;
                }
                if (ImGui::Selectable(module->settings.name.c_str(),
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
            auto const& module{
                std::get<codegen::NormalModuleSchema>(modules[new_integer_scalar_module_index_])};
            auto const name{std::string{new_integer_scalar_name_.data()}};
            auto const identity{
                TypeIdentity{.origin = TypeOrigin::declaration,
                             .module_name = module.settings.name,
                             .namespace_name = module.settings.namespace_name.value_or(""),
                             .name = name}};
            auto const id{document_->allocate_declaration_id()};
            auto selection{std::optional<TypeIdentity>{identity}};
            auto named_codes{std::vector<codegen::PackedNamedCodeSchema>{}};
            if (pending_packed_integer_scalar_binding_.has_value()) {
                auto const& binding{*pending_packed_integer_scalar_binding_};
                if (auto const* packed_info{document_->declaration(binding.packed_declaration)};
                    packed_info != nullptr) {
                    selection = packed_info->identity;
                }
                if (auto const* packed_schema{
                        document_->packed_value_schema(binding.packed_declaration)};
                    packed_schema != nullptr) {
                    auto const segment{
                        std::ranges::find_if(packed_schema->segments, [&](auto const& candidate) {
                            return codegen::packed_segment_name(candidate) == binding.field_name;
                        })};
                    if (segment != packed_schema->segments.end()) {
                        if (auto const* field{std::get_if<codegen::PackedFieldSchema>(&*segment)}) {
                            named_codes = field->named_codes;
                        }
                    }
                }
            }
            if (apply_document_edit(
                    CreateIntegerScalar{
                        .declaration = id,
                        .module_index = new_integer_scalar_module_index_,
                        .schema =
                            codegen::IntegerScalarSchema{
                                .name = name,
                                .signedness = new_integer_scalar_signed_,
                                .minimum_value = *minimum,
                                .maximum_value = *maximum,
                                .bit_width = new_integer_scalar_width_auto_
                                               ? std::nullopt
                                               : std::optional{new_integer_scalar_bit_width_},
                                .named_codes = std::move(named_codes),
                                .relationship = std::nullopt,
                                .cpp_emission = codegen::IntegerScalarCppEmission::none,
                                .cpp_type = std::nullopt},
                        .insertion_index = std::nullopt},
                    selection) &&
                bind_new_integer_scalar_to_packed_field(identity)) {
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
        if (std::holds_alternative<codegen::NormalModuleSchema>(modules[index])) {
            first_module = index;
            break;
        }
    }
    if (!first_module.has_value()) {
        ImGui::TextDisabled("The target has no ordinary module to receive a new declaration.");
    } else {
        if (new_linear_quantized_module_index_ >= modules.size() ||
            !std::holds_alternative<codegen::NormalModuleSchema>(
                modules[new_linear_quantized_module_index_])) {
            new_linear_quantized_module_index_ = *first_module;
        }
        auto const& selected_module{
            std::get<codegen::NormalModuleSchema>(modules[new_linear_quantized_module_index_])};
        if (!module_initiated_dialog_ &&
            ImGui::BeginCombo("Module", selected_module.settings.name.c_str())) {
            for (std::size_t index{}; index < modules.size(); ++index) {
                auto const* module{std::get_if<codegen::NormalModuleSchema>(&modules[index])};
                if (module == nullptr) {
                    continue;
                }
                if (ImGui::Selectable(module->settings.name.c_str(),
                                      index == new_linear_quantized_module_index_)) {
                    new_linear_quantized_module_index_ = index;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::InputText(
            "Name", new_linear_quantized_name_.data(), new_linear_quantized_name_.size());
        if (new_linear_quantized_source_.front() == '\0') {
            for (auto const& node : document_->types().types()) {
                if (std::holds_alternative<IntegerScalarType>(node.definition)) {
                    std::snprintf(new_linear_quantized_source_.data(),
                                  new_linear_quantized_source_.size(),
                                  "%s",
                                  node.cpp_spelling.c_str());
                    break;
                }
            }
        }
        auto const source_label{new_linear_quantized_source_.front() == '\0'
                                    ? "Select integer scalar"
                                    : new_linear_quantized_source_.data()};
        if (ImGui::BeginCombo("Semantic source", source_label)) {
            for (auto const& node : document_->types().types()) {
                if (!std::holds_alternative<IntegerScalarType>(node.definition)) {
                    continue;
                }
                auto const selected{node.cpp_spelling == new_linear_quantized_source_.data()};
                if (ImGui::Selectable(node.cpp_spelling.c_str(), selected)) {
                    std::snprintf(new_linear_quantized_source_.data(),
                                  new_linear_quantized_source_.size(),
                                  "%s",
                                  node.cpp_spelling.c_str());
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
            auto const identity{
                TypeIdentity{.origin = TypeOrigin::declaration,
                             .module_name = selected_module.settings.name,
                             .namespace_name = selected_module.settings.namespace_name.value_or(""),
                             .name = name}};
            auto const id{document_->allocate_declaration_id()};
            if (apply_document_edit(
                    CreateLinearQuantized{
                        .declaration = id,
                        .module_index = new_linear_quantized_module_index_,
                        .schema =
                            codegen::LinearQuantizedSchema{
                                .name = name,
                                .source =
                                    codegen::TypeRef{.name = new_linear_quantized_source_.data(),
                                                     .suffix = {},
                                                     .nested = std::nullopt},
                                .bit_width = new_linear_quantized_bit_width_,
                                .reserved_codes = new_linear_quantized_reserved_codes_,
                                .clipping = new_linear_quantized_clipping_ == 0
                                              ? codegen::QuantizationClipping::reject
                                              : codegen::QuantizationClipping::clamp},
                        .insertion_index = std::nullopt},
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
        if (std::holds_alternative<codegen::NormalModuleSchema>(modules[index])) {
            first_module = index;
            break;
        }
    }
    if (!first_module.has_value()) {
        ImGui::TextDisabled("The target has no ordinary module to receive a new declaration.");
    } else {
        if (new_integer_varint_module_index_ >= modules.size() ||
            !std::holds_alternative<codegen::NormalModuleSchema>(
                modules[new_integer_varint_module_index_])) {
            new_integer_varint_module_index_ = *first_module;
        }
        auto const& selected_module{
            std::get<codegen::NormalModuleSchema>(modules[new_integer_varint_module_index_])};
        if (!module_initiated_dialog_ &&
            ImGui::BeginCombo("Module", selected_module.settings.name.c_str())) {
            for (std::size_t index{}; index < modules.size(); ++index) {
                auto const* module{std::get_if<codegen::NormalModuleSchema>(&modules[index])};
                if (module != nullptr &&
                    ImGui::Selectable(module->settings.name.c_str(),
                                      index == new_integer_varint_module_index_)) {
                    new_integer_varint_module_index_ = index;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::InputText("Name", new_integer_varint_name_.data(), new_integer_varint_name_.size());
        if (new_integer_varint_source_.front() == '\0') {
            for (auto const& node : document_->types().types()) {
                if (auto const* scalar{std::get_if<IntegerScalarType>(&node.definition)}) {
                    std::snprintf(new_integer_varint_source_.data(),
                                  new_integer_varint_source_.size(),
                                  "%s",
                                  node.cpp_spelling.c_str());
                    new_integer_varint_encoding_ = scalar->signedness ? 2 : 0;
                    break;
                }
            }
        }
        auto source_signed{std::optional<bool>{}};
        for (auto const& node : document_->types().types()) {
            if (node.cpp_spelling == new_integer_varint_source_.data()) {
                if (auto const* scalar{std::get_if<IntegerScalarType>(&node.definition)}) {
                    source_signed = scalar->signedness;
                }
                break;
            }
        }
        auto const source_label{new_integer_varint_source_.front() == '\0'
                                    ? "Select integer scalar"
                                    : new_integer_varint_source_.data()};
        if (ImGui::BeginCombo("Semantic source", source_label)) {
            for (auto const& node : document_->types().types()) {
                auto const* scalar{std::get_if<IntegerScalarType>(&node.definition)};
                if (scalar == nullptr) {
                    continue;
                }
                auto const selected{node.cpp_spelling == new_integer_varint_source_.data()};
                if (ImGui::Selectable(node.cpp_spelling.c_str(), selected)) {
                    std::snprintf(new_integer_varint_source_.data(),
                                  new_integer_varint_source_.size(),
                                  "%s",
                                  node.cpp_spelling.c_str());
                    source_signed = scalar->signedness;
                    new_integer_varint_encoding_ = scalar->signedness ? 2 : 0;
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
            auto const identity{
                TypeIdentity{.origin = TypeOrigin::declaration,
                             .module_name = selected_module.settings.name,
                             .namespace_name = selected_module.settings.namespace_name.value_or(""),
                             .name = name}};
            auto const encoding{new_integer_varint_encoding_ == 0
                                    ? codegen::IntegerVarintEncoding::unsigned_varint
                                : new_integer_varint_encoding_ == 1
                                    ? codegen::IntegerVarintEncoding::signed_varint
                                    : codegen::IntegerVarintEncoding::zigzag_varint};
            auto const id{document_->allocate_declaration_id()};
            if (apply_document_edit(
                    CreateIntegerVarint{
                        .declaration = id,
                        .module_index = new_integer_varint_module_index_,
                        .schema =
                            codegen::IntegerVarintSchema{
                                .name = name,
                                .source =
                                    codegen::TypeRef{.name = new_integer_varint_source_.data(),
                                                     .suffix = {},
                                                     .nested = std::nullopt},
                                .encoding = encoding},
                        .insertion_index = std::nullopt},
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

auto PlannerUi::bind_new_fixed_point_to_packed_field(TypeIdentity const& fixed_point) -> bool {
    if (!pending_packed_fixed_point_binding_.has_value()) {
        return true;
    }
    auto const binding{*pending_packed_fixed_point_binding_};
    auto const* packed_info{document_->declaration(binding.packed_declaration)};
    auto const* packed_schema{document_->packed_value_schema(binding.packed_declaration)};
    auto const packed_identity{packed_info == nullptr ? std::optional<TypeIdentity>{}
                                                      : std::optional{packed_info->identity}};
    auto rollback_creation = [&](std::string message) {
        auto const rollback{document_->undo()};
        if (rollback.has_value() && *rollback) {
            sync_document_graph(packed_identity);
            schema_edit_message_ = std::move(message) + " The new fixed point was rolled back.";
        } else if (!rollback.has_value()) {
            schema_edit_message_ = std::move(message) +
                                   " The new fixed point could not be rolled "
                                   "back: " +
                                   rollback.error().message;
        } else {
            schema_edit_message_ = std::move(message) +
                                   " The new fixed point could not be rolled back because history "
                                   "did not change.";
        }
        return false;
    };

    if (packed_info == nullptr || packed_schema == nullptr) {
        return rollback_creation("The packed declaration is no longer available.");
    }
    auto replacement{*packed_schema};
    auto const segment{std::ranges::find_if(replacement.segments, [&](auto const& candidate) {
        return codegen::packed_segment_name(candidate) == binding.field_name;
    })};
    if (segment == replacement.segments.end()) {
        return rollback_creation("The selected packed field is no longer available.");
    }
    auto* field{std::get_if<codegen::PackedFieldSchema>(&*segment)};
    if (field == nullptr) {
        return rollback_creation("The selected packed segment is no longer a field.");
    }

    field->type = codegen::TypeRef{.name = fixed_point.namespace_name.empty()
                                             ? fixed_point.name
                                             : fixed_point.namespace_name + "::" + fixed_point.name,
                                   .suffix = {},
                                   .nested = std::nullopt};
    field->kind = codegen::PackedFieldKind::fixed_point;
    field->bits.reset();
    field->range_helper = false;
    field->minimum_value.reset();
    field->maximum_value.reset();
    field->named_codes.clear();
    field->relationship.reset();
    if (!apply_document_edit(ReplacePackedValue{.declaration = binding.packed_declaration,
                                                .schema = std::move(replacement)},
                             packed_info->identity)) {
        return rollback_creation(
            "The fixed point was valid, but binding the packed field failed: " +
            schema_edit_message_);
    }

    analysis_session_.inputs.selection.field = binding.field_name;
    return true;
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
                           pending_packed_fixed_point_binding_->field_name.c_str());
        ImGui::Separator();
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
        if (new_fixed_point_module_index_ >= modules.size() ||
            !std::holds_alternative<codegen::NormalModuleSchema>(
                modules[new_fixed_point_module_index_])) {
            new_fixed_point_module_index_ = *first_module;
        }
        auto const& selected_module{
            std::get<codegen::NormalModuleSchema>(modules[new_fixed_point_module_index_])};
        if (!module_initiated_dialog_ &&
            ImGui::BeginCombo("Module", selected_module.settings.name.c_str())) {
            for (std::size_t index{}; index < modules.size(); ++index) {
                auto const* module{std::get_if<codegen::NormalModuleSchema>(&modules[index])};
                if (module != nullptr &&
                    ImGui::Selectable(module->settings.name.c_str(),
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
            auto const identity{
                TypeIdentity{.origin = TypeOrigin::declaration,
                             .module_name = selected_module.settings.name,
                             .namespace_name = selected_module.settings.namespace_name.value_or(""),
                             .name = name}};
            auto const id{document_->allocate_declaration_id()};
            auto selection{std::optional<TypeIdentity>{identity}};
            if (pending_packed_fixed_point_binding_.has_value()) {
                if (auto const* packed_info{document_->declaration(
                        pending_packed_fixed_point_binding_->packed_declaration)};
                    packed_info != nullptr) {
                    selection = packed_info->identity;
                }
            }
            if (apply_document_edit(
                    CreateFixedPoint{
                        .declaration = id,
                        .module_index = new_fixed_point_module_index_,
                        .schema =
                            codegen::FixedPointSchema{
                                .name = name,
                                .signedness = new_fixed_point_signed_,
                                .total_bits = new_fixed_point_total_bits_,
                                .fractional_bits = new_fixed_point_fractional_bits_,
                                .rounding = new_fixed_point_rounding_ == 0
                                              ? codegen::FixedPointRounding::nearest_even
                                              : codegen::FixedPointRounding::toward_zero},
                        .insertion_index = std::nullopt},
                    selection) &&
                bind_new_fixed_point_to_packed_field(identity)) {
                new_fixed_point_name_.fill('\0');
                new_fixed_point_signed_ = true;
                new_fixed_point_total_bits_ = 16;
                new_fixed_point_fractional_bits_ = 8;
                new_fixed_point_rounding_ = 0;
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

auto PlannerUi::bind_new_mini_float_to_packed_field(TypeIdentity const& mini_float) -> bool {
    if (!pending_packed_mini_float_binding_.has_value()) {
        return true;
    }
    auto const binding{*pending_packed_mini_float_binding_};
    auto const* packed_info{document_->declaration(binding.packed_declaration)};
    auto const* packed_schema{document_->packed_value_schema(binding.packed_declaration)};
    auto const packed_identity{packed_info == nullptr ? std::optional<TypeIdentity>{}
                                                      : std::optional{packed_info->identity}};
    auto rollback_creation = [&](std::string message) {
        auto const rollback{document_->undo()};
        if (rollback.has_value() && *rollback) {
            sync_document_graph(packed_identity);
            schema_edit_message_ = std::move(message) + " The new mini float was rolled back.";
        } else if (!rollback.has_value()) {
            schema_edit_message_ = std::move(message) +
                                   " The new mini float could not be rolled "
                                   "back: " +
                                   rollback.error().message;
        } else {
            schema_edit_message_ = std::move(message) +
                                   " The new mini float could not be rolled back because history "
                                   "did not change.";
        }
        return false;
    };

    if (packed_info == nullptr || packed_schema == nullptr) {
        return rollback_creation("The packed declaration is no longer available.");
    }
    auto replacement{*packed_schema};
    auto const segment{std::ranges::find_if(replacement.segments, [&](auto const& candidate) {
        return codegen::packed_segment_name(candidate) == binding.field_name;
    })};
    if (segment == replacement.segments.end()) {
        return rollback_creation("The selected packed field is no longer available.");
    }
    auto* field{std::get_if<codegen::PackedFieldSchema>(&*segment)};
    if (field == nullptr) {
        return rollback_creation("The selected packed segment is no longer a field.");
    }

    field->type = codegen::TypeRef{.name = mini_float.namespace_name.empty()
                                             ? mini_float.name
                                             : mini_float.namespace_name + "::" + mini_float.name,
                                   .suffix = {},
                                   .nested = std::nullopt};
    field->kind = codegen::PackedFieldKind::mini_float;
    field->bits.reset();
    field->range_helper = false;
    field->minimum_value.reset();
    field->maximum_value.reset();
    field->named_codes.clear();
    field->relationship.reset();
    if (!apply_document_edit(ReplacePackedValue{.declaration = binding.packed_declaration,
                                                .schema = std::move(replacement)},
                             packed_info->identity)) {
        return rollback_creation("The mini float was valid, but binding the packed field failed: " +
                                 schema_edit_message_);
    }

    analysis_session_.inputs.selection.field = binding.field_name;
    return true;
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
                           pending_packed_mini_float_binding_->field_name.c_str());
        ImGui::Separator();
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
        if (new_mini_float_module_index_ >= modules.size() ||
            !std::holds_alternative<codegen::NormalModuleSchema>(
                modules[new_mini_float_module_index_])) {
            new_mini_float_module_index_ = *first_module;
        }
        auto const& selected_module{
            std::get<codegen::NormalModuleSchema>(modules[new_mini_float_module_index_])};
        if (!module_initiated_dialog_ &&
            ImGui::BeginCombo("Module", selected_module.settings.name.c_str())) {
            for (std::size_t index{}; index < modules.size(); ++index) {
                auto const* module{std::get_if<codegen::NormalModuleSchema>(&modules[index])};
                if (module != nullptr && ImGui::Selectable(module->settings.name.c_str(),
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
            auto const identity{
                TypeIdentity{.origin = TypeOrigin::declaration,
                             .module_name = selected_module.settings.name,
                             .namespace_name = selected_module.settings.namespace_name.value_or(""),
                             .name = name}};
            auto const id{document_->allocate_declaration_id()};
            auto selection{std::optional<TypeIdentity>{identity}};
            if (pending_packed_mini_float_binding_.has_value()) {
                if (auto const* packed_info{document_->declaration(
                        pending_packed_mini_float_binding_->packed_declaration)};
                    packed_info != nullptr) {
                    selection = packed_info->identity;
                }
            }
            if (apply_document_edit(
                    CreateMiniFloat{.declaration = id,
                                    .module_index = new_mini_float_module_index_,
                                    .schema =
                                        codegen::MiniFloatSchema{
                                            .name = name,
                                            .sign_bits = new_mini_float_sign_bits_,
                                            .exponent_bits = new_mini_float_exponent_bits_,
                                            .significand_bits = new_mini_float_significand_bits_,
                                            .exponent_bias = new_mini_float_exponent_bias_},
                                    .insertion_index = std::nullopt},
                    selection) &&
                bind_new_mini_float_to_packed_field(identity)) {
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
        if (std::holds_alternative<codegen::NormalModuleSchema>(modules[index])) {
            first_module = index;
            break;
        }
    }

    std::vector<TypeId> sources;
    auto const types{analysis_session_.inputs.workspace.types().types()};
    for (std::size_t index{}; index < types.size(); ++index) {
        auto const* scalar{std::get_if<IntegerScalarType>(&types[index].definition)};
        if (scalar != nullptr &&
            std::ranges::any_of(scalar->named_codes, &PackedNamedCode::sentinel)) {
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
            !std::holds_alternative<codegen::NormalModuleSchema>(
                modules[new_optional_sentinel_module_index_])) {
            new_optional_sentinel_module_index_ = *first_module;
        }
        auto const& selected_module{
            std::get<codegen::NormalModuleSchema>(modules[new_optional_sentinel_module_index_])};
        if (!module_initiated_dialog_ &&
            ImGui::BeginCombo("Module", selected_module.settings.name.c_str())) {
            for (std::size_t index{}; index < modules.size(); ++index) {
                auto const* module{std::get_if<codegen::NormalModuleSchema>(&modules[index])};
                if (module != nullptr &&
                    ImGui::Selectable(module->settings.name.c_str(),
                                      index == new_optional_sentinel_module_index_)) {
                    new_optional_sentinel_module_index_ = index;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::InputText(
            "Name", new_optional_sentinel_name_.data(), new_optional_sentinel_name_.size());
        auto selected_source{std::ranges::find_if(sources, [&](TypeId const type) {
            return analysis_session_.inputs.workspace.types().type(type).cpp_spelling ==
                   new_optional_sentinel_source_.data();
        })};
        if (selected_source == sources.end()) {
            selected_source = sources.begin();
            auto const& node{analysis_session_.inputs.workspace.types().type(*selected_source)};
            std::snprintf(new_optional_sentinel_source_.data(),
                          new_optional_sentinel_source_.size(),
                          "%s",
                          node.cpp_spelling.c_str());
            auto const& scalar{std::get<IntegerScalarType>(node.definition)};
            auto const sentinel{std::ranges::find_if(
                scalar.named_codes, [](auto const& code) { return code.sentinel; })};
            std::snprintf(new_optional_sentinel_code_.data(),
                          new_optional_sentinel_code_.size(),
                          "%s",
                          sentinel->name.c_str());
        }

        auto selected_source_type{*selected_source};
        if (ImGui::BeginCombo("Semantic source",
                              analysis_session_.inputs.workspace.types()
                                  .type(selected_source_type)
                                  .cpp_spelling.c_str())) {
            for (auto const type : sources) {
                auto const& candidate{analysis_session_.inputs.workspace.types().type(type)};
                auto const selected{type == selected_source_type};
                if (ImGui::Selectable(candidate.cpp_spelling.c_str(), selected)) {
                    selected_source_type = type;
                    std::snprintf(new_optional_sentinel_source_.data(),
                                  new_optional_sentinel_source_.size(),
                                  "%s",
                                  candidate.cpp_spelling.c_str());
                    auto const& scalar{std::get<IntegerScalarType>(candidate.definition)};
                    auto const sentinel{std::ranges::find_if(
                        scalar.named_codes, [](auto const& code) { return code.sentinel; })};
                    std::snprintf(new_optional_sentinel_code_.data(),
                                  new_optional_sentinel_code_.size(),
                                  "%s",
                                  sentinel->name.c_str());
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        auto const& source_node{
            analysis_session_.inputs.workspace.types().type(selected_source_type)};
        auto const& source_scalar{std::get<IntegerScalarType>(source_node.definition)};
        auto selected_code{std::ranges::find_if(source_scalar.named_codes, [&](auto const& code) {
            return code.sentinel && code.name == new_optional_sentinel_code_.data();
        })};
        if (selected_code == source_scalar.named_codes.end()) {
            selected_code = std::ranges::find_if(source_scalar.named_codes,
                                                 [](auto const& code) { return code.sentinel; });
            std::snprintf(new_optional_sentinel_code_.data(),
                          new_optional_sentinel_code_.size(),
                          "%s",
                          selected_code->name.c_str());
        }
        if (ImGui::BeginCombo("Absence sentinel", selected_code->name.c_str())) {
            for (auto const& code : source_scalar.named_codes) {
                if (!code.sentinel) {
                    continue;
                }
                auto const selected{code.name == selected_code->name};
                if (ImGui::Selectable(code.name.c_str(), selected)) {
                    std::snprintf(new_optional_sentinel_code_.data(),
                                  new_optional_sentinel_code_.size(),
                                  "%s",
                                  code.name.c_str());
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
            auto const identity{
                TypeIdentity{.origin = TypeOrigin::declaration,
                             .module_name = selected_module.settings.name,
                             .namespace_name = selected_module.settings.namespace_name.value_or(""),
                             .name = name}};
            auto const id{document_->allocate_declaration_id()};
            if (apply_document_edit(
                    CreateOptionalSentinel{
                        .declaration = id,
                        .module_index = new_optional_sentinel_module_index_,
                        .schema =
                            codegen::OptionalSentinelSchema{
                                .name = name,
                                .source =
                                    codegen::TypeRef{.name = new_optional_sentinel_source_.data(),
                                                     .suffix = {},
                                                     .nested = std::nullopt},
                                .sentinel = new_optional_sentinel_code_.data()},
                        .insertion_index = std::nullopt},
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
        if (std::holds_alternative<codegen::NormalModuleSchema>(modules[index])) {
            first_module = index;
            break;
        }
    }

    std::vector<TypeId> sources;
    auto const types{analysis_session_.inputs.workspace.types().types()};
    for (std::size_t index{}; index < types.size(); ++index) {
        if (std::holds_alternative<IntegerScalarType>(types[index].definition)) {
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
            !std::holds_alternative<codegen::NormalModuleSchema>(
                modules[new_optional_presence_bit_module_index_])) {
            new_optional_presence_bit_module_index_ = *first_module;
        }
        auto const& selected_module{std::get<codegen::NormalModuleSchema>(
            modules[new_optional_presence_bit_module_index_])};
        if (!module_initiated_dialog_ &&
            ImGui::BeginCombo("Module", selected_module.settings.name.c_str())) {
            for (std::size_t index{}; index < modules.size(); ++index) {
                auto const* module{std::get_if<codegen::NormalModuleSchema>(&modules[index])};
                if (module != nullptr &&
                    ImGui::Selectable(module->settings.name.c_str(),
                                      index == new_optional_presence_bit_module_index_)) {
                    new_optional_presence_bit_module_index_ = index;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::InputText(
            "Name", new_optional_presence_bit_name_.data(), new_optional_presence_bit_name_.size());
        auto selected_source{std::ranges::find_if(sources, [&](TypeId const type) {
            return analysis_session_.inputs.workspace.types().type(type).cpp_spelling ==
                   new_optional_presence_bit_source_.data();
        })};
        if (selected_source == sources.end()) {
            selected_source = sources.begin();
            auto const& node{analysis_session_.inputs.workspace.types().type(*selected_source)};
            std::snprintf(new_optional_presence_bit_source_.data(),
                          new_optional_presence_bit_source_.size(),
                          "%s",
                          node.cpp_spelling.c_str());
        }

        auto selected_source_type{*selected_source};
        if (ImGui::BeginCombo("Semantic source",
                              analysis_session_.inputs.workspace.types()
                                  .type(selected_source_type)
                                  .cpp_spelling.c_str())) {
            for (auto const type : sources) {
                auto const& candidate{analysis_session_.inputs.workspace.types().type(type)};
                auto const selected{type == selected_source_type};
                if (ImGui::Selectable(candidate.cpp_spelling.c_str(), selected)) {
                    selected_source_type = type;
                    std::snprintf(new_optional_presence_bit_source_.data(),
                                  new_optional_presence_bit_source_.size(),
                                  "%s",
                                  candidate.cpp_spelling.c_str());
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        auto const& source_scalar{std::get<IntegerScalarType>(
            analysis_session_.inputs.workspace.types().type(selected_source_type).definition)};
        ImGui::Text("Encoding: 1 presence bit + %u payload bits = %u bits/value",
                    source_scalar.bit_width,
                    source_scalar.bit_width + 1);
        ImGui::TextDisabled(
            "Concrete byte packing and presence-bit ordering remain unspecified until placement.");

        auto const ready{new_optional_presence_bit_name_.front() != '\0'};
        ImGui::BeginDisabled(!ready);
        if (ImGui::Button("Create")) {
            auto const name{std::string{new_optional_presence_bit_name_.data()}};
            auto const identity{
                TypeIdentity{.origin = TypeOrigin::declaration,
                             .module_name = selected_module.settings.name,
                             .namespace_name = selected_module.settings.namespace_name.value_or(""),
                             .name = name}};
            auto const id{document_->allocate_declaration_id()};
            if (apply_document_edit(
                    CreateOptionalPresenceBit{
                        .declaration = id,
                        .module_index = new_optional_presence_bit_module_index_,
                        .schema =
                            codegen::OptionalPresenceBitSchema{
                                .name = name,
                                .source =
                                    codegen::TypeRef{.name =
                                                         new_optional_presence_bit_source_.data(),
                                                     .suffix = {},
                                                     .nested = std::nullopt}},
                        .insertion_index = std::nullopt},
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
