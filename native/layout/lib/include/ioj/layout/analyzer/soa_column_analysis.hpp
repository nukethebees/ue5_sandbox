#pragma once

#include <ioj/layout/analyzer/cache_line_tiling.hpp>
#include <ioj/layout/type_facts.hpp>

#include <lispb/schema/indices.h>

#include <cstdint>
#include <optional>
#include <string>

namespace ioj::layout {

struct SoaColumnAnalysis {
    std::string name{};
    lispb::schema::TypeId semantic_type{};
    std::string schema_type{};
    std::string physical_type{};
    bool overridden{};
    std::optional<TypeFacts> type_facts{};
    std::optional<std::uint64_t> total_bytes{};
    std::optional<std::uint64_t> allocation_offset_bytes{};
    std::optional<std::uint64_t> padding_before_bytes{};
    std::optional<std::uint64_t> minimum_cache_lines{};
    std::optional<std::uint64_t> elements_per_cache_line{};
    std::optional<std::uint64_t> minimum_pages{};
    std::optional<std::uint64_t> complete_elements_per_page{};
    std::optional<CacheLineTiling> cache_line_tiling{};
};

}
