#pragma once
#include <ioj/layout/analysis/row_range.hpp>
#include <ioj/layout/type_facts.hpp>

#include <cstdint>
#include <optional>
namespace ioj::layout {
struct AggregateLayout {
    RowRange members{};
    std::optional<std::uint64_t> size_bytes{};
    std::optional<std::uint64_t> alignment_bytes{};
    std::optional<std::uint64_t> payload_bytes{};
    std::optional<std::uint64_t> internal_padding_bytes{};
    std::optional<std::uint64_t> tail_padding_bytes{};
    std::optional<std::uint64_t> largest_alternative_bytes{};
    std::optional<TypeFacts> discriminant_facts{};
    std::optional<std::uint64_t> payload_size_bytes{};
    std::optional<std::uint64_t> payload_alignment_bytes{};
    std::optional<std::uint64_t> payload_offset_bytes{};
    RowRange tags{};
};
}
