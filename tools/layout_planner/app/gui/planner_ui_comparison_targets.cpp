#include "../platform/file_dialog.hpp"
#include "planner_ui_comparison_common.hpp"

namespace ioj::layout_planner {

auto PlannerUi::draw_comparison_target_profile_picker() -> bool {
    ImGui::Text("A: %s", analysis_session_.primary_abi().name().c_str());
    ImGui::Text("B: %s", analysis_session_.comparison_abi().name().c_str());
    ImGui::TextWrapped("B profile: %s",
                       comparison_target_profile_path_.front() == '\0'
                           ? "None"
                           : comparison_target_profile_path_.data());

    bool changed{};
    if (ImGui::Button("Load B profile")) {
        auto chosen{file_dialog_->open_file(comparison_target_profile_path_.data(), "")};
        if (!chosen.has_value()) {
            comparison_target_profile_error_ = chosen.error();
        } else if (chosen->has_value()) {
            changed = load_comparison_target_profile(**chosen);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Use built-in B profile")) {
        use_builtin_comparison_target_profile();
        changed = true;
    }
    if (!comparison_target_profile_error_.empty()) {
        ImGui::TextColored(
            ImVec4{0.95F, 0.45F, 0.35F, 1.0F}, "%s", comparison_target_profile_error_.c_str());
    }
    return changed;
}

void PlannerUi::draw_enum_target_comparison() {
    if (detail::section("Target-profile comparison")) {
        ImGui::TextWrapped(
            "Compare one semantic enum domain and C++ backing choice under two explicit target "
            "profiles. Target A uses the shared Target Profile; target B is session-only.");
        if (draw_comparison_target_profile_picker()) {
            refresh_analysis();
        }
    }
    if (detail::section("Standalone backing scale")) {
        if (draw_element_count()) {
            return;
        }
        if (!analysis_session_.results().enum_target_comparison.has_value()) {
            ImGui::TextDisabled("No enum target comparison is available.");
            return;
        }

        auto const& comparison{*analysis_session_.results().enum_target_comparison};
        auto const& first{comparison.first};
        auto const& second{comparison.second};
        auto const first_size{
            first.backing_facts.transform([](TypeFacts const& facts) { return facts.size_bytes; })};
        auto const second_size{second.backing_facts.transform(
            [](TypeFacts const& facts) { return facts.size_bytes; })};
        auto const first_alignment{first.backing_facts.transform(
            [](TypeFacts const& facts) { return facts.alignment_bytes; })};
        auto const second_alignment{second.backing_facts.transform(
            [](TypeFacts const& facts) { return facts.alignment_bytes; })};
        auto const first_value_bits{first.backing_facts.and_then(
            [](TypeFacts const& facts) { return as_uint64(facts.unsigned_value_bits); })};
        auto const second_value_bits{second.backing_facts.and_then(
            [](TypeFacts const& facts) { return as_uint64(facts.unsigned_value_bits); })};
        auto const first_backing_signedness{first.backing_facts.and_then(
            [](TypeFacts const& facts) { return facts.integer_signed; })};
        auto const second_backing_signedness{second.backing_facts.and_then(
            [](TypeFacts const& facts) { return facts.integer_signed; })};
        auto const declared_width{[](EnumDomainAnalysis const& analysis) {
            return analysis.declared_bit_width.has_value()
                     ? std::to_string(*analysis.declared_bit_width) + " bits"
                     : std::string{"Auto"};
        }};
        auto const domain_range{[](EnumDomainAnalysis const& analysis) {
            return format_enum_code(analysis.minimum_value) + " .. " +
                   format_enum_code(analysis.maximum_value);
        }};

        if (ImGui::BeginTable("enum-target-comparison",
                              4,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("Fact");
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
                analysis_session_.comparison_abi().identity().architecture.value_or("Unknown"));
            comparison_row("ABI",
                           analysis_session_.primary_abi().identity().abi.value_or("Unknown"),
                           analysis_session_.comparison_abi().identity().abi.value_or("Unknown"));
            comparison_row("C++ backing spelling", first.backing_type, second.backing_type);
            comparison_row("Live semantic values",
                           std::to_string(first.live_value_count),
                           std::to_string(second.live_value_count));
            comparison_row("Reserved semantic values",
                           std::to_string(first.reserved_value_count),
                           std::to_string(second.reserved_value_count));
            comparison_row("Semantic value range", domain_range(first), domain_range(second));
            comparison_row("Declared semantic signedness",
                           format_signedness(first.declared_signedness),
                           format_signedness(second.declared_signedness));
            comparison_row("Resolved semantic signedness",
                           format_signedness(first.signed_domain),
                           format_signedness(second.signed_domain));
            comparison_row("Minimum semantic width",
                           detail::format_number(as_uint64(first.minimum_required_bits)),
                           detail::format_number(as_uint64(second.minimum_required_bits)));
            comparison_row(
                "Declared semantic width", declared_width(first), declared_width(second));
            comparison_row("Effective semantic width",
                           detail::format_number(as_uint64(first.effective_bit_width)),
                           detail::format_number(as_uint64(second.effective_bit_width)));
            comparison_row("Semantic width fits domain",
                           detail::format_fit(first.semantic_width_can_represent_domain),
                           detail::format_fit(second.semantic_width_can_represent_domain));
            comparison_row("Unused semantic codes",
                           detail::format_number(first.unused_semantic_codes),
                           detail::format_number(second.unused_semantic_codes));
            comparison_row("Backing physical signedness",
                           format_signedness(first_backing_signedness),
                           format_signedness(second_backing_signedness));
            comparison_row("Backing storage size",
                           detail::format_bytes(first_size),
                           detail::format_bytes(second_size),
                           detail::format_delta_bytes(comparison.backing_size_delta));
            comparison_row("Backing alignment",
                           detail::format_bytes(first_alignment),
                           detail::format_bytes(second_alignment),
                           detail::format_delta_bytes(comparison.backing_alignment_delta));
            comparison_row("Backing storage bits",
                           detail::format_number(first.backing_bits),
                           detail::format_number(second.backing_bits),
                           detail::format_delta_number(comparison.backing_bit_delta));
            comparison_row("Unsigned backing value bits",
                           detail::format_number(first_value_bits),
                           detail::format_number(second_value_bits),
                           detail::format_delta_number(comparison.backing_value_bit_delta));
            comparison_row("Backing represents domain",
                           detail::format_fit(first.backing_can_represent_domain),
                           detail::format_fit(second.backing_can_represent_domain));
            comparison_row("Unused backing codes",
                           detail::format_number(first.unused_backing_codes),
                           detail::format_number(second.unused_backing_codes),
                           detail::format_delta_number(comparison.unused_backing_code_delta));
            comparison_row("Standalone value count",
                           std::to_string(first.aggregate.element_count),
                           std::to_string(second.aggregate.element_count));
            comparison_row("Standalone backing storage",
                           detail::format_bytes(first.aggregate.total_storage_bytes),
                           detail::format_bytes(second.aggregate.total_storage_bytes),
                           detail::format_delta_bytes(comparison.total_storage_delta));
            comparison_row("Cache-line size",
                           detail::format_bytes(first.aggregate.cache_line_bytes),
                           detail::format_bytes(second.aggregate.cache_line_bytes),
                           detail::format_delta_bytes(comparison.cache_line_size_delta));
            comparison_row("Minimum cache lines",
                           detail::format_number(first.aggregate.minimum_cache_lines),
                           detail::format_number(second.aggregate.minimum_cache_lines),
                           detail::format_delta_number(comparison.minimum_cache_line_delta));
            comparison_row(
                "Complete standalone values / cache line",
                detail::format_number(first.aggregate.complete_elements_per_cache_line),
                detail::format_number(second.aggregate.complete_elements_per_cache_line),
                detail::format_delta_number(comparison.complete_elements_per_cache_line_delta));
            comparison_row("Cache-line-straddling values",
                           detail::format_number(first.aggregate.cache_line_straddling_elements),
                           detail::format_number(second.aggregate.cache_line_straddling_elements),
                           detail::format_delta_number(comparison.cache_line_straddling_delta));
            comparison_row("Standalone footprint <= L1 data cache",
                           detail::format_fit(first.aggregate.cache_capacity.fits_l1_data),
                           detail::format_fit(second.aggregate.cache_capacity.fits_l1_data));
            comparison_row("Standalone footprint <= L2 cache",
                           detail::format_fit(first.aggregate.cache_capacity.fits_l2),
                           detail::format_fit(second.aggregate.cache_capacity.fits_l2));
            comparison_row("Standalone footprint <= L3 cache",
                           detail::format_fit(first.aggregate.cache_capacity.fits_l3),
                           detail::format_fit(second.aggregate.cache_capacity.fits_l3));
            comparison_row("Page size",
                           detail::format_bytes(first.aggregate.page_bytes),
                           detail::format_bytes(second.aggregate.page_bytes),
                           detail::format_delta_bytes(comparison.page_size_delta));
            comparison_row("Minimum pages",
                           detail::format_number(first.aggregate.minimum_pages),
                           detail::format_number(second.aggregate.minimum_pages),
                           detail::format_delta_number(comparison.minimum_page_delta));
            comparison_row(
                "Complete standalone values / page",
                detail::format_number(first.aggregate.complete_elements_per_page),
                detail::format_number(second.aggregate.complete_elements_per_page),
                detail::format_delta_number(comparison.complete_elements_per_page_delta));
            comparison_row("Page-straddling values",
                           detail::format_number(first.aggregate.page_straddling_elements),
                           detail::format_number(second.aggregate.page_straddling_elements),
                           detail::format_delta_number(comparison.page_straddling_delta));
            ImGui::EndTable();
        }

        ImGui::TextDisabled(
            "Semantic width and unused semantic codes describe the value domain, not a standalone "
            "sizeof. Aggregate rows model a contiguous array of the generated standalone C++ "
            "backing "
            "at a cache-line/page-aligned origin; they do not imply bit-packed enum storage. "
            "Unused "
            "codes are code-space capacity, not allocated-byte waste or a performance prediction.");
        draw_diagnostics(comparison.diagnostics);
    }
}

auto PlannerUi::draw_soa_target_comparison() -> bool {
    if (detail::section("Target-profile comparison")) {
        ImGui::TextWrapped(
            "Compare the active physical SoA variant under two explicit target profiles. Capacity, "
            "allocation strategy, selected workload, and multiplicity are held fixed. Target A "
            "uses the shared Target Profile; target B is session-only.");
        ImGui::Text("Held physical variant: %s",
                    analysis_session_.inputs.workspace.active_variant().name.c_str());
        if (draw_comparison_target_profile_picker()) {
            refresh_analysis();
        }
    }
    if (detail::section("Selected workload scale")) {
        if (draw_element_count()) {
            return true;
        }

        if (!analysis_session_.results().soa_target_comparison.has_value()) {
            ImGui::TextDisabled("No SoA target comparison is available.");
            return false;
        }

        auto const& comparison{*analysis_session_.results().soa_target_comparison};
        auto const& first{comparison.first};
        auto const& second{comparison.second};
        auto const allocation_strategy_name = [](SoaAllocationStrategy const strategy) {
            return strategy == SoaAllocationStrategy::aligned_contiguous
                     ? "Aligned contiguous block"
                     : "Separate columns";
        };
        if (ImGui::BeginTable("soa-target-comparison",
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
                analysis_session_.primary_abi().identity().architecture.value_or("Unknown"),
                analysis_session_.comparison_abi().identity().architecture.value_or("Unknown"));
            comparison_row(
                "Capacity", std::to_string(first.capacity), std::to_string(second.capacity));
            comparison_row(
                "Capacity source",
                first.capacity_overridden ? "Active variant override" : "Session default",
                second.capacity_overridden ? "Active variant override" : "Session default");
            comparison_row("Allocation strategy",
                           allocation_strategy_name(first.allocation_strategy),
                           allocation_strategy_name(second.allocation_strategy));
            comparison_row("Allocation count",
                           std::to_string(first.allocation_count),
                           std::to_string(second.allocation_count),
                           detail::format_delta_number(comparison.allocation_count_delta));
            comparison_row("Bytes / logical entity",
                           detail::format_bytes(first.bytes_per_logical_element),
                           detail::format_bytes(second.bytes_per_logical_element),
                           detail::format_delta_bytes(comparison.bytes_per_logical_element_delta));
            comparison_row("Payload at capacity",
                           detail::format_bytes(first.total_payload_bytes),
                           detail::format_bytes(second.total_payload_bytes),
                           detail::format_delta_bytes(comparison.total_payload_delta));
            comparison_row("Allocated bytes",
                           detail::format_bytes(first.total_allocation_bytes),
                           detail::format_bytes(second.total_allocation_bytes),
                           detail::format_delta_bytes(comparison.total_allocation_delta));
            comparison_row("Alignment padding",
                           detail::format_bytes(first.total_alignment_padding_bytes),
                           detail::format_bytes(second.total_alignment_padding_bytes),
                           detail::format_delta_bytes(comparison.total_alignment_padding_delta));
            comparison_row("Block alignment",
                           detail::format_bytes(first.allocation_alignment_bytes),
                           detail::format_bytes(second.allocation_alignment_bytes),
                           detail::format_delta_bytes(comparison.allocation_alignment_delta));
            comparison_row("Cache-line size",
                           detail::format_bytes(first.cache_line_bytes),
                           detail::format_bytes(second.cache_line_bytes),
                           detail::format_delta_bytes(comparison.cache_line_size_delta));
            comparison_row("Page size",
                           detail::format_bytes(first.page_bytes),
                           detail::format_bytes(second.page_bytes),
                           detail::format_delta_bytes(comparison.page_size_delta));
            comparison_row("Minimum pages at capacity",
                           detail::format_number(first.minimum_pages),
                           detail::format_number(second.minimum_pages),
                           detail::format_delta_number(comparison.minimum_page_delta));
            comparison_row("Allocated footprint <= L1 data cache",
                           detail::format_fit(first.cache_capacity.fits_l1_data),
                           detail::format_fit(second.cache_capacity.fits_l1_data));
            comparison_row("Allocated footprint <= L2 cache",
                           detail::format_fit(first.cache_capacity.fits_l2),
                           detail::format_fit(second.cache_capacity.fits_l2));
            comparison_row("Allocated footprint <= L3 cache",
                           detail::format_fit(first.cache_capacity.fits_l3),
                           detail::format_fit(second.cache_capacity.fits_l3));
            ImGui::EndTable();
        }

        if (!comparison.columns.empty() &&
            ImGui::TreeNodeEx("Column layout", ImGuiTreeNodeFlags_SpanAvailWidth)) {
            for (auto const& column : comparison.columns) {
                ImGui::PushID(column.name.c_str());
                if (detail::section(column.name.c_str())) {
                    auto const first_size{column.first.type_facts.transform(
                        [](TypeFacts const& facts) { return facts.size_bytes; })};
                    auto const second_size{column.second.type_facts.transform(
                        [](TypeFacts const& facts) { return facts.size_bytes; })};
                    auto const first_alignment{column.first.type_facts.transform(
                        [](TypeFacts const& facts) { return facts.alignment_bytes; })};
                    auto const second_alignment{column.second.type_facts.transform(
                        [](TypeFacts const& facts) { return facts.alignment_bytes; })};
                    if (ImGui::BeginTable("soa-column-target-comparison",
                                          4,
                                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                              ImGuiTableFlags_Resizable)) {
                        ImGui::TableSetupColumn("Physical fact");
                        ImGui::TableSetupColumn(analysis_session_.primary_abi().name().c_str());
                        ImGui::TableSetupColumn(analysis_session_.comparison_abi().name().c_str());
                        ImGui::TableSetupColumn("Difference (B - A)");
                        ImGui::TableHeadersRow();
                        comparison_row("Semantic type",
                                       type_label(analysis_session_.inputs.workspace.types().type(
                                           column.first.semantic_type)),
                                       type_label(analysis_session_.inputs.workspace.types().type(
                                           column.second.semantic_type)));
                        comparison_row("Physical type",
                                       column.first.physical_type,
                                       column.second.physical_type);
                        comparison_row("Element size",
                                       detail::format_bytes(first_size),
                                       detail::format_bytes(second_size),
                                       detail::format_delta_bytes(column.element_size_delta));
                        comparison_row("Element alignment",
                                       detail::format_bytes(first_alignment),
                                       detail::format_bytes(second_alignment),
                                       detail::format_delta_bytes(column.element_alignment_delta));
                        comparison_row("Payload at capacity",
                                       detail::format_bytes(column.first.total_bytes),
                                       detail::format_bytes(column.second.total_bytes),
                                       detail::format_delta_bytes(column.total_byte_delta));
                        comparison_row("Block offset",
                                       detail::format_bytes(column.first.allocation_offset_bytes),
                                       detail::format_bytes(column.second.allocation_offset_bytes),
                                       detail::format_delta_bytes(column.allocation_offset_delta));
                        comparison_row("Padding before",
                                       detail::format_bytes(column.first.padding_before_bytes),
                                       detail::format_bytes(column.second.padding_before_bytes),
                                       detail::format_delta_bytes(column.padding_before_delta));
                        comparison_row(
                            "Minimum cache lines at capacity",
                            detail::format_number(column.first.minimum_cache_lines),
                            detail::format_number(column.second.minimum_cache_lines),
                            detail::format_delta_number(column.minimum_cache_line_delta));
                        comparison_row(
                            "Complete elements per cache line",
                            detail::format_number(column.first.elements_per_cache_line),
                            detail::format_number(column.second.elements_per_cache_line),
                            detail::format_delta_number(column.elements_per_cache_line_delta));
                        comparison_row("Minimum pages at capacity",
                                       detail::format_number(column.first.minimum_pages),
                                       detail::format_number(column.second.minimum_pages),
                                       detail::format_delta_number(column.minimum_page_delta));
                        comparison_row(
                            "Complete elements per page",
                            detail::format_number(column.first.complete_elements_per_page),
                            detail::format_number(column.second.complete_elements_per_page),
                            detail::format_delta_number(column.complete_elements_per_page_delta));
                        ImGui::EndTable();
                    }
                }
                ImGui::PopID();
            }
            ImGui::TreePop();
        }

        if (analysis_session_.results().soa_target_access_comparison.has_value()) {
            auto const& access{*analysis_session_.results().soa_target_access_comparison};
            std::string selected_columns;
            for (auto const& column_name : access.column_names) {
                if (!selected_columns.empty()) {
                    selected_columns += ", ";
                }
                selected_columns += column_name;
            }
            if (detail::section("Selected workload across targets")) {
                ImGui::Text("Columns: %s", selected_columns.c_str());
                ImGui::Text("Elements: %llu   Multiplicity: %llu",
                            static_cast<unsigned long long>(access.element_count),
                            static_cast<unsigned long long>(access.multiplicity));
                auto const* const footprint_qualifier{access.footprint_exact ? "Exact" : "Minimum"};
                if (ImGui::BeginTable("soa-target-access-comparison",
                                      4,
                                      ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                          ImGuiTableFlags_Resizable)) {
                    ImGui::TableSetupColumn("Workload fact");
                    ImGui::TableSetupColumn(analysis_session_.primary_abi().name().c_str());
                    ImGui::TableSetupColumn(analysis_session_.comparison_abi().name().c_str());
                    ImGui::TableSetupColumn("Difference (B - A)");
                    ImGui::TableHeadersRow();
                    comparison_row("Useful payload",
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
                    comparison_row("Complete workload payload",
                                   detail::format_bytes(access.first_full_logical_payload_bytes),
                                   detail::format_bytes(access.second_full_logical_payload_bytes),
                                   detail::format_delta_bytes(access.full_logical_payload_delta));
                    comparison_row("Unselected workload payload",
                                   detail::format_bytes(access.first_unselected_payload_bytes),
                                   detail::format_bytes(access.second_unselected_payload_bytes),
                                   detail::format_delta_bytes(access.unselected_payload_delta));
                    comparison_row(
                        "Allocated payload at capacity",
                        detail::format_bytes(access.first_allocated_capacity_payload_bytes),
                        detail::format_bytes(access.second_allocated_capacity_payload_bytes),
                        detail::format_delta_bytes(access.allocated_capacity_payload_delta));
                    comparison_row("Unused allocated capacity payload",
                                   detail::format_bytes(access.first_capacity_slack_payload_bytes),
                                   detail::format_bytes(access.second_capacity_slack_payload_bytes),
                                   detail::format_delta_bytes(access.capacity_slack_payload_delta));
                    comparison_row("Total allocation",
                                   detail::format_bytes(access.first_total_allocation_bytes),
                                   detail::format_bytes(access.second_total_allocation_bytes),
                                   detail::format_delta_bytes(access.total_allocation_byte_delta));
                    comparison_row("Alignment padding",
                                   detail::format_bytes(access.first_alignment_padding_bytes),
                                   detail::format_bytes(access.second_alignment_padding_bytes),
                                   detail::format_delta_bytes(access.alignment_padding_byte_delta));
                    comparison_row(
                        (std::string{footprint_qualifier} + " cache-line footprint").c_str(),
                        detail::format_bytes(access.first.cache_bytes),
                        detail::format_bytes(access.second.cache_bytes),
                        detail::format_delta_bytes(access.cache_byte_delta));
                    comparison_row((std::string{footprint_qualifier} + " cache lines").c_str(),
                                   detail::format_number(access.first.cache_lines),
                                   detail::format_number(access.second.cache_lines),
                                   detail::format_delta_number(access.cache_line_delta));
                    comparison_row(
                        (std::string{footprint_qualifier} + " read cache coverage").c_str(),
                        detail::format_bytes(access.first.read_cache_bytes),
                        detail::format_bytes(access.second.read_cache_bytes),
                        detail::format_delta_bytes(access.read_cache_byte_delta));
                    comparison_row(
                        (std::string{footprint_qualifier} + " write cache coverage").c_str(),
                        detail::format_bytes(access.first.write_cache_bytes),
                        detail::format_bytes(access.second.write_cache_bytes),
                        detail::format_delta_bytes(access.write_cache_byte_delta));
                    comparison_row("Non-payload bytes in cache footprint",
                                   detail::format_bytes(access.first_non_payload_cache_bytes),
                                   detail::format_bytes(access.second_non_payload_cache_bytes),
                                   detail::format_delta_bytes(access.non_payload_cache_byte_delta));
                    comparison_row((std::string{footprint_qualifier} + " page footprint").c_str(),
                                   detail::format_bytes(access.first.page_bytes),
                                   detail::format_bytes(access.second.page_bytes),
                                   detail::format_delta_bytes(access.page_byte_delta));
                    comparison_row((std::string{footprint_qualifier} + " pages").c_str(),
                                   detail::format_number(access.first.pages),
                                   detail::format_number(access.second.pages),
                                   detail::format_delta_number(access.page_delta));
                    comparison_row(
                        (std::string{footprint_qualifier} + " read page coverage").c_str(),
                        detail::format_bytes(access.first.read_page_bytes),
                        detail::format_bytes(access.second.read_page_bytes),
                        detail::format_delta_bytes(access.read_page_byte_delta));
                    comparison_row(
                        (std::string{footprint_qualifier} + " write page coverage").c_str(),
                        detail::format_bytes(access.first.write_page_bytes),
                        detail::format_bytes(access.second.write_page_bytes),
                        detail::format_delta_bytes(access.write_page_byte_delta));
                    comparison_row("Non-payload bytes in page footprint",
                                   detail::format_bytes(access.first_non_payload_page_bytes),
                                   detail::format_bytes(access.second_non_payload_page_bytes),
                                   detail::format_delta_bytes(access.non_payload_page_byte_delta));
                    comparison_row(
                        "Cache footprint <= L1 data cache",
                        detail::format_fit(
                            access.first_minimum_cache_footprint_capacity.fits_l1_data),
                        detail::format_fit(
                            access.second_minimum_cache_footprint_capacity.fits_l1_data));
                    comparison_row(
                        "Cache footprint <= L2 cache",
                        detail::format_fit(access.first_minimum_cache_footprint_capacity.fits_l2),
                        detail::format_fit(access.second_minimum_cache_footprint_capacity.fits_l2));
                    comparison_row(
                        "Cache footprint <= L3 cache",
                        detail::format_fit(access.first_minimum_cache_footprint_capacity.fits_l3),
                        detail::format_fit(access.second_minimum_cache_footprint_capacity.fits_l3));
                    ImGui::EndTable();
                }

                if (!access.columns.empty() &&
                    ImGui::TreeNode("Per-column workload contributions")) {
                    for (auto const& column : access.columns) {
                        ImGui::PushID(column.name.c_str());
                        if (detail::section(column.name.c_str())) {
                            if (ImGui::BeginTable("soa-target-access-column-comparison",
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
                                    "Minimum cache-line footprint",
                                    detail::format_bytes(column.first.minimum_cache_bytes),
                                    detail::format_bytes(column.second.minimum_cache_bytes),
                                    detail::format_delta_bytes(column.cache_byte_delta));
                                comparison_row(
                                    "Non-payload bytes in cache footprint",
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
                                comparison_row(
                                    "Minimum page footprint",
                                    detail::format_bytes(column.first.minimum_page_bytes),
                                    detail::format_bytes(column.second.minimum_page_bytes),
                                    detail::format_delta_bytes(column.page_byte_delta));
                                comparison_row(
                                    "Non-payload bytes in page footprint",
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
                draw_diagnostics(access.diagnostics);
            }
        } else {
            ImGui::TextDisabled(
                "Select one or more SoA columns in Layout to compare a workload across targets.");
        }

        ImGui::TextDisabled(
            "Target comparison holds semantic data, active physical variant, capacity, allocation, "
            "and workload fixed. Cache/page values are deterministic coverage facts under the "
            "stated "
            "allocation model, not measured traffic or a performance prediction.");
        draw_diagnostics(comparison.diagnostics);
    }
    return false;
}

void PlannerUi::draw_union_target_comparison() {
    ImGui::TextWrapped(
        "Compare one semantic raw union under two explicit physical target profiles. Target A "
        "uses the shared Target Profile; target B is session-only.");
    if (draw_comparison_target_profile_picker()) {
        refresh_analysis();
    }
    if (detail::section("Analysis scale")) {
        if (draw_element_count()) {
            return;
        }
        if (!analysis_session_.results().union_target_comparison.has_value()) {
            ImGui::TextDisabled("No raw-union target comparison is available.");
            return;
        }

        auto const& comparison{*analysis_session_.results().union_target_comparison};
        auto const& first{comparison.first};
        auto const& second{comparison.second};
        auto const& first_aggregate{first.aggregate};
        auto const& second_aggregate{second.aggregate};
        if (ImGui::BeginTable("union-target-comparison",
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
                analysis_session_.primary_abi().identity().architecture.value_or("Unknown"),
                analysis_session_.comparison_abi().identity().architecture.value_or("Unknown"));
            comparison_row("Element count",
                           std::to_string(first_aggregate.element_count),
                           std::to_string(second_aggregate.element_count));
            comparison_row("Largest alternative extent",
                           detail::format_bytes(first.largest_alternative_bytes),
                           detail::format_bytes(second.largest_alternative_bytes),
                           detail::format_delta_bytes(comparison.largest_alternative_delta));
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
            comparison_row("Aggregate tail padding",
                           detail::format_bytes(first_aggregate.total_tail_padding_bytes),
                           detail::format_bytes(second_aggregate.total_tail_padding_bytes),
                           detail::format_delta_bytes(comparison.total_tail_padding_delta));
            comparison_row("Cache-line size",
                           detail::format_bytes(first_aggregate.cache_line_bytes),
                           detail::format_bytes(second_aggregate.cache_line_bytes),
                           detail::format_delta_bytes(comparison.cache_line_size_delta));
            comparison_row("Minimum cache lines",
                           detail::format_number(first_aggregate.minimum_cache_lines),
                           detail::format_number(second_aggregate.minimum_cache_lines),
                           detail::format_delta_number(comparison.minimum_cache_line_delta));
            comparison_row(
                "Complete elements per cache line",
                detail::format_number(first_aggregate.complete_elements_per_cache_line),
                detail::format_number(second_aggregate.complete_elements_per_cache_line),
                detail::format_delta_number(comparison.complete_elements_per_cache_line_delta));
            comparison_row("Cache-line-straddling elements",
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
            comparison_row("Fits L1 data cache",
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

        if (!comparison.alternatives.empty() &&
            ImGui::TreeNodeEx("Alternative layout", ImGuiTreeNodeFlags_SpanAvailWidth)) {
            for (auto const& alternative : comparison.alternatives) {
                ImGui::PushID(alternative.name.c_str());
                if (detail::section(alternative.name.c_str())) {
                    auto const first_size{alternative.first.element_facts.transform(
                        [](TypeFacts const& facts) { return facts.size_bytes; })};
                    auto const second_size{alternative.second.element_facts.transform(
                        [](TypeFacts const& facts) { return facts.size_bytes; })};
                    auto const first_alignment{alternative.first.element_facts.transform(
                        [](TypeFacts const& facts) { return facts.alignment_bytes; })};
                    auto const second_alignment{alternative.second.element_facts.transform(
                        [](TypeFacts const& facts) { return facts.alignment_bytes; })};
                    if (ImGui::BeginTable("union-alternative-target-comparison",
                                          4,
                                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                              ImGuiTableFlags_Resizable)) {
                        ImGui::TableSetupColumn("Physical fact");
                        ImGui::TableSetupColumn(analysis_session_.primary_abi().name().c_str());
                        ImGui::TableSetupColumn(analysis_session_.comparison_abi().name().c_str());
                        ImGui::TableSetupColumn("Difference (B - A)");
                        ImGui::TableHeadersRow();
                        comparison_row("Semantic type",
                                       type_label(analysis_session_.inputs.workspace.types().type(
                                           alternative.first.semantic_type)),
                                       type_label(analysis_session_.inputs.workspace.types().type(
                                           alternative.second.semantic_type)));
                        comparison_row("Element count",
                                       std::to_string(alternative.first.element_count),
                                       std::to_string(alternative.second.element_count));
                        comparison_row("Element size",
                                       detail::format_bytes(first_size),
                                       detail::format_bytes(second_size),
                                       detail::format_delta_bytes(alternative.element_size_delta));
                        comparison_row(
                            "Element alignment",
                            detail::format_bytes(first_alignment),
                            detail::format_bytes(second_alignment),
                            detail::format_delta_bytes(alternative.element_alignment_delta));
                        comparison_row("Alternative extent",
                                       detail::format_bytes(alternative.first.extent_bytes),
                                       detail::format_bytes(alternative.second.extent_bytes),
                                       detail::format_delta_bytes(alternative.extent_delta));
                        comparison_row("Conditional slack per object",
                                       detail::format_bytes(alternative.first.slack_bytes),
                                       detail::format_bytes(alternative.second.slack_bytes),
                                       detail::format_delta_bytes(alternative.slack_delta));
                        comparison_row("Conditional slack at selected count",
                                       detail::format_bytes(alternative.first.total_slack_bytes),
                                       detail::format_bytes(alternative.second.total_slack_bytes),
                                       detail::format_delta_bytes(alternative.total_slack_delta));
                        ImGui::EndTable();
                    }
                }
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
        if (analysis_session_.results().union_target_distribution_comparison.has_value()) {
            auto const& distribution{
                *analysis_session_.results().union_target_distribution_comparison};
            if (detail::section("Explicit workload across targets")) {
                if (ImGui::BeginTable("raw-union-distribution-target-comparison",
                                      4,
                                      ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                          ImGuiTableFlags_Resizable)) {
                    ImGui::TableSetupColumn("Distribution fact");
                    ImGui::TableSetupColumn(analysis_session_.primary_abi().name().c_str());
                    ImGui::TableSetupColumn(analysis_session_.comparison_abi().name().c_str());
                    ImGui::TableSetupColumn("Difference (B - A)");
                    ImGui::TableHeadersRow();
                    comparison_row("Total weight",
                                   detail::format_number(distribution.first.total_weight),
                                   detail::format_number(distribution.second.total_weight),
                                   detail::format_delta_number(distribution.total_weight_delta));
                    comparison_row("Weighted active extent",
                                   detail::format_bytes(distribution.first.total_extent_bytes),
                                   detail::format_bytes(distribution.second.total_extent_bytes),
                                   detail::format_delta_bytes(distribution.total_extent_delta));
                    comparison_row("Weighted conditional slack",
                                   detail::format_bytes(distribution.first.total_slack_bytes),
                                   detail::format_bytes(distribution.second.total_slack_bytes),
                                   detail::format_delta_bytes(distribution.total_slack_delta));
                    comparison_row(
                        "Expected active extent / value",
                        format_optional_decimal(distribution.first.expected_extent_bytes_per_value),
                        format_optional_decimal(
                            distribution.second.expected_extent_bytes_per_value),
                        format_decimal_delta(distribution.expected_extent_per_value_delta));
                    comparison_row(
                        "Expected conditional slack / value",
                        format_optional_decimal(distribution.first.expected_slack_bytes_per_value),
                        format_optional_decimal(distribution.second.expected_slack_bytes_per_value),
                        format_decimal_delta(distribution.expected_slack_per_value_delta));
                    comparison_row(
                        "Expected active extent at selected count",
                        format_optional_decimal(distribution.first.expected_selected_extent_bytes),
                        format_optional_decimal(distribution.second.expected_selected_extent_bytes),
                        format_decimal_delta(distribution.expected_selected_extent_delta));
                    comparison_row(
                        "Expected conditional slack at selected count",
                        format_optional_decimal(distribution.first.expected_selected_slack_bytes),
                        format_optional_decimal(distribution.second.expected_selected_slack_bytes),
                        format_decimal_delta(distribution.expected_selected_slack_delta));
                    ImGui::EndTable();
                }
                if (!distribution.entries.empty() && ImGui::TreeNode("Workload alternatives")) {
                    for (auto const& entry : distribution.entries) {
                        ImGui::PushID(entry.alternative_name.c_str());
                        if (detail::section(entry.alternative_name.c_str())) {
                            if (ImGui::BeginTable("raw-union-distribution-entry-target-comparison",
                                                  4,
                                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                                      ImGuiTableFlags_Resizable)) {
                                ImGui::TableSetupColumn("Distribution fact");
                                ImGui::TableSetupColumn(
                                    analysis_session_.primary_abi().name().c_str());
                                ImGui::TableSetupColumn(
                                    analysis_session_.comparison_abi().name().c_str());
                                ImGui::TableSetupColumn("Difference (B - A)");
                                ImGui::TableHeadersRow();
                                comparison_row("Weight",
                                               std::to_string(entry.first.weight),
                                               std::to_string(entry.second.weight));
                                comparison_row("Active extent",
                                               detail::format_bytes(entry.first.extent_bytes),
                                               detail::format_bytes(entry.second.extent_bytes),
                                               detail::format_delta_bytes(entry.extent_delta));
                                comparison_row("Conditional slack",
                                               detail::format_bytes(entry.first.slack_bytes),
                                               detail::format_bytes(entry.second.slack_bytes),
                                               detail::format_delta_bytes(entry.slack_delta));
                                comparison_row(
                                    "Weighted active extent",
                                    detail::format_bytes(entry.first.weighted_extent_bytes),
                                    detail::format_bytes(entry.second.weighted_extent_bytes),
                                    detail::format_delta_bytes(entry.weighted_extent_delta));
                                comparison_row(
                                    "Weighted conditional slack",
                                    detail::format_bytes(entry.first.weighted_slack_bytes),
                                    detail::format_bytes(entry.second.weighted_slack_bytes),
                                    detail::format_delta_bytes(entry.weighted_slack_delta));
                                ImGui::EndTable();
                            }
                        }
                        ImGui::PopID();
                    }
                    ImGui::TreePop();
                }
                draw_diagnostics(distribution.diagnostics);
            }
        }
        ImGui::TextDisabled(
            "Alternative slack without a workload assumes every object uses that alternative. The "
            "optional session workload is conditional, stores no discriminant, and is not a "
            "performance prediction or an ABI-compatibility verdict.");
        draw_diagnostics(comparison.diagnostics);
    }
}

void PlannerUi::draw_tagged_union_target_comparison() {
    ImGui::TextWrapped(
        "Compare one semantic tagged union under two explicit physical target profiles. The "
        "discriminant, tag roles, alternatives, and selected count are held fixed. Target A "
        "uses the shared Target Profile; target B is session-only.");
    if (draw_comparison_target_profile_picker()) {
        refresh_analysis();
    }
    if (!analysis_session_.results().tagged_union_target_comparison.has_value()) {
        ImGui::TextDisabled("No tagged-union target comparison is available.");
        return;
    }

    auto const& comparison{*analysis_session_.results().tagged_union_target_comparison};
    auto const& first{comparison.first};
    auto const& second{comparison.second};
    if (detail::section("Analysis scale")) {
        if (draw_element_count()) {
            return;
        }
        auto const& first_aggregate{first.aggregate};
        auto const& second_aggregate{second.aggregate};
        auto const first_discriminant_size{first.discriminant_facts.transform(
            [](TypeFacts const& facts) { return facts.size_bytes; })};
        auto const second_discriminant_size{second.discriminant_facts.transform(
            [](TypeFacts const& facts) { return facts.size_bytes; })};
        auto const first_discriminant_alignment{first.discriminant_facts.transform(
            [](TypeFacts const& facts) { return facts.alignment_bytes; })};
        auto const second_discriminant_alignment{second.discriminant_facts.transform(
            [](TypeFacts const& facts) { return facts.alignment_bytes; })};
        if (ImGui::BeginTable("tagged-union-target-comparison",
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
            comparison_row("Element count",
                           std::to_string(first_aggregate.element_count),
                           std::to_string(second_aggregate.element_count));
            comparison_row("Discriminant semantic type",
                           type_label(analysis_session_.inputs.workspace.types().type(
                               first.discriminant_type)),
                           type_label(analysis_session_.inputs.workspace.types().type(
                               second.discriminant_type)));
            comparison_row("Discriminant size",
                           detail::format_bytes(first_discriminant_size),
                           detail::format_bytes(second_discriminant_size),
                           detail::format_delta_bytes(comparison.discriminant_size_delta));
            comparison_row("Discriminant alignment",
                           detail::format_bytes(first_discriminant_alignment),
                           detail::format_bytes(second_discriminant_alignment),
                           detail::format_delta_bytes(comparison.discriminant_alignment_delta));
            comparison_row("Largest alternative extent",
                           detail::format_bytes(first.largest_alternative_bytes),
                           detail::format_bytes(second.largest_alternative_bytes),
                           detail::format_delta_bytes(comparison.largest_alternative_delta));
            comparison_row("Payload union size",
                           detail::format_bytes(first.payload_size_bytes),
                           detail::format_bytes(second.payload_size_bytes),
                           detail::format_delta_bytes(comparison.payload_size_delta));
            comparison_row("Payload alignment",
                           detail::format_bytes(first.payload_alignment_bytes),
                           detail::format_bytes(second.payload_alignment_bytes),
                           detail::format_delta_bytes(comparison.payload_alignment_delta));
            comparison_row("Payload offset",
                           detail::format_bytes(first.payload_offset_bytes),
                           detail::format_bytes(second.payload_offset_bytes),
                           detail::format_delta_bytes(comparison.payload_offset_delta));
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
            comparison_row("Aggregate discriminant bytes",
                           detail::format_bytes(first_aggregate.total_discriminant_bytes),
                           detail::format_bytes(second_aggregate.total_discriminant_bytes),
                           detail::format_delta_bytes(comparison.total_discriminant_delta));
            comparison_row("Aggregate payload bytes",
                           detail::format_bytes(first_aggregate.total_payload_bytes),
                           detail::format_bytes(second_aggregate.total_payload_bytes),
                           detail::format_delta_bytes(comparison.total_payload_delta));
            comparison_row("Aggregate padding",
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
            comparison_row("Minimum cache lines",
                           detail::format_number(first_aggregate.minimum_cache_lines),
                           detail::format_number(second_aggregate.minimum_cache_lines),
                           detail::format_delta_number(comparison.minimum_cache_line_delta));
            comparison_row(
                "Complete elements per cache line",
                detail::format_number(first_aggregate.complete_elements_per_cache_line),
                detail::format_number(second_aggregate.complete_elements_per_cache_line),
                detail::format_delta_number(comparison.complete_elements_per_cache_line_delta));
            comparison_row("Cache-line-straddling elements",
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
            comparison_row("Fits L1 data cache",
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
    }
    if (detail::section("Semantic tag coverage (held fixed)")) {
        auto join_tags = [](std::vector<std::string> const& tags) {
            std::string joined;
            for (auto const& tag : tags) {
                if (!joined.empty()) {
                    joined += ", ";
                }
                joined += tag;
            }
            return joined.empty() ? std::string{"None"} : joined;
        };
        ImGui::TextWrapped("Mapped live: %s", join_tags(first.mapped_live_tags).c_str());
        ImGui::TextWrapped("Unmapped live: %s", join_tags(first.unmapped_live_tags).c_str());
        ImGui::TextWrapped("Sentinels: %s", join_tags(first.sentinel_tags).c_str());
        ImGui::Text("Count sentinel: %s",
                    first.count_sentinel_tag.has_value() ? first.count_sentinel_tag->c_str()
                                                         : "None");

        if (!comparison.alternatives.empty() &&
            ImGui::TreeNodeEx("Alternative payload layout", ImGuiTreeNodeFlags_SpanAvailWidth)) {
            for (auto const& alternative : comparison.alternatives) {
                ImGui::PushID(alternative.name.c_str());
                if (detail::section(alternative.name.c_str())) {
                    auto const first_size{alternative.first.element_facts.transform(
                        [](TypeFacts const& facts) { return facts.size_bytes; })};
                    auto const second_size{alternative.second.element_facts.transform(
                        [](TypeFacts const& facts) { return facts.size_bytes; })};
                    auto const first_alignment{alternative.first.element_facts.transform(
                        [](TypeFacts const& facts) { return facts.alignment_bytes; })};
                    auto const second_alignment{alternative.second.element_facts.transform(
                        [](TypeFacts const& facts) { return facts.alignment_bytes; })};
                    if (ImGui::BeginTable("tagged-alternative-target-comparison",
                                          4,
                                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                              ImGuiTableFlags_Resizable)) {
                        ImGui::TableSetupColumn("Physical fact");
                        ImGui::TableSetupColumn(analysis_session_.primary_abi().name().c_str());
                        ImGui::TableSetupColumn(analysis_session_.comparison_abi().name().c_str());
                        ImGui::TableSetupColumn("Difference (B - A)");
                        ImGui::TableHeadersRow();
                        comparison_row("Tag", alternative.first.tag, alternative.second.tag);
                        comparison_row("Semantic type",
                                       type_label(analysis_session_.inputs.workspace.types().type(
                                           alternative.first.semantic_type)),
                                       type_label(analysis_session_.inputs.workspace.types().type(
                                           alternative.second.semantic_type)));
                        comparison_row("Element count",
                                       std::to_string(alternative.first.element_count),
                                       std::to_string(alternative.second.element_count));
                        comparison_row("Element size",
                                       detail::format_bytes(first_size),
                                       detail::format_bytes(second_size),
                                       detail::format_delta_bytes(alternative.element_size_delta));
                        comparison_row(
                            "Element alignment",
                            detail::format_bytes(first_alignment),
                            detail::format_bytes(second_alignment),
                            detail::format_delta_bytes(alternative.element_alignment_delta));
                        comparison_row("Alternative extent",
                                       detail::format_bytes(alternative.first.extent_bytes),
                                       detail::format_bytes(alternative.second.extent_bytes),
                                       detail::format_delta_bytes(alternative.extent_delta));
                        comparison_row("Conditional payload slack per object",
                                       detail::format_bytes(alternative.first.payload_slack_bytes),
                                       detail::format_bytes(alternative.second.payload_slack_bytes),
                                       detail::format_delta_bytes(alternative.payload_slack_delta));
                        comparison_row(
                            "Conditional payload slack at selected count",
                            detail::format_bytes(alternative.first.total_payload_slack_bytes),
                            detail::format_bytes(alternative.second.total_payload_slack_bytes),
                            detail::format_delta_bytes(alternative.total_payload_slack_delta));
                        ImGui::EndTable();
                    }
                }
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
        if (analysis_session_.results().tagged_union_target_distribution_comparison.has_value()) {
            auto const& distribution{
                *analysis_session_.results().tagged_union_target_distribution_comparison};
            if (detail::section("Explicit distribution across targets")) {
                if (ImGui::BeginTable("tagged-distribution-target-comparison",
                                      4,
                                      ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                          ImGuiTableFlags_Resizable)) {
                    ImGui::TableSetupColumn("Distribution fact");
                    ImGui::TableSetupColumn(analysis_session_.primary_abi().name().c_str());
                    ImGui::TableSetupColumn(analysis_session_.comparison_abi().name().c_str());
                    ImGui::TableSetupColumn("Difference (B - A)");
                    ImGui::TableHeadersRow();
                    comparison_row("Total weight",
                                   detail::format_number(distribution.first.total_weight),
                                   detail::format_number(distribution.second.total_weight),
                                   detail::format_delta_number(distribution.total_weight_delta));
                    comparison_row(
                        "Weighted payload extent",
                        detail::format_bytes(distribution.first.total_payload_extent_bytes),
                        detail::format_bytes(distribution.second.total_payload_extent_bytes),
                        detail::format_delta_bytes(distribution.total_payload_extent_delta));
                    comparison_row(
                        "Weighted payload slack",
                        detail::format_bytes(distribution.first.total_payload_slack_bytes),
                        detail::format_bytes(distribution.second.total_payload_slack_bytes),
                        detail::format_delta_bytes(distribution.total_payload_slack_delta));
                    comparison_row(
                        "Expected payload extent / value",
                        format_optional_decimal(
                            distribution.first.expected_payload_extent_bytes_per_value),
                        format_optional_decimal(
                            distribution.second.expected_payload_extent_bytes_per_value),
                        format_decimal_delta(distribution.expected_payload_extent_per_value_delta));
                    comparison_row(
                        "Expected payload slack / value",
                        format_optional_decimal(
                            distribution.first.expected_payload_slack_bytes_per_value),
                        format_optional_decimal(
                            distribution.second.expected_payload_slack_bytes_per_value),
                        format_decimal_delta(distribution.expected_payload_slack_per_value_delta));
                    comparison_row(
                        "Expected payload extent at selected count",
                        format_optional_decimal(
                            distribution.first.expected_selected_payload_extent_bytes),
                        format_optional_decimal(
                            distribution.second.expected_selected_payload_extent_bytes),
                        format_decimal_delta(distribution.expected_selected_payload_extent_delta));
                    comparison_row(
                        "Expected payload slack at selected count",
                        format_optional_decimal(
                            distribution.first.expected_selected_payload_slack_bytes),
                        format_optional_decimal(
                            distribution.second.expected_selected_payload_slack_bytes),
                        format_decimal_delta(distribution.expected_selected_payload_slack_delta));
                    ImGui::EndTable();
                }
                if (!distribution.entries.empty() && ImGui::TreeNode("Distribution entries")) {
                    for (auto const& entry : distribution.entries) {
                        ImGui::PushID(entry.tag.c_str());
                        if (detail::section(entry.tag.c_str())) {
                            if (ImGui::BeginTable("tagged-distribution-entry-target-comparison",
                                                  4,
                                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                                      ImGuiTableFlags_Resizable)) {
                                ImGui::TableSetupColumn("Distribution fact");
                                ImGui::TableSetupColumn(
                                    analysis_session_.primary_abi().name().c_str());
                                ImGui::TableSetupColumn(
                                    analysis_session_.comparison_abi().name().c_str());
                                ImGui::TableSetupColumn("Difference (B - A)");
                                ImGui::TableHeadersRow();
                                comparison_row("Alternative",
                                               entry.first.alternative_name,
                                               entry.second.alternative_name);
                                comparison_row("Weight",
                                               std::to_string(entry.first.weight),
                                               std::to_string(entry.second.weight));
                                comparison_row(
                                    "Payload extent",
                                    detail::format_bytes(entry.first.payload_extent_bytes),
                                    detail::format_bytes(entry.second.payload_extent_bytes),
                                    detail::format_delta_bytes(entry.payload_extent_delta));
                                comparison_row(
                                    "Payload slack",
                                    detail::format_bytes(entry.first.payload_slack_bytes),
                                    detail::format_bytes(entry.second.payload_slack_bytes),
                                    detail::format_delta_bytes(entry.payload_slack_delta));
                                comparison_row(
                                    "Weighted payload extent",
                                    detail::format_bytes(entry.first.weighted_payload_extent_bytes),
                                    detail::format_bytes(
                                        entry.second.weighted_payload_extent_bytes),
                                    detail::format_delta_bytes(
                                        entry.weighted_payload_extent_delta));
                                comparison_row(
                                    "Weighted payload slack",
                                    detail::format_bytes(entry.first.weighted_payload_slack_bytes),
                                    detail::format_bytes(entry.second.weighted_payload_slack_bytes),
                                    detail::format_delta_bytes(entry.weighted_payload_slack_delta));
                                ImGui::EndTable();
                            }
                        }
                        ImGui::PopID();
                    }
                    ImGui::TreePop();
                }
                draw_diagnostics(distribution.diagnostics);
            }
        }
        ImGui::TextDisabled(
            "Tag coverage is semantic and unchanged by target selection. Layout and conditional "
            "slack "
            "are deterministic physical facts, not a performance prediction or ABI-compatibility "
            "verdict. The optional session workload distribution remains a separate analysis.");
        draw_diagnostics(comparison.diagnostics);
    }
}

} // namespace ioj::layout_planner
