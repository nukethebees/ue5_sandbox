#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>

namespace ml::soa_storage_detail {

// Indices describe the original rows in strictly descending order.
template <typename Size, typename Copy>
void for_each_removal_run(Size num, std::span<Size const> indices, Copy copy) {
    assert(indices.size() <= static_cast<std::size_t>(num));
#ifndef NDEBUG
    auto previous{num};
    for (auto const index : indices) {
        assert(index >= 0 && index < previous);
        previous = index;
    }
#endif
    auto const count{static_cast<Size>(indices.size())};
    auto const final_num{num - count};
    auto hole{static_cast<std::ptrdiff_t>(count) - 1};
    auto removed_tail{hole};
    while (removed_tail >= 0 && indices[removed_tail] < final_num) {
        --removed_tail;
    }
    auto source{final_num};
    while (hole >= 0 && indices[hole] < final_num) {
        auto destination{indices[hole--]};
        auto end{destination + 1};
        while (hole >= 0 && indices[hole] == end && end < final_num) {
            ++end;
            --hole;
        }
        while (destination < end) {
            while (removed_tail >= 0 && indices[removed_tail] == source) {
                ++source;
                --removed_tail;
            }
            auto const source_end{removed_tail >= 0 ? indices[removed_tail] : num};
            auto const move_count{std::min(end - destination, source_end - source)};
            copy(destination, source, move_count);
            destination += move_count;
            source += move_count;
        }
    }
}

}
