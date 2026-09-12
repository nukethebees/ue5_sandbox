#pragma once

#include <array>
#include <cassert>
#include <cstdint>

namespace ml {
struct FLoopBounds {
    std::int32_t begin;
    std::int32_t end;
};

inline auto make_rotated_loop_bounds(std::int32_t const begin,
                                     std::int32_t const end,
                                     std::int32_t const offset) -> std::array<FLoopBounds, 2> {
    assert(begin >= 0);
    assert(end >= begin);
    assert(offset >= 0);

    if (begin == end) {
        return {FLoopBounds{begin, begin}, FLoopBounds{begin, begin}};
    }

    auto const count{end - begin};
    auto const normalized_offset{offset % count};
    auto const pivot{begin + normalized_offset};

    return {FLoopBounds{pivot, end}, FLoopBounds{begin, pivot}};
}
}
