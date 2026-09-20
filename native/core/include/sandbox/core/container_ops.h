#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <vector>

namespace ml {
template <typename T, typename Allocator>
void remove_at_swap(std::vector<T, Allocator>& values,
                    std::int32_t const index,
                    std::int32_t const count) {
    assert(index >= 0);
    assert(count >= 0);

    auto const old_size{values.size()};
    auto const index_offset{static_cast<std::size_t>(index)};
    auto const count_size{static_cast<std::size_t>(count)};
    assert(index_offset <= old_size);
    assert(count_size <= old_size - index_offset);

    auto const tail{old_size - index_offset - count_size};
    auto const moved{std::min(tail, count_size)};
    auto const source{old_size - moved};
    for (std::size_t offset{}; offset < moved; ++offset) {
        values[index_offset + offset] = values[source + offset];
    }

    values.resize(old_size - count_size);
}
}
