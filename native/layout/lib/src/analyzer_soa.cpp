#include "analyzer_internal.hpp"

namespace ioj::layout {

auto Analyzer::analyze_soa(lispb::schema::TypeGraph const& types,
                           lispb::schema::TypeId const type,
                           Variant const& variant,
                           AbiProfile const& abi,
                           std::uint64_t const default_capacity,
                           SoaAllocationStrategy const allocation_strategy) -> SoaAnalysis {
    auto const& soa{std::get<lispb::schema::SoaType>(types.type(type).definition)};
    auto capacity{default_capacity};
    bool capacity_overridden{};
    if (auto const found{variant.overrides.capacities.find(type)};
        found != variant.overrides.capacities.end()) {
        capacity = found->second;
        capacity_overridden = true;
    }
    SoaAnalysis result{.type = type,
                       .capacity = capacity,
                       .capacity_overridden = capacity_overridden,
                       .allocation_strategy = allocation_strategy,
                       .allocation_count =
                           capacity == 0 || soa.columns.empty()
                               ? 0U
                               : (allocation_strategy == SoaAllocationStrategy::separate_columns
                                      ? static_cast<std::uint64_t>(soa.columns.size())
                                      : 1U),
                       .columns = {},
                       .bytes_per_logical_element = std::nullopt,
                       .total_payload_bytes = std::nullopt,
                       .total_allocation_bytes = std::nullopt,
                       .total_alignment_padding_bytes = std::nullopt,
                       .allocation_alignment_bytes = std::nullopt,
                       .cache_line_bytes = abi.memory_facts().cache_line_bytes,
                       .page_bytes = abi.memory_facts().page_bytes,
                       .minimum_pages = std::nullopt,
                       .cache_capacity = {},
                       .diagnostics = {}};
    auto const cache_line_bytes{result.cache_line_bytes};
    auto const has_cache_line_size{cache_line_bytes.has_value() && *cache_line_bytes != 0};
    if (!cache_line_bytes.has_value()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::warning,
             "Cache-line size is unknown for ABI profile '" + abi.name() + "'."});
    } else if (*cache_line_bytes == 0) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "ABI profile cache-line size must be non-zero."});
    }
    auto const has_page_size{result.page_bytes.has_value() && *result.page_bytes != 0};
    if (!result.page_bytes.has_value()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::warning,
             "Page size is unknown for ABI profile '" + abi.name() + "'."});
    } else if (*result.page_bytes == 0) {
        result.page_bytes.reset();
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "ABI profile page size must be non-zero."});
    }
    std::uint64_t row_bytes{};
    std::uint64_t total_bytes{};
    std::uint64_t total_pages{};
    bool complete{true};
    bool pages_complete{has_page_size};
    bool allocation_complete{true};
    std::uint64_t allocation_offset{};
    std::uint64_t allocation_padding{};
    std::uint64_t allocation_alignment{1};
    result.columns.reserve(soa.columns.size());

    for (auto const& column : soa.columns) {
        SoaColumnAnalysis column_result{
            .name = column.name,
            .semantic_type = column.semantic_type.type,
            .schema_type = types.type(column.semantic_type.type).cpp_spelling,
            .physical_type = effective_column_type(types, type, column, variant),
            .overridden = variant.overrides.soa_column_types.contains(
                FieldOverrideId{.type = type, .field_name = column.name}),
            .type_facts = std::nullopt,
            .total_bytes = std::nullopt,
            .allocation_offset_bytes = std::nullopt,
            .padding_before_bytes = std::nullopt,
            .minimum_cache_lines = std::nullopt,
            .elements_per_cache_line = std::nullopt,
            .minimum_pages = std::nullopt,
            .complete_elements_per_page = std::nullopt,
            .cache_line_tiling = std::nullopt};
        column_result.type_facts = abi.find(column_result.physical_type);
        if (!column_result.type_facts.has_value()) {
            complete = false;
            pages_complete = false;
            allocation_complete = false;
            result.diagnostics.push_back({DiagnosticSeverity::warning,
                                          "Unknown physical facts for SoA column '" + column.name +
                                              "' type '" + column_result.physical_type + "'."});
            result.columns.push_back(std::move(column_result));
            continue;
        }

        auto const size{column_result.type_facts->size_bytes};
        auto const alignment{column_result.type_facts->alignment_bytes};
        if (size == 0 || alignment == 0) {
            complete = false;
            pages_complete = false;
            allocation_complete = false;
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "SoA column '" + column.name + "' has a zero-byte size or zero-byte alignment."});
            result.columns.push_back(std::move(column_result));
            continue;
        }
        column_result.total_bytes = checked_multiply(size, capacity);
        if (!column_result.total_bytes.has_value()) {
            complete = false;
            pages_complete = false;
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Byte count overflows uint64 for SoA column '" + column.name + "'."});
        } else if (has_cache_line_size) {
            column_result.minimum_cache_lines =
                minimum_regions(*column_result.total_bytes, *cache_line_bytes);
        }
        if (has_cache_line_size) {
            column_result.cache_line_tiling = cache_line_tiling(size, *cache_line_bytes);
            column_result.elements_per_cache_line =
                column_result.cache_line_tiling->exact_elements_per_cache_line;
        }
        if (has_page_size) {
            column_result.complete_elements_per_page = *result.page_bytes / size;
            if (column_result.total_bytes.has_value()) {
                column_result.minimum_pages =
                    minimum_regions(*column_result.total_bytes, *result.page_bytes);
                auto const next_pages{checked_add(total_pages, *column_result.minimum_pages)};
                if (!next_pages.has_value()) {
                    pages_complete = false;
                    result.diagnostics.push_back(
                        {DiagnosticSeverity::error,
                         "SoA minimum page count across separate columns overflows uint64."});
                } else {
                    total_pages = *next_pages;
                }
            }
        }

        auto const next_row_bytes{checked_add(row_bytes, size)};
        if (!next_row_bytes.has_value()) {
            complete = false;
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, "SoA bytes per logical element overflow uint64."});
        } else {
            row_bytes = *next_row_bytes;
        }
        if (column_result.total_bytes.has_value()) {
            auto const next_total{checked_add(total_bytes, *column_result.total_bytes)};
            if (!next_total.has_value()) {
                complete = false;
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error, "SoA total payload bytes overflow uint64."});
            } else {
                total_bytes = *next_total;
            }
        }
        if (allocation_strategy == SoaAllocationStrategy::aligned_contiguous) {
            auto const aligned{align_up(allocation_offset, alignment)};
            if (!aligned.has_value() || !column_result.total_bytes.has_value()) {
                allocation_complete = false;
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error,
                     "Aligned contiguous allocation arithmetic overflows at SoA column '" +
                         column.name + "'."});
            } else {
                column_result.allocation_offset_bytes = *aligned;
                column_result.padding_before_bytes = *aligned - allocation_offset;
                auto const next_padding{
                    checked_add(allocation_padding, *column_result.padding_before_bytes)};
                auto const next_offset{checked_add(*aligned, *column_result.total_bytes)};
                if (!next_padding.has_value() || !next_offset.has_value()) {
                    allocation_complete = false;
                    result.diagnostics.push_back(
                        {DiagnosticSeverity::error,
                         "Aligned contiguous allocation total overflows at SoA column '" +
                             column.name + "'."});
                } else {
                    allocation_padding = *next_padding;
                    allocation_offset = *next_offset;
                    allocation_alignment = std::max(allocation_alignment, alignment);
                }
            }
        }
        result.columns.push_back(std::move(column_result));
    }

    if (complete) {
        result.bytes_per_logical_element = row_bytes;
        result.total_payload_bytes = total_bytes;
    }
    if (allocation_strategy == SoaAllocationStrategy::separate_columns) {
        result.total_allocation_bytes = result.total_payload_bytes;
        result.total_alignment_padding_bytes = 0;
    } else if (allocation_complete) {
        result.total_allocation_bytes = allocation_offset;
        result.total_alignment_padding_bytes = allocation_padding;
        result.allocation_alignment_bytes = allocation_alignment;
    }
    result.cache_capacity =
        cache_capacity_analysis(result.total_allocation_bytes, abi.memory_facts());
    if (allocation_strategy == SoaAllocationStrategy::aligned_contiguous &&
        result.total_allocation_bytes.has_value() && has_page_size) {
        result.minimum_pages = minimum_regions(*result.total_allocation_bytes, *result.page_bytes);
    } else if (pages_complete) {
        result.minimum_pages = total_pages;
    }
    return result;
}

auto Analyzer::compare_soa_targets(SoaAnalysis const& first, SoaAnalysis const& second)
    -> SoaTargetComparison {
    SoaTargetComparison result;
    result.first = first;
    result.second = second;
    for (auto const& diagnostic : first.diagnostics) {
        result.diagnostics.push_back(
            {diagnostic.severity, "First target SoA layout: " + diagnostic.message});
    }
    for (auto const& diagnostic : second.diagnostics) {
        result.diagnostics.push_back(
            {diagnostic.severity, "Second target SoA layout: " + diagnostic.message});
    }

    if (first.type != second.type) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared target SoA analyses describe different semantic types."});
        return result;
    }
    if (first.capacity != second.capacity ||
        first.capacity_overridden != second.capacity_overridden) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared target SoA analyses use different capacities or capacity overrides."});
        return result;
    }
    if (first.allocation_strategy != second.allocation_strategy) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared target SoA analyses use different allocation strategies."});
        return result;
    }
    if (first.columns.size() != second.columns.size()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared target SoA analyses do not contain the same ordered columns."});
        return result;
    }

    std::set<std::string, std::less<>> names;
    for (std::size_t index{}; index < first.columns.size(); ++index) {
        auto const& first_column{first.columns[index]};
        auto const& second_column{second.columns[index]};
        auto const unique_name{names.insert(first_column.name).second};
        if (!unique_name || first_column.name != second_column.name ||
            first_column.semantic_type != second_column.semantic_type ||
            first_column.schema_type != second_column.schema_type ||
            first_column.physical_type != second_column.physical_type ||
            first_column.overridden != second_column.overridden) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Compared target SoA analyses do not contain the same unique ordered columns "
                 "and physical variant."});
            return result;
        }
    }

    result.compatible = true;
    result.columns.reserve(first.columns.size());
    for (std::size_t index{}; index < first.columns.size(); ++index) {
        auto const& first_column{first.columns[index]};
        auto const& second_column{second.columns[index]};
        auto const first_size{first_column.type_facts.transform(
            [](TypeFacts const& facts) { return facts.size_bytes; })};
        auto const second_size{second_column.type_facts.transform(
            [](TypeFacts const& facts) { return facts.size_bytes; })};
        auto const first_alignment{first_column.type_facts.transform(
            [](TypeFacts const& facts) { return facts.alignment_bytes; })};
        auto const second_alignment{second_column.type_facts.transform(
            [](TypeFacts const& facts) { return facts.alignment_bytes; })};
        result.columns.push_back(
            {.name = first_column.name,
             .first = first_column,
             .second = second_column,
             .element_size_delta = numeric_delta(first_size, second_size),
             .element_alignment_delta = numeric_delta(first_alignment, second_alignment),
             .total_byte_delta = numeric_delta(first_column.total_bytes, second_column.total_bytes),
             .allocation_offset_delta = numeric_delta(first_column.allocation_offset_bytes,
                                                      second_column.allocation_offset_bytes),
             .padding_before_delta = numeric_delta(first_column.padding_before_bytes,
                                                   second_column.padding_before_bytes),
             .minimum_cache_line_delta =
                 numeric_delta(first_column.minimum_cache_lines, second_column.minimum_cache_lines),
             .elements_per_cache_line_delta = numeric_delta(first_column.elements_per_cache_line,
                                                            second_column.elements_per_cache_line),
             .minimum_page_delta =
                 numeric_delta(first_column.minimum_pages, second_column.minimum_pages),
             .complete_elements_per_page_delta =
                 numeric_delta(first_column.complete_elements_per_page,
                               second_column.complete_elements_per_page)});
    }

    result.allocation_count_delta = numeric_delta(first.allocation_count, second.allocation_count);
    result.bytes_per_logical_element_delta =
        numeric_delta(first.bytes_per_logical_element, second.bytes_per_logical_element);
    result.total_payload_delta =
        numeric_delta(first.total_payload_bytes, second.total_payload_bytes);
    result.total_allocation_delta =
        numeric_delta(first.total_allocation_bytes, second.total_allocation_bytes);
    result.total_alignment_padding_delta =
        numeric_delta(first.total_alignment_padding_bytes, second.total_alignment_padding_bytes);
    result.allocation_alignment_delta =
        numeric_delta(first.allocation_alignment_bytes, second.allocation_alignment_bytes);
    result.cache_line_size_delta = numeric_delta(first.cache_line_bytes, second.cache_line_bytes);
    result.page_size_delta = numeric_delta(first.page_bytes, second.page_bytes);
    result.minimum_page_delta = numeric_delta(first.minimum_pages, second.minimum_pages);
    return result;
}

auto Analyzer::derive_relationship_target_facts(lispb::schema::TypeGraph const& types,
                                                Variant const& variant,
                                                AbiProfile const& abi,
                                                std::uint64_t const default_capacity,
                                                SoaAllocationStrategy const allocation_strategy)
    -> std::vector<RelationshipTargetFacts> {
    std::vector<RelationshipTargetFacts> result;
    auto const resolved_types{types.types()};
    result.reserve(resolved_types.size());

    for (std::size_t type_index{}; type_index < resolved_types.size(); ++type_index) {
        if (!std::holds_alternative<lispb::schema::SoaType>(
                resolved_types[type_index].definition)) {
            continue;
        }

        auto const type{lispb::schema::TypeId{.value = static_cast<std::uint32_t>(type_index)}};
        auto const analysis{
            analyze_soa(types, type, variant, abi, default_capacity, allocation_strategy)};
        result.push_back({.target = type,
                          .element_capacity = analysis.capacity,
                          .byte_extent = analysis.total_allocation_bytes});
    }
    return result;
}

auto Analyzer::analyze_soa_access(SoaAnalysis const& soa,
                                  std::span<AccessIntent const> const accesses,
                                  AbiProfile const& abi,
                                  std::uint64_t const element_count,
                                  std::uint64_t const multiplicity) -> SoaAccessAnalysis {
    SoaAccessAnalysis result{.column_names = {},
                             .accesses = {},
                             .columns = {},
                             .element_count = element_count,
                             .multiplicity = multiplicity,
                             .allocation_strategy = soa.allocation_strategy,
                             .footprint_exact = soa.allocation_strategy ==
                                                SoaAllocationStrategy::aligned_contiguous,
                             .allocation_count = soa.allocation_count,
                             .useful_bytes = std::uint64_t{},
                             .read_useful_bytes = std::uint64_t{},
                             .write_useful_bytes = std::uint64_t{},
                             .logical_read_useful_bytes = std::nullopt,
                             .logical_write_useful_bytes = std::nullopt,
                             .full_logical_payload_bytes = std::nullopt,
                             .unselected_payload_bytes = std::nullopt,
                             .allocated_capacity_payload_bytes = soa.total_payload_bytes,
                             .capacity_slack_payload_bytes = std::nullopt,
                             .total_allocation_bytes = soa.total_allocation_bytes,
                             .alignment_padding_bytes = soa.total_alignment_padding_bytes,
                             .cache_line_bytes = abi.memory_facts().cache_line_bytes,
                             .minimum_cache_lines_touched = std::uint64_t{},
                             .minimum_cache_bytes_touched = std::nullopt,
                             .minimum_read_cache_lines_touched = std::uint64_t{},
                             .minimum_read_cache_bytes_touched = std::nullopt,
                             .minimum_write_cache_lines_touched = std::uint64_t{},
                             .minimum_write_cache_bytes_touched = std::nullopt,
                             .non_payload_cache_bytes = std::nullopt,
                             .minimum_cache_footprint_capacity =
                                 cache_capacity_analysis(std::nullopt, abi.memory_facts()),
                             .page_bytes = abi.memory_facts().page_bytes,
                             .minimum_pages_touched = std::uint64_t{},
                             .minimum_page_bytes_touched = std::nullopt,
                             .minimum_read_pages_touched = std::uint64_t{},
                             .minimum_read_page_bytes_touched = std::nullopt,
                             .minimum_write_pages_touched = std::uint64_t{},
                             .minimum_write_page_bytes_touched = std::nullopt,
                             .non_payload_page_bytes = std::nullopt,
                             .diagnostics = {}};
    if (multiplicity == 0) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "Access multiplicity must be non-zero."});
    }
    if (!result.cache_line_bytes.has_value() || *result.cache_line_bytes == 0) {
        result.minimum_cache_lines_touched.reset();
        result.minimum_read_cache_lines_touched.reset();
        result.minimum_write_cache_lines_touched.reset();
    }
    if (!result.page_bytes.has_value() || *result.page_bytes == 0) {
        result.minimum_pages_touched.reset();
        result.minimum_read_pages_touched.reset();
        result.minimum_write_pages_touched.reset();
    }
    if (soa.bytes_per_logical_element.has_value()) {
        result.full_logical_payload_bytes =
            checked_multiply(*soa.bytes_per_logical_element, element_count);
        if (!result.full_logical_payload_bytes.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Complete SoA logical payload at the selected count overflows uint64."});
        }
    }
    if (element_count > soa.capacity) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::warning,
             "Selected access count exceeds the modeled SoA allocation capacity."});
    }
    if (result.allocated_capacity_payload_bytes.has_value() &&
        result.full_logical_payload_bytes.has_value() &&
        *result.allocated_capacity_payload_bytes >= *result.full_logical_payload_bytes) {
        result.capacity_slack_payload_bytes =
            *result.allocated_capacity_payload_bytes - *result.full_logical_payload_bytes;
    }
    std::set<std::string, std::less<>> unique_names;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> accessed_intervals;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> read_intervals;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> write_intervals;
    for (auto const& access : accesses) {
        auto const& column_name{access.name};
        if (!accept_unique_access(
                unique_names, result.accesses, result.diagnostics, access, "SoA column")) {
            continue;
        }
        auto const column{std::ranges::find(soa.columns, column_name, &SoaColumnAnalysis::name)};
        if (column == soa.columns.end()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Selected SoA column '" + column_name + "' no longer exists."});
            continue;
        }
        result.column_names.push_back(column_name);
        result.accesses.push_back(access);

        auto accumulate = [&](std::optional<std::uint64_t> const value,
                              std::optional<std::uint64_t>& total,
                              std::string_view const fact) {
            if (!value.has_value() || !total.has_value()) {
                total.reset();
                if (!value.has_value()) {
                    result.diagnostics.push_back({DiagnosticSeverity::warning,
                                                  "Selected SoA column '" + column_name +
                                                      "' has Unknown " + std::string{fact} + "."});
                }
                return;
            }
            total = checked_add(*total, *value);
            if (!total.has_value()) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error,
                     "Selected SoA " + std::string{fact} + " total overflows uint64."});
            }
        };
        std::optional<std::uint64_t> flat_accessed_bytes;
        if (column->type_facts.has_value()) {
            flat_accessed_bytes = checked_multiply(column->type_facts->size_bytes, element_count);
            if (!flat_accessed_bytes.has_value()) {
                result.diagnostics.push_back({DiagnosticSeverity::error,
                                              "Selected SoA column '" + column_name +
                                                  "' payload byte count overflows uint64."});
            }
        }
        SoaColumnAccessAnalysis column_access{
            .name = column_name,
            .operation = access.operation,
            .physical_type = column->physical_type,
            .element_bytes = column->type_facts.transform(
                [](TypeFacts const& facts) { return facts.size_bytes; }),
            .useful_bytes = flat_accessed_bytes,
            .read_useful_bytes = classified_useful_bytes(flat_accessed_bytes,
                                                         access_includes_read(access.operation)),
            .write_useful_bytes = classified_useful_bytes(flat_accessed_bytes,
                                                          access_includes_write(access.operation)),
            .logical_read_useful_bytes = logical_useful_bytes(
                classified_useful_bytes(flat_accessed_bytes,
                                        access_includes_read(access.operation)),
                multiplicity),
            .logical_write_useful_bytes = logical_useful_bytes(
                classified_useful_bytes(flat_accessed_bytes,
                                        access_includes_write(access.operation)),
                multiplicity),
            .minimum_cache_lines = std::nullopt,
            .minimum_cache_bytes = std::nullopt,
            .non_payload_cache_bytes = std::nullopt,
            .aligned_cache_line_straddling_elements = std::nullopt,
            .minimum_pages = std::nullopt,
            .minimum_page_bytes = std::nullopt,
            .non_payload_page_bytes = std::nullopt,
            .aligned_page_straddling_elements = std::nullopt,
            .allocated_capacity_payload_bytes = column->total_bytes,
            .capacity_slack_payload_bytes = std::nullopt};
        if (multiplicity != 0 && column_access.read_useful_bytes.has_value() &&
            !column_access.logical_read_useful_bytes.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Selected SoA column '" + column_name +
                     "' logical read useful byte total overflows uint64."});
        }
        if (multiplicity != 0 && column_access.write_useful_bytes.has_value() &&
            !column_access.logical_write_useful_bytes.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Selected SoA column '" + column_name +
                     "' logical write useful byte total overflows uint64."});
        }
        if (column_access.allocated_capacity_payload_bytes.has_value() &&
            column_access.useful_bytes.has_value() &&
            *column_access.allocated_capacity_payload_bytes >= *column_access.useful_bytes) {
            column_access.capacity_slack_payload_bytes =
                *column_access.allocated_capacity_payload_bytes - *column_access.useful_bytes;
        }
        auto derive_column_regions = [&](std::optional<std::uint64_t> const region_size,
                                         std::optional<std::uint64_t>& regions,
                                         std::optional<std::uint64_t>& bytes,
                                         std::optional<std::uint64_t>& non_payload_bytes,
                                         std::string_view const region_name) {
            if (!region_size.has_value() || *region_size == 0 ||
                !column_access.useful_bytes.has_value()) {
                return;
            }
            regions = minimum_regions(*column_access.useful_bytes, *region_size);
            bytes = checked_multiply(*regions, *region_size);
            if (!bytes.has_value()) {
                result.diagnostics.push_back({DiagnosticSeverity::error,
                                              "Selected SoA column '" + column_name + "' minimum " +
                                                  std::string{region_name} +
                                                  " footprint overflows uint64."});
            } else {
                non_payload_bytes = *bytes - *column_access.useful_bytes;
            }
        };
        derive_column_regions(result.cache_line_bytes,
                              column_access.minimum_cache_lines,
                              column_access.minimum_cache_bytes,
                              column_access.non_payload_cache_bytes,
                              "cache-line");
        derive_column_regions(result.page_bytes,
                              column_access.minimum_pages,
                              column_access.minimum_page_bytes,
                              column_access.non_payload_page_bytes,
                              "page");
        if (access_includes_read(access.operation)) {
            accumulate(column_access.minimum_cache_lines,
                       result.minimum_read_cache_lines_touched,
                       "minimum read cache-line count");
            accumulate(column_access.minimum_pages,
                       result.minimum_read_pages_touched,
                       "minimum read page count");
        }
        if (access_includes_write(access.operation)) {
            accumulate(column_access.minimum_cache_lines,
                       result.minimum_write_cache_lines_touched,
                       "minimum write cache-line count");
            accumulate(column_access.minimum_pages,
                       result.minimum_write_pages_touched,
                       "minimum write page count");
        }
        if (soa.allocation_strategy == SoaAllocationStrategy::aligned_contiguous &&
            column->allocation_offset_bytes.has_value() && flat_accessed_bytes.has_value() &&
            *flat_accessed_bytes != 0) {
            auto const interval{std::pair{*column->allocation_offset_bytes, *flat_accessed_bytes}};
            accessed_intervals.push_back(interval);
            if (access_includes_read(access.operation)) {
                read_intervals.push_back(interval);
            }
            if (access_includes_write(access.operation)) {
                write_intervals.push_back(interval);
            }
        }
        if (column_access.element_bytes.has_value()) {
            if (*column_access.element_bytes == 0) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error,
                     "Selected SoA column '" + column_name + "' has zero-sized elements."});
            } else {
                if (result.cache_line_bytes.has_value() && *result.cache_line_bytes != 0) {
                    column_access.aligned_cache_line_straddling_elements =
                        soa.allocation_strategy == SoaAllocationStrategy::aligned_contiguous &&
                                column->allocation_offset_bytes.has_value()
                            ? straddling_elements_at_offset(*column_access.element_bytes,
                                                            element_count,
                                                            *result.cache_line_bytes,
                                                            column_access.useful_bytes,
                                                            *column->allocation_offset_bytes)
                            : straddling_elements(*column_access.element_bytes,
                                                  element_count,
                                                  *result.cache_line_bytes,
                                                  column_access.useful_bytes);
                }
                if (result.page_bytes.has_value() && *result.page_bytes != 0) {
                    column_access.aligned_page_straddling_elements =
                        soa.allocation_strategy == SoaAllocationStrategy::aligned_contiguous &&
                                column->allocation_offset_bytes.has_value()
                            ? straddling_elements_at_offset(*column_access.element_bytes,
                                                            element_count,
                                                            *result.page_bytes,
                                                            column_access.useful_bytes,
                                                            *column->allocation_offset_bytes)
                            : straddling_elements(*column_access.element_bytes,
                                                  element_count,
                                                  *result.page_bytes,
                                                  column_access.useful_bytes);
                }
            }
        }
        accumulate(column_access.read_useful_bytes, result.read_useful_bytes, "read useful bytes");
        accumulate(
            column_access.write_useful_bytes, result.write_useful_bytes, "write useful bytes");
        result.columns.push_back(std::move(column_access));
        if (flat_accessed_bytes.has_value()) {
            accumulate(flat_accessed_bytes, result.useful_bytes, "payload bytes");
        } else {
            result.useful_bytes.reset();
            if (!column->type_facts.has_value()) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::warning,
                     "Selected SoA column '" + column_name + "' has Unknown payload bytes."});
            }
        }
        if (result.minimum_cache_lines_touched.has_value()) {
            auto const cache_lines{flat_accessed_bytes.transform([&](std::uint64_t const bytes) {
                return minimum_regions(bytes, *result.cache_line_bytes);
            })};
            accumulate(cache_lines, result.minimum_cache_lines_touched, "minimum cache-line count");
        }
        if (result.minimum_pages_touched.has_value()) {
            auto const pages{flat_accessed_bytes.transform([&](std::uint64_t const bytes) {
                return minimum_regions(bytes, *result.page_bytes);
            })};
            accumulate(pages, result.minimum_pages_touched, "minimum page count");
        }
    }

    if (result.column_names.empty()) {
        result.useful_bytes.reset();
        result.read_useful_bytes.reset();
        result.write_useful_bytes.reset();
        result.logical_read_useful_bytes =
            logical_useful_bytes(result.read_useful_bytes, multiplicity);
        result.logical_write_useful_bytes =
            logical_useful_bytes(result.write_useful_bytes, multiplicity);
        result.minimum_cache_lines_touched.reset();
        result.minimum_read_cache_lines_touched.reset();
        result.minimum_write_cache_lines_touched.reset();
        result.minimum_pages_touched.reset();
        result.minimum_read_pages_touched.reset();
        result.minimum_write_pages_touched.reset();
        if (result.diagnostics.empty()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::warning, "No SoA columns are selected for access."});
        }
        return result;
    }

    if (std::ranges::none_of(result.accesses, [](AccessIntent const& access) {
            return access_includes_read(access.operation);
        })) {
        result.minimum_read_cache_lines_touched = 0;
        result.minimum_read_cache_bytes_touched = 0;
        result.minimum_read_pages_touched = 0;
        result.minimum_read_page_bytes_touched = 0;
    }
    if (std::ranges::none_of(result.accesses, [](AccessIntent const& access) {
            return access_includes_write(access.operation);
        })) {
        result.minimum_write_cache_lines_touched = 0;
        result.minimum_write_cache_bytes_touched = 0;
        result.minimum_write_pages_touched = 0;
        result.minimum_write_page_bytes_touched = 0;
    }

    if (result.full_logical_payload_bytes.has_value() && result.useful_bytes.has_value()) {
        if (*result.full_logical_payload_bytes >= *result.useful_bytes) {
            result.unselected_payload_bytes =
                *result.full_logical_payload_bytes - *result.useful_bytes;
        } else {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Selected SoA payload bytes exceed the complete logical payload."});
        }
    }
    if (soa.allocation_strategy == SoaAllocationStrategy::aligned_contiguous) {
        if (element_count > soa.capacity || !soa.total_allocation_bytes.has_value()) {
            result.footprint_exact = false;
            result.minimum_cache_lines_touched.reset();
            result.minimum_read_cache_lines_touched.reset();
            result.minimum_write_cache_lines_touched.reset();
            result.minimum_pages_touched.reset();
            result.minimum_read_pages_touched.reset();
            result.minimum_write_pages_touched.reset();
        } else {
            auto exact_regions = [&](auto& intervals,
                                     std::optional<std::uint64_t> const region_size,
                                     std::optional<std::uint64_t>& regions,
                                     char const* const region_name) {
                if (element_count == 0 || intervals.empty()) {
                    regions = 0;
                    return;
                }
                if (!region_size.has_value() || *region_size == 0) {
                    regions.reset();
                    return;
                }
                std::ranges::sort(intervals);
                std::string error;
                regions = touched_regions(0, intervals, 1, *region_size, error);
                if (!regions.has_value()) {
                    result.diagnostics.push_back(
                        {DiagnosticSeverity::error,
                         std::string{"Contiguous SoA "} + region_name + ": " + error});
                }
            };
            exact_regions(accessed_intervals,
                          result.cache_line_bytes,
                          result.minimum_cache_lines_touched,
                          "cache-line access analysis");
            exact_regions(read_intervals,
                          result.cache_line_bytes,
                          result.minimum_read_cache_lines_touched,
                          "read cache-line access analysis");
            exact_regions(write_intervals,
                          result.cache_line_bytes,
                          result.minimum_write_cache_lines_touched,
                          "write cache-line access analysis");
            exact_regions(accessed_intervals,
                          result.page_bytes,
                          result.minimum_pages_touched,
                          "page access analysis");
            exact_regions(read_intervals,
                          result.page_bytes,
                          result.minimum_read_pages_touched,
                          "read page access analysis");
            exact_regions(write_intervals,
                          result.page_bytes,
                          result.minimum_write_pages_touched,
                          "write page access analysis");
        }
    }
    auto derive_region_bytes = [&](std::optional<std::uint64_t>& region_size,
                                   std::optional<std::uint64_t> const regions,
                                   std::optional<std::uint64_t>& touched_bytes,
                                   std::optional<std::uint64_t>& non_payload_bytes,
                                   char const* const region_name) {
        if (!region_size.has_value()) {
            result.diagnostics.push_back({DiagnosticSeverity::warning,
                                          std::string{region_name} +
                                              " size is unknown for ABI profile '" + abi.name() +
                                              "'."});
            return;
        }
        if (*region_size == 0) {
            region_size.reset();
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, std::string{region_name} + " size must be non-zero."});
            return;
        }
        if (!regions.has_value()) {
            return;
        }
        touched_bytes = checked_multiply(*regions, *region_size);
        if (!touched_bytes.has_value()) {
            result.diagnostics.push_back({DiagnosticSeverity::error,
                                          "Selected SoA minimum " + std::string{region_name} +
                                              " footprint overflows uint64."});
            return;
        }
        if (result.useful_bytes.has_value()) {
            if (*touched_bytes >= *result.useful_bytes) {
                non_payload_bytes = *touched_bytes - *result.useful_bytes;
            } else {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error,
                     "Selected SoA minimum " + std::string{region_name} +
                         " footprint is smaller than useful payload bytes."});
            }
        }
    };
    derive_region_bytes(result.cache_line_bytes,
                        result.minimum_cache_lines_touched,
                        result.minimum_cache_bytes_touched,
                        result.non_payload_cache_bytes,
                        "cache-line");
    result.minimum_cache_footprint_capacity =
        cache_capacity_analysis(result.minimum_cache_bytes_touched, abi.memory_facts());
    derive_region_bytes(result.page_bytes,
                        result.minimum_pages_touched,
                        result.minimum_page_bytes_touched,
                        result.non_payload_page_bytes,
                        "page");
    auto derive_classified_region_bytes = [&](std::optional<std::uint64_t> const region_size,
                                              std::optional<std::uint64_t> const regions,
                                              std::optional<std::uint64_t>& bytes,
                                              char const* const category) {
        if (!region_size.has_value() || !regions.has_value()) {
            return;
        }
        bytes = checked_multiply(*regions, *region_size);
        if (!bytes.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 std::string{category} + " address-coverage byte count overflows uint64."});
        }
    };
    derive_classified_region_bytes(result.cache_line_bytes,
                                   result.minimum_read_cache_lines_touched,
                                   result.minimum_read_cache_bytes_touched,
                                   "Minimum read cache-line");
    derive_classified_region_bytes(result.cache_line_bytes,
                                   result.minimum_write_cache_lines_touched,
                                   result.minimum_write_cache_bytes_touched,
                                   "Minimum write cache-line");
    derive_classified_region_bytes(result.page_bytes,
                                   result.minimum_read_pages_touched,
                                   result.minimum_read_page_bytes_touched,
                                   "Minimum read page");
    derive_classified_region_bytes(result.page_bytes,
                                   result.minimum_write_pages_touched,
                                   result.minimum_write_page_bytes_touched,
                                   "Minimum write page");
    result.logical_read_useful_bytes = logical_useful_bytes(result.read_useful_bytes, multiplicity);
    result.logical_write_useful_bytes =
        logical_useful_bytes(result.write_useful_bytes, multiplicity);
    if (multiplicity != 0 && result.read_useful_bytes.has_value() &&
        !result.logical_read_useful_bytes.has_value()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "Logical read useful byte total overflows uint64."});
    }
    if (multiplicity != 0 && result.write_useful_bytes.has_value() &&
        !result.logical_write_useful_bytes.has_value()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "Logical write useful byte total overflows uint64."});
    }
    return result;
}

auto Analyzer::analyze_soa_access(SoaAnalysis const& soa,
                                  std::span<std::string const> const column_names,
                                  AbiProfile const& abi,
                                  std::uint64_t const element_count,
                                  AccessOperation const operation,
                                  std::uint64_t const multiplicity) -> SoaAccessAnalysis {
    std::vector<AccessIntent> accesses;
    accesses.reserve(column_names.size());
    for (auto const& column_name : column_names) {
        accesses.push_back({.name = column_name, .operation = operation});
    }
    return analyze_soa_access(soa, accesses, abi, element_count, multiplicity);
}

auto Analyzer::compare_record_soa_access(RecordAccessAnalysis const& record,
                                         SoaAccessAnalysis const& soa)
    -> RecordSoaAccessComparison {
    RecordSoaAccessComparison result{
        .member_names = soa.column_names,
        .accesses = soa.accesses,
        .element_count = soa.element_count,
        .multiplicity = record.multiplicity,
        .soa_allocation_strategy = soa.allocation_strategy,
        .soa_footprint_exact = soa.footprint_exact,
        .record = {.useful_bytes = record.useful_bytes,
                   .read_useful_bytes = record.read_useful_bytes,
                   .write_useful_bytes = record.write_useful_bytes,
                   .logical_read_useful_bytes = record.logical_read_useful_bytes,
                   .logical_write_useful_bytes = record.logical_write_useful_bytes,
                   .cache_lines = record.cache_lines_touched,
                   .cache_bytes = record.cache_bytes_touched,
                   .read_cache_lines = record.read_cache_lines_touched,
                   .read_cache_bytes = record.read_cache_bytes_touched,
                   .write_cache_lines = record.write_cache_lines_touched,
                   .write_cache_bytes = record.write_cache_bytes_touched,
                   .pages = record.pages_touched,
                   .page_bytes = record.page_bytes_touched,
                   .read_pages = record.read_pages_touched,
                   .read_page_bytes = record.read_page_bytes_touched,
                   .write_pages = record.write_pages_touched,
                   .write_page_bytes = record.write_page_bytes_touched},
        .soa = {.useful_bytes = soa.useful_bytes,
                .read_useful_bytes = soa.read_useful_bytes,
                .write_useful_bytes = soa.write_useful_bytes,
                .logical_read_useful_bytes = soa.logical_read_useful_bytes,
                .logical_write_useful_bytes = soa.logical_write_useful_bytes,
                .cache_lines = soa.minimum_cache_lines_touched,
                .cache_bytes = soa.minimum_cache_bytes_touched,
                .read_cache_lines = soa.minimum_read_cache_lines_touched,
                .read_cache_bytes = soa.minimum_read_cache_bytes_touched,
                .write_cache_lines = soa.minimum_write_cache_lines_touched,
                .write_cache_bytes = soa.minimum_write_cache_bytes_touched,
                .pages = soa.minimum_pages_touched,
                .page_bytes = soa.minimum_page_bytes_touched,
                .read_pages = soa.minimum_read_pages_touched,
                .read_page_bytes = soa.minimum_read_page_bytes_touched,
                .write_pages = soa.minimum_write_pages_touched,
                .write_page_bytes = soa.minimum_write_page_bytes_touched},
        .record_cache_footprint_capacity = record.cache_footprint_capacity,
        .soa_minimum_cache_footprint_capacity = soa.minimum_cache_footprint_capacity,
        .record_non_useful_cache_bytes = record.non_selected_cache_bytes,
        .soa_non_useful_cache_bytes = soa.non_payload_cache_bytes,
        .record_non_useful_page_bytes = record.non_selected_page_bytes,
        .soa_non_useful_page_bytes = soa.non_payload_page_bytes,
        .useful_byte_delta = std::nullopt,
        .read_useful_byte_delta = std::nullopt,
        .write_useful_byte_delta = std::nullopt,
        .logical_read_useful_byte_delta = std::nullopt,
        .logical_write_useful_byte_delta = std::nullopt,
        .cache_line_delta = std::nullopt,
        .cache_byte_delta = std::nullopt,
        .read_cache_line_delta = std::nullopt,
        .read_cache_byte_delta = std::nullopt,
        .write_cache_line_delta = std::nullopt,
        .write_cache_byte_delta = std::nullopt,
        .page_delta = std::nullopt,
        .page_byte_delta = std::nullopt,
        .read_page_delta = std::nullopt,
        .read_page_byte_delta = std::nullopt,
        .write_page_delta = std::nullopt,
        .write_page_byte_delta = std::nullopt,
        .non_useful_cache_byte_delta = std::nullopt,
        .non_useful_page_byte_delta = std::nullopt,
        .diagnostics = {}};
    for (auto const& diagnostic : record.diagnostics) {
        result.diagnostics.push_back({diagnostic.severity, "AoS access: " + diagnostic.message});
    }
    for (auto const& diagnostic : soa.diagnostics) {
        result.diagnostics.push_back({diagnostic.severity, "SoA access: " + diagnostic.message});
    }
    if (record.element_count != soa.element_count) {
        result.diagnostics.push_back({DiagnosticSeverity::error,
                                      "AoS and SoA access analyses use different element counts."});
        return result;
    }
    if (!access_intents_match(record.accesses, soa.accesses)) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "AoS and SoA access analyses use different per-field access classifications."});
        return result;
    }
    if (record.multiplicity != soa.multiplicity) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "AoS and SoA access analyses use different access multiplicities."});
        return result;
    }
    auto const record_names{
        std::set<std::string, std::less<>>{record.member_names.begin(), record.member_names.end()}};
    auto const soa_names{
        std::set<std::string, std::less<>>{soa.column_names.begin(), soa.column_names.end()}};
    if (record_names != soa_names) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "AoS and SoA access analyses do not select the same named fields."});
        return result;
    }
    result.useful_byte_delta = numeric_delta(result.record.useful_bytes, result.soa.useful_bytes);
    result.read_useful_byte_delta =
        numeric_delta(result.record.read_useful_bytes, result.soa.read_useful_bytes);
    result.write_useful_byte_delta =
        numeric_delta(result.record.write_useful_bytes, result.soa.write_useful_bytes);
    result.logical_read_useful_byte_delta = numeric_delta(result.record.logical_read_useful_bytes,
                                                          result.soa.logical_read_useful_bytes);
    result.logical_write_useful_byte_delta = numeric_delta(result.record.logical_write_useful_bytes,
                                                           result.soa.logical_write_useful_bytes);
    result.cache_line_delta = numeric_delta(result.record.cache_lines, result.soa.cache_lines);
    result.cache_byte_delta = numeric_delta(result.record.cache_bytes, result.soa.cache_bytes);
    result.read_cache_line_delta =
        numeric_delta(result.record.read_cache_lines, result.soa.read_cache_lines);
    result.read_cache_byte_delta =
        numeric_delta(result.record.read_cache_bytes, result.soa.read_cache_bytes);
    result.write_cache_line_delta =
        numeric_delta(result.record.write_cache_lines, result.soa.write_cache_lines);
    result.write_cache_byte_delta =
        numeric_delta(result.record.write_cache_bytes, result.soa.write_cache_bytes);
    result.page_delta = numeric_delta(result.record.pages, result.soa.pages);
    result.page_byte_delta = numeric_delta(result.record.page_bytes, result.soa.page_bytes);
    result.read_page_delta = numeric_delta(result.record.read_pages, result.soa.read_pages);
    result.read_page_byte_delta =
        numeric_delta(result.record.read_page_bytes, result.soa.read_page_bytes);
    result.write_page_delta = numeric_delta(result.record.write_pages, result.soa.write_pages);
    result.write_page_byte_delta =
        numeric_delta(result.record.write_page_bytes, result.soa.write_page_bytes);
    result.non_useful_cache_byte_delta =
        numeric_delta(result.record_non_useful_cache_bytes, result.soa_non_useful_cache_bytes);
    result.non_useful_page_byte_delta =
        numeric_delta(result.record_non_useful_page_bytes, result.soa_non_useful_page_bytes);
    return result;
}

auto Analyzer::compare_soa_access(SoaAccessAnalysis const& first, SoaAccessAnalysis const& second)
    -> SoaAccessComparison {
    auto summary = [](SoaAccessAnalysis const& analysis) {
        return AccessFootprintSummary{
            .useful_bytes = analysis.useful_bytes,
            .read_useful_bytes = analysis.read_useful_bytes,
            .write_useful_bytes = analysis.write_useful_bytes,
            .logical_read_useful_bytes = analysis.logical_read_useful_bytes,
            .logical_write_useful_bytes = analysis.logical_write_useful_bytes,
            .cache_lines = analysis.minimum_cache_lines_touched,
            .cache_bytes = analysis.minimum_cache_bytes_touched,
            .read_cache_lines = analysis.minimum_read_cache_lines_touched,
            .read_cache_bytes = analysis.minimum_read_cache_bytes_touched,
            .write_cache_lines = analysis.minimum_write_cache_lines_touched,
            .write_cache_bytes = analysis.minimum_write_cache_bytes_touched,
            .pages = analysis.minimum_pages_touched,
            .page_bytes = analysis.minimum_page_bytes_touched,
            .read_pages = analysis.minimum_read_pages_touched,
            .read_page_bytes = analysis.minimum_read_page_bytes_touched,
            .write_pages = analysis.minimum_write_pages_touched,
            .write_page_bytes = analysis.minimum_write_page_bytes_touched};
    };
    SoaAccessComparison result{
        .column_names = first.column_names,
        .accesses = first.accesses,
        .columns = {},
        .element_count = first.element_count,
        .multiplicity = first.multiplicity,
        .allocation_strategy = first.allocation_strategy,
        .footprint_exact = first.footprint_exact && second.footprint_exact,
        .first = summary(first),
        .second = summary(second),
        .first_allocation_count = first.allocation_count,
        .second_allocation_count = second.allocation_count,
        .first_total_allocation_bytes = first.total_allocation_bytes,
        .second_total_allocation_bytes = second.total_allocation_bytes,
        .first_alignment_padding_bytes = first.alignment_padding_bytes,
        .second_alignment_padding_bytes = second.alignment_padding_bytes,
        .allocation_count_delta = std::nullopt,
        .total_allocation_byte_delta = std::nullopt,
        .alignment_padding_byte_delta = std::nullopt,
        .first_minimum_cache_footprint_capacity = first.minimum_cache_footprint_capacity,
        .second_minimum_cache_footprint_capacity = second.minimum_cache_footprint_capacity,
        .first_allocated_capacity_payload_bytes = first.allocated_capacity_payload_bytes,
        .second_allocated_capacity_payload_bytes = second.allocated_capacity_payload_bytes,
        .first_capacity_slack_payload_bytes = first.capacity_slack_payload_bytes,
        .second_capacity_slack_payload_bytes = second.capacity_slack_payload_bytes,
        .first_full_logical_payload_bytes = first.full_logical_payload_bytes,
        .second_full_logical_payload_bytes = second.full_logical_payload_bytes,
        .first_unselected_payload_bytes = first.unselected_payload_bytes,
        .second_unselected_payload_bytes = second.unselected_payload_bytes,
        .first_non_payload_cache_bytes = first.non_payload_cache_bytes,
        .second_non_payload_cache_bytes = second.non_payload_cache_bytes,
        .first_non_payload_page_bytes = first.non_payload_page_bytes,
        .second_non_payload_page_bytes = second.non_payload_page_bytes,
        .useful_byte_delta = std::nullopt,
        .read_useful_byte_delta = std::nullopt,
        .write_useful_byte_delta = std::nullopt,
        .logical_read_useful_byte_delta = std::nullopt,
        .logical_write_useful_byte_delta = std::nullopt,
        .cache_line_delta = std::nullopt,
        .cache_byte_delta = std::nullopt,
        .read_cache_line_delta = std::nullopt,
        .read_cache_byte_delta = std::nullopt,
        .write_cache_line_delta = std::nullopt,
        .write_cache_byte_delta = std::nullopt,
        .page_delta = std::nullopt,
        .page_byte_delta = std::nullopt,
        .read_page_delta = std::nullopt,
        .read_page_byte_delta = std::nullopt,
        .write_page_delta = std::nullopt,
        .write_page_byte_delta = std::nullopt,
        .allocated_capacity_payload_delta = std::nullopt,
        .capacity_slack_payload_delta = std::nullopt,
        .full_logical_payload_delta = std::nullopt,
        .unselected_payload_delta = std::nullopt,
        .non_payload_cache_byte_delta = std::nullopt,
        .non_payload_page_byte_delta = std::nullopt,
        .diagnostics = {}};
    for (auto const& diagnostic : first.diagnostics) {
        result.diagnostics.push_back(
            {diagnostic.severity, "First SoA access: " + diagnostic.message});
    }
    for (auto const& diagnostic : second.diagnostics) {
        result.diagnostics.push_back(
            {diagnostic.severity, "Second SoA access: " + diagnostic.message});
    }
    if (first.element_count != second.element_count) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared SoA access analyses use different element counts."});
        return result;
    }
    if (first.allocation_strategy != second.allocation_strategy) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared SoA access analyses use different allocation strategies."});
        return result;
    }
    if (!access_intents_match(first.accesses, second.accesses)) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared SoA access analyses use different per-column access classifications."});
        return result;
    }
    if (first.multiplicity != second.multiplicity) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared SoA access analyses use different access multiplicities."});
        return result;
    }
    auto const first_names{
        std::set<std::string, std::less<>>{first.column_names.begin(), first.column_names.end()}};
    auto const second_names{
        std::set<std::string, std::less<>>{second.column_names.begin(), second.column_names.end()}};
    if (first_names.size() != first.column_names.size() ||
        second_names.size() != second.column_names.size() || first_names != second_names) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared SoA access analyses do not select the same unique named columns."});
        return result;
    }
    auto detail_names = [](std::span<SoaColumnAccessAnalysis const> const columns) {
        std::set<std::string, std::less<>> names;
        for (auto const& column : columns) {
            if (!names.insert(column.name).second) {
                return std::optional<std::set<std::string, std::less<>>>{};
            }
        }
        return std::optional{std::move(names)};
    };
    auto const first_detail_names{detail_names(first.columns)};
    auto const second_detail_names{detail_names(second.columns)};
    if (!first_detail_names.has_value() || !second_detail_names.has_value() ||
        *first_detail_names != first_names || *second_detail_names != second_names) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared SoA access analyses do not contain exactly one detail for every selected "
             "column."});
        return result;
    }
    for (auto const& column_name : result.column_names) {
        auto const first_column{
            std::ranges::find(first.columns, column_name, &SoaColumnAccessAnalysis::name)};
        auto const second_column{
            std::ranges::find(second.columns, column_name, &SoaColumnAccessAnalysis::name)};
        if (first_column == first.columns.end() || second_column == second.columns.end()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Compared SoA access analysis is missing details for selected column '" +
                     column_name + "'."});
            return result;
        }
        if (first_column->operation != second_column->operation) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Compared SoA access analyses classify selected column '" + column_name +
                     "' differently."});
            return result;
        }
    }
    for (auto const& column_name : result.column_names) {
        auto const first_column{
            std::ranges::find(first.columns, column_name, &SoaColumnAccessAnalysis::name)};
        auto const second_column{
            std::ranges::find(second.columns, column_name, &SoaColumnAccessAnalysis::name)};
        result.columns.push_back(
            {.name = column_name,
             .first = *first_column,
             .second = *second_column,
             .element_byte_delta =
                 numeric_delta(first_column->element_bytes, second_column->element_bytes),
             .useful_byte_delta =
                 numeric_delta(first_column->useful_bytes, second_column->useful_bytes),
             .read_useful_byte_delta =
                 numeric_delta(first_column->read_useful_bytes, second_column->read_useful_bytes),
             .write_useful_byte_delta =
                 numeric_delta(first_column->write_useful_bytes, second_column->write_useful_bytes),
             .logical_read_useful_byte_delta = numeric_delta(
                 first_column->logical_read_useful_bytes, second_column->logical_read_useful_bytes),
             .logical_write_useful_byte_delta =
                 numeric_delta(first_column->logical_write_useful_bytes,
                               second_column->logical_write_useful_bytes),
             .cache_line_delta = numeric_delta(first_column->minimum_cache_lines,
                                               second_column->minimum_cache_lines),
             .cache_byte_delta = numeric_delta(first_column->minimum_cache_bytes,
                                               second_column->minimum_cache_bytes),
             .non_payload_cache_byte_delta = numeric_delta(first_column->non_payload_cache_bytes,
                                                           second_column->non_payload_cache_bytes),
             .aligned_cache_line_straddling_element_delta =
                 numeric_delta(first_column->aligned_cache_line_straddling_elements,
                               second_column->aligned_cache_line_straddling_elements),
             .page_delta = numeric_delta(first_column->minimum_pages, second_column->minimum_pages),
             .page_byte_delta =
                 numeric_delta(first_column->minimum_page_bytes, second_column->minimum_page_bytes),
             .non_payload_page_byte_delta = numeric_delta(first_column->non_payload_page_bytes,
                                                          second_column->non_payload_page_bytes),
             .aligned_page_straddling_element_delta =
                 numeric_delta(first_column->aligned_page_straddling_elements,
                               second_column->aligned_page_straddling_elements),
             .allocated_capacity_payload_delta =
                 numeric_delta(first_column->allocated_capacity_payload_bytes,
                               second_column->allocated_capacity_payload_bytes),
             .capacity_slack_payload_delta =
                 numeric_delta(first_column->capacity_slack_payload_bytes,
                               second_column->capacity_slack_payload_bytes)});
    }
    result.useful_byte_delta = numeric_delta(result.first.useful_bytes, result.second.useful_bytes);
    result.allocation_count_delta = numeric_delta(first.allocation_count, second.allocation_count);
    result.total_allocation_byte_delta =
        numeric_delta(result.first_total_allocation_bytes, result.second_total_allocation_bytes);
    result.alignment_padding_byte_delta =
        numeric_delta(result.first_alignment_padding_bytes, result.second_alignment_padding_bytes);
    result.read_useful_byte_delta =
        numeric_delta(result.first.read_useful_bytes, result.second.read_useful_bytes);
    result.write_useful_byte_delta =
        numeric_delta(result.first.write_useful_bytes, result.second.write_useful_bytes);
    result.logical_read_useful_byte_delta = numeric_delta(result.first.logical_read_useful_bytes,
                                                          result.second.logical_read_useful_bytes);
    result.logical_write_useful_byte_delta = numeric_delta(
        result.first.logical_write_useful_bytes, result.second.logical_write_useful_bytes);
    result.cache_line_delta = numeric_delta(result.first.cache_lines, result.second.cache_lines);
    result.cache_byte_delta = numeric_delta(result.first.cache_bytes, result.second.cache_bytes);
    result.read_cache_line_delta =
        numeric_delta(result.first.read_cache_lines, result.second.read_cache_lines);
    result.read_cache_byte_delta =
        numeric_delta(result.first.read_cache_bytes, result.second.read_cache_bytes);
    result.write_cache_line_delta =
        numeric_delta(result.first.write_cache_lines, result.second.write_cache_lines);
    result.write_cache_byte_delta =
        numeric_delta(result.first.write_cache_bytes, result.second.write_cache_bytes);
    result.page_delta = numeric_delta(result.first.pages, result.second.pages);
    result.page_byte_delta = numeric_delta(result.first.page_bytes, result.second.page_bytes);
    result.read_page_delta = numeric_delta(result.first.read_pages, result.second.read_pages);
    result.read_page_byte_delta =
        numeric_delta(result.first.read_page_bytes, result.second.read_page_bytes);
    result.write_page_delta = numeric_delta(result.first.write_pages, result.second.write_pages);
    result.write_page_byte_delta =
        numeric_delta(result.first.write_page_bytes, result.second.write_page_bytes);
    result.allocated_capacity_payload_delta =
        numeric_delta(result.first_allocated_capacity_payload_bytes,
                      result.second_allocated_capacity_payload_bytes);
    result.capacity_slack_payload_delta = numeric_delta(result.first_capacity_slack_payload_bytes,
                                                        result.second_capacity_slack_payload_bytes);
    result.full_logical_payload_delta = numeric_delta(result.first_full_logical_payload_bytes,
                                                      result.second_full_logical_payload_bytes);
    result.unselected_payload_delta = numeric_delta(result.first_unselected_payload_bytes,
                                                    result.second_unselected_payload_bytes);
    result.non_payload_cache_byte_delta =
        numeric_delta(result.first_non_payload_cache_bytes, result.second_non_payload_cache_bytes);
    result.non_payload_page_byte_delta =
        numeric_delta(result.first_non_payload_page_bytes, result.second_non_payload_page_bytes);
    return result;
}

} // namespace ioj::layout
