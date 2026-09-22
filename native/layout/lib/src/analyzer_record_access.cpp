#include "analyzer_internal.hpp"

namespace ioj::layout {

auto Analyzer::analyze_record_access(RecordAnalysis const& record,
                                     std::span<AccessIntent const> const accesses,
                                     AbiProfile const& abi,
                                     std::uint64_t const multiplicity) -> RecordAccessAnalysis {
    RecordAccessAnalysis result{.type = record.type,
                                .member_names = {},
                                .accesses = {},
                                .element_count = record.aggregate.element_count,
                                .multiplicity = multiplicity,
                                .useful_bytes = std::nullopt,
                                .read_useful_bytes = std::uint64_t{},
                                .write_useful_bytes = std::uint64_t{},
                                .logical_read_useful_bytes = std::nullopt,
                                .logical_write_useful_bytes = std::nullopt,
                                .object_footprint_bytes = record.aggregate.total_storage_bytes,
                                .cache_line_bytes = std::nullopt,
                                .cache_lines_touched = std::nullopt,
                                .cache_bytes_touched = std::nullopt,
                                .read_cache_lines_touched = std::nullopt,
                                .read_cache_bytes_touched = std::nullopt,
                                .write_cache_lines_touched = std::nullopt,
                                .write_cache_bytes_touched = std::nullopt,
                                .non_selected_cache_bytes = std::nullopt,
                                .cache_footprint_capacity =
                                    cache_capacity_analysis(std::nullopt, abi.memory_facts()),
                                .page_bytes = std::nullopt,
                                .pages_touched = std::nullopt,
                                .page_bytes_touched = std::nullopt,
                                .read_pages_touched = std::nullopt,
                                .read_page_bytes_touched = std::nullopt,
                                .write_pages_touched = std::nullopt,
                                .write_page_bytes_touched = std::nullopt,
                                .non_selected_page_bytes = std::nullopt,
                                .diagnostics = {}};
    if (multiplicity == 0) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "Access multiplicity must be non-zero."});
    }
    if (!record.size_bytes.has_value()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "Record has incomplete target layout facts."});
    }

    std::set<std::string, std::less<>> unique_names;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> member_layouts;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> read_member_layouts;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> write_member_layouts;
    std::uint64_t useful_bytes_per_element{};
    std::optional<std::uint64_t> read_bytes_per_element{std::uint64_t{}};
    std::optional<std::uint64_t> write_bytes_per_element{std::uint64_t{}};
    for (auto const& access : accesses) {
        auto const& member_name{access.name};
        if (!accept_unique_access(
                unique_names, result.accesses, result.diagnostics, access, "record member")) {
            continue;
        }
        auto const member{
            std::ranges::find(record.members, member_name, &RecordMemberAnalysis::name)};
        if (member == record.members.end()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Selected record member '" + member_name + "' no longer exists."});
            continue;
        }
        result.member_names.push_back(member_name);
        result.accesses.push_back(access);
        if (!member->offset_bytes.has_value() || !member->extent_bytes.has_value()) {
            result.diagnostics.push_back({DiagnosticSeverity::error,
                                          "Selected record member '" + member_name +
                                              "' has incomplete target layout facts."});
            if (access_includes_read(access.operation)) {
                read_bytes_per_element.reset();
            }
            if (access_includes_write(access.operation)) {
                write_bytes_per_element.reset();
            }
            continue;
        }
        member_layouts.emplace_back(*member->offset_bytes, *member->extent_bytes);
        if (access_includes_read(access.operation)) {
            read_member_layouts.emplace_back(*member->offset_bytes, *member->extent_bytes);
        }
        if (access_includes_write(access.operation)) {
            write_member_layouts.emplace_back(*member->offset_bytes, *member->extent_bytes);
        }
        auto const next{checked_add(useful_bytes_per_element, *member->extent_bytes)};
        if (!next.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, "Selected member byte count overflows uint64."});
            return result;
        }
        useful_bytes_per_element = *next;

        auto accumulate_classified = [&](std::optional<std::uint64_t>& total,
                                         bool const included,
                                         char const* const category) {
            if (!included || !total.has_value()) {
                return;
            }
            total = checked_add(*total, *member->extent_bytes);
            if (!total.has_value()) {
                result.diagnostics.push_back({DiagnosticSeverity::error,
                                              std::string{"Selected member "} + category +
                                                  " byte count per element overflows uint64."});
            }
        };
        accumulate_classified(
            read_bytes_per_element, access_includes_read(access.operation), "read");
        accumulate_classified(
            write_bytes_per_element, access_includes_write(access.operation), "write");
    }
    if (member_layouts.empty()) {
        if (result.diagnostics.empty()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::warning, "No record members are selected for access."});
        }
        return result;
    }
    if (!record.size_bytes.has_value()) {
        return result;
    }
    std::ranges::sort(member_layouts);
    std::ranges::sort(read_member_layouts);
    std::ranges::sort(write_member_layouts);
    result.useful_bytes = checked_multiply(useful_bytes_per_element, result.element_count);
    if (!result.useful_bytes.has_value()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "Selected member useful byte count overflows uint64."});
    }
    result.read_useful_bytes = read_bytes_per_element.and_then(
        [&](std::uint64_t const bytes) { return checked_multiply(bytes, result.element_count); });
    result.write_useful_bytes = write_bytes_per_element.and_then(
        [&](std::uint64_t const bytes) { return checked_multiply(bytes, result.element_count); });
    if (read_bytes_per_element.has_value() && !result.read_useful_bytes.has_value()) {
        result.diagnostics.push_back({DiagnosticSeverity::error,
                                      "Selected member read useful byte count overflows uint64."});
    }
    if (write_bytes_per_element.has_value() && !result.write_useful_bytes.has_value()) {
        result.diagnostics.push_back({DiagnosticSeverity::error,
                                      "Selected member write useful byte count overflows uint64."});
    }

    auto analyze_regions = [&](std::optional<std::uint64_t> const region_bytes,
                               char const* const region_name,
                               std::optional<std::uint64_t>& output_regions,
                               std::optional<std::uint64_t>& output_region_bytes) {
        output_region_bytes = region_bytes;
        if (!region_bytes.has_value()) {
            result.diagnostics.push_back({DiagnosticSeverity::warning,
                                          std::string{region_name} +
                                              " size is unknown for ABI profile '" + abi.name() +
                                              "'."});
            return;
        }
        if (*region_bytes == 0) {
            output_region_bytes.reset();
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, std::string{region_name} + " size must be non-zero."});
            return;
        }
        std::string error;
        output_regions = touched_regions(
            *record.size_bytes, member_layouts, result.element_count, *region_bytes, error);
        if (!output_regions.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, std::string{region_name} + ": " + error});
        }
    };
    auto const& memory{abi.memory_facts()};
    analyze_regions(
        memory.cache_line_bytes, "Cache-line", result.cache_lines_touched, result.cache_line_bytes);
    analyze_regions(memory.page_bytes, "Page", result.pages_touched, result.page_bytes);

    auto analyze_operation_regions = [&](auto const& layouts,
                                         std::optional<std::uint64_t> const region_bytes,
                                         std::optional<std::uint64_t>& output_regions,
                                         std::optional<std::uint64_t>& output_bytes,
                                         char const* const category) {
        if (layouts.empty()) {
            output_regions = 0;
            output_bytes = 0;
            return;
        }
        if (!region_bytes.has_value() || *region_bytes == 0) {
            return;
        }
        std::string error;
        output_regions = touched_regions(
            *record.size_bytes, layouts, result.element_count, *region_bytes, error);
        if (!output_regions.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, std::string{category} + ": " + error});
            return;
        }
        output_bytes = checked_multiply(*output_regions, *region_bytes);
        if (!output_bytes.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 std::string{category} + " address-coverage byte count overflows uint64."});
        }
    };
    analyze_operation_regions(read_member_layouts,
                              memory.cache_line_bytes,
                              result.read_cache_lines_touched,
                              result.read_cache_bytes_touched,
                              "Read cache-line");
    analyze_operation_regions(write_member_layouts,
                              memory.cache_line_bytes,
                              result.write_cache_lines_touched,
                              result.write_cache_bytes_touched,
                              "Write cache-line");
    analyze_operation_regions(read_member_layouts,
                              memory.page_bytes,
                              result.read_pages_touched,
                              result.read_page_bytes_touched,
                              "Read page");
    analyze_operation_regions(write_member_layouts,
                              memory.page_bytes,
                              result.write_pages_touched,
                              result.write_page_bytes_touched,
                              "Write page");

    auto derive_footprint = [&](std::optional<std::uint64_t> const region_count,
                                std::optional<std::uint64_t> const region_bytes,
                                std::optional<std::uint64_t>& footprint_bytes,
                                std::optional<std::uint64_t>& non_selected_bytes,
                                char const* const region_name) {
        if (!region_count.has_value() || !region_bytes.has_value()) {
            return;
        }
        footprint_bytes = checked_multiply(*region_count, *region_bytes);
        if (!footprint_bytes.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 std::string{"Touched "} + region_name + " byte count overflows uint64."});
            return;
        }
        if (!result.useful_bytes.has_value()) {
            return;
        }
        if (*footprint_bytes < *result.useful_bytes) {
            result.diagnostics.push_back({DiagnosticSeverity::error,
                                          std::string{"Touched "} + region_name +
                                              " bytes are smaller than useful member bytes."});
            return;
        }
        non_selected_bytes = *footprint_bytes - *result.useful_bytes;
    };
    derive_footprint(result.cache_lines_touched,
                     result.cache_line_bytes,
                     result.cache_bytes_touched,
                     result.non_selected_cache_bytes,
                     "cache-line");
    derive_footprint(result.pages_touched,
                     result.page_bytes,
                     result.page_bytes_touched,
                     result.non_selected_page_bytes,
                     "page");
    result.cache_footprint_capacity =
        cache_capacity_analysis(result.cache_bytes_touched, abi.memory_facts());
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

auto Analyzer::analyze_record_access(RecordAnalysis const& record,
                                     std::span<std::string const> const member_names,
                                     AbiProfile const& abi,
                                     AccessOperation const operation,
                                     std::uint64_t const multiplicity) -> RecordAccessAnalysis {
    std::vector<AccessIntent> accesses;
    accesses.reserve(member_names.size());
    for (auto const& member_name : member_names) {
        accesses.push_back({.name = member_name, .operation = operation});
    }
    return analyze_record_access(record, accesses, abi, multiplicity);
}

auto Analyzer::analyze_record_member_access(RecordAnalysis const& record,
                                            std::string_view const member_name,
                                            AbiProfile const& abi,
                                            AccessOperation const operation,
                                            std::uint64_t const multiplicity)
    -> RecordAccessAnalysis {
    auto const names{std::vector<std::string>{std::string{member_name}}};
    return analyze_record_access(record, names, abi, operation, multiplicity);
}

auto Analyzer::compare_record_access(RecordAccessAnalysis const& first,
                                     RecordAccessAnalysis const& second) -> RecordAccessComparison {
    RecordAccessComparison result;
    result.first = first;
    result.second = second;
    for (auto const& diagnostic : first.diagnostics) {
        result.diagnostics.push_back(
            {diagnostic.severity, "First target record access: " + diagnostic.message});
    }
    for (auto const& diagnostic : second.diagnostics) {
        result.diagnostics.push_back(
            {diagnostic.severity, "Second target record access: " + diagnostic.message});
    }
    if (first.type != second.type) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared record access analyses describe different records."});
        return result;
    }
    if (first.element_count != second.element_count) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared record access analyses use different element counts."});
        return result;
    }
    if (first.multiplicity != second.multiplicity) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared record access analyses use different access multiplicities."});
        return result;
    }

    auto unique_names{[](auto const& values, auto const projection) {
        std::set<std::string, std::less<>> names;
        for (auto const& value : values) {
            names.insert(projection(value));
        }
        return names;
    }};
    auto const first_member_names{unique_names(
        first.member_names, [](std::string const& name) -> std::string const& { return name; })};
    auto const second_member_names{unique_names(
        second.member_names, [](std::string const& name) -> std::string const& { return name; })};
    auto const first_access_names{
        unique_names(first.accesses,
                     [](AccessIntent const& access) -> std::string const& { return access.name; })};
    auto const second_access_names{
        unique_names(second.accesses,
                     [](AccessIntent const& access) -> std::string const& { return access.name; })};
    if (first_member_names.size() != first.member_names.size() ||
        second_member_names.size() != second.member_names.size() ||
        first_access_names.size() != first.accesses.size() ||
        second_access_names.size() != second.accesses.size() ||
        first_member_names != first_access_names || second_member_names != second_access_names ||
        first_member_names != second_member_names) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared record access analyses do not contain the same complete unique selected "
             "member set."});
        return result;
    }
    if (!access_intents_match(first.accesses, second.accesses)) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared record access analyses use different per-member access classifications."});
        return result;
    }

    result.useful_byte_delta = numeric_delta(first.useful_bytes, second.useful_bytes);
    result.read_useful_byte_delta =
        numeric_delta(first.read_useful_bytes, second.read_useful_bytes);
    result.write_useful_byte_delta =
        numeric_delta(first.write_useful_bytes, second.write_useful_bytes);
    result.logical_read_useful_byte_delta =
        numeric_delta(first.logical_read_useful_bytes, second.logical_read_useful_bytes);
    result.logical_write_useful_byte_delta =
        numeric_delta(first.logical_write_useful_bytes, second.logical_write_useful_bytes);
    result.object_footprint_byte_delta =
        numeric_delta(first.object_footprint_bytes, second.object_footprint_bytes);
    result.cache_line_size_delta = numeric_delta(first.cache_line_bytes, second.cache_line_bytes);
    result.cache_line_delta = numeric_delta(first.cache_lines_touched, second.cache_lines_touched);
    result.cache_byte_delta = numeric_delta(first.cache_bytes_touched, second.cache_bytes_touched);
    result.read_cache_line_delta =
        numeric_delta(first.read_cache_lines_touched, second.read_cache_lines_touched);
    result.read_cache_byte_delta =
        numeric_delta(first.read_cache_bytes_touched, second.read_cache_bytes_touched);
    result.write_cache_line_delta =
        numeric_delta(first.write_cache_lines_touched, second.write_cache_lines_touched);
    result.write_cache_byte_delta =
        numeric_delta(first.write_cache_bytes_touched, second.write_cache_bytes_touched);
    result.non_selected_cache_byte_delta =
        numeric_delta(first.non_selected_cache_bytes, second.non_selected_cache_bytes);
    result.page_size_delta = numeric_delta(first.page_bytes, second.page_bytes);
    result.page_delta = numeric_delta(first.pages_touched, second.pages_touched);
    result.page_byte_delta = numeric_delta(first.page_bytes_touched, second.page_bytes_touched);
    result.read_page_delta = numeric_delta(first.read_pages_touched, second.read_pages_touched);
    result.read_page_byte_delta =
        numeric_delta(first.read_page_bytes_touched, second.read_page_bytes_touched);
    result.write_page_delta = numeric_delta(first.write_pages_touched, second.write_pages_touched);
    result.write_page_byte_delta =
        numeric_delta(first.write_page_bytes_touched, second.write_page_bytes_touched);
    result.non_selected_page_byte_delta =
        numeric_delta(first.non_selected_page_bytes, second.non_selected_page_bytes);
    return result;
}

} // namespace ioj::layout
