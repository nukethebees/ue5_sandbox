#include "planner_ui.hpp"

#include "planner_ui_support.hpp"

#include <imgui.h>

#include <string>

namespace ioj::layout_planner {
namespace {

using namespace layout;

inline constexpr ImVec4 changed_color{0.4F, 0.75F, 0.95F, 1.0F};

void comparison_row(char const* const label,
                    std::string const& baseline,
                    std::string const& variant,
                    std::string const& difference = {}) {
    auto const changed{baseline != variant};
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(label);
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(baseline.c_str());
    ImGui::TableNextColumn();
    if (changed) {
        ImGui::PushStyleColor(ImGuiCol_Text, changed_color);
    }
    ImGui::TextUnformatted(variant.c_str());
    if (changed) {
        ImGui::PopStyleColor();
    }
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(difference.empty() ? (changed ? "Changed" : "—") : difference.c_str());
}

auto field_by_name(layout::PackedAnalysis const& analysis, std::string const& name)
    -> layout::PackedFieldAnalysis const* {
    auto const found{std::ranges::find(analysis.fields, name, &layout::PackedFieldAnalysis::name)};
    return found == analysis.fields.end() ? nullptr : &*found;
}

auto column_by_name(layout::SoaAnalysis const& analysis, std::string const& name)
    -> layout::SoaColumnAnalysis const* {
    auto const found{std::ranges::find(analysis.columns, name, &layout::SoaColumnAnalysis::name)};
    return found == analysis.columns.end() ? nullptr : &*found;
}

} // namespace

void PlannerUi::draw_comparison_panel() {
    ImGui::Begin("Comparison");
    ImGui::TextDisabled("ABI profile: %s", abi_.name().c_str());
    if (workspace_.active_variant_id() == LayoutWorkspace::baseline_variant_id) {
        ImGui::TextDisabled(
            "Baseline is selected. Create an experiment to compare a planning change.");
    }

    if (baseline_packed_.has_value() && active_packed_.has_value()) {
        if (ImGui::BeginTable("packed-comparison",
                              4,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("Fact");
            ImGui::TableSetupColumn("Baseline");
            ImGui::TableSetupColumn("Variant");
            ImGui::TableSetupColumn("Difference");
            ImGui::TableHeadersRow();
            comparison_row(
                "Storage type", baseline_packed_->storage_type, active_packed_->storage_type);
            comparison_row(
                "Storage bytes",
                detail::format_bytes(baseline_packed_->storage_facts.transform(
                    [](layout::TypeFacts const& facts) { return facts.size_bytes; })),
                detail::format_bytes(active_packed_->storage_facts.transform(
                    [](layout::TypeFacts const& facts) { return facts.size_bytes; })),
                detail::format_delta_bytes(layout::numeric_delta(
                    baseline_packed_->storage_facts.transform(
                        [](layout::TypeFacts const& facts) { return facts.size_bytes; }),
                    active_packed_->storage_facts.transform(
                        [](layout::TypeFacts const& facts) { return facts.size_bytes; }))));
            comparison_row("Storage bits",
                           detail::format_number(baseline_packed_->storage_bits),
                           detail::format_number(active_packed_->storage_bits),
                           detail::format_delta_number(layout::numeric_delta(
                               baseline_packed_->storage_bits, active_packed_->storage_bits)));
            comparison_row("Bits used",
                           detail::format_number(baseline_packed_->bits_used),
                           detail::format_number(active_packed_->bits_used),
                           detail::format_delta_number(layout::numeric_delta(
                               baseline_packed_->bits_used, active_packed_->bits_used)));
            comparison_row("Unused bits",
                           detail::format_number(baseline_packed_->unused_bits),
                           detail::format_number(active_packed_->unused_bits));
            comparison_row("Overflow bits",
                           detail::format_number(baseline_packed_->overflow_bits),
                           detail::format_number(active_packed_->overflow_bits));
            for (auto const& baseline : baseline_packed_->fields) {
                auto const* active{field_by_name(*active_packed_, baseline.name)};
                if (active == nullptr) {
                    continue;
                }
                comparison_row((baseline.name + " width").c_str(),
                               std::to_string(baseline.bit_width),
                               std::to_string(active->bit_width),
                               detail::format_delta_number(
                                   layout::numeric_delta(baseline.bit_width, active->bit_width)));
                auto const baseline_range{
                    baseline.most_significant_bit.has_value()
                        ? "[" + std::to_string(*baseline.most_significant_bit) + ":" +
                              std::to_string(baseline.least_significant_bit) + "]"
                        : "Invalid"};
                auto const active_range{
                    active->most_significant_bit.has_value()
                        ? "[" + std::to_string(*active->most_significant_bit) + ":" +
                              std::to_string(active->least_significant_bit) + "]"
                        : "Invalid"};
                comparison_row((baseline.name + " range").c_str(), baseline_range, active_range);
                comparison_row((baseline.name + " maximum").c_str(),
                               detail::format_number(baseline.maximum_unsigned_value),
                               detail::format_number(active->maximum_unsigned_value));
            }
            ImGui::EndTable();
        }
        draw_diagnostics(active_packed_->diagnostics);
    } else if (baseline_soa_.has_value() && active_soa_.has_value()) {
        if (ImGui::BeginTable("soa-comparison",
                              4,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("Fact");
            ImGui::TableSetupColumn("Baseline");
            ImGui::TableSetupColumn("Variant");
            ImGui::TableSetupColumn("Difference");
            ImGui::TableHeadersRow();
            comparison_row("Capacity",
                           std::to_string(baseline_soa_->capacity),
                           std::to_string(active_soa_->capacity),
                           detail::format_delta_number(layout::numeric_delta(
                               baseline_soa_->capacity, active_soa_->capacity)));
            comparison_row("Bytes / logical entity",
                           detail::format_bytes(baseline_soa_->bytes_per_logical_element),
                           detail::format_bytes(active_soa_->bytes_per_logical_element),
                           detail::format_delta_bytes(
                               layout::numeric_delta(baseline_soa_->bytes_per_logical_element,
                                                     active_soa_->bytes_per_logical_element)));
            comparison_row(
                "Total payload",
                detail::format_bytes(baseline_soa_->total_payload_bytes),
                detail::format_bytes(active_soa_->total_payload_bytes),
                detail::format_delta_bytes(layout::numeric_delta(
                    baseline_soa_->total_payload_bytes, active_soa_->total_payload_bytes)));
            for (auto const& baseline : baseline_soa_->columns) {
                auto const* active{column_by_name(*active_soa_, baseline.name)};
                if (active == nullptr) {
                    continue;
                }
                comparison_row((baseline.name + " type").c_str(),
                               baseline.physical_type,
                               active->physical_type);
                comparison_row(
                    (baseline.name + " element bytes").c_str(),
                    detail::format_bytes(baseline.type_facts.transform(
                        [](layout::TypeFacts const& facts) { return facts.size_bytes; })),
                    detail::format_bytes(active->type_facts.transform(
                        [](layout::TypeFacts const& facts) { return facts.size_bytes; })),
                    detail::format_delta_bytes(layout::numeric_delta(
                        baseline.type_facts.transform(
                            [](layout::TypeFacts const& facts) { return facts.size_bytes; }),
                        active->type_facts.transform(
                            [](layout::TypeFacts const& facts) { return facts.size_bytes; }))));
                comparison_row((baseline.name + " payload").c_str(),
                               detail::format_bytes(baseline.total_bytes),
                               detail::format_bytes(active->total_bytes),
                               detail::format_delta_bytes(layout::numeric_delta(
                                   baseline.total_bytes, active->total_bytes)));
                comparison_row((baseline.name + " cache lines").c_str(),
                               detail::format_number(baseline.minimum_cache_lines),
                               detail::format_number(active->minimum_cache_lines));
                comparison_row((baseline.name + " elements / 64 B").c_str(),
                               detail::format_number(baseline.elements_per_cache_line),
                               detail::format_number(active->elements_per_cache_line));
            }
            ImGui::EndTable();
        }
        ImGui::TextDisabled("Cache-line counts are minimum payload coverage, not allocator traffic "
                            "or a performance estimate.");
        draw_diagnostics(active_soa_->diagnostics);
    }
    ImGui::End();
}

} // namespace ioj::layout_planner
