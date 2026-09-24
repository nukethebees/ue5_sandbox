#include "planner_ui_comparison_common.hpp"

namespace ioj::layout_planner {

void PlannerUi::draw_comparison_panel() {
    if (!comparison_view_open_) {
        return;
    }
    auto const was_open{comparison_view_open_};
    ImGui::Begin("Comparison", &comparison_view_open_, ImGuiWindowFlags_HorizontalScrollbar);
    persist_view_visibility(was_open, comparison_view_open_);
    ImGui::TextDisabled("ABI profile: %s", analysis_session_.primary_abi().name().c_str());

    if (analysis_session_.inputs.selection.type.has_value()) {
        auto const& selected_node{analysis_session_.inputs.workspace.types().type(
            *analysis_session_.inputs.selection.type)};
        if (std::holds_alternative<EnumType>(selected_node.definition)) {
            draw_enum_target_comparison();
            ImGui::End();
            return;
        }

        if (std::holds_alternative<RecordType>(selected_node.definition)) {
            ImGui::TextWrapped(
                "Compare one semantic record under two explicit physical target profiles. Target "
                "A uses the shared Target Profile; target B is session-only.");
            if (draw_comparison_target_profile_picker()) {
                refresh_analysis();
            }

            if (detail::section("Analysis scale")) {
                if (draw_element_count()) {
                    ImGui::End();
                    return;
                }
                if (!analysis_session_.results().record_target_comparison.has_value()) {
                    ImGui::TextDisabled("No record target comparison is available.");
                    ImGui::End();
                    return;
                }

                auto const& comparison{*analysis_session_.results().record_target_comparison};
                auto const& first{comparison.first};
                auto const& second{comparison.second};
                auto const& first_aggregate{first.aggregate};
                auto const& second_aggregate{second.aggregate};
                if (ImGui::BeginTable("record-target-comparison",
                                      4,
                                      ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                          ImGuiTableFlags_Resizable)) {
                    ImGui::TableSetupColumn("Physical fact");
                    ImGui::TableSetupColumn(analysis_session_.primary_abi().name().c_str());
                    ImGui::TableSetupColumn(analysis_session_.comparison_abi().name().c_str());
                    ImGui::TableSetupColumn("Difference (B - A)");
                    ImGui::TableHeadersRow();
                    comparison_row("Profile",
                                   analysis_session_.primary_abi().name(),
                                   analysis_session_.comparison_abi().name());
                    comparison_row(
                        "Platform",
                        analysis_session_.primary_abi().identity().platform.value_or("Unknown"),
                        analysis_session_.comparison_abi().identity().platform.value_or("Unknown"));
                    comparison_row(
                        "Architecture",
                        analysis_session_.primary_abi().identity().architecture.value_or("Unknown"),
                        analysis_session_.comparison_abi().identity().architecture.value_or(
                            "Unknown"));
                    comparison_row(
                        "ABI",
                        analysis_session_.primary_abi().identity().abi.value_or("Unknown"),
                        analysis_session_.comparison_abi().identity().abi.value_or("Unknown"));
                    comparison_row(
                        "Compiler",
                        analysis_session_.primary_abi().identity().compiler.value_or("Unknown"),
                        analysis_session_.comparison_abi().identity().compiler.value_or("Unknown"));
                    comparison_row(
                        "Build configuration",
                        analysis_session_.primary_abi().identity().build_configuration.value_or(
                            "Unknown"),
                        analysis_session_.comparison_abi().identity().build_configuration.value_or(
                            "Unknown"));
                    comparison_row("Element count",
                                   std::to_string(first_aggregate.element_count),
                                   std::to_string(second_aggregate.element_count));
                    comparison_row("Member extents per object",
                                   detail::format_bytes(first.payload_bytes),
                                   detail::format_bytes(second.payload_bytes),
                                   detail::format_delta_bytes(comparison.payload_delta));
                    comparison_row("Padding per object",
                                   detail::format_bytes(first.internal_padding_bytes),
                                   detail::format_bytes(second.internal_padding_bytes),
                                   detail::format_delta_bytes(comparison.internal_padding_delta));
                    comparison_row("Tail padding per object",
                                   detail::format_bytes(first.tail_padding_bytes),
                                   detail::format_bytes(second.tail_padding_bytes),
                                   detail::format_delta_bytes(comparison.tail_padding_delta));
                    comparison_row("Object size",
                                   detail::format_bytes(first.size_bytes),
                                   detail::format_bytes(second.size_bytes),
                                   detail::format_delta_bytes(comparison.size_delta));
                    comparison_row("Object alignment",
                                   detail::format_bytes(first.alignment_bytes),
                                   detail::format_bytes(second.alignment_bytes),
                                   detail::format_delta_bytes(comparison.alignment_delta));
                    comparison_row("Aggregate storage",
                                   detail::format_bytes(first_aggregate.total_storage_bytes),
                                   detail::format_bytes(second_aggregate.total_storage_bytes),
                                   detail::format_delta_bytes(comparison.total_storage_delta));
                    comparison_row("Aggregate member extents",
                                   detail::format_bytes(first_aggregate.total_payload_bytes),
                                   detail::format_bytes(second_aggregate.total_payload_bytes),
                                   detail::format_delta_bytes(comparison.total_payload_delta));
                    comparison_row(
                        "Aggregate padding",
                        detail::format_bytes(first_aggregate.total_internal_padding_bytes),
                        detail::format_bytes(second_aggregate.total_internal_padding_bytes),
                        detail::format_delta_bytes(comparison.total_internal_padding_delta));
                    comparison_row("Aggregate tail padding",
                                   detail::format_bytes(first_aggregate.total_tail_padding_bytes),
                                   detail::format_bytes(second_aggregate.total_tail_padding_bytes),
                                   detail::format_delta_bytes(comparison.total_tail_padding_delta));
                    comparison_row("Aggregate total padding",
                                   detail::format_bytes(first_aggregate.total_padding_bytes),
                                   detail::format_bytes(second_aggregate.total_padding_bytes),
                                   detail::format_delta_bytes(comparison.total_padding_delta));
                    comparison_row("Cache-line size",
                                   detail::format_bytes(first_aggregate.cache_line_bytes),
                                   detail::format_bytes(second_aggregate.cache_line_bytes),
                                   detail::format_delta_bytes(comparison.cache_line_size_delta));
                    comparison_row(
                        "Minimum cache lines",
                        detail::format_number(first_aggregate.minimum_cache_lines),
                        detail::format_number(second_aggregate.minimum_cache_lines),
                        detail::format_delta_number(comparison.minimum_cache_line_delta));
                    comparison_row(
                        "Complete elements per cache line",
                        detail::format_number(first_aggregate.complete_elements_per_cache_line),
                        detail::format_number(second_aggregate.complete_elements_per_cache_line),
                        detail::format_delta_number(
                            comparison.complete_elements_per_cache_line_delta));
                    comparison_row(
                        "Cache-line-straddling elements",
                        detail::format_number(first_aggregate.cache_line_straddling_elements),
                        detail::format_number(second_aggregate.cache_line_straddling_elements),
                        detail::format_delta_number(comparison.cache_line_straddling_delta));
                    comparison_row("Page size",
                                   detail::format_bytes(first_aggregate.page_bytes),
                                   detail::format_bytes(second_aggregate.page_bytes),
                                   detail::format_delta_bytes(comparison.page_size_delta));
                    comparison_row("Minimum pages",
                                   detail::format_number(first_aggregate.minimum_pages),
                                   detail::format_number(second_aggregate.minimum_pages),
                                   detail::format_delta_number(comparison.minimum_page_delta));
                    comparison_row(
                        "Complete elements per page",
                        detail::format_number(first_aggregate.complete_elements_per_page),
                        detail::format_number(second_aggregate.complete_elements_per_page),
                        detail::format_delta_number(comparison.complete_elements_per_page_delta));
                    comparison_row("Page-straddling elements",
                                   detail::format_number(first_aggregate.page_straddling_elements),
                                   detail::format_number(second_aggregate.page_straddling_elements),
                                   detail::format_delta_number(comparison.page_straddling_delta));
                    comparison_row(
                        "Fits L1 data cache",
                        detail::format_fit(first_aggregate.cache_capacity.fits_l1_data),
                        detail::format_fit(second_aggregate.cache_capacity.fits_l1_data));
                    comparison_row("Fits L2 cache",
                                   detail::format_fit(first_aggregate.cache_capacity.fits_l2),
                                   detail::format_fit(second_aggregate.cache_capacity.fits_l2));
                    comparison_row("Fits L3 cache",
                                   detail::format_fit(first_aggregate.cache_capacity.fits_l3),
                                   detail::format_fit(second_aggregate.cache_capacity.fits_l3));
                    ImGui::EndTable();
                }

                if (ImGui::TreeNodeEx("Member layout", ImGuiTreeNodeFlags_SpanAvailWidth)) {
                    for (auto const& member : comparison.members) {
                        ImGui::PushID(member.name.c_str());
                        if (detail::section(member.name.c_str())) {
                            if (ImGui::BeginTable("member-target-comparison",
                                                  4,
                                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                                      ImGuiTableFlags_Resizable)) {
                                ImGui::TableSetupColumn("Physical fact");
                                ImGui::TableSetupColumn(
                                    analysis_session_.primary_abi().name().c_str());
                                ImGui::TableSetupColumn(
                                    analysis_session_.comparison_abi().name().c_str());
                                ImGui::TableSetupColumn("Difference (B - A)");
                                ImGui::TableHeadersRow();
                                comparison_row(
                                    "Semantic type",
                                    type_label(analysis_session_.inputs.workspace.types().type(
                                        member.first.semantic_type)),
                                    type_label(analysis_session_.inputs.workspace.types().type(
                                        member.second.semantic_type)));
                                comparison_row("Element count",
                                               std::to_string(member.first.element_count),
                                               std::to_string(member.second.element_count));
                                comparison_row(
                                    "Element size",
                                    detail::format_bytes(member.first.element_facts.transform(
                                        [](TypeFacts const& facts) { return facts.size_bytes; })),
                                    detail::format_bytes(member.second.element_facts.transform(
                                        [](TypeFacts const& facts) { return facts.size_bytes; })),
                                    detail::format_delta_bytes(member.element_size_delta));
                                comparison_row(
                                    "Element alignment",
                                    detail::format_bytes(member.first.element_facts.transform(
                                        [](TypeFacts const& facts) {
                                            return facts.alignment_bytes;
                                        })),
                                    detail::format_bytes(member.second.element_facts.transform(
                                        [](TypeFacts const& facts) {
                                            return facts.alignment_bytes;
                                        })),
                                    detail::format_delta_bytes(member.element_alignment_delta));
                                comparison_row("Offset",
                                               detail::format_bytes(member.first.offset_bytes),
                                               detail::format_bytes(member.second.offset_bytes),
                                               detail::format_delta_bytes(member.offset_delta));
                                comparison_row("Extent",
                                               detail::format_bytes(member.first.extent_bytes),
                                               detail::format_bytes(member.second.extent_bytes),
                                               detail::format_delta_bytes(member.extent_delta));
                                comparison_row(
                                    "Padding before",
                                    detail::format_bytes(member.first.padding_before_bytes),
                                    detail::format_bytes(member.second.padding_before_bytes),
                                    detail::format_delta_bytes(member.padding_before_delta));
                                ImGui::EndTable();
                            }
                        }
                        ImGui::PopID();
                    }
                    ImGui::TreePop();
                }
                if (analysis_session_.results().record_target_access_comparison.has_value()) {
                    auto const& access{
                        *analysis_session_.results().record_target_access_comparison};
                    auto const& first_access{access.first};
                    auto const& second_access{access.second};
                    std::string member_names;
                    for (auto const& intent : first_access.accesses) {
                        if (!member_names.empty()) {
                            member_names += ", ";
                        }
                        member_names += intent.name;
                        member_names += " (";
                        member_names += detail::access_operation_name(intent.operation);
                        member_names += ')';
                    }
                    if (detail::section("Selected-member workload")) {
                        ImGui::TextWrapped("Members: %s", member_names.c_str());
                        ImGui::Text("Accesses / element: %llu",
                                    static_cast<unsigned long long>(first_access.multiplicity));
                        if (ImGui::BeginTable("record-target-access-comparison",
                                              4,
                                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                                  ImGuiTableFlags_Resizable)) {
                            ImGui::TableSetupColumn("Workload fact");
                            ImGui::TableSetupColumn(analysis_session_.primary_abi().name().c_str());
                            ImGui::TableSetupColumn(
                                analysis_session_.comparison_abi().name().c_str());
                            ImGui::TableSetupColumn("Difference (B - A)");
                            ImGui::TableHeadersRow();
                            comparison_row("Element count",
                                           std::to_string(first_access.element_count),
                                           std::to_string(second_access.element_count));
                            comparison_row("Useful selected bytes",
                                           detail::format_bytes(first_access.useful_bytes),
                                           detail::format_bytes(second_access.useful_bytes),
                                           detail::format_delta_bytes(access.useful_byte_delta));
                            comparison_row(
                                "Classified read useful bytes",
                                detail::format_bytes(first_access.read_useful_bytes),
                                detail::format_bytes(second_access.read_useful_bytes),
                                detail::format_delta_bytes(access.read_useful_byte_delta));
                            comparison_row(
                                "Classified write useful bytes",
                                detail::format_bytes(first_access.write_useful_bytes),
                                detail::format_bytes(second_access.write_useful_bytes),
                                detail::format_delta_bytes(access.write_useful_byte_delta));
                            comparison_row(
                                "Logical read useful bytes",
                                detail::format_bytes(first_access.logical_read_useful_bytes),
                                detail::format_bytes(second_access.logical_read_useful_bytes),
                                detail::format_delta_bytes(access.logical_read_useful_byte_delta));
                            comparison_row(
                                "Logical write useful bytes",
                                detail::format_bytes(first_access.logical_write_useful_bytes),
                                detail::format_bytes(second_access.logical_write_useful_bytes),
                                detail::format_delta_bytes(access.logical_write_useful_byte_delta));
                            comparison_row(
                                "Enclosing AoS footprint",
                                detail::format_bytes(first_access.object_footprint_bytes),
                                detail::format_bytes(second_access.object_footprint_bytes),
                                detail::format_delta_bytes(access.object_footprint_byte_delta));
                            comparison_row(
                                "Cache-line size",
                                detail::format_bytes(first_access.cache_line_bytes),
                                detail::format_bytes(second_access.cache_line_bytes),
                                detail::format_delta_bytes(access.cache_line_size_delta));
                            comparison_row("Distinct cache lines touched",
                                           detail::format_number(first_access.cache_lines_touched),
                                           detail::format_number(second_access.cache_lines_touched),
                                           detail::format_delta_number(access.cache_line_delta));
                            comparison_row("Bytes in touched cache lines",
                                           detail::format_bytes(first_access.cache_bytes_touched),
                                           detail::format_bytes(second_access.cache_bytes_touched),
                                           detail::format_delta_bytes(access.cache_byte_delta));
                            comparison_row(
                                "Read cache lines covered",
                                detail::format_number(first_access.read_cache_lines_touched),
                                detail::format_number(second_access.read_cache_lines_touched),
                                detail::format_delta_number(access.read_cache_line_delta));
                            comparison_row(
                                "Read cache-line address coverage",
                                detail::format_bytes(first_access.read_cache_bytes_touched),
                                detail::format_bytes(second_access.read_cache_bytes_touched),
                                detail::format_delta_bytes(access.read_cache_byte_delta));
                            comparison_row(
                                "Write cache lines covered",
                                detail::format_number(first_access.write_cache_lines_touched),
                                detail::format_number(second_access.write_cache_lines_touched),
                                detail::format_delta_number(access.write_cache_line_delta));
                            comparison_row(
                                "Write cache-line address coverage",
                                detail::format_bytes(first_access.write_cache_bytes_touched),
                                detail::format_bytes(second_access.write_cache_bytes_touched),
                                detail::format_delta_bytes(access.write_cache_byte_delta));
                            comparison_row(
                                "Non-selected bytes in touched cache lines",
                                detail::format_bytes(first_access.non_selected_cache_bytes),
                                detail::format_bytes(second_access.non_selected_cache_bytes),
                                detail::format_delta_bytes(access.non_selected_cache_byte_delta));
                            comparison_row(
                                "Exact footprint <= L1 data cache",
                                detail::format_fit(
                                    first_access.cache_footprint_capacity.fits_l1_data),
                                detail::format_fit(
                                    second_access.cache_footprint_capacity.fits_l1_data));
                            comparison_row(
                                "Exact footprint <= L2 cache",
                                detail::format_fit(first_access.cache_footprint_capacity.fits_l2),
                                detail::format_fit(second_access.cache_footprint_capacity.fits_l2));
                            comparison_row(
                                "Exact footprint <= L3 cache",
                                detail::format_fit(first_access.cache_footprint_capacity.fits_l3),
                                detail::format_fit(second_access.cache_footprint_capacity.fits_l3));
                            comparison_row("Page size",
                                           detail::format_bytes(first_access.page_bytes),
                                           detail::format_bytes(second_access.page_bytes),
                                           detail::format_delta_bytes(access.page_size_delta));
                            comparison_row("Distinct pages touched",
                                           detail::format_number(first_access.pages_touched),
                                           detail::format_number(second_access.pages_touched),
                                           detail::format_delta_number(access.page_delta));
                            comparison_row("Bytes in touched pages",
                                           detail::format_bytes(first_access.page_bytes_touched),
                                           detail::format_bytes(second_access.page_bytes_touched),
                                           detail::format_delta_bytes(access.page_byte_delta));
                            comparison_row("Read pages covered",
                                           detail::format_number(first_access.read_pages_touched),
                                           detail::format_number(second_access.read_pages_touched),
                                           detail::format_delta_number(access.read_page_delta));
                            comparison_row(
                                "Read page address coverage",
                                detail::format_bytes(first_access.read_page_bytes_touched),
                                detail::format_bytes(second_access.read_page_bytes_touched),
                                detail::format_delta_bytes(access.read_page_byte_delta));
                            comparison_row("Write pages covered",
                                           detail::format_number(first_access.write_pages_touched),
                                           detail::format_number(second_access.write_pages_touched),
                                           detail::format_delta_number(access.write_page_delta));
                            comparison_row(
                                "Write page address coverage",
                                detail::format_bytes(first_access.write_page_bytes_touched),
                                detail::format_bytes(second_access.write_page_bytes_touched),
                                detail::format_delta_bytes(access.write_page_byte_delta));
                            comparison_row(
                                "Non-selected bytes in touched pages",
                                detail::format_bytes(first_access.non_selected_page_bytes),
                                detail::format_bytes(second_access.non_selected_page_bytes),
                                detail::format_delta_bytes(access.non_selected_page_byte_delta));
                            ImGui::EndTable();
                        }
                        ImGui::TextDisabled(
                            "Both targets use the same explicit session workload. "
                            "Unique/read/write "
                            "cache-line and page figures are exact address coverage for an aligned "
                            "contiguous AoS array; multiplicity scales logical useful bytes only. "
                            "This "
                            "does not infer measured traffic, write policy, or performance.");
                        draw_diagnostics(access.diagnostics);
                    }
                }
                ImGui::TextDisabled(
                    "These are deterministic physical layout and footprint facts for the selected "
                    "profiles. They are not a performance prediction.");
                draw_diagnostics(comparison.diagnostics);
                ImGui::End();
                return;
            }
        }

        if (std::holds_alternative<UnionType>(selected_node.definition)) {
            draw_union_target_comparison();
            ImGui::End();
            return;
        }

        if (std::holds_alternative<TaggedUnionType>(selected_node.definition)) {
            draw_tagged_union_target_comparison();
            ImGui::End();
            return;
        }

        if (std::holds_alternative<PackedType>(selected_node.definition)) {
            if (detail::section("Target-profile comparison")) {
                ImGui::TextWrapped(
                    "Compare the active physical variant of this packed value under two explicit "
                    "target profiles. Target A uses the shared Target Profile; target B is "
                    "session-only.");
                ImGui::Text("Held physical variant: %s",
                            analysis_session_.inputs.workspace.active_variant().name.c_str());
                if (draw_comparison_target_profile_picker()) {
                    refresh_analysis();
                }
            }
            if (detail::section("Analysis scale")) {
                if (draw_element_count()) {
                    ImGui::End();
                    return;
                }

                if (analysis_session_.results().packed_target_comparison.has_value()) {
                    auto const& comparison{*analysis_session_.results().packed_target_comparison};
                    auto const& first{comparison.first};
                    auto const& second{comparison.second};
                    auto const& first_aggregate{first.aggregate};
                    auto const& second_aggregate{second.aggregate};
                    auto const first_size{first.storage_facts.transform(
                        [](TypeFacts const& facts) { return facts.size_bytes; })};
                    auto const second_size{second.storage_facts.transform(
                        [](TypeFacts const& facts) { return facts.size_bytes; })};
                    auto const first_alignment{first.storage_facts.transform(
                        [](TypeFacts const& facts) { return facts.alignment_bytes; })};
                    auto const second_alignment{second.storage_facts.transform(
                        [](TypeFacts const& facts) { return facts.alignment_bytes; })};
                    auto const first_value_bits{first.storage_facts.and_then(
                        [](TypeFacts const& facts) { return facts.unsigned_value_bits; })};
                    auto const second_value_bits{second.storage_facts.and_then(
                        [](TypeFacts const& facts) { return facts.unsigned_value_bits; })};
                    auto const known_overflow_bits{
                        [](PackedAnalysis const& analysis) -> std::optional<std::uint64_t> {
                            if (!analysis.storage_bits.has_value() ||
                                !analysis.bits_used.has_value()) {
                                return std::nullopt;
                            }
                            return analysis.overflow_bits.value_or(0);
                        }};
                    if (ImGui::BeginTable("packed-target-comparison",
                                          4,
                                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                              ImGuiTableFlags_Resizable)) {
                        ImGui::TableSetupColumn("Physical fact");
                        ImGui::TableSetupColumn(analysis_session_.primary_abi().name().c_str());
                        ImGui::TableSetupColumn(analysis_session_.comparison_abi().name().c_str());
                        ImGui::TableSetupColumn("Difference (B - A)");
                        ImGui::TableHeadersRow();
                        comparison_row("Profile",
                                       analysis_session_.primary_abi().name(),
                                       analysis_session_.comparison_abi().name());
                        comparison_row(
                            "Architecture",
                            analysis_session_.primary_abi().identity().architecture.value_or(
                                "Unknown"),
                            analysis_session_.comparison_abi().identity().architecture.value_or(
                                "Unknown"));
                        comparison_row("Element count",
                                       std::to_string(first_aggregate.element_count),
                                       std::to_string(second_aggregate.element_count));
                        comparison_row("Storage type", first.storage_type, second.storage_type);
                        comparison_row("Storage size",
                                       detail::format_bytes(first_size),
                                       detail::format_bytes(second_size),
                                       detail::format_delta_bytes(comparison.storage_size_delta));
                        comparison_row(
                            "Storage alignment",
                            detail::format_bytes(first_alignment),
                            detail::format_bytes(second_alignment),
                            detail::format_delta_bytes(comparison.storage_alignment_delta));
                        comparison_row(
                            "Storage value bits",
                            detail::format_number(first_value_bits),
                            detail::format_number(second_value_bits),
                            detail::format_delta_number(comparison.storage_value_bit_delta));
                        comparison_row("Physical storage bits",
                                       detail::format_number(first.storage_bits),
                                       detail::format_number(second.storage_bits),
                                       detail::format_delta_number(comparison.storage_bit_delta));
                        comparison_row("Bits used",
                                       detail::format_number(first.bits_used),
                                       detail::format_number(second.bits_used),
                                       detail::format_delta_number(comparison.bits_used_delta));
                        comparison_row("Payload bits",
                                       detail::format_number(first.payload_bits),
                                       detail::format_number(second.payload_bits),
                                       detail::format_delta_number(comparison.payload_bit_delta));
                        comparison_row("Reserved-region bits",
                                       detail::format_number(first.reserved_bits),
                                       detail::format_number(second.reserved_bits),
                                       detail::format_delta_number(comparison.reserved_bit_delta));
                        comparison_row("Unused physical bits",
                                       detail::format_number(first.unused_bits),
                                       detail::format_number(second.unused_bits),
                                       detail::format_delta_number(comparison.unused_bit_delta));
                        comparison_row("Overflow bits",
                                       detail::format_number(known_overflow_bits(first)),
                                       detail::format_number(known_overflow_bits(second)),
                                       detail::format_delta_number(comparison.overflow_bit_delta));
                        comparison_row("Aggregate storage",
                                       detail::format_bytes(first_aggregate.total_storage_bytes),
                                       detail::format_bytes(second_aggregate.total_storage_bytes),
                                       detail::format_delta_bytes(comparison.total_storage_delta));
                        comparison_row(
                            "Aggregate payload bits",
                            detail::format_number(first_aggregate.total_payload_bits),
                            detail::format_number(second_aggregate.total_payload_bits),
                            detail::format_delta_number(comparison.total_payload_bit_delta));
                        comparison_row(
                            "Aggregate reserved-region bits",
                            detail::format_number(first_aggregate.total_reserved_bits),
                            detail::format_number(second_aggregate.total_reserved_bits),
                            detail::format_delta_number(comparison.total_reserved_bit_delta));
                        comparison_row(
                            "Aggregate unused bits",
                            detail::format_number(first_aggregate.total_unused_bits),
                            detail::format_number(second_aggregate.total_unused_bits),
                            detail::format_delta_number(comparison.total_unused_bit_delta));
                        comparison_row(
                            "Cache-line size",
                            detail::format_bytes(first_aggregate.cache_line_bytes),
                            detail::format_bytes(second_aggregate.cache_line_bytes),
                            detail::format_delta_bytes(comparison.cache_line_size_delta));
                        comparison_row(
                            "Minimum cache lines",
                            detail::format_number(first_aggregate.minimum_cache_lines),
                            detail::format_number(second_aggregate.minimum_cache_lines),
                            detail::format_delta_number(comparison.minimum_cache_line_delta));
                        comparison_row(
                            "Complete elements per cache line",
                            detail::format_number(first_aggregate.complete_elements_per_cache_line),
                            detail::format_number(
                                second_aggregate.complete_elements_per_cache_line),
                            detail::format_delta_number(
                                comparison.complete_elements_per_cache_line_delta));
                        comparison_row(
                            "Cache-line-straddling elements",
                            detail::format_number(first_aggregate.cache_line_straddling_elements),
                            detail::format_number(second_aggregate.cache_line_straddling_elements),
                            detail::format_delta_number(comparison.cache_line_straddling_delta));
                        comparison_row("Page size",
                                       detail::format_bytes(first_aggregate.page_bytes),
                                       detail::format_bytes(second_aggregate.page_bytes),
                                       detail::format_delta_bytes(comparison.page_size_delta));
                        comparison_row("Minimum pages",
                                       detail::format_number(first_aggregate.minimum_pages),
                                       detail::format_number(second_aggregate.minimum_pages),
                                       detail::format_delta_number(comparison.minimum_page_delta));
                        comparison_row(
                            "Complete elements per page",
                            detail::format_number(first_aggregate.complete_elements_per_page),
                            detail::format_number(second_aggregate.complete_elements_per_page),
                            detail::format_delta_number(
                                comparison.complete_elements_per_page_delta));
                        comparison_row(
                            "Page-straddling elements",
                            detail::format_number(first_aggregate.page_straddling_elements),
                            detail::format_number(second_aggregate.page_straddling_elements),
                            detail::format_delta_number(comparison.page_straddling_delta));
                        comparison_row(
                            "Fits L1 data cache",
                            detail::format_fit(first_aggregate.cache_capacity.fits_l1_data),
                            detail::format_fit(second_aggregate.cache_capacity.fits_l1_data));
                        comparison_row("Fits L2 cache",
                                       detail::format_fit(first_aggregate.cache_capacity.fits_l2),
                                       detail::format_fit(second_aggregate.cache_capacity.fits_l2));
                        comparison_row("Fits L3 cache",
                                       detail::format_fit(first_aggregate.cache_capacity.fits_l3),
                                       detail::format_fit(second_aggregate.cache_capacity.fits_l3));
                        ImGui::EndTable();
                    }

                    if (ImGui::TreeNodeEx("Segment layout", ImGuiTreeNodeFlags_SpanAvailWidth)) {
                        for (auto const& field : comparison.fields) {
                            ImGui::PushID(field.name.c_str());
                            if (detail::section(field.name.c_str())) {
                                if (ImGui::BeginTable("packed-field-target-comparison",
                                                      4,
                                                      ImGuiTableFlags_Borders |
                                                          ImGuiTableFlags_RowBg |
                                                          ImGuiTableFlags_Resizable)) {
                                    ImGui::TableSetupColumn("Physical fact");
                                    ImGui::TableSetupColumn(
                                        analysis_session_.primary_abi().name().c_str());
                                    ImGui::TableSetupColumn(
                                        analysis_session_.comparison_abi().name().c_str());
                                    ImGui::TableSetupColumn("Difference (B - A)");
                                    ImGui::TableHeadersRow();
                                    comparison_row(
                                        "Role",
                                        field.first.reserved ? "Reserved region" : "Field",
                                        field.second.reserved ? "Reserved region" : "Field");
                                    comparison_row("Logical type",
                                                   field.first.logical_type,
                                                   field.second.logical_type);
                                    comparison_row("Width",
                                                   std::to_string(field.first.bit_width) + " bits",
                                                   std::to_string(field.second.bit_width) +
                                                       " bits");
                                    comparison_row(
                                        "Least-significant bit",
                                        std::to_string(field.first.least_significant_bit),
                                        std::to_string(field.second.least_significant_bit),
                                        detail::format_delta_number(
                                            field.least_significant_bit_delta));
                                    comparison_row(
                                        "Most-significant bit",
                                        detail::format_number(field.first.most_significant_bit),
                                        detail::format_number(field.second.most_significant_bit),
                                        detail::format_delta_number(
                                            field.most_significant_bit_delta));
                                    comparison_row(
                                        "Maximum unsigned value",
                                        detail::format_number(field.first.maximum_unsigned_value),
                                        detail::format_number(field.second.maximum_unsigned_value));
                                    comparison_row(
                                        "Unused codes",
                                        detail::format_number(field.first.unused_codes),
                                        detail::format_number(field.second.unused_codes),
                                        detail::format_delta_number(field.unused_code_delta));
                                    if (field.first.relationship_kind.has_value() ||
                                        field.second.relationship_kind.has_value()) {
                                        comparison_row("Relationship target extent",
                                                       detail::format_number(
                                                           field.first.relationship_target_extent),
                                                       detail::format_number(
                                                           field.second.relationship_target_extent),
                                                       detail::format_delta_number(
                                                           field.relationship_target_extent_delta));
                                        comparison_row(
                                            "Relationship minimum width",
                                            detail::format_number(
                                                field.first.relationship_minimum_required_bits),
                                            detail::format_number(
                                                field.second.relationship_minimum_required_bits),
                                            detail::format_delta_number(
                                                field.relationship_minimum_required_bit_delta));
                                        comparison_row(
                                            "Relationship capacity headroom",
                                            detail::format_number(
                                                field.first.relationship_capacity_headroom),
                                            detail::format_number(
                                                field.second.relationship_capacity_headroom),
                                            detail::format_delta_number(
                                                field.relationship_capacity_headroom_delta));
                                        comparison_row(
                                            "Relationship effective capacity limit",
                                            detail::format_number(
                                                field.first.relationship_effective_capacity_limit),
                                            detail::format_number(
                                                field.second.relationship_effective_capacity_limit),
                                            detail::format_delta_number(
                                                field.relationship_effective_capacity_limit_delta));
                                    }
                                    ImGui::EndTable();
                                }
                            }
                            ImGui::PopID();
                        }
                        ImGui::TreePop();
                    }
                    if (analysis_session_.results().packed_target_access_comparison.has_value()) {
                        auto const& access{
                            *analysis_session_.results().packed_target_access_comparison};
                        auto const& first_access{access.first};
                        auto const& second_access{access.second};
                        std::string field_names;
                        for (auto const& intent : first_access.accesses) {
                            if (!field_names.empty()) {
                                field_names += ", ";
                            }
                            field_names += intent.name;
                            field_names += " (";
                            field_names += detail::access_operation_name(intent.operation);
                            field_names += ')';
                        }
                        if (detail::section("Selected-field target workload")) {
                            ImGui::TextWrapped("Fields: %s", field_names.c_str());
                            ImGui::Text("Accesses / element: %llu",
                                        static_cast<unsigned long long>(first_access.multiplicity));
                            if (ImGui::BeginTable("packed-target-access-comparison",
                                                  4,
                                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                                      ImGuiTableFlags_Resizable)) {
                                ImGui::TableSetupColumn("Workload fact");
                                ImGui::TableSetupColumn(
                                    analysis_session_.primary_abi().name().c_str());
                                ImGui::TableSetupColumn(
                                    analysis_session_.comparison_abi().name().c_str());
                                ImGui::TableSetupColumn("Difference (B - A)");
                                ImGui::TableHeadersRow();
                                comparison_row("Element count",
                                               std::to_string(first_access.element_count),
                                               std::to_string(second_access.element_count));
                                comparison_row(
                                    "Selected useful bits",
                                    detail::format_number(first_access.useful_bits),
                                    detail::format_number(second_access.useful_bits),
                                    detail::format_delta_number(access.useful_bit_delta));
                                comparison_row(
                                    "Classified read useful bits",
                                    detail::format_number(first_access.read_useful_bits),
                                    detail::format_number(second_access.read_useful_bits),
                                    detail::format_delta_number(access.read_useful_bit_delta));
                                comparison_row(
                                    "Classified write useful bits",
                                    detail::format_number(first_access.write_useful_bits),
                                    detail::format_number(second_access.write_useful_bits),
                                    detail::format_delta_number(access.write_useful_bit_delta));
                                comparison_row(
                                    "Logical read useful bits",
                                    detail::format_number(first_access.logical_read_useful_bits),
                                    detail::format_number(second_access.logical_read_useful_bits),
                                    detail::format_delta_number(
                                        access.logical_read_useful_bit_delta));
                                comparison_row(
                                    "Logical write useful bits",
                                    detail::format_number(first_access.logical_write_useful_bits),
                                    detail::format_number(second_access.logical_write_useful_bits),
                                    detail::format_delta_number(
                                        access.logical_write_useful_bit_delta));
                                comparison_row(
                                    "Containing storage",
                                    detail::format_bytes(first_access.storage_footprint_bytes),
                                    detail::format_bytes(second_access.storage_footprint_bytes),
                                    detail::format_delta_bytes(
                                        access.storage_footprint_byte_delta));
                                comparison_row(
                                    "Containing storage bits",
                                    detail::format_number(first_access.storage_footprint_bits),
                                    detail::format_number(second_access.storage_footprint_bits),
                                    detail::format_delta_number(
                                        access.storage_footprint_bit_delta));
                                comparison_row(
                                    "Non-useful storage bits",
                                    detail::format_number(first_access.non_useful_storage_bits),
                                    detail::format_number(second_access.non_useful_storage_bits),
                                    detail::format_delta_number(
                                        access.non_useful_storage_bit_delta));
                                comparison_row(
                                    "Unselected ordinary-field bits",
                                    detail::format_number(first_access.unselected_field_bits),
                                    detail::format_number(second_access.unselected_field_bits),
                                    detail::format_delta_number(access.unselected_field_bit_delta));
                                comparison_row(
                                    "Explicit reserved-region bits",
                                    detail::format_number(first_access.reserved_region_bits),
                                    detail::format_number(second_access.reserved_region_bits),
                                    detail::format_delta_number(access.reserved_region_bit_delta));
                                comparison_row("Physically unused backing bits",
                                               detail::format_number(
                                                   first_access.physically_unused_storage_bits),
                                               detail::format_number(
                                                   second_access.physically_unused_storage_bits),
                                               detail::format_delta_number(
                                                   access.physically_unused_storage_bit_delta));
                                comparison_row(
                                    "Cache-line size",
                                    detail::format_bytes(first_access.cache_line_bytes),
                                    detail::format_bytes(second_access.cache_line_bytes),
                                    detail::format_delta_bytes(access.cache_line_size_delta));
                                comparison_row(
                                    "Minimum cache lines touched",
                                    detail::format_number(first_access.minimum_cache_lines_touched),
                                    detail::format_number(
                                        second_access.minimum_cache_lines_touched),
                                    detail::format_delta_number(access.cache_line_delta));
                                comparison_row(
                                    "Minimum cache-line footprint",
                                    detail::format_bytes(first_access.minimum_cache_bytes_touched),
                                    detail::format_bytes(second_access.minimum_cache_bytes_touched),
                                    detail::format_delta_bytes(access.cache_byte_delta));
                                comparison_row(
                                    "Read cache lines covered",
                                    detail::format_number(first_access.read_cache_lines_touched),
                                    detail::format_number(second_access.read_cache_lines_touched),
                                    detail::format_delta_number(access.read_cache_line_delta));
                                comparison_row(
                                    "Read cache-line address coverage",
                                    detail::format_bytes(first_access.read_cache_bytes_touched),
                                    detail::format_bytes(second_access.read_cache_bytes_touched),
                                    detail::format_delta_bytes(access.read_cache_byte_delta));
                                comparison_row(
                                    "Write cache lines covered",
                                    detail::format_number(first_access.write_cache_lines_touched),
                                    detail::format_number(second_access.write_cache_lines_touched),
                                    detail::format_delta_number(access.write_cache_line_delta));
                                comparison_row(
                                    "Write cache-line address coverage",
                                    detail::format_bytes(first_access.write_cache_bytes_touched),
                                    detail::format_bytes(second_access.write_cache_bytes_touched),
                                    detail::format_delta_bytes(access.write_cache_byte_delta));
                                comparison_row(
                                    "Minimum footprint <= L1 data cache",
                                    detail::format_fit(
                                        first_access.cache_footprint_capacity.fits_l1_data),
                                    detail::format_fit(
                                        second_access.cache_footprint_capacity.fits_l1_data));
                                comparison_row("Minimum footprint <= L2 cache",
                                               detail::format_fit(
                                                   first_access.cache_footprint_capacity.fits_l2),
                                               detail::format_fit(
                                                   second_access.cache_footprint_capacity.fits_l2));
                                comparison_row("Minimum footprint <= L3 cache",
                                               detail::format_fit(
                                                   first_access.cache_footprint_capacity.fits_l3),
                                               detail::format_fit(
                                                   second_access.cache_footprint_capacity.fits_l3));
                                comparison_row("Page size",
                                               detail::format_bytes(first_access.page_bytes),
                                               detail::format_bytes(second_access.page_bytes),
                                               detail::format_delta_bytes(access.page_size_delta));
                                comparison_row(
                                    "Minimum pages touched",
                                    detail::format_number(first_access.minimum_pages_touched),
                                    detail::format_number(second_access.minimum_pages_touched),
                                    detail::format_delta_number(access.page_delta));
                                comparison_row(
                                    "Minimum page footprint",
                                    detail::format_bytes(first_access.minimum_page_bytes_touched),
                                    detail::format_bytes(second_access.minimum_page_bytes_touched),
                                    detail::format_delta_bytes(access.page_byte_delta));
                                comparison_row(
                                    "Read pages covered",
                                    detail::format_number(first_access.read_pages_touched),
                                    detail::format_number(second_access.read_pages_touched),
                                    detail::format_delta_number(access.read_page_delta));
                                comparison_row(
                                    "Read page address coverage",
                                    detail::format_bytes(first_access.read_page_bytes_touched),
                                    detail::format_bytes(second_access.read_page_bytes_touched),
                                    detail::format_delta_bytes(access.read_page_byte_delta));
                                comparison_row(
                                    "Write pages covered",
                                    detail::format_number(first_access.write_pages_touched),
                                    detail::format_number(second_access.write_pages_touched),
                                    detail::format_delta_number(access.write_page_delta));
                                comparison_row(
                                    "Write page address coverage",
                                    detail::format_bytes(first_access.write_page_bytes_touched),
                                    detail::format_bytes(second_access.write_page_bytes_touched),
                                    detail::format_delta_bytes(access.write_page_byte_delta));
                                ImGui::EndTable();
                            }
                            ImGui::TextDisabled(
                                "Both profiles use the same active packed variant and explicit "
                                "session "
                                "workload. Physical coverage remains the containing packed-word "
                                "array; "
                                "selected fields do not imply sub-word fetches. Logical useful "
                                "totals "
                                "scale by multiplicity only, and no row is a performance claim.");
                            draw_diagnostics(access.diagnostics);
                        }
                    }
                    ImGui::TextDisabled(
                        "Target comparison holds the active physical variant fixed. It reports "
                        "layout and footprint facts, not sub-field fetches or performance.");
                    draw_diagnostics(comparison.diagnostics);
                } else {
                    ImGui::TextDisabled("No packed target comparison is available.");
                }
                if (detail::section("Physical-variant comparison under target A")) {}
            }
        }

        auto const* selected_soa{std::get_if<SoaType>(&selected_node.definition)};
        if (selected_soa != nullptr &&
            selected_soa->backend == codegen::SoaBackend::standard_library &&
            draw_soa_target_comparison()) {
            ImGui::End();
            return;
        }

        auto const* selected_quantized{std::get_if<LinearQuantizedType>(&selected_node.definition)};
        if (selected_quantized != nullptr) {
            auto comparison_type{analysis_session_.inputs.quantized_comparison_type.has_value()
                                     ? analysis_session_.inputs.workspace.types().find(
                                           *analysis_session_.inputs.quantized_comparison_type)
                                     : std::nullopt};
            auto const comparison_label{
                comparison_type.has_value()
                    ? type_label(analysis_session_.inputs.workspace.types().type(*comparison_type))
                    : std::string{"No same-source representation"}};
            ImGui::Text("A: %s", type_label(selected_node).c_str());
            ImGui::SetNextItemWidth(-1.0F);
            if (ImGui::BeginCombo("B##quantized-comparison", comparison_label.c_str())) {
                auto const types{analysis_session_.inputs.workspace.types().types()};
                for (std::size_t index{}; index < types.size(); ++index) {
                    auto const candidate{TypeId{static_cast<std::uint32_t>(index)}};
                    auto const* quantized{
                        std::get_if<LinearQuantizedType>(&types[index].definition)};
                    if (candidate == *analysis_session_.inputs.selection.type ||
                        quantized == nullptr ||
                        quantized->source.type != selected_quantized->source.type) {
                        continue;
                    }
                    auto const is_selected{comparison_type == candidate};
                    auto const label{type_label(types[index])};
                    if (ImGui::Selectable(label.c_str(), is_selected)) {
                        analysis_session_.inputs.quantized_comparison_type = types[index].identity;
                        refresh_analysis();
                        comparison_type = candidate;
                    }
                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            if (!analysis_session_.results().linear_quantized_comparison.has_value()) {
                ImGui::TextDisabled(
                    "Declare another linear quantized representation of the same semantic source "
                    "to compare precision and encoded payload cost.");
                ImGui::End();
                return;
            }

            if (detail::section("Analysis scale")) {
                if (draw_element_count()) {
                    ImGui::End();
                    return;
                }

                auto const& comparison{*analysis_session_.results().linear_quantized_comparison};
                auto const& second_node{
                    analysis_session_.inputs.workspace.types().type(comparison.second.type)};
                if (ImGui::BeginTable("linear-quantized-comparison",
                                      4,
                                      ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                          ImGuiTableFlags_Resizable)) {
                    ImGui::TableSetupColumn("Fact");
                    ImGui::TableSetupColumn(selected_node.identity.name.c_str());
                    ImGui::TableSetupColumn(second_node.identity.name.c_str());
                    ImGui::TableSetupColumn("Difference (B - A)");
                    ImGui::TableHeadersRow();
                    comparison_row("Semantic source",
                                   analysis_session_.inputs.workspace.types()
                                       .type(comparison.first.source_type)
                                       .identity.name,
                                   analysis_session_.inputs.workspace.types()
                                       .type(comparison.second.source_type)
                                       .identity.name);
                    comparison_row(
                        "Encoded width",
                        std::to_string(comparison.first.encoded_storage_bits) + " bits",
                        std::to_string(comparison.second.encoded_storage_bits) + " bits",
                        detail::format_delta_number(comparison.encoded_storage_bit_delta));
                    comparison_row("Total codes",
                                   detail::format_code_count(comparison.first.total_code_count),
                                   detail::format_code_count(comparison.second.total_code_count),
                                   detail::format_delta_number(comparison.total_code_count_delta));
                    comparison_row(
                        "Reserved codes",
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
                        std::string{
                            codegen::quantization_clipping_name(comparison.second.clipping)});
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
                ImGui::TextDisabled("Payload bits are exact encoding facts. Allocated bytes, "
                                    "stride, cache lines, and "
                                    "pages require a container placement policy and are "
                                    "intentionally not inferred.");
                draw_diagnostics(comparison.diagnostics);

                if (ImGui::Button("Select B representation") && comparison_type.has_value()) {
                    select_type(comparison_type);
                    analysis_session_.inputs.selection.field.clear();
                    graph_focus_selected_ = true;
                    refresh_analysis();
                }
                ImGui::SameLine();
                if (ImGui::Button("Select semantic source")) {
                    select_type(comparison.first.source_type);
                    analysis_session_.inputs.selection.field.clear();
                    graph_focus_selected_ = true;
                    refresh_analysis();
                }
                ImGui::End();
                return;
            }
        }

        auto const selected_optional_source{optional_source(selected_node.definition)};
        if (selected_optional_source.has_value()) {
            auto comparison_type{analysis_session_.inputs.optional_comparison_type.has_value()
                                     ? analysis_session_.inputs.workspace.types().find(
                                           *analysis_session_.inputs.optional_comparison_type)
                                     : std::nullopt};
            auto const comparison_label{
                comparison_type.has_value()
                    ? type_label(analysis_session_.inputs.workspace.types().type(*comparison_type))
                    : std::string{"No same-source representation"}};
            ImGui::Text("A: %s", type_label(selected_node).c_str());
            ImGui::SetNextItemWidth(-1.0F);
            if (ImGui::BeginCombo("B##optional-comparison", comparison_label.c_str())) {
                auto const types{analysis_session_.inputs.workspace.types().types()};
                for (std::size_t index{}; index < types.size(); ++index) {
                    auto const candidate{TypeId{static_cast<std::uint32_t>(index)}};
                    auto const candidate_source{optional_source(types[index].definition)};
                    if (candidate == *analysis_session_.inputs.selection.type ||
                        candidate_source != selected_optional_source) {
                        continue;
                    }
                    auto const is_selected{comparison_type == candidate};
                    auto const label{type_label(types[index])};
                    if (ImGui::Selectable(label.c_str(), is_selected)) {
                        analysis_session_.inputs.optional_comparison_type = types[index].identity;
                        refresh_analysis();
                        comparison_type = candidate;
                    }
                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            if (!analysis_session_.results().optional_encoding_comparison.has_value()) {
                ImGui::TextDisabled(
                    "Declare another sentinel or presence-bit optional representation of the "
                    "same semantic source to compare encoding consequences.");
                ImGui::End();
                return;
            }

            if (detail::section("Analysis scale")) {
                if (draw_element_count()) {
                    ImGui::End();
                    return;
                }

                auto const& comparison{*analysis_session_.results().optional_encoding_comparison};
                auto const& second_node{
                    analysis_session_.inputs.workspace.types().type(comparison.second.type)};
                if (ImGui::BeginTable("optional-encoding-comparison",
                                      4,
                                      ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                          ImGuiTableFlags_Resizable)) {
                    ImGui::TableSetupColumn("Fact");
                    ImGui::TableSetupColumn(selected_node.identity.name.c_str());
                    ImGui::TableSetupColumn(second_node.identity.name.c_str());
                    ImGui::TableSetupColumn("Difference (B - A)");
                    ImGui::TableHeadersRow();
                    comparison_row("Semantic source",
                                   analysis_session_.inputs.workspace.types()
                                       .type(comparison.first.source_type)
                                       .identity.name,
                                   analysis_session_.inputs.workspace.types()
                                       .type(comparison.second.source_type)
                                       .identity.name);
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
                    comparison_row(
                        "Encoded width",
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
                    "Code-space roles and payload bits are exact. Allocated bytes, bit order, "
                    "stride, "
                    "cache lines, and pages require a placement policy and remain unspecified.");
                draw_diagnostics(comparison.diagnostics);

                if (ImGui::Button("Select B representation") && comparison_type.has_value()) {
                    select_type(comparison_type);
                    analysis_session_.inputs.selection.field.clear();
                    graph_focus_selected_ = true;
                    refresh_analysis();
                }
                ImGui::SameLine();
                if (ImGui::Button("Select semantic source")) {
                    select_type(comparison.first.source_type);
                    analysis_session_.inputs.selection.field.clear();
                    graph_focus_selected_ = true;
                    refresh_analysis();
                }
                ImGui::End();
                return;
            }
        }

        auto const* selected_varint{std::get_if<IntegerVarintType>(&selected_node.definition)};
        if (selected_varint != nullptr) {
            auto comparison_type{analysis_session_.inputs.varint_comparison_type.has_value()
                                     ? analysis_session_.inputs.workspace.types().find(
                                           *analysis_session_.inputs.varint_comparison_type)
                                     : std::nullopt};
            auto const comparison_label{
                comparison_type.has_value()
                    ? type_label(analysis_session_.inputs.workspace.types().type(*comparison_type))
                    : std::string{"No same-source representation"}};
            ImGui::Text("A: %s", type_label(selected_node).c_str());
            ImGui::SetNextItemWidth(-1.0F);
            if (ImGui::BeginCombo("B##varint-comparison", comparison_label.c_str())) {
                auto const types{analysis_session_.inputs.workspace.types().types()};
                for (std::size_t index{}; index < types.size(); ++index) {
                    auto const candidate{TypeId{static_cast<std::uint32_t>(index)}};
                    auto const* varint{std::get_if<IntegerVarintType>(&types[index].definition)};
                    if (candidate == *analysis_session_.inputs.selection.type ||
                        varint == nullptr || varint->source.type != selected_varint->source.type) {
                        continue;
                    }
                    auto const is_selected{comparison_type == candidate};
                    auto const label{type_label(types[index])};
                    if (ImGui::Selectable(label.c_str(), is_selected)) {
                        analysis_session_.inputs.varint_comparison_type = types[index].identity;
                        refresh_analysis();
                        comparison_type = candidate;
                    }
                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            if (!analysis_session_.results().integer_varint_comparison.has_value()) {
                ImGui::TextDisabled(
                    "Declare another integer varint representation of the same semantic source "
                    "to compare encoded-size bounds.");
                ImGui::End();
                return;
            }

            if (detail::section("Analysis scale")) {
                if (draw_element_count()) {
                    ImGui::End();
                    return;
                }

                auto const& comparison{*analysis_session_.results().integer_varint_comparison};
                auto const& second_node{
                    analysis_session_.inputs.workspace.types().type(comparison.second.type)};
                auto const& source_node{
                    analysis_session_.inputs.workspace.types().type(comparison.first.source_type)};
                auto const& distribution_comparison{
                    analysis_session_.results().integer_varint_distribution_comparison};
                auto invalid_distribution_literals{std::size_t{}};
                if (auto const rows{varint_distributions_.find(source_node.identity)};
                    rows != varint_distributions_.end() && !rows->second.empty()) {
                    for (auto const& row : rows->second) {
                        if (!detail::parse_packed_integer(row.value.data()).has_value()) {
                            ++invalid_distribution_literals;
                        }
                    }
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
                    comparison_row("Semantic source",
                                   analysis_session_.inputs.workspace.types()
                                       .type(comparison.first.source_type)
                                       .identity.name,
                                   analysis_session_.inputs.workspace.types()
                                       .type(comparison.second.source_type)
                                       .identity.name);
                    comparison_row("Encoding",
                                   std::string{codegen::integer_varint_encoding_name(
                                       comparison.first.encoding)},
                                   std::string{codegen::integer_varint_encoding_name(
                                       comparison.second.encoding)});
                    comparison_row(
                        "Minimum bytes / value",
                        std::to_string(comparison.first.minimum_encoded_bytes),
                        std::to_string(comparison.second.minimum_encoded_bytes),
                        detail::format_delta_bytes(comparison.minimum_encoded_byte_delta));
                    comparison_row(
                        "Maximum bytes / value",
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
                        comparison_row("Distribution sample bytes",
                                       detail::format_bytes(
                                           distribution_comparison->first.total_encoded_bytes),
                                       detail::format_bytes(
                                           distribution_comparison->second.total_encoded_bytes),
                                       detail::format_delta_bytes(
                                           distribution_comparison->total_encoded_byte_delta));
                        comparison_row(
                            "Expected bytes / value",
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
                    auto const entry_count{
                        std::min(distribution_comparison->first.entries.size(),
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
                            detail::format_delta_number(numeric_delta(first_entry.encoded_bytes,
                                                                      second_entry.encoded_bytes))
                                .c_str());
                    }
                    ImGui::EndTable();
                }
                ImGui::TextDisabled(
                    "Bounds are exact for the declared domain. Expected rows use the "
                    "same explicit session distribution for both encodings.");
                if (invalid_distribution_literals != 0) {
                    ImGui::TextColored(
                        detail::diagnostic_color(DiagnosticSeverity::error),
                        "%llu distribution row%s contain an invalid integer literal and were "
                        "skipped.",
                        static_cast<unsigned long long>(invalid_distribution_literals),
                        invalid_distribution_literals == 1 ? "" : "s");
                }
                draw_diagnostics(comparison.diagnostics);
                if (distribution_comparison.has_value()) {
                    draw_diagnostics(distribution_comparison->diagnostics);
                }

                if (ImGui::Button("Select B representation") && comparison_type.has_value()) {
                    select_type(comparison_type);
                    analysis_session_.inputs.selection.field.clear();
                    graph_focus_selected_ = true;
                    refresh_analysis();
                }
                ImGui::SameLine();
                if (ImGui::Button("Select semantic source")) {
                    select_type(comparison.first.source_type);
                    analysis_session_.inputs.selection.field.clear();
                    graph_focus_selected_ = true;
                    refresh_analysis();
                }
                ImGui::End();
                return;
            }
        }
    }

    auto draw_variant_selector = [&](char const* id, std::uint64_t& selected_id) {
        auto changed{false};
        auto const* selected{analysis_session_.inputs.workspace.variant(selected_id)};
        ImGui::SetNextItemWidth(-1.0F);
        ImGui::PushID(id);
        if (ImGui::BeginCombo("##variant", selected->name.c_str())) {
            for (auto const& variant : analysis_session_.inputs.workspace.variants()) {
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
        selection_changed |=
            draw_variant_selector("comparison-a", analysis_session_.inputs.comparison_a_variant_id);
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("B");
        if (draw_variant_selector("comparison-b",
                                  analysis_session_.inputs.comparison_b_variant_id)) {
            analysis_session_.inputs.comparison_b_follows_active = false;
            selection_changed = true;
        }
        ImGui::TableNextColumn();
        if (ImGui::Button("Swap A / B", {-1.0F, 0.0F})) {
            std::swap(analysis_session_.inputs.comparison_a_variant_id,
                      analysis_session_.inputs.comparison_b_variant_id);
            analysis_session_.inputs.comparison_b_follows_active = false;
            selection_changed = true;
        }
        ImGui::TableNextColumn();
        ImGui::BeginDisabled(analysis_session_.inputs.comparison_b_follows_active &&
                             analysis_session_.inputs.comparison_b_variant_id ==
                                 analysis_session_.inputs.workspace.active_variant_id());
        if (ImGui::Button("Use active variant for B", {-1.0F, 0.0F})) {
            analysis_session_.inputs.comparison_b_variant_id =
                analysis_session_.inputs.workspace.active_variant_id();
            analysis_session_.inputs.comparison_b_follows_active = true;
            selection_changed = true;
        }
        ImGui::EndDisabled();
        ImGui::EndTable();
    }
    if (analysis_session_.inputs.comparison_b_follows_active) {
        ImGui::TextDisabled("B follows the active editing variant.");
    }
    if (selection_changed) {
        refresh_analysis();
    }

    auto const& comparison_a{*analysis_session_.inputs.workspace.variant(
        analysis_session_.inputs.comparison_a_variant_id)};
    auto const& comparison_b{*analysis_session_.inputs.workspace.variant(
        analysis_session_.inputs.comparison_b_variant_id)};

    if (analysis_session_.results().integer_scalar_capacity_comparison.has_value()) {
        auto const& comparison{*analysis_session_.results().integer_scalar_capacity_comparison};
        auto const& first{comparison.first};
        auto const& second{comparison.second};
        auto const format_relationship{[](IntegerScalarAnalysis const& analysis) {
            if (!analysis.relationship_kind.has_value() ||
                !analysis.relationship_target.has_value()) {
                return std::string{"None"};
            }
            auto result{
                std::string{codegen::semantic_relation_kind_name(*analysis.relationship_kind)}};
            if (analysis.relationship_unit.has_value()) {
                result += " (";
                result += codegen::semantic_relation_unit_name(*analysis.relationship_unit);
                result += ')';
            }
            return result + " -> " + *analysis.relationship_target;
        }};
        auto const format_exact{[](std::optional<ExactCodeCount> const count,
                                   std::optional<std::uint32_t> const minimum_bits) {
            if (count.has_value()) {
                return detail::format_code_count(*count);
            }
            return minimum_bits.value_or(0) > 64 ? std::string{"> 2^64"} : std::string{"Unknown"};
        }};
        auto const format_fit{[](std::optional<bool> const fits) {
            return fits.has_value() ? (*fits ? std::string{"Yes"} : std::string{"No"})
                                    : std::string{"Unknown"};
        }};
        auto fit_difference{std::string{"Unknown"}};
        if (comparison.relationship_width_fit_changed.has_value()) {
            if (!*comparison.relationship_width_fit_changed) {
                fit_difference = "—";
            } else {
                fit_difference = second.relationship_width_sufficient.value_or(false)
                                   ? "B now fits"
                                   : "B no longer fits";
            }
        }
        auto const relationship_kind{
            first.relationship_kind.value_or(codegen::SemanticRelationKind::references)};
        auto const extent_term{detail::relationship_extent_term(relationship_kind)};
        auto const extent_unit{
            detail::relationship_extent_unit(relationship_kind, first.relationship_unit)};
        auto const target_extent_label{"Target " + std::string{extent_term} + " (" +
                                       std::string{extent_unit} + ")"};
        auto const minimum_width_label{std::string{extent_term} + " minimum width"};
        auto const code_space_limit_label{"Code-space " + std::string{extent_term} + " limit"};
        auto const code_space_headroom_label{"Code-space " + std::string{extent_term} +
                                             " headroom"};
        auto const semantic_limit_label{"Semantic-range " + std::string{extent_term} + " limit"};
        auto const sentinel_limit_label{"Sentinel-placement " + std::string{extent_term} +
                                        " limit"};
        auto const effective_limit_label{"Effective valid " + std::string{extent_term} + " limit"};
        auto const effective_headroom_label{"Effective valid " + std::string{extent_term} +
                                            " headroom"};

        if (ImGui::BeginTable("integer-scalar-capacity-comparison",
                              4,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("Fact");
            ImGui::TableSetupColumn(comparison_a.name.c_str());
            ImGui::TableSetupColumn(comparison_b.name.c_str());
            ImGui::TableSetupColumn("Difference");
            ImGui::TableHeadersRow();
            comparison_row("Relationship", format_relationship(first), format_relationship(second));
            comparison_row("Declared width",
                           first.declared_bit_width.has_value()
                               ? std::to_string(*first.declared_bit_width) + " bits"
                               : "auto",
                           second.declared_bit_width.has_value()
                               ? std::to_string(*second.declared_bit_width) + " bits"
                               : "auto");
            comparison_row("Current effective width",
                           std::to_string(first.effective_bit_width) + " bits",
                           std::to_string(second.effective_bit_width) + " bits");
            comparison_row(
                target_extent_label.c_str(),
                detail::format_number(first.relationship_target_extent),
                detail::format_number(second.relationship_target_extent),
                detail::format_delta_number(comparison.relationship_target_extent_delta));
            comparison_row("Required live values",
                           format_exact(first.relationship_live_value_count,
                                        first.relationship_minimum_required_bits),
                           format_exact(second.relationship_live_value_count,
                                        second.relationship_minimum_required_bits));
            comparison_row("Required codes (including sentinels)",
                           format_exact(first.relationship_required_code_count,
                                        first.relationship_minimum_required_bits),
                           format_exact(second.relationship_required_code_count,
                                        second.relationship_minimum_required_bits));
            comparison_row(
                minimum_width_label.c_str(),
                detail::format_number(first.relationship_minimum_required_bits),
                detail::format_number(second.relationship_minimum_required_bits),
                detail::format_delta_number(comparison.relationship_minimum_required_bit_delta));
            comparison_row("Current planning width fits",
                           format_fit(first.relationship_width_sufficient),
                           format_fit(second.relationship_width_sufficient),
                           fit_difference);
            comparison_row(code_space_limit_label.c_str(),
                           detail::format_number(first.relationship_code_space_capacity_limit),
                           detail::format_number(second.relationship_code_space_capacity_limit),
                           detail::format_delta_number(
                               comparison.relationship_code_space_capacity_limit_delta));
            comparison_row(
                code_space_headroom_label.c_str(),
                detail::format_number(first.relationship_capacity_headroom),
                detail::format_number(second.relationship_capacity_headroom),
                detail::format_delta_number(comparison.relationship_capacity_headroom_delta));
            comparison_row(
                semantic_limit_label.c_str(),
                detail::format_number(first.relationship_semantic_capacity_limit),
                detail::format_number(second.relationship_semantic_capacity_limit),
                detail::format_delta_number(comparison.relationship_semantic_capacity_limit_delta));
            comparison_row(
                sentinel_limit_label.c_str(),
                detail::format_number(first.relationship_sentinel_capacity_limit),
                detail::format_number(second.relationship_sentinel_capacity_limit),
                detail::format_delta_number(comparison.relationship_sentinel_capacity_limit_delta));
            comparison_row(effective_limit_label.c_str(),
                           detail::format_number(first.relationship_effective_capacity_limit),
                           detail::format_number(second.relationship_effective_capacity_limit),
                           detail::format_delta_number(
                               comparison.relationship_effective_capacity_limit_delta));
            comparison_row(effective_headroom_label.c_str(),
                           detail::format_number(first.relationship_effective_capacity_headroom),
                           detail::format_number(second.relationship_effective_capacity_headroom),
                           detail::format_delta_number(
                               comparison.relationship_effective_capacity_headroom_delta));
            comparison_row("Allocated storage", "Unspecified", "Unspecified", "Unknown");
            ImGui::EndTable();
        }
        ImGui::TextDisabled(
            "Variant target facts are session planning facts. This semantic scalar has no implied "
            "standalone allocation, stride, cache, or page footprint.");
        if (!first.diagnostics.empty()) {
            if (detail::section("A diagnostics")) {
                draw_diagnostics(first.diagnostics);
            }
        }
        if (!second.diagnostics.empty()) {
            if (detail::section("B diagnostics")) {
                draw_diagnostics(second.diagnostics);
            }
        }
    } else if (analysis_session_.results().comparison_a_packed.has_value() &&
               analysis_session_.results().comparison_b_packed.has_value()) {
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
                           analysis_session_.results().comparison_a_packed->storage_type,
                           analysis_session_.results().comparison_b_packed->storage_type);
            comparison_row(
                "Storage bytes",
                detail::format_bytes(
                    analysis_session_.results().comparison_a_packed->storage_facts.transform(
                        [](layout::TypeFacts const& facts) { return facts.size_bytes; })),
                detail::format_bytes(
                    analysis_session_.results().comparison_b_packed->storage_facts.transform(
                        [](layout::TypeFacts const& facts) { return facts.size_bytes; })),
                detail::format_delta_bytes(layout::numeric_delta(
                    analysis_session_.results().comparison_a_packed->storage_facts.transform(
                        [](layout::TypeFacts const& facts) { return facts.size_bytes; }),
                    analysis_session_.results().comparison_b_packed->storage_facts.transform(
                        [](layout::TypeFacts const& facts) { return facts.size_bytes; }))));
            comparison_row("Storage bits",
                           detail::format_number(
                               analysis_session_.results().comparison_a_packed->storage_bits),
                           detail::format_number(
                               analysis_session_.results().comparison_b_packed->storage_bits),
                           detail::format_delta_number(layout::numeric_delta(
                               analysis_session_.results().comparison_a_packed->storage_bits,
                               analysis_session_.results().comparison_b_packed->storage_bits)));
            comparison_row(
                "Bits used",
                detail::format_number(analysis_session_.results().comparison_a_packed->bits_used),
                detail::format_number(analysis_session_.results().comparison_b_packed->bits_used),
                detail::format_delta_number(layout::numeric_delta(
                    analysis_session_.results().comparison_a_packed->bits_used,
                    analysis_session_.results().comparison_b_packed->bits_used)));
            comparison_row(
                "Unused bits",
                detail::format_number(analysis_session_.results().comparison_a_packed->unused_bits),
                detail::format_number(
                    analysis_session_.results().comparison_b_packed->unused_bits));
            comparison_row("Overflow bits",
                           detail::format_number(
                               analysis_session_.results().comparison_a_packed->overflow_bits),
                           detail::format_number(
                               analysis_session_.results().comparison_b_packed->overflow_bits));
            comparison_row(
                "Scaled storage",
                detail::format_bytes(
                    analysis_session_.results().comparison_a_packed->aggregate.total_storage_bytes),
                detail::format_bytes(
                    analysis_session_.results().comparison_b_packed->aggregate.total_storage_bytes),
                detail::format_delta_bytes(layout::numeric_delta(
                    analysis_session_.results().comparison_a_packed->aggregate.total_storage_bytes,
                    analysis_session_.results()
                        .comparison_b_packed->aggregate.total_storage_bytes)));
            comparison_row(
                "Scaled unused bits",
                detail::format_number(
                    analysis_session_.results().comparison_a_packed->aggregate.total_unused_bits),
                detail::format_number(
                    analysis_session_.results().comparison_b_packed->aggregate.total_unused_bits),
                detail::format_delta_number(layout::numeric_delta(
                    analysis_session_.results().comparison_a_packed->aggregate.total_unused_bits,
                    analysis_session_.results().comparison_b_packed->aggregate.total_unused_bits)));
            comparison_row(
                "Minimum cache lines",
                detail::format_number(
                    analysis_session_.results().comparison_a_packed->aggregate.minimum_cache_lines),
                detail::format_number(
                    analysis_session_.results().comparison_b_packed->aggregate.minimum_cache_lines),
                detail::format_delta_number(layout::numeric_delta(
                    analysis_session_.results().comparison_a_packed->aggregate.minimum_cache_lines,
                    analysis_session_.results()
                        .comparison_b_packed->aggregate.minimum_cache_lines)));
            comparison_row(
                "Complete elements / cache line",
                detail::format_number(
                    analysis_session_.results()
                        .comparison_a_packed->aggregate.complete_elements_per_cache_line),
                detail::format_number(
                    analysis_session_.results()
                        .comparison_b_packed->aggregate.complete_elements_per_cache_line),
                detail::format_delta_number(layout::numeric_delta(
                    analysis_session_.results()
                        .comparison_a_packed->aggregate.complete_elements_per_cache_line,
                    analysis_session_.results()
                        .comparison_b_packed->aggregate.complete_elements_per_cache_line)));
            comparison_row(
                "Cache-line-straddling elements",
                detail::format_number(
                    analysis_session_.results()
                        .comparison_a_packed->aggregate.cache_line_straddling_elements),
                detail::format_number(
                    analysis_session_.results()
                        .comparison_b_packed->aggregate.cache_line_straddling_elements),
                detail::format_delta_number(layout::numeric_delta(
                    analysis_session_.results()
                        .comparison_a_packed->aggregate.cache_line_straddling_elements,
                    analysis_session_.results()
                        .comparison_b_packed->aggregate.cache_line_straddling_elements)));
            comparison_row(
                "Minimum pages",
                detail::format_number(
                    analysis_session_.results().comparison_a_packed->aggregate.minimum_pages),
                detail::format_number(
                    analysis_session_.results().comparison_b_packed->aggregate.minimum_pages),
                detail::format_delta_number(layout::numeric_delta(
                    analysis_session_.results().comparison_a_packed->aggregate.minimum_pages,
                    analysis_session_.results().comparison_b_packed->aggregate.minimum_pages)));
            comparison_row("Complete elements / page",
                           detail::format_number(
                               analysis_session_.results()
                                   .comparison_a_packed->aggregate.complete_elements_per_page),
                           detail::format_number(
                               analysis_session_.results()
                                   .comparison_b_packed->aggregate.complete_elements_per_page),
                           detail::format_delta_number(layout::numeric_delta(
                               analysis_session_.results()
                                   .comparison_a_packed->aggregate.complete_elements_per_page,
                               analysis_session_.results()
                                   .comparison_b_packed->aggregate.complete_elements_per_page)));
            comparison_row(
                "Page-straddling elements",
                detail::format_number(analysis_session_.results()
                                          .comparison_a_packed->aggregate.page_straddling_elements),
                detail::format_number(analysis_session_.results()
                                          .comparison_b_packed->aggregate.page_straddling_elements),
                detail::format_delta_number(layout::numeric_delta(
                    analysis_session_.results()
                        .comparison_a_packed->aggregate.page_straddling_elements,
                    analysis_session_.results()
                        .comparison_b_packed->aggregate.page_straddling_elements)));
            for (auto const& baseline : analysis_session_.results().comparison_a_packed->fields) {
                auto const* active{
                    field_by_name(*analysis_session_.results().comparison_b_packed, baseline.name)};
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
                if (baseline.relationship_target_extent.has_value() ||
                    active->relationship_target_extent.has_value()) {
                    auto const relationship_kind{baseline.relationship_kind.value_or(
                        codegen::SemanticRelationKind::references)};
                    auto const extent_term{detail::relationship_extent_term(relationship_kind)};
                    auto const extent_unit{detail::relationship_extent_unit(
                        relationship_kind, baseline.relationship_unit)};
                    auto const relationship_label{baseline.name + " target " +
                                                  std::string{extent_term} + " (" +
                                                  std::string{extent_unit} + ")"};
                    comparison_row(relationship_label.c_str(),
                                   detail::format_number(baseline.relationship_target_extent),
                                   detail::format_number(active->relationship_target_extent),
                                   detail::format_delta_number(
                                       layout::numeric_delta(baseline.relationship_target_extent,
                                                             active->relationship_target_extent)));
                    comparison_row(
                        (baseline.name + " live values").c_str(),
                        baseline.relationship_live_value_count.has_value()
                            ? detail::format_code_count(*baseline.relationship_live_value_count)
                            : "Unknown",
                        active->relationship_live_value_count.has_value()
                            ? detail::format_code_count(*active->relationship_live_value_count)
                            : "Unknown");
                    auto const required_codes{[](layout::PackedFieldAnalysis const& field) {
                        if (field.relationship_required_code_count.has_value()) {
                            return detail::format_code_count(
                                *field.relationship_required_code_count);
                        }
                        return field.relationship_minimum_required_bits.value_or(0) > 64
                                 ? std::string{"> 2^64"}
                                 : std::string{"Unknown"};
                    }};
                    comparison_row((baseline.name + " required codes").c_str(),
                                   required_codes(baseline),
                                   required_codes(*active));
                    comparison_row(
                        (baseline.name + " " + std::string{extent_term} + " minimum width").c_str(),
                        detail::format_number(baseline.relationship_minimum_required_bits),
                        detail::format_number(active->relationship_minimum_required_bits),
                        detail::format_delta_number(
                            layout::numeric_delta(baseline.relationship_minimum_required_bits,
                                                  active->relationship_minimum_required_bits)));
                    auto const fits{[](std::optional<bool> const value) {
                        return value.has_value() ? (*value ? std::string{"Yes"} : std::string{"No"})
                                                 : std::string{"Unknown"};
                    }};
                    comparison_row((baseline.name + " planning width fits").c_str(),
                                   fits(baseline.relationship_width_sufficient),
                                   fits(active->relationship_width_sufficient));
                    comparison_row(
                        (baseline.name + " code-space " + std::string{extent_term} + " limit")
                            .c_str(),
                        detail::format_number(baseline.relationship_code_space_capacity_limit),
                        detail::format_number(active->relationship_code_space_capacity_limit),
                        detail::format_delta_number(
                            layout::numeric_delta(baseline.relationship_code_space_capacity_limit,
                                                  active->relationship_code_space_capacity_limit)));
                    comparison_row(
                        (baseline.name + " code-space " + std::string{extent_term} + " headroom")
                            .c_str(),
                        detail::format_number(baseline.relationship_capacity_headroom),
                        detail::format_number(active->relationship_capacity_headroom),
                        detail::format_delta_number(
                            layout::numeric_delta(baseline.relationship_capacity_headroom,
                                                  active->relationship_capacity_headroom)));
                    comparison_row(
                        (baseline.name + " semantic-range " + std::string{extent_term} + " limit")
                            .c_str(),
                        detail::format_number(baseline.relationship_semantic_capacity_limit),
                        detail::format_number(active->relationship_semantic_capacity_limit),
                        detail::format_delta_number(
                            layout::numeric_delta(baseline.relationship_semantic_capacity_limit,
                                                  active->relationship_semantic_capacity_limit)));
                    comparison_row(
                        (baseline.name + " sentinel-placement " + std::string{extent_term} +
                         " limit")
                            .c_str(),
                        detail::format_number(baseline.relationship_sentinel_capacity_limit),
                        detail::format_number(active->relationship_sentinel_capacity_limit),
                        detail::format_delta_number(
                            layout::numeric_delta(baseline.relationship_sentinel_capacity_limit,
                                                  active->relationship_sentinel_capacity_limit)));
                    comparison_row(
                        (baseline.name + " effective valid " + std::string{extent_term} + " limit")
                            .c_str(),
                        detail::format_number(baseline.relationship_effective_capacity_limit),
                        detail::format_number(active->relationship_effective_capacity_limit),
                        detail::format_delta_number(
                            layout::numeric_delta(baseline.relationship_effective_capacity_limit,
                                                  active->relationship_effective_capacity_limit)));
                    comparison_row(
                        (baseline.name + " effective valid " + std::string{extent_term} +
                         " headroom")
                            .c_str(),
                        detail::format_number(baseline.relationship_effective_capacity_headroom),
                        detail::format_number(active->relationship_effective_capacity_headroom),
                        detail::format_delta_number(layout::numeric_delta(
                            baseline.relationship_effective_capacity_headroom,
                            active->relationship_effective_capacity_headroom)));
                }
            }
            ImGui::EndTable();
        }
        ImGui::TextDisabled(
            "Packed boundary crossings assume contiguous arrays with cache-line/page-aligned "
            "origins; the counts are physical layout facts, not measured traffic or performance.");
        if (analysis_session_.results().packed_access_comparison.has_value()) {
            auto const& access{*analysis_session_.results().packed_access_comparison};
            std::string field_names;
            for (auto const& field_name : access.field_names) {
                if (!field_names.empty()) {
                    field_names += ", ";
                }
                field_names += field_name;
            }
            if (detail::section("Selected-field workload")) {
                ImGui::Text("Elements: %llu",
                            static_cast<unsigned long long>(access.element_count));
                ImGui::Text("Operation: %s", detail::access_operation_summary(access.accesses));
                ImGui::Text("Accesses / element: %llu",
                            static_cast<unsigned long long>(access.multiplicity));
                ImGui::TextWrapped("Fields: %s", field_names.c_str());
                if (ImGui::BeginTable("packed-access-comparison",
                                      4,
                                      ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                          ImGuiTableFlags_Resizable)) {
                    ImGui::TableSetupColumn("Fact");
                    ImGui::TableSetupColumn(comparison_a.name.c_str());
                    ImGui::TableSetupColumn(comparison_b.name.c_str());
                    ImGui::TableSetupColumn("Difference");
                    ImGui::TableHeadersRow();
                    comparison_row("Useful selected bits",
                                   detail::format_number(access.first.useful_bits),
                                   detail::format_number(access.second.useful_bits),
                                   detail::format_delta_number(access.useful_bit_delta));
                    comparison_row("Classified read useful bits",
                                   detail::format_number(access.first.read_useful_bits),
                                   detail::format_number(access.second.read_useful_bits),
                                   detail::format_delta_number(access.read_useful_bit_delta));
                    comparison_row("Classified write useful bits",
                                   detail::format_number(access.first.write_useful_bits),
                                   detail::format_number(access.second.write_useful_bits),
                                   detail::format_delta_number(access.write_useful_bit_delta));
                    comparison_row(
                        "Logical read useful bits",
                        detail::format_number(access.first.logical_read_useful_bits),
                        detail::format_number(access.second.logical_read_useful_bits),
                        detail::format_delta_number(access.logical_read_useful_bit_delta));
                    comparison_row(
                        "Logical write useful bits",
                        detail::format_number(access.first.logical_write_useful_bits),
                        detail::format_number(access.second.logical_write_useful_bits),
                        detail::format_delta_number(access.logical_write_useful_bit_delta));
                    comparison_row("Containing-word storage",
                                   detail::format_bytes(access.first.storage_footprint_bytes),
                                   detail::format_bytes(access.second.storage_footprint_bytes),
                                   detail::format_delta_bytes(access.storage_footprint_byte_delta));
                    comparison_row("Containing-word storage bits",
                                   detail::format_number(access.first.storage_footprint_bits),
                                   detail::format_number(access.second.storage_footprint_bits),
                                   detail::format_delta_number(access.storage_footprint_bit_delta));
                    comparison_row(
                        "Non-useful storage bits",
                        detail::format_number(access.first.non_useful_storage_bits),
                        detail::format_number(access.second.non_useful_storage_bits),
                        detail::format_delta_number(access.non_useful_storage_bit_delta));
                    comparison_row("Unselected ordinary-field bits",
                                   detail::format_number(access.first.unselected_field_bits),
                                   detail::format_number(access.second.unselected_field_bits),
                                   detail::format_delta_number(access.unselected_field_bit_delta));
                    comparison_row("Explicit reserved-region bits",
                                   detail::format_number(access.first.reserved_region_bits),
                                   detail::format_number(access.second.reserved_region_bits),
                                   detail::format_delta_number(access.reserved_region_bit_delta));
                    comparison_row(
                        "Physically unused backing bits",
                        detail::format_number(access.first.physically_unused_storage_bits),
                        detail::format_number(access.second.physically_unused_storage_bits),
                        detail::format_delta_number(access.physically_unused_storage_bit_delta));
                    comparison_row("Minimum cache lines",
                                   detail::format_number(access.first.minimum_cache_lines_touched),
                                   detail::format_number(access.second.minimum_cache_lines_touched),
                                   detail::format_delta_number(access.cache_line_delta));
                    comparison_row("Minimum cache-line footprint",
                                   detail::format_bytes(access.first.minimum_cache_bytes_touched),
                                   detail::format_bytes(access.second.minimum_cache_bytes_touched),
                                   detail::format_delta_bytes(access.cache_byte_delta));
                    comparison_row("Read cache-line coverage",
                                   detail::format_bytes(access.first.read_cache_bytes_touched),
                                   detail::format_bytes(access.second.read_cache_bytes_touched),
                                   detail::format_delta_bytes(access.read_cache_byte_delta));
                    comparison_row("Write cache-line coverage",
                                   detail::format_bytes(access.first.write_cache_bytes_touched),
                                   detail::format_bytes(access.second.write_cache_bytes_touched),
                                   detail::format_delta_bytes(access.write_cache_byte_delta));
                    comparison_row(
                        "Cache footprint <= L1 data cache",
                        detail::format_fit(access.first.cache_footprint_capacity.fits_l1_data),
                        detail::format_fit(access.second.cache_footprint_capacity.fits_l1_data));
                    comparison_row(
                        "Cache footprint <= L2 cache",
                        detail::format_fit(access.first.cache_footprint_capacity.fits_l2),
                        detail::format_fit(access.second.cache_footprint_capacity.fits_l2));
                    comparison_row(
                        "Cache footprint <= L3 cache",
                        detail::format_fit(access.first.cache_footprint_capacity.fits_l3),
                        detail::format_fit(access.second.cache_footprint_capacity.fits_l3));
                    comparison_row("Minimum pages",
                                   detail::format_number(access.first.minimum_pages_touched),
                                   detail::format_number(access.second.minimum_pages_touched),
                                   detail::format_delta_number(access.page_delta));
                    comparison_row("Minimum page footprint",
                                   detail::format_bytes(access.first.minimum_page_bytes_touched),
                                   detail::format_bytes(access.second.minimum_page_bytes_touched),
                                   detail::format_delta_bytes(access.page_byte_delta));
                    comparison_row("Read page coverage",
                                   detail::format_bytes(access.first.read_page_bytes_touched),
                                   detail::format_bytes(access.second.read_page_bytes_touched),
                                   detail::format_delta_bytes(access.read_page_byte_delta));
                    comparison_row("Write page coverage",
                                   detail::format_bytes(access.first.write_page_bytes_touched),
                                   detail::format_bytes(access.second.write_page_bytes_touched),
                                   detail::format_delta_bytes(access.write_page_byte_delta));
                    ImGui::EndTable();
                }
                if (ImGui::TreeNode("Selected-field contribution breakdown")) {
                    for (auto const& field : access.fields) {
                        ImGui::PushID(field.name.c_str());
                        if (ImGui::TreeNode(field.name.c_str())) {
                            if (ImGui::BeginTable("packed-field-access-comparison",
                                                  4,
                                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                                      ImGuiTableFlags_Resizable)) {
                                ImGui::TableSetupColumn("Fact");
                                ImGui::TableSetupColumn(comparison_a.name.c_str());
                                ImGui::TableSetupColumn(comparison_b.name.c_str());
                                ImGui::TableSetupColumn("Difference");
                                ImGui::TableHeadersRow();
                                comparison_row(
                                    "Operation",
                                    detail::access_operation_name(field.first.operation),
                                    detail::access_operation_name(field.second.operation));
                                comparison_row("Physical field width",
                                               detail::format_number(std::optional<std::uint64_t>{
                                                   field.first.bit_width}),
                                               detail::format_number(std::optional<std::uint64_t>{
                                                   field.second.bit_width}),
                                               detail::format_delta_number(field.bit_width_delta));
                                comparison_row("Useful bits",
                                               detail::format_number(field.first.useful_bits),
                                               detail::format_number(field.second.useful_bits),
                                               detail::format_delta_number(field.useful_bit_delta));
                                comparison_row(
                                    "Classified read useful bits",
                                    detail::format_number(field.first.read_useful_bits),
                                    detail::format_number(field.second.read_useful_bits),
                                    detail::format_delta_number(field.read_useful_bit_delta));
                                comparison_row(
                                    "Classified write useful bits",
                                    detail::format_number(field.first.write_useful_bits),
                                    detail::format_number(field.second.write_useful_bits),
                                    detail::format_delta_number(field.write_useful_bit_delta));
                                comparison_row(
                                    "Logical read useful bits",
                                    detail::format_number(field.first.logical_read_useful_bits),
                                    detail::format_number(field.second.logical_read_useful_bits),
                                    detail::format_delta_number(
                                        field.logical_read_useful_bit_delta));
                                comparison_row(
                                    "Logical write useful bits",
                                    detail::format_number(field.first.logical_write_useful_bits),
                                    detail::format_number(field.second.logical_write_useful_bits),
                                    detail::format_delta_number(
                                        field.logical_write_useful_bit_delta));
                                ImGui::EndTable();
                            }
                            ImGui::TreePop();
                        }
                        ImGui::PopID();
                    }
                    ImGui::TreePop();
                }
                ImGui::TextDisabled(
                    "Selected fields change useful-bit accounting. Physical coverage remains the "
                    "containing packed-word array; this is not a sub-word fetch estimate.");
                draw_diagnostics(access.diagnostics);
            }
        }
        draw_diagnostics(analysis_session_.results().comparison_b_packed->diagnostics);
    } else if (analysis_session_.results().comparison_a_soa.has_value() &&
               analysis_session_.results().comparison_b_soa.has_value()) {
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
                           std::to_string(analysis_session_.results().comparison_a_soa->capacity),
                           std::to_string(analysis_session_.results().comparison_b_soa->capacity),
                           detail::format_delta_number(layout::numeric_delta(
                               analysis_session_.results().comparison_a_soa->capacity,
                               analysis_session_.results().comparison_b_soa->capacity)));
            comparison_row(
                "Bytes / logical entity",
                detail::format_bytes(
                    analysis_session_.results().comparison_a_soa->bytes_per_logical_element),
                detail::format_bytes(
                    analysis_session_.results().comparison_b_soa->bytes_per_logical_element),
                detail::format_delta_bytes(layout::numeric_delta(
                    analysis_session_.results().comparison_a_soa->bytes_per_logical_element,
                    analysis_session_.results().comparison_b_soa->bytes_per_logical_element)));
            comparison_row("Total payload",
                           detail::format_bytes(
                               analysis_session_.results().comparison_a_soa->total_payload_bytes),
                           detail::format_bytes(
                               analysis_session_.results().comparison_b_soa->total_payload_bytes),
                           detail::format_delta_bytes(layout::numeric_delta(
                               analysis_session_.results().comparison_a_soa->total_payload_bytes,
                               analysis_session_.results().comparison_b_soa->total_payload_bytes)));
            for (auto const& baseline : analysis_session_.results().comparison_a_soa->columns) {
                auto const* active{
                    column_by_name(*analysis_session_.results().comparison_b_soa, baseline.name)};
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
        if (analysis_session_.results().soa_access_comparison.has_value()) {
            auto const& access{*analysis_session_.results().soa_access_comparison};
            std::string column_names;
            for (auto const& column_name : access.column_names) {
                if (!column_names.empty()) {
                    column_names += ", ";
                }
                column_names += column_name;
            }
            if (detail::section("Selected-column workload")) {
                ImGui::Text("Elements: %llu",
                            static_cast<unsigned long long>(access.element_count));
                ImGui::Text("Operation: %s", detail::access_operation_summary(access.accesses));
                ImGui::Text("Accesses / element: %llu",
                            static_cast<unsigned long long>(access.multiplicity));
                ImGui::Text("Allocation model: %s",
                            access.allocation_strategy == SoaAllocationStrategy::separate_columns
                                ? "Separate columns (minimum footprints)"
                                : "Aligned contiguous block (exact footprints)");
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
                    comparison_row(
                        "Allocation count",
                        detail::format_number(std::optional{access.first_allocation_count}),
                        detail::format_number(std::optional{access.second_allocation_count}),
                        detail::format_delta_number(access.allocation_count_delta));
                    comparison_row(access.footprint_exact ? "Total contiguous block"
                                                          : "Minimum allocation bytes",
                                   detail::format_bytes(access.first_total_allocation_bytes),
                                   detail::format_bytes(access.second_total_allocation_bytes),
                                   detail::format_delta_bytes(access.total_allocation_byte_delta));
                    comparison_row("Alignment padding",
                                   detail::format_bytes(access.first_alignment_padding_bytes),
                                   detail::format_bytes(access.second_alignment_padding_bytes),
                                   detail::format_delta_bytes(access.alignment_padding_byte_delta));
                    comparison_row("Useful selected payload",
                                   detail::format_bytes(access.first.useful_bytes),
                                   detail::format_bytes(access.second.useful_bytes),
                                   detail::format_delta_bytes(access.useful_byte_delta));
                    comparison_row("Classified read useful payload",
                                   detail::format_bytes(access.first.read_useful_bytes),
                                   detail::format_bytes(access.second.read_useful_bytes),
                                   detail::format_delta_bytes(access.read_useful_byte_delta));
                    comparison_row("Classified write useful payload",
                                   detail::format_bytes(access.first.write_useful_bytes),
                                   detail::format_bytes(access.second.write_useful_bytes),
                                   detail::format_delta_bytes(access.write_useful_byte_delta));
                    comparison_row(
                        "Logical read useful payload",
                        detail::format_bytes(access.first.logical_read_useful_bytes),
                        detail::format_bytes(access.second.logical_read_useful_bytes),
                        detail::format_delta_bytes(access.logical_read_useful_byte_delta));
                    comparison_row(
                        "Logical write useful payload",
                        detail::format_bytes(access.first.logical_write_useful_bytes),
                        detail::format_bytes(access.second.logical_write_useful_bytes),
                        detail::format_delta_bytes(access.logical_write_useful_byte_delta));
                    comparison_row("Complete logical payload at workload count",
                                   detail::format_bytes(access.first_full_logical_payload_bytes),
                                   detail::format_bytes(access.second_full_logical_payload_bytes),
                                   detail::format_delta_bytes(access.full_logical_payload_delta));
                    comparison_row("Unselected payload at workload count",
                                   detail::format_bytes(access.first_unselected_payload_bytes),
                                   detail::format_bytes(access.second_unselected_payload_bytes),
                                   detail::format_delta_bytes(access.unselected_payload_delta));
                    auto const* const qualifier{access.footprint_exact ? "Exact" : "Minimum"};
                    auto const* const qualifier_lower{access.footprint_exact ? "exact" : "minimum"};
                    comparison_row((std::string{qualifier} + " cache lines").c_str(),
                                   detail::format_number(access.first.cache_lines),
                                   detail::format_number(access.second.cache_lines),
                                   detail::format_delta_number(access.cache_line_delta));
                    comparison_row((std::string{qualifier} + " cache-line footprint").c_str(),
                                   detail::format_bytes(access.first.cache_bytes),
                                   detail::format_bytes(access.second.cache_bytes),
                                   detail::format_delta_bytes(access.cache_byte_delta));
                    comparison_row((std::string{qualifier} + " read cache-line coverage").c_str(),
                                   detail::format_bytes(access.first.read_cache_bytes),
                                   detail::format_bytes(access.second.read_cache_bytes),
                                   detail::format_delta_bytes(access.read_cache_byte_delta));
                    comparison_row((std::string{qualifier} + " write cache-line coverage").c_str(),
                                   detail::format_bytes(access.first.write_cache_bytes),
                                   detail::format_bytes(access.second.write_cache_bytes),
                                   detail::format_delta_bytes(access.write_cache_byte_delta));
                    comparison_row((std::string{"Non-payload bytes in "} + qualifier_lower +
                                    " cache footprint")
                                       .c_str(),
                                   detail::format_bytes(access.first_non_payload_cache_bytes),
                                   detail::format_bytes(access.second_non_payload_cache_bytes),
                                   detail::format_delta_bytes(access.non_payload_cache_byte_delta));
                    comparison_row(
                        (std::string{qualifier} + " footprint <= L1 data cache").c_str(),
                        detail::format_fit(
                            access.first_minimum_cache_footprint_capacity.fits_l1_data),
                        detail::format_fit(
                            access.second_minimum_cache_footprint_capacity.fits_l1_data));
                    comparison_row(
                        (std::string{qualifier} + " footprint <= L2 cache").c_str(),
                        detail::format_fit(access.first_minimum_cache_footprint_capacity.fits_l2),
                        detail::format_fit(access.second_minimum_cache_footprint_capacity.fits_l2));
                    comparison_row(
                        (std::string{qualifier} + " footprint <= L3 cache").c_str(),
                        detail::format_fit(access.first_minimum_cache_footprint_capacity.fits_l3),
                        detail::format_fit(access.second_minimum_cache_footprint_capacity.fits_l3));
                    comparison_row((std::string{qualifier} + " pages").c_str(),
                                   detail::format_number(access.first.pages),
                                   detail::format_number(access.second.pages),
                                   detail::format_delta_number(access.page_delta));
                    comparison_row((std::string{qualifier} + " page footprint").c_str(),
                                   detail::format_bytes(access.first.page_bytes),
                                   detail::format_bytes(access.second.page_bytes),
                                   detail::format_delta_bytes(access.page_byte_delta));
                    comparison_row((std::string{qualifier} + " read page coverage").c_str(),
                                   detail::format_bytes(access.first.read_page_bytes),
                                   detail::format_bytes(access.second.read_page_bytes),
                                   detail::format_delta_bytes(access.read_page_byte_delta));
                    comparison_row((std::string{qualifier} + " write page coverage").c_str(),
                                   detail::format_bytes(access.first.write_page_bytes),
                                   detail::format_bytes(access.second.write_page_bytes),
                                   detail::format_delta_bytes(access.write_page_byte_delta));
                    comparison_row(
                        (std::string{"Non-payload bytes in "} + qualifier_lower + " page footprint")
                            .c_str(),
                        detail::format_bytes(access.first_non_payload_page_bytes),
                        detail::format_bytes(access.second_non_payload_page_bytes),
                        detail::format_delta_bytes(access.non_payload_page_byte_delta));
                    comparison_row(
                        "Allocated payload at capacity",
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
                        if (detail::section(column.name.c_str())) {
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
                                comparison_row(
                                    "Operation",
                                    detail::access_operation_name(column.first.operation),
                                    detail::access_operation_name(column.second.operation));
                                comparison_row(
                                    "Element bytes",
                                    detail::format_bytes(column.first.element_bytes),
                                    detail::format_bytes(column.second.element_bytes),
                                    detail::format_delta_bytes(column.element_byte_delta));
                                comparison_row(
                                    "Useful workload payload",
                                    detail::format_bytes(column.first.useful_bytes),
                                    detail::format_bytes(column.second.useful_bytes),
                                    detail::format_delta_bytes(column.useful_byte_delta));
                                comparison_row(
                                    "Classified read useful payload",
                                    detail::format_bytes(column.first.read_useful_bytes),
                                    detail::format_bytes(column.second.read_useful_bytes),
                                    detail::format_delta_bytes(column.read_useful_byte_delta));
                                comparison_row(
                                    "Classified write useful payload",
                                    detail::format_bytes(column.first.write_useful_bytes),
                                    detail::format_bytes(column.second.write_useful_bytes),
                                    detail::format_delta_bytes(column.write_useful_byte_delta));
                                comparison_row(
                                    "Logical read useful payload",
                                    detail::format_bytes(column.first.logical_read_useful_bytes),
                                    detail::format_bytes(column.second.logical_read_useful_bytes),
                                    detail::format_delta_bytes(
                                        column.logical_read_useful_byte_delta));
                                comparison_row(
                                    "Logical write useful payload",
                                    detail::format_bytes(column.first.logical_write_useful_bytes),
                                    detail::format_bytes(column.second.logical_write_useful_bytes),
                                    detail::format_delta_bytes(
                                        column.logical_write_useful_byte_delta));
                                comparison_row(
                                    "Minimum cache lines",
                                    detail::format_number(column.first.minimum_cache_lines),
                                    detail::format_number(column.second.minimum_cache_lines),
                                    detail::format_delta_number(column.cache_line_delta));
                                comparison_row(
                                    "Minimum cache-line footprint",
                                    detail::format_bytes(column.first.minimum_cache_bytes),
                                    detail::format_bytes(column.second.minimum_cache_bytes),
                                    detail::format_delta_bytes(column.cache_byte_delta));
                                comparison_row(
                                    "Non-payload bytes in minimum cache footprint",
                                    detail::format_bytes(column.first.non_payload_cache_bytes),
                                    detail::format_bytes(column.second.non_payload_cache_bytes),
                                    detail::format_delta_bytes(
                                        column.non_payload_cache_byte_delta));
                                comparison_row(
                                    access.footprint_exact
                                        ? "Block-offset cache-line-straddling elements"
                                        : "Aligned-base cache-line-straddling elements",
                                    detail::format_number(
                                        column.first.aligned_cache_line_straddling_elements),
                                    detail::format_number(
                                        column.second.aligned_cache_line_straddling_elements),
                                    detail::format_delta_number(
                                        column.aligned_cache_line_straddling_element_delta));
                                comparison_row("Minimum pages",
                                               detail::format_number(column.first.minimum_pages),
                                               detail::format_number(column.second.minimum_pages),
                                               detail::format_delta_number(column.page_delta));
                                comparison_row(
                                    "Minimum page footprint",
                                    detail::format_bytes(column.first.minimum_page_bytes),
                                    detail::format_bytes(column.second.minimum_page_bytes),
                                    detail::format_delta_bytes(column.page_byte_delta));
                                comparison_row(
                                    "Non-payload bytes in minimum page footprint",
                                    detail::format_bytes(column.first.non_payload_page_bytes),
                                    detail::format_bytes(column.second.non_payload_page_bytes),
                                    detail::format_delta_bytes(column.non_payload_page_byte_delta));
                                comparison_row(access.footprint_exact
                                                   ? "Block-offset page-straddling elements"
                                                   : "Aligned-base page-straddling elements",
                                               detail::format_number(
                                                   column.first.aligned_page_straddling_elements),
                                               detail::format_number(
                                                   column.second.aligned_page_straddling_elements),
                                               detail::format_delta_number(
                                                   column.aligned_page_straddling_element_delta));
                                comparison_row("Allocated payload at capacity",
                                               detail::format_bytes(
                                                   column.first.allocated_capacity_payload_bytes),
                                               detail::format_bytes(
                                                   column.second.allocated_capacity_payload_bytes),
                                               detail::format_delta_bytes(
                                                   column.allocated_capacity_payload_delta));
                                comparison_row(
                                    "Unused allocated capacity payload",
                                    detail::format_bytes(column.first.capacity_slack_payload_bytes),
                                    detail::format_bytes(
                                        column.second.capacity_slack_payload_bytes),
                                    detail::format_delta_bytes(
                                        column.capacity_slack_payload_delta));
                                ImGui::EndTable();
                            }
                        }
                        ImGui::PopID();
                    }
                    ImGui::TreePop();
                }
                ImGui::TextDisabled(
                    access.footprint_exact
                        ? "Both variants use the same explicit session workload and an aligned "
                          "contiguous block. Aggregate cache/page figures are exact under that "
                          "stated "
                          "origin; straddling uses actual column offsets. They are not measured "
                          "traffic "
                          "or performance. Logical useful totals scale by multiplicity only."
                        : "Both variants use the same explicit session workload. Cache/page "
                          "figures "
                          "are lower bounds across separate allocations; straddling assumes "
                          "aligned "
                          "column origins. They are not measured traffic or performance. Logical "
                          "useful totals scale by multiplicity only.");
                draw_diagnostics(access.diagnostics);
            }
        }
        ImGui::TextDisabled(
            analysis_session_.results().soa_access_comparison.has_value() &&
                    analysis_session_.results().soa_access_comparison->footprint_exact
                ? "Cache-line counts are exact for the stated aligned contiguous "
                  "block, not allocator traffic or a performance estimate."
                : "Cache-line counts are minimum payload coverage, not allocator "
                  "traffic or a performance estimate.");
        draw_diagnostics(analysis_session_.results().comparison_b_soa->diagnostics);
    }
    ImGui::End();
}

} // namespace ioj::layout_planner
