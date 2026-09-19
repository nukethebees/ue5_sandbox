#include "planner_ui.hpp"

#include "planner_ui_support.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace ioj::layout_planner {
namespace {

using namespace layout;
using namespace lispb::schema;

inline constexpr ImVec4 changed_color{0.28F, 0.68F, 0.9F, 1.0F};
inline constexpr ImVec4 selected_color{0.35F, 0.52F, 0.88F, 1.0F};
inline constexpr ImVec4 unused_color{0.36F, 0.39F, 0.43F, 1.0F};
inline constexpr ImVec4 unused_hatch_color{0.82F, 0.85F, 0.89F, 0.45F};
inline constexpr ImVec4 bit_grid_color{0.86F, 0.9F, 0.95F, 0.2F};
inline constexpr ImVec4 packed_detail_text_color{0.9F, 0.93F, 0.98F, 1.0F};
inline constexpr ImVec4 overflow_color{0.85F, 0.55F, 0.2F, 1.0F};

struct PackedDividerAdjustment {
    std::size_t left_field_index{};
    std::uint32_t left_width{};
    std::uint32_t right_width{};
};

void draw_packed_bar(PackedAnalysis const& analysis,
                     PackedAnalysis const* const baseline,
                     std::string& selected_field,
                     char const* const label,
                     std::uint64_t const common_bits,
                     bool const editable,
                     std::optional<std::size_t>& dragged_divider,
                     std::optional<PackedDividerAdjustment>& adjustment) {
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

    bool has_unused{};
    float unused_left{};
    float unused_right{};
    if (analysis.unused_bits.value_or(0) != 0 && analysis.bits_used.has_value()) {
        has_unused = true;
        unused_left = origin.x + available * static_cast<float>(*analysis.bits_used) / denominator;
        unused_right = origin.x + storage_width;
        draw_list->AddRectFilled({unused_left, origin.y},
                                 {unused_right, origin.y + height},
                                 ImGui::GetColorU32(unused_color),
                                 2.0F);
        draw_list->PushClipRect({unused_left, origin.y}, {unused_right, origin.y + height}, true);
        constexpr float hatch_spacing{8.0F};
        for (auto x{unused_left - height}; x < unused_right; x += hatch_spacing) {
            draw_list->AddLine({x, origin.y + height},
                               {x + height, origin.y},
                               ImGui::GetColorU32(unused_hatch_color));
        }
        draw_list->PopClipRect();
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
    }

    if (analysis.storage_bits.has_value()) {
        auto const grid_step{*analysis.storage_bits > 128 ? std::uint64_t{8} : std::uint64_t{1}};
        for (auto bit{grid_step}; bit < *analysis.storage_bits; bit += grid_step) {
            auto const x{origin.x + available * static_cast<float>(bit) / denominator};
            draw_list->AddLine(
                {x, origin.y}, {x, origin.y + height}, ImGui::GetColorU32(bit_grid_color));
        }
    }

    if (has_unused) {
        draw_list->AddRect({unused_left, origin.y},
                           {unused_right, origin.y + height},
                           ImGui::GetColorU32(unused_hatch_color),
                           2.0F,
                           0,
                           1.5F);
        auto const label_text{std::to_string(*analysis.unused_bits) + " unused"};
        auto const label_size{ImGui::CalcTextSize(label_text.c_str())};
        if (unused_right - unused_left > label_size.x + 8.0F) {
            draw_list->AddText({unused_left + (unused_right - unused_left - label_size.x) * 0.5F,
                                origin.y + height * 0.5F - label_size.y * 0.5F},
                               ImGui::GetColorU32(ImGuiCol_Text),
                               label_text.c_str());
        }
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
        if (width > 72.0F) {
            draw_list->AddText({left + 6.0F, origin.y + 8.0F},
                               ImGui::GetColorU32(ImGuiCol_Text),
                               field.name.c_str());
            auto const field_bits{std::to_string(field.bit_width) + " bits"};
            draw_list->AddText({left + 6.0F, origin.y + 34.0F},
                               ImGui::GetColorU32(packed_detail_text_color),
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
    auto const mouse_position{ImGui::GetIO().MousePos};
    std::optional<std::size_t> hovered_divider;
    if (editable) {
        constexpr float divider_handle_half_width{5.0F};
        for (std::size_t index{}; index + 1 < analysis.fields.size(); ++index) {
            auto const& field{analysis.fields[index]};
            auto const divider_x{
                origin.x + available *
                               static_cast<float>(field.least_significant_bit + field.bit_width) /
                               denominator};
            auto const within_handle{hovered && std::abs(mouse_position.x - divider_x) <=
                                                    divider_handle_half_width};
            auto const active{dragged_divider.has_value() && *dragged_divider == index};
            auto const divider_color{active || within_handle ? ImGui::GetColorU32(selected_color)
                                                             : ImGui::GetColorU32(ImGuiCol_Border)};
            draw_list->AddLine({divider_x, origin.y},
                               {divider_x, origin.y + height},
                               divider_color,
                               active || within_handle ? 3.0F : 1.0F);
            if (within_handle) {
                hovered_divider = index;
            }
        }
    }
    if (editable && (hovered_divider.has_value() || dragged_divider.has_value())) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }
    if (editable && clicked && hovered_divider.has_value()) {
        dragged_divider = hovered_divider;
    }
    if (hovered) {
        auto const mouse_x{mouse_position.x};
        for (auto const& field : analysis.fields) {
            auto const left{origin.x + available * static_cast<float>(field.least_significant_bit) /
                                           denominator};
            auto const right{origin.x +
                             available *
                                 static_cast<float>(field.least_significant_bit + field.bit_width) /
                                 denominator};
            auto const width{std::max(2.0F, right - left)};
            if (mouse_x < left || mouse_x >= left + width) {
                continue;
            }
            if (clicked && !hovered_divider.has_value()) {
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
    if (editable && dragged_divider.has_value()) {
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            dragged_divider.reset();
        } else if (*dragged_divider + 1 < analysis.fields.size()) {
            auto const index{*dragged_divider};
            auto const& left_field{analysis.fields[index]};
            auto const& right_field{analysis.fields[index + 1]};
            auto const pair_width{static_cast<std::uint64_t>(left_field.bit_width) +
                                  static_cast<std::uint64_t>(right_field.bit_width)};
            if (pair_width >= 2) {
                auto const minimum_left{std::max(
                    std::uint64_t{1}, pair_width > 64 ? pair_width - 64 : std::uint64_t{1})};
                auto const maximum_left{std::min(std::uint64_t{64}, pair_width - 1)};
                if (minimum_left <= maximum_left) {
                    auto const left_edge{origin.x +
                                         available *
                                             static_cast<float>(left_field.least_significant_bit) /
                                             denominator};
                    auto const relative_bits{
                        std::round((mouse_position.x - left_edge) * denominator / available)};
                    auto const requested_left{relative_bits <= 0.0F
                                                  ? std::uint64_t{0}
                                                  : static_cast<std::uint64_t>(relative_bits)};
                    auto const new_left{std::clamp(requested_left, minimum_left, maximum_left)};
                    auto const new_right{pair_width - new_left};
                    if (new_left != left_field.bit_width) {
                        adjustment = {.left_field_index = index,
                                      .left_width = static_cast<std::uint32_t>(new_left),
                                      .right_width = static_cast<std::uint32_t>(new_right)};
                    }
                }
            }
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
            auto const width{
                std::max(2.0F, available * static_cast<float>(*column.total_bytes) / denominator)};
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

void PlannerUi::draw_packed_layout(PackedType const& packed,
                                   PackedAnalysis const& baseline,
                                   PackedAnalysis const& active) {
    auto const& identity{workspace_.types().type(active.type).identity};
    ImGui::Text("%s", identity.name.c_str());
    ImGui::TextDisabled("%s", identity.module_name.c_str());
    auto const common_bits{std::max({baseline.storage_bits.value_or(0),
                                     baseline.bits_used.value_or(0),
                                     active.storage_bits.value_or(0),
                                     active.bits_used.value_or(0),
                                     std::uint64_t{1}})};
    std::optional<std::size_t> baseline_divider;
    std::optional<PackedDividerAdjustment> adjustment;
    draw_packed_bar(baseline,
                    nullptr,
                    selected_field_,
                    "Baseline",
                    common_bits,
                    false,
                    baseline_divider,
                    adjustment);
    if (workspace_.active_variant_id() != LayoutWorkspace::baseline_variant_id) {
        draw_packed_bar(active,
                        &baseline,
                        selected_field_,
                        "Variant",
                        common_bits,
                        true,
                        packed_dragged_divider_,
                        adjustment);
        ImGui::TextDisabled("Drag a divider to transfer whole bits between adjacent fields.");
    }
    if (adjustment.has_value() && adjustment->left_field_index + 1 < packed.fields.size()) {
        auto const& left_field{packed.fields[adjustment->left_field_index]};
        auto const& right_field{packed.fields[adjustment->left_field_index + 1]};
        workspace_.set_packed_field_width(
            active.type,
            left_field.name,
            adjustment->left_width == left_field.bit_width
                ? std::optional<std::uint32_t>{}
                : std::optional<std::uint32_t>{adjustment->left_width});
        workspace_.set_packed_field_width(
            active.type,
            right_field.name,
            adjustment->right_width == right_field.bit_width
                ? std::optional<std::uint32_t>{}
                : std::optional<std::uint32_t>{adjustment->right_width});
    }
    if (active.unused_bits.value_or(0) != 0) {
        ImGui::TextDisabled("Hatched region: %llu unused storage bit%s.",
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

void PlannerUi::draw_soa_layout(SoaType const& soa,
                                SoaAnalysis const& baseline,
                                SoaAnalysis const& active) {
    auto const& identity{workspace_.types().type(active.type).identity};
    ImGui::Text("%s", identity.name.c_str());
    ImGui::TextDisabled("%s", identity.module_name.c_str());
    if (soa.related_storage_name.has_value()) {
        ImGui::TextDisabled("Related generated storage: %s (padding and gaps not modeled).",
                            soa.related_storage_name->c_str());
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
