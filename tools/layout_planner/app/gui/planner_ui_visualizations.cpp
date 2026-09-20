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
    if (analysis.unused_bits.value_or(0) != 0 && analysis.bits_used.has_value()) {
        has_unused = true;
        if (most_significant_first) {
            unused_left = origin.x;
            unused_right =
                origin.x + available * static_cast<float>(*analysis.unused_bits) / denominator;
        } else {
            unused_left =
                origin.x + available * static_cast<float>(*analysis.bits_used) / denominator;
            unused_right = origin.x + storage_width;
        }
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
                    ImGui::Text("Relationship: %s -> %s",
                                std::string{codegen::packed_field_relation_kind_name(
                                                *field.relationship_kind)}
                                    .c_str(),
                                field.relationship_target->c_str());
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
    activated = clicked;
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
            changed = workspace_.set_element_count(preset.count) || changed;
        }
        ImGui::PopID();
        ImGui::SameLine();
    }
    auto custom_count{workspace_.element_count()};
    ImGui::SetNextItemWidth(150.0F);
    if (ImGui::InputScalar("Elements", ImGuiDataType_U64, &custom_count) && custom_count != 0) {
        changed = workspace_.set_element_count(custom_count) || changed;
    }
    return changed;
}

void PlannerUi::draw_packed_layout(PackedType const& packed, PackedAnalysis const& baseline) {
    auto const& identity{workspace_.types().type(baseline.type).identity};
    ImGui::Text("%s", identity.name.c_str());
    ImGui::TextDisabled("%s", identity.module_name.c_str());

    ImGui::SeparatorText("Analysis scale");
    if (draw_element_count()) {
        return;
    }

    auto const& scale_analysis{*active_packed_};
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
        draw_stat("Minimum pages", detail::format_number(aggregate.minimum_pages));
        draw_stat("Complete elements / page",
                  detail::format_number(aggregate.complete_elements_per_page));
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
                        abi_.memory_facts().provenance.empty()
                            ? "Unknown"
                            : abi_.memory_facts().provenance.c_str());

    ImGui::SeparatorText("Bit layout");
    auto common_bits{std::max(
        {baseline.storage_bits.value_or(0), baseline.bits_used.value_or(0), std::uint64_t{1}})};
    for (auto const& [variant_id, analysis] : packed_variants_) {
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
                    selected_field_,
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
                auto const selected_field{selected_field_};
                if (apply_document_edit(ReplacePackedValue{.declaration = *declaration,
                                                           .schema = std::move(replacement)})) {
                    selected_field_ = selected_field;
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
    for (auto const& [variant_id, analysis] : packed_variants_) {
        auto const* variant{workspace_.variant(variant_id)};
        if (variant == nullptr) {
            continue;
        }
        auto const label{variant->name +
                         (variant_id == workspace_.active_variant_id() ? " (editing)" : "")};
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
                        selected_field_,
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
        if (activated && workspace_.active_variant_id() != variant_id) {
            workspace_.select_variant(variant_id);
            sync_variant_name();
        }
        if (adjustment.has_value() && adjustment->left_field_index + 1 < packed.segments.size()) {
            if (workspace_.active_variant_id() != variant_id) {
                workspace_.select_variant(variant_id);
                sync_variant_name();
            }
            auto const* left_field{
                std::get_if<PackedField>(&packed.segments[adjustment->left_field_index])};
            auto const* right_field{
                std::get_if<PackedField>(&packed.segments[adjustment->left_field_index + 1])};
            if (left_field == nullptr || right_field == nullptr) {
                continue;
            }
            workspace_.set_packed_field_width(
                analysis.type,
                left_field->name,
                adjustment->left_width == left_field->bit_width
                    ? std::optional<std::uint32_t>{}
                    : std::optional<std::uint32_t>{adjustment->left_width});
            workspace_.set_packed_field_width(
                analysis.type,
                right_field->name,
                adjustment->right_width == right_field->bit_width
                    ? std::optional<std::uint32_t>{}
                    : std::optional<std::uint32_t>{adjustment->right_width});
        }
        if (analysis.unused_bits.value_or(0) != 0) {
            ImGui::TextDisabled("Hatched region: %llu unused storage bit%s.",
                                static_cast<unsigned long long>(*analysis.unused_bits),
                                *analysis.unused_bits == 1 ? "" : "s");
        }
        if (analysis.overflow_bits.value_or(0) != 0) {
            ImGui::TextColored(overflow_color,
                               "%llu planned bit%s exceed storage.",
                               static_cast<unsigned long long>(*analysis.overflow_bits),
                               *analysis.overflow_bits == 1 ? "" : "s");
        }
    }
    if (!packed_variants_.empty()) {
        ImGui::TextDisabled("Click a variant to edit it. Drag a divider to transfer whole bits.");
    }
}

void PlannerUi::draw_record_layout(RecordAnalysis const& analysis) {
    auto const& node{workspace_.types().type(analysis.type)};
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
        draw_stat("Internal padding", detail::format_bytes(aggregate.total_internal_padding_bytes));
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
                        abi_.memory_facts().provenance.empty()
                            ? "Unknown"
                            : abi_.memory_facts().provenance.c_str());
    ImGui::TextDisabled(
        "Boundary crossing assumes a contiguous array whose base is cache-line/page aligned.");

    ImGui::SeparatorText("Sequential access set");
    if (record_access_analysis_.has_value()) {
        auto const& access{*record_access_analysis_};
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
            draw_stat("Useful selected bytes", detail::format_bytes(access.useful_bytes));
            draw_stat("Enclosing AoS footprint",
                      detail::format_bytes(access.object_footprint_bytes));
            draw_stat("Distinct cache lines touched",
                      detail::format_number(access.cache_lines_touched));
            draw_stat("Bytes in touched cache lines",
                      detail::format_bytes(access.cache_bytes_touched));
            draw_stat("Non-selected bytes in touched cache lines",
                      detail::format_bytes(access.non_selected_cache_bytes));
            draw_stat("Distinct pages touched", detail::format_number(access.pages_touched));
            ImGui::EndTable();
        }
        ImGui::TextDisabled(
            "One sequential read of the selected members per element; aligned contiguous AoS base "
            "assumed.");
        draw_diagnostics(access.diagnostics);
    } else {
        ImGui::TextDisabled("Select record members in the Access column to define the access set.");
    }

    ImGui::SeparatorText("Object layout");
    ImGui::Text("Size: %s B    Alignment: %s B",
                detail::format_number(analysis.size_bytes).c_str(),
                detail::format_number(analysis.alignment_bytes).c_str());
    ImGui::Text("Member extents: %s B    Internal padding: %s B    Tail padding: %s B",
                detail::format_number(analysis.payload_bytes).c_str(),
                detail::format_number(analysis.internal_padding_bytes).c_str(),
                detail::format_number(analysis.tail_padding_bytes).c_str());

    if (analysis.size_bytes.has_value() && *analysis.size_bytes != 0) {
        auto const width{std::max(1.0F, ImGui::GetContentRegionAvail().x)};
        auto const origin{ImGui::GetCursorScreenPos()};
        constexpr auto height{58.0F};
        auto* const draw_list{ImGui::GetWindowDrawList()};
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
            auto const selected{selected_field_ == member.name};
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
        draw_list->AddRect(origin,
                           {origin.x + width, origin.y + height},
                           ImGui::GetColorU32(ImGuiCol_Border),
                           3.0F,
                           0,
                           2.0F);
        ImGui::Dummy({width, height + 4.0F});
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
                                  selected_field_ == member.name,
                                  ImGuiSelectableFlags_None)) {
                selected_field_ = member.name;
            }
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(
                workspace_.types().type(member.semantic_type).cpp_spelling.c_str());
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
    auto const& active{*active_soa_};
    auto const& identity{workspace_.types().type(baseline.type).identity};
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
    if (ImGui::BeginTable("soa-columns",
                          9,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Column");
        ImGui::TableSetupColumn("Schema type");
        ImGui::TableSetupColumn("Planning type");
        ImGui::TableSetupColumn("Element B");
        ImGui::TableSetupColumn("Payload");
        ImGui::TableSetupColumn("Min lines");
        ImGui::TableSetupColumn("Exact / line");
        ImGui::TableSetupColumn("Min pages");
        ImGui::TableSetupColumn("Complete / page");
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
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(detail::format_number(column.minimum_pages).c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(
                detail::format_number(column.complete_elements_per_page).c_str());
        }
        ImGui::EndTable();
    }

    auto common_total{baseline.total_payload_bytes.value_or(0)};
    for (auto const& [variant_id, analysis] : soa_variants_) {
        static_cast<void>(variant_id);
        common_total = std::max(common_total, analysis.total_payload_bytes.value_or(0));
    }
    ImGui::SeparatorText("Aggregate column payload");
    ImGui::Text("Minimum pages across separate columns: %s",
                detail::format_number(active.minimum_pages).c_str());
    ImGui::Text("Fits L1 data cache: %s",
                detail::format_fit(active.cache_capacity.fits_l1_data).c_str());
    ImGui::Text("Fits L2 cache: %s", detail::format_fit(active.cache_capacity.fits_l2).c_str());
    ImGui::Text("Fits L3 cache: %s", detail::format_fit(active.cache_capacity.fits_l3).c_str());

    ImGui::SeparatorText("Sequential access set");
    if (soa_access_analysis_.has_value()) {
        auto const& access{*soa_access_analysis_};
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
            draw_stat("Useful selected payload", detail::format_bytes(access.useful_bytes));
            draw_stat("Complete logical payload at count",
                      detail::format_bytes(access.full_logical_payload_bytes));
            draw_stat("Unselected payload at count",
                      detail::format_bytes(access.unselected_payload_bytes));
            draw_stat("Allocated payload at capacity",
                      detail::format_bytes(access.allocated_capacity_payload_bytes));
            draw_stat("Unused allocated capacity payload",
                      detail::format_bytes(access.capacity_slack_payload_bytes));
            draw_stat("Minimum cache lines touched",
                      detail::format_number(access.minimum_cache_lines_touched));
            draw_stat("Minimum cache-line footprint",
                      detail::format_bytes(access.minimum_cache_bytes_touched));
            draw_stat("Non-payload bytes in minimum cache footprint",
                      detail::format_bytes(access.non_payload_cache_bytes));
            draw_stat("Minimum pages touched", detail::format_number(access.minimum_pages_touched));
            draw_stat("Minimum page footprint",
                      detail::format_bytes(access.minimum_page_bytes_touched));
            draw_stat("Non-payload bytes in minimum page footprint",
                      detail::format_bytes(access.non_payload_page_bytes));
            ImGui::EndTable();
        }
        ImGui::TextDisabled(
            "One sequential access to each selected column at the current count. Each standard-"
            "library column is a separate allocation; line/page values are minimum footprints, "
            "not measured traffic or performance.");
        draw_diagnostics(access.diagnostics);
    } else {
        ImGui::TextDisabled("Select SoA columns in the Access column to define the access set.");
    }

    bool activated{};
    draw_payload_regions(baseline, nullptr, selected_field_, "Baseline", common_total, activated);
    for (auto const& [variant_id, analysis] : soa_variants_) {
        auto const* variant{workspace_.variant(variant_id)};
        if (variant == nullptr) {
            continue;
        }
        auto const label{variant->name +
                         (variant_id == workspace_.active_variant_id() ? " (editing)" : "")};
        activated = false;
        ImGui::PushID(static_cast<int>(variant_id));
        draw_payload_regions(
            analysis, &baseline, selected_field_, label.c_str(), common_total, activated);
        ImGui::PopID();
        if (activated && workspace_.active_variant_id() != variant_id) {
            workspace_.select_variant(variant_id);
            sync_variant_name();
        }
    }
    ImGui::TextDisabled("Regions compare aggregate column payload; standard-library columns remain "
                        "separate allocations.");

    auto const found{std::ranges::find(active.columns, selected_field_, &SoaColumnAnalysis::name)};
    if (found != active.columns.end() && found->cache_line_tiling.has_value()) {
        draw_cache_line(*found->cache_line_tiling);
    }
}

} // namespace ioj::layout_planner
