#pragma once

#include <array>
#include <cassert>
#include <cstdint>

namespace ml {
struct FLoopBounds {
    std::uint32_t begin;
    std::uint32_t end;
};

inline auto make_rotated_loop_bounds(std::uint32_t const begin,
                                     std::uint32_t const end,
                                     std::uint32_t const offset) -> std::array<FLoopBounds, 2> {
    assert(end >= begin);

    if (begin == end) {
        return {FLoopBounds{begin, begin}, FLoopBounds{begin, begin}};
    }

    auto const count{end - begin};
    auto const normalized_offset{offset % count};
    auto const pivot{begin + normalized_offset};

    return {FLoopBounds{pivot, end}, FLoopBounds{begin, pivot}};
}
}
