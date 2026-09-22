#include "planner_ui_properties_common.hpp"

namespace ioj::layout_planner {

void PlannerUi::draw_enum_editor(TypeNode const& node, EnumType const&) {
    if (!document_.has_value()) {
        return;
    }
    auto const declaration{document_->find_declaration(node.identity)};
    if (!declaration.has_value()) {
        return;
    }
    auto const* schema{document_->enum_schema(*declaration)};
    if (schema == nullptr) {
        return;
    }

    if (selected_enumerator_.empty() && !schema->values.empty()) {
        selected_enumerator_ = schema->values.front().name;
    }
    auto selected{
        std::ranges::find(schema->values, selected_enumerator_, &codegen::EnumeratorSchema::name)};
    if (selected == schema->values.end() && !schema->values.empty()) {
        selected = schema->values.begin();
        selected_enumerator_ = selected->name;
    }
    auto const selected_index{selected == schema->values.end()
                                  ? std::optional<std::size_t>{}
                                  : std::optional<std::size_t>{static_cast<std::size_t>(
                                        selected - schema->values.begin())}};
    auto const enum_declaration_changed{enum_editor_declaration_ != declaration};
    if (enum_declaration_changed || enum_editor_value_ != selected_enumerator_) {
        enum_editor_declaration_ = declaration;
        enum_editor_value_ = selected_enumerator_;
        std::snprintf(enum_underlying_type_.data(),
                      enum_underlying_type_.size(),
                      "%s",
                      schema->underlying_type.has_value() ? schema->underlying_type->name.c_str()
                                                          : "std::uint8_t");
        std::snprintf(enum_export_specifier_.data(),
                      enum_export_specifier_.size(),
                      "%s",
                      schema->export_specifier.value_or("").c_str());
        if (enum_declaration_changed) {
            auto const* projection{
                schema->unreal_projection.has_value() ? &*schema->unreal_projection : nullptr};
            std::snprintf(enum_projection_name_.data(),
                          enum_projection_name_.size(),
                          "%s",
                          projection != nullptr ? projection->name.c_str() : "");
            std::snprintf(enum_projection_header_.data(),
                          enum_projection_header_.size(),
                          "%s",
                          projection != nullptr ? projection->header.string().c_str() : "");
            std::snprintf(enum_projection_header_include_.data(),
                          enum_projection_header_include_.size(),
                          "%s",
                          projection != nullptr ? projection->header_include.c_str() : "");
            std::snprintf(enum_projection_conversion_header_.data(),
                          enum_projection_conversion_header_.size(),
                          "%s",
                          projection != nullptr ? projection->conversion_header.string().c_str()
                                                : "");
            std::snprintf(enum_projection_native_header_include_.data(),
                          enum_projection_native_header_include_.size(),
                          "%s",
                          projection != nullptr ? projection->native_header_include.c_str() : "");
            auto const reflection{
                projection != nullptr
                    ? std::ranges::find(enum_projection_reflections, projection->reflection)
                    : enum_projection_reflections.begin()};
            enum_projection_reflection_ =
                reflection == enum_projection_reflections.end()
                    ? 0
                    : static_cast<int>(reflection - enum_projection_reflections.begin());
        }
        if (selected != schema->values.end()) {
            std::snprintf(
                enum_value_name_.data(), enum_value_name_.size(), "%s", selected->name.c_str());
            set_buffer(enum_value_initializer_, selected->initializer);
            set_buffer(enum_value_display_name_, selected->display_name);
            set_buffer(enum_value_serialized_name_, selected->serialized_name);
        }
    }

    auto automatic_backing{!schema->underlying_type.has_value()};
    if (ImGui::Checkbox("Auto C++ backing", &automatic_backing)) {
        auto replacement{*schema};
        replacement.underlying_type =
            automatic_backing
                ? std::optional<codegen::TypeRef>{}
                : std::optional{codegen::TypeRef{
                      .name = enum_underlying_type_.data(), .suffix = {}, .nested = std::nullopt}};
        static_cast<void>(apply_document_edit(
            ReplaceEnum{.declaration = *declaration, .schema = std::move(replacement)}));
        return;
    }
    if (schema->underlying_type.has_value()) {
        auto const submitted{ImGui::InputText("C++ backing type",
                                              enum_underlying_type_.data(),
                                              enum_underlying_type_.size(),
                                              ImGuiInputTextFlags_EnterReturnsTrue)};
        if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
            if (enum_underlying_type_.front() == '\0') {
                schema_edit_message_ = "Explicit enum C++ backing type cannot be empty.";
            } else if (schema->underlying_type->name != enum_underlying_type_.data()) {
                auto replacement{*schema};
                replacement.underlying_type = codegen::TypeRef{
                    .name = enum_underlying_type_.data(), .suffix = {}, .nested = std::nullopt};
                static_cast<void>(apply_document_edit(
                    ReplaceEnum{.declaration = *declaration, .schema = std::move(replacement)}));
            }
            return;
        }
    } else if (analysis_session_.results().enum_domain.has_value()) {
        ImGui::TextDisabled("Derived for C++: %s",
                            analysis_session_.results().enum_domain->backing_type.c_str());
    }

    auto automatic_width{!schema->bit_width.has_value()};
    if (ImGui::Checkbox("Auto semantic width", &automatic_width)) {
        auto replacement{*schema};
        if (automatic_width) {
            replacement.bit_width.reset();
        } else {
            replacement.bit_width =
                analysis_session_.results().enum_domain.has_value()
                    ? analysis_session_.results().enum_domain->minimum_required_bits.value_or(1)
                    : std::uint32_t{1};
        }
        static_cast<void>(apply_document_edit(
            ReplaceEnum{.declaration = *declaration, .schema = std::move(replacement)}));
        return;
    }
    if (schema->bit_width.has_value()) {
        auto bit_width{*schema->bit_width};
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
                auto replacement{*schema};
                replacement.bit_width = bit_width;
                static_cast<void>(apply_document_edit(
                    ReplaceEnum{.declaration = *declaration, .schema = std::move(replacement)}));
            }
            return;
        }
    }
    auto signedness_mode{schema->signedness.has_value() ? (*schema->signedness ? 2 : 1) : 0};
    constexpr std::array signedness_labels{"Auto / inferred", "Unsigned", "Signed"};
    detail::prepare_property_input("Semantic signedness");
    if (ImGui::Combo("##Semantic signedness",
                     &signedness_mode,
                     signedness_labels.data(),
                     static_cast<int>(signedness_labels.size()))) {
        auto replacement{*schema};
        replacement.signedness = signedness_mode == 0 ? std::optional<bool>{}
                               : signedness_mode == 2 ? std::optional<bool>{true}
                                                      : std::optional<bool>{false};
        static_cast<void>(apply_document_edit(
            ReplaceEnum{.declaration = *declaration, .schema = std::move(replacement)}));
        return;
    }
    ImGui::PushTextWrapPos(0.0F);
    ImGui::TextDisabled("Semantic width describes the value domain; C++ backing storage is a "
                        "separate lowering choice.");
    ImGui::PopTextWrapPos();

    ImGui::SeparatorText("Generation policy");
    auto reflection_index{static_cast<int>(std::ranges::find(enum_reflections, schema->reflection) -
                                           enum_reflections.begin())};
    if (reflection_index < 0 || reflection_index >= static_cast<int>(enum_reflections.size())) {
        reflection_index = 0;
    }
    detail::prepare_property_input("Reflection");
    if (ImGui::BeginCombo("##Reflection",
                          codegen::enum_reflection_name(
                              enum_reflections[static_cast<std::size_t>(reflection_index)])
                              .data())) {
        for (std::size_t index{}; index < enum_reflections.size(); ++index) {
            auto const reflection{enum_reflections[index]};
            auto const selected_reflection{index == static_cast<std::size_t>(reflection_index)};
            if (ImGui::Selectable(codegen::enum_reflection_name(reflection).data(),
                                  selected_reflection)) {
                auto replacement{*schema};
                replacement.reflection = reflection;
                ImGui::EndCombo();
                static_cast<void>(apply_document_edit(
                    ReplaceEnum{.declaration = *declaration, .schema = std::move(replacement)}));
                return;
            }
            if (selected_reflection) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    auto enum_array{schema->enum_array};
    if (ImGui::Checkbox("Generate enum-array helpers", &enum_array)) {
        auto replacement{*schema};
        replacement.enum_array = enum_array;
        static_cast<void>(apply_document_edit(
            ReplaceEnum{.declaration = *declaration, .schema = std::move(replacement)}));
        return;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Requires a valid final count-sentinel enumerator and implicit values.");
    }

    auto native_api{schema->native_api};
    if (ImGui::Checkbox("Use native enum API", &native_api)) {
        auto replacement{*schema};
        replacement.native_api = native_api;
        static_cast<void>(apply_document_edit(
            ReplaceEnum{.declaration = *declaration, .schema = std::move(replacement)}));
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
        auto const export_specifier{optional_text(enum_export_specifier_)};
        if (schema->export_specifier != export_specifier) {
            auto replacement{*schema};
            replacement.export_specifier = export_specifier;
            static_cast<void>(apply_document_edit(
                ReplaceEnum{.declaration = *declaration, .schema = std::move(replacement)}));
        }
        return;
    }

    ImGui::SeparatorText("Unreal projection");
    std::optional<codegen::EnumSchema> projection_edit;
    auto const projection_exists{schema->unreal_projection.has_value()};
    auto edit_projection_text = [&](char const* label, auto& buffer, auto&& assign) {
        detail::prepare_property_input(label);
        auto const input_id{"##" + std::string{label}};
        auto const submitted{ImGui::InputText(
            input_id.c_str(), buffer.data(), buffer.size(), ImGuiInputTextFlags_EnterReturnsTrue)};
        if (projection_exists && (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
            if (!projection_edit.has_value()) {
                projection_edit = *schema;
            }
            assign(*projection_edit->unreal_projection, buffer.data());
        }
    };
    edit_projection_text("Projected enum name",
                         enum_projection_name_,
                         [](auto& projection, char const* value) { projection.name = value; });
    edit_projection_text(
        "Generated header path", enum_projection_header_, [](auto& projection, char const* value) {
            projection.header = std::filesystem::path{value};
        });
    edit_projection_text(
        "Generated header include",
        enum_projection_header_include_,
        [](auto& projection, char const* value) { projection.header_include = value; });
    edit_projection_text("Conversion header path",
                         enum_projection_conversion_header_,
                         [](auto& projection, char const* value) {
                             projection.conversion_header = std::filesystem::path{value};
                         });
    edit_projection_text(
        "Native header include",
        enum_projection_native_header_include_,
        [](auto& projection, char const* value) { projection.native_header_include = value; });

    enum_projection_reflection_ = std::clamp(
        enum_projection_reflection_, 0, static_cast<int>(enum_projection_reflections.size() - 1));
    auto const selected_projection_reflection{
        enum_projection_reflections[static_cast<std::size_t>(enum_projection_reflection_)]};
    detail::prepare_property_input("Projection reflection");
    if (ImGui::BeginCombo("##Projection reflection",
                          codegen::enum_reflection_name(selected_projection_reflection).data())) {
        for (std::size_t index{}; index < enum_projection_reflections.size(); ++index) {
            auto const reflection{enum_projection_reflections[index]};
            auto const selected_reflection{enum_projection_reflection_ == static_cast<int>(index)};
            if (ImGui::Selectable(codegen::enum_reflection_name(reflection).data(),
                                  selected_reflection)) {
                enum_projection_reflection_ = static_cast<int>(index);
                if (projection_exists) {
                    projection_edit = *schema;
                    projection_edit->unreal_projection->reflection = reflection;
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
            ReplaceEnum{.declaration = *declaration, .schema = std::move(*projection_edit)}));
        return;
    }

    if (projection_exists) {
        if (ImGui::Button("Remove Unreal projection")) {
            auto replacement{*schema};
            replacement.unreal_projection.reset();
            static_cast<void>(apply_document_edit(
                ReplaceEnum{.declaration = *declaration, .schema = std::move(replacement)}));
            return;
        }
    } else {
        auto const fields_complete{enum_projection_name_.front() != '\0' &&
                                   enum_projection_header_.front() != '\0' &&
                                   enum_projection_header_include_.front() != '\0' &&
                                   enum_projection_conversion_header_.front() != '\0' &&
                                   enum_projection_native_header_include_.front() != '\0'};
        ImGui::BeginDisabled(!schema->native_api || !fields_complete);
        if (ImGui::Button("Add Unreal projection")) {
            auto replacement{*schema};
            replacement.unreal_projection = codegen::EnumUnrealProjection{
                .name = enum_projection_name_.data(),
                .header = std::filesystem::path{enum_projection_header_.data()},
                .header_include = enum_projection_header_include_.data(),
                .conversion_header =
                    std::filesystem::path{enum_projection_conversion_header_.data()},
                .native_header_include = enum_projection_native_header_include_.data(),
                .reflection = selected_projection_reflection};
            static_cast<void>(apply_document_edit(
                ReplaceEnum{.declaration = *declaration, .schema = std::move(replacement)}));
            return;
        }
        ImGui::EndDisabled();
        if (!schema->native_api) {
            ImGui::TextDisabled("A projection requires native enum API mode.");
        } else if (!fields_complete) {
            ImGui::TextDisabled("Fill every projection field before adding it.");
        }
    }
    ImGui::PushTextWrapPos(0.0F);
    ImGui::TextDisabled("Projection files are generated consumer outputs; semantic enum width and "
                        "target layout remain unchanged.");
    ImGui::PopTextWrapPos();

    ImGui::SeparatorText("Generated conversions");
    if (ImGui::BeginTable("enum-conversions", 2, ImGuiTableFlags_SizingStretchSame)) {
        for (auto const conversion : enum_conversions) {
            auto const descriptor{enum_conversion_descriptor(conversion)};
            ImGui::TableNextColumn();
            auto enabled{has_enum_conversion(schema->conversions, conversion)};
            if (ImGui::Checkbox(descriptor.source_name.data(), &enabled)) {
                auto replacement{*schema};
                replacement.conversions =
                    with_enum_conversion(schema->conversions, conversion, enabled);
                ImGui::EndTable();
                static_cast<void>(apply_document_edit(
                    ReplaceEnum{.declaration = *declaration, .schema = std::move(replacement)}));
                return;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", descriptor.description);
            }
        }
        ImGui::EndTable();
    }

    if (ImGui::Button("+ Enumerator")) {
        auto replacement{*schema};
        auto suffix{replacement.values.size()};
        std::string name;
        do {
            name = "Value" + std::to_string(suffix++);
        } while (std::ranges::find(replacement.values, name, &codegen::EnumeratorSchema::name) !=
                 replacement.values.end());
        auto insertion_index{replacement.values.size()};
        if (replacement.count.has_value()) {
            auto const count{std::ranges::find(
                replacement.values, *replacement.count, &codegen::EnumeratorSchema::name)};
            if (count != replacement.values.end()) {
                insertion_index = static_cast<std::size_t>(count - replacement.values.begin());
            }
        }
        replacement.values.insert(replacement.values.begin() +
                                      static_cast<std::ptrdiff_t>(insertion_index),
                                  {.name = name,
                                   .initializer = std::nullopt,
                                   .display_name = std::nullopt,
                                   .hidden = false,
                                   .serialized_name = std::nullopt});
        if (apply_document_edit(
                ReplaceEnum{.declaration = *declaration, .schema = std::move(replacement)})) {
            selected_enumerator_ = std::move(name);
            return;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value());
    if (ImGui::Button("Duplicate")) {
        auto replacement{*schema};
        auto copy{replacement.values[*selected_index]};
        auto suffix{std::size_t{1}};
        auto const stem{copy.name + "_copy"};
        copy.name = stem;
        while (std::ranges::find(replacement.values, copy.name, &codegen::EnumeratorSchema::name) !=
               replacement.values.end()) {
            copy.name = stem + std::to_string(suffix++);
        }
        auto const insertion_index{schema->count == replacement.values[*selected_index].name
                                       ? *selected_index
                                       : *selected_index + 1};
        replacement.values.insert(
            replacement.values.begin() + static_cast<std::ptrdiff_t>(insertion_index), copy);
        if (apply_document_edit(
                ReplaceEnum{.declaration = *declaration, .schema = std::move(replacement)})) {
            selected_enumerator_ = std::move(copy.name);
            return;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() || *selected_index == 0);
    if (ImGui::Button("Move up")) {
        auto replacement{*schema};
        std::swap(replacement.values[*selected_index], replacement.values[*selected_index - 1]);
        if (apply_document_edit(
                ReplaceEnum{.declaration = *declaration, .schema = std::move(replacement)})) {
            selected_enumerator_ = enum_editor_value_;
            return;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() ||
                         *selected_index + 1 >= schema->values.size());
    if (ImGui::Button("Move down")) {
        auto replacement{*schema};
        std::swap(replacement.values[*selected_index], replacement.values[*selected_index + 1]);
        if (apply_document_edit(
                ReplaceEnum{.declaration = *declaration, .schema = std::move(replacement)})) {
            selected_enumerator_ = enum_editor_value_;
            return;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() || schema->values.size() == 1);
    if (ImGui::Button("Delete")) {
        auto replacement{*schema};
        auto const deleted_name{replacement.values[*selected_index].name};
        replacement.values.erase(replacement.values.begin() +
                                 static_cast<std::ptrdiff_t>(*selected_index));
        if (replacement.count == deleted_name) {
            replacement.count.reset();
        }
        auto const next_index{std::min(*selected_index, replacement.values.size() - 1)};
        auto const next_name{replacement.values[next_index].name};
        if (apply_document_edit(
                ReplaceEnum{.declaration = *declaration, .schema = std::move(replacement)})) {
            selected_enumerator_ = next_name;
            return;
        }
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();

    std::optional<codegen::EnumSchema> pending;
    auto selected_after_edit{selected_enumerator_};
    if (detail::begin_editable_table("enumerators", 9, schema->values.size())) {
        detail::editable_table_column("Edit", ImGuiTableColumnFlags_WidthFixed);
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
        for (std::size_t index{}; index < schema->values.size(); ++index) {
            auto const& value{schema->values[index]};
            auto const row_selected{selected_enumerator_ == value.name};
            ImGui::PushID(static_cast<int>(index));
            auto select_cell = [&](char const* id, char const* text) {
                ImGui::PushID(id);
                if (ImGui::Selectable(text)) {
                    selected_enumerator_ = value.name;
                    enum_editor_declaration_.reset();
                }
                ImGui::PopID();
            };
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (detail::editable_table_row_handle(row_selected)) {
                selected_enumerator_ = value.name;
                enum_editor_declaration_.reset();
            }
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("ENUMERATOR_ROW", &index, sizeof(index));
                ImGui::Text("Move %s", value.name.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget()) {
                if (auto const* payload{ImGui::AcceptDragDropPayload("ENUMERATOR_ROW")}) {
                    auto const source_index{*static_cast<std::size_t const*>(payload->Data)};
                    if (source_index < schema->values.size() && source_index != index) {
                        pending = *schema;
                        selected_after_edit = pending->values[source_index].name;
                        move_element(pending->values, source_index, index);
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
                    pending = *schema;
                    auto& edited{pending->values[index]};
                    auto const previous_name{edited.name};
                    edited.name = enum_value_name_.data();
                    if (pending->count == previous_name) {
                        pending->count = edited.name;
                    }
                    selected_after_edit = edited.name;
                }
            } else {
                select_cell("name", value.name.c_str());
            }

            auto draw_optional_editor = [&](char const* label,
                                            auto& buffer,
                                            std::optional<std::string> codegen::EnumeratorSchema::*
                                                member) {
                ImGui::TableNextColumn();
                if (row_selected) {
                    ImGui::SetNextItemWidth(-1.0F);
                    auto const submitted{ImGui::InputText(
                        label, buffer.data(), buffer.size(), ImGuiInputTextFlags_EnterReturnsTrue)};
                    if (!pending.has_value() &&
                        (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                        pending = *schema;
                        pending->values[index].*member = optional_text(buffer);
                    }
                } else {
                    auto const& text{value.*member};
                    select_cell(label, text.has_value() ? text->c_str() : "-");
                }
            };
            draw_optional_editor(
                "##value", enum_value_initializer_, &codegen::EnumeratorSchema::initializer);
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
                "##display", enum_value_display_name_, &codegen::EnumeratorSchema::display_name);
            draw_optional_editor("##serialized",
                                 enum_value_serialized_name_,
                                 &codegen::EnumeratorSchema::serialized_name);

            ImGui::TableNextColumn();
            auto hidden{value.hidden};
            if (row_selected) {
                if (ImGui::Checkbox("##hidden", &hidden)) {
                    pending = *schema;
                    pending->values[index].hidden = hidden;
                }
            } else {
                select_cell("hidden", hidden ? "yes" : "-");
            }

            ImGui::TableNextColumn();
            auto sentinel{value.sentinel};
            if (row_selected) {
                if (ImGui::Checkbox("##sentinel", &sentinel)) {
                    pending = *schema;
                    pending->values[index].sentinel = sentinel;
                }
            } else {
                select_cell("sentinel", sentinel ? "yes" : "-");
            }

            ImGui::TableNextColumn();
            auto count_sentinel{schema->count == value.name};
            if (row_selected) {
                if (ImGui::Checkbox("##count", &count_sentinel)) {
                    pending = *schema;
                    if (count_sentinel) {
                        pending->count = value.name;
                    } else if (pending->count == value.name) {
                        pending->count.reset();
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
        if (pending->values[*selected_index].name.empty()) {
            schema_edit_message_ = "Enumerator name cannot be empty.";
            return;
        }
        if (apply_document_edit(
                ReplaceEnum{.declaration = *declaration, .schema = std::move(*pending)})) {
            selected_enumerator_ = std::move(selected_after_edit);
        }
    }
}

auto PlannerUi::draw_integer_scalar_editor(TypeNode const& node, IntegerScalarType const& scalar)
    -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const declaration{document_->find_declaration(node.identity)};
    if (!declaration.has_value()) {
        return false;
    }
    auto const* schema{document_->integer_scalar_schema(*declaration)};
    if (schema == nullptr) {
        return false;
    }

    if (selected_integer_scalar_code_.empty() && !schema->named_codes.empty()) {
        selected_integer_scalar_code_ = schema->named_codes.front().name;
    }
    auto selected{std::ranges::find(
        schema->named_codes, selected_integer_scalar_code_, &codegen::PackedNamedCodeSchema::name)};
    if (selected == schema->named_codes.end() && !schema->named_codes.empty()) {
        selected = schema->named_codes.begin();
        selected_integer_scalar_code_ = selected->name;
    }
    auto const selected_index{selected == schema->named_codes.end()
                                  ? std::optional<std::size_t>{}
                                  : std::optional<std::size_t>{static_cast<std::size_t>(
                                        selected - schema->named_codes.begin())}};
    if (integer_scalar_editor_declaration_ != declaration) {
        integer_scalar_editor_declaration_ = declaration;
        integer_scalar_editor_code_.clear();
        std::snprintf(integer_scalar_minimum_.data(),
                      integer_scalar_minimum_.size(),
                      "%s",
                      codegen::format_packed_integer(schema->minimum_value).c_str());
        std::snprintf(integer_scalar_maximum_.data(),
                      integer_scalar_maximum_.size(),
                      "%s",
                      codegen::format_packed_integer(schema->maximum_value).c_str());
        std::snprintf(integer_scalar_cpp_type_.data(),
                      integer_scalar_cpp_type_.size(),
                      "%s",
                      schema->cpp_type.has_value() ? schema->cpp_type->name.c_str() : "");
        if (schema->relationship.has_value()) {
            std::snprintf(integer_scalar_relationship_target_.data(),
                          integer_scalar_relationship_target_.size(),
                          "%s",
                          schema->relationship->target.name.c_str());
            auto const kind{
                std::ranges::find(semantic_relationship_kinds, schema->relationship->kind)};
            integer_scalar_relationship_kind_ =
                kind == semantic_relationship_kinds.end()
                    ? 0
                    : static_cast<int>(kind - semantic_relationship_kinds.begin());
            auto const unit{
                schema->relationship->unit.has_value()
                    ? std::ranges::find(semantic_relationship_units, *schema->relationship->unit)
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
        if (selected != schema->named_codes.end()) {
            std::snprintf(integer_scalar_code_name_.data(),
                          integer_scalar_code_name_.size(),
                          "%s",
                          selected->name.c_str());
            std::snprintf(integer_scalar_code_value_.data(),
                          integer_scalar_code_value_.size(),
                          "%s",
                          codegen::format_packed_integer(selected->value).c_str());
            integer_scalar_code_sentinel_ = selected->sentinel;
        }
    }

    auto signedness{schema->signedness};
    if (ImGui::Checkbox("Signed domain", &signedness)) {
        auto replacement{*schema};
        replacement.signedness = signedness;
        if (apply_document_edit(ReplaceIntegerScalar{.declaration = *declaration,
                                                     .schema = std::move(replacement)})) {
            integer_scalar_editor_declaration_.reset();
            return true;
        }
    }
    auto automatic_width{!schema->bit_width.has_value()};
    if (ImGui::Checkbox("Auto bit width", &automatic_width)) {
        auto replacement{*schema};
        replacement.bit_width =
            automatic_width
                ? std::nullopt
                : std::optional{analysis_session_.results().integer_scalar_analysis.has_value()
                                    ? analysis_session_.results()
                                          .integer_scalar_analysis->minimum_required_bits
                                    : std::uint32_t{1}};
        if (apply_document_edit(ReplaceIntegerScalar{.declaration = *declaration,
                                                     .schema = std::move(replacement)})) {
            integer_scalar_editor_declaration_.reset();
            return true;
        }
    }
    if (schema->bit_width.has_value()) {
        auto bit_width{*schema->bit_width};
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
                auto replacement{*schema};
                replacement.bit_width = bit_width;
                if (apply_document_edit(ReplaceIntegerScalar{.declaration = *declaration,
                                                             .schema = std::move(replacement)})) {
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
            auto replacement{*schema};
            replacement.minimum_value = *minimum;
            replacement.maximum_value = *maximum;
            if (apply_document_edit(ReplaceIntegerScalar{.declaration = *declaration,
                                                         .schema = std::move(replacement)})) {
                integer_scalar_editor_declaration_.reset();
                return true;
            }
        }
    }
    text_disabled_wrapped(
        "This is a semantic domain. A packed field or future representation chooses storage.");

    separator_text_with_tooltip(
        "C++ output policy",
        "Choose whether this semantic type emits C++ constants and a name lookup.");
    auto emit_cpp_constants{schema->cpp_emission != codegen::IntegerScalarCppEmission::none};
    if (ImGui::Checkbox("Emit named constants", &emit_cpp_constants)) {
        auto replacement{*schema};
        replacement.cpp_emission = emit_cpp_constants ? codegen::IntegerScalarCppEmission::constants
                                                      : codegen::IntegerScalarCppEmission::none;
        replacement.cpp_type =
            emit_cpp_constants ? std::optional{codegen::TypeRef{
                                     .name = schema->signedness ? "std::int64_t" : "std::uint64_t",
                                     .suffix = {},
                                     .nested = std::nullopt}}
                               : std::nullopt;
        if (apply_document_edit(ReplaceIntegerScalar{.declaration = *declaration,
                                                     .schema = std::move(replacement)})) {
            integer_scalar_editor_declaration_.reset();
            return true;
        }
    }
    if (emit_cpp_constants) {
        ImGui::SetNextItemWidth(220.0F);
        auto const submitted{ImGui::InputText("Constants type",
                                              integer_scalar_cpp_type_.data(),
                                              integer_scalar_cpp_type_.size(),
                                              ImGuiInputTextFlags_EnterReturnsTrue)};
        if (submitted || ImGui::IsItemDeactivatedAfterEdit()) {
            if (integer_scalar_cpp_type_.front() == '\0') {
                schema_edit_message_ = "C++ constants type cannot be empty.";
            } else if (!schema->cpp_type.has_value() ||
                       schema->cpp_type->name != integer_scalar_cpp_type_.data()) {
                auto replacement{*schema};
                replacement.cpp_type = codegen::TypeRef{
                    .name = integer_scalar_cpp_type_.data(), .suffix = {}, .nested = std::nullopt};
                if (apply_document_edit(ReplaceIntegerScalar{.declaration = *declaration,
                                                             .schema = std::move(replacement)})) {
                    integer_scalar_editor_declaration_.reset();
                    return true;
                }
            }
        }
        auto emit_name_lookup{schema->cpp_emission ==
                              codegen::IntegerScalarCppEmission::constants_with_names};
        if (ImGui::Checkbox("Emit value-to-name lookup", &emit_name_lookup)) {
            auto replacement{*schema};
            replacement.cpp_emission = emit_name_lookup
                                         ? codegen::IntegerScalarCppEmission::constants_with_names
                                         : codegen::IntegerScalarCppEmission::constants;
            if (apply_document_edit(ReplaceIntegerScalar{.declaration = *declaration,
                                                         .schema = std::move(replacement)})) {
                integer_scalar_editor_declaration_.reset();
                return true;
            }
        }
        text_disabled_wrapped("Named codes emit as <Scalar>_<Code>; lookup returns an empty view "
                              "for unnamed values.");
    } else {
        text_disabled_wrapped("No C++ scalar type or constants are emitted for this domain.");
    }

    separator_text_with_tooltip(
        "Semantic relationship",
        "Describe what this value refers to, such as an index, count, or offset into another "
        "type. This does not choose physical storage.");
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
                          codegen::semantic_relation_kind_name(current_kind).data())) {
        for (std::size_t kind_index{}; kind_index < semantic_relationship_kinds.size();
             ++kind_index) {
            auto const kind{semantic_relationship_kinds[kind_index]};
            auto const chosen{integer_scalar_relationship_kind_ == static_cast<int>(kind_index)};
            if (ImGui::Selectable(codegen::semantic_relation_kind_name(kind).data(), chosen)) {
                integer_scalar_relationship_kind_ = static_cast<int>(kind_index);
                if (schema->relationship.has_value()) {
                    auto replacement{*schema};
                    replacement.relationship->kind = kind;
                    replacement.relationship->unit =
                        kind == codegen::SemanticRelationKind::offset_into
                            ? std::optional{current_unit}
                            : std::nullopt;
                    if (apply_document_edit(ReplaceIntegerScalar{
                            .declaration = *declaration, .schema = std::move(replacement)})) {
                        integer_scalar_editor_declaration_.reset();
                        ImGui::EndCombo();
                        return true;
                    }
                }
            }
        }
        ImGui::EndCombo();
    }
    if (current_kind == codegen::SemanticRelationKind::offset_into) {
        ImGui::TextUnformatted("Unit");
        ImGui::SetItemTooltip("The unit used to measure an offset.");
        ImGui::SetNextItemWidth(-1.0F);
        if (ImGui::BeginCombo("##relationship-unit",
                              codegen::semantic_relation_unit_name(current_unit).data())) {
            for (std::size_t unit_index{}; unit_index < semantic_relationship_units.size();
                 ++unit_index) {
                auto const unit{semantic_relationship_units[unit_index]};
                auto const chosen{integer_scalar_relationship_unit_ ==
                                  static_cast<int>(unit_index)};
                if (ImGui::Selectable(codegen::semantic_relation_unit_name(unit).data(), chosen)) {
                    integer_scalar_relationship_unit_ = static_cast<int>(unit_index);
                    if (schema->relationship.has_value()) {
                        auto replacement{*schema};
                        replacement.relationship->unit = unit;
                        if (apply_document_edit(ReplaceIntegerScalar{
                                .declaration = *declaration, .schema = std::move(replacement)})) {
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
    if (schema->relationship.has_value() &&
        (target_submitted || ImGui::IsItemDeactivatedAfterEdit())) {
        if (integer_scalar_relationship_target_.front() == '\0') {
            schema_edit_message_ = "Relationship target cannot be empty.";
        } else {
            auto replacement{*schema};
            replacement.relationship->target.name = integer_scalar_relationship_target_.data();
            if (apply_document_edit(ReplaceIntegerScalar{.declaration = *declaration,
                                                         .schema = std::move(replacement)})) {
                integer_scalar_editor_declaration_.reset();
                return true;
            }
        }
    }
    ImGui::PushID("integer-scalar-relationship-target");
    auto picked_relationship_target{draw_type_picker(node.identity.module_name, node.identity)};
    ImGui::PopID();
    if (picked_relationship_target.has_value()) {
        std::snprintf(integer_scalar_relationship_target_.data(),
                      integer_scalar_relationship_target_.size(),
                      "%s",
                      picked_relationship_target->c_str());
        auto replacement{*schema};
        replacement.relationship = codegen::SemanticRelationSchema{
            .kind = current_kind,
            .target = codegen::TypeRef{.name = *picked_relationship_target,
                                       .suffix = {},
                                       .nested = std::nullopt},
            .unit = current_kind == codegen::SemanticRelationKind::offset_into
                      ? std::optional{current_unit}
                      : std::nullopt};
        if (apply_document_edit(ReplaceIntegerScalar{.declaration = *declaration,
                                                     .schema = std::move(replacement)})) {
            integer_scalar_editor_declaration_.reset();
            return true;
        }
    }
    ImGui::SameLine();
    if (schema->relationship.has_value()) {
        auto const clear_clicked{ImGui::SmallButton("Clear")};
        ImGui::SetItemTooltip("Remove this semantic relationship.");
        if (clear_clicked) {
            auto replacement{*schema};
            replacement.relationship.reset();
            if (apply_document_edit(ReplaceIntegerScalar{.declaration = *declaration,
                                                         .schema = std::move(replacement)})) {
                integer_scalar_editor_declaration_.reset();
                return true;
            }
        }
        ImGui::SameLine();
        if (scalar.relationship.has_value() && detail::semantic_type_navigation_button()) {
            select_type(scalar.relationship->target.type);
            analysis_session_.inputs.selection.field.clear();
            return true;
        }
        if (scalar.relationship.has_value()) {
            ImGui::SetItemTooltip("Open the relationship target.");
        }
    } else {
        ImGui::BeginDisabled(integer_scalar_relationship_target_.front() == '\0');
        auto const add_clicked{ImGui::SmallButton("Add")};
        ImGui::SetItemTooltip("Add a relationship using this kind and target.");
        if (add_clicked) {
            auto replacement{*schema};
            replacement.relationship = codegen::SemanticRelationSchema{
                .kind = current_kind,
                .target = codegen::TypeRef{.name = integer_scalar_relationship_target_.data(),
                                           .suffix = {},
                                           .nested = std::nullopt},
                .unit = current_kind == codegen::SemanticRelationKind::offset_into
                          ? std::optional{current_unit}
                          : std::nullopt};
            if (apply_document_edit(ReplaceIntegerScalar{.declaration = *declaration,
                                                         .schema = std::move(replacement)})) {
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
        separator_text_with_tooltip(
            heading.c_str(),
            "Shows how the current target extent affects the required integer range and width.");
        ImGui::PushTextWrapPos(0.0F);
        ImGui::Text("Target %s: %llu %s",
                    term.data(),
                    static_cast<unsigned long long>(*analysis.relationship_target_extent),
                    unit.data());
        ImGui::Text("Live values: %s",
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
        ImGui::Text("Code-space %s limit: %s",
                    term.data(),
                    detail::format_number(analysis.relationship_code_space_capacity_limit).c_str());
        ImGui::Text("Code-space %s headroom: %s",
                    term.data(),
                    detail::format_number(analysis.relationship_capacity_headroom).c_str());
        ImGui::Text("Semantic-range %s limit: %s",
                    term.data(),
                    detail::format_number(analysis.relationship_semantic_capacity_limit).c_str());
        ImGui::Text("Sentinel-placement %s limit: %s",
                    term.data(),
                    detail::format_number(analysis.relationship_sentinel_capacity_limit).c_str());
        ImGui::Text("Effective valid %s limit: %s",
                    term.data(),
                    detail::format_number(analysis.relationship_effective_capacity_limit).c_str());
        ImGui::Text(
            "Effective valid %s headroom: %s",
            term.data(),
            detail::format_number(analysis.relationship_effective_capacity_headroom).c_str());
        if (kind == codegen::SemanticRelationKind::count_of) {
            text_disabled_wrapped(
                "Live counts include 0 through capacity; named sentinels add code states.");
        } else if (kind == codegen::SemanticRelationKind::offset_into) {
            text_disabled_wrapped(
                "Live offsets span 0 through extent-1 in the declared unit; named sentinels add "
                "code states.");
        } else {
            text_disabled_wrapped(
                "Live indices span 0 through capacity-1; named sentinels add code states.");
        }
        ImGui::PopTextWrapPos();
    }

    auto const code_width{schema->bit_width.value_or(64)};
    auto const named_value{first_available_scalar_code(*schema, code_width, false)};
    auto const sentinel_value{first_available_scalar_code(*schema, code_width, true)};
    separator_text_with_tooltip("Named codes",
                                "Give specific integer values names or reserve them as sentinels.");
    ImGui::BeginDisabled(!named_value.has_value());
    if (ImGui::Button("+ Named code")) {
        auto replacement{*schema};
        auto name{unique_code_name(replacement.named_codes, "Code")};
        replacement.named_codes.push_back({.name = name, .value = *named_value, .sentinel = false});
        if (apply_document_edit(ReplaceIntegerScalar{.declaration = *declaration,
                                                     .schema = std::move(replacement)})) {
            selected_integer_scalar_code_ = std::move(name);
            integer_scalar_editor_declaration_.reset();
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!sentinel_value.has_value());
    if (ImGui::Button("+ Sentinel")) {
        auto replacement{*schema};
        auto name{unique_code_name(replacement.named_codes, "Invalid")};
        replacement.named_codes.push_back(
            {.name = name, .value = *sentinel_value, .sentinel = true});
        if (apply_document_edit(ReplaceIntegerScalar{.declaration = *declaration,
                                                     .schema = std::move(replacement)})) {
            selected_integer_scalar_code_ = std::move(name);
            integer_scalar_editor_declaration_.reset();
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    auto const duplicate_value{
        selected_index.has_value()
            ? first_available_scalar_code(
                  *schema, code_width, schema->named_codes[*selected_index].sentinel)
            : std::nullopt};
    ImGui::BeginDisabled(!selected_index.has_value() || !duplicate_value.has_value());
    if (ImGui::Button("Duplicate")) {
        auto replacement{*schema};
        auto copy{replacement.named_codes[*selected_index]};
        copy.name = unique_code_name(replacement.named_codes, copy.name + "_copy");
        copy.value = *duplicate_value;
        replacement.named_codes.insert(replacement.named_codes.begin() +
                                           static_cast<std::ptrdiff_t>(*selected_index + 1),
                                       copy);
        if (apply_document_edit(ReplaceIntegerScalar{.declaration = *declaration,
                                                     .schema = std::move(replacement)})) {
            selected_integer_scalar_code_ = std::move(copy.name);
            integer_scalar_editor_declaration_.reset();
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() || *selected_index == 0);
    if (ImGui::Button("Move up")) {
        auto replacement{*schema};
        std::swap(replacement.named_codes[*selected_index],
                  replacement.named_codes[*selected_index - 1]);
        if (apply_document_edit(ReplaceIntegerScalar{.declaration = *declaration,
                                                     .schema = std::move(replacement)})) {
            integer_scalar_editor_declaration_.reset();
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value() ||
                         *selected_index + 1 >= schema->named_codes.size());
    if (ImGui::Button("Move down")) {
        auto replacement{*schema};
        std::swap(replacement.named_codes[*selected_index],
                  replacement.named_codes[*selected_index + 1]);
        if (apply_document_edit(ReplaceIntegerScalar{.declaration = *declaration,
                                                     .schema = std::move(replacement)})) {
            integer_scalar_editor_declaration_.reset();
            return true;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected_index.has_value());
    if (ImGui::Button("Delete")) {
        auto replacement{*schema};
        replacement.named_codes.erase(replacement.named_codes.begin() +
                                      static_cast<std::ptrdiff_t>(*selected_index));
        auto const next_code{
            replacement.named_codes.empty()
                ? std::string{}
                : replacement
                      .named_codes[std::min(*selected_index, replacement.named_codes.size() - 1)]
                      .name};
        if (apply_document_edit(ReplaceIntegerScalar{.declaration = *declaration,
                                                     .schema = std::move(replacement)})) {
            selected_integer_scalar_code_ = next_code;
            integer_scalar_editor_declaration_.reset();
            return true;
        }
    }
    ImGui::EndDisabled();

    std::optional<codegen::IntegerScalarSchema> pending;
    auto selected_after_edit{selected_integer_scalar_code_};
    if (detail::begin_editable_table("integer-scalar-codes", 4, schema->named_codes.size())) {
        detail::editable_table_column("Edit", ImGuiTableColumnFlags_WidthFixed);
        detail::editable_table_column("Name");
        detail::editable_table_column("Value");
        detail::editable_table_column("Sentinel", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();
        for (std::size_t index{}; index < schema->named_codes.size(); ++index) {
            auto const& code{schema->named_codes[index]};
            auto const row_selected{selected_integer_scalar_code_ == code.name};
            ImGui::PushID(static_cast<int>(index));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (detail::editable_table_row_handle(row_selected)) {
                selected_integer_scalar_code_ = code.name;
                integer_scalar_editor_code_.clear();
            }
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("INTEGER_SCALAR_CODE_ROW", &index, sizeof(index));
                ImGui::Text("Move %s", code.name.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget()) {
                if (auto const* payload{ImGui::AcceptDragDropPayload("INTEGER_SCALAR_CODE_ROW")}) {
                    auto const source_index{*static_cast<std::size_t const*>(payload->Data)};
                    if (source_index < schema->named_codes.size() && source_index != index) {
                        pending = *schema;
                        selected_after_edit = pending->named_codes[source_index].name;
                        move_element(pending->named_codes, source_index, index);
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
                if (!pending.has_value() && (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                    pending = *schema;
                    pending->named_codes[index].name = integer_scalar_code_name_.data();
                    selected_after_edit = pending->named_codes[index].name;
                }
            } else {
                ImGui::TextUnformatted(code.name.c_str());
            }

            ImGui::TableNextColumn();
            if (row_selected) {
                ImGui::SetNextItemWidth(-1.0F);
                auto const submitted{ImGui::InputText("##value",
                                                      integer_scalar_code_value_.data(),
                                                      integer_scalar_code_value_.size(),
                                                      ImGuiInputTextFlags_EnterReturnsTrue)};
                if (!pending.has_value() && (submitted || ImGui::IsItemDeactivatedAfterEdit())) {
                    if (auto const value{
                            detail::parse_packed_integer(integer_scalar_code_value_.data())}) {
                        pending = *schema;
                        pending->named_codes[index].value = *value;
                    } else {
                        schema_edit_message_ =
                            "Named code value must be a signed decimal or hexadecimal integer.";
                    }
                }
            } else {
                auto const value{codegen::format_packed_integer(code.value)};
                ImGui::TextUnformatted(value.c_str());
            }

            ImGui::TableNextColumn();
            if (row_selected) {
                auto sentinel{integer_scalar_code_sentinel_};
                auto const alternate_value{
                    first_available_scalar_code(*schema, code_width, !code.sentinel)};
                ImGui::BeginDisabled(!alternate_value.has_value());
                if (ImGui::Checkbox("##sentinel", &sentinel)) {
                    pending = *schema;
                    pending->named_codes[index].sentinel = sentinel;
                    pending->named_codes[index].value = *alternate_value;
                }
                ImGui::EndDisabled();
            } else {
                ImGui::TextUnformatted(code.sentinel ? "yes" : "-");
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (pending.has_value()) {
        if (std::ranges::any_of(pending->named_codes,
                                [](auto const& code) { return code.name.empty(); })) {
            schema_edit_message_ = "Named code name cannot be empty.";
            return false;
        }
        if (apply_document_edit(
                ReplaceIntegerScalar{.declaration = *declaration, .schema = std::move(*pending)})) {
            selected_integer_scalar_code_ = std::move(selected_after_edit);
            integer_scalar_editor_declaration_.reset();
            return true;
        }
    }
    return false;
}

auto PlannerUi::draw_linear_quantized_editor(TypeNode const& node, LinearQuantizedType const&)
    -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const declaration{document_->find_declaration(node.identity)};
    if (!declaration.has_value()) {
        return false;
    }
    auto const* schema{document_->linear_quantized_schema(*declaration)};
    if (schema == nullptr) {
        return false;
    }
    linear_quantized_editor_declaration_ = declaration;

    if (ImGui::BeginCombo("Semantic source", schema->source.name.c_str())) {
        for (auto const& candidate : document_->types().types()) {
            if (!std::holds_alternative<IntegerScalarType>(candidate.definition)) {
                continue;
            }
            auto const selected{candidate.cpp_spelling == schema->source.name};
            if (ImGui::Selectable(candidate.cpp_spelling.c_str(), selected)) {
                auto replacement{*schema};
                replacement.source = codegen::TypeRef{
                    .name = candidate.cpp_spelling, .suffix = {}, .nested = std::nullopt};
                ImGui::EndCombo();
                if (apply_document_edit(ReplaceLinearQuantized{.declaration = *declaration,
                                                               .schema = std::move(replacement)})) {
                    linear_quantized_editor_declaration_.reset();
                    return true;
                }
                return false;
            }
        }
        ImGui::EndCombo();
    }

    auto bit_width{schema->bit_width};
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
            auto replacement{*schema};
            replacement.bit_width = bit_width;
            if (apply_document_edit(ReplaceLinearQuantized{.declaration = *declaration,
                                                           .schema = std::move(replacement)})) {
                linear_quantized_editor_declaration_.reset();
                return true;
            }
        }
    }

    auto reserved_codes{schema->reserved_codes};
    ImGui::SetNextItemWidth(180.0F);
    auto const reserved_submitted{ImGui::InputScalar("Reserved codes",
                                                     ImGuiDataType_U64,
                                                     &reserved_codes,
                                                     nullptr,
                                                     nullptr,
                                                     "%llu",
                                                     ImGuiInputTextFlags_EnterReturnsTrue)};
    if (reserved_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
        auto const maximum_reserved{schema->bit_width == 64
                                        ? (std::numeric_limits<std::uint64_t>::max)() - 1
                                        : (std::uint64_t{1} << schema->bit_width) - 2};
        if (reserved_codes > maximum_reserved) {
            schema_edit_message_ = "At least two usable encoded codes are required.";
        } else {
            auto replacement{*schema};
            replacement.reserved_codes = reserved_codes;
            if (apply_document_edit(ReplaceLinearQuantized{.declaration = *declaration,
                                                           .schema = std::move(replacement)})) {
                linear_quantized_editor_declaration_.reset();
                return true;
            }
        }
    }

    auto clipping{schema->clipping == codegen::QuantizationClipping::reject ? 0 : 1};
    constexpr std::array clipping_labels{"Reject", "Clamp"};
    if (ImGui::Combo("Out-of-range values",
                     &clipping,
                     clipping_labels.data(),
                     static_cast<int>(clipping_labels.size()))) {
        auto replacement{*schema};
        replacement.clipping = clipping == 0 ? codegen::QuantizationClipping::reject
                                             : codegen::QuantizationClipping::clamp;
        if (apply_document_edit(ReplaceLinearQuantized{.declaration = *declaration,
                                                       .schema = std::move(replacement)})) {
            linear_quantized_editor_declaration_.reset();
            return true;
        }
    }
    return false;
}

auto PlannerUi::draw_integer_varint_editor(TypeNode const& node, IntegerVarintType const& varint)
    -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const declaration{document_->find_declaration(node.identity)};
    if (!declaration.has_value()) {
        return false;
    }
    auto const* schema{document_->integer_varint_schema(*declaration)};
    if (schema == nullptr) {
        return false;
    }
    integer_varint_editor_declaration_ = declaration;

    if (ImGui::BeginCombo("Semantic source", schema->source.name.c_str())) {
        for (auto const& candidate : document_->types().types()) {
            auto const* scalar{std::get_if<IntegerScalarType>(&candidate.definition)};
            if (scalar == nullptr) {
                continue;
            }
            auto const selected{candidate.cpp_spelling == schema->source.name};
            if (ImGui::Selectable(candidate.cpp_spelling.c_str(), selected)) {
                auto replacement{*schema};
                replacement.source = codegen::TypeRef{
                    .name = candidate.cpp_spelling, .suffix = {}, .nested = std::nullopt};
                if (scalar->signedness &&
                    replacement.encoding == codegen::IntegerVarintEncoding::unsigned_varint) {
                    replacement.encoding = codegen::IntegerVarintEncoding::zigzag_varint;
                } else if (!scalar->signedness) {
                    replacement.encoding = codegen::IntegerVarintEncoding::unsigned_varint;
                }
                ImGui::EndCombo();
                if (apply_document_edit(ReplaceIntegerVarint{.declaration = *declaration,
                                                             .schema = std::move(replacement)})) {
                    integer_varint_editor_declaration_.reset();
                    return true;
                }
                return false;
            }
        }
        ImGui::EndCombo();
    }

    auto const& source_scalar{std::get<IntegerScalarType>(
        analysis_session_.inputs.workspace.types().type(varint.source.type).definition)};
    auto const current_label{codegen::integer_varint_encoding_name(schema->encoding)};
    if (ImGui::BeginCombo("Encoding", current_label.data())) {
        constexpr std::array encodings{codegen::IntegerVarintEncoding::unsigned_varint,
                                       codegen::IntegerVarintEncoding::signed_varint,
                                       codegen::IntegerVarintEncoding::zigzag_varint};
        for (auto const encoding : encodings) {
            auto const compatible{source_scalar.signedness ==
                                  (encoding != codegen::IntegerVarintEncoding::unsigned_varint)};
            if (!compatible) {
                continue;
            }
            auto const label{codegen::integer_varint_encoding_name(encoding)};
            auto const selected{encoding == schema->encoding};
            if (ImGui::Selectable(label.data(), selected)) {
                auto replacement{*schema};
                replacement.encoding = encoding;
                ImGui::EndCombo();
                if (apply_document_edit(ReplaceIntegerVarint{.declaration = *declaration,
                                                             .schema = std::move(replacement)})) {
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

auto PlannerUi::draw_optional_sentinel_editor(TypeNode const& node,
                                              OptionalSentinelType const& optional) -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const declaration{document_->find_declaration(node.identity)};
    if (!declaration.has_value()) {
        return false;
    }
    auto const* schema{document_->optional_sentinel_schema(*declaration)};
    if (schema == nullptr) {
        return false;
    }
    optional_sentinel_editor_declaration_ = declaration;

    auto const& current_source{
        analysis_session_.inputs.workspace.types().type(optional.source.type)};
    if (ImGui::BeginCombo("Source scalar", current_source.cpp_spelling.c_str())) {
        auto const types{analysis_session_.inputs.workspace.types().types()};
        for (std::size_t index{}; index < types.size(); ++index) {
            auto const* candidate{std::get_if<IntegerScalarType>(&types[index].definition)};
            if (candidate == nullptr ||
                !std::ranges::any_of(candidate->named_codes, &PackedNamedCode::sentinel)) {
                continue;
            }
            auto const type{TypeId{static_cast<std::uint32_t>(index)}};
            auto const selected{type == optional.source.type};
            if (ImGui::Selectable(types[index].cpp_spelling.c_str(), selected)) {
                auto replacement{*schema};
                replacement.source = codegen::TypeRef{
                    .name = types[index].cpp_spelling, .suffix = {}, .nested = std::nullopt};
                auto const existing_code{std::ranges::find(
                    candidate->named_codes, replacement.sentinel, &PackedNamedCode::name)};
                if (existing_code == candidate->named_codes.end() || !existing_code->sentinel) {
                    replacement.sentinel =
                        std::ranges::find_if(candidate->named_codes, [](auto const& code) {
                            return code.sentinel;
                        })->name;
                }
                ImGui::EndCombo();
                if (apply_document_edit(ReplaceOptionalSentinel{
                        .declaration = *declaration, .schema = std::move(replacement)})) {
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

    auto const& scalar{std::get<IntegerScalarType>(current_source.definition)};
    if (ImGui::BeginCombo("Absence sentinel", schema->sentinel.c_str())) {
        for (auto const& code : scalar.named_codes) {
            if (!code.sentinel) {
                continue;
            }
            auto const selected{code.name == schema->sentinel};
            auto const value{codegen::format_packed_integer(code.value)};
            auto const label{code.name + " = " + value};
            if (ImGui::Selectable(label.c_str(), selected)) {
                auto replacement{*schema};
                replacement.sentinel = code.name;
                ImGui::EndCombo();
                if (apply_document_edit(ReplaceOptionalSentinel{
                        .declaration = *declaration, .schema = std::move(replacement)})) {
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

auto PlannerUi::draw_optional_presence_bit_editor(TypeNode const& node,
                                                  OptionalPresenceBitType const& optional) -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const declaration{document_->find_declaration(node.identity)};
    if (!declaration.has_value()) {
        return false;
    }
    auto const* schema{document_->optional_presence_bit_schema(*declaration)};
    if (schema == nullptr) {
        return false;
    }
    optional_presence_bit_editor_declaration_ = declaration;

    auto const& current_source{
        analysis_session_.inputs.workspace.types().type(optional.source.type)};
    if (ImGui::BeginCombo("Source scalar", current_source.cpp_spelling.c_str())) {
        auto const types{analysis_session_.inputs.workspace.types().types()};
        for (std::size_t index{}; index < types.size(); ++index) {
            if (!std::holds_alternative<IntegerScalarType>(types[index].definition)) {
                continue;
            }
            auto const type{TypeId{static_cast<std::uint32_t>(index)}};
            auto const selected{type == optional.source.type};
            if (ImGui::Selectable(types[index].cpp_spelling.c_str(), selected)) {
                auto replacement{*schema};
                replacement.source = codegen::TypeRef{
                    .name = types[index].cpp_spelling, .suffix = {}, .nested = std::nullopt};
                ImGui::EndCombo();
                if (apply_document_edit(ReplaceOptionalPresenceBit{
                        .declaration = *declaration, .schema = std::move(replacement)})) {
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

auto PlannerUi::draw_fixed_point_editor(TypeNode const& node, FixedPointType const&) -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const declaration{document_->find_declaration(node.identity)};
    if (!declaration.has_value()) {
        return false;
    }
    auto const* schema{document_->fixed_point_schema(*declaration)};
    if (schema == nullptr) {
        return false;
    }
    fixed_point_editor_declaration_ = declaration;

    auto signedness{schema->signedness};
    if (ImGui::Checkbox("Signed representation", &signedness)) {
        auto replacement{*schema};
        replacement.signedness = signedness;
        if (signedness && replacement.fractional_bits == replacement.total_bits) {
            --replacement.fractional_bits;
        }
        if (apply_document_edit(
                ReplaceFixedPoint{.declaration = *declaration, .schema = std::move(replacement)})) {
            fixed_point_editor_declaration_.reset();
            return true;
        }
    }

    auto total_bits{schema->total_bits};
    ImGui::SetNextItemWidth(150.0F);
    auto const total_submitted{ImGui::InputScalar("Total bit width",
                                                  ImGuiDataType_U32,
                                                  &total_bits,
                                                  nullptr,
                                                  nullptr,
                                                  "%u",
                                                  ImGuiInputTextFlags_EnterReturnsTrue)};
    if (total_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
        auto const minimum_total{schema->fractional_bits + (schema->signedness ? 1U : 0U)};
        if (total_bits == 0 || total_bits > 64 || total_bits < minimum_total) {
            schema_edit_message_ = schema->signedness
                                     ? "Total width must be 1..64 and leave the sign plus all "
                                       "fractional bits."
                                     : "Total width must be 1..64 and contain all fractional "
                                       "bits.";
        } else {
            auto replacement{*schema};
            replacement.total_bits = total_bits;
            if (apply_document_edit(ReplaceFixedPoint{.declaration = *declaration,
                                                      .schema = std::move(replacement)})) {
                fixed_point_editor_declaration_.reset();
                return true;
            }
        }
    }

    auto fractional_bits{schema->fractional_bits};
    ImGui::SetNextItemWidth(150.0F);
    auto const fractional_submitted{ImGui::InputScalar("Fractional bit width",
                                                       ImGuiDataType_U32,
                                                       &fractional_bits,
                                                       nullptr,
                                                       nullptr,
                                                       "%u",
                                                       ImGuiInputTextFlags_EnterReturnsTrue)};
    if (fractional_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
        auto const maximum_fractional{schema->total_bits - (schema->signedness ? 1U : 0U)};
        if (fractional_bits > maximum_fractional) {
            schema_edit_message_ = schema->signedness
                                     ? "Fractional width must leave one sign bit."
                                     : "Fractional width cannot exceed total width.";
        } else {
            auto replacement{*schema};
            replacement.fractional_bits = fractional_bits;
            if (apply_document_edit(ReplaceFixedPoint{.declaration = *declaration,
                                                      .schema = std::move(replacement)})) {
                fixed_point_editor_declaration_.reset();
                return true;
            }
        }
    }

    auto rounding{schema->rounding == codegen::FixedPointRounding::nearest_even ? 0 : 1};
    constexpr std::array rounding_labels{"Nearest even", "Toward zero"};
    if (ImGui::Combo("Rounding policy",
                     &rounding,
                     rounding_labels.data(),
                     static_cast<int>(rounding_labels.size()))) {
        auto replacement{*schema};
        replacement.rounding = rounding == 0 ? codegen::FixedPointRounding::nearest_even
                                             : codegen::FixedPointRounding::toward_zero;
        if (apply_document_edit(
                ReplaceFixedPoint{.declaration = *declaration, .schema = std::move(replacement)})) {
            fixed_point_editor_declaration_.reset();
            return true;
        }
    }
    return false;
}

auto PlannerUi::draw_mini_float_editor(TypeNode const& node, MiniFloatType const&) -> bool {
    if (!document_.has_value()) {
        return false;
    }
    auto const declaration{document_->find_declaration(node.identity)};
    if (!declaration.has_value()) {
        return false;
    }
    auto const* schema{document_->mini_float_schema(*declaration)};
    if (schema == nullptr) {
        return false;
    }

    auto has_sign{schema->sign_bits == 1};
    if (ImGui::Checkbox("Sign bit", &has_sign)) {
        auto replacement{*schema};
        replacement.sign_bits = has_sign ? 1U : 0U;
        auto const total{static_cast<std::uint64_t>(replacement.sign_bits) +
                         replacement.exponent_bits + replacement.significand_bits};
        if (total > 64) {
            schema_edit_message_ = "Mini-float total width must not exceed 64 bits.";
        } else if (apply_document_edit(ReplaceMiniFloat{.declaration = *declaration,
                                                        .schema = std::move(replacement)})) {
            return true;
        }
    }

    auto exponent_bits{schema->exponent_bits};
    ImGui::SetNextItemWidth(150.0F);
    auto const exponent_submitted{ImGui::InputScalar("Exponent bit width",
                                                     ImGuiDataType_U32,
                                                     &exponent_bits,
                                                     nullptr,
                                                     nullptr,
                                                     "%u",
                                                     ImGuiInputTextFlags_EnterReturnsTrue)};
    if (exponent_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
        auto const total{static_cast<std::uint64_t>(schema->sign_bits) + exponent_bits +
                         schema->significand_bits};
        if (exponent_bits < 2 || exponent_bits > 15) {
            schema_edit_message_ = "Exponent width must be in the range 2..15.";
        } else if (total > 64) {
            schema_edit_message_ = "Mini-float total width must not exceed 64 bits.";
        } else {
            auto replacement{*schema};
            replacement.exponent_bits = exponent_bits;
            if (apply_document_edit(ReplaceMiniFloat{.declaration = *declaration,
                                                     .schema = std::move(replacement)})) {
                return true;
            }
        }
    }

    auto significand_bits{schema->significand_bits};
    ImGui::SetNextItemWidth(150.0F);
    auto const significand_submitted{ImGui::InputScalar("Significand bit width",
                                                        ImGuiDataType_U32,
                                                        &significand_bits,
                                                        nullptr,
                                                        nullptr,
                                                        "%u",
                                                        ImGuiInputTextFlags_EnterReturnsTrue)};
    if (significand_submitted || ImGui::IsItemDeactivatedAfterEdit()) {
        auto const total{static_cast<std::uint64_t>(schema->sign_bits) + schema->exponent_bits +
                         significand_bits};
        if (significand_bits > 62) {
            schema_edit_message_ = "Significand width must be in the range 0..62.";
        } else if (total > 64) {
            schema_edit_message_ = "Mini-float total width must not exceed 64 bits.";
        } else {
            auto replacement{*schema};
            replacement.significand_bits = significand_bits;
            if (apply_document_edit(ReplaceMiniFloat{.declaration = *declaration,
                                                     .schema = std::move(replacement)})) {
                return true;
            }
        }
    }

    auto exponent_bias{schema->exponent_bias};
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
            auto replacement{*schema};
            replacement.exponent_bias = exponent_bias;
            if (apply_document_edit(ReplaceMiniFloat{.declaration = *declaration,
                                                     .schema = std::move(replacement)})) {
                return true;
            }
        }
    }
    return false;
}

} // namespace ioj::layout_planner
