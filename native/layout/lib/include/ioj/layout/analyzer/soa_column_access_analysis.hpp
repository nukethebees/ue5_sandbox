#pragma once

#include <ioj/layout/analyzer/access_operation.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace ioj::layout {

struct SoaColumnAccessAnalysis {
    std::string name{};
    AccessOperation operation{AccessOperation::read};
    std::string physical_type{};
    std::optional<std::uint64_t> element_bytes{};
    std::optional<std::uint64_t> useful_bytes{};
    std::optional<std::uint64_t> read_useful_bytes{};
    std::optional<std::uint64_t> write_useful_bytes{};
    std::optional<std::uint64_t> logical_read_useful_bytes{};
    std::optional<std::uint64_t> logical_write_useful_bytes{};
    std::optional<std::uint64_t> minimum_cache_lines{};
    std::optional<std::uint64_t> minimum_cache_bytes{};
    std::optional<std::uint64_t> non_payload_cache_bytes{};
    std::optional<std::uint64_t> aligned_cache_line_straddling_elements{};
    std::optional<std::uint64_t> minimum_pages{};
    std::optional<std::uint64_t> minimum_page_bytes{};
    std::optional<std::uint64_t> non_payload_page_bytes{};
    std::optional<std::uint64_t> aligned_page_straddling_elements{};
    std::optional<std::uint64_t> allocated_capacity_payload_bytes{};
    std::optional<std::uint64_t> capacity_slack_payload_bytes{};
};

}
