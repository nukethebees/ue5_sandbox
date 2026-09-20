#include "planner_ui.hpp"

#include "planner_ui_support.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace ioj::layout_planner {
namespace {

using namespace layout;
using namespace lispb::schema;

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

auto format_decimal(long double const value) -> std::string {
    std::array<char, 64> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "%.12g", static_cast<double>(value));
    return buffer.data();
}

auto format_decimal_delta(std::optional<long double> const value) -> std::string {
    if (!value.has_value()) {
        return "Unknown";
    }
    if (*value == 0.0L) {
        return "0";
    }
    std::array<char, 64> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "%+.12g", static_cast<double>(*value));
    return buffer.data();
}

auto format_optional_decimal(std::optional<long double> const value) -> std::string {
    return value.has_value() ? format_decimal(*value) : "Unknown";
}

auto type_label(lispb::schema::TypeNode const& node) -> std::string {
    return node.identity.module_name.empty()
             ? node.identity.name
             : node.identity.module_name + "::" + node.identity.name;
}

auto optional_source(TypeDefinition const& definition) -> std::optional<TypeId> {
    if (auto const* sentinel{std::get_if<OptionalSentinelType>(&definition)}) {
        return sentinel->source.type;
    }
    if (auto const* presence{std::get_if<OptionalPresenceBitType>(&definition)}) {
        return presence->source.type;
    }
    return std::nullopt;
}

auto optional_encoding_label(OptionalEncodingKind const kind) -> char const* {
    return kind == OptionalEncodingKind::sentinel ? "sentinel" : "presence bit";
}

} // namespace

void PlannerUi::draw_comparison_panel() {
    ImGui::Begin("Comparison");
    ImGui::TextDisabled("ABI profile: %s", abi_.name().c_str());

    if (selected_type_.has_value()) {
        auto const& selected_node{workspace_.types().type(*selected_type_)};
        auto const* selected_quantized{std::get_if<LinearQuantizedType>(&selected_node.definition)};
        if (selected_quantized != nullptr) {
            auto comparison_type{quantized_comparison_type_.has_value()
                                     ? workspace_.types().find(*quantized_comparison_type_)
                                     : std::nullopt};
            auto const comparison_label{comparison_type.has_value()
                                            ? type_label(workspace_.types().type(*comparison_type))
                                            : std::string{"No same-source representation"}};
            ImGui::Text("A: %s", type_label(selected_node).c_str());
            ImGui::SetNextItemWidth(-1.0F);
            if (ImGui::BeginCombo("B##quantized-comparison", comparison_label.c_str())) {
                auto const types{workspace_.types().types()};
                for (std::size_t index{}; index < types.size(); ++index) {
                    auto const candidate{TypeId{static_cast<std::uint32_t>(index)}};
                    auto const* quantized{
                        std::get_if<LinearQuantizedType>(&types[index].definition)};
                    if (candidate == *selected_type_ || quantized == nullptr ||
                        quantized->source.type != selected_quantized->source.type) {
                        continue;
                    }
                    auto const is_selected{comparison_type == candidate};
                    auto const label{type_label(types[index])};
                    if (ImGui::Selectable(label.c_str(), is_selected)) {
                        quantized_comparison_type_ = types[index].identity;
                        refresh_analysis();
                        comparison_type = candidate;
                    }
                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            if (!linear_quantized_comparison_.has_value()) {
                ImGui::TextDisabled(
                    "Declare another linear quantized representation of the same semantic source "
                    "to compare precision and encoded payload cost.");
                ImGui::End();
                return;
            }

            ImGui::SeparatorText("Analysis scale");
            if (draw_element_count()) {
                ImGui::End();
                return;
            }

            auto const& comparison{*linear_quantized_comparison_};
            auto const& second_node{workspace_.types().type(comparison.second.type)};
            if (ImGui::BeginTable("linear-quantized-comparison",
                                  4,
                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_Resizable)) {
                ImGui::TableSetupColumn("Fact");
                ImGui::TableSetupColumn(selected_node.identity.name.c_str());
                ImGui::TableSetupColumn(second_node.identity.name.c_str());
                ImGui::TableSetupColumn("Difference (B - A)");
                ImGui::TableHeadersRow();
                comparison_row(
                    "Semantic source",
                    workspace_.types().type(comparison.first.source_type).identity.name,
                    workspace_.types().type(comparison.second.source_type).identity.name);
                comparison_row("Encoded width",
                               std::to_string(comparison.first.encoded_storage_bits) + " bits",
                               std::to_string(comparison.second.encoded_storage_bits) + " bits",
                               detail::format_delta_number(comparison.encoded_storage_bit_delta));
                comparison_row("Total codes",
                               detail::format_code_count(comparison.first.total_code_count),
                               detail::format_code_count(comparison.second.total_code_count),
                               detail::format_delta_number(comparison.total_code_count_delta));
                comparison_row("Reserved codes",
                               std::to_string(comparison.first.reserved_code_count),
                               std::to_string(comparison.second.reserved_code_count),
                               detail::format_delta_number(comparison.reserved_code_count_delta));
                comparison_row("Usable codes",
                               detail::format_code_count(comparison.first.usable_code_count),
                               detail::format_code_count(comparison.second.usable_code_count),
                               detail::format_delta_number(comparison.usable_code_count_delta));
                comparison_row(
                    "Clipping",
                    std::string{codegen::quantization_clipping_name(comparison.first.clipping)},
                    std::string{codegen::quantization_clipping_name(comparison.second.clipping)});
                comparison_row("Resolution",
                               format_decimal(comparison.first.resolution),
                               format_decimal(comparison.second.resolution),
                               format_decimal_delta(comparison.resolution_delta));
                comparison_row("Maximum rounding error",
                               format_decimal(comparison.first.maximum_rounding_error),
                               format_decimal(comparison.second.maximum_rounding_error),
                               format_decimal_delta(comparison.maximum_rounding_error_delta));
                comparison_row("Minimum endpoint exact",
                               comparison.first.minimum_endpoint_exact ? "Yes" : "No",
                               comparison.second.minimum_endpoint_exact ? "Yes" : "No");
                comparison_row("Maximum endpoint exact",
                               comparison.first.maximum_endpoint_exact ? "Yes" : "No",
                               comparison.second.maximum_endpoint_exact ? "Yes" : "No");
                comparison_row("Element count",
                               std::to_string(comparison.element_count),
                               std::to_string(comparison.element_count));
                comparison_row("Encoded payload bits",
                               detail::format_number(comparison.first_total_encoded_bits),
                               detail::format_number(comparison.second_total_encoded_bits),
                               detail::format_delta_number(comparison.total_encoded_bit_delta));
                comparison_row("Allocated storage", "Unspecified", "Unspecified", "Unknown");
                ImGui::EndTable();
            }
            ImGui::TextDisabled(
                "Payload bits are exact encoding facts. Allocated bytes, stride, cache lines, and "
                "pages require a container placement policy and are intentionally not inferred.");
            draw_diagnostics(comparison.diagnostics);

            if (ImGui::Button("Select B representation") && comparison_type.has_value()) {
                selected_type_ = comparison_type;
                selected_field_.clear();
                graph_focus_selected_ = true;
                refresh_analysis();
            }
            ImGui::SameLine();
            if (ImGui::Button("Select semantic source")) {
                selected_type_ = comparison.first.source_type;
                selected_field_.clear();
                graph_focus_selected_ = true;
                refresh_analysis();
            }
            ImGui::End();
            return;
        }

        auto const selected_optional_source{optional_source(selected_node.definition)};
        if (selected_optional_source.has_value()) {
            auto comparison_type{optional_comparison_type_.has_value()
                                     ? workspace_.types().find(*optional_comparison_type_)
                                     : std::nullopt};
            auto const comparison_label{comparison_type.has_value()
                                            ? type_label(workspace_.types().type(*comparison_type))
                                            : std::string{"No same-source representation"}};
            ImGui::Text("A: %s", type_label(selected_node).c_str());
            ImGui::SetNextItemWidth(-1.0F);
            if (ImGui::BeginCombo("B##optional-comparison", comparison_label.c_str())) {
                auto const types{workspace_.types().types()};
                for (std::size_t index{}; index < types.size(); ++index) {
                    auto const candidate{TypeId{static_cast<std::uint32_t>(index)}};
                    auto const candidate_source{optional_source(types[index].definition)};
                    if (candidate == *selected_type_ ||
                        candidate_source != selected_optional_source) {
                        continue;
                    }
                    auto const is_selected{comparison_type == candidate};
                    auto const label{type_label(types[index])};
                    if (ImGui::Selectable(label.c_str(), is_selected)) {
                        optional_comparison_type_ = types[index].identity;
                        refresh_analysis();
                        comparison_type = candidate;
                    }
                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            if (!optional_encoding_comparison_.has_value()) {
                ImGui::TextDisabled(
                    "Declare another sentinel or presence-bit optional representation of the "
                    "same semantic source to compare encoding consequences.");
                ImGui::End();
                return;
            }

            ImGui::SeparatorText("Analysis scale");
            if (draw_element_count()) {
                ImGui::End();
                return;
            }

            auto const& comparison{*optional_encoding_comparison_};
            auto const& second_node{workspace_.types().type(comparison.second.type)};
            if (ImGui::BeginTable("optional-encoding-comparison",
                                  4,
                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_Resizable)) {
                ImGui::TableSetupColumn("Fact");
                ImGui::TableSetupColumn(selected_node.identity.name.c_str());
                ImGui::TableSetupColumn(second_node.identity.name.c_str());
                ImGui::TableSetupColumn("Difference (B - A)");
                ImGui::TableHeadersRow();
                comparison_row(
                    "Semantic source",
                    workspace_.types().type(comparison.first.source_type).identity.name,
                    workspace_.types().type(comparison.second.source_type).identity.name);
                comparison_row("Encoding policy",
                               optional_encoding_label(comparison.first.kind),
                               optional_encoding_label(comparison.second.kind));
                comparison_row("Absence sentinel",
                               comparison.first.absence_sentinel_name.value_or("None"),
                               comparison.second.absence_sentinel_name.value_or("None"));
                comparison_row("Present values",
                               detail::format_number(comparison.first.present_value_count),
                               detail::format_number(comparison.second.present_value_count));
                comparison_row("Canonical absence states",
                               std::to_string(comparison.first.canonical_absence_state_count),
                               std::to_string(comparison.second.canonical_absence_state_count));
                comparison_row("Source sentinel codes",
                               std::to_string(comparison.first.source_sentinel_code_count),
                               std::to_string(comparison.second.source_sentinel_code_count));
                comparison_row(
                    "Sentinels used for absence",
                    std::to_string(comparison.first.source_sentinel_codes_used_for_absence),
                    std::to_string(comparison.second.source_sentinel_codes_used_for_absence));
                comparison_row(
                    "Remaining source sentinels",
                    std::to_string(comparison.first.remaining_source_sentinel_code_count),
                    std::to_string(comparison.second.remaining_source_sentinel_code_count));
                comparison_row(
                    "Source unused payload codes",
                    detail::format_number(comparison.first.source_unused_payload_codes),
                    detail::format_number(comparison.second.source_unused_payload_codes));
                comparison_row(
                    "Noncanonical absent patterns",
                    detail::format_number(comparison.first.noncanonical_absence_patterns),
                    detail::format_number(comparison.second.noncanonical_absence_patterns));
                comparison_row("Encoded width",
                               std::to_string(comparison.first.encoded_storage_bits) + " bits",
                               std::to_string(comparison.second.encoded_storage_bits) + " bits",
                               detail::format_delta_number(comparison.encoded_storage_bit_delta));
                comparison_row("Element count",
                               std::to_string(comparison.element_count),
                               std::to_string(comparison.element_count));
                comparison_row("Encoded payload bits",
                               detail::format_number(comparison.first.total_encoded_bits),
                               detail::format_number(comparison.second.total_encoded_bits),
                               detail::format_delta_number(comparison.total_encoded_bit_delta));
                comparison_row("Allocated storage", "Unspecified", "Unspecified", "Unknown");
                ImGui::EndTable();
            }
            ImGui::TextDisabled(
                "Code-space roles and payload bits are exact. Allocated bytes, bit order, stride, "
                "cache lines, and pages require a placement policy and remain unspecified.");
            draw_diagnostics(comparison.diagnostics);

            if (ImGui::Button("Select B representation") && comparison_type.has_value()) {
                selected_type_ = comparison_type;
                selected_field_.clear();
                graph_focus_selected_ = true;
                refresh_analysis();
            }
            ImGui::SameLine();
            if (ImGui::Button("Select semantic source")) {
                selected_type_ = comparison.first.source_type;
                selected_field_.clear();
                graph_focus_selected_ = true;
                refresh_analysis();
            }
            ImGui::End();
            return;
        }

        auto const* selected_varint{std::get_if<IntegerVarintType>(&selected_node.definition)};
        if (selected_varint != nullptr) {
            auto comparison_type{varint_comparison_type_.has_value()
                                     ? workspace_.types().find(*varint_comparison_type_)
                                     : std::nullopt};
            auto const comparison_label{comparison_type.has_value()
                                            ? type_label(workspace_.types().type(*comparison_type))
                                            : std::string{"No same-source representation"}};
            ImGui::Text("A: %s", type_label(selected_node).c_str());
            ImGui::SetNextItemWidth(-1.0F);
            if (ImGui::BeginCombo("B##varint-comparison", comparison_label.c_str())) {
                auto const types{workspace_.types().types()};
                for (std::size_t index{}; index < types.size(); ++index) {
                    auto const candidate{TypeId{static_cast<std::uint32_t>(index)}};
                    auto const* varint{std::get_if<IntegerVarintType>(&types[index].definition)};
                    if (candidate == *selected_type_ || varint == nullptr ||
                        varint->source.type != selected_varint->source.type) {
                        continue;
                    }
                    auto const is_selected{comparison_type == candidate};
                    auto const label{type_label(types[index])};
                    if (ImGui::Selectable(label.c_str(), is_selected)) {
                        varint_comparison_type_ = types[index].identity;
                        refresh_analysis();
                        comparison_type = candidate;
                    }
                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            if (!integer_varint_comparison_.has_value()) {
                ImGui::TextDisabled(
                    "Declare another integer varint representation of the same semantic source "
                    "to compare encoded-size bounds.");
                ImGui::End();
                return;
            }

            ImGui::SeparatorText("Analysis scale");
            if (draw_element_count()) {
                ImGui::End();
                return;
            }

            auto const& comparison{*integer_varint_comparison_};
            auto const& second_node{workspace_.types().type(comparison.second.type)};
            auto const& source_node{workspace_.types().type(comparison.first.source_type)};
            std::optional<IntegerVarintDistributionComparison> distribution_comparison;
            auto invalid_distribution_literals{std::size_t{}};
            if (auto const rows{varint_distributions_.find(source_node.identity)};
                rows != varint_distributions_.end() && !rows->second.empty()) {
                std::vector<IntegerVarintDistributionEntry> entries;
                entries.reserve(rows->second.size());
                for (auto const& row : rows->second) {
                    if (auto const value{detail::parse_packed_integer(row.value.data())}) {
                        entries.push_back({.value = *value, .weight = row.weight});
                    } else {
                        ++invalid_distribution_literals;
                    }
                }
                distribution_comparison =
                    Analyzer::compare_integer_varint_distribution(workspace_.types(),
                                                                  comparison.first.type,
                                                                  comparison.second.type,
                                                                  entries,
                                                                  workspace_.element_count());
            }
            if (ImGui::BeginTable("integer-varint-comparison",
                                  4,
                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_Resizable)) {
                ImGui::TableSetupColumn("Fact");
                ImGui::TableSetupColumn(selected_node.identity.name.c_str());
                ImGui::TableSetupColumn(second_node.identity.name.c_str());
                ImGui::TableSetupColumn("Difference (B - A)");
                ImGui::TableHeadersRow();
                comparison_row(
                    "Semantic source",
                    workspace_.types().type(comparison.first.source_type).identity.name,
                    workspace_.types().type(comparison.second.source_type).identity.name);
                comparison_row(
                    "Encoding",
                    std::string{codegen::integer_varint_encoding_name(comparison.first.encoding)},
                    std::string{codegen::integer_varint_encoding_name(comparison.second.encoding)});
                comparison_row("Minimum bytes / value",
                               std::to_string(comparison.first.minimum_encoded_bytes),
                               std::to_string(comparison.second.minimum_encoded_bytes),
                               detail::format_delta_bytes(comparison.minimum_encoded_byte_delta));
                comparison_row("Maximum bytes / value",
                               std::to_string(comparison.first.maximum_encoded_bytes),
                               std::to_string(comparison.second.maximum_encoded_bytes),
                               detail::format_delta_bytes(comparison.maximum_encoded_byte_delta));
                comparison_row("Element count",
                               std::to_string(comparison.first.element_count),
                               std::to_string(comparison.second.element_count));
                comparison_row("Minimum total",
                               detail::format_bytes(comparison.first.minimum_total_bytes),
                               detail::format_bytes(comparison.second.minimum_total_bytes),
                               detail::format_delta_bytes(comparison.minimum_total_byte_delta));
                comparison_row("Maximum total",
                               detail::format_bytes(comparison.first.maximum_total_bytes),
                               detail::format_bytes(comparison.second.maximum_total_bytes),
                               detail::format_delta_bytes(comparison.maximum_total_byte_delta));
                if (distribution_comparison.has_value()) {
                    comparison_row(
                        "Distribution sample bytes",
                        detail::format_bytes(distribution_comparison->first.total_encoded_bytes),
                        detail::format_bytes(distribution_comparison->second.total_encoded_bytes),
                        detail::format_delta_bytes(
                            distribution_comparison->total_encoded_byte_delta));
                    comparison_row("Expected bytes / value",
                                   format_optional_decimal(
                                       distribution_comparison->first.expected_bytes_per_value),
                                   format_optional_decimal(
                                       distribution_comparison->second.expected_bytes_per_value),
                                   format_decimal_delta(
                                       distribution_comparison->expected_bytes_per_value_delta));
                    comparison_row("Distribution estimate at count",
                                   format_optional_decimal(
                                       distribution_comparison->first.expected_selected_bytes),
                                   format_optional_decimal(
                                       distribution_comparison->second.expected_selected_bytes),
                                   format_decimal_delta(
                                       distribution_comparison->expected_selected_bytes_delta));
                } else {
                    comparison_row("Expected bytes / value", "Unknown", "Unknown", "Unknown");
                    comparison_row(
                        "Distribution estimate at count", "Unknown", "Unknown", "Unknown");
                }
                ImGui::EndTable();
            }
            if (distribution_comparison.has_value() &&
                !distribution_comparison->first.entries.empty() &&
                ImGui::BeginTable("integer-varint-entry-comparison",
                                  5,
                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_Resizable)) {
                ImGui::TableSetupColumn("Value");
                ImGui::TableSetupColumn("Weight");
                ImGui::TableSetupColumn((selected_node.identity.name + " bytes").c_str());
                ImGui::TableSetupColumn((second_node.identity.name + " bytes").c_str());
                ImGui::TableSetupColumn("Difference (B - A)");
                ImGui::TableHeadersRow();
                auto const entry_count{std::min(distribution_comparison->first.entries.size(),
                                                distribution_comparison->second.entries.size())};
                for (std::size_t index{}; index < entry_count; ++index) {
                    auto const& first_entry{distribution_comparison->first.entries[index]};
                    auto const& second_entry{distribution_comparison->second.entries[index]};
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(
                        codegen::format_packed_integer(first_entry.value).c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%llu", static_cast<unsigned long long>(first_entry.weight));
                    ImGui::TableNextColumn();
                    ImGui::Text("%u", first_entry.encoded_bytes);
                    ImGui::TableNextColumn();
                    ImGui::Text("%u", second_entry.encoded_bytes);
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(
                        detail::format_delta_number(
                            numeric_delta(first_entry.encoded_bytes, second_entry.encoded_bytes))
                            .c_str());
                }
                ImGui::EndTable();
            }
            ImGui::TextDisabled("Bounds are exact for the declared domain. Expected rows use the "
                                "same explicit session distribution for both encodings.");
            if (invalid_distribution_literals != 0) {
                ImGui::TextColored(
                    detail::diagnostic_color(DiagnosticSeverity::error),
                    "%llu distribution row%s contain an invalid integer literal and were skipped.",
                    static_cast<unsigned long long>(invalid_distribution_literals),
                    invalid_distribution_literals == 1 ? "" : "s");
            }
            draw_diagnostics(comparison.diagnostics);
            if (distribution_comparison.has_value()) {
                draw_diagnostics(distribution_comparison->diagnostics);
            }

            if (ImGui::Button("Select B representation") && comparison_type.has_value()) {
                selected_type_ = comparison_type;
                selected_field_.clear();
                graph_focus_selected_ = true;
                refresh_analysis();
            }
            ImGui::SameLine();
            if (ImGui::Button("Select semantic source")) {
                selected_type_ = comparison.first.source_type;
                selected_field_.clear();
                graph_focus_selected_ = true;
                refresh_analysis();
            }
            ImGui::End();
            return;
        }
    }

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
                comparison_row((baseline.name + " complete elements / cache line").c_str(),
                               detail::format_number(baseline.elements_per_cache_line),
                               detail::format_number(active->elements_per_cache_line));
            }
            ImGui::EndTable();
        }
        if (soa_access_comparison_.has_value()) {
            auto const& access{*soa_access_comparison_};
            std::string column_names;
            for (auto const& column_name : access.column_names) {
                if (!column_names.empty()) {
                    column_names += ", ";
                }
                column_names += column_name;
            }
            ImGui::SeparatorText("Selected-column workload");
            ImGui::Text("Elements: %llu", static_cast<unsigned long long>(access.element_count));
            ImGui::TextWrapped("Columns: %s", column_names.c_str());
            if (ImGui::BeginTable("soa-access-comparison",
                                  4,
                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_Resizable)) {
                ImGui::TableSetupColumn("Fact");
                ImGui::TableSetupColumn(comparison_a.name.c_str());
                ImGui::TableSetupColumn(comparison_b.name.c_str());
                ImGui::TableSetupColumn("Difference");
                ImGui::TableHeadersRow();
                comparison_row("Useful selected payload",
                               detail::format_bytes(access.first.useful_bytes),
                               detail::format_bytes(access.second.useful_bytes),
                               detail::format_delta_bytes(access.useful_byte_delta));
                comparison_row("Complete logical payload at workload count",
                               detail::format_bytes(access.first_full_logical_payload_bytes),
                               detail::format_bytes(access.second_full_logical_payload_bytes),
                               detail::format_delta_bytes(access.full_logical_payload_delta));
                comparison_row("Unselected payload at workload count",
                               detail::format_bytes(access.first_unselected_payload_bytes),
                               detail::format_bytes(access.second_unselected_payload_bytes),
                               detail::format_delta_bytes(access.unselected_payload_delta));
                comparison_row("Minimum cache lines",
                               detail::format_number(access.first.cache_lines),
                               detail::format_number(access.second.cache_lines),
                               detail::format_delta_number(access.cache_line_delta));
                comparison_row("Minimum cache-line footprint",
                               detail::format_bytes(access.first.cache_bytes),
                               detail::format_bytes(access.second.cache_bytes),
                               detail::format_delta_bytes(access.cache_byte_delta));
                comparison_row("Non-payload bytes in minimum cache footprint",
                               detail::format_bytes(access.first_non_payload_cache_bytes),
                               detail::format_bytes(access.second_non_payload_cache_bytes),
                               detail::format_delta_bytes(access.non_payload_cache_byte_delta));
                comparison_row("Minimum pages",
                               detail::format_number(access.first.pages),
                               detail::format_number(access.second.pages),
                               detail::format_delta_number(access.page_delta));
                comparison_row("Minimum page footprint",
                               detail::format_bytes(access.first.page_bytes),
                               detail::format_bytes(access.second.page_bytes),
                               detail::format_delta_bytes(access.page_byte_delta));
                comparison_row("Non-payload bytes in minimum page footprint",
                               detail::format_bytes(access.first_non_payload_page_bytes),
                               detail::format_bytes(access.second_non_payload_page_bytes),
                               detail::format_delta_bytes(access.non_payload_page_byte_delta));
                comparison_row("Allocated payload at capacity",
                               detail::format_bytes(access.first_allocated_capacity_payload_bytes),
                               detail::format_bytes(access.second_allocated_capacity_payload_bytes),
                               detail::format_delta_bytes(access.allocated_capacity_payload_delta));
                comparison_row("Unused allocated capacity payload",
                               detail::format_bytes(access.first_capacity_slack_payload_bytes),
                               detail::format_bytes(access.second_capacity_slack_payload_bytes),
                               detail::format_delta_bytes(access.capacity_slack_payload_delta));
                ImGui::EndTable();
            }
            if (!access.columns.empty() && ImGui::TreeNode("Per-column contributions")) {
                for (auto const& column : access.columns) {
                    ImGui::PushID(column.name.c_str());
                    ImGui::SeparatorText(column.name.c_str());
                    if (ImGui::BeginTable("soa-access-column-comparison",
                                          4,
                                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                              ImGuiTableFlags_Resizable)) {
                        ImGui::TableSetupColumn("Fact");
                        ImGui::TableSetupColumn(comparison_a.name.c_str());
                        ImGui::TableSetupColumn(comparison_b.name.c_str());
                        ImGui::TableSetupColumn("Difference");
                        ImGui::TableHeadersRow();
                        comparison_row("Physical type",
                                       column.first.physical_type,
                                       column.second.physical_type);
                        comparison_row("Element bytes",
                                       detail::format_bytes(column.first.element_bytes),
                                       detail::format_bytes(column.second.element_bytes),
                                       detail::format_delta_bytes(column.element_byte_delta));
                        comparison_row("Useful workload payload",
                                       detail::format_bytes(column.first.useful_bytes),
                                       detail::format_bytes(column.second.useful_bytes),
                                       detail::format_delta_bytes(column.useful_byte_delta));
                        comparison_row("Minimum cache lines",
                                       detail::format_number(column.first.minimum_cache_lines),
                                       detail::format_number(column.second.minimum_cache_lines),
                                       detail::format_delta_number(column.cache_line_delta));
                        comparison_row("Minimum cache-line footprint",
                                       detail::format_bytes(column.first.minimum_cache_bytes),
                                       detail::format_bytes(column.second.minimum_cache_bytes),
                                       detail::format_delta_bytes(column.cache_byte_delta));
                        comparison_row(
                            "Non-payload bytes in minimum cache footprint",
                            detail::format_bytes(column.first.non_payload_cache_bytes),
                            detail::format_bytes(column.second.non_payload_cache_bytes),
                            detail::format_delta_bytes(column.non_payload_cache_byte_delta));
                        comparison_row("Minimum pages",
                                       detail::format_number(column.first.minimum_pages),
                                       detail::format_number(column.second.minimum_pages),
                                       detail::format_delta_number(column.page_delta));
                        comparison_row("Minimum page footprint",
                                       detail::format_bytes(column.first.minimum_page_bytes),
                                       detail::format_bytes(column.second.minimum_page_bytes),
                                       detail::format_delta_bytes(column.page_byte_delta));
                        comparison_row(
                            "Non-payload bytes in minimum page footprint",
                            detail::format_bytes(column.first.non_payload_page_bytes),
                            detail::format_bytes(column.second.non_payload_page_bytes),
                            detail::format_delta_bytes(column.non_payload_page_byte_delta));
                        comparison_row(
                            "Allocated payload at capacity",
                            detail::format_bytes(column.first.allocated_capacity_payload_bytes),
                            detail::format_bytes(column.second.allocated_capacity_payload_bytes),
                            detail::format_delta_bytes(column.allocated_capacity_payload_delta));
                        comparison_row(
                            "Unused allocated capacity payload",
                            detail::format_bytes(column.first.capacity_slack_payload_bytes),
                            detail::format_bytes(column.second.capacity_slack_payload_bytes),
                            detail::format_delta_bytes(column.capacity_slack_payload_delta));
                        ImGui::EndTable();
                    }
                    ImGui::PopID();
                }
                ImGui::TreePop();
            }
            ImGui::TextDisabled(
                "Both variants use the same explicit session workload. Cache-line/page figures "
                "are minimum footprints across separate column allocations, not measured traffic "
                "or performance.");
            draw_diagnostics(access.diagnostics);
        }
        ImGui::TextDisabled("Cache-line counts are minimum payload coverage, not allocator traffic "
                            "or a performance estimate.");
        draw_diagnostics(comparison_b_soa_->diagnostics);
    }
    ImGui::End();
}

} // namespace ioj::layout_planner
