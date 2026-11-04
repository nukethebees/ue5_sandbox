#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <utility>
#include <vector>

namespace ioj {
template <typename T, typename Allocator>
void remove_at_swap(std::vector<T, Allocator>& values,
                    std::size_t const index,
                    std::size_t const count = 1) {
    auto const old_size{values.size()};
    assert(index <= old_size);
    assert(count <= old_size - index);

    auto const tail{old_size - index - count};
    auto const moved{std::min(tail, count)};
    auto const source{old_size - moved};
    for (std::size_t offset{}; offset < moved; ++offset) {
        values[index + offset] = std::move(values[source + offset]);
    }

    auto const new_end{values.end() -
                       static_cast<typename std::vector<T, Allocator>::difference_type>(count)};
    values.erase(new_end, values.end());
}

template <typename T, typename Allocator, typename Predicate>
auto remove_at_swap_if(std::vector<T, Allocator>& values, Predicate predicate) -> std::size_t {
    auto const old_size{values.size()};
    for (auto index{old_size}; index > 0; --index) {
        if (predicate(values[index - 1])) {
            ioj::remove_at_swap(values, index - 1);
        }
    }
    return old_size - values.size();
}
}
