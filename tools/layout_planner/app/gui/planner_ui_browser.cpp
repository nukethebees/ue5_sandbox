#include "planner_ui.hpp"

#include "planner_ui_support.hpp"

#include <imgui.h>

#include <algorithm>
#include <cctype>
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

} // namespace ioj::layout_planner
