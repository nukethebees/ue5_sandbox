#pragma once

#include <cassert>
#include <concepts>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>

namespace ioj {
// indices[new_index] is the old row index that belongs at new_index.
// The indices are restored before returning so one permutation can be applied to every stream.
// Limit row counts to INT32_MAX so the high bit can mark visited indices of either signedness.
template <typename T, typename Index, std::size_t Extent>
    requires (std::same_as<Index, std::int32_t> || std::same_as<Index, std::uint32_t>)
void apply_permutation(std::span<T> const values, std::span<Index, Extent> const indices) {
    assert(values.size() <= static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()));
    auto const count{static_cast<std::uint32_t>(values.size())};
    assert(indices.size() == values.size());

    for ([[maybe_unused]] auto const index : indices) {
        assert(static_cast<std::uint32_t>(index) < count);
    }

    constexpr std::uint32_t visited_bit{std::uint32_t{1} << 31};
    for (std::uint32_t start_index{}; start_index < count; ++start_index) {
        if ((static_cast<std::uint32_t>(indices[start_index]) & visited_bit) != 0) {
            continue;
        }

        auto value{std::move(values[start_index])};
        auto destination_index{start_index};
        for (;;) {
            auto const source_index{static_cast<std::uint32_t>(indices[destination_index])};
            assert(source_index < count);
            indices[destination_index] = ~indices[destination_index];

            if (source_index == start_index) {
                values[destination_index] = std::move(value);
                break;
            }

            values[destination_index] = std::move(values[source_index]);
            destination_index = source_index;
        }
    }

    for (auto& index : indices) {
        assert((static_cast<std::uint32_t>(index) & visited_bit) != 0);
        index = ~index;
    }
}

struct ColumnPermutation {
    template <typename... T>
    void operator()(T*... columns) const {
        (apply_permutation(std::span{columns, count}, indices), ...);
    }

    std::uint32_t count;
    std::span<std::uint32_t> indices;
};
}
