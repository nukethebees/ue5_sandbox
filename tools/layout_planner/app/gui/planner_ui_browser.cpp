#include "planner_ui.hpp"

#include "planner_ui_support.hpp"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <variant>

namespace ioj::layout_planner {
namespace {

using namespace layout;

auto lowercase(std::string_view const text) -> std::string {
    std::string result{text};
    std::ranges::transform(result, result.begin(), [](unsigned char const character) {
        return static_cast<char>(std::tolower(character));
    });
    return result;
}

auto matches_filter(LayoutDefinition const& definition, std::string_view const filter) -> bool {
    if (filter.empty()) {
        return true;
    }
    auto const& id{detail::definition_id(definition)};
    auto haystack{id.module_name + " " + id.schema_name};
    if (auto const* soa{std::get_if<SoaLayout>(&definition)};
        soa != nullptr && soa->related_storage_name.has_value()) {
        haystack += " " + *soa->related_storage_name;
    }
    return lowercase(haystack).find(lowercase(filter)) != std::string::npos;
}

auto complete(LayoutDefinition const& definition,
              Variant const& baseline,
              AbiProfile const& abi,
              std::uint64_t const default_capacity) -> bool {
    if (auto const* packed{std::get_if<PackedLayout>(&definition)}) {
        return !detail::has_error(Analyzer::analyze(*packed, baseline, abi).diagnostics);
    }
    auto const& soa{std::get<SoaLayout>(definition)};
    auto const analysis{Analyzer::analyze(soa, baseline, abi, default_capacity)};
    return analysis.total_payload_bytes.has_value() && !detail::has_error(analysis.diagnostics);
}

} // namespace

void PlannerUi::draw_project_panel() {
    ImGui::Begin("Project / Schema");
    ImGui::SetNextItemWidth(-1.0F);
    ImGui::InputTextWithHint("##schema-filter",
                             "Filter schemas and storage names",
                             schema_filter_.data(),
                             schema_filter_.size());

    if (workspace_.catalog().items().empty()) {
        ImGui::TextDisabled("No supported schemas loaded.");
    }

    auto const filter{std::string_view{schema_filter_.data()}};
    auto const& baseline{*workspace_.variant(LayoutWorkspace::baseline_variant_id)};
    for (auto const kind : {SchemaKind::packed_value, SchemaKind::standard_library_soa}) {
        auto const group{kind == SchemaKind::packed_value ? "Packed values" : "SoAs"};
        if (!ImGui::CollapsingHeader(group, ImGuiTreeNodeFlags_DefaultOpen)) {
            continue;
        }

        std::string_view current_module;
        for (auto const& definition : workspace_.catalog().items()) {
            auto const& id{detail::definition_id(definition)};
            if (id.kind != kind || !matches_filter(definition, filter)) {
                continue;
            }
            if (current_module != id.module_name) {
                current_module = id.module_name;
                ImGui::SeparatorText(id.module_name.c_str());
            }
            auto const selected{selected_schema_.has_value() && *selected_schema_ == id};
            ImGui::PushID(id.module_name.c_str());
            if (ImGui::Selectable(id.schema_name.c_str(), selected)) {
                selected_schema_ = id;
                selected_field_.clear();
            }
            ImGui::SameLine();
            auto const status{complete(definition, baseline, abi_, workspace_.default_capacity())
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
