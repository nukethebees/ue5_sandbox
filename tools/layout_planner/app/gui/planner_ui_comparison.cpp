#include "planner_ui.hpp"

#include "planner_ui_support.hpp"

#include <imgui.h>

#include <string>
#include <utility>

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

    auto draw_variant_selector = [&](char const* id, std::uint64_t& selected_id) {
        auto changed{false};
        auto const* selected{workspace_.variant(selected_id)};
        ImGui::SetNextItemWidth(-1.0F);
        ImGui::PushID(id);
        if (ImGui::BeginCombo("##variant", selected->name.c_str())) {
            for (auto const& variant : workspace_.variants()) {
                auto const is_selected{variant.id == selected_id};
                if (ImGui::Selectable(variant.name.c_str(), is_selected)) {
                    selected_id = variant.id;
                    changed = true;
                }
                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::PopID();
        return changed;
    };

    auto selection_changed{false};
    if (ImGui::BeginTable("comparison-variants",
                          2,
                          ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings)) {
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("A");
        selection_changed |= draw_variant_selector("comparison-a", comparison_a_variant_id_);
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("B");
        if (draw_variant_selector("comparison-b", comparison_b_variant_id_)) {
            comparison_b_follows_active_ = false;
            selection_changed = true;
        }
        ImGui::TableNextColumn();
        if (ImGui::Button("Swap A / B", {-1.0F, 0.0F})) {
            std::swap(comparison_a_variant_id_, comparison_b_variant_id_);
            comparison_b_follows_active_ = false;
            selection_changed = true;
        }
        ImGui::TableNextColumn();
        ImGui::BeginDisabled(comparison_b_follows_active_ &&
                             comparison_b_variant_id_ == workspace_.active_variant_id());
        if (ImGui::Button("Use active variant for B", {-1.0F, 0.0F})) {
            comparison_b_variant_id_ = workspace_.active_variant_id();
            comparison_b_follows_active_ = true;
            selection_changed = true;
        }
        ImGui::EndDisabled();
        ImGui::EndTable();
    }
    if (comparison_b_follows_active_) {
        ImGui::TextDisabled("B follows the active editing variant.");
    }
    if (selection_changed) {
        refresh_analysis();
    }

    auto const& comparison_a{*workspace_.variant(comparison_a_variant_id_)};
    auto const& comparison_b{*workspace_.variant(comparison_b_variant_id_)};

    if (comparison_a_packed_.has_value() && comparison_b_packed_.has_value()) {
        if (ImGui::BeginTable("packed-comparison",
                              4,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("Fact");
            ImGui::TableSetupColumn(comparison_a.name.c_str());
            ImGui::TableSetupColumn(comparison_b.name.c_str());
            ImGui::TableSetupColumn("Difference");
            ImGui::TableHeadersRow();
            comparison_row("Storage type",
                           comparison_a_packed_->storage_type,
                           comparison_b_packed_->storage_type);
            comparison_row(
                "Storage bytes",
                detail::format_bytes(comparison_a_packed_->storage_facts.transform(
                    [](layout::TypeFacts const& facts) { return facts.size_bytes; })),
                detail::format_bytes(comparison_b_packed_->storage_facts.transform(
                    [](layout::TypeFacts const& facts) { return facts.size_bytes; })),
                detail::format_delta_bytes(layout::numeric_delta(
                    comparison_a_packed_->storage_facts.transform(
                        [](layout::TypeFacts const& facts) { return facts.size_bytes; }),
                    comparison_b_packed_->storage_facts.transform(
                        [](layout::TypeFacts const& facts) { return facts.size_bytes; }))));
            comparison_row(
                "Storage bits",
                detail::format_number(comparison_a_packed_->storage_bits),
                detail::format_number(comparison_b_packed_->storage_bits),
                detail::format_delta_number(layout::numeric_delta(
                    comparison_a_packed_->storage_bits, comparison_b_packed_->storage_bits)));
            comparison_row("Bits used",
                           detail::format_number(comparison_a_packed_->bits_used),
                           detail::format_number(comparison_b_packed_->bits_used),
                           detail::format_delta_number(layout::numeric_delta(
                               comparison_a_packed_->bits_used, comparison_b_packed_->bits_used)));
            comparison_row("Unused bits",
                           detail::format_number(comparison_a_packed_->unused_bits),
                           detail::format_number(comparison_b_packed_->unused_bits));
            comparison_row("Overflow bits",
                           detail::format_number(comparison_a_packed_->overflow_bits),
                           detail::format_number(comparison_b_packed_->overflow_bits));
            comparison_row(
                "Scaled storage",
                detail::format_bytes(comparison_a_packed_->aggregate.total_storage_bytes),
                detail::format_bytes(comparison_b_packed_->aggregate.total_storage_bytes),
                detail::format_delta_bytes(
                    layout::numeric_delta(comparison_a_packed_->aggregate.total_storage_bytes,
                                          comparison_b_packed_->aggregate.total_storage_bytes)));
            comparison_row("Scaled unused bits",
                           detail::format_number(comparison_a_packed_->aggregate.total_unused_bits),
                           detail::format_number(comparison_b_packed_->aggregate.total_unused_bits),
                           detail::format_delta_number(layout::numeric_delta(
                               comparison_a_packed_->aggregate.total_unused_bits,
                               comparison_b_packed_->aggregate.total_unused_bits)));
            comparison_row(
                "Minimum cache lines",
                detail::format_number(comparison_a_packed_->aggregate.minimum_cache_lines),
                detail::format_number(comparison_b_packed_->aggregate.minimum_cache_lines),
                detail::format_delta_number(
                    layout::numeric_delta(comparison_a_packed_->aggregate.minimum_cache_lines,
                                          comparison_b_packed_->aggregate.minimum_cache_lines)));
            comparison_row("Minimum pages",
                           detail::format_number(comparison_a_packed_->aggregate.minimum_pages),
                           detail::format_number(comparison_b_packed_->aggregate.minimum_pages),
                           detail::format_delta_number(layout::numeric_delta(
                               comparison_a_packed_->aggregate.minimum_pages,
                               comparison_b_packed_->aggregate.minimum_pages)));
            for (auto const& baseline : comparison_a_packed_->fields) {
                auto const* active{field_by_name(*comparison_b_packed_, baseline.name)};
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
        draw_diagnostics(comparison_b_packed_->diagnostics);
    } else if (comparison_a_soa_.has_value() && comparison_b_soa_.has_value()) {
        if (ImGui::BeginTable("soa-comparison",
                              4,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("Fact");
            ImGui::TableSetupColumn(comparison_a.name.c_str());
            ImGui::TableSetupColumn(comparison_b.name.c_str());
            ImGui::TableSetupColumn("Difference");
            ImGui::TableHeadersRow();
            comparison_row("Capacity",
                           std::to_string(comparison_a_soa_->capacity),
                           std::to_string(comparison_b_soa_->capacity),
                           detail::format_delta_number(layout::numeric_delta(
                               comparison_a_soa_->capacity, comparison_b_soa_->capacity)));
            comparison_row("Bytes / logical entity",
                           detail::format_bytes(comparison_a_soa_->bytes_per_logical_element),
                           detail::format_bytes(comparison_b_soa_->bytes_per_logical_element),
                           detail::format_delta_bytes(layout::numeric_delta(
                               comparison_a_soa_->bytes_per_logical_element,
                               comparison_b_soa_->bytes_per_logical_element)));
            comparison_row("Total payload",
                           detail::format_bytes(comparison_a_soa_->total_payload_bytes),
                           detail::format_bytes(comparison_b_soa_->total_payload_bytes),
                           detail::format_delta_bytes(
                               layout::numeric_delta(comparison_a_soa_->total_payload_bytes,
                                                     comparison_b_soa_->total_payload_bytes)));
            for (auto const& baseline : comparison_a_soa_->columns) {
                auto const* active{column_by_name(*comparison_b_soa_, baseline.name)};
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
        draw_diagnostics(comparison_b_soa_->diagnostics);
    }
    ImGui::End();
}

} // namespace ioj::layout_planner
