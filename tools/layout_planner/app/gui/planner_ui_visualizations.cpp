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
#include <vector>

namespace ioj::layout_planner {
namespace {

using namespace layout;
using namespace lispb::schema;

inline constexpr ImVec4 changed_color{0.28F, 0.68F, 0.9F, 1.0F};
inline constexpr ImVec4 selected_color{0.35F, 0.52F, 0.88F, 1.0F};
inline constexpr ImVec4 unused_color{0.36F, 0.39F, 0.43F, 1.0F};
inline constexpr ImVec4 bit_grid_color{0.86F, 0.9F, 0.95F, 0.2F};
inline constexpr ImVec4 packed_detail_text_color{0.9F, 0.93F, 0.98F, 1.0F};
inline constexpr ImVec4 overflow_color{0.85F, 0.55F, 0.2F, 1.0F};
inline constexpr ImVec4 read_access_color{0.25F, 0.72F, 0.48F, 1.0F};
inline constexpr ImVec4 write_access_color{0.9F, 0.43F, 0.28F, 1.0F};
inline constexpr ImVec4 read_write_access_color{0.72F, 0.42F, 0.9F, 1.0F};

struct PackedDividerAdjustment {
    std::size_t left_field_index{};
    std::uint32_t left_width{};
    std::uint32_t right_width{};
};

void draw_packed_bar(PackedAnalysis const& analysis,
                     PackedAnalysis const* const baseline,
                     std::string& selected_field,
                     std::map<std::string, AccessOperation, std::less<>> const& access_fields,
                     char const* const label,
                     std::uint64_t const common_bits,
                     bool const editable,
                     std::optional<std::size_t>& dragged_divider,
                     std::optional<PackedDividerAdjustment>& adjustment,
                     bool& activated) {
    ImGui::TextUnformatted(label);
    ImGui::SameLine();
    ImGui::TextDisabled("%s — %s",
                        analysis.storage_type.c_str(),
                        detail::format_number(analysis.storage_bits).c_str());

    auto const available{std::max(1.0F, ImGui::GetContentRegionAvail().x)};
    auto const origin{ImGui::GetCursorScreenPos()};
    constexpr float height{74.0F};
    auto const denominator{static_cast<float>(std::max<std::uint64_t>(1, common_bits))};
    auto const most_significant_first{analysis.bit_order ==
                                      codegen::PackedBitOrder::most_significant_first};
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
    std::uint64_t unused_begin{};
    std::uint64_t unused_end{};
    if (analysis.unused_bits.value_or(0) != 0 && analysis.bits_used.has_value()) {
        has_unused = true;
        unused_begin = most_significant_first ? 0 : *analysis.bits_used;
        unused_end = unused_begin + *analysis.unused_bits;
        if (most_significant_first) {
            unused_left = origin.x;
            unused_right =
                origin.x + available * static_cast<float>(*analysis.unused_bits) / denominator;
        } else {
            unused_left =
                origin.x + available * static_cast<float>(*analysis.bits_used) / denominator;
            unused_right = origin.x + storage_width;
        }
    }

    for (std::size_t index{}; index < analysis.fields.size(); ++index) {
        auto const& field{analysis.fields[index]};
        if (most_significant_first && !field.most_significant_bit.has_value()) {
            continue;
        }
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
                         : field.reserved
                             ? ImVec4{0.34F, 0.30F, 0.24F, 1.0F}
                             : ImVec4{0.18F + 0.06F * (index % 2), 0.36F, 0.52F, 1.0F}};
        draw_list->AddRectFilled(
            {left, origin.y}, {left + width, origin.y + height}, ImGui::GetColorU32(color), 2.0F);
        draw_list->AddRect({left, origin.y},
                           {left + width, origin.y + height},
                           ImGui::GetColorU32(ImGuiCol_Border));
        auto const access{access_fields.find(field.name)};
        if (!field.reserved && access != access_fields.end()) {
            auto const access_color{access->second == AccessOperation::read ? read_access_color
                                    : access->second == AccessOperation::write
                                        ? write_access_color
                                        : read_write_access_color};
            constexpr float access_band_height{7.0F};
            draw_list->AddRectFilled({left + 1.0F, origin.y + height - access_band_height},
                                     {left + width - 1.0F, origin.y + height - 1.0F},
                                     ImGui::GetColorU32(access_color));
        }
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
        auto const range{std::to_string(unused_begin) + ".." + std::to_string(unused_end - 1)};
        detail::draw_labeled_gap(draw_list,
                                 {unused_left, origin.y},
                                 {unused_right, origin.y + height},
                                 "unused bits " + range,
                                 "unused " + range);
    }

    for (std::size_t index{}; index < analysis.fields.size(); ++index) {
        auto const& field{analysis.fields[index]};
        if (most_significant_first && !field.most_significant_bit.has_value()) {
            continue;
        }
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
        auto const overflow_width{available * static_cast<float>(*analysis.overflow_bits) /
                                  denominator};
        auto const overflow_left{most_significant_first ? origin.x : origin.x + storage_width};
        auto const overflow_right{
            most_significant_first
                ? origin.x + overflow_width
                : origin.x + available * static_cast<float>(*analysis.bits_used) / denominator};
        draw_list->AddRectFilled({overflow_left, origin.y},
                                 {overflow_right, origin.y + height},
                                 ImGui::GetColorU32(overflow_color),
                                 0.0F);
        draw_list->AddText({overflow_left + 4.0F, origin.y + height - 20.0F},
                           ImGui::GetColorU32(ImGuiCol_Text),
                           most_significant_first ? "overflow below bit 0" : "overflow");
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
    activated = clicked;
    auto const hovered{ImGui::IsItemHovered()};
    ImGui::PopID();
    auto const mouse_position{ImGui::GetIO().MousePos};
    std::optional<std::size_t> hovered_divider;
    if (editable) {
        constexpr float divider_handle_half_width{5.0F};
        for (std::size_t index{}; index + 1 < analysis.fields.size(); ++index) {
            auto const& field{analysis.fields[index]};
            auto const& next_field{analysis.fields[index + 1]};
            if (most_significant_first && (!field.most_significant_bit.has_value() ||
                                           !next_field.most_significant_bit.has_value())) {
                continue;
            }
            auto const divider_bit{most_significant_first
                                       ? field.least_significant_bit
                                       : field.least_significant_bit + field.bit_width};
            auto const divider_x{origin.x +
                                 available * static_cast<float>(divider_bit) / denominator};
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
        if (has_unused && mouse_x >= unused_left && mouse_x < unused_right) {
            ImGui::SetTooltip("Unused storage bits %llu..%llu",
                              static_cast<unsigned long long>(unused_begin),
                              static_cast<unsigned long long>(unused_end - 1));
        }
        for (auto const& field : analysis.fields) {
            if (most_significant_first && !field.most_significant_bit.has_value()) {
                continue;
            }
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
            ImGui::Text(
                "%s width: %u bits", field.reserved ? "Reserved" : "Planning", field.bit_width);
            if (auto const access{access_fields.find(field.name)};
                !field.reserved && access != access_fields.end()) {
                ImGui::Text("Workload operation: %s",
                            detail::access_operation_name(access->second));
            }
            if (field.schema_bit_width_auto) {
                ImGui::Text("Source width: auto => %u bits", field.schema_bit_width);
            }
            if (field.most_significant_bit.has_value()) {
                ImGui::Text("Bit range: [%llu:%llu]",
                            static_cast<unsigned long long>(*field.most_significant_bit),
                            static_cast<unsigned long long>(field.least_significant_bit));
            }
            if (!field.reserved) {
                if (field.minimum_signed_value.has_value() &&
                    field.maximum_signed_value.has_value()) {
                    ImGui::Text("Signed range: %lld..%lld",
                                static_cast<long long>(*field.minimum_signed_value),
                                static_cast<long long>(*field.maximum_signed_value));
                } else {
                    ImGui::Text("Maximum unsigned value: %s",
                                detail::format_number(field.maximum_unsigned_value).c_str());
                }
                if (field.minimum_semantic_value.has_value()) {
                    auto const minimum{
                        codegen::format_packed_integer(*field.minimum_semantic_value)};
                    auto const maximum{
                        codegen::format_packed_integer(*field.maximum_semantic_value)};
                    ImGui::Text("Semantic range: %s..%s", minimum.c_str(), maximum.c_str());
                    ImGui::Text("Semantic values: %s",
                                detail::format_number(field.semantic_value_count).c_str());
                    ImGui::Text("Sentinel codes: %llu",
                                static_cast<unsigned long long>(field.sentinel_code_count));
                    ImGui::Text("Required codes: %s",
                                detail::format_number(field.required_code_count).c_str());
                    ImGui::Text("Minimum direct width: %u bits", *field.minimum_required_bits);
                    ImGui::Text("Unused field codes: %s",
                                detail::format_number(field.unused_codes).c_str());
                }
                for (auto const& code : field.named_codes) {
                    auto const value{codegen::format_packed_integer(code.value)};
                    ImGui::Text("%s = %s%s",
                                code.name.c_str(),
                                value.c_str(),
                                code.sentinel ? " (sentinel)" : "");
                }
                if (field.relationship_kind.has_value() && field.relationship_target.has_value()) {
                    ImGui::Text(
                        "Relationship: %s -> %s",
                        std::string{codegen::semantic_relation_kind_name(*field.relationship_kind)}
                            .c_str(),
                        field.relationship_target->c_str());
                    if (field.relationship_target_extent.has_value()) {
                        auto const kind{*field.relationship_kind};
                        auto const term{detail::relationship_extent_term(kind)};
                        auto const unit{
                            detail::relationship_extent_unit(kind, field.relationship_unit)};
                        ImGui::Separator();
                        ImGui::Text(
                            "Session target %s: %llu %s",
                            term.data(),
                            static_cast<unsigned long long>(*field.relationship_target_extent),
                            unit.data());
                        ImGui::Text(
                            "Required live values: %s",
                            field.relationship_live_value_count.has_value()
                                ? detail::format_code_count(*field.relationship_live_value_count)
                                      .c_str()
                                : "Unknown");
                        auto const required_codes{
                            field.relationship_required_code_count.has_value()
                                ? detail::format_code_count(*field.relationship_required_code_count)
                            : field.relationship_minimum_required_bits.value_or(0) > 64
                                ? std::string{"> 2^64"}
                                : std::string{"Unknown"}};
                        ImGui::Text("Required codes: %s", required_codes.c_str());
                        ImGui::Text(
                            "Minimum width: %s",
                            field.relationship_minimum_required_bits.has_value()
                                ? (std::to_string(*field.relationship_minimum_required_bits) +
                                   " bits")
                                      .c_str()
                                : "Unknown");
                        ImGui::Text("Planning width fits: %s",
                                    field.relationship_width_sufficient.has_value()
                                        ? (*field.relationship_width_sufficient ? "Yes" : "No")
                                        : "Unknown");
                        ImGui::Text(
                            "Code-space %s limit: %s",
                            term.data(),
                            detail::format_number(field.relationship_code_space_capacity_limit)
                                .c_str());
                        ImGui::Text(
                            "Code-space %s headroom: %s",
                            term.data(),
                            detail::format_number(field.relationship_capacity_headroom).c_str());
                        ImGui::Text(
                            "Semantic-range %s limit: %s",
                            term.data(),
                            detail::format_number(field.relationship_semantic_capacity_limit)
                                .c_str());
                        ImGui::Text(
                            "Sentinel-placement %s limit: %s",
                            term.data(),
                            detail::format_number(field.relationship_sentinel_capacity_limit)
                                .c_str());
                        ImGui::Text(
                            "Effective valid %s limit: %s",
                            term.data(),
                            detail::format_number(field.relationship_effective_capacity_limit)
                                .c_str());
                        ImGui::Text(
                            "Effective valid %s headroom: %s",
                            term.data(),
                            detail::format_number(field.relationship_effective_capacity_headroom)
                                .c_str());
                        if (kind == codegen::SemanticRelationKind::count_of) {
                            ImGui::TextDisabled(
                                "count_of includes the terminal count 0..capacity.");
                        } else if (kind == codegen::SemanticRelationKind::offset_into) {
                            ImGui::TextDisabled(
                                "offset_into uses live offsets 0..extent-1 in the declared unit.");
                        } else {
                            ImGui::TextDisabled("index_into uses live indices 0..capacity-1.");
                        }
                    }
                }
            }
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
            if (left_field.reserved || right_field.reserved ||
                (most_significant_first && (!left_field.most_significant_bit.has_value() ||
                                            !right_field.most_significant_bit.has_value()))) {
                dragged_divider.reset();
            } else {
                auto const pair_width{static_cast<std::uint64_t>(left_field.bit_width) +
                                      static_cast<std::uint64_t>(right_field.bit_width)};
                auto const minimum_left{std::max(
                    std::uint64_t{1}, pair_width > 64 ? pair_width - 64 : std::uint64_t{1})};
                auto const maximum_left{std::min(std::uint64_t{64}, pair_width - 1)};
                if (pair_width >= 2 && minimum_left <= maximum_left) {
                    auto const pair_low_bit{most_significant_first
                                                ? right_field.least_significant_bit
                                                : left_field.least_significant_bit};
                    auto const pair_low{origin.x +
                                        available * static_cast<float>(pair_low_bit) / denominator};
                    auto const relative_bits{
                        std::round((mouse_position.x - pair_low) * denominator / available)};
                    auto const relative_from_low{relative_bits <= 0.0F
                                                     ? std::uint64_t{0}
                                                     : static_cast<std::uint64_t>(relative_bits)};
                    auto const requested_left{most_significant_first
                                                  ? (relative_from_low >= pair_width
                                                         ? std::uint64_t{0}
                                                         : pair_width - relative_from_low)
                                                  : relative_from_low};
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

auto soa_alignment_gaps(SoaAnalysis const& analysis)
    -> std::vector<std::pair<std::uint64_t, std::uint64_t>> {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> gaps;
    if (analysis.allocation_strategy != SoaAllocationStrategy::aligned_contiguous ||
        !analysis.total_allocation_bytes.has_value()) {
        return gaps;
    }

    std::vector<std::pair<std::uint64_t, std::uint64_t>> occupied;
    for (auto const& column : analysis.columns) {
        if (!column.allocation_offset_bytes.has_value() || !column.total_bytes.has_value() ||
            *column.allocation_offset_bytes > *analysis.total_allocation_bytes ||
            *column.total_bytes >
                *analysis.total_allocation_bytes - *column.allocation_offset_bytes) {
            return {};
        }
        occupied.emplace_back(*column.allocation_offset_bytes,
                              *column.allocation_offset_bytes + *column.total_bytes);
    }
    std::ranges::sort(occupied);
    auto next_byte{std::uint64_t{0}};
    for (auto const& [begin, end] : occupied) {
        if (begin > next_byte) {
            gaps.emplace_back(next_byte, begin);
        }
        next_byte = std::max(next_byte, end);
    }
    if (next_byte < *analysis.total_allocation_bytes) {
        gaps.emplace_back(next_byte, *analysis.total_allocation_bytes);
    }
    return gaps;
}

void draw_payload_regions(SoaAnalysis const& analysis,
                          SoaAnalysis const* const baseline,
                          std::string& selected_field,
                          char const* const label,
                          std::uint64_t const common_total,
                          bool& activated) {
    ImGui::TextUnformatted(label);
    ImGui::SameLine();
    ImGui::TextDisabled("capacity %llu — %s",
                        static_cast<unsigned long long>(analysis.capacity),
                        detail::format_bytes(analysis.total_allocation_bytes).c_str());
    if (!analysis.total_allocation_bytes.has_value()) {
        ImGui::TextDisabled(
            "Allocation is unknown because one or more column size/alignment facts are unknown.");
        return;
    }

    auto const available{std::max(1.0F, ImGui::GetContentRegionAvail().x)};
    auto const origin{ImGui::GetCursorScreenPos()};
    constexpr float height{64.0F};
    auto const denominator{static_cast<float>(std::max<std::uint64_t>(1, common_total))};
    auto x{origin.x};
    auto const total_width{available * static_cast<float>(*analysis.total_allocation_bytes) /
                           denominator};
    auto const gaps{soa_alignment_gaps(analysis)};
    auto* draw_list{ImGui::GetWindowDrawList()};
    draw_list->AddRectFilled({origin.x, origin.y},
                             {origin.x + total_width, origin.y + height},
                             ImGui::GetColorU32(unused_color),
                             2.0F);
    for (std::size_t index{}; index < analysis.columns.size(); ++index) {
        auto const& column{analysis.columns[index]};
        if (!column.total_bytes.has_value()) {
            continue;
        }
        if (column.allocation_offset_bytes.has_value()) {
            x = origin.x +
                available * static_cast<float>(*column.allocation_offset_bytes) / denominator;
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
        if (!column.allocation_offset_bytes.has_value()) {
            x += width;
        }
    }
    for (auto const& [begin, end] : gaps) {
        auto const left{origin.x + available * static_cast<float>(begin) / denominator};
        auto const right{origin.x + available * static_cast<float>(end) / denominator};
        auto const range{std::to_string(begin) + ".." + std::to_string(end - 1)};
        detail::draw_labeled_gap(draw_list,
                                 {left, origin.y},
                                 {right, origin.y + height},
                                 "padding " + range,
                                 "pad " + range);
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
    activated = clicked;
    auto const hovered{ImGui::IsItemHovered()};
    ImGui::PopID();
    if (hovered) {
        auto const mouse_x{ImGui::GetIO().MousePos.x};
        for (auto const& [begin, end] : gaps) {
            auto const left{origin.x + available * static_cast<float>(begin) / denominator};
            auto const right{origin.x + available * static_cast<float>(end) / denominator};
            if (mouse_x >= left && mouse_x < right) {
                ImGui::SetTooltip("Alignment padding: bytes %llu..%llu",
                                  static_cast<unsigned long long>(begin),
                                  static_cast<unsigned long long>(end - 1));
                break;
            }
        }
        auto x{origin.x};
        for (auto const& column : analysis.columns) {
            if (!column.total_bytes.has_value()) {
                continue;
            }
            if (column.allocation_offset_bytes.has_value()) {
                x = origin.x +
                    available * static_cast<float>(*column.allocation_offset_bytes) / denominator;
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
                ImGui::Text("Block offset: %s",
                            detail::format_bytes(column.allocation_offset_bytes).c_str());
                ImGui::Text("Padding before: %s",
                            detail::format_bytes(column.padding_before_bytes).c_str());
                ImGui::Text("Minimum cache lines: %s",
                            detail::format_number(column.minimum_cache_lines).c_str());
                ImGui::EndTooltip();
                break;
            }
            if (!column.allocation_offset_bytes.has_value()) {
                x += width;
            }
        }
    }
    for (auto const& [begin, end] : gaps) {
        ImGui::TextWrapped("Alignment padding: bytes %llu..%llu",
                           static_cast<unsigned long long>(begin),
                           static_cast<unsigned long long>(end - 1));
    }
    ImGui::Dummy({available, ImGui::GetStyle().ItemSpacing.y});
}

void draw_soa_region_map(SoaAnalysis const& analysis,
                         SoaAccessAnalysis const* const access,
                         bool const pages,
                         float const pixels_per_region) {
    auto const region_bytes{pages ? analysis.page_bytes : analysis.cache_line_bytes};
    auto const* const region_name{pages ? "page" : "cache line"};
    if (!analysis.total_allocation_bytes.has_value() || !region_bytes.has_value() ||
        *region_bytes == 0) {
        ImGui::TextDisabled("The contiguous block or selected target region size is Unknown.");
        return;
    }
    if (*analysis.total_allocation_bytes == 0) {
        ImGui::TextDisabled("The zero-capacity block occupies no target regions.");
        return;
    }

    auto const total_regions{*analysis.total_allocation_bytes / *region_bytes +
                             (*analysis.total_allocation_bytes % *region_bytes == 0 ? 0U : 1U)};
    constexpr std::uint64_t maximum_regions{50'000};
    if (total_regions > maximum_regions) {
        ImGui::TextDisabled("The block spans %llu %ss, above the %llu-region visualization limit.",
                            static_cast<unsigned long long>(total_regions),
                            region_name,
                            static_cast<unsigned long long>(maximum_regions));
        return;
    }

    auto const requested_width{static_cast<double>(total_regions) * pixels_per_region};
    constexpr double maximum_canvas_width{16'000'000.0};
    if (requested_width > maximum_canvas_width) {
        ImGui::TextDisabled(
            "This zoom would create a %.1f Mpx canvas. Reduce zoom below the 16 Mpx limit.",
            requested_width / 1'000'000.0);
        return;
    }
    auto const content_width{
        std::max(ImGui::GetContentRegionAvail().x, static_cast<float>(requested_width))};
    constexpr float canvas_height{128.0F};
    if (!ImGui::BeginChild(
            "soa-region-map", {0.0F, 156.0F}, true, ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::EndChild();
        return;
    }
    auto const origin{ImGui::GetCursorScreenPos()};
    ImGui::InvisibleButton("soa-region-map-canvas", {content_width, canvas_height});
    auto const hovered{ImGui::IsItemHovered()};
    auto* const draw_list{ImGui::GetWindowDrawList()};
    auto const pixels_per_byte{static_cast<double>(pixels_per_region) /
                               static_cast<double>(*region_bytes)};
    auto const block_right{
        origin.x + static_cast<float>(static_cast<double>(*analysis.total_allocation_bytes) *
                                      pixels_per_byte)};
    draw_list->AddRectFilled({origin.x, origin.y + 24.0F},
                             {block_right, origin.y + 114.0F},
                             ImGui::GetColorU32(unused_color));
    auto const gaps{soa_alignment_gaps(analysis)};

    for (std::size_t index{}; index < analysis.columns.size(); ++index) {
        auto const& column{analysis.columns[index]};
        if (!column.allocation_offset_bytes.has_value() || !column.total_bytes.has_value()) {
            continue;
        }
        auto const left{origin.x +
                        static_cast<float>(static_cast<double>(*column.allocation_offset_bytes) *
                                           pixels_per_byte)};
        auto const right{
            left + static_cast<float>(static_cast<double>(*column.total_bytes) * pixels_per_byte)};
        auto const color{ImVec4{0.17F + 0.05F * (index % 3), 0.38F, 0.52F, 0.88F}};
        draw_list->AddRectFilled(
            {left, origin.y + 26.0F}, {right, origin.y + 76.0F}, ImGui::GetColorU32(color));
        draw_list->AddRect({left, origin.y + 26.0F},
                           {right, origin.y + 76.0F},
                           ImGui::GetColorU32(ImGuiCol_Border));
        if (right - left > 54.0F) {
            draw_list->AddText({left + 4.0F, origin.y + 42.0F},
                               ImGui::GetColorU32(ImGuiCol_Text),
                               column.name.c_str());
        }
    }
    for (auto const& [begin, end] : gaps) {
        auto const left{origin.x +
                        static_cast<float>(static_cast<double>(begin) * pixels_per_byte)};
        auto const right{origin.x + static_cast<float>(static_cast<double>(end) * pixels_per_byte)};
        auto const range{std::to_string(begin) + ".." + std::to_string(end - 1)};
        detail::draw_labeled_gap(draw_list,
                                 {left, origin.y + 26.0F},
                                 {right, origin.y + 76.0F},
                                 "padding " + range,
                                 "pad " + range);
    }

    if (access != nullptr && access->footprint_exact) {
        for (auto const& selected : access->columns) {
            auto const column{
                std::ranges::find(analysis.columns, selected.name, &SoaColumnAnalysis::name)};
            if (column == analysis.columns.end() || !column->allocation_offset_bytes.has_value() ||
                !selected.useful_bytes.has_value()) {
                continue;
            }
            auto const left{origin.x + static_cast<float>(
                                           static_cast<double>(*column->allocation_offset_bytes) *
                                           pixels_per_byte)};
            auto const right{left + static_cast<float>(static_cast<double>(*selected.useful_bytes) *
                                                       pixels_per_byte)};
            auto const color{selected.operation == AccessOperation::read
                                 ? ImVec4{0.20F, 0.64F, 0.92F, 0.95F}
                             : selected.operation == AccessOperation::write
                                 ? ImVec4{0.94F, 0.48F, 0.20F, 0.95F}
                                 : ImVec4{0.72F, 0.36F, 0.90F, 0.95F}};
            draw_list->AddRectFilled(
                {left, origin.y + 82.0F}, {right, origin.y + 110.0F}, ImGui::GetColorU32(color));
            draw_list->AddRect({left, origin.y + 82.0F},
                               {right, origin.y + 110.0F},
                               ImGui::GetColorU32(ImGuiCol_Border));
        }
    }

    auto const scroll_x{ImGui::GetScrollX()};
    auto const visible_width{ImGui::GetWindowWidth()};
    auto const first_region{
        static_cast<std::uint64_t>(std::max(0.0F, std::floor(scroll_x / pixels_per_region)))};
    auto const last_region{std::min(
        total_regions,
        static_cast<std::uint64_t>(std::ceil((scroll_x + visible_width) / pixels_per_region)) + 1)};
    auto const label_stride{std::max(
        std::uint64_t{1}, static_cast<std::uint64_t>(std::ceil(56.0F / pixels_per_region)))};
    for (auto region{first_region}; region <= last_region; ++region) {
        auto const x{origin.x + static_cast<float>(region) * pixels_per_region};
        draw_list->AddLine(
            {x, origin.y + 18.0F}, {x, origin.y + 116.0F}, ImGui::GetColorU32(bit_grid_color));
        if (region < total_regions && region % label_stride == 0) {
            auto const label{std::to_string(region)};
            draw_list->AddText(
                {x + 3.0F, origin.y}, ImGui::GetColorU32(ImGuiCol_TextDisabled), label.c_str());
        }
    }
    draw_list->AddRect({origin.x, origin.y + 24.0F},
                       {block_right, origin.y + 114.0F},
                       ImGui::GetColorU32(ImGuiCol_Border),
                       0.0F,
                       0,
                       2.0F);

    if (hovered) {
        auto const mouse_x{ImGui::GetIO().MousePos.x};
        auto const byte_offset{static_cast<std::uint64_t>(
            std::max(0.0, std::floor(static_cast<double>(mouse_x - origin.x) / pixels_per_byte)))};
        if (byte_offset < *analysis.total_allocation_bytes) {
            auto const column = std::ranges::find_if(analysis.columns, [&](auto const& candidate) {
                return candidate.allocation_offset_bytes.has_value() &&
                       candidate.total_bytes.has_value() &&
                       byte_offset >= *candidate.allocation_offset_bytes &&
                       byte_offset - *candidate.allocation_offset_bytes < *candidate.total_bytes;
            });
            ImGui::BeginTooltip();
            ImGui::Text("Block byte: %llu", static_cast<unsigned long long>(byte_offset));
            ImGui::Text("%s: %llu",
                        pages ? "Page" : "Cache line",
                        static_cast<unsigned long long>(byte_offset / *region_bytes));
            if (column == analysis.columns.end()) {
                auto const gap{std::ranges::find_if(gaps, [&](auto const& candidate) {
                    return byte_offset >= candidate.first && byte_offset < candidate.second;
                })};
                if (gap != gaps.end()) {
                    ImGui::Text("Alignment padding: bytes %llu..%llu",
                                static_cast<unsigned long long>(gap->first),
                                static_cast<unsigned long long>(gap->second - 1));
                } else {
                    ImGui::TextUnformatted("Unclassified block byte");
                }
            } else {
                ImGui::Text("Column: %s", column->name.c_str());
                ImGui::Text("Column byte: %llu",
                            static_cast<unsigned long long>(byte_offset -
                                                            *column->allocation_offset_bytes));
                if (access != nullptr && access->footprint_exact) {
                    auto const selected{std::ranges::find(
                        access->columns, column->name, &SoaColumnAccessAnalysis::name)};
                    if (selected != access->columns.end() && selected->useful_bytes.has_value() &&
                        byte_offset - *column->allocation_offset_bytes < *selected->useful_bytes) {
                        ImGui::Text("Selected access: %s",
                                    detail::access_operation_name(selected->operation));
                    }
                }
            }
            ImGui::EndTooltip();
        }
    }
    ImGui::EndChild();
    ImGui::TextDisabled("Capacity blocks");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4{0.20F, 0.64F, 0.92F, 1.0F}, "Read");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4{0.94F, 0.48F, 0.20F, 1.0F}, "Write");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4{0.72F, 0.36F, 0.90F, 1.0F}, "Read + write");
}

void draw_cache_line(CacheLineTiling const& tiling) {
    auto const heading{std::to_string(tiling.cache_line_bytes) + "-byte cache-line view"};
    ImGui::SeparatorText(heading.c_str());
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
        ImGui::Text("%llu elements / %llu-byte line",
                    static_cast<unsigned long long>(*tiling.exact_elements_per_cache_line),
                    static_cast<unsigned long long>(tiling.cache_line_bytes));
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

void draw_footprint_composition(char const* const label,
                                std::optional<std::uint64_t> const useful_bytes,
                                std::optional<std::uint64_t> const footprint_bytes,
                                std::optional<std::uint64_t> const non_useful_bytes) {
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    if (!useful_bytes.has_value() || !footprint_bytes.has_value() ||
        !non_useful_bytes.has_value() || *useful_bytes > *footprint_bytes ||
        *non_useful_bytes != *footprint_bytes - *useful_bytes) {
        ImGui::TextDisabled("Unknown or inconsistent footprint facts.");
        ImGui::PopID();
        return;
    }
    if (*footprint_bytes == 0) {
        ImGui::TextDisabled("No footprint at the current element count.");
        ImGui::PopID();
        return;
    }

    auto const available{std::max(1.0F, ImGui::GetContentRegionAvail().x)};
    constexpr float height{20.0F};
    auto const origin{ImGui::GetCursorScreenPos()};
    auto const useful_width{available *
                            static_cast<float>(static_cast<long double>(*useful_bytes) /
                                               static_cast<long double>(*footprint_bytes))};
    auto* draw_list{ImGui::GetWindowDrawList()};
    draw_list->AddRectFilled({origin.x, origin.y},
                             {origin.x + available, origin.y + height},
                             ImGui::GetColorU32(unused_color),
                             2.0F);
    if (useful_width > 0.0F) {
        draw_list->AddRectFilled({origin.x, origin.y},
                                 {origin.x + useful_width, origin.y + height},
                                 ImGui::GetColorU32(selected_color),
                                 2.0F);
    }
    draw_list->AddRect({origin.x, origin.y},
                       {origin.x + available, origin.y + height},
                       ImGui::GetColorU32(ImGuiCol_Border),
                       2.0F);
    ImGui::InvisibleButton("footprint-composition", {available, height});
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Useful: %s", detail::format_bytes(useful_bytes).c_str());
        ImGui::Text("Non-useful: %s", detail::format_bytes(non_useful_bytes).c_str());
        ImGui::Text("Footprint: %s", detail::format_bytes(footprint_bytes).c_str());
        ImGui::EndTooltip();
    }
    ImGui::TextDisabled("%s useful | %s non-useful | %s total",
                        detail::format_bytes(useful_bytes).c_str(),
                        detail::format_bytes(non_useful_bytes).c_str(),
                        detail::format_bytes(footprint_bytes).c_str());
    ImGui::PopID();
}

} // namespace

auto PlannerUi::draw_element_count() -> bool {
    struct CountPreset {
        char const* label;
        std::uint64_t count;
    };
    constexpr std::array count_presets{CountPreset{"1", 1},
                                       CountPreset{"100", 100},
                                       CountPreset{"1K", 1'000},
                                       CountPreset{"10K", 10'000},
                                       CountPreset{"100K", 100'000},
                                       CountPreset{"1M", 1'000'000}};
    bool changed{};
    for (auto const& preset : count_presets) {
        ImGui::PushID(preset.label);
        if (ImGui::SmallButton(preset.label)) {
            changed = analysis_session_.inputs.workspace.set_element_count(preset.count) || changed;
        }
        ImGui::PopID();
        ImGui::SameLine();
    }
    auto custom_count{analysis_session_.inputs.workspace.element_count()};
    ImGui::SetNextItemWidth(150.0F);
    if (ImGui::InputScalar("Elements", ImGuiDataType_U64, &custom_count) && custom_count != 0) {
        changed = analysis_session_.inputs.workspace.set_element_count(custom_count) || changed;
    }
    return changed;
}

auto PlannerUi::draw_access_operation() -> bool {
    bool changed{};
    auto operation_index{static_cast<int>(analysis_session_.inputs.access_operation)};
    ImGui::SetNextItemWidth(180.0F);
    if (ImGui::Combo("Default / all", &operation_index, "Read\0Write\0Read + write\0")) {
        analysis_session_.inputs.access_operation = static_cast<AccessOperation>(operation_index);
        for (auto& [name, operation] : analysis_session_.inputs.selection.packed_access_fields) {
            static_cast<void>(name);
            operation = analysis_session_.inputs.access_operation;
        }
        for (auto& [name, operation] : analysis_session_.inputs.selection.record_access_members) {
            static_cast<void>(name);
            operation = analysis_session_.inputs.access_operation;
        }
        for (auto& [name, operation] : analysis_session_.inputs.selection.soa_access_columns) {
            static_cast<void>(name);
            operation = analysis_session_.inputs.access_operation;
        }
        changed = true;
    }
    auto multiplicity{analysis_session_.inputs.access_multiplicity};
    ImGui::SetNextItemWidth(180.0F);
    if (ImGui::InputScalar("Accesses / element", ImGuiDataType_U64, &multiplicity)) {
        if (multiplicity == 0) {
            access_multiplicity_error_ = true;
        } else {
            analysis_session_.inputs.access_multiplicity = multiplicity;
            access_multiplicity_error_ = false;
            changed = true;
        }
    }
    if (access_multiplicity_error_) {
        ImGui::TextColored(ImVec4{0.95F, 0.45F, 0.35F, 1.0F},
                           "Access multiplicity must be non-zero.");
    }
    return changed;
}

void PlannerUi::draw_packed_layout(PackedType const& packed, PackedAnalysis const& baseline) {
    auto const& identity{analysis_session_.inputs.workspace.types().type(baseline.type).identity};
    ImGui::Text("%s", identity.name.c_str());
    ImGui::TextDisabled("%s", identity.module_name.c_str());

    ImGui::SeparatorText("Analysis scale");
    if (draw_element_count()) {
        return;
    }

    auto const& scale_analysis{*analysis_session_.results().active_packed};
    auto const& aggregate{scale_analysis.aggregate};
    auto const byte_order{
        scale_analysis.byte_order.has_value()
            ? std::string{codegen::packed_byte_order_name(*scale_analysis.byte_order)}
            : std::string{"Unspecified"}};
    ImGui::Text("Serialized byte order: %s", byte_order.c_str());
    ImGui::Text("Segment bit order: %s",
                std::string{codegen::packed_bit_order_name(scale_analysis.bit_order)}.c_str());
    ImGui::TextDisabled(
        "Byte order is a serialization fact; the diagram uses numeric storage bit positions.");
    if (ImGui::BeginTable("packed-aggregate",
                          2,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_SizingStretchProp)) {
        auto draw_stat{[](char const* const label, std::string const& value) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(label);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(value.c_str());
        }};
        draw_stat("Physical storage", detail::format_bytes(aggregate.total_storage_bytes));
        draw_stat("Payload bits", detail::format_number(aggregate.total_payload_bits));
        draw_stat("Reserved bits", detail::format_number(aggregate.total_reserved_bits));
        draw_stat("Unused packed bits", detail::format_number(aggregate.total_unused_bits));
        draw_stat("Minimum cache lines", detail::format_number(aggregate.minimum_cache_lines));
        draw_stat("Complete elements / cache line",
                  detail::format_number(aggregate.complete_elements_per_cache_line));
        draw_stat("Elements crossing cache-line boundaries",
                  detail::format_number(aggregate.cache_line_straddling_elements));
        draw_stat("Minimum pages", detail::format_number(aggregate.minimum_pages));
        draw_stat("Complete elements / page",
                  detail::format_number(aggregate.complete_elements_per_page));
        draw_stat("Elements crossing page boundaries",
                  detail::format_number(aggregate.page_straddling_elements));
        draw_stat("Fits L1 data cache", detail::format_fit(aggregate.cache_capacity.fits_l1_data));
        draw_stat("Fits L2 cache", detail::format_fit(aggregate.cache_capacity.fits_l2));
        draw_stat("Fits L3 cache", detail::format_fit(aggregate.cache_capacity.fits_l3));
        ImGui::EndTable();
    }
    if (scale_analysis.storage_bits.has_value() && scale_analysis.unused_bits.has_value() &&
        *scale_analysis.storage_bits != 0) {
        auto const waste{static_cast<double>(*scale_analysis.unused_bits) * 100.0 /
                         static_cast<double>(*scale_analysis.storage_bits)};
        ImGui::TextDisabled("Per-element packed-bit waste: %.3f%% (%llu of %llu bits).",
                            waste,
                            static_cast<unsigned long long>(*scale_analysis.unused_bits),
                            static_cast<unsigned long long>(*scale_analysis.storage_bits));
    }
    ImGui::TextDisabled("Target memory facts: %s",
                        analysis_session_.primary_abi().memory_facts().provenance.empty()
                            ? "Unknown"
                            : analysis_session_.primary_abi().memory_facts().provenance.c_str());
    ImGui::TextDisabled("Boundary crossing assumes a contiguous packed-value array whose base is "
                        "cache-line/page aligned.");

    ImGui::SeparatorText("Sequential access set");
    if (draw_access_operation()) {
        refresh_analysis();
    }
    if (analysis_session_.results().packed_access_analysis.has_value()) {
        auto const& access{*analysis_session_.results().packed_access_analysis};
        std::string field_names;
        for (auto const& field_name : access.field_names) {
            if (!field_names.empty()) {
                field_names += ", ";
            }
            field_names += field_name;
        }
        ImGui::Text("Fields: %s", field_names.c_str());
        if (ImGui::BeginTable("packed-access",
                              2,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_SizingStretchProp)) {
            auto draw_access_stat{[](char const* const label, std::string const& value) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(label);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(value.c_str());
            }};
            draw_access_stat("Selected useful bits", detail::format_number(access.useful_bits));
            draw_access_stat("Read useful bits", detail::format_number(access.read_useful_bits));
            draw_access_stat("Write useful bits", detail::format_number(access.write_useful_bits));
            draw_access_stat("Logical read useful bits",
                             detail::format_number(access.logical_read_useful_bits));
            draw_access_stat("Logical write useful bits",
                             detail::format_number(access.logical_write_useful_bits));
            draw_access_stat("Packed storage footprint",
                             detail::format_bytes(access.storage_footprint_bytes));
            draw_access_stat("Non-useful storage bits",
                             detail::format_number(access.non_useful_storage_bits));
            draw_access_stat("Unselected ordinary-field bits",
                             detail::format_number(access.unselected_field_bits));
            draw_access_stat("Explicit reserved-region bits",
                             detail::format_number(access.reserved_region_bits));
            draw_access_stat("Physically unused backing bits",
                             detail::format_number(access.physically_unused_storage_bits));
            draw_access_stat("Minimum cache lines",
                             detail::format_number(access.minimum_cache_lines_touched));
            draw_access_stat("Minimum cache-line bytes",
                             detail::format_bytes(access.minimum_cache_bytes_touched));
            draw_access_stat("Read cache lines",
                             detail::format_number(access.read_cache_lines_touched));
            draw_access_stat("Write cache lines",
                             detail::format_number(access.write_cache_lines_touched));
            draw_access_stat("Minimum pages", detail::format_number(access.minimum_pages_touched));
            draw_access_stat("Minimum page bytes",
                             detail::format_bytes(access.minimum_page_bytes_touched));
            draw_access_stat("Read pages", detail::format_number(access.read_pages_touched));
            draw_access_stat("Write pages", detail::format_number(access.write_pages_touched));
            draw_access_stat("Cache footprint fits L1 data",
                             detail::format_fit(access.cache_footprint_capacity.fits_l1_data));
            draw_access_stat("Cache footprint fits L2",
                             detail::format_fit(access.cache_footprint_capacity.fits_l2));
            draw_access_stat("Cache footprint fits L3",
                             detail::format_fit(access.cache_footprint_capacity.fits_l3));
            ImGui::EndTable();
        }
        ImGui::TextDisabled(
            "Useful bits follow the selected field set; physical cache/page coverage addresses "
            "each containing packed storage element and is not a sub-word fetch estimate.");
        draw_diagnostics(access.diagnostics);
    } else {
        ImGui::TextDisabled("Select a non-reserved field or choose an explicit access set.");
    }

    ImGui::SeparatorText("Bit layout");
    std::map<std::string, AccessOperation, std::less<>> effective_access_fields;
    if (analysis_session_.inputs.selection.packed_access_set_explicit) {
        effective_access_fields = analysis_session_.inputs.selection.packed_access_fields;
    } else if (!analysis_session_.inputs.selection.field.empty()) {
        auto const selected{std::ranges::find(
            baseline.fields, analysis_session_.inputs.selection.field, &PackedFieldAnalysis::name)};
        if (selected != baseline.fields.end() && !selected->reserved) {
            effective_access_fields.emplace(analysis_session_.inputs.selection.field,
                                            analysis_session_.inputs.access_operation);
        }
    }
    if (!effective_access_fields.empty()) {
        ImGui::ColorButton("read-workload-color",
                           read_access_color,
                           ImGuiColorEditFlags_NoTooltip,
                           {10.0F, 10.0F});
        ImGui::SameLine();
        ImGui::TextDisabled("Read");
        ImGui::SameLine();
        ImGui::ColorButton("write-workload-color",
                           write_access_color,
                           ImGuiColorEditFlags_NoTooltip,
                           {10.0F, 10.0F});
        ImGui::SameLine();
        ImGui::TextDisabled("Write");
        ImGui::SameLine();
        ImGui::ColorButton("read-write-workload-color",
                           read_write_access_color,
                           ImGuiColorEditFlags_NoTooltip,
                           {10.0F, 10.0F});
        ImGui::SameLine();
        ImGui::TextDisabled("Read + write — operation bands, not independent fetch regions");
    }
    auto common_bits{std::max(
        {baseline.storage_bits.value_or(0), baseline.bits_used.value_or(0), std::uint64_t{1}})};
    for (auto const& [variant_id, analysis] : analysis_session_.results().packed_variants) {
        static_cast<void>(variant_id);
        common_bits = std::max(
            {common_bits, analysis.storage_bits.value_or(0), analysis.bits_used.value_or(0)});
    }
    auto const declaration{document_.has_value() ? document_->find_declaration(identity)
                                                 : std::optional<DeclarationId>{}};
    auto const* packed_schema{declaration.has_value() ? document_->packed_value_schema(*declaration)
                                                      : nullptr};
    std::optional<std::size_t> inactive_baseline_divider;
    auto& baseline_divider{!packed_dragged_variant_id_.has_value() ||
                                   *packed_dragged_variant_id_ ==
                                       LayoutWorkspace::baseline_variant_id
                               ? packed_dragged_divider_
                               : inactive_baseline_divider};
    auto const baseline_was_dragging{baseline_divider.has_value()};
    auto const baseline_dragged_index{baseline_divider};
    std::optional<PackedDividerAdjustment> adjustment;
    bool activated{};
    draw_packed_bar(baseline,
                    nullptr,
                    analysis_session_.inputs.selection.field,
                    effective_access_fields,
                    "Baseline",
                    common_bits,
                    packed_schema != nullptr,
                    baseline_divider,
                    adjustment,
                    activated);
    if (activated && baseline_divider.has_value()) {
        packed_dragged_variant_id_ = LayoutWorkspace::baseline_variant_id;
    }
    if (adjustment.has_value()) {
        packed_dragged_left_width_ = adjustment->left_width;
        packed_dragged_right_width_ = adjustment->right_width;
    }
    if (baseline_was_dragging && !baseline_divider.has_value() &&
        packed_dragged_variant_id_ == LayoutWorkspace::baseline_variant_id) {
        if (packed_dragged_left_width_.has_value() && packed_dragged_right_width_.has_value() &&
            packed_schema != nullptr && baseline_dragged_index.has_value()) {
            auto replacement{*packed_schema};
            auto const divider{*baseline_dragged_index};
            if (divider + 1 < replacement.segments.size()) {
                std::visit(
                    [&](auto& segment) {
                        segment.bits = static_cast<int>(*packed_dragged_left_width_);
                    },
                    replacement.segments[divider]);
                std::visit(
                    [&](auto& segment) {
                        segment.bits = static_cast<int>(*packed_dragged_right_width_);
                    },
                    replacement.segments[divider + 1]);
                auto const selected_field{analysis_session_.inputs.selection.field};
                if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                           .schema = std::move(replacement)})) {
                    analysis_session_.inputs.selection.field = selected_field;
                }
            }
        }
        packed_dragged_variant_id_.reset();
        packed_dragged_left_width_.reset();
        packed_dragged_right_width_.reset();
        return;
    }
    if (packed_schema != nullptr && baseline.fields.size() > 1) {
        ImGui::TextDisabled(
            "Drag a baseline divider to change both adjacent LispB field widths as one undo step.");
    }
    if (baseline.unused_bits.value_or(0) != 0) {
        ImGui::TextDisabled("Baseline has %llu unused storage bit%s.",
                            static_cast<unsigned long long>(*baseline.unused_bits),
                            *baseline.unused_bits == 1 ? "" : "s");
    }
    if (baseline.overflow_bits.value_or(0) != 0) {
        ImGui::TextColored(overflow_color,
                           "Baseline exceeds storage by %llu bit%s.",
                           static_cast<unsigned long long>(*baseline.overflow_bits),
                           *baseline.overflow_bits == 1 ? "" : "s");
    }
    for (auto const& [variant_id, analysis] : analysis_session_.results().packed_variants) {
        auto const* variant{analysis_session_.inputs.workspace.variant(variant_id)};
        if (variant == nullptr) {
            continue;
        }
        auto const label{variant->name +
                         (variant_id == analysis_session_.inputs.workspace.active_variant_id()
                              ? " (editing)"
                              : "")};
        adjustment.reset();
        activated = false;
        std::optional<std::size_t> inactive_divider;
        auto& dragged_divider{!packed_dragged_variant_id_.has_value() ||
                                      *packed_dragged_variant_id_ == variant_id
                                  ? packed_dragged_divider_
                                  : inactive_divider};
        ImGui::PushID(static_cast<int>(variant_id));
        draw_packed_bar(analysis,
                        &baseline,
                        analysis_session_.inputs.selection.field,
                        effective_access_fields,
                        label.c_str(),
                        common_bits,
                        true,
                        dragged_divider,
                        adjustment,
                        activated);
        ImGui::PopID();
        if (activated && packed_dragged_divider_.has_value()) {
            packed_dragged_variant_id_ = variant_id;
        } else if (packed_dragged_variant_id_.has_value() &&
                   *packed_dragged_variant_id_ == variant_id &&
                   !packed_dragged_divider_.has_value()) {
            packed_dragged_variant_id_.reset();
        }
        if (activated && analysis_session_.inputs.workspace.active_variant_id() != variant_id) {
            analysis_session_.inputs.workspace.select_variant(variant_id);
            sync_variant_name();
        }
        if (adjustment.has_value() && adjustment->left_field_index + 1 < packed.segments.size()) {
            if (analysis_session_.inputs.workspace.active_variant_id() != variant_id) {
                analysis_session_.inputs.workspace.select_variant(variant_id);
                sync_variant_name();
            }
            auto const* left_field{
                std::get_if<PackedField>(&packed.segments[adjustment->left_field_index])};
            auto const* right_field{
                std::get_if<PackedField>(&packed.segments[adjustment->left_field_index + 1])};
            if (left_field == nullptr || right_field == nullptr) {
                continue;
            }
            analysis_session_.inputs.workspace.set_packed_field_width(
                analysis.type,
                left_field->name,
                adjustment->left_width == left_field->bit_width
                    ? std::optional<std::uint32_t>{}
                    : std::optional<std::uint32_t>{adjustment->left_width});
            analysis_session_.inputs.workspace.set_packed_field_width(
                analysis.type,
                right_field->name,
                adjustment->right_width == right_field->bit_width
                    ? std::optional<std::uint32_t>{}
                    : std::optional<std::uint32_t>{adjustment->right_width});
        }
        if (analysis.unused_bits.value_or(0) != 0 && analysis.bits_used.has_value()) {
            auto const first_unused_bit{analysis.bit_order ==
                                                codegen::PackedBitOrder::most_significant_first
                                            ? std::uint64_t{0}
                                            : *analysis.bits_used};
            ImGui::TextWrapped(
                "Hatched region: unused storage bits %llu..%llu.",
                static_cast<unsigned long long>(first_unused_bit),
                static_cast<unsigned long long>(first_unused_bit + *analysis.unused_bits - 1));
        }
        if (analysis.overflow_bits.value_or(0) != 0) {
            ImGui::TextColored(overflow_color,
                               "%llu planned bit%s exceed storage.",
                               static_cast<unsigned long long>(*analysis.overflow_bits),
                               *analysis.overflow_bits == 1 ? "" : "s");
        }
    }
    if (!analysis_session_.results().packed_variants.empty()) {
        ImGui::TextDisabled("Click a variant to edit it. Drag a divider to transfer whole bits.");
    }
}

void PlannerUi::draw_record_layout(RecordAnalysis const& analysis) {
    auto const& node{analysis_session_.inputs.workspace.types().type(analysis.type)};
    ImGui::Text("%s", node.identity.name.c_str());
    ImGui::TextDisabled("%s", node.identity.module_name.c_str());

    ImGui::SeparatorText("Analysis scale");
    if (draw_element_count()) {
        return;
    }
    auto const& aggregate{analysis.aggregate};
    if (ImGui::BeginTable("record-aggregate",
                          2,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_SizingStretchProp)) {
        auto draw_stat{[](char const* const label, std::string const& value) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(label);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(value.c_str());
        }};
        draw_stat("Physical storage", detail::format_bytes(aggregate.total_storage_bytes));
        draw_stat("Member extents", detail::format_bytes(aggregate.total_payload_bytes));
        draw_stat("Padding", detail::format_bytes(aggregate.total_internal_padding_bytes));
        draw_stat("Tail padding", detail::format_bytes(aggregate.total_tail_padding_bytes));
        draw_stat("Total padding", detail::format_bytes(aggregate.total_padding_bytes));
        draw_stat("Minimum cache lines", detail::format_number(aggregate.minimum_cache_lines));
        draw_stat("Complete elements / cache line",
                  detail::format_number(aggregate.complete_elements_per_cache_line));
        draw_stat("Elements crossing cache-line boundaries",
                  detail::format_number(aggregate.cache_line_straddling_elements));
        draw_stat("Minimum pages", detail::format_number(aggregate.minimum_pages));
        draw_stat("Complete elements / page",
                  detail::format_number(aggregate.complete_elements_per_page));
        draw_stat("Elements crossing page boundaries",
                  detail::format_number(aggregate.page_straddling_elements));
        draw_stat("Fits L1 data cache", detail::format_fit(aggregate.cache_capacity.fits_l1_data));
        draw_stat("Fits L2 cache", detail::format_fit(aggregate.cache_capacity.fits_l2));
        draw_stat("Fits L3 cache", detail::format_fit(aggregate.cache_capacity.fits_l3));
        ImGui::EndTable();
    }
    if (analysis.size_bytes.has_value() && analysis.internal_padding_bytes.has_value() &&
        analysis.tail_padding_bytes.has_value() && *analysis.size_bytes != 0) {
        auto const padding{*analysis.internal_padding_bytes + *analysis.tail_padding_bytes};
        auto const waste{static_cast<double>(padding) * 100.0 /
                         static_cast<double>(*analysis.size_bytes)};
        ImGui::TextDisabled("Per-element ABI padding: %.3f%% (%llu of %llu bytes).",
                            waste,
                            static_cast<unsigned long long>(padding),
                            static_cast<unsigned long long>(*analysis.size_bytes));
    }
    ImGui::TextDisabled("Target memory facts: %s",
                        analysis_session_.primary_abi().memory_facts().provenance.empty()
                            ? "Unknown"
                            : analysis_session_.primary_abi().memory_facts().provenance.c_str());
    ImGui::TextDisabled(
        "Boundary crossing assumes a contiguous array whose base is cache-line/page aligned.");

    ImGui::SeparatorText("Sequential access set");
    if (draw_access_operation()) {
        refresh_analysis();
    }
    if (analysis_session_.results().record_access_analysis.has_value()) {
        auto const& access{*analysis_session_.results().record_access_analysis};
        std::string member_names;
        for (auto const& member_name : access.member_names) {
            if (!member_names.empty()) {
                member_names += ", ";
            }
            member_names += member_name;
        }
        ImGui::TextWrapped("Members: %s", member_names.c_str());
        if (ImGui::BeginTable("record-member-access",
                              2,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_SizingStretchProp)) {
            auto draw_stat{[](char const* const label, std::string const& value) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(label);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(value.c_str());
            }};
            draw_stat("Operation", detail::access_operation_summary(access.accesses));
            draw_stat("Useful selected bytes", detail::format_bytes(access.useful_bytes));
            draw_stat("Classified read useful bytes",
                      detail::format_bytes(access.read_useful_bytes));
            draw_stat("Classified write useful bytes",
                      detail::format_bytes(access.write_useful_bytes));
            draw_stat("Logical read useful bytes",
                      detail::format_bytes(access.logical_read_useful_bytes));
            draw_stat("Logical write useful bytes",
                      detail::format_bytes(access.logical_write_useful_bytes));
            draw_stat("Enclosing AoS footprint",
                      detail::format_bytes(access.object_footprint_bytes));
            draw_stat("Distinct cache lines touched",
                      detail::format_number(access.cache_lines_touched));
            draw_stat("Bytes in touched cache lines",
                      detail::format_bytes(access.cache_bytes_touched));
            draw_stat("Read cache lines covered",
                      detail::format_number(access.read_cache_lines_touched));
            draw_stat("Read cache-line address coverage",
                      detail::format_bytes(access.read_cache_bytes_touched));
            draw_stat("Write cache lines covered",
                      detail::format_number(access.write_cache_lines_touched));
            draw_stat("Write cache-line address coverage",
                      detail::format_bytes(access.write_cache_bytes_touched));
            draw_stat("Non-selected bytes in touched cache lines",
                      detail::format_bytes(access.non_selected_cache_bytes));
            draw_stat("Exact footprint <= L1 data cache",
                      detail::format_fit(access.cache_footprint_capacity.fits_l1_data));
            draw_stat("Exact footprint <= L2 cache",
                      detail::format_fit(access.cache_footprint_capacity.fits_l2));
            draw_stat("Exact footprint <= L3 cache",
                      detail::format_fit(access.cache_footprint_capacity.fits_l3));
            draw_stat("Distinct pages touched", detail::format_number(access.pages_touched));
            draw_stat("Bytes in touched pages", detail::format_bytes(access.page_bytes_touched));
            draw_stat("Read pages covered", detail::format_number(access.read_pages_touched));
            draw_stat("Read page address coverage",
                      detail::format_bytes(access.read_page_bytes_touched));
            draw_stat("Write pages covered", detail::format_number(access.write_pages_touched));
            draw_stat("Write page address coverage",
                      detail::format_bytes(access.write_page_bytes_touched));
            draw_stat("Non-selected bytes in touched pages",
                      detail::format_bytes(access.non_selected_page_bytes));
            ImGui::EndTable();
        }
        draw_footprint_composition("Exact cache-line footprint composition",
                                   access.useful_bytes,
                                   access.cache_bytes_touched,
                                   access.non_selected_cache_bytes);
        draw_footprint_composition("Exact page footprint composition",
                                   access.useful_bytes,
                                   access.page_bytes_touched,
                                   access.non_selected_page_bytes);
        ImGui::TextDisabled(
            "One sequential classified access to the selected members per element; aligned "
            "contiguous AoS base assumed. Footprints describe address coverage and do not infer "
            "write allocation, eviction, or bus traffic. Logical useful totals multiply by the "
            "declared accesses per element only.");
        draw_diagnostics(access.diagnostics);
    } else {
        ImGui::TextDisabled("Select record members in the Access column to define the access set.");
    }

    ImGui::SeparatorText("Object layout");
    ImGui::Text("Size: %s B    Alignment: %s B",
                detail::format_number(analysis.size_bytes).c_str(),
                detail::format_number(analysis.alignment_bytes).c_str());
    ImGui::Text("Member extents: %s B    Padding: %s B    Tail padding: %s B",
                detail::format_number(analysis.payload_bytes).c_str(),
                detail::format_number(analysis.internal_padding_bytes).c_str(),
                detail::format_number(analysis.tail_padding_bytes).c_str());

    if (analysis.size_bytes.has_value() && *analysis.size_bytes != 0) {
        auto const width{std::max(1.0F, ImGui::GetContentRegionAvail().x)};
        auto const origin{ImGui::GetCursorScreenPos()};
        constexpr auto height{58.0F};
        auto* const draw_list{ImGui::GetWindowDrawList()};
        struct ByteGap {
            std::uint64_t begin{};
            std::uint64_t end{};
            char const* kind{};
        };
        std::vector<ByteGap> gaps;
        auto const all_member_extents_known{
            std::ranges::all_of(analysis.members, [&](auto const& member) {
                return member.offset_bytes.has_value() && member.extent_bytes.has_value() &&
                     *member.offset_bytes <= *analysis.size_bytes &&
                     *member.extent_bytes <= *analysis.size_bytes - *member.offset_bytes;
            })};
        if (all_member_extents_known) {
            auto next_byte{std::uint64_t{0}};
            for (auto const& member : analysis.members) {
                if (*member.offset_bytes > next_byte) {
                    gaps.push_back({next_byte, *member.offset_bytes, "Padding"});
                }
                next_byte = std::max(next_byte, *member.offset_bytes + *member.extent_bytes);
            }
            if (next_byte < *analysis.size_bytes) {
                gaps.push_back(
                    {next_byte,
                     *analysis.size_bytes,
                     analysis.members.empty() ? "Empty-object storage" : "Tail padding"});
            }
        }
        draw_list->AddRectFilled(
            origin, {origin.x + width, origin.y + height}, ImGui::GetColorU32(unused_color), 3.0F);
        for (std::size_t index{}; index < analysis.members.size(); ++index) {
            auto const& member{analysis.members[index]};
            if (!member.offset_bytes.has_value() || !member.extent_bytes.has_value()) {
                continue;
            }
            auto const left{origin.x + width * static_cast<float>(*member.offset_bytes) /
                                           static_cast<float>(*analysis.size_bytes)};
            auto const member_width{width * static_cast<float>(*member.extent_bytes) /
                                    static_cast<float>(*analysis.size_bytes)};
            auto const selected{analysis_session_.inputs.selection.field == member.name};
            auto const color{
                selected
                    ? selected_color
                    : ImVec4{0.48F + 0.07F * static_cast<float>(index % 2), 0.34F, 0.18F, 1.0F}};
            draw_list->AddRectFilled({left, origin.y},
                                     {left + member_width, origin.y + height},
                                     ImGui::GetColorU32(color),
                                     2.0F);
            draw_list->AddRect({left, origin.y},
                               {left + member_width, origin.y + height},
                               ImGui::GetColorU32(ImGuiCol_Border));
            if (member_width >= 42.0F) {
                draw_list->AddText({left + 4.0F, origin.y + 5.0F},
                                   ImGui::GetColorU32(packed_detail_text_color),
                                   member.name.c_str());
                auto const range{std::to_string(*member.offset_bytes) + ".." +
                                 std::to_string(*member.offset_bytes + *member.extent_bytes - 1)};
                draw_list->AddText({left + 4.0F, origin.y + 27.0F},
                                   ImGui::GetColorU32(packed_detail_text_color),
                                   range.c_str());
            }
        }
        for (auto const& gap : gaps) {
            auto const left{origin.x + width * static_cast<float>(gap.begin) /
                                           static_cast<float>(*analysis.size_bytes)};
            auto const right{origin.x + width * static_cast<float>(gap.end) /
                                            static_cast<float>(*analysis.size_bytes)};
            auto const range{std::to_string(gap.begin) + ".." + std::to_string(gap.end - 1)};
            auto const label{std::string{gap.kind} + " " + range};
            auto const compact_label{std::string{analysis.members.empty() ? "empty " : "pad "} +
                                     range};
            detail::draw_labeled_gap(
                draw_list, {left, origin.y}, {right, origin.y + height}, label, compact_label);
        }
        draw_list->AddRect(origin,
                           {origin.x + width, origin.y + height},
                           ImGui::GetColorU32(ImGuiCol_Border),
                           3.0F,
                           0,
                           2.0F);
        ImGui::InvisibleButton("##record-object-map", {width, height});
        if (ImGui::IsItemHovered()) {
            auto const relative_x{std::clamp(ImGui::GetIO().MousePos.x - origin.x, 0.0F, width)};
            auto const byte{
                std::min(*analysis.size_bytes - 1,
                         static_cast<std::uint64_t>(
                             relative_x * static_cast<float>(*analysis.size_bytes) / width))};
            for (auto const& gap : gaps) {
                if (byte >= gap.begin && byte < gap.end) {
                    ImGui::SetTooltip("%s: bytes %llu..%llu",
                                      gap.kind,
                                      static_cast<unsigned long long>(gap.begin),
                                      static_cast<unsigned long long>(gap.end - 1));
                    break;
                }
            }
        }
        for (auto const& gap : gaps) {
            ImGui::TextWrapped("%s: bytes %llu..%llu",
                               gap.kind,
                               static_cast<unsigned long long>(gap.begin),
                               static_cast<unsigned long long>(gap.end - 1));
        }
        if (!all_member_extents_known) {
            ImGui::TextWrapped("Grey space includes unknown member ranges; padding cannot be "
                               "located precisely.");
        }
    }

    if (ImGui::BeginTable("record-layout",
                          7,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Member");
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Count");
        ImGui::TableSetupColumn("Offset B");
        ImGui::TableSetupColumn("Extent B");
        ImGui::TableSetupColumn("Align B");
        ImGui::TableSetupColumn("Pad before B");
        ImGui::TableHeadersRow();
        for (auto const& member : analysis.members) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (ImGui::Selectable(member.name.c_str(),
                                  analysis_session_.inputs.selection.field == member.name,
                                  ImGuiSelectableFlags_None)) {
                analysis_session_.inputs.selection.field = member.name;
            }
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(analysis_session_.inputs.workspace.types()
                                       .type(member.semantic_type)
                                       .cpp_spelling.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%llu", static_cast<unsigned long long>(member.element_count));
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(detail::format_number(member.offset_bytes).c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(detail::format_number(member.extent_bytes).c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(
                detail::format_number(member.element_facts.transform([](TypeFacts const& facts) {
                    return facts.alignment_bytes;
                })).c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(detail::format_number(member.padding_before_bytes).c_str());
        }
        ImGui::EndTable();
    }
    draw_diagnostics(analysis.diagnostics);
}

void PlannerUi::draw_soa_layout(SoaType const& soa, SoaAnalysis const& baseline) {
    auto const& active{*analysis_session_.results().active_soa};
    auto const& identity{analysis_session_.inputs.workspace.types().type(baseline.type).identity};
    ImGui::Text("%s", identity.name.c_str());
    ImGui::TextDisabled("%s", identity.module_name.c_str());
    if (soa.related_storage_name.has_value()) {
        ImGui::TextDisabled("Related generated storage: %s (padding and gaps not modeled).",
                            soa.related_storage_name->c_str());
    }
    ImGui::SeparatorText("Analysis scale");
    if (draw_element_count()) {
        return;
    }
    auto strategy_index{static_cast<int>(analysis_session_.inputs.soa_allocation_strategy)};
    ImGui::SetNextItemWidth(240.0F);
    if (ImGui::Combo("Allocation strategy",
                     &strategy_index,
                     "Separate columns\0Aligned contiguous block\0")) {
        analysis_session_.inputs.soa_allocation_strategy =
            static_cast<SoaAllocationStrategy>(strategy_index);
        refresh_analysis();
        return;
    }
    ImGui::TextDisabled(
        analysis_session_.inputs.soa_allocation_strategy == SoaAllocationStrategy::separate_columns
            ? "Each column is a separate allocation; region footprints are lower bounds."
            : "Columns occupy one alignment-aware block; region coverage assumes a region-aligned "
              "block origin.");
    if (ImGui::BeginTable("soa-columns",
                          11,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Column");
        ImGui::TableSetupColumn("Schema type");
        ImGui::TableSetupColumn("Planning type");
        ImGui::TableSetupColumn("Element B");
        ImGui::TableSetupColumn("Payload");
        ImGui::TableSetupColumn("Block offset");
        ImGui::TableSetupColumn("Pad before");
        ImGui::TableSetupColumn("Min lines");
        ImGui::TableSetupColumn("Exact / line");
        ImGui::TableSetupColumn("Min pages");
        ImGui::TableSetupColumn("Complete / page");
        ImGui::TableHeadersRow();
        for (auto const& column : active.columns) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            auto const selected{analysis_session_.inputs.selection.field == column.name};
            if (ImGui::Selectable(
                    column.name.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
                analysis_session_.inputs.selection.field = column.name;
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
            ImGui::TextUnformatted(detail::format_bytes(column.allocation_offset_bytes).c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(detail::format_bytes(column.padding_before_bytes).c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(detail::format_number(column.minimum_cache_lines).c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(detail::format_number(column.elements_per_cache_line).c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(detail::format_number(column.minimum_pages).c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(
                detail::format_number(column.complete_elements_per_page).c_str());
        }
        ImGui::EndTable();
    }

    auto common_total{baseline.total_allocation_bytes.value_or(0)};
    for (auto const& [variant_id, analysis] : analysis_session_.results().soa_variants) {
        static_cast<void>(variant_id);
        common_total = std::max(common_total, analysis.total_allocation_bytes.value_or(0));
    }
    ImGui::SeparatorText("Allocation");
    ImGui::Text("Modeled allocations: %llu",
                static_cast<unsigned long long>(active.allocation_count));
    if (analysis_session_.inputs.soa_allocation_strategy ==
        SoaAllocationStrategy::separate_columns) {
        ImGui::Text("Payload / minimum allocation bytes: %s",
                    detail::format_bytes(active.total_payload_bytes).c_str());
    } else {
        ImGui::Text("Payload: %s    Alignment padding: %s    Total block: %s",
                    detail::format_bytes(active.total_payload_bytes).c_str(),
                    detail::format_bytes(active.total_alignment_padding_bytes).c_str(),
                    detail::format_bytes(active.total_allocation_bytes).c_str());
    }
    ImGui::Text("Allocation alignment: %s",
                detail::format_bytes(active.allocation_alignment_bytes).c_str());
    ImGui::Text(analysis_session_.inputs.soa_allocation_strategy ==
                        SoaAllocationStrategy::separate_columns
                    ? "Minimum allocation pages: %s"
                    : "Contiguous block pages: %s",
                detail::format_number(active.minimum_pages).c_str());
    ImGui::Text("Fits L1 data cache: %s",
                detail::format_fit(active.cache_capacity.fits_l1_data).c_str());
    ImGui::Text("Fits L2 cache: %s", detail::format_fit(active.cache_capacity.fits_l2).c_str());
    ImGui::Text("Fits L3 cache: %s", detail::format_fit(active.cache_capacity.fits_l3).c_str());

    ImGui::SeparatorText("Sequential access set");
    if (draw_access_operation()) {
        refresh_analysis();
    }
    if (analysis_session_.results().soa_access_analysis.has_value()) {
        auto const& access{*analysis_session_.results().soa_access_analysis};
        std::string column_names;
        for (auto const& column_name : access.column_names) {
            if (!column_names.empty()) {
                column_names += ", ";
            }
            column_names += column_name;
        }
        ImGui::TextWrapped("Columns: %s", column_names.c_str());
        if (ImGui::BeginTable("soa-column-access",
                              2,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_SizingStretchProp)) {
            auto draw_stat{[](char const* const label, std::string const& value) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(label);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(value.c_str());
            }};
            auto const* const region_qualifier{access.footprint_exact ? "Exact" : "Minimum"};
            auto const* const region_qualifier_lower{access.footprint_exact ? "exact" : "minimum"};
            draw_stat("Operation", detail::access_operation_summary(access.accesses));
            draw_stat("Footprint model",
                      access.footprint_exact ? "Exact aligned contiguous block"
                                             : "Minimum across separate allocations");
            draw_stat("Useful selected payload", detail::format_bytes(access.useful_bytes));
            draw_stat("Classified read useful payload",
                      detail::format_bytes(access.read_useful_bytes));
            draw_stat("Classified write useful payload",
                      detail::format_bytes(access.write_useful_bytes));
            draw_stat("Logical read useful payload",
                      detail::format_bytes(access.logical_read_useful_bytes));
            draw_stat("Logical write useful payload",
                      detail::format_bytes(access.logical_write_useful_bytes));
            draw_stat("Complete logical payload at count",
                      detail::format_bytes(access.full_logical_payload_bytes));
            draw_stat("Unselected payload at count",
                      detail::format_bytes(access.unselected_payload_bytes));
            draw_stat("Allocated payload at capacity",
                      detail::format_bytes(access.allocated_capacity_payload_bytes));
            draw_stat("Unused allocated capacity payload",
                      detail::format_bytes(access.capacity_slack_payload_bytes));
            draw_stat((std::string{region_qualifier} + " cache lines touched").c_str(),
                      detail::format_number(access.minimum_cache_lines_touched));
            draw_stat((std::string{region_qualifier} + " cache-line footprint").c_str(),
                      detail::format_bytes(access.minimum_cache_bytes_touched));
            draw_stat((std::string{region_qualifier} + " read cache lines").c_str(),
                      detail::format_number(access.minimum_read_cache_lines_touched));
            draw_stat((std::string{region_qualifier} + " read cache-line coverage").c_str(),
                      detail::format_bytes(access.minimum_read_cache_bytes_touched));
            draw_stat((std::string{region_qualifier} + " write cache lines").c_str(),
                      detail::format_number(access.minimum_write_cache_lines_touched));
            draw_stat((std::string{region_qualifier} + " write cache-line coverage").c_str(),
                      detail::format_bytes(access.minimum_write_cache_bytes_touched));
            draw_stat(
                (std::string{"Non-payload bytes in "} + region_qualifier_lower + " cache footprint")
                    .c_str(),
                detail::format_bytes(access.non_payload_cache_bytes));
            draw_stat((std::string{region_qualifier} + " footprint <= L1 data cache").c_str(),
                      detail::format_fit(access.minimum_cache_footprint_capacity.fits_l1_data));
            draw_stat((std::string{region_qualifier} + " footprint <= L2 cache").c_str(),
                      detail::format_fit(access.minimum_cache_footprint_capacity.fits_l2));
            draw_stat((std::string{region_qualifier} + " footprint <= L3 cache").c_str(),
                      detail::format_fit(access.minimum_cache_footprint_capacity.fits_l3));
            draw_stat((std::string{region_qualifier} + " pages touched").c_str(),
                      detail::format_number(access.minimum_pages_touched));
            draw_stat((std::string{region_qualifier} + " page footprint").c_str(),
                      detail::format_bytes(access.minimum_page_bytes_touched));
            draw_stat((std::string{region_qualifier} + " read pages").c_str(),
                      detail::format_number(access.minimum_read_pages_touched));
            draw_stat((std::string{region_qualifier} + " read page coverage").c_str(),
                      detail::format_bytes(access.minimum_read_page_bytes_touched));
            draw_stat((std::string{region_qualifier} + " write pages").c_str(),
                      detail::format_number(access.minimum_write_pages_touched));
            draw_stat((std::string{region_qualifier} + " write page coverage").c_str(),
                      detail::format_bytes(access.minimum_write_page_bytes_touched));
            draw_stat(
                (std::string{"Non-payload bytes in "} + region_qualifier_lower + " page footprint")
                    .c_str(),
                detail::format_bytes(access.non_payload_page_bytes));
            ImGui::EndTable();
        }
        draw_footprint_composition(access.footprint_exact
                                       ? "Exact cache-line footprint composition"
                                       : "Minimum cache-line footprint composition",
                                   access.useful_bytes,
                                   access.minimum_cache_bytes_touched,
                                   access.non_payload_cache_bytes);
        draw_footprint_composition(access.footprint_exact ? "Exact page footprint composition"
                                                          : "Minimum page footprint composition",
                                   access.useful_bytes,
                                   access.minimum_page_bytes_touched,
                                   access.non_payload_page_bytes);
        if (!access.columns.empty() && ImGui::TreeNode("Selected column boundary crossings")) {
            if (ImGui::BeginTable("soa-access-boundary-crossings",
                                  5,
                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_Resizable)) {
                ImGui::TableSetupColumn("Column");
                ImGui::TableSetupColumn("Operation");
                ImGui::TableSetupColumn("Element bytes");
                ImGui::TableSetupColumn("Cache-line straddles");
                ImGui::TableSetupColumn("Page straddles");
                ImGui::TableHeadersRow();
                for (auto const& column : access.columns) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(column.name.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(detail::access_operation_name(column.operation));
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(detail::format_bytes(column.element_bytes).c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(
                        detail::format_number(column.aligned_cache_line_straddling_elements)
                            .c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(
                        detail::format_number(column.aligned_page_straddling_elements).c_str());
                }
                ImGui::EndTable();
            }
            ImGui::TextDisabled(access.footprint_exact
                                    ? "Counts use each column's actual offset within the "
                                      "region-aligned contiguous block."
                                    : "Counts assume each separate column allocation begins on "
                                      "the corresponding cache-line/page boundary.");
            ImGui::TreePop();
        }
        ImGui::TextDisabled(access.footprint_exact
                                ? "One sequential classified access to each selected column at the "
                                  "current count. Line/page values are exact for the selected "
                                  "prefixes in a region-aligned contiguous block, not write "
                                  "allocation, measured traffic, or performance. Logical useful "
                                  "totals multiply by accesses per element only."
                                : "One sequential classified access to each selected column at the "
                                  "current count. Each standard-library column is a separate "
                                  "allocation; line/page values are lower bounds, not write "
                                  "allocation, measured traffic, or performance. Logical useful "
                                  "totals multiply by accesses per element only.");
        draw_diagnostics(access.diagnostics);
    } else {
        ImGui::TextDisabled("Select SoA columns in the Access column to define the access set.");
    }

    ImGui::SeparatorText("Cache-line / page block map");
    if (analysis_session_.inputs.soa_allocation_strategy ==
        SoaAllocationStrategy::separate_columns) {
        ImGui::TextDisabled(
            "A single address map is unavailable because separate allocation origins are Unknown."
            " Select the aligned contiguous strategy to inspect one region-aligned block.");
    } else {
        auto region_kind{soa_region_map_pages_ ? 1 : 0};
        ImGui::SetNextItemWidth(180.0F);
        if (ImGui::Combo("Region", &region_kind, "Cache lines\0Pages\0")) {
            soa_region_map_pages_ = region_kind == 1;
        }
        ImGui::SetNextItemWidth(240.0F);
        ImGui::SliderFloat("Zoom",
                           &soa_region_pixels_,
                           16.0F,
                           4'096.0F,
                           "%.0f px / region",
                           ImGuiSliderFlags_Logarithmic);
        draw_soa_region_map(active,
                            analysis_session_.results().soa_access_analysis.has_value()
                                ? &*analysis_session_.results().soa_access_analysis
                                : nullptr,
                            soa_region_map_pages_,
                            soa_region_pixels_);
        if (analysis_session_.results().soa_access_analysis.has_value() &&
            !analysis_session_.results().soa_access_analysis->footprint_exact) {
            ImGui::TextDisabled(
                "The selected-prefix overlay is unavailable because the access count exceeds "
                "capacity or the contiguous allocation is Unknown.");
        }
        ImGui::TextDisabled(
            "The block origin is aligned to the selected target region. Boundaries and selected "
            "prefixes show addresses only; they are not cache traffic or a performance estimate.");
    }

    if (analysis_session_.results().record_soa_access_comparison.has_value() &&
        soa.equivalent_type.has_value()) {
        auto const& comparison{*analysis_session_.results().record_soa_access_comparison};
        auto const& record_identity{
            analysis_session_.inputs.workspace.types().type(soa.equivalent_type->type).identity};
        ImGui::SeparatorText("Equivalent AoS / SoA access");
        ImGui::Text("Equivalent record: %s", record_identity.name.c_str());
        if (ImGui::BeginTable("record-soa-access-comparison",
                              4,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Metric");
            ImGui::TableSetupColumn("AoS exact");
            ImGui::TableSetupColumn(comparison.soa_footprint_exact ? "SoA exact" : "SoA minimum");
            ImGui::TableSetupColumn("SoA - AoS");
            ImGui::TableHeadersRow();
            auto draw_stat{[](char const* const label,
                              std::string const& record_value,
                              std::string const& soa_value,
                              std::string const& delta) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(label);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(record_value.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(soa_value.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(delta.c_str());
            }};
            draw_stat("Classified read useful payload",
                      detail::format_bytes(comparison.record.read_useful_bytes),
                      detail::format_bytes(comparison.soa.read_useful_bytes),
                      detail::format_delta_bytes(comparison.read_useful_byte_delta));
            draw_stat("Classified write useful payload",
                      detail::format_bytes(comparison.record.write_useful_bytes),
                      detail::format_bytes(comparison.soa.write_useful_bytes),
                      detail::format_delta_bytes(comparison.write_useful_byte_delta));
            draw_stat("Logical read useful payload",
                      detail::format_bytes(comparison.record.logical_read_useful_bytes),
                      detail::format_bytes(comparison.soa.logical_read_useful_bytes),
                      detail::format_delta_bytes(comparison.logical_read_useful_byte_delta));
            draw_stat("Logical write useful payload",
                      detail::format_bytes(comparison.record.logical_write_useful_bytes),
                      detail::format_bytes(comparison.soa.logical_write_useful_bytes),
                      detail::format_delta_bytes(comparison.logical_write_useful_byte_delta));
            draw_stat("Useful payload",
                      detail::format_bytes(comparison.record.useful_bytes),
                      detail::format_bytes(comparison.soa.useful_bytes),
                      detail::format_delta_bytes(comparison.useful_byte_delta));
            draw_stat("Cache lines",
                      detail::format_number(comparison.record.cache_lines),
                      detail::format_number(comparison.soa.cache_lines),
                      detail::format_delta_number(comparison.cache_line_delta));
            draw_stat("Cache-line footprint",
                      detail::format_bytes(comparison.record.cache_bytes),
                      detail::format_bytes(comparison.soa.cache_bytes),
                      detail::format_delta_bytes(comparison.cache_byte_delta));
            draw_stat("Read cache-line address coverage",
                      detail::format_bytes(comparison.record.read_cache_bytes),
                      detail::format_bytes(comparison.soa.read_cache_bytes),
                      detail::format_delta_bytes(comparison.read_cache_byte_delta));
            draw_stat("Write cache-line address coverage",
                      detail::format_bytes(comparison.record.write_cache_bytes),
                      detail::format_bytes(comparison.soa.write_cache_bytes),
                      detail::format_delta_bytes(comparison.write_cache_byte_delta));
            draw_stat("Non-useful bytes in cache-line footprint",
                      detail::format_bytes(comparison.record_non_useful_cache_bytes),
                      detail::format_bytes(comparison.soa_non_useful_cache_bytes),
                      detail::format_delta_bytes(comparison.non_useful_cache_byte_delta));
            draw_stat(
                "Footprint <= L1 data cache",
                detail::format_fit(comparison.record_cache_footprint_capacity.fits_l1_data),
                detail::format_fit(comparison.soa_minimum_cache_footprint_capacity.fits_l1_data),
                "—");
            draw_stat("Footprint <= L2 cache",
                      detail::format_fit(comparison.record_cache_footprint_capacity.fits_l2),
                      detail::format_fit(comparison.soa_minimum_cache_footprint_capacity.fits_l2),
                      "—");
            draw_stat("Footprint <= L3 cache",
                      detail::format_fit(comparison.record_cache_footprint_capacity.fits_l3),
                      detail::format_fit(comparison.soa_minimum_cache_footprint_capacity.fits_l3),
                      "—");
            draw_stat("Pages",
                      detail::format_number(comparison.record.pages),
                      detail::format_number(comparison.soa.pages),
                      detail::format_delta_number(comparison.page_delta));
            draw_stat("Page footprint",
                      detail::format_bytes(comparison.record.page_bytes),
                      detail::format_bytes(comparison.soa.page_bytes),
                      detail::format_delta_bytes(comparison.page_byte_delta));
            draw_stat("Read page address coverage",
                      detail::format_bytes(comparison.record.read_page_bytes),
                      detail::format_bytes(comparison.soa.read_page_bytes),
                      detail::format_delta_bytes(comparison.read_page_byte_delta));
            draw_stat("Write page address coverage",
                      detail::format_bytes(comparison.record.write_page_bytes),
                      detail::format_bytes(comparison.soa.write_page_bytes),
                      detail::format_delta_bytes(comparison.write_page_byte_delta));
            draw_stat("Non-useful bytes in page footprint",
                      detail::format_bytes(comparison.record_non_useful_page_bytes),
                      detail::format_bytes(comparison.soa_non_useful_page_bytes),
                      detail::format_delta_bytes(comparison.non_useful_page_byte_delta));
            ImGui::EndTable();
        }
        ImGui::TextDisabled("Cache-line footprint composition");
        draw_footprint_composition("AoS exact cache-line footprint",
                                   comparison.record.useful_bytes,
                                   comparison.record.cache_bytes,
                                   comparison.record_non_useful_cache_bytes);
        draw_footprint_composition(comparison.soa_footprint_exact
                                       ? "SoA exact cache-line footprint"
                                       : "SoA minimum cache-line footprint",
                                   comparison.soa.useful_bytes,
                                   comparison.soa.cache_bytes,
                                   comparison.soa_non_useful_cache_bytes);
        ImGui::TextDisabled("Page footprint composition");
        draw_footprint_composition("AoS exact page footprint",
                                   comparison.record.useful_bytes,
                                   comparison.record.page_bytes,
                                   comparison.record_non_useful_page_bytes);
        draw_footprint_composition(comparison.soa_footprint_exact ? "SoA exact page footprint"
                                                                  : "SoA minimum page footprint",
                                   comparison.soa.useful_bytes,
                                   comparison.soa.page_bytes,
                                   comparison.soa_non_useful_page_bytes);
        ImGui::TextDisabled(
            comparison.soa_footprint_exact
                ? "AoS figures use an aligned contiguous object array; SoA figures "
                  "use the selected aligned contiguous column block. Both are exact "
                  "under their stated region-aligned origins; deltas are not speed "
                  "estimates."
                : "AoS figures are exact for an aligned contiguous object array. "
                  "SoA figures are lower bounds across separate column allocations "
                  "because their origins are unknown; deltas are not speed estimates.");
        draw_diagnostics(comparison.diagnostics);
    }

    bool activated{};
    draw_payload_regions(baseline,
                         nullptr,
                         analysis_session_.inputs.selection.field,
                         "Baseline",
                         common_total,
                         activated);
    for (auto const& [variant_id, analysis] : analysis_session_.results().soa_variants) {
        auto const* variant{analysis_session_.inputs.workspace.variant(variant_id)};
        if (variant == nullptr) {
            continue;
        }
        auto const label{variant->name +
                         (variant_id == analysis_session_.inputs.workspace.active_variant_id()
                              ? " (editing)"
                              : "")};
        activated = false;
        ImGui::PushID(static_cast<int>(variant_id));
        draw_payload_regions(analysis,
                             &baseline,
                             analysis_session_.inputs.selection.field,
                             label.c_str(),
                             common_total,
                             activated);
        ImGui::PopID();
        if (activated && analysis_session_.inputs.workspace.active_variant_id() != variant_id) {
            analysis_session_.inputs.workspace.select_variant(variant_id);
            sync_variant_name();
        }
    }
    ImGui::TextDisabled(
        analysis_session_.inputs.soa_allocation_strategy == SoaAllocationStrategy::separate_columns
            ? "Regions compare aggregate payload across separate standard-library allocations."
            : "Regions compare one alignment-aware contiguous column allocation, including gaps.");

    auto const found{std::ranges::find(
        active.columns, analysis_session_.inputs.selection.field, &SoaColumnAnalysis::name)};
    if (found != active.columns.end() && found->cache_line_tiling.has_value()) {
        draw_cache_line(*found->cache_line_tiling);
    }
}

} // namespace ioj::layout_planner
