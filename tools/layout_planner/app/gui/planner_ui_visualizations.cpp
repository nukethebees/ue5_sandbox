#include "planner_ui.hpp"

#include "planner_ui_support.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>

namespace ioj::layout_planner {
namespace {

using namespace layout;

inline constexpr ImVec4 changed_color{0.28F, 0.68F, 0.9F, 1.0F};
inline constexpr ImVec4 selected_color{0.35F, 0.52F, 0.88F, 1.0F};
inline constexpr ImVec4 unused_color{0.25F, 0.27F, 0.3F, 1.0F};
inline constexpr ImVec4 overflow_color{0.85F, 0.55F, 0.2F, 1.0F};

void draw_packed_bar(PackedAnalysis const& analysis,
                     PackedAnalysis const* const baseline,
                     std::string& selected_field,
                     char const* const label,
                     std::uint64_t const common_bits) {
    ImGui::TextUnformatted(label);
    ImGui::SameLine();
    ImGui::TextDisabled("%s — %s",
                        analysis.storage_type.c_str(),
                        detail::format_number(analysis.storage_bits).c_str());

    auto const available{std::max(1.0F, ImGui::GetContentRegionAvail().x)};
    auto const origin{ImGui::GetCursorScreenPos()};
    constexpr float height{74.0F};
    auto const denominator{static_cast<float>(std::max<std::uint64_t>(1, common_bits))};
    auto const storage_width{available * static_cast<float>(analysis.storage_bits.value_or(0)) /
                             denominator};
    auto* draw_list{ImGui::GetWindowDrawList()};
    draw_list->AddRect({origin.x, origin.y},
                       {origin.x + storage_width, origin.y + height},
                       ImGui::GetColorU32(ImGuiCol_Border),
                       3.0F,
                       0,
                       2.0F);

    if (analysis.unused_bits.value_or(0) != 0 && analysis.bits_used.has_value()) {
        auto const left{origin.x +
                        available * static_cast<float>(*analysis.bits_used) / denominator};
        draw_list->AddRectFilled({left, origin.y},
                                 {origin.x + storage_width, origin.y + height},
                                 ImGui::GetColorU32(unused_color),
                                 2.0F);
    }

    for (std::size_t index{}; index < analysis.fields.size(); ++index) {
        auto const& field{analysis.fields[index]};
        auto const left{origin.x +
                        available * static_cast<float>(field.least_significant_bit) / denominator};
        auto const right{origin.x +
                         available *
                             static_cast<float>(field.least_significant_bit + field.bit_width) /
                             denominator};
        auto const width{std::max(2.0F, right - left)};
        auto const changed{baseline != nullptr && index < baseline->fields.size() &&
                           baseline->fields[index].bit_width != field.bit_width};
        auto const selected{selected_field == field.name};
        auto const color{selected  ? selected_color
                         : changed ? changed_color
                                   : ImVec4{0.18F + 0.06F * (index % 2), 0.36F, 0.52F, 1.0F}};
        draw_list->AddRectFilled(
            {left, origin.y}, {left + width, origin.y + height}, ImGui::GetColorU32(color), 2.0F);
        draw_list->AddRect({left, origin.y},
                           {left + width, origin.y + height},
                           ImGui::GetColorU32(ImGuiCol_Border));
        if (field.least_significant_bit >= analysis.storage_bits.value_or(0)) {
            draw_list->AddLine({left, origin.y},
                               {left + width, origin.y + height},
                               ImGui::GetColorU32(overflow_color),
                               2.0F);
        }

        if (width > 72.0F) {
            draw_list->AddText({left + 6.0F, origin.y + 8.0F},
                               ImGui::GetColorU32(ImGuiCol_Text),
                               field.name.c_str());
            auto const field_bits{std::to_string(field.bit_width) + " bits"};
            draw_list->AddText({left + 6.0F, origin.y + 34.0F},
                               ImGui::GetColorU32(ImGuiCol_TextDisabled),
                               field_bits.c_str());
        }
    }

    if (analysis.overflow_bits.value_or(0) != 0) {
        auto const overflow_left{origin.x + storage_width};
        auto const overflow_right{origin.x + available * static_cast<float>(*analysis.bits_used) /
                                                 denominator};
        draw_list->AddRectFilled({overflow_left, origin.y},
                                 {overflow_right, origin.y + height},
                                 ImGui::GetColorU32(overflow_color),
                                 0.0F);
        draw_list->AddText({overflow_left + 4.0F, origin.y + height - 20.0F},
                           ImGui::GetColorU32(ImGuiCol_Text),
                           "overflow");
    }
    draw_list->AddText(
        {origin.x, origin.y + height + 3.0F}, ImGui::GetColorU32(ImGuiCol_TextDisabled), "bit 0");
    if (analysis.storage_bits.has_value()) {
        auto const text{std::to_string(*analysis.storage_bits)};
        draw_list->AddText({origin.x + storage_width - ImGui::CalcTextSize(text.c_str()).x,
                            origin.y + height + 3.0F},
                           ImGui::GetColorU32(ImGuiCol_TextDisabled),
                           text.c_str());
    }
    ImGui::PushID(label);
    ImGui::InvisibleButton("packed-layout", {available, height});
    auto const clicked{ImGui::IsItemClicked()};
    auto const hovered{ImGui::IsItemHovered()};
    ImGui::PopID();
    if (hovered) {
        auto const mouse_x{ImGui::GetIO().MousePos.x};
        for (auto const& field : analysis.fields) {
            auto const left{origin.x + available *
                                           static_cast<float>(field.least_significant_bit) /
                                           denominator};
            auto const right{origin.x + available *
                                            static_cast<float>(field.least_significant_bit +
                                                               field.bit_width) /
                                            denominator};
            auto const width{std::max(2.0F, right - left)};
            if (mouse_x < left || mouse_x >= left + width) {
                continue;
            }
            if (clicked) {
                selected_field = field.name;
            }
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(field.name.c_str());
            ImGui::Text("Logical type: %s", field.logical_type.c_str());
            ImGui::Text("Planning width: %u bits", field.bit_width);
            if (field.most_significant_bit.has_value()) {
                ImGui::Text("Bit range: [%llu:%llu]",
                            static_cast<unsigned long long>(*field.most_significant_bit),
                            static_cast<unsigned long long>(field.least_significant_bit));
            }
            ImGui::Text("Maximum unsigned value: %s",
                        detail::format_number(field.maximum_unsigned_value).c_str());
            ImGui::EndTooltip();
            break;
        }
    }
    ImGui::Dummy({available, 24.0F});
}

void draw_payload_regions(SoaAnalysis const& analysis,
                          SoaAnalysis const* const baseline,
                          std::string& selected_field,
                          char const* const label,
                          std::uint64_t const common_total) {
    ImGui::TextUnformatted(label);
    ImGui::SameLine();
    ImGui::TextDisabled("capacity %llu — %s",
                        static_cast<unsigned long long>(analysis.capacity),
                        detail::format_bytes(analysis.total_payload_bytes).c_str());
    if (!analysis.total_payload_bytes.has_value()) {
        ImGui::TextDisabled(
            "Aggregate payload is unknown because one or more column facts are unknown.");
        return;
    }

    auto const available{std::max(1.0F, ImGui::GetContentRegionAvail().x)};
    auto const origin{ImGui::GetCursorScreenPos()};
    constexpr float height{64.0F};
    auto const denominator{static_cast<float>(std::max<std::uint64_t>(1, common_total))};
    auto x{origin.x};
    auto const total_width{available * static_cast<float>(*analysis.total_payload_bytes) /
                           denominator};
    auto* draw_list{ImGui::GetWindowDrawList()};
    for (std::size_t index{}; index < analysis.columns.size(); ++index) {
        auto const& column{analysis.columns[index]};
        if (!column.total_bytes.has_value()) {
            continue;
        }
        auto const width{
            std::max(2.0F, available * static_cast<float>(*column.total_bytes) / denominator)};
        auto const changed{baseline != nullptr && index < baseline->columns.size() &&
                           baseline->columns[index].physical_type != column.physical_type};
        auto const selected{selected_field == column.name};
        auto const color{selected  ? selected_color
                         : changed ? changed_color
                                   : ImVec4{0.17F + 0.05F * (index % 3), 0.38F, 0.52F, 1.0F}};
        draw_list->AddRectFilled(
            {x, origin.y}, {x + width, origin.y + height}, ImGui::GetColorU32(color), 2.0F);
        draw_list->AddRect(
            {x, origin.y}, {x + width, origin.y + height}, ImGui::GetColorU32(ImGuiCol_Border));
        if (width > 78.0F) {
            draw_list->AddText({x + 5.0F, origin.y + 7.0F},
                               ImGui::GetColorU32(ImGuiCol_Text),
                               column.name.c_str());
            auto const bytes{detail::format_bytes(column.total_bytes)};
            draw_list->AddText({x + 5.0F, origin.y + 32.0F},
                               ImGui::GetColorU32(ImGuiCol_TextDisabled),
                               bytes.c_str());
        }
        x += width;
    }
    draw_list->AddRect({origin.x, origin.y},
                       {origin.x + total_width, origin.y + height},
                       ImGui::GetColorU32(ImGuiCol_Border),
                       2.0F,
                       0,
                       2.0F);
    ImGui::PushID(label);
    ImGui::InvisibleButton("payload-layout", {available, height});
    auto const clicked{ImGui::IsItemClicked()};
    auto const hovered{ImGui::IsItemHovered()};
    ImGui::PopID();
    if (hovered) {
        auto const mouse_x{ImGui::GetIO().MousePos.x};
        auto x{origin.x};
        for (auto const& column : analysis.columns) {
            if (!column.total_bytes.has_value()) {
                continue;
            }
            auto const width{std::max(
                2.0F,
                available * static_cast<float>(*column.total_bytes) / denominator)};
            if (mouse_x >= x && mouse_x < x + width) {
                if (clicked) {
                    selected_field = column.name;
                }
                ImGui::BeginTooltip();
                ImGui::TextUnformatted(column.name.c_str());
                ImGui::Text("Schema type: %s", column.schema_type.c_str());
                ImGui::Text("Planning type: %s", column.physical_type.c_str());
                ImGui::Text(
                    "Element size: %s",
                    detail::format_number(column.type_facts.transform([](TypeFacts const& facts) {
                        return facts.size_bytes;
                    })).c_str());
                ImGui::Text("Payload: %s", detail::format_bytes(column.total_bytes).c_str());
                ImGui::Text("Minimum cache lines: %s",
                            detail::format_number(column.minimum_cache_lines).c_str());
                ImGui::EndTooltip();
                break;
            }
            x += width;
        }
    }
    ImGui::Dummy({available, ImGui::GetStyle().ItemSpacing.y});
}

void draw_cache_line(CacheLineTiling const& tiling) {
    ImGui::SeparatorText("64-byte cache-line view");
    ImGui::Text("%llu-byte element | minimum %llu line%s per element",
                static_cast<unsigned long long>(tiling.element_bytes),
                static_cast<unsigned long long>(tiling.minimum_cache_lines_per_element),
                tiling.minimum_cache_lines_per_element == 1 ? "" : "s");
    if (tiling.element_bytes > tiling.cache_line_bytes) {
        ImGui::TextDisabled("This element spans multiple cache lines.");
        return;
    }

    auto const origin{ImGui::GetCursorScreenPos()};
    auto const available{std::max(1.0F, ImGui::GetContentRegionAvail().x)};
    constexpr float height{34.0F};
    auto* draw_list{ImGui::GetWindowDrawList()};
    draw_list->AddRectFilled({origin.x, origin.y},
                             {origin.x + available, origin.y + height},
                             ImGui::GetColorU32(ImVec4{0.14F, 0.18F, 0.22F, 1.0F}));
    for (std::uint64_t block{}; block <= 16; ++block) {
        auto const x{origin.x + available * static_cast<float>(block) / 16.0F};
        draw_list->AddLine(
            {x, origin.y}, {x, origin.y + height}, ImGui::GetColorU32(ImGuiCol_Border));
    }
    auto const element_width{available * static_cast<float>(tiling.element_bytes) /
                             static_cast<float>(tiling.cache_line_bytes)};
    for (auto offset{std::uint64_t{0}}; offset < tiling.cache_line_bytes;
         offset += tiling.element_bytes) {
        auto const x{origin.x + available * static_cast<float>(offset) /
                                    static_cast<float>(tiling.cache_line_bytes)};
        draw_list->AddLine(
            {x, origin.y}, {x, origin.y + height}, ImGui::GetColorU32(changed_color), 2.0F);
        if (tiling.exact_elements_per_cache_line.has_value() &&
            *tiling.exact_elements_per_cache_line <= 16) {
            auto const index{std::to_string(offset / tiling.element_bytes)};
            draw_list->AddText({x + std::min(4.0F, element_width * 0.1F), origin.y + 8.0F},
                               ImGui::GetColorU32(ImGuiCol_Text),
                               index.c_str());
        }
    }
    ImGui::Dummy({available, height});
    if (tiling.exact_elements_per_cache_line.has_value()) {
        ImGui::Text("%llu elements / 64-byte line",
                    static_cast<unsigned long long>(*tiling.exact_elements_per_cache_line));
    } else {
        ImGui::Text(
            "%llu complete elements, then %llu byte%s of the next element in an aligned line.",
            static_cast<unsigned long long>(tiling.complete_elements_from_line_start),
            static_cast<unsigned long long>(tiling.boundary_fragment_bytes),
            tiling.boundary_fragment_bytes == 1 ? "" : "s");
    }
    ImGui::TextDisabled(
        "This shows byte tiling only; allocation alignment and CPU behavior are not predicted.");
}

} // namespace

void PlannerUi::draw_packed_layout(PackedLayout const&,
                                   PackedAnalysis const& baseline,
                                   PackedAnalysis const& active) {
    ImGui::Text("%s", active.id.schema_name.c_str());
    ImGui::TextDisabled("%s", active.id.module_name.c_str());
    auto const common_bits{std::max({baseline.storage_bits.value_or(0),
                                     baseline.bits_used.value_or(0),
                                     active.storage_bits.value_or(0),
                                     active.bits_used.value_or(0),
                                     std::uint64_t{1}})};
    draw_packed_bar(baseline, nullptr, selected_field_, "Baseline", common_bits);
    if (workspace_.active_variant_id() != LayoutWorkspace::baseline_variant_id) {
        draw_packed_bar(active, &baseline, selected_field_, "Variant", common_bits);
    }
    if (active.unused_bits.value_or(0) != 0) {
        ImGui::TextDisabled("Grey region: %llu unused storage bit%s.",
                            static_cast<unsigned long long>(*active.unused_bits),
                            *active.unused_bits == 1 ? "" : "s");
    }
    if (active.overflow_bits.value_or(0) != 0) {
        ImGui::TextColored(overflow_color,
                           "%llu planned bit%s exceed storage.",
                           static_cast<unsigned long long>(*active.overflow_bits),
                           *active.overflow_bits == 1 ? "" : "s");
    }
}

void PlannerUi::draw_soa_layout(SoaLayout const& layout,
                                SoaAnalysis const& baseline,
                                SoaAnalysis const& active) {
    ImGui::Text("%s", active.id.schema_name.c_str());
    ImGui::TextDisabled("%s", active.id.module_name.c_str());
    if (layout.related_storage_name.has_value()) {
        ImGui::TextDisabled("Related generated storage: %s (padding and gaps not modeled).",
                            layout.related_storage_name->c_str());
    }
    if (ImGui::BeginTable("soa-columns",
                          7,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Column");
        ImGui::TableSetupColumn("Schema type");
        ImGui::TableSetupColumn("Planning type");
        ImGui::TableSetupColumn("Element B");
        ImGui::TableSetupColumn("Payload");
        ImGui::TableSetupColumn("Min lines");
        ImGui::TableSetupColumn("Exact / 64 B");
        ImGui::TableHeadersRow();
        for (auto const& column : active.columns) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            auto const selected{selected_field_ == column.name};
            if (ImGui::Selectable(
                    column.name.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
                selected_field_ = column.name;
            }
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(column.schema_type.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(column.physical_type.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(
                detail::format_number(column.type_facts.transform([](TypeFacts const& facts) {
                    return facts.size_bytes;
                })).c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(detail::format_bytes(column.total_bytes).c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(detail::format_number(column.minimum_cache_lines).c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(detail::format_number(column.elements_per_cache_line).c_str());
        }
        ImGui::EndTable();
    }

    auto const common_total{
        std::max(baseline.total_payload_bytes.value_or(0), active.total_payload_bytes.value_or(0))};
    ImGui::SeparatorText("Aggregate column payload");
    draw_payload_regions(baseline, nullptr, selected_field_, "Baseline", common_total);
    if (workspace_.active_variant_id() != LayoutWorkspace::baseline_variant_id) {
        draw_payload_regions(active, &baseline, selected_field_, "Variant", common_total);
    }
    ImGui::TextDisabled("Regions compare aggregate column payload; standard-library columns remain "
                        "separate allocations.");

    auto const found{std::ranges::find(active.columns, selected_field_, &SoaColumnAnalysis::name)};
    if (found != active.columns.end() && found->cache_line_tiling.has_value()) {
        draw_cache_line(*found->cache_line_tiling);
    }
}

} // namespace ioj::layout_planner
