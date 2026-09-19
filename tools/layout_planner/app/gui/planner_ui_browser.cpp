#include "planner_ui.hpp"

#include "planner_ui_support.hpp"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <string_view>

namespace ioj::layout_planner {
namespace {

using namespace layout;
using namespace lispb::schema;

enum class BrowserGroup { enumeration, packed, soa };

auto lowercase(std::string_view const text) -> std::string {
    std::string result{text};
    std::ranges::transform(result, result.begin(), [](unsigned char const character) {
        return static_cast<char>(std::tolower(character));
    });
    return result;
}

auto browser_group(TypeNode const& node) -> std::optional<BrowserGroup> {
    if (std::holds_alternative<EnumType>(node.definition)) {
        return BrowserGroup::enumeration;
    }
    if (std::holds_alternative<PackedType>(node.definition)) {
        return BrowserGroup::packed;
    }
    if (auto const* soa{std::get_if<SoaType>(&node.definition)};
        soa != nullptr && soa->backend == codegen::SoaBackend::standard_library) {
        return BrowserGroup::soa;
    }
    return std::nullopt;
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
    auto const analysis{Analyzer::analyze_soa(types, type, baseline, abi, default_capacity)};
    return analysis.total_payload_bytes.has_value() && !detail::has_error(analysis.diagnostics);
}

} // namespace

void PlannerUi::draw_project_panel() {
    ImGui::Begin("Project / Schema");
    ImGui::BeginDisabled(!document_.has_value());
    if (ImGui::Button("+ New enum")) {
        open_new_enum_dialog_ = true;
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
    if (types.empty()) {
        ImGui::TextDisabled("No semantic types loaded.");
    }

    auto const filter{std::string_view{schema_filter_.data()}};
    auto const& baseline{*workspace_.variant(LayoutWorkspace::baseline_variant_id)};
    for (auto const group : {BrowserGroup::enumeration, BrowserGroup::packed, BrowserGroup::soa}) {
        auto const* label{group == BrowserGroup::enumeration ? "Enums"
                          : group == BrowserGroup::packed    ? "Packed values"
                                                             : "SoAs"};
        if (!ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen)) {
            continue;
        }

        std::string_view current_module;
        for (std::size_t index{}; index < types.size(); ++index) {
            auto const& node{types[index]};
            if (browser_group(node) != group || !matches_filter(node, filter)) {
                continue;
            }
            if (current_module != node.identity.module_name) {
                current_module = node.identity.module_name;
                ImGui::SeparatorText(current_module.data());
            }
            auto const type{TypeId{static_cast<std::uint32_t>(index)}};
            auto const selected{selected_type_.has_value() && *selected_type_ == type};
            ImGui::PushID(static_cast<int>(index));
            if (ImGui::Selectable(node.identity.name.c_str(), selected)) {
                selected_type_ = type;
                selected_field_.clear();
                packed_dragged_divider_.reset();
                packed_dragged_variant_id_.reset();
            }
            ImGui::SameLine();
            auto const* status{
                complete(workspace_.types(), type, baseline, abi_, workspace_.default_capacity())
                    ? "facts available"
                    : "contains unknowns"};
            ImGui::TextDisabled("%s", status);
            ImGui::PopID();
        }
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

} // namespace ioj::layout_planner
