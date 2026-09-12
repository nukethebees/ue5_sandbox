#pragma once

#include <cassert>
#include <cstdint>
#include <span>
#include <utility>

namespace ml {
// indices[new_index] is the old row index that belongs at new_index.
// The indices are restored before returning so one permutation can be applied to every stream.
template <typename T>
void apply_permutation(std::span<T> const values, std::span<std::int32_t> const indices) {
    auto const count{static_cast<std::int32_t>(values.size())};
    assert(indices.size() == values.size());

    for (std::int32_t start_index{}; start_index < count; ++start_index) {
        if (indices[static_cast<std::size_t>(start_index)] < 0) {
            continue;
        }

        auto value{std::move(values[static_cast<std::size_t>(start_index)])};
        auto destination_index{start_index};
        for (;;) {
            auto const source_index{indices[static_cast<std::size_t>(destination_index)]};
            assert(source_index >= 0 && source_index < count);
            indices[static_cast<std::size_t>(destination_index)] = ~source_index;

            if (source_index == start_index) {
                values[static_cast<std::size_t>(destination_index)] = std::move(value);
                break;
            }

            values[static_cast<std::size_t>(destination_index)] =
                std::move(values[static_cast<std::size_t>(source_index)]);
            destination_index = source_index;
        }
    }

    for (auto& index : indices) {
        assert(index < 0);
        index = ~index;
    }
}
}
