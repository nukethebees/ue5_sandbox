#include "planner_ui.hpp"

#include "planner_ui_support.hpp"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

namespace ioj::layout_planner {
namespace {

using namespace layout;
using namespace lispb::schema;

auto lowercase(std::string_view const text) -> std::string {
    std::string result{text};
    std::ranges::transform(result, result.begin(), [](unsigned char const character) {
        return static_cast<char>(std::tolower(character));
    });
    return result;
}

auto visible_type(TypeNode const& node) -> bool {
    if (std::holds_alternative<EnumType>(node.definition)) {
        return true;
    }
    if (std::holds_alternative<PackedType>(node.definition)) {
        return true;
    }
    if (std::holds_alternative<RecordType>(node.definition)) {
        return true;
    }
    if (auto const* soa{std::get_if<SoaType>(&node.definition)};
        soa != nullptr && soa->backend == codegen::SoaBackend::standard_library) {
        return true;
    }
    return false;
}

auto type_kind(TypeNode const& node) -> char const* {
    if (std::holds_alternative<EnumType>(node.definition)) {
        return "enum";
    }
    if (std::holds_alternative<PackedType>(node.definition)) {
        return "packed value";
    }
    if (std::holds_alternative<RecordType>(node.definition)) {
        return "record";
    }
    return "SoA";
}

auto module_label(codegen::ModuleSchema const& module) -> std::string {
    return std::visit(
        [](auto const& value) {
            using Module = std::decay_t<decltype(value)>;
            auto const* kind{std::is_same_v<Module, codegen::EnumModuleSchema> ? "enums"
                             : std::is_same_v<Module, codegen::PackedValueModuleSchema>
                                 ? "packed values"
                             : std::is_same_v<Module, codegen::RecordModuleSchema> ? "records"
                             : std::is_same_v<Module, codegen::SoaModuleSchema>    ? "SoA"
                                                                                   : "vectors"};
            return value.settings.name + "  [" + kind + "]";
        },
        module);
}

auto matches_filter(TypeNode const& node, std::string_view const filter) -> bool {
    if (filter.empty()) {
        return true;
    }
    auto haystack{node.identity.module_name + " " + node.identity.namespace_name + " " +
                  node.identity.name + " " + node.cpp_spelling};
    if (auto const* soa{std::get_if<SoaType>(&node.definition)};
        soa != nullptr && soa->related_storage_name.has_value()) {
        haystack += " " + *soa->related_storage_name;
    }
    return lowercase(haystack).find(lowercase(filter)) != std::string::npos;
}

auto complete(TypeGraph const& types,
              TypeId const type,
              Variant const& baseline,
              AbiProfile const& abi,
              std::uint64_t const default_capacity) -> bool {
    auto const& definition{types.type(type).definition};
    if (std::holds_alternative<EnumType>(definition)) {
        return true;
    }
    if (std::holds_alternative<PackedType>(definition)) {
        return !detail::has_error(Analyzer::analyze_packed(types, type, baseline, abi).diagnostics);
    }
    if (std::holds_alternative<RecordType>(definition)) {
        auto const analysis{Analyzer::analyze_record(types, type, abi)};
        return analysis.size_bytes.has_value() && !detail::has_error(analysis.diagnostics);
    }
    auto const analysis{Analyzer::analyze_soa(types, type, baseline, abi, default_capacity)};
    return analysis.total_payload_bytes.has_value() && !detail::has_error(analysis.diagnostics);
}

} // namespace

void PlannerUi::draw_project_panel() {
    ImGui::Begin("Project / Schema");
    if (!project_path_.empty()) {
        ImGui::TextDisabled("%s", project_path_.string().c_str());
    }
    ImGui::BeginDisabled(!document_.has_value());
    if (ImGui::Button("+ New enum")) {
        open_new_enum_dialog_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("+ New packed value")) {
        open_new_packed_value_dialog_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("+ New SoA")) {
        open_new_soa_dialog_ = true;
    }
    ImGui::EndDisabled();
    if (!schema_edit_message_.empty()) {
        ImGui::SameLine();
        ImGui::TextWrapped("%s", schema_edit_message_.c_str());
    }
    ImGui::SetNextItemWidth(-1.0F);
    ImGui::InputTextWithHint(
        "##schema-filter", "Filter semantic types", schema_filter_.data(), schema_filter_.size());

    auto const types{workspace_.types().types()};
    if (types.empty() || !document_.has_value()) {
        ImGui::TextDisabled("No semantic types loaded.");
    }

    auto const filter{std::string_view{schema_filter_.data()}};
    auto const& baseline{*workspace_.variant(LayoutWorkspace::baseline_variant_id)};
    auto const modules{document_.has_value() ? std::span{document_->manifest().modules}
                                             : std::span<codegen::ModuleSchema const>{}};
    for (std::size_t module_index{}; module_index < modules.size(); ++module_index) {
        std::vector<DeclarationInfo const*> declarations;
        for (auto const& declaration : document_->declarations()) {
            if (declaration.module_index != module_index) {
                continue;
            }
            auto const type{workspace_.types().find(declaration.identity)};
            if (!type.has_value()) {
                continue;
            }
            auto const& node{workspace_.types().type(*type)};
            if (visible_type(node) && matches_filter(node, filter)) {
                declarations.push_back(&declaration);
            }
        }
        if (declarations.empty()) {
            continue;
        }
        std::ranges::sort(declarations, {}, &DeclarationInfo::declaration_index);

        auto const label{module_label(modules[module_index])};
        ImGui::PushID(static_cast<int>(module_index));
        auto const open{ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen)};
        if (ImGui::IsItemHovered() && declarations.front()->source.has_value()) {
            auto const source_index{declarations.front()->source->source_file_index};
            if (source_index < document_->source_files().size()) {
                ImGui::SetTooltip("%s",
                                  document_->source_files()[source_index].path.string().c_str());
            }
        }
        if (open) {
            for (auto const* declaration : declarations) {
                auto const type{*workspace_.types().find(declaration->identity)};
                auto const& node{workspace_.types().type(type)};
                auto const item_label{node.identity.name + "  [" + type_kind(node) + "]"};
                auto const selected{selected_type_.has_value() && *selected_type_ == type};
                ImGui::PushID(static_cast<int>(declaration->id.value));
                if (ImGui::Selectable(item_label.c_str(), selected)) {
                    selected_type_ = type;
                    selected_field_.clear();
                    packed_dragged_divider_.reset();
                    packed_dragged_variant_id_.reset();
                }
                ImGui::SameLine();
                auto const* status{
                    complete(
                        workspace_.types(), type, baseline, abi_, workspace_.default_capacity())
                        ? "facts available"
                        : "contains unknowns"};
                ImGui::TextDisabled("%s", status);
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    if (!load_diagnostics_.empty()) {
        ImGui::SeparatorText("Load diagnostics");
        draw_diagnostics(load_diagnostics_);
    }
    ImGui::End();
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

    auto const& modules{document_->manifest().modules};
    auto first_enum_module{std::optional<std::size_t>{}};
    for (std::size_t index{}; index < modules.size(); ++index) {
        if (std::holds_alternative<codegen::EnumModuleSchema>(modules[index])) {
            first_enum_module = index;
            break;
        }
    }
    if (!first_enum_module.has_value()) {
        ImGui::TextDisabled("The target has no enum module to receive a new declaration.");
    } else {
        if (new_enum_module_index_ >= modules.size() ||
            !std::holds_alternative<codegen::EnumModuleSchema>(modules[new_enum_module_index_])) {
            new_enum_module_index_ = *first_enum_module;
        }
        auto const& selected_module{
            std::get<codegen::EnumModuleSchema>(modules[new_enum_module_index_])};
        if (ImGui::BeginCombo("Module", selected_module.settings.name.c_str())) {
            for (std::size_t index{}; index < modules.size(); ++index) {
                auto const* module{std::get_if<codegen::EnumModuleSchema>(&modules[index])};
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
        ImGui::InputText(
            "Underlying type", new_enum_underlying_type_.data(), new_enum_underlying_type_.size());
        ImGui::TextDisabled("Use a registered name such as @native_uint8 or a C++ spelling.");

        auto const ready{new_enum_name_.front() != '\0' &&
                         new_enum_underlying_type_.front() != '\0'};
        ImGui::BeginDisabled(!ready);
        if (ImGui::Button("Create")) {
            auto const& module{
                std::get<codegen::EnumModuleSchema>(modules[new_enum_module_index_])};
            auto const name{std::string{new_enum_name_.data()}};
            auto const identity{
                TypeIdentity{.origin = TypeOrigin::declaration,
                             .module_name = module.settings.name,
                             .namespace_name = module.settings.namespace_name.value_or(""),
                             .name = name}};
            auto const id{document_->allocate_declaration_id()};
            if (apply_document_edit(
                    CreateEnum{
                        .declaration = id,
                        .module_index = new_enum_module_index_,
                        .schema =
                            codegen::EnumSchema{
                                .name = name,
                                .underlying_type =
                                    codegen::TypeRef{.name = new_enum_underlying_type_.data(),
                                                     .suffix = {},
                                                     .nested = std::nullopt},
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
                    identity)) {
                new_enum_name_.fill('\0');
                std::snprintf(new_enum_underlying_type_.data(),
                              new_enum_underlying_type_.size(),
                              "%s",
                              "std::uint8_t");
                selected_enumerator_ = "Value0";
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
        if (std::holds_alternative<codegen::PackedValueModuleSchema>(modules[index])) {
            first_packed_module = index;
            break;
        }
    }
    if (!first_packed_module.has_value()) {
        ImGui::TextDisabled("The target has no packed-value module to receive a new declaration.");
    } else {
        if (new_packed_module_index_ >= modules.size() ||
            !std::holds_alternative<codegen::PackedValueModuleSchema>(
                modules[new_packed_module_index_])) {
            new_packed_module_index_ = *first_packed_module;
        }
        auto const& selected_module{
            std::get<codegen::PackedValueModuleSchema>(modules[new_packed_module_index_])};
        if (ImGui::BeginCombo("Module", selected_module.settings.name.c_str())) {
            for (std::size_t index{}; index < modules.size(); ++index) {
                auto const* module{std::get_if<codegen::PackedValueModuleSchema>(&modules[index])};
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
        ImGui::TextDisabled("The declaration starts with one editable 1-bit uint8 field.");

        auto const ready{new_packed_value_name_.front() != '\0' &&
                         new_packed_storage_type_.front() != '\0'};
        ImGui::BeginDisabled(!ready);
        if (ImGui::Button("Create")) {
            auto const& module{
                std::get<codegen::PackedValueModuleSchema>(modules[new_packed_module_index_])};
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
                                .fields = {codegen::PackedFieldSchema{
                                    "value",
                                    codegen::TypeRef{.name = "std::uint8_t",
                                                     .suffix = {},
                                                     .nested = std::nullopt},
                                    1}},
                                .invalid_value = std::nullopt,
                                .export_specifier = std::nullopt},
                        .insertion_index = std::nullopt},
                    identity)) {
                new_packed_value_name_.fill('\0');
                std::snprintf(new_packed_storage_type_.data(),
                              new_packed_storage_type_.size(),
                              "%s",
                              "std::uint32_t");
                selected_field_ = "value";
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
        auto const* module{std::get_if<codegen::SoaModuleSchema>(&modules[index])};
        if (module != nullptr && module->backend == codegen::SoaBackend::standard_library) {
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
                std::get_if<codegen::SoaModuleSchema>(&modules[new_soa_module_index_])};
            return module != nullptr && module->backend == codegen::SoaBackend::standard_library;
        }()};
        if (!selected_valid) {
            new_soa_module_index_ = *first_soa_module;
        }
        auto const& selected_module{
            std::get<codegen::SoaModuleSchema>(modules[new_soa_module_index_])};
        if (ImGui::BeginCombo("Module", selected_module.settings.name.c_str())) {
            for (std::size_t index{}; index < modules.size(); ++index) {
                auto const* module{std::get_if<codegen::SoaModuleSchema>(&modules[index])};
                if (module == nullptr || module->backend != codegen::SoaBackend::standard_library) {
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
            auto const& module{std::get<codegen::SoaModuleSchema>(modules[new_soa_module_index_])};
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
                .mask_dimensions = {}}};
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
                selected_field_ = "values";
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
